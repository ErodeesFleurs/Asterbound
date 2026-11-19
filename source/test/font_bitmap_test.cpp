#include <ft2build.h>
#include FT_FREETYPE_H
import std;
import star.font;
import star.font_bitmap;
import star.format;
import star.image;
import star.math_common;
import star.vector;
import star.string;

#include "gtest/gtest.h"

using namespace Star;

namespace {
void check_border(Image const& image, Vec4B color) {
  for (unsigned x = 0; x < image.width(); ++x) {
    EXPECT_EQ(image.get32(x, 0), color);
    EXPECT_EQ(image.get32(x, image.height() - 1), color);
  }
  for (unsigned y = 1; y < image.height() - 1; ++y) {
    EXPECT_EQ(image.get32(0, y), color);
    EXPECT_EQ(image.get32(image.width() - 1, y), color);
  }
}

// Compare with FreeType's original top-down scanlines rather than a font hash:
// hinter differences between supported library versions aren't regressions.
void check_real_glyph(String const& path, String::Char ch, bool colored, std::uint8_t threshold) {
  FT_Library raw_library{};
  ASSERT_EQ(FT_Init_FreeType(&raw_library), 0);
  std::unique_ptr<FT_LibraryRec_, decltype(&FT_Done_FreeType)> library(raw_library, FT_Done_FreeType);
  FT_Face raw_face{};
  ASSERT_EQ(FT_New_Face(library.get(), path.utf8().c_str(), 0, &raw_face), 0);
  std::unique_ptr<FT_FaceRec, decltype(&FT_Done_Face)> face(raw_face, FT_Done_Face);
  constexpr unsigned size = 25;
  ASSERT_EQ(FT_Set_Pixel_Sizes(face.get(), size, 0), 0);
  ASSERT_NE(FT_Get_Char_Index(face.get(), ch), 0u);
  ASSERT_EQ(FT_Load_Char(face.get(), ch, FT_LOAD_COLOR | FT_LOAD_NO_SVG | FT_LOAD_RENDER), 0);
  auto const& bitmap = face->glyph->bitmap;
  ASSERT_GT(bitmap.pitch, 0);
  ASSERT_GT(bitmap.rows, 0u);
  ASSERT_EQ(bitmap.pixel_mode, colored ? FT_PIXEL_MODE_BGRA : FT_PIXEL_MODE_GRAY);

  auto owner = FontLibrary::create();
  ASSERT_TRUE(owner);
  auto font = Font::load(*owner, path, size);
  ASSERT_TRUE(font);
  (*font)->set_alpha_threshold(threshold);
  auto glyph = (*font)->render(ch);
  ASSERT_TRUE(glyph);
  EXPECT_EQ(glyph->colored, colored);
  EXPECT_EQ(glyph->offset, Vec2I(face->glyph->bitmap_left - 1,
      face->glyph->bitmap_top - static_cast<int>(bitmap.rows) + static_cast<int>(size / 4) - 1));
  ASSERT_EQ(glyph->image.size(), Vec2U(bitmap.width + 2, bitmap.rows + 2));
  check_border(glyph->image, colored ? Vec4B(0, 0, 0, 0) : Vec4B(255, 255, 255, 0));
  for (unsigned y = 0; y < bitmap.rows; ++y) {
    auto const* row = bitmap.buffer + std::size_t(y) * bitmap.pitch;
    for (unsigned x = 0; x < bitmap.width; ++x) {
      Vec4B expected(255, 255, 255, 0);
      if (colored) {
        auto const* src = row + x * 4;
        expected = {src[0], src[1], src[2], src[3]};
        if (src[3] != 0 && src[3] != 255) {
#pragma float_control(precise, on)
          float const alpha = static_cast<float>(src[3]) / 255.0f;
          for (unsigned i = 0; i < 3; ++i)
            expected[i] /= alpha;
        }
      } else {
        expected[3] = threshold ? (row[x] >= threshold ? 255 : 0) : row[x];
      }
      ASSERT_EQ(glyph->image.get32(x + 1, bitmap.rows - y), expected) << x << ',' << y;
    }
  }
}
}

TEST(FontBitmapTest, BundledGrayAndColorFacesMatchFreeTypePixelsAndBearings) {
  check_real_glyph(STAR_BUNDLED_FONTS "/hobo.ttf", U'g', false, 0);
  check_real_glyph(STAR_BUNDLED_FONTS "/hobo.ttf", U'g', false, 120);
  check_real_glyph(STAR_BUNDLED_FONTS "/font/twemoji.woff2", U'\U0001f600', true, 240);
}

TEST(FontBitmapTest, PaddedGrayRowsPreserveOrientationAndTransparentWhite) {
  // No trailing padding in the last physical row. The buffer always starts at
  // the first allocated byte, even for negative pitch.
  std::array<std::uint8_t, 7> down{0, 127, 191, 192, 193, 128, 255}, up{128, 255, 191, 192, 193, 0, 127};
  for (int pitch : {5, -5}) {
    FT_Bitmap bitmap{};
    bitmap.width = bitmap.rows = 2;
    bitmap.pitch = pitch;
    bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
    bitmap.buffer = pitch > 0 ? down.data() : up.data();
    for (std::uint8_t threshold : {0, 128}) {
      auto glyph = render_font_bitmap(bitmap, -2, -1, 12, threshold);
      ASSERT_TRUE(glyph);
      EXPECT_FALSE(glyph->colored);
      EXPECT_EQ(glyph->offset, Vec2I(-3, -1));
      ASSERT_EQ(glyph->image.size(), Vec2U(4, 4));
      check_border(glyph->image, {255, 255, 255, 0});
      EXPECT_EQ(glyph->image.get32(1, 2), Vec4B(255, 255, 255, 0));
      EXPECT_EQ(glyph->image.get32(2, 2), Vec4B(255, 255, 255, threshold ? 0 : 127));
      EXPECT_EQ(glyph->image.get32(1, 1), Vec4B(255, 255, 255, threshold ? 255 : 128));
      EXPECT_EQ(glyph->image.get32(2, 1), Vec4B(255, 255, 255, 255));
    }
  }
}

TEST(FontBitmapTest, MisalignedBgraRowsIgnorePaddingAndUnpremultiplyOnlyPartialAlpha) {
  std::array<std::uint8_t, 19> down{0,0,0,0, 1,64,32,128, 201,202,203, 4,5,6,255, 1,2,3,4};
  std::array<std::uint8_t, 19> up{4,5,6,255, 1,2,3,4, 201,202,203, 0,0,0,0, 1,64,32,128};
  for (int pitch : {11, -11}) {
    FT_Bitmap bitmap{};
    bitmap.width = bitmap.rows = 2;
    bitmap.pitch = pitch;
    bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
    bitmap.buffer = pitch > 0 ? down.data() : up.data();
    auto glyph = render_font_bitmap(bitmap, 5, 7, 12, 240);
    ASSERT_TRUE(glyph);
    EXPECT_TRUE(glyph->colored);
    EXPECT_EQ(glyph->offset, Vec2I(4, 7));
    ASSERT_EQ(glyph->image.size(), Vec2U(4, 4));
    EXPECT_EQ(glyph->image.pixel_format(), PixelFormat::BGRA32);
    check_border(glyph->image, {0, 0, 0, 0});
    EXPECT_EQ(glyph->image.get32(1, 2), Vec4B(0, 0, 0, 0));
    EXPECT_EQ(glyph->image.get32(2, 2), Vec4B(1, 127, 63, 128));
    EXPECT_EQ(glyph->image.get32(1, 1), Vec4B(4, 5, 6, 255));
    EXPECT_EQ(glyph->image.get32(2, 1), Vec4B(63, 127, 191, 4));
  }
}

TEST(FontBitmapTest, EmptyAndMalformedLayoutsAreHandledBeforePixelAccess) {
  FT_Bitmap bitmap{};
  auto empty = render_font_bitmap(bitmap, 1, 2, 12, 0);
  ASSERT_TRUE(empty);
  EXPECT_TRUE(empty->image.empty());
  EXPECT_FALSE(empty->colored);
  EXPECT_EQ(empty->offset, Vec2I(0, 0));
  bitmap.width = bitmap.rows = 1;
  bitmap.pitch = 4;
  bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
  EXPECT_FALSE(render_font_bitmap(bitmap, 0, 0, 12, 0)); // No buffer.
  std::array<std::uint8_t, 4> pixel{};
  bitmap.buffer = pixel.data();
  bitmap.pitch = -3;
  EXPECT_FALSE(render_font_bitmap(bitmap, 0, 0, 12, 0)); // Truncated active row.
  bitmap.pitch = std::numeric_limits<int>::min();
  EXPECT_TRUE(render_font_bitmap(bitmap, 0, 0, 12, 0)); // Single row: no stride step.
  EXPECT_FALSE(render_font_bitmap(bitmap, std::numeric_limits<int>::min(), 0, 12, 0));
  EXPECT_FALSE(render_font_bitmap(bitmap, 0, std::numeric_limits<int>::min(), 1, 0));
  bitmap.pixel_mode = FT_PIXEL_MODE_MONO;
  EXPECT_FALSE(render_font_bitmap(bitmap, 0, 0, 12, 0));
  bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
  bitmap.width = std::numeric_limits<unsigned>::max();
  EXPECT_FALSE(render_font_bitmap(bitmap, 0, 0, 12, 0)); // Border overflow.
  bitmap.rows = bitmap.width = std::numeric_limits<unsigned>::max() - 2;
  EXPECT_FALSE(render_font_bitmap(bitmap, 0, 0, 12, 0)); // Byte-count overflow.
}
