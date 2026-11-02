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
import star.rect;

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
  EXPECT_EQ(actual.size(), expected.size());
  EXPECT_EQ(actual.pixelFormat(), expected.pixelFormat());
  if (actual.size() != expected.size() || actual.pixelFormat() != expected.pixelFormat())
    return;
  if (std::ranges::equal(actual.data(), expected.data()))
    return;
  ADD_FAILURE() << "image bytes differ";
  auto bytesPerPixel = actual.bytesPerPixel();
  auto rowBytes = actual.width() * bytesPerPixel;
  for (size_t offset = 0; offset < actual.data().size(); ++offset) {
    EXPECT_EQ(actual.data()[offset], expected.data()[offset])
      << "pixel " << offset % rowBytes / bytesPerPixel << "," << offset / rowBytes << " channel byte "
      << offset % bytesPerPixel;
  }
}

std::expected<void, StarException> sequentialOperations(List<ImageOperation> const& operations, Image& image,
                                                        ImageReferenceCallback callback = {}) {
  for (auto const& operation : operations) {
    auto processed = processImageOperation(operation, image, callback);
    if (!processed)
      return std::unexpected(std::move(processed.error()));
  }
  return {};
}

void expectOptimizedOperations(List<ImageOperation> const& operations, Image const& original,
                               ImageReferenceCallback callback = {}) {
  Image expected = original;
  auto sequential = sequentialOperations(operations, expected, callback);
  std::vector<ImageOperation const*> pointers;
  for (auto const& operation : operations)
    pointers.push_back(&operation);
  Image actual = original;
  auto optimized = processImageOperations(std::span<ImageOperation const* const>(pointers), actual, callback);
  EXPECT_EQ(bool(optimized), bool(sequential));
  if (bool(optimized) != bool(sequential))
    return;
  if (!sequential)
    EXPECT_EQ(std::string(optimized.error().what()), std::string(sequential.error().what()));
  expectPixels(actual, expected);
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

TEST(ImageProcessingTest, OptimizedDeterministicColorScaleCropChainsMatchSequential) {
  std::uint32_t state = 0x57a12b9u;
  auto next = [&] {
    state = state * 1664525u + 1013904223u;
    return state;
  };
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    for (unsigned trial = 0; trial < 24; ++trial) {
      SCOPED_TRACE(trial);
      Image original(7, 5, format);
      for (unsigned y = 0; y < original.height(); ++y) {
        for (unsigned x = 0; x < original.width(); ++x) {
          auto value = next();
          ASSERT_TRUE(original.set(x, y, Vec4B(value, value >> 8, value >> 16, x % 3 ? value >> 24 : 0)));
        }
      }
      ColorReplaceImageOperation replace;
      replace.colorReplaceMap[Vec4B(17, 31, 71, 0)] = Vec4B(61, 19, 13, 0);
      replace.colorReplaceMap[Vec4B(61, 19, 13, 0)] = Vec4B(7, 11, 23, 127);
      List<ImageOperation> colors{HueShiftImageOperation{0.173f},
                                  SaturationShiftImageOperation{-0.31f},
                                  BrightnessMultiplyImageOperation{1.37f},
                                  FadeToColorImageOperation(Vec3B(71, 139, 213), 0.23f),
                                  SetColorImageOperation{Vec3B(17, 31, 71)},
                                  replace,
                                  MultiplyImageOperation{Vec4B(179, 93, 233, 113)}};
      List<ImageOperation> operations;
      for (unsigned k = 0; k < colors.size(); ++k)
        operations.append(colors[(k + trial) % colors.size()]);
      operations.append(CropImageOperation{RectI(1, 1, 7, 5)});
      operations.append(ScaleImageOperation{ScaleImageOperation::Nearest, Vec2F(2.7f, 2.0f)});
      for (unsigned k = 0; k < colors.size(); ++k)
        operations.append(colors[(k + trial + 3) % colors.size()]);
      operations.append(CropImageOperation{RectI(1, 1, 12, 6)});
      operations.append(ScaleImageOperation{ScaleImageOperation::Bilinear, Vec2F(0.75f, 1.25f)});
      operations.append(CropImageOperation{RectI(1, 1, 6, 4)});
      operations.append(ScanLinesImageOperation{FadeToColorImageOperation(Vec3B(13, 71, 131), 0.3f),
                                                FadeToColorImageOperation(Vec3B(191, 37, 19), 0.6f)});
      operations.append(FlipImageOperation{FlipImageOperation::FlipXY});
      expectOptimizedOperations(operations, original);
    }
  }
}

TEST(ImageProcessingTest, OptimizedEmptyAndZeroScaleSchedulingKeepsSynthesizedPixels) {
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    for (auto size : {Vec2U(0, 0), Vec2U(0, 3), Vec2U(3, 0), Vec2U(2, 3)}) {
      for (auto scale : {Vec2F(3, 2), Vec2F(0, 2), Vec2F(2, 0), Vec2F(0, 0)}) {
        Image original = Image::filled(size, Vec4B(29, 71, 139, 0), format);
        List<ImageOperation> operations{
          ScaleImageOperation{ScaleImageOperation::Nearest, scale}, SetColorImageOperation{Vec3B(17, 91, 203)},
          MultiplyImageOperation{Vec4B(255, 127, 31, 79)}, CropImageOperation{RectI(0, 0, 1, 1)}};
        expectOptimizedOperations(operations, original);
      }
    }
    for (auto crop : {RectI(4, 0, 4, 3), RectI(0, 3, 4, 3), RectI(4, 3, 4, 3)}) {
      Image original = Image::filled(Vec2U(4, 3), Vec4B(91, 47, 213, 0), format);
      expectOptimizedOperations({SetColorImageOperation{Vec3B(17, 31, 71)}, CropImageOperation{crop}}, original);
      expectOptimizedOperations(
        {ScaleImageOperation{ScaleImageOperation::Nearest, Vec2F(1, 1)}, CropImageOperation{crop}}, original);
      expectOptimizedOperations(
        {ScaleImageOperation{ScaleImageOperation::Bilinear, Vec2F(1, 1)}, CropImageOperation{crop}}, original);
    }
  }
}

TEST(ImageProcessingTest, OptimizedInvalidCropsAndErrorsRetainSequentialPrefixMutations) {
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    Image original = Image::filled(Vec2U(3, 2), Vec4B(91, 47, 213, 0), format);
    for (auto crop : {RectI(0, 0, 100, 2), RectI(4, 0, 5, 1)}) {
      expectOptimizedOperations({SetColorImageOperation{Vec3B(17, 31, 71)}, CropImageOperation{crop},
                                 ErrorImageOperation{ImageOperationException("later parse failure")}},
                                original);
      expectOptimizedOperations({ScaleImageOperation{ScaleImageOperation::Nearest, Vec2F(2, 2)},
                                 SetColorImageOperation{Vec3B(17, 31, 71)}, CropImageOperation{crop}},
                                original);
      expectOptimizedOperations(
        {ScaleImageOperation{ScaleImageOperation::Bilinear, Vec2F(2, 2)}, CropImageOperation{crop}}, original);
    }
    expectOptimizedOperations(
      {ErrorImageOperation{ImageOperationException("first parse failure")}, SetColorImageOperation{Vec3B(17, 31, 71)}},
      original);
    expectOptimizedOperations({ScaleImageOperation{ScaleImageOperation::Nearest, Vec2F(2, 2)},
                               SetColorImageOperation{Vec3B(17, 31, 71)}, CropImageOperation{RectI(1, 1, 4, 3)},
                               ErrorImageOperation{ImageOperationException("after fused prefix")}},
                              original);
    expectOptimizedOperations(
      {ScaleImageOperation{ScaleImageOperation::Bicubic, Vec2F(2, 2)}, CropImageOperation{RectI(1, 1, 4, 3)}},
      original);
  }
}

TEST(ImageProcessingTest, DirtyReplaceSettledSubsetConsecutiveBlendsAndResetsMatchSequential) {
  Vec4B black(0, 0, 0, 0), red(255, 0, 0, 255), blue(0, 0, 255, 255), green(0, 255, 0, 255);
  ColorReplaceImageOperation settled, subset, expanded, overlap;
  settled.colorReplaceMap[black] = red;
  settled.colorReplaceMap[blue] = red;
  subset.colorReplaceMap[black] = green;
  expanded.colorReplaceMap[black] = green;
  expanded.colorReplaceMap[red] = blue;
  overlap.colorReplaceMap[black] = red;
  overlap.colorReplaceMap[red] = blue;// Not settled: the second replace must still run globally.
  Image zero = Image::filled(Vec2U(3, 2), black);
  Image one = Image::filled(Vec2U(4, 3), Vec4B(255, 255, 255, 255), PixelFormat::BGRA32);
  Image empty;
  auto callback =
    [&](String const& name) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
    if (name == "zero")
      return std::cref(zero);
    if (name == "one")
      return std::cref(one);
    return std::cref(empty);
  };
  BlendImageOperation first{BlendImageOperation::Multiply, {"zero", "one"}, Vec2I(-1, -1)};
  BlendImageOperation second{BlendImageOperation::Multiply, {"zero"}, Vec2I(-15, -10)};
  BlendImageOperation outside{BlendImageOperation::Screen, {"one", "empty"}, Vec2I(1000, -1000)};
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32}) {
    Image original = Image::filled(Vec2U(64, 40), black, format);
    ASSERT_TRUE(original.set(63, 39, blue));
    for (auto next : {settled, subset, expanded, overlap}) {
      List<ImageOperation> operations{settled,
                                      first,
                                      second,
                                      outside,
                                      next,
                                      first,
                                      next,
                                      MultiplyImageOperation{Vec4B(0, 0, 0, 0)},
                                      first,
                                      settled,
                                      SetColorImageOperation{Vec3B(0, 0, 0)},
                                      first,
                                      settled};
      expectOptimizedOperations(operations, original, callback);
    }
    expectOptimizedOperations({overlap, first, overlap, overlap}, original, callback);
    expectOptimizedOperations({settled, outside, subset, first, subset, outside, ColorReplaceImageOperation{}},
                              original, callback);
    // A large dirty rectangle takes the bitmap/run-cache replacement path, too.
    Image wideZero = Image::filled(Vec2U(64, 32), black);
    expectOptimizedOperations(
      {settled, first, subset, first, subset}, original,
      [&](String const&) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
        return std::cref(wideZero);
      });
  }
}

TEST(ImageProcessingTest, OptimizerDoesNotResolveReferencesPastEarlierErrors) {
  ColorReplaceImageOperation replace;
  replace.colorReplaceMap[Vec4B(0, 0, 0, 0)] = Vec4B(17, 31, 71, 0);
  Image reference = Image::filled(Vec2U(2, 2), Vec4B(0, 0, 0, 0));
  std::array<List<ImageOperation>, 5> chains{
    List<ImageOperation>{ErrorImageOperation{ImageOperationException("first parse failure")},
                         BlendImageOperation{BlendImageOperation::Multiply, {"unreachable"}, Vec2I()}},
    List<ImageOperation>{CropImageOperation{RectI(0, 0, 100, 100)},
                         BlendImageOperation{BlendImageOperation::Multiply, {"unreachable"}, Vec2I()}},
    List<ImageOperation>{replace, BlendImageOperation{BlendImageOperation::Multiply, {"good"}, Vec2I()},
                         BlendImageOperation{BlendImageOperation::Screen, {"missing", "unreachable"}, Vec2I()}, replace,
                         ErrorImageOperation{ImageOperationException("later parse failure")}},
    List<ImageOperation>{replace, BlendImageOperation{BlendImageOperation::Multiply, {"good", "failure"}, Vec2I()},
                         replace, BlendImageOperation{BlendImageOperation::Multiply, {"unreachable"}, Vec2I()}},
    List<ImageOperation>{replace, BlendImageOperation{BlendImageOperation::Multiply, {"good"}, Vec2I()},
                         ErrorImageOperation{ImageOperationException("between blends")},
                         BlendImageOperation{BlendImageOperation::Multiply, {"unreachable"}, Vec2I()}}};
  for (auto const& operations : chains) {
    std::vector<String> expectedCalls, actualCalls;
    auto callback = [&](std::vector<String>& calls) -> ImageReferenceCallback {
      return [&calls, &reference](
               String const& name) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
        calls.push_back(name);
        if (name == "good")
          return std::cref(reference);
        if (name == "failure")
          return std::unexpected(ImageOperationException("reference failure"));
        return std::nullopt;
      };
    };
    Image original = Image::filled(Vec2U(4, 3), Vec4B(0, 0, 0, 0), PixelFormat::BGR24);
    Image expected = original, actual = original;
    auto sequential = sequentialOperations(operations, expected, callback(expectedCalls));
    std::vector<ImageOperation const*> pointers;
    for (auto const& operation : operations)
      pointers.push_back(&operation);
    auto optimized =
      processImageOperations(std::span<ImageOperation const* const>(pointers), actual, callback(actualCalls));
    ASSERT_FALSE(sequential);
    ASSERT_FALSE(optimized);
    EXPECT_EQ(std::string(optimized.error().what()), std::string(sequential.error().what()));
    EXPECT_EQ(actualCalls, expectedCalls);
    expectPixels(actual, expected);
  }
}

TEST(ImageProcessingTest, OptimizedMissingCallbacksAndFailedDirtyBlendKeepPromotionAndPrefix) {
  ColorReplaceImageOperation replace;
  replace.colorReplaceMap[Vec4B(0, 0, 0, 255)] = Vec4B(17, 31, 71, 113);
  Image original = Image::filled(Vec2U(8, 5), Vec4B(0, 0, 0, 255), PixelFormat::RGB24);
  expectOptimizedOperations({BlendImageOperation{BlendImageOperation::Multiply, {"missing"}, Vec2I()},
                             ErrorImageOperation{ImageOperationException("later parse failure")}},
                            original);
  expectOptimizedOperations({replace, BlendImageOperation{BlendImageOperation::Screen, {"missing"}, Vec2I()}, replace},
                            original);
  expectOptimizedOperations(
    {replace, AlphaMaskImageOperation{AlphaMaskImageOperation::Additive, {"missing"}, Vec2I()}, replace}, original);
  expectOptimizedOperations({}, original);
}

TEST(ImageProcessingTest, OptimizedScaleCropPreservesFloatFormatScalarByteSetters) {
  for (auto format : {PixelFormat::RGB_F, PixelFormat::RGBA_F}) {
    Image original(3, 2, format);
    // Nonzero raw storage must not be copied by a byte-pixel nearest fast path.
    std::fill(original.data().begin(), original.data().end(), std::uint8_t(0x3f));
    for (auto mode : {ScaleImageOperation::Nearest, ScaleImageOperation::Bilinear}) {
      expectOptimizedOperations({ScaleImageOperation{mode, Vec2F(2, 2)}, CropImageOperation{RectI(1, 1, 4, 3)}},
                                original);
      expectOptimizedOperations({ScaleImageOperation{mode, Vec2F(2, 2)}, CropImageOperation{RectI(0, 0, 20, 20)},
                                 ErrorImageOperation{ImageOperationException("later error")}},
                                original);
    }
  }
}

TEST(ImageProcessingTest, DirtyBlendRetainsPerOperationCallbackValueSemantics) {
  ColorReplaceImageOperation replace;
  replace.colorReplaceMap[Vec4B(0, 0, 0, 0)] = Vec4B(17, 31, 71, 0);
  Image reference = Image::filled(Vec2U(2, 2), Vec4B(0, 0, 0, 0));
  ImageReferenceCallback callback =
    [calls = 0u, &reference](
      String const& name) mutable -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
    if (name != std::string_view(calls++ == 0 ? "first" : "second"))
      return std::unexpected(ImageOperationException("callback state leaked across operations"));
    return std::cref(reference);
  };
  BlendImageOperation blend{BlendImageOperation::Multiply, {"first", "second"}, Vec2I()};
  List<ImageOperation> operations{replace, blend, replace, blend, blend, replace};
  Image original = Image::filled(Vec2U(8, 5), Vec4B(0, 0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(expectOptimizedOperations(operations, original, callback));
  Image expected = original;
  ASSERT_TRUE(sequentialOperations(operations, expected, callback));
  auto listed = processImageOperations(operations, original, callback);
  ASSERT_TRUE(listed);
  ASSERT_NO_FATAL_FAILURE(expectPixels(*listed, expected));
  ASSERT_NO_FATAL_FAILURE(expectPixels(original, Image::filled(Vec2U(8, 5), Vec4B(0, 0, 0, 0))));
}

TEST(ImageProcessingTest, ListOperationsPreserveInputOnSuccessAndReferenceFailure) {
  Image original = Image::filled(Vec2U(3, 2), Vec4B(91, 47, 213, 0), PixelFormat::BGR24);
  Image before = original;
  List<ImageOperation> operations{SetColorImageOperation{Vec3B(17, 31, 71)},
                                  ScaleImageOperation{ScaleImageOperation::Nearest, Vec2F(2, 2)},
                                  CropImageOperation{RectI(1, 1, 4, 3)}};
  Image expected = original;
  ASSERT_TRUE(sequentialOperations(operations, expected));
  auto listed = processImageOperations(operations, original);
  ASSERT_TRUE(listed);
  ASSERT_NO_FATAL_FAILURE(expectPixels(*listed, expected));
  ASSERT_NO_FATAL_FAILURE(expectPixels(original, before));

  operations.append(BlendImageOperation{BlendImageOperation::Multiply, {"failure", "unreachable"}, Vec2I()});
  std::vector<String> calls;
  ImageReferenceCallback callback =
    [&](String const& name) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
    calls.push_back(name);
    return std::unexpected(ImageOperationException("reference failure"));
  };
  expected = original;
  auto sequential = sequentialOperations(operations, expected, callback);
  ASSERT_FALSE(sequential);
  calls.clear();
  auto failed = processImageOperations(operations, original, callback);
  ASSERT_FALSE(failed);
  EXPECT_EQ(std::string(failed.error().what()), std::string(sequential.error().what()));
  EXPECT_EQ(calls, (std::vector<String>{"failure"}));
  ASSERT_NO_FATAL_FAILURE(expectPixels(original, before));
}
