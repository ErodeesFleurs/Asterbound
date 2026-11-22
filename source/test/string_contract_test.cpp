import std;
import star.format;
import star.string;
import star.string_view;
import star.unicode;

#include "gtest/gtest.h"

using namespace Star;

namespace {
struct string_range {
  std::array<std::string, 3> values{"alpha", "水", "omega"};
  friend auto begin(string_range const& range) { return range.values.begin(); }
  friend auto end(string_range const& range) { return range.values.end(); }
};

struct checked_tag_lookup {
  std::unique_ptr<int> calls = std::make_unique<int>(0);
  std::expected<String, StarException> operator()(String const& key) & {
    ++*calls;
    if (key == "bad")
      return std::unexpected(OutOfRangeException("tag lookup rejected key", false));
    return key + std::to_string(*calls);
  }
  std::expected<String, StarException> operator()(String const&) && = delete;
};

struct counted_string_conversion {
  int* calls;
  operator String() const {
    ++*calls;
    return "next";
  }
};

struct contextual_tag_map {
  std::map<String, String> entries;
  struct comparison {
    bool equal;
    explicit operator bool() const { return equal; }
  };
  struct iterator {
    std::map<String, String>::const_iterator position;
    auto operator->() const { return std::addressof(*position); }
    friend comparison operator==(iterator left, iterator right) {
      return {left.position == right.position};
    }
  };
  iterator find(String const& key) const { return {entries.find(key)}; }
  iterator end() const { return {entries.end()}; }
};

static_assert(string_tag_lookup<checked_tag_lookup>);
static_assert(!string_tag_lookup<decltype([](String const&) { return 42; })>);
static_assert(string_view_tag_lookup<decltype([](std::string_view key) { return String(key); })>);
}

TEST(StringContractTest, RepetitionChecksEncodedByteCapacityAfterValidatingThePoint) {
  auto invalid_point = static_cast<String::Char>(0x110000);
  auto empty = String::create(invalid_point, 0);
  ASSERT_TRUE(empty);
  EXPECT_TRUE(empty->empty());
  auto invalid = String::create(invalid_point, std::numeric_limits<std::size_t>::max());
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<UnicodeException>());
  auto overflow = String::create(U'水', std::numeric_limits<std::size_t>::max() / 3 + 1);
  ASSERT_FALSE(overflow);
  EXPECT_TRUE(overflow.error().is<StringException>());
  auto zeros = String::create(U'\0', 257);
  ASSERT_TRUE(zeros);
  EXPECT_EQ(zeros->utf8(), std::string(257, '\0'));
  auto repeated = String::create(U'水', 5);
  ASSERT_TRUE(repeated);
  EXPECT_EQ(repeated->utf8(), "水水水水水");
}

TEST(StringContractTest, PaddingRepeatsWholeFillerAndPreservesValidationOrder) {
  String base("é");
  String filler(std::string_view("\0水", 4));
  auto left = base.pad_left(4, filler);
  auto right = base.pad_right(4, filler);
  ASSERT_TRUE(left);
  ASSERT_TRUE(right);
  EXPECT_EQ(left->utf8(), std::string_view("\0水\0水é", 10));
  EXPECT_EQ(right->utf8(), std::string_view("é\0水\0水", 10));
  String invalid(std::string_view("\xff", 1));
  for (auto pad : {&String::pad_left, &String::pad_right}) {
    auto unchanged = (invalid.*pad)(9, "");
    ASSERT_TRUE(unchanged);
    EXPECT_EQ(*unchanged, invalid);
    auto bad_filler = (base.*pad)(0, invalid);
    ASSERT_FALSE(bad_filler);
    EXPECT_TRUE(bad_filler.error().is<UnicodeException>());
    auto bad_base = (invalid.*pad)(0, filler);
    ASSERT_FALSE(bad_base);
    EXPECT_TRUE(bad_base.error().is<UnicodeException>());
    auto overflow = (base.*pad)(std::numeric_limits<std::size_t>::max(), filler);
    ASSERT_FALSE(overflow);
    EXPECT_TRUE(overflow.error().is<StringException>());
  }
}

TEST(StringContractTest, JoiningDoesNotConvertLaterArgumentsAfterFirstError) {
  int conversions = 0;
  String invalid(std::string("a\xff", 2));
  auto failed = String::join_with("/", invalid, "b", "c",
      counted_string_conversion{&conversions}, counted_string_conversion{&conversions});
  ASSERT_FALSE(failed);
  EXPECT_EQ(conversions, 0);

  auto joined = String::join_with("/", "a", "b", "c",
      counted_string_conversion{&conversions}, counted_string_conversion{&conversions});
  ASSERT_TRUE(joined);
  EXPECT_EQ(*joined, "a/b/c/next/next");
  EXPECT_EQ(conversions, 2);
}

TEST(StringContractTest, TagMapAcceptsContextualEqualityAndPreservesMissingTags) {
  contextual_tag_map tags{{{"known", "value"}}};
  String source("<known>/<missing>");
  EXPECT_EQ(source.replace_tags(tags), "value/<missing>");
  EXPECT_EQ(source.replace_tags(tags, true, "_"), "value/_");
}

TEST(StringContractTest, RangeConstructionSupportsAdlAndDistinctSentinels) {
  string_range range;
  EXPECT_EQ(StringList::from(range), (StringList{"alpha", "水", "omega"}));
  auto first = std::counted_iterator(range.values.begin(), 2);
  StringList prefix(first, std::default_sentinel);
  EXPECT_EQ(prefix, (StringList{"alpha", "水"}));
  EXPECT_EQ(range.values[0], "alpha");
}

TEST(StringContractTest, InputRangeIsConsumedOnceWithoutPrecounting) {
  std::istringstream input("alpha 水 omega");
  auto words = std::ranges::istream_view<std::string>(input);
  StringList result(std::from_range, words);
  EXPECT_EQ(result, (StringList{"alpha", "水", "omega"}));
}

TEST(StringContractTest, DefaultIteratorEndpointRetainsSinglePassConstruction) {
  std::istringstream input("alpha 水 omega");
  StringList result(std::istream_iterator<std::string>(input), {});
  EXPECT_EQ(result, (StringList{"alpha", "水", "omega"}));
}

TEST(StringContractTest, ExplicitMoveRangeTransfersLongStringStorage) {
  std::vector<String> source;
  source.emplace_back(std::string(4096, 'a'));
  source.emplace_back(std::string(4096, 'b'));
  auto first_storage = source[0].utf8().data();
  auto second_storage = source[1].utf8().data();
  auto moving = std::ranges::subrange(std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
  StringList result(std::from_range, moving);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], String(std::string(4096, 'a')));
  EXPECT_EQ(result[1], String(std::string(4096, 'b')));
  EXPECT_EQ(result[0].utf8().data(), first_storage);
  EXPECT_EQ(result[1].utf8().data(), second_storage);
}

TEST(StringContractTest, MoveOnlyTagCallbackUsesStoredLvalueAndStopsAtFirstError) {
  checked_tag_lookup lookup;
  auto result = String("<a><b>").lookup_tags(lookup);
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, "a1b2");
  EXPECT_EQ(*lookup.calls, 2);

  checked_tag_lookup failing;
  auto failed = String("<a><bad><later>").lookup_tags(failing);
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<OutOfRangeException>());
  EXPECT_EQ(*failing.calls, 2);

  auto temporary = String("<a><b>").lookup_tags(checked_tag_lookup{});
  ASSERT_TRUE(temporary);
  EXPECT_EQ(*temporary, "a1b2");
}

TEST(StringContractTest, ViewTagCallbackRetainsOwningResultsUntilBytesAreCopied) {
  auto replacement = std::make_unique<String>(std::string(4096, 'x') + std::string("\0水", 4));
  String expected = String("before") + *replacement + "after";
  auto result = String("before<key>after").maybe_lookup_tags_view(
      [value = std::move(replacement)](std::string_view) mutable { return *value; });
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, expected);
}

TEST(StringContractTest, ViewTagLookupAcceptsExistingUtf8BearingBorrowedKeys) {
  String owner("MiXeD 水");
  auto key = CaseInsensitiveStringView::create(owner);
  ASSERT_TRUE(key);
  auto result = String("before<x>after").maybe_lookup_tags_view(
      [&key](std::string_view) { return *key; });
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, "beforeMiXeD 水after");
}
