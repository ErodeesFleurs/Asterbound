import std;
import star.directives;
import star.image_processing;
import star.image;
import star.vector;
import star.rect;
import star.string;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

namespace {
struct DirectiveReferenceFailureTag {
  static constexpr std::string_view name() { return "DirectiveReferenceFailure"; }
};
using DirectiveReferenceFailure = StarError<DirectiveReferenceFailureTag, StarException>;

Image directiveSource(PixelFormat format = PixelFormat::RGBA32) {
  Image image(7, 5, format);
  for (unsigned y = 0; y < image.height(); ++y)
    for (unsigned x = 0; x < image.width(); ++x)
      EXPECT_TRUE(image.setrgb(x, y, Vec4B(17 + x * 23, 31 + y * 29, 83, (x + y) % 3 ? 191 : 0)));
  return image;
}

void expectDirectiveImage(Image const& actual, Image const& expected) {
  ASSERT_EQ(actual.size(), expected.size());
  ASSERT_EQ(actual.pixelFormat(), expected.pixelFormat());
  ASSERT_EQ(actual.data().size(), expected.data().size());
  EXPECT_TRUE(std::equal(actual.data().begin(), actual.data().end(), expected.data().begin()));
}

std::expected<void, StarException> sequentialValidGroup(DirectivesGroup const& group, Image& image) {
  return group.forEach(
    [&](Directives::Entry const& entry, Directives const& directives) -> std::expected<void, StarException> {
      auto shared = directives.get();
      if (!shared)
        return std::unexpected(std::move(shared.error()));
      return processImageOperation(entry.loadOperation(shared->get()), image);
    });
}
}// namespace

TEST(ImageDirectivePipelineTest, OptimizationCrossesGroupsWithoutChangingPixelsOrSource) {
  for (auto [format, bilinear] : {std::pair{PixelFormat::RGB24, false}, std::pair{PixelFormat::BGR24, true},
                                  std::pair{PixelFormat::RGBA32, true}, std::pair{PixelFormat::BGRA32, false}}) {
    auto source = directiveSource(format);
    Image untouched = source;
    DirectivesGroup group(String("?brightness=17?multiply=f1e3d5a7"));
    group.append(
      Directives(bilinear ? std::string_view("?scalebilinear=2.5;1.75") : std::string_view("?scalenearest=2.5;1.75")));
    group.append(Directives("?crop=2;1;13;8"));
    group.append(Directives("?saturation=-12?flipy?setcolor=123456"));
    Image expected = source;
    ASSERT_TRUE(sequentialValidGroup(group, expected));
    auto actual = group.applyNewImage(source);
    ASSERT_TRUE(actual) << actual.error().what();
    expectDirectiveImage(*actual, expected);
    expectDirectiveImage(source, untouched);
  }
}

TEST(ImageDirectivePipelineTest, LaterParseErrorRetainsSuccessfullyExecutedPrefix) {
  DirectivesGroup prefix(String("?setcolor=123456?scalenearest=2;3"));
  Image expected = directiveSource();
  ASSERT_TRUE(sequentialValidGroup(prefix, expected));
  for (bool separateGroup : {false, true}) {
    DirectivesGroup group(String("?setcolor=123456?scalenearest=2;3"));
    if (separateGroup)
      group.append(Directives("?crop=not-a-number"));
    else
      group = DirectivesGroup(String("?setcolor=123456?scalenearest=2;3?crop=not-a-number"));
    Image actual = directiveSource();
    auto result = group.applyExistingImage(actual);
    ASSERT_FALSE(result);
    expectDirectiveImage(actual, expected);
  }
}

TEST(ImageDirectivePipelineTest, EarlierExecutionErrorWinsOverLaterInvalidOperation) {
  Image expected = directiveSource();
  ASSERT_TRUE(processImageOperation(SetColorImageOperation{Vec3B(18, 52, 86)}, expected));
  auto firstError = processImageOperation(CropImageOperation{RectI(0, 0, 99, 99)}, expected);
  ASSERT_FALSE(firstError);
  EXPECT_TRUE(firstError.error().is<ImageException>());
  for (auto suffix : {std::string_view("?notAnOperation"), std::string_view("?crop=not-a-number")}) {
    DirectivesGroup group(String("?setcolor=123456?crop=0;0;99;99"));
    group.append(Directives(suffix));
    Image actual = directiveSource();
    auto result = group.applyExistingImage(actual);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<ImageException>());
    expectDirectiveImage(actual, expected);
  }
}

TEST(ImageDirectivePipelineTest, FailedReferenceKeepsEarlierBlendAndPrecedesParseError) {
  Image tile(2, 2, PixelFormat::RGBA32);
  tile.fill(Vec4B(127, 213, 83, 191));
  std::vector<String> calls;
  auto reference =
    [&](String const& path) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
    calls.push_back(path);
    if (path == "/tile.png")
      return std::cref(tile);
    return std::unexpected(DirectiveReferenceFailure());
  };
  DirectivesGroup prefix(String("?blendmult=/tile.png;1;1"));
  Image expected = directiveSource();
  ASSERT_TRUE(prefix.applyExistingImage(expected, reference));
  EXPECT_EQ(calls, (std::vector<String>{"/tile.png"}));
  calls.clear();
  DirectivesGroup group(String("?blendmult=/tile.png;1;1?blendscreen=/missing.png?crop=not-a-number"));
  Image actual = directiveSource();
  auto result = group.applyExistingImage(actual, reference);
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<DirectiveReferenceFailure>());
  EXPECT_EQ(calls, (std::vector<String>{"/tile.png", "/missing.png"}));
  expectDirectiveImage(actual, expected);
}
