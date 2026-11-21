import std;

#include "gtest/gtest.h"

import star.format;
import star.list;
import star.pythonic;
import star.static_vector;

using namespace Star;

namespace {

template <typename Result, typename Input>
concept can_slice = requires(Input const& input) { Star::slice<Result>(input); };

template <typename Input>
concept can_member_slice = requires(Input const& input) { input.slice(); };

struct MissingIndex {
  std::size_t size() const;
};

struct SignedSize {
  int size() const;
  int operator[](std::size_t) const;
};

struct BooleanAppend {
  bool push_back(int);
};

struct WrongErrorAppend {
  std::expected<void, int> push_back(int);
};

struct ConsumingAppend {
  void push_back(std::unique_ptr<int>&&);
};

struct NondefaultResult {
  NondefaultResult() = delete;
  void push_back(int);
};

static_assert(can_slice<std::vector<int>, std::array<int, 3>>);
static_assert(can_slice<StaticList<int, 2>, std::array<int, 3>>);
static_assert(!can_slice<std::vector<int>, MissingIndex>);
static_assert(!can_slice<std::vector<int>, SignedSize>);
static_assert(!can_slice<BooleanAppend, std::array<int, 3>>);
static_assert(!can_slice<WrongErrorAppend, std::array<int, 3>>);
static_assert(!can_slice<ConsumingAppend, std::array<std::unique_ptr<int>, 3>>);
static_assert(!can_slice<std::vector<std::unique_ptr<int>>, std::array<std::unique_ptr<int>, 3>>);
static_assert(!can_slice<NondefaultResult, std::array<int, 3>>);
static_assert(!can_member_slice<List<std::unique_ptr<int>>>);
static_assert(!can_member_slice<StaticList<std::unique_ptr<int>, 2>>);
static_assert(!can_member_slice<SmallList<std::unique_ptr<int>, 2>>);
static_assert(!can_member_slice<Deque<std::unique_ptr<int>>>);
static_assert(std::same_as<decltype(Star::slice(std::declval<List<int> const&>())), List<int>>);
static_assert(std::same_as<decltype(Star::slice<List<int>>(std::declval<List<int> const&>())), List<int>>);
static_assert(std::same_as<decltype(Star::slice(std::declval<StaticList<int, 2> const&>())),
                           std::expected<StaticList<int, 2>, StarException>>);

struct RecursiveValue {
  List<RecursiveValue> children;
};
static_assert(can_member_slice<List<RecursiveValue>>);

constexpr auto reverse_plan = normalize_slice(5, std::nullopt, -5, -2);
static_assert(reverse_plan.reverse && reverse_plan.start == 4 && reverse_plan.stride == 2 && reverse_plan.count == 3);
static_assert(normalize_slice(std::numeric_limits<std::size_t>::max(), std::nullopt, std::nullopt, 1).count
              == std::numeric_limits<std::size_t>::max());

struct IndexedInput {
  std::size_t length;
  std::vector<std::size_t>& visited;

  std::size_t size() const { return length; }
  std::size_t operator[](std::size_t index) const {
    EXPECT_LT(index, length);
    visited.push_back(index);
    return index;
  }
};

struct OwningValue {
  OwningValue(int v, std::vector<int>& copies) : value(std::make_unique<int>(v)), copies(copies) {}
  OwningValue(OwningValue const& other) : value(std::make_unique<int>(*other.value)), copies(other.copies) {
    copies.push_back(*value);
  }
  OwningValue(OwningValue&&) = default;
  OwningValue& operator=(OwningValue const&) = delete;
  OwningValue& operator=(OwningValue&&) = delete;

  std::unique_ptr<int> value;
  std::vector<int>& copies;
};

}// namespace

TEST(ListSlice, PositiveBoundsPreserveLegacyClamping) {
  List<int> values{0, 1, 2, 3, 4};
  EXPECT_EQ(values.slice(), (List<int>{0, 1, 2, 3, 4}));
  EXPECT_EQ(values.slice(1, 5, 2), (List<int>{1, 3}));
  EXPECT_EQ(values.slice(-4, -1), (List<int>{1, 2, 3}));
  EXPECT_EQ(values.slice(-5, std::nullopt), values);
  EXPECT_EQ(values.slice(-99, 99), values);
  EXPECT_TRUE(values.slice(99).empty());
  // Legacy stop bounds at or below -size denote before-begin, not index zero.
  EXPECT_TRUE(values.slice(0, -5).empty());
  EXPECT_TRUE(values.slice(0, -99).empty());
  EXPECT_TRUE(values.slice(4, 1).empty());
  EXPECT_TRUE(values.slice(2, 2).empty());
}

TEST(ListSlice, NegativeStrideTraversesDefaultAndExplicitBounds) {
  List<int> values{0, 1, 2, 3, 4};
  EXPECT_EQ(values.slice(std::nullopt, std::nullopt, -1), (List<int>{4, 3, 2, 1, 0}));
  EXPECT_EQ(values.slice(std::nullopt, std::nullopt, -2), (List<int>{4, 2, 0}));
  EXPECT_EQ(values.slice(4, 1, -1), (List<int>{4, 3, 2}));
  EXPECT_EQ(values.slice(-2, -4, -1), (List<int>{3, 2}));
  EXPECT_EQ(values.slice(99, std::nullopt, -2), (List<int>{4, 2, 0}));
  EXPECT_EQ(values.slice(5, -5, -1), (List<int>{4, 3, 2, 1, 0}));
  EXPECT_EQ(values.slice(-5, std::nullopt, -1), (List<int>{0}));
  EXPECT_EQ(values.slice(-99, -99, -1), (List<int>{0}));
  EXPECT_EQ(values.slice(4, -1, -1), (List<int>{}));
  EXPECT_TRUE(values.slice(1, 4, -1).empty());
  EXPECT_TRUE(values.slice(4, 99, -1).empty());
  EXPECT_TRUE(values.slice(2, 2, -1).empty());
}

TEST(ListSlice, ExtremeIndexesAndStridesDoNotOverflow) {
  constexpr int low = std::numeric_limits<int>::min();
  constexpr int high = std::numeric_limits<int>::max();
  List<int> values{0, 1, 2, 3, 4};
  EXPECT_EQ(values.slice(low, high), values);
  EXPECT_TRUE(values.slice(high, low).empty());
  EXPECT_TRUE(values.slice(0, low).empty());
  EXPECT_EQ(values.slice(high, low, -1), (List<int>{4, 3, 2, 1, 0}));
  EXPECT_EQ(values.slice(low, std::nullopt, -1), (List<int>{0}));
  EXPECT_EQ(values.slice(std::nullopt, std::nullopt, low), (List<int>{4}));
  EXPECT_EQ(values.slice(std::nullopt, std::nullopt, high), (List<int>{0}));
  EXPECT_EQ(values.slice(3, 0, low), (List<int>{3}));
}

TEST(ListSlice, LargeVirtualSequencesKeepFullWidthIndexes) {
  std::vector<std::size_t> visited;
  auto high = static_cast<std::size_t>(std::numeric_limits<int>::max());
  IndexedInput wide{high + 3, visited};
  auto forward = Star::slice<std::vector<std::size_t>>(wide, std::nullopt, std::nullopt, std::numeric_limits<int>::max());
  EXPECT_EQ(forward, (std::vector<std::size_t>{0, high}));
  EXPECT_EQ(visited, forward);

  visited.clear();
  auto reverse = Star::slice<std::vector<std::size_t>>(wide, std::nullopt, std::nullopt, std::numeric_limits<int>::min());
  EXPECT_EQ(reverse, (std::vector<std::size_t>{high + 2, 1}));
  EXPECT_EQ(visited, reverse);

  visited.clear();
  constexpr auto maximum = std::numeric_limits<std::size_t>::max();
  IndexedInput largest{maximum, visited};
  auto tail = Star::slice<std::vector<std::size_t>>(largest, -3);
  EXPECT_EQ(tail, (std::vector<std::size_t>{maximum - 3, maximum - 2, maximum - 1}));
  EXPECT_EQ(visited, tail);

  visited.clear();
  auto reverse_tail = Star::slice<std::vector<std::size_t>>(largest, -1, -4, -1);
  EXPECT_EQ(reverse_tail, (std::vector<std::size_t>{maximum - 1, maximum - 2, maximum - 3}));
  EXPECT_EQ(visited, reverse_tail);
}

TEST(ListSlice, EmptyAndZeroStrideNeverIndexInput) {
  std::vector<std::size_t> visited;
  IndexedInput empty{0, visited};
  EXPECT_TRUE(Star::slice<std::vector<std::size_t>>(empty).empty());
  EXPECT_TRUE(Star::slice<std::vector<std::size_t>>(empty, std::nullopt, std::nullopt, -1).empty());
  EXPECT_TRUE(Star::slice<std::vector<std::size_t>>(empty, -1, 1, 0).empty());
  IndexedInput values{5, visited};
  EXPECT_TRUE(Star::slice<std::vector<std::size_t>>(values, std::nullopt, std::nullopt, 0).empty());
  EXPECT_TRUE(
    Star::slice<std::vector<std::size_t>>(values, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0)
      .empty());
  auto bounded = Star::slice<StaticList<std::size_t, 0>>(values, std::nullopt, std::nullopt, 0);
  ASSERT_TRUE(bounded);
  EXPECT_TRUE(bounded->empty());
  EXPECT_TRUE(visited.empty());
}

TEST(ListSlice, BoundedOverflowRetainsOrderedPrefixAndFirstError) {
  std::vector<std::size_t> visited;
  IndexedInput input{5, visited};
  auto forward = Star::slice<StaticList<std::size_t, 2>>(input);
  ASSERT_FALSE(forward);
  EXPECT_TRUE(forward.error().is<StaticVectorSizeException>());
  EXPECT_EQ(visited, (std::vector<std::size_t>{0, 1, 2}));

  visited.clear();
  auto reverse = Star::slice<StaticList<std::size_t, 2>>(input, std::nullopt, std::nullopt, -1);
  ASSERT_FALSE(reverse);
  EXPECT_TRUE(reverse.error().is<StaticVectorSizeException>());
  EXPECT_EQ(visited, (std::vector<std::size_t>{4, 3, 2}));

  // Full-width distances must not wrap the computed result size to zero.
  visited.clear();
  IndexedInput largest{std::numeric_limits<std::size_t>::max(), visited};
  auto largest_forward = Star::slice<StaticList<std::size_t, 2>>(largest);
  ASSERT_FALSE(largest_forward);
  EXPECT_TRUE(largest_forward.error().is<StaticVectorSizeException>());
  EXPECT_EQ(visited, (std::vector<std::size_t>{0, 1, 2}));
  visited.clear();
  auto largest_reverse = Star::slice<StaticList<std::size_t, 2>>(largest, std::nullopt, std::nullopt, -1);
  ASSERT_FALSE(largest_reverse);
  EXPECT_TRUE(largest_reverse.error().is<StaticVectorSizeException>());
  auto maximum = std::numeric_limits<std::size_t>::max();
  EXPECT_EQ(visited, (std::vector<std::size_t>{maximum - 1, maximum - 2, maximum - 3}));
}

TEST(ListSlice, ConcreteFamiliesAndDifferentResultTypesOwnTheirElements) {
  SmallList<int, 2> small{0, 1, 2, 3};
  EXPECT_EQ(small.slice(std::nullopt, std::nullopt, -2), (SmallList<int, 2>{3, 1}));
  Deque<int> deque{0, 1, 2, 3};
  EXPECT_EQ(deque.slice(std::nullopt, std::nullopt, -2), (Deque<int>{3, 1}));
  auto bounded = StaticList<int, 4>::create({0, 1, 2, 3});
  ASSERT_TRUE(bounded);
  auto reversed = bounded->slice(std::nullopt, std::nullopt, -2);
  ASSERT_TRUE(reversed);
  EXPECT_EQ(reversed->size(), 2u);
  EXPECT_EQ((*reversed)[0], 3);
  EXPECT_EQ((*reversed)[1], 1);
  EXPECT_EQ((*bounded)[0], 0);

  std::array<int, 4> source{0, 1, 2, 3};
  auto result = Star::slice<List<int>>(source, std::nullopt, std::nullopt, -2);
  source[3] = 99;
  EXPECT_EQ(result, (List<int>{3, 1}));
  List<bool> flags{false, true, false, true};
  EXPECT_EQ(flags.slice(1, 3), (List<bool>{true, false}));
  EXPECT_EQ(flags.slice(std::nullopt, std::nullopt, -2), (List<bool>{true, true}));
  EXPECT_EQ(Star::slice(std::u32string(U"abcde"), std::nullopt, std::nullopt, -2), U"eca");
}

TEST(ListSlice, CopiesFollowSliceOrderAndSurviveSourceDestruction) {
  std::vector<int> copies;
  auto result = [&] {
    List<OwningValue> source;
    source.reserve(4);
    for (int i = 0; i != 4; ++i)
      source.emplace_back(i, copies);
    {
      auto contiguous = source.slice(1, 3);
      EXPECT_EQ(copies, (std::vector<int>{1, 2}));
      EXPECT_EQ(contiguous.size(), 2u);
      copies.clear();
    }
    auto sliced = source.slice(3, std::nullopt, -2);
    EXPECT_EQ(copies, (std::vector<int>{3, 1}));
    EXPECT_EQ(*source[3].value, 3);
    EXPECT_EQ(*source[1].value, 1);
    *source[3].value = 99;
    return sliced;
  }();
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(*result[0].value, 3);
  EXPECT_EQ(*result[1].value, 1);
}
