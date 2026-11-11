#include <png.h>
import std;
import star.format;
import star.io_device;
import star.vector;
import star.image;
import star.logging;


import star.math_common;
import star.algorithm;
namespace Star {

namespace {

struct PngState {
  Logger& logger;
  IODevice& device;
  bool writing;
  png_structp png = nullptr;
  png_infop info = nullptr;
  png_infop end = nullptr;
  std::optional<StarException> error{};

  ~PngState() {
    if (writing)
      png_destroy_write_struct(&png, &info);
    else
      png_destroy_read_struct(&png, &info, &end);
  }

  std::expected<void, StarException> check(bool success, std::string_view message) {
    if (error)
      return std::unexpected(std::move(*error));
    if (!success)
      return std::unexpected(ImageException(std::string(message)));
    return {};
  }
};

void log_png_message(PngState& state, std::string_view kind, std::string_view message) noexcept {
  auto name = state.device.device_name();
  if (!name) {
    state.error = std::move(name.error());
    return;
  }
  auto logged = state.logger.debug("PNG {} in file: '{}', {}", kind, *name, message);
  if (!logged)
    state.error = std::move(logged.error());
}

void png_error(png_structp png, png_const_charp message) noexcept {
  auto& state = *static_cast<PngState*>(png_get_error_ptr(png));
  log_png_message(state, "error", message);
  if (!state.error)
    state.error = ImageException(std::string(message));
  png_longjmp(png, 1);
}

void png_warning(png_structp png, png_const_charp message) noexcept {
  auto& state = *static_cast<PngState*>(png_get_error_ptr(png));
  log_png_message(state, "warning", message);
  if (state.error)
    png_longjmp(png, 1);
}

void read_png_data(png_structp png, png_bytep data, png_size_t length) noexcept {
  auto& state = *static_cast<PngState*>(png_get_io_ptr(png));
  {
    auto result = state.device.read_full({reinterpret_cast<char*>(data), length});
    if (!result)
      state.error = std::move(result.error());
  }
  // Destroy the result before jumping back through the C decoder.
  if (state.error)
    png_longjmp(png, 1);
}

void write_png_data(png_structp png, png_bytep data, png_size_t length) noexcept {
  auto& state = *static_cast<PngState*>(png_get_io_ptr(png));
  {
    auto result = state.device.write_full({reinterpret_cast<char*>(data), length});
    if (!result)
      state.error = std::move(result.error());
  }
  if (state.error)
    png_longjmp(png, 1);
}

void flush_png_data(png_structp) noexcept {}

// Each jump target lives in a separate frame with only trivial locals. Owners,
// Images, row arrays and diagnostics live in the caller, never across a jump.
bool create_png_info(PngState& state) {
  if (setjmp(png_jmpbuf(state.png)))
    return false;
  state.info = png_create_info_struct(state.png);
  if (!state.info)
    return false;
  if (!state.writing) {
    state.end = png_create_info_struct(state.png);
    if (!state.end)
      return false;
  }
  return true;
}

std::expected<void, StarException> initialize_png(PngState& state) {
  if (state.writing)
    state.png = png_create_write_struct(PNG_LIBPNG_VER_STRING, &state, png_error, png_warning);
  else
    state.png = png_create_read_struct(PNG_LIBPNG_VER_STRING, &state, png_error, png_warning);
  auto created = state.check(state.png != nullptr, "Internal libPNG error");
  if (!created)
    return std::unexpected(std::move(created.error()));
  return state.check(create_png_info(state), "Internal libPNG error");
}

struct PngMetadata {
  png_uint_32 width;
  png_uint_32 height;
  png_uint_32 bitdepth;
  png_uint_32 channels;
};

bool read_png_info(PngState& state, PngMetadata& metadata, bool strip16) {
  if (setjmp(png_jmpbuf(state.png)))
    return false;
  png_set_read_fn(state.png, &state, read_png_data);
  png_set_sig_bytes(state.png, 8);
  png_read_info(state.png, state.info);

  metadata.width = png_get_image_width(state.png, state.info);
  metadata.height = png_get_image_height(state.png, state.info);
  metadata.bitdepth = png_get_bit_depth(state.png, state.info);
  metadata.channels = png_get_channels(state.png, state.info);
  auto colorType = png_get_color_type(state.png, state.info);

  if (colorType == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(state.png);
    metadata.channels = 3;
    metadata.bitdepth = 8;
  }
  if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
    if (metadata.bitdepth < 8) {
      png_set_expand_gray_1_2_4_to_8(state.png);
      metadata.bitdepth = 8;
    }
    png_set_gray_to_rgb(state.png);
    metadata.channels = colorType == PNG_COLOR_TYPE_GRAY_ALPHA ? 4 : 3;
  }
  if (png_get_valid(state.png, state.info, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(state.png);
    metadata.channels += 1;
  }
  if (strip16 && metadata.bitdepth == 16) {
    png_set_strip_16(state.png);
    metadata.bitdepth = 8;
  }
  return true;
}

bool read_png_rows(PngState& state, png_bytepp rows) {
  if (setjmp(png_jmpbuf(state.png)))
    return false;
  png_read_image(state.png, rows);
  return true;
}

bool write_png_rows(PngState& state, unsigned width, unsigned height, unsigned channels, png_bytepp rows) {
  if (setjmp(png_jmpbuf(state.png)))
    return false;
  png_set_IHDR(state.png, state.info, width, height, 8,
      channels == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA,
      PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_set_write_fn(state.png, &state, write_png_data, flush_png_data);
  png_set_rows(state.png, state.info, rows);
  png_write_png(state.png, state.info, PNG_TRANSFORM_IDENTITY, nullptr);
  return true;
}

std::expected<void, StarException> read_png_signature(IODevice& device) {
  png_byte header[8]{};
  auto result = device.read_full({reinterpret_cast<char*>(header), sizeof(header)});
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (png_sig_cmp(header, 0, sizeof(header))) {
    auto name = device.device_name();
    if (!name)
      return std::unexpected(std::move(name.error()));
    return std::unexpected(ImageException(Star::format("File {} is not a png image!", *name)));
  }
  return {};
}

template <std::size_t Channels>
concept byte_pixel_channels = Channels == 3 || Channels == 4;

template <std::size_t Channels>
  requires byte_pixel_channels<Channels>
void fill_pixels(std::span<std::uint8_t> output, std::size_t count, Vec4B color) {
  if (count == 0)
    return;
  auto pixels = output.first(count * Channels);
  if (color[0] == 0 && color[1] == 0 && color[2] == 0 && (Channels == 3 || color[3] == 0)) {
    std::ranges::fill(pixels, 0);
    return;
  }
  auto destination = pixels.begin();
  for (std::size_t pixel = 0; pixel != count; ++pixel, std::advance(destination, Channels)) {
    destination[0] = color[0];
    destination[1] = color[1];
    destination[2] = color[2];
    if constexpr (Channels == 4)
      destination[3] = color[3];
  }
}

// Source and destination must be disjoint.
template <std::size_t Source, std::size_t Destination>
  requires(byte_pixel_channels<Source> && byte_pixel_channels<Destination>)
void copy_pixels(std::span<std::uint8_t const> input, std::size_t count, std::span<std::uint8_t> output) {
  if (count == 0)
    return;
  if constexpr (Source == Destination) {
    std::ranges::copy(input.first(count * Source), output.begin());
  } else {
    auto source = input.first(count * Source).begin();
    auto destination = output.first(count * Destination).begin();
    for (std::size_t pixel = 0; pixel != count; ++pixel) {
      *destination++ = *source++;
      *destination++ = *source++;
      *destination++ = *source++;
      if constexpr (Source == 4)
        ++source;
      if constexpr (Destination == 4)
        *destination++ = 255;
    }
  }
}

// Pixel ranges are disjoint, but rows may overlap: keep forward feedback.
template <std::size_t Channels>
  requires byte_pixel_channels<Channels>
void copy_forward_pixels(std::span<std::uint8_t const> input, std::size_t count, std::span<std::uint8_t> output) {
  auto source = input.first(count * Channels).begin();
  auto destination = output.first(count * Channels).begin();
  for (std::size_t pixel = 0; pixel != count; ++pixel)
    std::ranges::copy_n(std::next(source, pixel * Channels), Channels, std::next(destination, pixel * Channels));
}

template <std::size_t Source, std::size_t Destination>
  requires(byte_pixel_channels<Source> && byte_pixel_channels<Destination>)
void draw_rows(std::span<std::uint8_t const> input, std::span<std::uint8_t> output, std::size_t input_stride,
               std::size_t output_stride, unsigned width, unsigned height) {
  for (unsigned y = 0; y != height; ++y) {
    auto input_row = input.subspan(std::size_t(y) * input_stride, std::size_t(width) * Source);
    auto output_row = output.subspan(std::size_t(y) * output_stride, std::size_t(width) * Destination);
    for (unsigned x = 0; x != width; ++x) {
      auto source = input_row.subspan(std::size_t(x) * Source, Source);
      auto destination = output_row.subspan(std::size_t(x) * Destination, Destination);
      unsigned source_alpha = Source == 4 ? source[3] : 255;
      unsigned destination_alpha = Destination == 4 ? destination[3] : 255;
      for (unsigned channel = 0; channel != 3; ++channel) {
        unsigned source_premultiplied = unsigned(source[channel]) * source_alpha / 255;
        unsigned destination_premultiplied = unsigned(destination[channel]) * destination_alpha / 255;
        destination[channel] =
          static_cast<std::uint8_t>(source_premultiplied + destination_premultiplied * (255 - source_alpha) / 255);
      }
      if constexpr (Destination == 4)
        destination[3] = static_cast<std::uint8_t>(source_alpha + destination_alpha * (255 - source_alpha) / 255);
    }
  }
}

}// namespace

std::expected<bool, StarException> Image::is_png(std::shared_ptr<IODevice> device) {
  png_byte header[8]{};
  auto read = device->read_absolute(0, {reinterpret_cast<char*>(header), sizeof(header)});
  if (!read)
    return std::unexpected(std::move(read.error()));
  return *read == sizeof(header) && !png_sig_cmp(header, 0, sizeof(header));
}

std::expected<Image, StarException> Image::read_png(Logger& logger, std::shared_ptr<IODevice> device) {
  auto signature = read_png_signature(*device);
  if (!signature)
    return std::unexpected(std::move(signature.error()));
  PngState state{logger, *device, false};
  auto initialized = initialize_png(state);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  PngMetadata metadata{};
  auto info = state.check(read_png_info(state, metadata, true), "Internal error reading png.");
  if (!info)
    return std::unexpected(std::move(info.error()));
  if (metadata.bitdepth != 8 || (metadata.channels != 3 && metadata.channels != 4))
    return std::unexpected(ImageException("Unsupported PNG pixel format"));

  Vec2U dimensions(metadata.width, metadata.height);
  auto format = metadata.channels == 3 ? PixelFormat::RGB24 : PixelFormat::RGBA32;
  auto layout = size_bytes(dimensions, format);
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  auto image = for_overwrite(dimensions, format);
  auto rows = std::make_unique_for_overwrite<png_bytep[]>(metadata.height);
  auto pixels = image.data();
  std::size_t stride = std::size_t(metadata.width) * metadata.channels;
  for (std::size_t i = 0; i < metadata.height; ++i)
    rows[i] = pixels.subspan((metadata.height - i - 1) * stride, stride).data();
  auto read = state.check(read_png_rows(state, rows.get()), "Internal error reading png.");
  if (!read)
    return std::unexpected(std::move(read.error()));
  return image;
}

std::expected<std::tuple<Vec2U, PixelFormat>, StarException>
Image::read_png_metadata(Logger& logger, std::shared_ptr<IODevice> device) {
  auto signature = read_png_signature(*device);
  if (!signature)
    return std::unexpected(std::move(signature.error()));
  PngState state{logger, *device, false};
  auto initialized = initialize_png(state);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  PngMetadata metadata{};
  auto info = state.check(read_png_info(state, metadata, false), "Internal error reading png.");
  if (!info)
    return std::unexpected(std::move(info.error()));
  auto dimensions = Vec2U(metadata.width, metadata.height);
  auto format = metadata.channels == 3 ? PixelFormat::RGB24 : PixelFormat::RGBA32;
  auto layout = size_bytes(dimensions, format);
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  return std::make_tuple(dimensions, format);
}

std::expected<std::size_t, StarException> Image::size_bytes(Vec2U size, PixelFormat format) {
  if (size[0] == 0 || size[1] == 0)
    return 0;
  auto channels = Star::bytes_per_pixel(format);
  auto maximum = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
  if (size[0] > maximum / channels)
    return std::unexpected(ImageException("Image row exceeds addressable storage"));
  auto stride = std::size_t(size[0]) * channels;
  if (size[1] > maximum / stride)
    return std::unexpected(ImageException("Image size exceeds addressable storage"));
  return stride * size[1];
}

Image Image::for_overwrite(Vec2U size, PixelFormat format) {
  Image image(format);
  image.reallocate(size, format, false);
  return image;
}

Image Image::filled(Vec2U size, Vec4B color, PixelFormat format) {
  auto channels = Star::bytes_per_pixel(format);
  Image image = channels == 3 || channels == 4 ? for_overwrite(size, format) : Image(size, format);
  image.fill(color);
  return image;
}

Image::Image(PixelFormat format) noexcept : m_data(nullptr), m_width(0), m_height(0), m_pixelFormat(format) {}

Image::Image(Vec2U size, PixelFormat format) : Image(size[0], size[1], format) {}

Image::Image(unsigned width, unsigned height, PixelFormat format) : Image(format) {
  reallocate({width, height}, format, true);
}

Image::~Image() = default;

Image::Image(Image const& image) : Image(image.m_pixelFormat) { operator=(image); }

Image::Image(Image&& image) noexcept
    : m_data(std::move(image.m_data)), m_width(std::exchange(image.m_width, 0)),
      m_height(std::exchange(image.m_height, 0)), m_pixelFormat(std::exchange(image.m_pixelFormat, PixelFormat{})) {}

Image& Image::operator=(Image const& image) {
  if (this == &image)
    return *this;
  if (size() != image.size() || m_pixelFormat != image.m_pixelFormat)
    reallocate(image.size(), image.m_pixelFormat, false);
  auto bytes = image.size_bytes();
  if (bytes != 0)
    std::ranges::copy(image.data(), data().begin());
  return *this;
}

Image& Image::operator=(Image&& image) noexcept {
  if (this == &image)
    return *this;
  m_data = std::move(image.m_data);
  m_width = std::exchange(image.m_width, 0);
  m_height = std::exchange(image.m_height, 0);
  m_pixelFormat = std::exchange(image.m_pixelFormat, PixelFormat{});
  return *this;
}

void Image::reset(Vec2U size, std::optional<PixelFormat> format) { reset(size[0], size[1], format); }

void Image::reset(unsigned width, unsigned height, std::optional<PixelFormat> format) {
  auto selected = format.value_or(m_pixelFormat);
  if (m_width != width || m_height != height || m_pixelFormat != selected)
    reallocate({width, height}, selected, true);
}

void Image::reallocate(Vec2U size, PixelFormat format, bool initialize) {
  auto layout = size_bytes(size, format);
  if (!layout) {
    Star::fatal_error(layout.error().what(), false);
    std::terminate();
  }
  auto bytes = *layout;
  if (bytes == 0) {
    m_data.reset();
  } else {
    if (bytes != size_bytes())
      m_data = std::make_unique_for_overwrite<std::uint8_t[]>(bytes);
    if (initialize)
      std::ranges::fill(std::span(m_data.get(), bytes), 0);
  }
  m_width = size[0];
  m_height = size[1];
  m_pixelFormat = format;
}

void Image::fill(Vec3B const& color) { fill(Vec4B(color, 255)); }

void Image::fill(Vec4B const& color) {
  auto count = std::size_t(m_width) * m_height;
  if (bytes_per_pixel() == 3) {
    fill_pixels<3>(data(), count, color);
  } else {
    // Preserve the historical packed byte setter path for float formats.
    if constexpr (Star::DebugEnabled) {
      if (count != 0)
        Star::star_assert_failed(bytes_per_pixel() == 4);
    }
    fill_pixels<4>(data(), count, color);
  }
}

std::expected<void, StarException> Image::fill_rect(Vec2U const& pos, Vec2U const& extent, Vec3B const& color) {
  return fill_rect(pos, extent, Vec4B(color, 255));
}

std::expected<void, StarException> Image::fill_rect(Vec2U const& pos, Vec2U const& extent, Vec4B const& color) {
  if (pos[0] >= m_width || pos[1] >= m_height)
    return {};
  auto width = std::min(extent[0], m_width - pos[0]);
  auto height = std::min(extent[1], m_height - pos[1]);
  auto channels = bytes_per_pixel();
  if (width == 0 || height == 0 || (channels != 3 && channels != 4))
    return {};
  auto stride = std::size_t(m_width) * channels;
  auto rows = data().subspan(std::size_t(pos[1]) * stride + std::size_t(pos[0]) * channels);
  for (unsigned y = 0; y != height; ++y) {
    auto output = rows.subspan(std::size_t(y) * stride, std::size_t(width) * channels);
    if (channels == 3)
      fill_pixels<3>(output, width, color);
    else
      fill_pixels<4>(output, width, color);
  }
  return {};
}

std::expected<void, StarException> Image::set(Vec2U const& pos, Vec4B const& color) {
  if (pos[0] >= m_width || pos[1] >= m_height)
    return std::unexpected(ImageException(Star::format("{} out of range in Image::set", pos)));
  if (bytes_per_pixel() == 4)
    set32(pos, color);
  else if (bytes_per_pixel() == 3)
    set24(pos, color.vec3());
  return {};
}

std::expected<void, StarException> Image::set(Vec2U const& pos, Vec3B const& color) {
  return set(pos, Vec4B(color, 255));
}

std::expected<Vec4B, StarException> Image::get(Vec2U const& pos) const {
  if (pos[0] >= m_width || pos[1] >= m_height)
    return std::unexpected(ImageException(Star::format("{} out of range in Image::get", pos)));
  if (bytes_per_pixel() == 4)
    return get32(pos[0], pos[1]);
  if (bytes_per_pixel() == 3)
    return Vec4B(get24(pos[0], pos[1]), 255);
  return Vec4B{};
}

std::expected<void, StarException> Image::set_rgb(Vec2U const& pos, Vec4B const& color) {
  if (m_pixelFormat == PixelFormat::BGR24 || m_pixelFormat == PixelFormat::BGRA32)
    return set(pos, Vec4B(color[2], color[1], color[0], color[3]));
  return set(pos, color);
}

std::expected<void, StarException> Image::set_rgb(Vec2U const& pos, Vec3B const& color) {
  return set_rgb(pos, Vec4B(color, 255));
}

std::expected<Vec4B, StarException> Image::get_rgb(Vec2U const& pos) const {
  auto color = get(pos);
  if (color && (m_pixelFormat == PixelFormat::BGR24 || m_pixelFormat == PixelFormat::BGRA32))
    std::swap((*color)[0], (*color)[2]);
  return color;
}

Vec4B Image::clamp(Vec2I const& pos) const {
  if (empty())
    return {};
  auto x = std::min(static_cast<unsigned>(std::max(pos[0], 0)), m_width - 1);
  auto y = std::min(static_cast<unsigned>(std::max(pos[1], 0)), m_height - 1);
  if (bytes_per_pixel() == 4)
    return get32(x, y);
  if (bytes_per_pixel() == 3)
    return Vec4B(get24(x, y), 255);
  return {};
}

Vec4B Image::clamp_rgb(Vec2I const& pos) const {
  auto color = clamp(pos);
  if (m_pixelFormat == PixelFormat::BGR24 || m_pixelFormat == PixelFormat::BGRA32)
    std::swap(color[0], color[2]);
  return color;
}

std::expected<Image, StarException> Image::sub_image(Vec2U const& pos, Vec2U const& extent) const {
  if (pos[0] > m_width || pos[1] > m_height || extent[0] > m_width - pos[0] || extent[1] > m_height - pos[1])
    return std::unexpected(ImageException(
      Star::format("sub_image with pos {} size {} out of image bounds ({}, {})", pos, extent, m_width, m_height)));
  auto channels = bytes_per_pixel();
  bool byte_format = channels == 3 || channels == 4;
  Image sub = byte_format ? for_overwrite(extent, m_pixelFormat) : Image(extent, m_pixelFormat);
  if (sub.empty())
    return sub;
  if (byte_format) {
    auto source_stride = std::size_t(m_width) * channels;
    auto row_bytes = std::size_t(extent[0]) * channels;
    auto input = data().subspan(std::size_t(pos[1]) * source_stride + std::size_t(pos[0]) * channels);
    auto output = sub.data();
    for (unsigned y = 0; y != extent[1]; ++y)
      std::ranges::copy(input.subspan(std::size_t(y) * source_stride, row_bytes),
                        output.subspan(std::size_t(y) * row_bytes, row_bytes).begin());
  } else {
    for (unsigned y = 0; y != extent[1]; ++y)
      for (unsigned x = 0; x != extent[0]; ++x) {
        auto pixel = get(pos + Vec2U(x, y));
        if (!pixel)
          return std::unexpected(std::move(pixel.error()));
        auto assigned = sub.set(x, y, *pixel);
        if (!assigned)
          return std::unexpected(std::move(assigned.error()));
      }
  }
  return sub;
}

std::expected<void, StarException> Image::copy_into(Vec2U const& pos, Image const& image) {
  if (pos[0] >= m_width || pos[1] >= m_height || image.empty())
    return {};
  if (this == &image && pos == Vec2U())
    return {};
  auto width = std::min(image.width(), m_width - pos[0]);
  auto height = std::min(image.height(), m_height - pos[1]);
  auto from = image.bytes_per_pixel(), to = bytes_per_pixel();
  if ((from == 3 || from == 4) && (to == 3 || to == 4)) {
    auto input_stride = std::size_t(image.width()) * from;
    auto output_stride = std::size_t(m_width) * to;
    auto input = image.data();
    auto output = data().subspan(std::size_t(pos[1]) * output_stride + std::size_t(pos[0]) * to);
    for (unsigned y = 0; y != height; ++y) {
      auto input_row = input.subspan(std::size_t(y) * input_stride, std::size_t(width) * from);
      auto output_row = output.subspan(std::size_t(y) * output_stride, std::size_t(width) * to);
      if (this == &image) {
        if (from == 3)
          copy_forward_pixels<3>(input_row, width, output_row);
        else
          copy_forward_pixels<4>(input_row, width, output_row);
      } else if (from == to)
        std::ranges::copy(input_row, output_row.begin());
      else if (from == 3)
        copy_pixels<3, 4>(input_row, width, output_row);
      else
        copy_pixels<4, 3>(input_row, width, output_row);
    }
    return {};
  }
  // Retain legacy float getter/setter behavior.
  for (unsigned y = 0; y != height; ++y)
    for (unsigned x = 0; x != width; ++x) {
      auto pixel = image.get(x, y);
      if (!pixel)
        return std::unexpected(std::move(pixel.error()));
      auto assigned = set(pos[0] + x, pos[1] + y, *pixel);
      if (!assigned)
        return std::unexpected(std::move(assigned.error()));
    }
  return {};
}

std::expected<void, StarException> Image::draw_into(Vec2U const& pos, Image const& image) {
  if (pos[0] >= m_width || pos[1] >= m_height || image.empty())
    return {};
  auto width = std::min(image.width(), m_width - pos[0]);
  auto height = std::min(image.height(), m_height - pos[1]);
  auto from = image.bytes_per_pixel(), to = bytes_per_pixel();
  if ((from == 3 || from == 4) && (to == 3 || to == 4)) {
    auto input_stride = std::size_t(image.width()) * from;
    auto output_stride = std::size_t(m_width) * to;
    auto input = image.data();
    auto output = data().subspan(std::size_t(pos[1]) * output_stride + std::size_t(pos[0]) * to);
    if (from == 3 && to == 3)
      draw_rows<3, 3>(input, output, input_stride, output_stride, width, height);
    else if (from == 3)
      draw_rows<3, 4>(input, output, input_stride, output_stride, width, height);
    else if (to == 3)
      draw_rows<4, 3>(input, output, input_stride, output_stride, width, height);
    else
      draw_rows<4, 4>(input, output, input_stride, output_stride, width, height);
    return {};
  }
  for (unsigned y = 0; y != height; ++y)
    for (unsigned x = 0; x != width; ++x) {
      auto destination = get(pos[0] + x, pos[1] + y);
      if (!destination)
        return std::unexpected(std::move(destination.error()));
      auto source = image.get(x, y);
      if (!source)
        return std::unexpected(std::move(source.error()));
      Vec3U destination_premultiplied =
        Vec3U((*destination)[0], (*destination)[1], (*destination)[2]) * (*destination)[3] / 255;
      Vec3U source_premultiplied = Vec3U((*source)[0], (*source)[1], (*source)[2]) * (*source)[3] / 255;
      auto over = source_premultiplied + destination_premultiplied * (255 - (*source)[3]) / 255;
      unsigned alpha = (*source)[3] + (*destination)[3] * (255 - (*source)[3]) / 255;
      auto assigned = set(pos[0] + x, pos[1] + y, Vec4B(over[0], over[1], over[2], alpha));
      if (!assigned)
        return std::unexpected(std::move(assigned.error()));
    }
  return {};
}

Image Image::convert(PixelFormat format) const {
  auto from = bytes_per_pixel(), to = Star::bytes_per_pixel(format);
  if ((from == 3 || from == 4) && (to == 3 || to == 4)) {
    auto converted = for_overwrite(size(), format);
    auto count = std::size_t(m_width) * m_height;
    if (from == 3 && to == 3)
      copy_pixels<3, 3>(data(), count, converted.data());
    else if (from == 3)
      copy_pixels<3, 4>(data(), count, converted.data());
    else if (to == 3)
      copy_pixels<4, 3>(data(), count, converted.data());
    else
      copy_pixels<4, 4>(data(), count, converted.data());
    return converted;
  }
  Image converted(size(), format);
  for (unsigned y = 0; y < m_height; ++y)
    for (unsigned x = 0; x < m_width; ++x) {
      auto pixel = from == 4 ? get32(x, y) : Vec4B(get24(x, y), 255);
      if (to == 4)
        converted.set32(x, y, pixel);
      else
        converted.set24(x, y, pixel.vec3());
    }
  return converted;
}

std::expected<void, StarException> Image::write_png(Logger& logger, std::shared_ptr<IODevice> device) const {
  if (empty())
    return std::unexpected(ImageException("Cannot write an empty PNG image"));
  PngState state{logger, *device, true};
  auto initialized = initialize_png(state);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  unsigned channels = bytes_per_pixel() == 3 ? 3 : 4;
  auto rows = std::make_unique_for_overwrite<png_bytep[]>(m_height);
  std::size_t stride = std::size_t(m_width) * channels;
  auto pixels = data();
  for (std::size_t i = 0; i < m_height; ++i)
    rows[i] = const_cast<png_bytep>(pixels.subspan((m_height - i - 1) * stride, stride).data());
  return state.check(write_png_rows(state, m_width, m_height, channels, rows.get()), "Internal error writing png.");
}

ImageView::ImageView(Image const& image) : size(image.size()), data(image.data()), format(image.pixel_format()) {}
}
