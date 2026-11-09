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

[[nodiscard]] std::expected<Image, StarException> scaleNearest(Image const& srcImage, Vec2F const& scale);
[[nodiscard]] std::expected<Image, StarException> scaleBilinear(Image const& srcImage, Vec2F const& scale);
[[nodiscard]] std::expected<Image, StarException> scaleBicubic(Image const& srcImage, Vec2F const& scale);

// Compute only a window of the scaled image, byte-equivalent to scaling then
// subImage(). Empty windows are allowed; invalid windows return an error.
bool scaledCropFits(Image const& srcImage, Vec2F const& scale, RectI const& window);
[[nodiscard]] std::expected<Image, StarException> scaleNearestCropped(Image const& srcImage, Vec2F const& scale,
                                                                      RectI const& window);
[[nodiscard]] std::expected<Image, StarException> scaleBilinearCropped(Image const& srcImage, Vec2F const& scale,
                                                                       RectI const& window);
}

namespace Star {

static Vec2U scaledSize(Image const& srcImage, Vec2F const& scale) {
  Vec2U size = Vec2U::round(vmult(Vec2F(srcImage.size()), scale));
  size[0] = std::max(size[0], 1u);
  size[1] = std::max(size[1], 1u);
  return size;
}

static std::expected<Image, StarException> nearestWindow(Image const& srcImage, Vec2F const& scale, Vec2U const& offset,
                                                         Vec2U const& destSize) {
  Vec2U srcSize = srcImage.size();
  Image destImage(destSize, srcImage.pixelFormat());

  // A newly allocated image is zeroed, matching clamp()'s transparent black
  // for an empty source (with implicit opaque alpha in the 24-bit formats).
  if (srcImage.empty() || destImage.empty())
    return destImage;

  auto bpp = srcImage.bytesPerPixel();
  if (bpp == 3 || bpp == 4) {
    // Keep actual division: fast-math can otherwise hoist a reciprocal out of
    // the loop and move a source coordinate across a rounding half-boundary.
    volatile float scaleX = scale[0], scaleY = scale[1];
    int maxX = static_cast<int>(srcSize[0]) - 1;
    int maxY = static_cast<int>(srcSize[1]) - 1;
    std::vector<std::size_t> columns(destSize[0]);
    for (unsigned x = 0; x < destSize[0]; ++x) {
      int sourceX = scale[0] == 0.0f ? 0 : static_cast<int>(std::round(static_cast<float>(offset[0] + x) / scaleX));
      columns[x] = static_cast<std::size_t>(std::clamp(sourceX, 0, maxX)) * bpp;
    }

    auto source = srcImage.data();
    auto destination = destImage.data();
    std::size_t sourceStride = static_cast<std::size_t>(srcSize[0]) * bpp;
    std::size_t destinationStride = static_cast<std::size_t>(destSize[0]) * bpp;
    for (unsigned y = 0; y < destSize[1]; ++y) {
      int sourceY = scale[1] == 0.0f ? 0 : static_cast<int>(std::round(static_cast<float>(offset[1] + y) / scaleY));
      auto sourceRow = source.data() + static_cast<std::size_t>(std::clamp(sourceY, 0, maxY)) * sourceStride;
      auto destinationRow = destination.data() + static_cast<std::size_t>(y) * destinationStride;
      for (unsigned x = 0; x < destSize[0]; ++x)
        std::memcpy(destinationRow + static_cast<std::size_t>(x) * bpp, sourceRow + columns[x], bpp);
    }
    return destImage;
  }

  // Float formats do not have byte-equivalent clamp()/set() semantics.
  // Preserve their existing scalar behavior rather than copying float pixels.

  for (unsigned y = 0; y < destSize[1]; ++y) {
    for (unsigned x = 0; x < destSize[0]; ++x) {
      auto assigned = destImage.set({x, y}, srcImage.clamp(Vec2I::round(vdiv(Vec2F(offset + Vec2U(x, y)), scale))));
      if (!assigned)
        return std::unexpected(std::move(assigned.error()));
    }
  }
  return destImage;
}

std::expected<Image, StarException> scaleNearest(Image const& srcImage, Vec2F const& scale) {
  return nearestWindow(srcImage, scale, Vec2U(), scaledSize(srcImage, scale));
}

static Vec4B bilinearPixel(Image const& srcImage, Vec2F const& scale, unsigned x, unsigned y) {
  auto pos = vdiv(Vec2F(x, y), scale);
  auto ipart = Vec2I::floor(pos);
  auto fpart = pos - Vec2F(ipart);

  auto result = lerp(
    fpart[1], lerp(fpart[0], Vec4F(srcImage.clamp(ipart[0], ipart[1])), Vec4F(srcImage.clamp(ipart[0] + 1, ipart[1]))),
    lerp(fpart[0], Vec4F(srcImage.clamp(ipart[0], ipart[1] + 1)), Vec4F(srcImage.clamp(ipart[0] + 1, ipart[1] + 1))));
  return Vec4B(result);
}

static std::expected<Image, StarException> bilinearWindow(Image const& srcImage, Vec2F const& scale,
                                                          Vec2U const& offset, Vec2U const& destSize) {
  Image destImage(destSize, srcImage.pixelFormat());
  if (destImage.empty())
    return destImage;

  for (unsigned y = 0; y < destSize[1]; ++y) {
    for (unsigned x = 0; x < destSize[0]; ++x) {
      auto assigned = destImage.set({x, y}, bilinearPixel(srcImage, scale, offset[0] + x, offset[1] + y));
      if (!assigned)
        return std::unexpected(std::move(assigned.error()));
    }
  }

  return destImage;
}

std::expected<Image, StarException> scaleBilinear(Image const& srcImage, Vec2F const& scale) {
  return bilinearWindow(srcImage, scale, Vec2U(), scaledSize(srcImage, scale));
}

bool scaledCropFits(Image const& srcImage, Vec2F const& scale, RectI const& window) {
  // Check min/max, not size() or min + size: malformed rectangles can overflow
  // signed subtraction/addition before a bounds check gets to reject them.
  auto min = window.min();
  auto max = window.max();
  for (unsigned axis = 0; axis < 2; ++axis) {
    if (min[axis] < 0 || max[axis] < min[axis])
      return false;
    // Bit checks remain meaningful with fast-math, unlike isfinite().
    if ((std::bit_cast<std::uint32_t>(scale[axis]) & 0x7f800000u) == 0x7f800000u || scale[axis] < 0.0f)
      return false;
  }

  auto extent = vmult(Vec2F(srcImage.size()), scale);
  for (unsigned axis = 0; axis < 2; ++axis) {
    if ((std::bit_cast<std::uint32_t>(extent[axis]) & 0x7f800000u) == 0x7f800000u)
      return false;
    auto rounded = std::round(extent[axis]);
    if (static_cast<double>(rounded) > static_cast<double>(std::numeric_limits<unsigned>::max()))
      return false;
    auto size = std::max(static_cast<unsigned>(rounded), 1u);
    if (static_cast<unsigned>(max[axis]) > size)
      return false;
  }
  return true;
}

std::expected<Image, StarException> scaleNearestCropped(Image const& srcImage, Vec2F const& scale,
                                                        RectI const& window) {
  if (!scaledCropFits(srcImage, scale, window))
    return std::unexpected(ImageException(Star::format("crop window {} out of scaled image bounds", window)));
  return nearestWindow(srcImage, scale, Vec2U(window.min()), Vec2U(window.size()));
}

std::expected<Image, StarException> scaleBilinearCropped(Image const& srcImage, Vec2F const& scale,
                                                         RectI const& window) {
  if (!scaledCropFits(srcImage, scale, window))
    return std::unexpected(ImageException(Star::format("crop window {} out of scaled image bounds", window)));
  return bilinearWindow(srcImage, scale, Vec2U(window.min()), Vec2U(window.size()));
}

std::expected<Image, StarException> scaleBicubic(Image const& srcImage, Vec2F const& scale) {
  Vec2U srcSize = srcImage.size();
  Vec2U destSize = Vec2U::round(vmult(Vec2F(srcSize), scale));
  destSize[0] = std::max(destSize[0], 1u);
  destSize[1] = std::max(destSize[1], 1u);

  Image destImage(destSize, srcImage.pixelFormat());

  for (unsigned y = 0; y < destSize[1]; ++y) {
    for (unsigned x = 0; x < destSize[0]; ++x) {
      auto pos = vdiv(Vec2F(x, y), scale);
      auto ipart = Vec2I::floor(pos);
      auto fpart = pos - Vec2F(ipart);

      Vec4F a = cubic4(fpart[0],
          Vec4F(srcImage.clamp(ipart[0], ipart[1])),
          Vec4F(srcImage.clamp(ipart[0] + 1, ipart[1])),
          Vec4F(srcImage.clamp(ipart[0] + 2, ipart[1])),
          Vec4F(srcImage.clamp(ipart[0] + 3, ipart[1])));

      Vec4F b = cubic4(fpart[0],
          Vec4F(srcImage.clamp(ipart[0], ipart[1] + 1)),
          Vec4F(srcImage.clamp(ipart[0] + 1, ipart[1] + 1)),
          Vec4F(srcImage.clamp(ipart[0] + 2, ipart[1] + 1)),
          Vec4F(srcImage.clamp(ipart[0] + 3, ipart[1] + 1)));

      Vec4F c = cubic4(fpart[0],
          Vec4F(srcImage.clamp(ipart[0], ipart[1] + 2)),
          Vec4F(srcImage.clamp(ipart[0] + 1, ipart[1] + 2)),
          Vec4F(srcImage.clamp(ipart[0] + 2, ipart[1] + 2)),
          Vec4F(srcImage.clamp(ipart[0] + 3, ipart[1] + 2)));

      Vec4F d = cubic4(fpart[0],
          Vec4F(srcImage.clamp(ipart[0], ipart[1] + 3)),
          Vec4F(srcImage.clamp(ipart[0] + 1, ipart[1] + 3)),
          Vec4F(srcImage.clamp(ipart[0] + 2, ipart[1] + 3)),
          Vec4F(srcImage.clamp(ipart[0] + 3, ipart[1] + 3)));

      auto result = cubic4(fpart[1], a, b, c, d);

      auto assigned = destImage.set({x, y},
                                    Vec4B(std::clamp(result[0], 0.0f, 255.0f), std::clamp(result[1], 0.0f, 255.0f),
                                          std::clamp(result[2], 0.0f, 255.0f), std::clamp(result[3], 0.0f, 255.0f)));
      if (!assigned)
        return std::unexpected(std::move(assigned.error()));
    }
  }

  return destImage;
}

}
