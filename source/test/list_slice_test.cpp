import std;

#include "gtest/gtest.h"

import star.format;
import star.list;
import star.pythonic;
import star.static_vector;

using namespace Star;

namespace {

template <typename Result, typename Input>
concept can_slice = requires(Input const& input) { Star::slice<Result, Input>(input, SliceNil); };

struct missing_index {
  std::size_t size() const;
};

struct signed_size {
  int size() const;
  int operator[](std::size_t) const;
};

struct boolean_append {
  bool push_back(int);
};

struct wrong_error_append {
  std::expected<void, int> push_back(int);
};

struct consuming_append {
  void push_back(std::unique_ptr<int>&&);
};

struct nondefault_result {
  nondefault_result() = delete;
  void push_back(int);
};

static_assert(can_slice<std::vector<int>, std::array<int, 3>>);
static_assert(can_slice<StaticList<int, 2>, std::array<int, 3>>);
static_assert(!can_slice<std::vector<int>, missing_index>);
static_assert(!can_slice<std::vector<int>, signed_size>);
static_assert(!can_slice<boolean_append, std::array<int, 3>>);
static_assert(!can_slice<wrong_error_append, std::array<int, 3>>);
static_assert(!can_slice<consuming_append, std::array<std::unique_ptr<int>, 3>>);
static_assert(!can_slice<std::vector<std::unique_ptr<int>>, std::array<std::unique_ptr<int>, 3>>);
static_assert(!can_slice<nondefault_result, std::array<int, 3>>);
static_assert(std::same_as<decltype(Star::slice(std::declval<List<int> const&>(), SliceNil)), List<int>>);
static_assert(std::same_as<decltype(Star::slice(std::declval<StaticList<int, 2> const&>(), SliceNil)),
                           std::expected<StaticList<int, 2>, StarException>>);

struct indexed_input {
  std::size_t length;
  std::vector<std::size_t>& visited;

  std::size_t size() const { return length; }
  std::size_t operator[](std::size_t index) const {
    EXPECT_LT(index, length);
    visited.push_back(index);
    return index;
  }
};

struct owning_value {
  owning_value(int v, std::vector<int>& copies) : value(std::make_unique<int>(v)), copies(&copies) {}
  owning_value(owning_value const& other) : value(std::make_unique<int>(*other.value)), copies(other.copies) {
    copies->push_back(*value);
  }
  owning_value(owning_value&&) = default;
  owning_value& operator=(owning_value const&) = delete;
  owning_value& operator=(owning_value&&) = delete;

  std::unique_ptr<int> value;
  std::vector<int>* copies;
};

}// namespace

TEST(ListSlice, PositiveBoundsPreserveLegacyClamping) {
  List<int> values{0, 1, 2, 3, 4};
  EXPECT_EQ(values.slice(), (List<int>{0, 1, 2, 3, 4}));
  EXPECT_EQ(values.slice(1, 5, 2), (List<int>{1, 3}));
  EXPECT_EQ(values.slice(-4, -1), (List<int>{1, 2, 3}));
  EXPECT_EQ(values.slice(-5, SliceNil), values);
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
  EXPECT_EQ(values.slice(SliceNil, SliceNil, -1), (List<int>{4, 3, 2, 1, 0}));
  EXPECT_EQ(values.slice(SliceNil, SliceNil, -2), (List<int>{4, 2, 0}));
  EXPECT_EQ(values.slice(4, 1, -1), (List<int>{4, 3, 2}));
  EXPECT_EQ(values.slice(-2, -4, -1), (List<int>{3, 2}));
  EXPECT_EQ(values.slice(99, SliceNil, -2), (List<int>{4, 2, 0}));
  EXPECT_EQ(values.slice(5, -5, -1), (List<int>{4, 3, 2, 1, 0}));
  EXPECT_EQ(values.slice(-5, SliceNil, -1), (List<int>{0}));
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
  EXPECT_EQ(values.slice(low, SliceNil, -1), (List<int>{0}));
  EXPECT_EQ(values.slice(SliceNil, SliceNil, low), (List<int>{4}));
  EXPECT_EQ(values.slice(SliceNil, SliceNil, high), (List<int>{0}));
  EXPECT_EQ(values.slice(3, 0, low), (List<int>{3}));
}

TEST(ListSlice, LargeVirtualSequencesKeepFullWidthIndexes) {
  std::vector<std::size_t> visited;
  auto high = static_cast<std::size_t>(std::numeric_limits<int>::max());
  indexed_input wide{high + 3, visited};
  auto forward = Star::slice<std::vector<std::size_t>>(wide, SliceNil, SliceNil, std::numeric_limits<int>::max());
  EXPECT_EQ(forward, (std::vector<std::size_t>{0, high}));
  EXPECT_EQ(visited, forward);

  visited.clear();
  auto reverse = Star::slice<std::vector<std::size_t>>(wide, SliceNil, SliceNil, std::numeric_limits<int>::min());
  EXPECT_EQ(reverse, (std::vector<std::size_t>{high + 2, 1}));
  EXPECT_EQ(visited, reverse);

  visited.clear();
  constexpr auto maximum = std::numeric_limits<std::size_t>::max();
  indexed_input largest{maximum, visited};
  auto tail = Star::slice<std::vector<std::size_t>>(largest, -3);
  EXPECT_EQ(tail, (std::vector<std::size_t>{maximum - 3, maximum - 2, maximum - 1}));
  EXPECT_EQ(visited, tail);

  visited.clear();
  auto reverseTail = Star::slice<std::vector<std::size_t>>(largest, -1, -4, -1);
  EXPECT_EQ(reverseTail, (std::vector<std::size_t>{maximum - 1, maximum - 2, maximum - 3}));
  EXPECT_EQ(visited, reverseTail);
}

TEST(ListSlice, EmptyAndZeroStrideNeverIndexInput) {
  std::vector<std::size_t> visited;
  indexed_input empty{0, visited};
  EXPECT_TRUE(Star::slice<std::vector<std::size_t>>(empty, SliceNil).empty());
  EXPECT_TRUE(Star::slice<std::vector<std::size_t>>(empty, SliceNil, SliceNil, -1).empty());
  EXPECT_TRUE(Star::slice<std::vector<std::size_t>>(empty, -1, 1, 0).empty());
  indexed_input values{5, visited};
  EXPECT_TRUE(Star::slice<std::vector<std::size_t>>(values, SliceNil, SliceNil, 0).empty());
  EXPECT_TRUE(
    Star::slice<std::vector<std::size_t>>(values, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0)
      .empty());
  auto bounded = Star::slice<StaticList<std::size_t, 0>>(values, SliceNil, SliceNil, 0);
  ASSERT_TRUE(bounded);
  EXPECT_TRUE(bounded->empty());
  EXPECT_TRUE(visited.empty());
}

TEST(ListSlice, BoundedOverflowRetainsOrderedPrefixAndFirstError) {
  std::vector<std::size_t> visited;
  indexed_input input{5, visited};
  auto forward = Star::slice<StaticList<std::size_t, 2>>(input, SliceNil);
  ASSERT_FALSE(forward);
  EXPECT_TRUE(forward.error().is<StaticVectorSizeException>());
  EXPECT_EQ(visited, (std::vector<std::size_t>{0, 1, 2}));

  visited.clear();
  auto reverse = Star::slice<StaticList<std::size_t, 2>>(input, SliceNil, SliceNil, -1);
  ASSERT_FALSE(reverse);
  EXPECT_TRUE(reverse.error().is<StaticVectorSizeException>());
  EXPECT_EQ(visited, (std::vector<std::size_t>{4, 3, 2}));

  // Full-width distances must not wrap the computed result size to zero.
  visited.clear();
  indexed_input largest{std::numeric_limits<std::size_t>::max(), visited};
  auto largestForward = Star::slice<StaticList<std::size_t, 2>>(largest, SliceNil);
  ASSERT_FALSE(largestForward);
  EXPECT_TRUE(largestForward.error().is<StaticVectorSizeException>());
  EXPECT_EQ(visited, (std::vector<std::size_t>{0, 1, 2}));
  visited.clear();
  auto largestReverse = Star::slice<StaticList<std::size_t, 2>>(largest, SliceNil, SliceNil, -1);
  ASSERT_FALSE(largestReverse);
  EXPECT_TRUE(largestReverse.error().is<StaticVectorSizeException>());
  auto maximum = std::numeric_limits<std::size_t>::max();
  EXPECT_EQ(visited, (std::vector<std::size_t>{maximum - 1, maximum - 2, maximum - 3}));
}

TEST(ListSlice, ConcreteFamiliesAndDifferentResultTypesOwnTheirElements) {
  SmallList<int, 2> small{0, 1, 2, 3};
  EXPECT_EQ(small.slice(SliceNil, SliceNil, -2), (SmallList<int, 2>{3, 1}));
  Deque<int> deque{0, 1, 2, 3};
  EXPECT_EQ(deque.slice(SliceNil, SliceNil, -2), (Deque<int>{3, 1}));
  auto bounded = StaticList<int, 4>::create({0, 1, 2, 3});
  ASSERT_TRUE(bounded);
  auto reversed = bounded->slice(SliceNil, SliceNil, -2);
  ASSERT_TRUE(reversed);
  EXPECT_EQ(reversed->size(), 2u);
  EXPECT_EQ((*reversed)[0], 3);
  EXPECT_EQ((*reversed)[1], 1);
  EXPECT_EQ((*bounded)[0], 0);

  std::array<int, 4> source{0, 1, 2, 3};
  auto result = Star::slice<List<int>>(source, SliceNil, SliceNil, -2);
  source[3] = 99;
  EXPECT_EQ(result, (List<int>{3, 1}));
  List<bool> flags{false, true, false, true};
  EXPECT_EQ(flags.slice(SliceNil, SliceNil, -2), (List<bool>{true, true}));
  EXPECT_EQ(Star::slice(std::u32string(U"abcde"), SliceNil, SliceNil, -2), U"eca");
}

TEST(ListSlice, CopiesFollowSliceOrderAndSurviveSourceDestruction) {
  std::vector<int> copies;
  auto result = [&] {
    std::vector<owning_value> source;
    source.reserve(4);
    for (int i = 0; i != 4; ++i)
      source.emplace_back(i, copies);
    auto sliced = Star::slice(source, 3, SliceNil, -2);
    EXPECT_EQ(copies, (std::vector<int>{3, 1}));
    EXPECT_EQ(*source[3].value, 3);
    EXPECT_EQ(*source[1].value, 1);
    EXPECT_NE(sliced[0].value.get(), source[3].value.get());
    *source[3].value = 99;
    return sliced;
  }();
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(*result[0].value, 3);
  EXPECT_EQ(*result[1].value, 1);
}
