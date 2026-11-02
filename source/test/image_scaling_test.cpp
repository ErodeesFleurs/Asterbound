import std;
import star.image_scaling;
import star.image;
import star.directives;
import star.vector;
import star.string;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

namespace {
// The original per-pixel implementation, including its byte-oriented get/set
// behavior. Volatile divisors keep this oracle scalar under fast-math too.
std::expected<Image, StarException> scalarNearest(Image const& source, Vec2F const& scale) {
  Vec2U size = Vec2U::round(vmult(Vec2F(source.size()), scale));
  size[0] = std::max(size[0], 1u);
  size[1] = std::max(size[1], 1u);
  Image result(size, source.pixelFormat());
  volatile float scaleX = scale[0], scaleY = scale[1];
  for (unsigned y = 0; y < size[1]; ++y) {
    for (unsigned x = 0; x < size[0]; ++x) {
      auto pixel =
        source.clamp(Vec2I::round(vdiv(Vec2F(x, y), Vec2F(static_cast<float>(scaleX), static_cast<float>(scaleY)))));
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
      EXPECT_TRUE(image.setrgb(x, y,
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
  ASSERT_EQ(actual.pixelFormat(), expected.pixelFormat());
  ASSERT_EQ(actual.data().size(), expected.data().size());
  EXPECT_TRUE(std::equal(actual.data().begin(), actual.data().end(), expected.data().begin()));
}
}// namespace

TEST(ImageScalingTest, NearestMatchesOriginalScalarAcrossByteFormatsAndSizes) {
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    for (auto size : {Vec2U(1, 1), Vec2U(1, 7), Vec2U(7, 1), Vec2U(2, 3), Vec2U(7, 5), Vec2U(31, 17)}) {
      Image source = patternedImage(size, format);
      for (auto scale : {Vec2F(1, 1), Vec2F(2, 2), Vec2F(0.5f, 0.75f), Vec2F(1.5f, 2.5f),
                         Vec2F(0.15384616f, 0.15384616f), Vec2F(2.0f / 3.0f, 1.125f), Vec2F(0.01f, 3)}) {
        auto expected = scalarNearest(source, scale);
        auto actual = scaleNearest(source, scale);
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
    auto actual = scaleNearest(source, Vec2F(2, 2));
    ASSERT_TRUE(actual);
    ASSERT_EQ(actual->size(), Vec2U(6, 6));
    constexpr std::array<unsigned, 6> indices{0, 1, 1, 2, 2, 2};
    for (unsigned y = 0; y < 6; ++y) {
      for (unsigned x = 0; x < 6; ++x) {
        auto pixel = actual->getrgb(x, y);
        auto expected = source.getrgb(indices[x], indices[y]);
        ASSERT_TRUE(pixel);
        ASSERT_TRUE(expected);
        EXPECT_EQ(*pixel, *expected) << "pixel " << x << "," << y;
      }
    }
    auto first = actual->getrgb(0, 0);
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, Vec4B(17, 31, 213, source.bytesPerPixel() == 4 ? 0 : 255));
  }
}

TEST(ImageScalingTest, NearestDivisionBoundarySurvivesDirectivesConsumer) {
  // 3 / 0.15384616f rounds to exactly 19.5f, hence source index 20.
  // Multiplication by its rounded reciprocal instead selects index 19.
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    Image source = patternedImage(Vec2U(27, 27), format);
    auto expected = scalarNearest(source, Vec2F(0.15384616f, 0.15384616f));
    ASSERT_TRUE(expected);
    auto direct = scaleNearest(source, Vec2F(0.15384616f, 0.15384616f));
    ASSERT_TRUE(direct);
    expectSameImage(*direct, *expected);
    ASSERT_EQ(direct->size(), Vec2U(4, 4));
    auto selected = source.getrgb(20, 20);
    auto actual = direct->getrgb(3, 3);
    ASSERT_TRUE(selected);
    ASSERT_TRUE(actual);
    EXPECT_EQ(*actual, *selected);

    DirectivesGroup directives(String("?scalenearest=0.15384616;0.15384616"));
    auto processed = directives.applyNewImage(source);
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
      auto actual = scaleNearest(source, Vec2F(2, 1.5f));
      ASSERT_TRUE(actual);
      EXPECT_EQ(actual->size(), Vec2U(size[0] == 0 ? 1 : 8, size[1] == 0 ? 1 : 5));
      EXPECT_EQ(actual->pixelFormat(), format);
      EXPECT_TRUE(std::all_of(actual->data().begin(), actual->data().end(), [](auto byte) { return byte == 0; }));
      if (actual->bytesPerPixel() <= 4) {
        auto pixel = actual->get(0, 0);
        ASSERT_TRUE(pixel);
        EXPECT_EQ(*pixel, Vec4B(0, 0, 0, actual->bytesPerPixel() == 3 ? 255 : 0));
      }
    }
  }
  auto empty = scaleNearest(Image(), Vec2F(0, 0));
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
      auto actual = scaleNearest(source, scale);
      ASSERT_TRUE(actual);
      ASSERT_EQ(actual->size(), Vec2U(scale[0] == 0 ? 1 : 6, scale[1] == 0 ? 1 : 8));
      EXPECT_EQ(actual->pixelFormat(), format);
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
    for (std::size_t offset = 0; offset < source.data().size(); offset += source.bytesPerPixel())
      std::memcpy(source.data().data() + offset, pixel.data(), source.bytesPerPixel());
    for (auto scale : {Vec2F(1, 1), Vec2F(2, 1.5f), Vec2F(0.5f, 0.75f)}) {
      auto expected = scalarNearest(source, scale);
      auto actual = scaleNearest(source, scale);
      ASSERT_TRUE(expected);
      ASSERT_TRUE(actual);
      expectSameImage(*actual, *expected);
      // Image::set(Vec4B) currently leaves float storage zeroed; copying the
      // raw float pixels here would silently introduce different semantics.
      EXPECT_TRUE(std::all_of(actual->data().begin(), actual->data().end(), [](auto byte) { return byte == 0; }));
    }
  }
}
