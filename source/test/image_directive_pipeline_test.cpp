import std;
import star.directives;
import star.image_processing;
import star.image;
import star.vector;
import star.rect;
import star.string;
import star.string_view;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

namespace {
struct DirectiveReferenceFailureTag {
  static constexpr std::string_view name() { return "DirectiveReferenceFailure"; }
};
using DirectiveReferenceFailure = StarError<DirectiveReferenceFailureTag, StarException>;

Image directive_source(PixelFormat format = PixelFormat::RGBA32) {
  Image image(7, 5, format);
  for (unsigned y = 0; y < image.height(); ++y)
    for (unsigned x = 0; x < image.width(); ++x)
      EXPECT_TRUE(image.set_rgb(x, y, Vec4B(17 + x * 23, 31 + y * 29, 83, (x + y) % 3 ? 191 : 0)));
  return image;
}

void expect_directive_image(Image const& actual, Image const& expected) {
  ASSERT_EQ(actual.size(), expected.size());
  ASSERT_EQ(actual.pixel_format(), expected.pixel_format());
  ASSERT_EQ(actual.data().size(), expected.data().size());
  EXPECT_TRUE(std::equal(actual.data().begin(), actual.data().end(), expected.data().begin()));
}

std::expected<void, StarException> sequential_valid_group(DirectivesGroup const& group, Image& image) {
  return group.for_each(
    [&](Directives::Entry const& entry, Directives const& directives) -> std::expected<void, StarException> {
      return process_image_operation(directives.entry_operation(entry), image);
    });
}

template <typename Owner>
concept borrowable_directives = requires(Owner&& owner, Directives::Entry const& entry) {
  std::forward<Owner>(owner).entries();
  std::forward<Owner>(owner).operations();
  std::forward<Owner>(owner).entry_string(entry);
  std::forward<Owner>(owner).entry_operation(entry);
  std::forward<Owner>(owner).prefix();
  std::forward<Owner>(owner).maybe_string();
};

template <typename Owner>
concept borrowable_directive_group = requires(Owner&& owner) { std::forward<Owner>(owner).list(); };

static_assert(borrowable_directives<Directives&> && borrowable_directives<Directives const&>);
static_assert(!borrowable_directives<Directives> && !borrowable_directives<Directives const>);
static_assert(borrowable_directive_group<DirectivesGroup&> && borrowable_directive_group<DirectivesGroup const&>);
static_assert(!borrowable_directive_group<DirectivesGroup> && !borrowable_directive_group<DirectivesGroup const>);
}// namespace

TEST(ImageDirectivePipelineTest, OptimizationCrossesGroupsWithoutChangingPixelsOrSource) {
  for (auto [format, bilinear] : {std::pair{PixelFormat::RGB24, false}, std::pair{PixelFormat::BGR24, true},
                                  std::pair{PixelFormat::RGBA32, true}, std::pair{PixelFormat::BGRA32, false}}) {
    auto source = directive_source(format);
    Image untouched = source;
    DirectivesGroup group(String("?brightness=17?multiply=f1e3d5a7"));
    group.append(
      Directives(bilinear ? std::string_view("?scalebilinear=2.5;1.75") : std::string_view("?scalenearest=2.5;1.75")));
    group.append(Directives("?crop=2;1;13;8"));
    group.append(Directives("?saturation=-12?flipy?setcolor=123456"));
    Image expected = source;
    ASSERT_TRUE(sequential_valid_group(group, expected));
    auto actual = group.apply_new_image(source);
    ASSERT_TRUE(actual) << actual.error().what();
    expect_directive_image(*actual, expected);
    expect_directive_image(source, untouched);
  }
}

TEST(ImageDirectivePipelineTest, LaterParseErrorRetainsSuccessfullyExecutedPrefix) {
  DirectivesGroup prefix(String("?setcolor=123456?scalenearest=2;3"));
  Image expected = directive_source();
  ASSERT_TRUE(sequential_valid_group(prefix, expected));
  for (bool separateGroup : {false, true}) {
    DirectivesGroup group(String("?setcolor=123456?scalenearest=2;3"));
    if (separateGroup)
      group.append(Directives("?crop=not-a-number"));
    else
      group = DirectivesGroup(String("?setcolor=123456?scalenearest=2;3?crop=not-a-number"));
    Image actual = directive_source();
    auto result = group.apply_existing_image(actual);
    ASSERT_FALSE(result);
    expect_directive_image(actual, expected);
  }
}

TEST(ImageDirectivePipelineTest, EarlierExecutionErrorWinsOverLaterInvalidOperation) {
  Image expected = directive_source();
  ASSERT_TRUE(process_image_operation(SetColorImageOperation{Vec3B(18, 52, 86)}, expected));
  auto firstError = process_image_operation(CropImageOperation{RectI(0, 0, 99, 99)}, expected);
  ASSERT_FALSE(firstError);
  EXPECT_TRUE(firstError.error().is<ImageException>());
  for (auto suffix : {std::string_view("?notAnOperation"), std::string_view("?crop=not-a-number")}) {
    DirectivesGroup group(String("?setcolor=123456?crop=0;0;99;99"));
    group.append(Directives(suffix));
    Image actual = directive_source();
    auto result = group.apply_existing_image(actual);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<ImageException>());
    expect_directive_image(actual, expected);
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
  Image expected = directive_source();
  ASSERT_TRUE(prefix.apply_existing_image(expected, reference));
  EXPECT_EQ(calls, (std::vector<String>{"/tile.png"}));
  calls.clear();
  DirectivesGroup group(String("?blendmult=/tile.png;1;1?blendscreen=/missing.png?crop=not-a-number"));
  Image actual = directive_source();
  auto result = group.apply_existing_image(actual, reference);
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<DirectiveReferenceFailure>());
  EXPECT_EQ(calls, (std::vector<String>{"/tile.png", "/missing.png"}));
  expect_directive_image(actual, expected);
}

TEST(ImageDirectivePipelineTest, EmptyIdentitySkipsChildrenAndTruthinessMatchesContent) {
  Directives empty;
  Directives separators("???");
  EXPECT_EQ(empty, separators);
  EXPECT_EQ(empty.hash(), separators.hash());
  EXPECT_FALSE(separators);
  EXPECT_TRUE(separators.entries().empty());
  EXPECT_TRUE(separators.operations().empty());
  EXPECT_FALSE(separators.maybe_string());
  EXPECT_TRUE(separators.prefix().empty());

  DirectivesGroup group;
  auto empty_hash = group.hash();
  group.append(empty);
  group += separators;
  EXPECT_TRUE(group.empty());
  EXPECT_FALSE(group);
  EXPECT_TRUE(group.list().empty());
  EXPECT_EQ(group.hash(), empty_hash);
  EXPECT_EQ(group, DirectivesGroup{});

  group.append(Directives("?flipy"));
  DirectivesGroup expected(String("?flipy"));
  EXPECT_TRUE(group);
  EXPECT_FALSE(group.empty());
  EXPECT_EQ(group, expected);
  auto nonempty_hash = group.hash();
  group.append(Directives{});
  EXPECT_EQ(group, expected);
  EXPECT_EQ(group.hash(), nonempty_hash);
  group.clear();
  EXPECT_FALSE(group);
  EXPECT_EQ(group.hash(), empty_hash);
}

TEST(ImageDirectivePipelineTest, ExactTextAndStructuralBoundariesDefineIdentity) {
  String text("prefix??brightness=1?flipy?");
  Directives directives(text);
  Directives equal(text);
  EXPECT_EQ(directives, equal);
  EXPECT_EQ(directives.string(), text);
  EXPECT_EQ(directives.build_string(), "prefix?brightness=1?flipy");
  EXPECT_NE(directives, Directives("prefix?brightness=1?flipy"));
  EXPECT_NE(directives, Directives("prefix??brightness=2?flipy?"));

  Directives utf8("前缀?blendmult=/图像.png;1;2?flipy");
  EXPECT_EQ(utf8.prefix().utf8(), "前缀");
  EXPECT_EQ(utf8.entry_string(utf8.entries()[1]).utf8(), "blendmult=/图像.png;1;2");
  EXPECT_EQ(utf8.entry_string(utf8.entries()[2]).utf8(), "flipy");
  EXPECT_EQ(std::get<BlendImageOperation>(utf8.operations()[1]).blendImages[0], "/图像.png");
  EXPECT_EQ(Directives("?flipy").string(), "?flipy");

  DirectivesGroup split(String("?brightness=1"));
  split.append(Directives("?flipy"));
  DirectivesGroup same = split;
  DirectivesGroup flat(split.to_string());
  EXPECT_EQ(split, same);
  EXPECT_EQ(split.hash(), same.hash());
  EXPECT_EQ(split.to_string(), flat.to_string());
  EXPECT_NE(split, flat);
  DirectivesGroup reversed(String("?flipy"));
  reversed.append(Directives("?brightness=1"));
  EXPECT_NE(split, reversed);
}

TEST(ImageDirectivePipelineTest, MovesTransferCountAndLeaveReusableEmptySources) {
  Directives directives("prefix?flipy");
  auto entry = directives.entries()[1];
  auto text = directives.entry_string(entry);
  auto operations = directives.operations();
  Directives retained = directives;
  Directives moved(std::move(directives));
  EXPECT_FALSE(directives);
  EXPECT_EQ(text.utf8(), "flipy");
  EXPECT_EQ(operations.data(), moved.operations().data());
  EXPECT_EQ(operations.data(), retained.operations().data());
  EXPECT_EQ(retained.entry_operation(entry).index(), moved.entry_operation(entry).index());
  directives = "?flipx";
  moved = std::move(directives);
  EXPECT_FALSE(directives);
  EXPECT_EQ(moved.string(), "?flipx");

  DirectivesGroup source(String("?brightness=1"));
  source.append(Directives("?flipy"));
  DirectivesGroup expected = source;
  auto hash = source.hash();
  DirectivesGroup destination(std::move(source));
  EXPECT_EQ(destination, expected);
  EXPECT_EQ(destination.hash(), hash);
  EXPECT_FALSE(source);
  EXPECT_TRUE(source.list().empty());
  EXPECT_EQ(source.hash(), DirectivesGroup{}.hash());
  source.append(Directives("?flipx"));
  destination = std::move(source);
  EXPECT_EQ(destination, DirectivesGroup(String("?flipx")));
  EXPECT_FALSE(source);
  EXPECT_TRUE(source.list().empty());
  source.append(Directives("?flipy"));
  destination = source;
  EXPECT_EQ(destination, source);
  EXPECT_EQ(destination.hash(), source.hash());
  std::size_t count = 0;
  destination.for_each([&](Directives::Entry const&, Directives const&) { ++count; });
  EXPECT_EQ(count, 1u);
}

TEST(ImageDirectivePipelineTest, LongSharedOperationsPublishStableValuesAcrossThreads) {
  String text("prefix");
  for (unsigned i = 0; i < 96; ++i)
    text += "?brightness=1";
  text += "?border=1;ffffffff?crop=invalid";
  ASSERT_GE(text.utf8().size(), 1000u);
  Directives owner(text);
  auto entries = owner.entries();
  ASSERT_EQ(entries.size(), 99u);
  EXPECT_EQ(owner.prefix().utf8(), "prefix");
  EXPECT_EQ(owner.entry_string(entries.front()).utf8(), "prefix");
  EXPECT_EQ(owner.entry_string(entries[96]).utf8(), "brightness=1");
  EXPECT_EQ(owner.entry_string(entries[97]).utf8(), "border=1;ffffffff");
  EXPECT_EQ(owner.entry_string(entries.back()).utf8(), "crop=invalid");

  constexpr std::size_t thread_count = 8;
  std::barrier<> start(thread_count);
  std::array<bool, thread_count> valid{};
  DirectivesGroup group(String("?brightness=1?flipy"));
  DirectivesGroup expected_group = group;
  auto expected_hash = expected_group.hash();
  std::vector<std::jthread> workers;
  workers.reserve(thread_count);
  for (std::size_t thread = 0; thread < thread_count; ++thread) {
    workers.emplace_back([&, copy = owner, thread] {
      start.arrive_and_wait();
      bool matches = true;
      for (unsigned pass = 0; pass < 8; ++pass) {
        copy.load_operations();
        auto operations = copy.operations();
        matches &= operations.size() == entries.size()
          && std::holds_alternative<NullImageOperation>(operations.front())
          && std::holds_alternative<ErrorImageOperation>(operations.back())
          && operations.data() == owner.operations().data()
          && group.hash() == expected_hash;
        for (std::size_t i = 1; i <= 96; ++i) {
          matches &= std::holds_alternative<BrightnessMultiplyImageOperation>(operations[i]);
          if (std::holds_alternative<BrightnessMultiplyImageOperation>(operations[i]))
            matches &= std::abs(std::get<BrightnessMultiplyImageOperation>(operations[i]).brightnessMultiply - 1.01f) < 1e-6f;
        }
      }
      valid[thread] = matches;
    });
  }
  for (auto& worker : workers)
    worker.join();
  EXPECT_TRUE(std::ranges::all_of(valid, std::identity{}));
}

TEST(ImageDirectivePipelineTest, ErrorReportsRetryFailuresAndShareSuccessfulSuppression) {
  Directives owner("?crop=invalid?brightness=invalid?flipy");
  Directives copy = owner;
  auto entries = owner.entries();
  ASSERT_EQ(entries.size(), 3u);
  unsigned calls = 0;
  auto failed = owner.report_error(entries[0], [&]() -> std::expected<void, StarException> {
    ++calls;
    EXPECT_TRUE(std::holds_alternative<ErrorImageOperation>(copy.entry_operation(entries[0])));
    return std::unexpected(DirectiveReferenceFailure());
  });
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<DirectiveReferenceFailure>());
  EXPECT_EQ(calls, 1u);
  ASSERT_TRUE(copy.report_error(entries[0], [&]() -> std::expected<void, StarException> {
    ++calls;
    owner.load_operations();
    return {};
  }));
  ASSERT_TRUE(owner.report_error(entries[0], [&]() -> std::expected<void, StarException> {
    ++calls;
    return std::unexpected(DirectiveReferenceFailure());
  }));
  EXPECT_EQ(calls, 2u);
  ASSERT_TRUE(copy.report_error(entries[1], [&]() -> std::expected<void, StarException> {
    ++calls;
    // Reentry on a different error also exercises callback invocation without
    // holding the publication or suppression lock.
    return owner.report_error(entries[0], []() -> std::expected<void, StarException> {
      return std::unexpected(DirectiveReferenceFailure());
    });
  }));
  EXPECT_EQ(calls, 3u);
  ASSERT_TRUE(owner.report_error(entries[2], [&]() -> std::expected<void, StarException> {
    ++calls;
    return {};
  }));
  EXPECT_EQ(calls, 3u);
}

TEST(ImageDirectivePipelineTest, ErrorReportPinsOriginalOwnerAcrossCallbackReplacement) {
  for (bool clear_owner : {false, true}) {
    Directives owner("?crop=invalid");
    Directives original = owner;
    auto const& entry = original.entries()[0];
    unsigned calls = 0;
    ASSERT_TRUE(owner.report_error(entry, [&]() -> std::expected<void, StarException> {
      ++calls;
      if (clear_owner)
        owner = Directives{};
      else
        owner = "?brightness=invalid";
      return {};
    }));
    ASSERT_TRUE(original.report_error(entry, [&]() -> std::expected<void, StarException> {
      ++calls;
      return std::unexpected(DirectiveReferenceFailure());
    }));
    EXPECT_EQ(calls, 1u);
    if (clear_owner)
      owner = "?brightness=invalid";
    ASSERT_TRUE(owner.report_error(owner.entries()[0], [&]() -> std::expected<void, StarException> {
      ++calls;
      return {};
    }));
    EXPECT_EQ(calls, 2u);
  }
}

TEST(ImageDirectivePipelineTest, AcceptedNullPrefixStillPromotesTwentyFourBitInput) {
  for (auto format : {PixelFormat::RGB24, PixelFormat::BGR24}) {
    Image source = directive_source(format);
    Image expected = source;
    ASSERT_TRUE(process_image_operation(NullImageOperation{}, expected));
    DirectivesGroup group(String("prefix"));
    auto actual = group.apply_new_image(source);
    ASSERT_TRUE(actual);
    expect_directive_image(*actual, expected);
    ASSERT_TRUE(group.apply_existing_image(source));
    expect_directive_image(source, expected);
  }
}

TEST(ImageDirectivePipelineTest, ChildPrefixesAreAcceptedOnlyInTheirContext) {
  Image source = directive_source(PixelFormat::RGB24);
  DirectivesGroup prefix(String("?brightness=1"));
  Image brightened = source;
  ASSERT_TRUE(prefix.apply_existing_image(brightened));
  DirectivesGroup accepted = prefix;
  accepted.append(Directives("prefix?flipy"));
  Image expected = brightened;
  ASSERT_TRUE(process_image_operation(FlipImageOperation{FlipImageOperation::FlipY}, expected));
  auto actual = accepted.apply_new_image(source);
  ASSERT_TRUE(actual);
  expect_directive_image(*actual, expected);

  DirectivesGroup rejected = prefix;
  rejected.append(Directives("?prefix?flipy"));
  Image partially_applied = source;
  auto result = rejected.apply_existing_image(partially_applied);
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<DirectivesException>());
  expect_directive_image(partially_applied, brightened);
}
