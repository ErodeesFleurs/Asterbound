import std;

#include "gtest/gtest.h"

import star.array;
import star.format;

using namespace Star;

namespace {

struct non_default {
  non_default() = delete;
  explicit non_default(int v) : value(v) {}
  int value;
};

struct unassignable {
  unassignable() = default;
  unassignable(unassignable const&) = default;
  unassignable& operator=(unassignable const&) = delete;
};

struct explicit_integer {
  explicit_integer() = default;
  explicit_integer& operator=(int) { return *this; }
  explicit explicit_integer(int) {}
};

struct unrelated {};

template <typename Destination, typename Source>
concept cross_assignable = requires(Destination& destination, Source const& source) {
  { destination = source } -> std::same_as<Destination&>;
};

static_assert(std::default_initializable<Array<int, 4>>);
static_assert(std::default_initializable<Array<non_default, 0>>);
static_assert(!std::default_initializable<Array<non_default, 1>>);
static_assert(std::constructible_from<Array<int, 2>, double, double>);
static_assert(!std::constructible_from<Array<int, 2>, int>);
static_assert(!std::constructible_from<Array<int, 2>, int, int, int>);
static_assert(!std::constructible_from<Array<int, 0>, int>);
static_assert(!std::constructible_from<Array<int, 2>, int, unrelated>);
static_assert(!std::constructible_from<Array<unassignable, 1>, unassignable const&>);
static_assert(!std::constructible_from<Array<non_default, 1>, non_default const&>);
static_assert(!std::constructible_from<Array<explicit_integer, 1>, int>);
static_assert(std::constructible_from<Array<int, 2>, Array<short, 2> const&>);
static_assert(!std::constructible_from<Array<int, 2>, Array<unrelated, 2> const&>);
static_assert(cross_assignable<Array<int, 2>, Array<short, 2>>);
static_assert(!cross_assignable<Array<int, 2>, Array<unrelated, 2>>);
static_assert(!cross_assignable<Array<unassignable, 2>, Array<unassignable, 2>>);
static_assert(std::default_initializable<Array<std::unique_ptr<int>, 2>>);
static_assert(std::movable<Array<std::unique_ptr<int>, 2>>);
static_assert(!std::copy_constructible<Array<std::unique_ptr<int>, 2>>);
static_assert(!std::is_copy_assignable_v<Array<std::unique_ptr<int>, 2>>);
static_assert(std::copyable<Array<int, 2>>);
static_assert(std::same_as<decltype(std::declval<Array<int, 2>&>().get<0>()), int&>);
static_assert(std::same_as<decltype(std::declval<Array<int, 2> const&>().get<0>()), int const&>);
static_assert(std::same_as<decltype(std::declval<Array<int, 2>&>().data()), int*>);
static_assert(std::same_as<decltype(std::declval<Array<int, 2> const&>().data()), int const*>);
static_assert(std::ranges::contiguous_range<Array<int, 4>>);
static_assert(std::ranges::contiguous_range<Array<int, 4> const>);
static_assert(std::ranges::contiguous_range<Array<int, 0>>);
static_assert(!std::is_base_of_v<std::array<int, 4>, Array<int, 4>>);
static_assert(!std::is_convertible_v<Array<int, 4>&, std::array<int, 4>&>);
static_assert(!std::is_convertible_v<Array<int, 4> const&, std::array<int, 4> const&>);

constexpr bool constexpr_default_and_access() {
  Array<int, 4> values;
  for (auto value : values)
    if (value != 0)
      return false;
  values.get<1>() = 17;
  values.data()[3] = 29;
  auto const& immutable = values;
  return immutable.get<1>() == 17 && immutable.data()[3] == 29 && values.data() == &values.get<0>();
}

static_assert(constexpr_default_and_access());
static_assert(Array<int, 0>{}.empty());

constexpr bool constexpr_container_mutation() {
  Array<int, 4> values;
  values.fill(2);
  values.front() = 5;
  values.back() = 8;
  values[1] = 6;
  values.data()[2] = 7;
  for (auto& value : values)
    ++value;
  for (auto it = values.rbegin(); it != values.rend(); ++it)
    *it *= 2;
  Array<int, 4> replacement;
  replacement.fill(3);
  values.swap(replacement);
  auto const& immutable = replacement;
  return values.front() == 3 && values.back() == 3 && immutable.front() == 12 && immutable.back() == 18
    && *immutable.cbegin() == 12 && *immutable.crbegin() == 18 && immutable.data()[1] == 14 && immutable[2] == 16
    && immutable.cend() - immutable.cbegin() == 4;
}

static_assert(constexpr_container_mutation());

struct argument {
  int value = 0;
};

struct ordered_assignment {
  static inline std::vector<std::pair<char, int>> events;
  static inline int next_slot = 0;

  ordered_assignment() : slot(next_slot++) { events.emplace_back('D', slot); }
  ordered_assignment(argument const& source) : value(source.value) { events.emplace_back('C', value); }
  ordered_assignment& operator=(ordered_assignment const& source) {
    value = source.value;
    events.emplace_back('E', slot * 100 + value);
    return *this;
  }
  ordered_assignment& operator=(argument const& source) {
    value = source.value;
    events.emplace_back('A', slot * 100 + value);
    return *this;
  }

  static void reset() {
    events.clear();
    next_slot = 0;
  }

  int value = 0;
  int slot = -1;
};

}// namespace

TEST(ArrayConstruction, DefaultValueInitializesEveryScalarSlot) {
  Array<int, 64> integers;
  Array<double, 16> floating;
  Array<int*, 4> pointers;
  for (auto value : integers)
    EXPECT_EQ(value, 0);
  for (auto value : floating)
    EXPECT_EQ(value, 0.0);
  for (auto value : pointers)
    EXPECT_EQ(value, nullptr);
}

TEST(ArrayConstruction, ConvertsFirstArgumentThenDefaultsAndAssignsInOrder) {
  ordered_assignment::reset();
  Array<ordered_assignment, 3> values{argument{7}, argument{8}, argument{9}};
  EXPECT_EQ(
    ordered_assignment::events,
    (std::vector<std::pair<char, int>>{{'C', 7}, {'D', 0}, {'D', 1}, {'D', 2}, {'E', 7}, {'A', 108}, {'A', 209}}));
  EXPECT_EQ(values[0].value, 7);
  EXPECT_EQ(values[1].value, 8);
  EXPECT_EQ(values[2].value, 9);
}

TEST(ArrayConstruction, FilledDefaultsThenAssignsEverySlot) {
  ordered_assignment source(argument{13});
  ordered_assignment::reset();
  auto values = Array<ordered_assignment, 3>::filled(source);
  EXPECT_EQ(ordered_assignment::events,
            (std::vector<std::pair<char, int>>{{'D', 0}, {'D', 1}, {'D', 2}, {'E', 13}, {'E', 113}, {'E', 213}}));
  for (auto const& value : values)
    EXPECT_EQ(value.value, 13);
}

TEST(ArrayConstruction, IteratorCopyClampsCountAndValueInitializesTail) {
  std::array<int, 5> source{7, 8, 9, 10, 11};
  auto partial = Array<int, 4>::copy_from(source.begin(), 2);
  auto clamped = Array<int, 3>::copy_from(source.begin(), source.size());
  auto none = Array<int, 2>::copy_from(source.begin(), 0);
  EXPECT_EQ(partial, (Array<int, 4>{7, 8, 0, 0}));
  EXPECT_EQ(clamped, (Array<int, 3>{7, 8, 9}));
  EXPECT_EQ(none, (Array<int, 2>{0, 0}));
  partial[0] = 99;
  EXPECT_EQ(source[0], 7);
}

TEST(ArrayConstruction, RetainsAssignmentCompatibleNarrowing) {
  Array<int, 3> integers(3.75, -2.75, 9.125);
  EXPECT_EQ(integers[0], 3);
  EXPECT_EQ(integers[1], -2);
  EXPECT_EQ(integers[2], 9);
  Array<float, 2> floating{1.25, 2.5};
  EXPECT_EQ(floating[0], 1.25f);
  EXPECT_EQ(floating[1], 2.5f);
}

TEST(ArrayConstruction, CrossTypeDefaultsThenAssignsRatherThanConverts) {
  Array<argument, 3> source{argument{4}, argument{5}, argument{6}};
  ordered_assignment::reset();
  Array<ordered_assignment, 3> values(source);
  EXPECT_EQ(ordered_assignment::events,
            (std::vector<std::pair<char, int>>{{'D', 0}, {'D', 1}, {'D', 2}, {'A', 4}, {'A', 105}, {'A', 206}}));
  EXPECT_EQ(values[0].value, 4);
  EXPECT_EQ(values[1].value, 5);
  EXPECT_EQ(values[2].value, 6);
}

TEST(ArrayAssignment, CrossTypeMutatesExistingSlotsInOrderAndReturnsSelf) {
  Array<ordered_assignment, 3> destination;
  Array<argument, 3> source{argument{1}, argument{2}, argument{3}};
  ordered_assignment::events.clear();
  auto* identity = &(destination = source);
  EXPECT_EQ(identity, &destination);
  ASSERT_EQ(ordered_assignment::events.size(), 3u);
  for (std::size_t i = 0; i < source.size(); ++i) {
    EXPECT_EQ(destination[i].value, source[i].value);
    EXPECT_EQ(ordered_assignment::events[i], (std::pair<char, int>{'A', destination[i].slot * 100 + source[i].value}));
  }
  Array<int, 3> integers;
  Array<double, 3> fractional{8.75, -4.5, 6.125};
  integers = fractional;
  EXPECT_EQ(integers, (Array<int, 3>{8, -4, 6}));
}

TEST(ArrayResize, TruncatesPrefixAndValueInitializesExpandedTail) {
  Array<int, 4> original{11, 22, 33, 44};
  auto shorter = original.to_size<2>();
  auto longer = original.to_size<7>();
  auto same = original.to_size<4>();
  EXPECT_EQ(shorter, (Array<int, 2>{11, 22}));
  EXPECT_EQ(longer, (Array<int, 7>{11, 22, 33, 44, 0, 0, 0}));
  EXPECT_EQ(same, original);
  shorter[0] = 99;
  longer[1] = 88;
  EXPECT_EQ(original, (Array<int, 4>{11, 22, 33, 44}));
}

TEST(ArrayAccess, CheckedBoundaryPreservesMutableAndConstReferences) {
  Array<int, 3> values{10, 20, 30};
  auto last = values.at(2);
  ASSERT_TRUE(last);
  EXPECT_EQ(&last->get(), &values[2]);
  last->get() = 47;
  EXPECT_EQ(values.get<2>(), 47);
  auto const& immutable = values;
  auto first = immutable.at(0);
  ASSERT_TRUE(first);
  static_assert(std::same_as<decltype(first), std::expected<std::reference_wrapper<int const>, StarException>>);
  EXPECT_EQ(&first->get(), values.data());
  auto boundary = values.at(values.size());
  auto far_boundary = immutable.at(std::numeric_limits<std::size_t>::max());
  ASSERT_FALSE(boundary);
  ASSERT_FALSE(far_boundary);
  EXPECT_TRUE(boundary.error().is<OutOfRangeException>());
  EXPECT_TRUE(far_boundary.error().is<OutOfRangeException>());
  EXPECT_EQ(values, (Array<int, 3>{10, 20, 47}));
}

TEST(ArrayContainer, RangesAccessAndSwapMutateTheSameOwnedElements) {
  Array<int, 4> values;
  values.fill(2);
  values.front() = 5;
  values.back() = 8;
  values[1] = 6;
  values.data()[2] = 7;
  int* identity = values.data();
  for (auto& value : values)
    ++value;
  for (auto it = values.rbegin(); it != values.rend(); ++it)
    *it *= 2;
  EXPECT_EQ(values, (Array<int, 4>{12, 14, 16, 18}));
  Array<int, 4> replacement;
  replacement.fill(3);
  values.swap(replacement);
  EXPECT_EQ(values.data(), identity);
  EXPECT_EQ(values, (Array<int, 4>{3, 3, 3, 3}));
  auto const& immutable = replacement;
  EXPECT_EQ(std::vector<int>(immutable.cbegin(), immutable.cend()), (std::vector<int>{12, 14, 16, 18}));
  EXPECT_EQ(std::vector<int>(immutable.crbegin(), immutable.crend()), (std::vector<int>{18, 16, 14, 12}));
  EXPECT_EQ(std::ranges::data(immutable), &immutable.front());
  EXPECT_EQ(immutable.max_size(), 4u);
}

TEST(ArrayZeroSize, HasNoElementsAndResizesWithoutReadingEmptyStorage) {
  Array<int, 0> empty;
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(empty.begin(), empty.end());
  EXPECT_EQ(empty.size(), 0u);
  EXPECT_EQ(empty.cbegin(), empty.cend());
  EXPECT_EQ(empty.rbegin(), empty.rend());
  EXPECT_EQ(empty.crbegin(), empty.crend());
  EXPECT_TRUE(std::ranges::empty(empty));
  empty.fill(9);
  Array<int, 0> other;
  empty.swap(other);
  EXPECT_TRUE(other.empty());
  auto checked = empty.at(0);
  ASSERT_FALSE(checked);
  EXPECT_TRUE(checked.error().is<OutOfRangeException>());
  EXPECT_EQ(empty.to_size<3>(), (Array<int, 3>{0, 0, 0}));
  EXPECT_TRUE((Array<int, 2>{4, 5}.to_size<0>().empty()));
  Array<non_default, 0> non_default_empty;
  auto copied = non_default_empty;
  auto moved = std::move(copied);
  EXPECT_TRUE(moved.empty());
}

TEST(ArrayValueSemantics, ScalarCopiesAreIndependentAndMovesTransferOwners) {
  Array<int, 2> source{12, 34};
  auto copied = source;
  copied[0] = 56;
  EXPECT_EQ(source[0], 12);
  Array<int, 2> assigned;
  assigned = source;
  assigned[1] = 78;
  EXPECT_EQ(source[1], 34);

  Array<std::unique_ptr<int>, 2> owners;
  EXPECT_FALSE(owners[0]);
  EXPECT_FALSE(owners[1]);
  owners[0] = std::make_unique<int>(41);
  owners[1] = std::make_unique<int>(42);
  auto* first_identity = owners[0].get();
  auto* second_identity = owners[1].get();
  auto moved = std::move(owners);
  EXPECT_FALSE(owners[0]);
  EXPECT_FALSE(owners[1]);
  EXPECT_EQ(moved[0].get(), first_identity);
  EXPECT_EQ(moved[1].get(), second_identity);
  Array<std::unique_ptr<int>, 2> destination;
  destination[0] = std::make_unique<int>(99);
  destination = std::move(moved);
  EXPECT_FALSE(moved[0]);
  EXPECT_FALSE(moved[1]);
  ASSERT_TRUE(destination[0]);
  ASSERT_TRUE(destination[1]);
  EXPECT_EQ(destination[0].get(), first_identity);
  EXPECT_EQ(destination[1].get(), second_identity);
  EXPECT_EQ(*destination[0], 41);
  EXPECT_EQ(*destination[1], 42);
}
