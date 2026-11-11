module;

import std;
import star.format;
import star.interpolation;
import star.vector;
import star.math_common;
import star.image;
import star.rect;

export module star.image_scaling;

export namespace Star {

[[nodiscard]] std::expected<Image, StarException> scale_nearest(Image const& src_image, Vec2F const& scale);
[[nodiscard]] std::expected<Image, StarException> scale_bilinear(Image const& src_image, Vec2F const& scale);
[[nodiscard]] std::expected<Image, StarException> scale_bicubic(Image const& src_image, Vec2F const& scale);

// Compute only a window of the scaled image, byte-equivalent to scaling then
// sub_image(). Empty windows are allowed; invalid windows return an error.
bool scaled_crop_fits(Image const& src_image, Vec2F const& scale, RectI const& window);
[[nodiscard]] std::expected<Image, StarException> scale_nearest_cropped(Image const& src_image, Vec2F const& scale,
                                                                        RectI const& window);
[[nodiscard]] std::expected<Image, StarException> scale_bilinear_cropped(Image const& src_image, Vec2F const& scale,
                                                                         RectI const& window);
}

namespace Star {

static bool finite_nonnegative(float value) {
  auto bits = std::bit_cast<std::uint32_t>(value);
  return (bits & 0x7f800000u) != 0x7f800000u && ((bits & 0x80000000u) == 0 || (bits & 0x7fffffffu) == 0);
}

static std::expected<Vec2U, StarException> scaled_size(Image const& src_image, Vec2F const& scale) {
  Vec2U size;
  for (unsigned axis = 0; axis < 2; ++axis) {
    // Bit checks, including after multiplication, survive finite-math-only.
    if (!finite_nonnegative(scale[axis]))
      return std::unexpected(ImageException("image scale must be finite and nonnegative"));
    float extent = static_cast<float>(src_image.size()[axis]) * scale[axis];
    if (!finite_nonnegative(extent))
      return std::unexpected(ImageException("scaled image extent is not finite"));
    float rounded = std::round(extent);
    if (static_cast<double>(rounded) > static_cast<double>(std::numeric_limits<unsigned>::max()))
      return std::unexpected(ImageException("scaled image extent exceeds the coordinate range"));
    size[axis] = std::max(static_cast<unsigned>(rounded), 1u);
  }
  return size;
}

static bool crop_fits(Vec2U const& size, RectI const& window) {
  // Do not subtract malformed signed rectangle endpoints.
  for (unsigned axis = 0; axis < 2; ++axis) {
    if (window.min()[axis] < 0 || window.max()[axis] < window.min()[axis]
        || static_cast<unsigned>(window.max()[axis]) > size[axis])
      return false;
  }
  return true;
}

static unsigned nearest_index(unsigned position, float const volatile& scale, unsigned extent) {
  if (scale == 0.0f)
    return 0;
  // Keep actual division, including in the scalar fallback. A reciprocal can
  // move a sample across a rounding half-boundary under fast-math.
  float coordinate = static_cast<float>(position) / scale;
  if (!finite_nonnegative(coordinate))
    return extent - 1;
  float rounded = std::round(coordinate);
  if (static_cast<double>(rounded) >= static_cast<double>(extent - 1))
    return extent - 1;
  return static_cast<unsigned>(rounded);
}

static std::expected<Image, StarException> nearest_window(Image const& src_image, Vec2F const& scale,
                                                          Vec2U const& offset, Vec2U const& dest_size) {
  auto bytes = Image::size_bytes(dest_size, src_image.pixel_format());
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  auto src_size = src_image.size();
  auto bpp = src_image.bytes_per_pixel();
  bool byte_format = bpp == 3 || bpp == 4;
  Image dest_image = byte_format && !src_image.empty() ? Image::for_overwrite(dest_size, src_image.pixel_format())
                                                       : Image(dest_size, src_image.pixel_format());
  // Empty sources retain zero initialization, including implicit 24-bit alpha.
  if (src_image.empty() || dest_image.empty())
    return dest_image;

  volatile float scale_x = scale[0], scale_y = scale[1];
  std::vector<std::size_t> columns(dest_size[0]);
  for (unsigned x = 0; x < dest_size[0]; ++x)
    columns[x] = nearest_index(offset[0] + x, scale_x, src_size[0]);

  if (byte_format) {
    for (auto& column : columns)
      column *= bpp;
    auto source = src_image.data();
    auto destination = dest_image.data();
    std::size_t source_stride = static_cast<std::size_t>(src_size[0]) * bpp;
    std::size_t destination_stride = static_cast<std::size_t>(dest_size[0]) * bpp;
    for (unsigned y = 0; y < dest_size[1]; ++y) {
      auto source_row =
        source.subspan(nearest_index(offset[1] + y, scale_y, src_size[1]) * source_stride, source_stride);
      auto destination_row = destination.subspan(static_cast<std::size_t>(y) * destination_stride, destination_stride);
      auto output = destination_row.begin();
      for (unsigned x = 0; x < dest_size[0]; ++x)
        output = std::ranges::copy_n(std::next(source_row.begin(), columns[x]), bpp, output).out;
    }
    return dest_image;
  }

  // Float formats keep scalar clamp()/byte-setter behavior, not float copies.
  for (unsigned y = 0; y < dest_size[1]; ++y) {
    auto source_y = nearest_index(offset[1] + y, scale_y, src_size[1]);
    for (unsigned x = 0; x < dest_size[0]; ++x) {
      auto assigned = dest_image.set(
        {x, y},
        src_image.clamp(
          static_cast<int>(std::min(columns[x], static_cast<std::size_t>(std::numeric_limits<int>::max()))),
          static_cast<int>(std::min(source_y, static_cast<unsigned>(std::numeric_limits<int>::max())))));
      if (!assigned)
        return std::unexpected(std::move(assigned.error()));
    }
  }
  return dest_image;
}

template <std::size_t N>
concept interpolation_samples = N == 2 || N == 4;

template <std::size_t N>
  requires interpolation_samples<N>
struct axis_samples {
  std::array<std::size_t, N> indices;
  float fraction;
};

template <std::size_t N>
  requires interpolation_samples<N>
static axis_samples<N> interpolation_axis(float coordinate, bool zero_coordinate, unsigned extent) {
  axis_samples<N> samples{};
  if (extent == 0)
    return samples;
  if (zero_coordinate)
    coordinate = 0.0f;
  // The old floor-to-int and subsequent +1/+3 must never overflow. Beyond
  // that range every tap lies past the edge, so use that edge with fraction0.
  if (!finite_nonnegative(coordinate)
      || static_cast<double>(coordinate) > static_cast<double>(std::numeric_limits<int>::max() - (N - 1))) {
    samples.indices.fill(extent - 1);
    return samples;
  }
  auto integral = static_cast<unsigned>(std::floor(coordinate));
  samples.fraction = coordinate - static_cast<float>(integral);
  for (std::size_t tap = 0; tap < N; ++tap)
    samples.indices[tap] = std::min(static_cast<std::size_t>(integral) + tap, static_cast<std::size_t>(extent - 1));
  return samples;
}

template <typename Sampler>
concept pixel_sampler = requires(Sampler& sample, std::size_t x, std::size_t y) {
  { sample(x, y) } -> std::same_as<Vec4F>;
};

template <std::size_t N, pixel_sampler Sampler>
  requires interpolation_samples<N>
static Vec4B interpolate_pixel(float x, float y, Sampler&& sample) {
  if constexpr (N == 2) {
    // Keep the original association and byte truncation.
    return Vec4B(lerp(y, lerp(x, sample(0, 0), sample(1, 0)), lerp(x, sample(0, 1), sample(1, 1))));
  } else {
    // This is intentionally floor..floor+3, not a centered cubic window.
    Vec4F a = cubic4(x, sample(0, 0), sample(1, 0), sample(2, 0), sample(3, 0));
    Vec4F b = cubic4(x, sample(0, 1), sample(1, 1), sample(2, 1), sample(3, 1));
    Vec4F c = cubic4(x, sample(0, 2), sample(1, 2), sample(2, 2), sample(3, 2));
    Vec4F d = cubic4(x, sample(0, 3), sample(1, 3), sample(2, 3), sample(3, 3));
    auto result = cubic4(y, a, b, c, d);
    return Vec4B(std::clamp(result[0], 0.0f, 255.0f), std::clamp(result[1], 0.0f, 255.0f),
                 std::clamp(result[2], 0.0f, 255.0f), std::clamp(result[3], 0.0f, 255.0f));
  }
}

template <std::size_t N>
  requires interpolation_samples<N>
static std::expected<Image, StarException> interpolated_window(Image const& src_image, Vec2F const& scale,
                                                               Vec2U const& offset, Vec2U const& dest_size) {
  auto bytes = Image::size_bytes(dest_size, src_image.pixel_format());
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  auto bpp = src_image.bytes_per_pixel();
  bool byte_format = bpp == 3 || bpp == 4;
  Image dest_image = byte_format && !src_image.empty() ? Image::for_overwrite(dest_size, src_image.pixel_format())
                                                       : Image(dest_size, src_image.pixel_format());
  if (dest_image.empty() || src_image.empty())
    return dest_image;

  auto src_size = src_image.size();
  // The legacy packed division refines each numerator with a reciprocal
  // residual. Hoisting a refined reciprocal changes final byte truncation.
  // Volatile divisors retain that division while reusing its coordinates.
  volatile float scale_x = scale[0] == 0.0f ? 1.0f : scale[0];
  volatile float scale_y = scale[1] == 0.0f ? 1.0f : scale[1];
  std::vector<Vec2F> coordinates(std::max(dest_size[0], dest_size[1]));
  for (unsigned i = 0; i < coordinates.size(); ++i)
    coordinates[i] = vdiv(Vec2F(offset[0] + std::min(i, dest_size[0] - 1), offset[1] + std::min(i, dest_size[1] - 1)),
                          Vec2F(static_cast<float>(scale_x), static_cast<float>(scale_y)));
  std::vector<axis_samples<N>> columns(dest_size[0]);
  for (unsigned x = 0; x < dest_size[0]; ++x)
    columns[x] = interpolation_axis<N>(coordinates[x][0], scale[0] == 0.0f || offset[0] + x == 0, src_size[0]);

  if (byte_format) {
    for (auto& column : columns)
      for (auto& index : column.indices)
        index *= bpp;
    auto source = src_image.data();
    auto destination = dest_image.data();
    std::size_t source_stride = static_cast<std::size_t>(src_size[0]) * bpp;
    std::size_t destination_stride = static_cast<std::size_t>(dest_size[0]) * bpp;
    for (unsigned y = 0; y < dest_size[1]; ++y) {
      auto vertical = interpolation_axis<N>(coordinates[y][1], scale[1] == 0.0f || offset[1] + y == 0, src_size[1]);
      std::array<std::span<std::uint8_t const>, N> rows;
      for (std::size_t tap = 0; tap < N; ++tap)
        rows[tap] = source.subspan(vertical.indices[tap] * source_stride, source_stride);
      auto output = destination.subspan(static_cast<std::size_t>(y) * destination_stride, destination_stride);
      auto destination_pixel = output.begin();
      for (unsigned x = 0; x < dest_size[0]; ++x) {
        auto const& column = columns[x];
        auto pixel = interpolate_pixel<N>(column.fraction, vertical.fraction, [&](std::size_t sx, std::size_t sy) {
          auto p = std::next(rows[sy].begin(), column.indices[sx]);
          return Vec4F(p[0], p[1], p[2], bpp == 4 ? p[3] : 255);
        });
        destination_pixel[0] = pixel[0];
        destination_pixel[1] = pixel[1];
        destination_pixel[2] = pixel[2];
        if (bpp == 4)
          destination_pixel[3] = pixel[3];
        std::advance(destination_pixel, bpp);
      }
    }
    return dest_image;
  }

  // Float pixels deliberately retain the old clamp()/byte-setter semantics.
  for (unsigned y = 0; y < dest_size[1]; ++y) {
    auto vertical = interpolation_axis<N>(coordinates[y][1], scale[1] == 0.0f || offset[1] + y == 0, src_size[1]);
    for (unsigned x = 0; x < dest_size[0]; ++x) {
      auto const& column = columns[x];
      auto pixel = interpolate_pixel<N>(column.fraction, vertical.fraction, [&](std::size_t sx, std::size_t sy) {
        return Vec4F(src_image.clamp(
          static_cast<int>(std::min(column.indices[sx], static_cast<std::size_t>(std::numeric_limits<int>::max()))),
          static_cast<int>(std::min(vertical.indices[sy], static_cast<std::size_t>(std::numeric_limits<int>::max())))));
      });
      auto assigned = dest_image.set({x, y}, pixel);
      if (!assigned)
        return std::unexpected(std::move(assigned.error()));
    }
  }
  return dest_image;
}

std::expected<Image, StarException> scale_nearest(Image const& src_image, Vec2F const& scale) {
  auto size = scaled_size(src_image, scale);
  if (!size)
    return std::unexpected(std::move(size.error()));
  return nearest_window(src_image, scale, Vec2U(), *size);
}

std::expected<Image, StarException> scale_bilinear(Image const& src_image, Vec2F const& scale) {
  auto size = scaled_size(src_image, scale);
  if (!size)
    return std::unexpected(std::move(size.error()));
  return interpolated_window<2>(src_image, scale, Vec2U(), *size);
}

std::expected<Image, StarException> scale_bicubic(Image const& src_image, Vec2F const& scale) {
  auto size = scaled_size(src_image, scale);
  if (!size)
    return std::unexpected(std::move(size.error()));
  return interpolated_window<4>(src_image, scale, Vec2U(), *size);
}

bool scaled_crop_fits(Image const& src_image, Vec2F const& scale, RectI const& window) {
  auto size = scaled_size(src_image, scale);
  if (!size || !crop_fits(*size, window))
    return false;
  return Image::size_bytes(Vec2U(window.size()), src_image.pixel_format()).has_value();
}

std::expected<Image, StarException> scale_nearest_cropped(Image const& src_image, Vec2F const& scale,
                                                          RectI const& window) {
  auto size = scaled_size(src_image, scale);
  if (!size)
    return std::unexpected(std::move(size.error()));
  if (!crop_fits(*size, window))
    return std::unexpected(ImageException(Star::format("crop window {} out of scaled image bounds", window)));
  return nearest_window(src_image, scale, Vec2U(window.min()), Vec2U(window.size()));
}

std::expected<Image, StarException> scale_bilinear_cropped(Image const& src_image, Vec2F const& scale,
                                                           RectI const& window) {
  auto size = scaled_size(src_image, scale);
  if (!size)
    return std::unexpected(std::move(size.error()));
  if (!crop_fits(*size, window))
    return std::unexpected(ImageException(Star::format("crop window {} out of scaled image bounds", window)));
  return interpolated_window<2>(src_image, scale, Vec2U(window.min()), Vec2U(window.size()));
}
}
