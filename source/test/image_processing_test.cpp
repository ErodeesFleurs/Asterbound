import std;
import star.image_processing;
import star.image;
import star.color;
import star.vector;
import star.string;
import star.string_view;
import star.list;
import star.map;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

namespace {
Vec4B scalarHsv(Vec4B pixel, ImageOperation const& operation) {
  if (pixel[3] == 0)
    return pixel;
  if (auto hue = std::get_if<HueShiftImageOperation>(&operation))
    return Color::hueShiftVec4B(pixel, hue->hueShiftAmount);
  Color color = Color::rgba(pixel);
  if (auto saturation = std::get_if<SaturationShiftImageOperation>(&operation))
    color.setSaturation(std::clamp(color.saturation() + saturation->saturationShiftAmount, 0.0f, 1.0f));
  else
    color.setValue(
      std::clamp(color.value() * std::get<BrightnessMultiplyImageOperation>(operation).brightnessMultiply, 0.0f, 1.0f));
  return color.toRgba();
}

void expectPixels(Image const& actual, Image const& expected) {
  ASSERT_EQ(actual.size(), expected.size());
  ASSERT_EQ(actual.pixelFormat(), expected.pixelFormat());
  for (unsigned y = 0; y < actual.height(); ++y) {
    for (unsigned x = 0; x < actual.width(); ++x) {
      auto a = actual.get(x, y);
      auto b = expected.get(x, y);
      ASSERT_TRUE(a);
      ASSERT_TRUE(b);
      EXPECT_EQ(*a, *b) << "pixel " << x << "," << y;
    }
  }
}
}// namespace

TEST(ImageProcessingTest, HsvRepeatedColorsCollisionsAndHiddenRgb) {
  // More than 256 distinct opaque colors force collisions in the recent cache.
  // Replay them in reverse and in runs, including equal RGB with different alpha.
  for (auto format : {PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    Image original(1208, 1, format);
    for (unsigned x = 0; x < original.width(); ++x) {
      unsigned n = x < 604 ? x / 2 : (1207 - x) / 2;
      Vec4B pixel(n % 256, 37 + n / 256, 173, n % 3 == 0 ? 61 : 255);
      if (n == 0)
        pixel = Vec4B(91, 47, 213, 0);
      ASSERT_TRUE(original.set(x, 0, pixel));
    }
    for (ImageOperation operation :
         {ImageOperation(HueShiftImageOperation{0.173f}), ImageOperation(HueShiftImageOperation{-1.0f}),
          ImageOperation(SaturationShiftImageOperation{-0.31f}),
          ImageOperation(BrightnessMultiplyImageOperation{1.37f})}) {
      Image expected = original;
      expected.forEachPixel([&](unsigned, unsigned, Vec4B& pixel) { pixel = scalarHsv(pixel, operation); });
      Image actual = original;
      ASSERT_TRUE(processImageOperation(operation, actual));
      expectPixels(actual, expected);
      EXPECT_EQ(actual.get32(0, 0), Vec4B(91, 47, 213, 0));
    }
  }
}

TEST(ImageProcessingTest, ExactHsvIdentitiesKeepBytesAfterFormatConversion) {
  // These fixed outputs deliberately specify the new identity semantics, not
  // an assertion that the former floating-point HSV round trip was lossless.
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    for (auto directive : {"hueshift=0", "hueshift=-0", "saturation=0", "brightness=0"}) {
      Image image(3, 1, format);
      ASSERT_TRUE(image.set(0, 0, Vec4B(17, 91, 203, 127)));
      ASSERT_TRUE(image.set(1, 0, Vec4B(91, 47, 213, 0)));
      ASSERT_TRUE(image.set(2, 0, Vec4B(255, 1, 127, 255)));
      ASSERT_TRUE(processImageOperation(imageOperationFromString(StringView(std::string_view(directive))), image));
      bool was24 = format == PixelFormat::RGB24 || format == PixelFormat::BGR24;
      EXPECT_EQ(image.pixelFormat(),
                format == PixelFormat::BGR24     ? PixelFormat::BGRA32
                  : format == PixelFormat::RGB24 ? PixelFormat::RGBA32
                                                 : format);
      EXPECT_EQ(image.get32(0, 0), Vec4B(17, 91, 203, was24 ? 255 : 127));
      EXPECT_EQ(image.get32(1, 0), Vec4B(91, 47, 213, was24 ? 255 : 0));
      EXPECT_EQ(image.get32(2, 0), Vec4B(255, 1, 127, 255));
    }
  }
}

TEST(ImageProcessingTest, ReplaceThresholdRunsTransparentKeysAndSingleStepChains) {
  Vec4B a(19, 37, 83, 255), b(71, 43, 5, 255), c(11, 113, 79, 127);
  Vec4B transparent(91, 47, 213, 0), hidden(1, 2, 3, 0);
  for (unsigned extraKeys : {0u, 200u}) {
    ColorReplaceImageOperation operation;
    operation.colorReplaceMap[a] = b;
    operation.colorReplaceMap[b] = c;
    operation.colorReplaceMap[transparent] = hidden;
    operation.colorReplaceMap[Vec4B(0, 0, 0, 0)] = hidden;
    for (unsigned i = 0; i < extraKeys; ++i)
      operation.colorReplaceMap[Vec4B(i, 231, 17, 255)] = Vec4B(3, i, 71, 61);
    for (unsigned width : {0u, 1023u, 1024u, 2049u}) {
      for (auto format : {PixelFormat::RGBA32, PixelFormat::BGRA32, PixelFormat::RGB24, PixelFormat::BGR24}) {
        Image image(width, 1, format);
        std::array palette{Vec4B(0, 0, 0, 0), a, b, transparent, Vec4B(29, 31, 97, 255), c, Vec4B(17, 231, 17, 255)};
        for (unsigned x = 0; x < width; ++x)
          ASSERT_TRUE(image.set(x, 0, palette[(x / 7) % palette.size()]));
        Image expected = image.convert(format == PixelFormat::BGR24     ? PixelFormat::BGRA32
                                         : format == PixelFormat::RGB24 ? PixelFormat::RGBA32
                                                                        : format);
        expected.forEachPixel([&](unsigned, unsigned, Vec4B& pixel) {
          if (auto replacement = operation.colorReplaceMap.maybe(pixel))
            pixel = *replacement;
        });
        ASSERT_TRUE(processImageOperation(operation, image));
        expectPixels(image, expected);
      }
    }
  }
  Image image = Image::filled(Vec2U(1024, 1), transparent);
  Image expected = image;
  ASSERT_TRUE(processImageOperation(ColorReplaceImageOperation{}, image));
  expectPixels(image, expected);
}

TEST(ImageProcessingTest, BlendMatchesOrderedFourChannelScalarSemantics) {
  Image first = Image::filled(Vec2U(2, 3), Vec4B(61, 137, 203, 79), PixelFormat::BGRA32);
  Image second = Image::filled(Vec2U(4, 1), Vec4B(173, 29, 89, 151), PixelFormat::RGB24);
  Image empty;
  std::array<Image const*, 3> references{&first, &second, &empty};
  for (auto mode : {BlendImageOperation::Multiply, BlendImageOperation::Screen}) {
    for (auto offset : {Vec2I(0, 0), Vec2I(-1, -2), Vec2I(1, 1), Vec2I(9, -9),
                        Vec2I(std::numeric_limits<int>::min(), 0), Vec2I(0, std::numeric_limits<int>::min()),
                        Vec2I(std::numeric_limits<int>::max(), std::numeric_limits<int>::max())}) {
      for (auto format : {PixelFormat::RGBA32, PixelFormat::BGRA32, PixelFormat::RGB24, PixelFormat::BGR24}) {
        Image actual = Image::filled(Vec2U(5, 4), Vec4B(97, 181, 43, 113), format);
        ASSERT_TRUE(actual.set(1, 1, Vec4B(91, 47, 213, 0)));
        Image expected = actual.convert(format == PixelFormat::BGR24     ? PixelFormat::BGRA32
                                          : format == PixelFormat::RGB24 ? PixelFormat::RGBA32
                                                                         : format);
        // Full-image scalar oracle: no bounding-box or cache implementation.
        for (unsigned y = 0; y < expected.height(); ++y) {
          for (unsigned x = 0; x < expected.width(); ++x) {
            Vec4F result = Color::v4bToFloat(expected.get32(x, y));
            for (auto reference : references) {
              std::int64_t px = std::int64_t(x) + offset[0], py = std::int64_t(y) + offset[1];
              if (px < 0 || py < 0 || px >= reference->width() || py >= reference->height())
                continue;
              auto pixel = reference->get(Vec2U(px, py));
              ASSERT_TRUE(pixel);
              Vec4F other = Color::v4bToFloat(*pixel);
              for (unsigned channel = 0; channel < 4; ++channel)
                result[channel] = mode == BlendImageOperation::Multiply
                  ? result[channel] * other[channel]
                  : 1.0f - (1.0f - result[channel]) * (1.0f - other[channel]);
            }
            expected.set32(x, y, Color::v4fToByte(result));
          }
        }
        unsigned calls = 0;
        BlendImageOperation operation{mode, {"first", "second", "empty"}, offset};
        ASSERT_TRUE(processImageOperation(
          operation, actual,
          [&](String const& name) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
            EXPECT_EQ(name.utf8(), std::string_view(calls == 0 ? "first" : calls == 1 ? "second" : "empty"));
            return std::cref(*references[calls++]);
          }));
        EXPECT_EQ(calls, 3u);
        expectPixels(actual, expected);
      }
    }
  }
}

TEST(ImageProcessingTest, BlendErrorsPrecedeOverlapAndRetainReferenceOrder) {
  Image image = Image::filled(Vec2U(1, 1), Vec4B(7, 19, 31, 0), PixelFormat::BGR24);
  BlendImageOperation operation{BlendImageOperation::Multiply,
                                {"missing", "later"},
                                Vec2I(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())};
  auto absentCallback = processImageOperation(operation, image);
  ASSERT_FALSE(absentCallback);
  EXPECT_NE(std::string_view(absentCallback.error().what()).find("Missing image ref callback"), std::string_view::npos);
  EXPECT_EQ(image.pixelFormat(), PixelFormat::BGRA32);
  unsigned calls = 0;
  auto missing = processImageOperation(
    operation, image,
    [&](String const& name) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
      ++calls;
      EXPECT_EQ(name, "missing");
      return std::nullopt;
    });
  ASSERT_FALSE(missing);
  EXPECT_EQ(calls, 1u);
  EXPECT_NE(std::string_view(missing.error().what()).find("Missing blend image 'missing'"), std::string_view::npos);
  calls = 0;
  auto failed = processImageOperation(
    operation, image,
    [&](String const&) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
      ++calls;
      return std::unexpected(ImageOperationException("reference failure"));
    });
  ASSERT_FALSE(failed);
  EXPECT_EQ(calls, 1u);
  EXPECT_NE(std::string_view(failed.error().what()).find("reference failure"), std::string_view::npos);
  calls = 0;
  Image unchanged = image;
  auto laterFailure = processImageOperation(
    operation, image,
    [&](String const& name) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
      ++calls;
      if (name == "missing")
        return std::cref(unchanged);
      EXPECT_EQ(name, "later");
      return std::unexpected(ImageOperationException("later reference failure"));
    });
  ASSERT_FALSE(laterFailure);
  EXPECT_EQ(calls, 2u);
  EXPECT_NE(std::string_view(laterFailure.error().what()).find("later reference failure"), std::string_view::npos);
  expectPixels(image, unchanged);
  operation.blendImages.clear();
  Image before = image;
  ASSERT_TRUE(processImageOperation(operation, image));
  expectPixels(image, before);
  Image invalid(1, 1, PixelFormat::RGB24);
  auto malformed = processImageOperation(ErrorImageOperation{ImageOperationException("invalid directive")}, invalid);
  ASSERT_FALSE(malformed);
  EXPECT_EQ(invalid.pixelFormat(), PixelFormat::RGB24);
}
