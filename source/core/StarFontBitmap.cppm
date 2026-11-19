module;

#include <ft2build.h>
#include FT_FREETYPE_H

export module star.font_bitmap;

import std;
import star.font;
import star.format;
import star.image;
import star.math_common;
import star.vector;

export namespace Star {

// FT owns the bitmap and its storage until the next glyph load.
[[nodiscard]] std::expected<RenderedGlyph, StarException> render_font_bitmap(FT_Bitmap const& bitmap,
    int left, int top, unsigned pixel_size, std::uint8_t alpha_threshold) {
  if (!bitmap.width || !bitmap.rows)
    return RenderedGlyph{};
  if (!bitmap.buffer)
    return std::unexpected(FontException("Missing font bitmap buffer"));
  bool const colored = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA;
  if (!colored && bitmap.pixel_mode != FT_PIXEL_MODE_GRAY)
    return std::unexpected(FontException::format("Unsupported font bitmap pixel mode: {}", bitmap.pixel_mode));

  if (bitmap.width > std::numeric_limits<unsigned>::max() - 2 || bitmap.rows > std::numeric_limits<unsigned>::max() - 2)
    return std::unexpected(FontException("Font bitmap border overflows dimensions"));
  Vec2U const size(bitmap.width + 2, bitmap.rows + 2);
  if (auto bytes = Image::size_bytes(size, PixelFormat::BGRA32); !bytes)
    return std::unexpected(std::move(bytes.error()));
  auto const offset_x = std::int64_t(left) - 1;
  auto const offset_y = std::int64_t(top) - bitmap.rows + pixel_size / 4 - 1;
  if (!std::in_range<int>(offset_x) || !std::in_range<int>(offset_y))
    return std::unexpected(FontException("Font bitmap offset out of range"));

  // FT_Bitmap::buffer is the lowest address for either flow. Negative pitch
  // stores the bottom row there, not a top-row pointer into the allocation.
  auto const stride = static_cast<std::uint64_t>(std::abs(std::int64_t(bitmap.pitch)));
  auto const row_bytes = std::uint64_t(bitmap.width) * (colored ? 4 : 1);
  auto const extent = (bitmap.rows - 1) * stride + row_bytes;
  if (stride < row_bytes || std::cmp_greater(extent, std::numeric_limits<std::ptrdiff_t>::max()))
    return std::unexpected(FontException("Invalid font bitmap pitch"));
  std::span<std::uint8_t const> const pixels(bitmap.buffer, static_cast<std::size_t>(extent));

  auto image = Image::for_overwrite(size, PixelFormat::BGRA32);
  auto const border = colored ? Vec4B(0, 0, 0, 0) : Vec4B(255, 255, 255, 0);
  for (unsigned x = 0; x != size[0]; ++x) {
    image.set32(x, 0, border);
    image.set32(x, size[1] - 1, border);
  }
  for (unsigned y = 1; y <= bitmap.rows; ++y) {
    auto const source_y = bitmap.pitch < 0 ? y - 1 : bitmap.rows - y;
    auto const row = pixels.subspan(static_cast<std::size_t>(source_y * stride), static_cast<std::size_t>(row_bytes));
    image.set32(0, y, border);
    image.set32(size[0] - 1, y, border);
    if (colored) {
      for (unsigned x = 0; x != bitmap.width; ++x) {
        auto const i = std::size_t(x) * 4;
        Vec4B pixel(row[i], row[i + 1], row[i + 2], row[i + 3]);
        if (pixel[3] != 0 && pixel[3] != 255) {
#pragma float_control(precise, on)
          // Keep the legacy float division followed by truncation to a byte.
          float const a = static_cast<float>(pixel[3]) / 255.0f;
          pixel[0] /= a;
          pixel[1] /= a;
          pixel[2] /= a;
        }
        image.set32(x + 1, y, pixel);
      }
    } else {
      for (unsigned x = 0; x != bitmap.width; ++x) {
        auto const coverage = row[x];
        image.set32(x + 1, y, {255, 255, 255, alpha_threshold ? std::uint8_t(coverage >= alpha_threshold ? 255 : 0) : coverage});
      }
    }
  }
  return RenderedGlyph{std::move(image), Vec2I(static_cast<int>(offset_x), static_cast<int>(offset_y)), colored};
}

}
