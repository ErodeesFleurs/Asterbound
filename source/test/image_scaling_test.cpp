import std;
import star.image_scaling;
import star.image;
import star.rect;
import star.directives;
import star.vector;
import star.string;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

namespace {
// Keep the reference's binary32 division exact even when surrounding tests use
// fast-math: packed reciprocal refinement can cross a nearest-pixel boundary.
#pragma float_control(precise, on, push)
[[gnu::noinline]] int scalarNearestIndex(unsigned position, float scale) {
  return static_cast<int>(std::round(static_cast<float>(position) / scale));
}
#pragma float_control(pop)

// Preserve the original byte-oriented get/set behavior without vector division.
std::expected<Image, StarException> scalarNearest(Image const& source, Vec2F const& scale) {
  Vec2U size = Vec2U::round(vmult(Vec2F(source.size()), scale));
  size[0] = std::max(size[0], 1u);
  size[1] = std::max(size[1], 1u);
  Image result(size, source.pixel_format());
  for (unsigned y = 0; y < size[1]; ++y) {
    for (unsigned x = 0; x < size[0]; ++x) {
      auto pixel = source.clamp(Vec2I(scalarNearestIndex(x, scale[0]), scalarNearestIndex(y, scale[1])));
      auto assigned = result.set(x, y, pixel);
      if (!assigned)
        return std::unexpected(std::move(assigned.error()));
    }
  }
  return result;
}

Image patternedImage(Vec2U size, PixelFormat format) {
  Image image(size, format);
  for (unsigned y = 0; y < size[1]; ++y) {
    for (unsigned x = 0; x < size[0]; ++x) {
      EXPECT_TRUE(image.set_rgb(x, y,
                                Vec4B(17 + 7 * x, 31 + 11 * y, 213 - 3 * x - 2 * y,
                                      (x + y) % 3 == 0     ? 0
                                        : (x + y) % 3 == 1 ? 71
                                                           : 255)));
    }
  }
  return image;
}

void expectSameImage(Image const& actual, Image const& expected) {
  ASSERT_EQ(actual.size(), expected.size());
  ASSERT_EQ(actual.pixel_format(), expected.pixel_format());
  ASSERT_EQ(actual.data().size(), expected.data().size());
  EXPECT_TRUE(std::equal(actual.data().begin(), actual.data().end(), expected.data().begin()));
}

void expectCroppedMatchesFull(Image const& source, Vec2F const& scale) {
  for (bool nearest : {true, false}) {
    auto full = nearest ? scale_nearest(source, scale) : scale_bilinear(source, scale);
    ASSERT_TRUE(full);
    Vec2I end(full->size());
    Vec2I inset(end[0] / 3, end[1] / 3);
    std::array windows{RectI(Vec2I(), end),         RectI(inset, end), RectI(end - Vec2I(1, 1), end),
                       RectI(Vec2I(), Vec2I(1, 1)), RectI(end, end),   RectI(Vec2I(end[0], 0), end),
                       RectI(Vec2I(0, end[1]), end)};
    for (size_t index = 0; index < windows.size(); ++index) {
      auto const& window = windows[index];
      auto earlier = std::span(windows).first(index);
      if (std::ranges::find(earlier, window) != earlier.end())
        continue;
      ASSERT_TRUE(scaled_crop_fits(source, scale, window));
      auto expected = full->sub_image(Vec2U(window.min()), Vec2U(window.size()));
      auto actual =
        nearest ? scale_nearest_cropped(source, scale, window) : scale_bilinear_cropped(source, scale, window);
      ASSERT_TRUE(expected);
      ASSERT_TRUE(actual);
      expectSameImage(*actual, *expected);
    }
  }
}
}// namespace

TEST(ImageScalingTest, NearestMatchesOriginalScalarAcrossByteFormatsAndSizes) {
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    for (auto size : {Vec2U(1, 1), Vec2U(1, 7), Vec2U(7, 1), Vec2U(2, 3), Vec2U(7, 5), Vec2U(31, 17)}) {
      Image source = patternedImage(size, format);
      for (auto scale : {Vec2F(1, 1), Vec2F(2, 2), Vec2F(0.5f, 0.75f), Vec2F(1.5f, 2.5f),
                         Vec2F(0.15384616f, 0.15384616f), Vec2F(2.0f / 3.0f, 1.125f), Vec2F(0.01f, 3)}) {
        auto expected = scalarNearest(source, scale);
        auto actual = scale_nearest(source, scale);
        ASSERT_TRUE(expected);
        ASSERT_TRUE(actual);
        expectSameImage(*actual, *expected);
      }
    }
  }
}

TEST(ImageScalingTest, NearestRoundsHalfUpAndClampsLastPixel) {
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    Image source = patternedImage(Vec2U(3, 3), format);
    auto actual = scale_nearest(source, Vec2F(2, 2));
    ASSERT_TRUE(actual);
    ASSERT_EQ(actual->size(), Vec2U(6, 6));
    constexpr std::array<unsigned, 6> indices{0, 1, 1, 2, 2, 2};
    for (unsigned y = 0; y < 6; ++y) {
      for (unsigned x = 0; x < 6; ++x) {
        auto pixel = actual->get_rgb(x, y);
        auto expected = source.get_rgb(indices[x], indices[y]);
        ASSERT_TRUE(pixel);
        ASSERT_TRUE(expected);
        EXPECT_EQ(*pixel, *expected) << "pixel " << x << "," << y;
      }
    }
    auto first = actual->get_rgb(0, 0);
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, Vec4B(17, 31, 213, source.bytes_per_pixel() == 4 ? 0 : 255));
  }
}

TEST(ImageScalingTest, NearestDivisionBoundarySurvivesDirectivesConsumer) {
  // Strict binary32 division selects 6 at position 1 (just below 6.5), and 20
  // at position 3 (exactly 19.5). Reciprocal refinement can change either.
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    Image source = patternedImage(Vec2U(27, 27), format);
    auto expected = scalarNearest(source, Vec2F(0.15384616f, 0.15384616f));
    ASSERT_TRUE(expected);
    auto direct = scale_nearest(source, Vec2F(0.15384616f, 0.15384616f));
    ASSERT_TRUE(direct);
    expectSameImage(*direct, *expected);
    ASSERT_EQ(direct->size(), Vec2U(4, 4));
    constexpr std::array<unsigned, 4> indices{0, 6, 13, 20};
    for (unsigned y = 0; y < indices.size(); ++y) {
      for (unsigned x = 0; x < indices.size(); ++x) {
        auto selected = source.get_rgb(indices[x], indices[y]);
        auto actual = direct->get_rgb(x, y);
        ASSERT_TRUE(selected);
        ASSERT_TRUE(actual);
        EXPECT_EQ(*actual, *selected) << "pixel " << x << "," << y;
      }
    }

    DirectivesGroup directives(String("?scalenearest=0.15384616;0.15384616"));
    auto processed = directives.apply_new_image(source);
    ASSERT_TRUE(processed);
    // Directive processing promotes 24-bit input before executing operations.
    Image promoted = source.convert(format == PixelFormat::RGB24     ? PixelFormat::RGBA32
                                      : format == PixelFormat::BGR24 ? PixelFormat::BGRA32
                                                                     : format);
    auto expectedProcessed = scalarNearest(promoted, Vec2F(0.15384616f, 0.15384616f));
    ASSERT_TRUE(expectedProcessed);
    expectSameImage(*processed, *expectedProcessed);
  }
}

TEST(ImageScalingTest, NearestEmptyAxesProduceZeroedMinimumDimensions) {
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32,
                      PixelFormat::RGB_F, PixelFormat::RGBA_F}) {
    for (auto size : {Vec2U(0, 0), Vec2U(0, 3), Vec2U(4, 0)}) {
      Image source(size, format);
      auto actual = scale_nearest(source, Vec2F(2, 1.5f));
      ASSERT_TRUE(actual);
      EXPECT_EQ(actual->size(), Vec2U(size[0] == 0 ? 1 : 8, size[1] == 0 ? 1 : 5));
      EXPECT_EQ(actual->pixel_format(), format);
      EXPECT_TRUE(std::all_of(actual->data().begin(), actual->data().end(), [](auto byte) { return byte == 0; }));
      if (actual->bytes_per_pixel() <= 4) {
        auto pixel = actual->get(0, 0);
        ASSERT_TRUE(pixel);
        EXPECT_EQ(*pixel, Vec4B(0, 0, 0, actual->bytes_per_pixel() == 3 ? 255 : 0));
      }
    }
  }
  auto empty = scale_nearest(Image(), Vec2F(0, 0));
  ASSERT_TRUE(empty);
  EXPECT_EQ(empty->size(), Vec2U(1, 1));
  EXPECT_EQ(empty->get32(0, 0), Vec4B(0, 0, 0, 0));
}

TEST(ImageScalingTest, NearestZeroScaleKeepsFirstSourceCoordinate) {
  constexpr std::array<unsigned, 6> columns{0, 1, 1, 2, 2, 2};
  constexpr std::array<unsigned, 8> rows{0, 1, 1, 2, 2, 3, 3, 3};
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    Image source = patternedImage(Vec2U(3, 4), format);
    for (auto scale : {Vec2F(0, 2), Vec2F(2, 0), Vec2F(0, 0)}) {
      auto actual = scale_nearest(source, scale);
      ASSERT_TRUE(actual);
      ASSERT_EQ(actual->size(), Vec2U(scale[0] == 0 ? 1 : 6, scale[1] == 0 ? 1 : 8));
      EXPECT_EQ(actual->pixel_format(), format);
      for (unsigned y = 0; y < actual->height(); ++y) {
        for (unsigned x = 0; x < actual->width(); ++x) {
          auto pixel = actual->get(x, y);
          auto expected = source.get(scale[0] == 0 ? 0 : columns[x], scale[1] == 0 ? 0 : rows[y]);
          ASSERT_TRUE(pixel);
          ASSERT_TRUE(expected);
          EXPECT_EQ(*pixel, *expected);
        }
      }
    }
  }
}

TEST(ImageScalingTest, NearestFloatFormatsRetainLegacyByteSetterBehavior) {
  for (auto format : {PixelFormat::RGB_F, PixelFormat::RGBA_F}) {
    Image source(3, 2, format);
    constexpr std::array<float, 4> pixel{0.125f, 0.25f, 0.75f, 0.5f};
    for (std::size_t offset = 0; offset < source.data().size(); offset += source.bytes_per_pixel())
      std::memcpy(source.data().data() + offset, pixel.data(), source.bytes_per_pixel());
    for (auto scale : {Vec2F(1, 1), Vec2F(2, 1.5f), Vec2F(0.5f, 0.75f)}) {
      auto expected = scalarNearest(source, scale);
      auto actual = scale_nearest(source, scale);
      ASSERT_TRUE(expected);
      ASSERT_TRUE(actual);
      expectSameImage(*actual, *expected);
      // Image::set(Vec4B) currently leaves float storage zeroed; copying the
      // raw float pixels here would silently introduce different semantics.
      EXPECT_TRUE(std::all_of(actual->data().begin(), actual->data().end(), [](auto byte) { return byte == 0; }));
    }
  }
}

TEST(ImageScalingTest, CroppedScalesMatchFullScaleThenSubImageAcrossByteFormats) {
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    for (auto size : {Vec2U(1, 1), Vec2U(1, 7), Vec2U(7, 1), Vec2U(2, 3), Vec2U(27, 27)}) {
      auto source = patternedImage(size, format);
      for (auto scale : {Vec2F(1, 1), Vec2F(2, 2), Vec2F(0.5f, 0.75f), Vec2F(1.5f, 2.5f),
                         Vec2F(0.15384616f, 0.15384616f), Vec2F(2.0f / 3.0f, 1.125f), Vec2F(0.01f, 3)})
        expectCroppedMatchesFull(source, scale);
    }
  }
}

TEST(ImageScalingTest, CroppedNearestRetainsAbsoluteDivisionHalfBoundary) {
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    auto source = patternedImage(Vec2U(27, 27), format);
    auto scale = Vec2F(0.15384616f, 0.15384616f);
    auto full = scale_nearest(source, scale);
    ASSERT_TRUE(full);
    RectI window(Vec2I(2, 2), Vec2I(4, 4));
    auto cropped = scale_nearest_cropped(source, scale, window);
    auto expected = full->sub_image(Vec2U(2, 2), Vec2U(2, 2));
    ASSERT_TRUE(cropped);
    ASSERT_TRUE(expected);
    expectSameImage(*cropped, *expected);
    auto selected = source.get(20, 20);
    auto pixel = cropped->get(1, 1);
    ASSERT_TRUE(selected);
    ASSERT_TRUE(pixel);
    EXPECT_EQ(*pixel, *selected);
  }
}

TEST(ImageScalingTest, CroppedScalesRetainEmptyZeroAndFloatBehavior) {
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32,
                      PixelFormat::RGB_F, PixelFormat::RGBA_F}) {
    for (auto size : {Vec2U(0, 0), Vec2U(0, 3), Vec2U(4, 0), Vec2U(3, 2)}) {
      Image source(size, format);
      if (source.bytes_per_pixel() > 4) {
        constexpr std::array<float, 4> pixel{0.125f, 0.25f, 0.75f, 0.5f};
        for (std::size_t offset = 0; offset < source.data().size(); offset += source.bytes_per_pixel())
          std::memcpy(source.data().data() + offset, pixel.data(), source.bytes_per_pixel());
      } else {
        source = patternedImage(size, format);
      }
      for (auto scale : {Vec2F(1, 1), Vec2F(2, 1.5f), Vec2F(0.5f, 0.75f), Vec2F(0, 2), Vec2F(2, 0), Vec2F(0, 0)})
        expectCroppedMatchesFull(source, scale);
    }
  }
}

TEST(ImageScalingTest, CroppedBilinearPreservesUnpremultipliedAlphaAndHiddenRgb) {
  for (auto format : {PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    Image source(2, 1, format);
    ASSERT_TRUE(source.set_rgb(0, 0, Vec4B(255, 0, 0, 0)));
    ASSERT_TRUE(source.set_rgb(1, 0, Vec4B(0, 0, 255, 255)));
    auto full = scale_bilinear(source, Vec2F(2, 1));
    auto cropped = scale_bilinear_cropped(source, Vec2F(2, 1), RectI(Vec2I(), Vec2I(2, 1)));
    ASSERT_TRUE(full);
    ASSERT_TRUE(cropped);
    auto expected = full->sub_image(Vec2U(), Vec2U(2, 1));
    ASSERT_TRUE(expected);
    expectSameImage(*cropped, *expected);
    auto hidden = cropped->get_rgb(0, 0);
    auto interpolated = cropped->get_rgb(1, 0);
    ASSERT_TRUE(hidden);
    ASSERT_TRUE(interpolated);
    EXPECT_EQ(*hidden, Vec4B(255, 0, 0, 0));
    EXPECT_EQ(*interpolated, Vec4B(127, 0, 127, 127));
  }
}

TEST(ImageScalingTest, CroppedScaleComputesSmallWindowOfLargeOutput) {
  auto source = patternedImage(Vec2U(2, 2), PixelFormat::RGBA32);
  auto scale = Vec2F(256, 256);
  RectI window(Vec2I(101, 222), Vec2I(225, 227));
  for (bool nearest : {true, false}) {
    auto full = nearest ? scale_nearest(source, scale) : scale_bilinear(source, scale);
    auto cropped =
      nearest ? scale_nearest_cropped(source, scale, window) : scale_bilinear_cropped(source, scale, window);
    ASSERT_TRUE(full);
    ASSERT_TRUE(cropped);
    auto expected = full->sub_image(Vec2U(window.min()), Vec2U(window.size()));
    ASSERT_TRUE(expected);
    expectSameImage(*cropped, *expected);
  }

  // The hypothetical full image here would occupy 160 petabytes. The cropped
  // APIs must allocate only the requested pixels (and nearest's window columns).
  source = Image::filled(Vec2U(2, 2), Vec4B(73, 129, 211, 0));
  scale = Vec2F(100000000, 100000000);
  window = RectI(Vec2I(100000000, 100000000), Vec2I(100000002, 100000003));
  ASSERT_TRUE(scaled_crop_fits(source, scale, window));
  for (bool nearest : {true, false}) {
    auto cropped =
      nearest ? scale_nearest_cropped(source, scale, window) : scale_bilinear_cropped(source, scale, window);
    ASSERT_TRUE(cropped);
    expectSameImage(*cropped, Image::filled(Vec2U(2, 3), Vec4B(73, 129, 211, 0)));
  }
}

TEST(ImageScalingTest, ScaledCropFitsRejectsInvalidBoundsWithoutOverflow) {
  Image source(2, 3);
  for (auto window :
       {RectI(Vec2I(-1, 0), Vec2I(1, 1)), RectI(Vec2I(0, -1), Vec2I(1, 1)), RectI(Vec2I(2, 2), Vec2I(1, 3)),
        RectI(Vec2I(), Vec2I(5, 6)), RectI(Vec2I(), Vec2I(4, 7)), RectI(Vec2I(5, 0), Vec2I(5, 0)),
        RectI(Vec2I(std::numeric_limits<int>::min(), 0), Vec2I(std::numeric_limits<int>::max(), 1)),
        RectI(Vec2I(std::numeric_limits<int>::max() - 1, 0), Vec2I(std::numeric_limits<int>::max(), 1))}) {
    EXPECT_FALSE(scaled_crop_fits(source, Vec2F(2, 2), window));
    EXPECT_FALSE(scale_nearest_cropped(source, Vec2F(2, 2), window));
    EXPECT_FALSE(scale_bilinear_cropped(source, Vec2F(2, 2), window));
  }
  EXPECT_TRUE(scaled_crop_fits(source, Vec2F(2, 2), RectI(Vec2I(4, 6), Vec2I(4, 6))));
  EXPECT_TRUE(scaled_crop_fits(source, Vec2F(0, 0), RectI(Vec2I(), Vec2I(1, 1))));
  // No signed narrowing of a valid scaled size above INT_MAX.
  EXPECT_TRUE(
    scaled_crop_fits(source, Vec2F(1500000000, 1),
                     RectI(Vec2I(std::numeric_limits<int>::max() - 1, 0), Vec2I(std::numeric_limits<int>::max(), 1))));
  for (auto scale : {Vec2F(-1, 1), Vec2F(1, -1), Vec2F(std::bit_cast<float>(0x7f800000u), 1),
                     Vec2F(1, std::bit_cast<float>(0x7fc00000u)), Vec2F(2147483648.0f, 1),
                     Vec2F(std::numeric_limits<float>::max(), 1)}) {
    EXPECT_FALSE(scaled_crop_fits(source, scale, RectI(Vec2I(), Vec2I(1, 1))));
    EXPECT_FALSE(scale_nearest_cropped(source, scale, RectI(Vec2I(), Vec2I(1, 1))));
    EXPECT_FALSE(scale_bilinear_cropped(source, scale, RectI(Vec2I(), Vec2I(1, 1))));
  }
}
