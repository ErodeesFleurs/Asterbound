import std;
import star.format;
import star.list;
import star.string;
import star.lua;
import star.static_vector;

#include "gtest/gtest.h"

using namespace Star;

namespace {

template <typename Sequence>
concept mutable_checked_borrow = requires(Sequence& sequence) {
  sequence.first();
  sequence.last();
};

template <typename Sequence>
concept temporary_checked_borrow = requires(Sequence&& sequence) { std::move(sequence).first(); };

template <typename Sequence>
concept temporary_indexed_borrow = requires(Sequence&& sequence) {
  std::move(sequence).at(0);
  std::move(sequence).wrap(0);
};

template <typename Sequence>
concept checked_indexed_borrow = requires(Sequence& sequence) {
  sequence.at(0);
  sequence.wrap(0);
};

struct NotAnElement {};
struct NotAPredicate {
  void operator()(int) const {}
};
struct CheckedComparator {
  std::expected<bool, StarException> operator()(int, int) const { return true; }
};

template <typename Sequence>
concept accepts_invalid_emplace = requires(Sequence& sequence) { sequence.emplace_back(NotAnElement{}); };
template <typename Sequence>
concept accepts_void_filter = requires(Sequence& sequence) { sequence.filter(NotAPredicate{}); };
template <typename Sequence>
concept accepts_checked_comparator = requires(Sequence& sequence) { sequence.sort(CheckedComparator{}); };
template <typename T> struct SequenceAllocator {
  using value_type = T;
  int identity = 0;
  SequenceAllocator() = default;
  explicit SequenceAllocator(int value) : identity(value) {}
  template <typename U> SequenceAllocator(SequenceAllocator<U> const& other) : identity(other.identity) {}
  T* allocate(std::size_t size) { return std::allocator<T>{}.allocate(size); }
  void deallocate(T* pointer, std::size_t size) { std::allocator<T>{}.deallocate(pointer, size); }
  template <typename U> bool operator==(SequenceAllocator<U> const& other) const { return identity == other.identity; }
};

template <typename Sequence>
constexpr bool checked_borrow_contract =
  mutable_checked_borrow<Sequence> && !temporary_checked_borrow<Sequence> && !temporary_checked_borrow<Sequence const>;

static_assert(checked_borrow_contract<List<int>>);
static_assert(checked_borrow_contract<SmallList<int, 4>>);
static_assert(checked_borrow_contract<StaticList<int, 4>>);
static_assert(checked_borrow_contract<Deque<int>>);
static_assert(checked_borrow_contract<LinkedList<int>>);
static_assert(!temporary_indexed_borrow<List<int>>);
static_assert(!temporary_indexed_borrow<SmallList<int, 4>>);
static_assert(!temporary_indexed_borrow<StaticList<int, 4>>);
static_assert(!temporary_indexed_borrow<Deque<int>>);
static_assert(!mutable_checked_borrow<List<bool>>);
static_assert(!checked_indexed_borrow<List<bool>>);
static_assert(mutable_checked_borrow<Deque<bool>>);
static_assert(!accepts_invalid_emplace<List<int>>);
static_assert(!accepts_invalid_emplace<SmallList<int, 4>>);
static_assert(!accepts_invalid_emplace<StaticList<int, 4>>);
static_assert(!accepts_invalid_emplace<Deque<int>>);
static_assert(!accepts_invalid_emplace<LinkedList<int>>);
static_assert(!accepts_void_filter<List<int>>);
static_assert(!accepts_checked_comparator<List<int>>);
static_assert(!accepts_checked_comparator<LinkedList<int>>);
static_assert(std::same_as<decltype(std::declval<StaticList<int, 4>&>().emplace_back(1)),
                           std::expected<std::reference_wrapper<int>, StarException>>);
static_assert(
  std::same_as<decltype(std::declval<StringList const&>().filtered([](String const&) { return true; })), StringList>);
static_assert(std::same_as<decltype(std::declval<StringList const&>().sorted()), StringList>);
static_assert(
  std::same_as<decltype(std::declval<LuaVariadic<int> const&>().filtered([](int) { return true; })), LuaVariadic<int>>);
static_assert([] {
  List<int> sequence;
  return sequence.empty() && sequence.size() == 0;
}());

template <typename Sequence> void expect_set_overflow_contract() {
  Sequence sequence;
  auto added = sequence.set(2, 7);
  ASSERT_TRUE(added);
  EXPECT_EQ(sequence.size(), 3u);
  EXPECT_EQ(sequence[0], 0);
  EXPECT_EQ(sequence[1], 0);
  EXPECT_EQ(sequence[2], 7);
  auto failed = sequence.set(std::numeric_limits<std::size_t>::max(), 9);
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().template is<OutOfRangeException>());
  EXPECT_EQ(sequence.size(), 3u);
  EXPECT_EQ(sequence[2], 7);
}

template <typename Sequence> void expect_range_and_callback_contract() {
  Sequence sequence;
  auto source = std::views::iota(0, 4) | std::views::transform([](int value) { return value + 1; });
  if constexpr (std::is_void_v<decltype(sequence.append_range(source))>) {
    sequence.append_range(source);
  } else {
    ASSERT_TRUE(sequence.append_range(source));
  }
  auto predicate = [state = std::make_unique<int>(0)](int value) mutable {
    ++*state;
    return value % 2 == 0;
  };
  sequence.filter(predicate);
  EXPECT_EQ(std::vector<int>(sequence.begin(), sequence.end()), (std::vector<int>{2, 4}));
  std::vector<int> visited;
  auto status = sequence.exec([&](int& value) -> std::expected<void, StarException> {
    visited.push_back(value);
    if (value == 4)
      return std::unexpected(OutOfRangeException("stop", false));
    value *= 10;
    return {};
  });
  ASSERT_FALSE(status);
  EXPECT_EQ(visited, (std::vector<int>{2, 4}));
  EXPECT_EQ(std::vector<int>(sequence.begin(), sequence.end()), (std::vector<int>{20, 4}));
}

}// namespace

TEST(ListContract, SetRejectsOverflowWithoutMutation) {
  expect_set_overflow_contract<List<int>>();
  expect_set_overflow_contract<SmallList<int, 4>>();
  expect_set_overflow_contract<StaticList<int, 4>>();
  expect_set_overflow_contract<Deque<int>>();
}

TEST(ListContract, BoundedSetRetainsCapacityErrorWithoutMutation) {
  auto values = StaticList<int, 2>::create({1, 2});
  ASSERT_TRUE(values);
  auto rejected = values->set(2, 9);
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<StaticVectorSizeException>());
  EXPECT_EQ(values->size(), 2u);
  EXPECT_EQ((*values)[0], 1);
  EXPECT_EQ((*values)[1], 2);
}

TEST(ListContract, AllFamiliesSupportPrvaluesAndBorrowMoveOnlyCallbacks) {
  expect_range_and_callback_contract<List<int>>();
  expect_range_and_callback_contract<SmallList<int, 4>>();
  expect_range_and_callback_contract<StaticList<int, 4>>();
  expect_range_and_callback_contract<Deque<int>>();
  expect_range_and_callback_contract<LinkedList<int>>();
}

TEST(ListContract, SizedRangeAndSmallBatchesPreserveOrderWithoutReevaluation) {
  int dereferences = 0;
  auto source = std::views::iota(0, 12) | std::views::transform([&](int value) {
                  ++dereferences;
                  return value;
                });
  List<int> sequence(std::from_range, source);
  EXPECT_EQ(dereferences, 12);
  EXPECT_EQ(sequence.size(), 12u);
  for (int value = 12; value != 112; ++value) {
    std::array<int, 1> batch{value};
    sequence.append_range(batch);
  }
  EXPECT_TRUE(std::ranges::equal(sequence, std::views::iota(0, 112)));
}

TEST(ListContract, SinglePassRangesAndDistinctSentinelsAreConsumedOnce) {
  std::istringstream input("1 2 3 4");
  auto source = std::ranges::istream_view<int>(input);
  List<int> sequence(source.begin(), std::default_sentinel);
  EXPECT_EQ(sequence, (List<int>{1, 2, 3, 4}));
  std::istringstream more("5 6");
  sequence.append_range(std::ranges::istream_view<int>(more));
  EXPECT_EQ(sequence, (List<int>{1, 2, 3, 4, 5, 6}));
  std::istringstream front("-1 0");
  Deque<int> deque{1, 2};
  deque.prepend_range(std::ranges::istream_view<int>(front));
  EXPECT_EQ(deque, (Deque<int>{-1, 0, 1, 2}));
}

TEST(ListContract, AppendDoesNotEvaluateBorrowedProjectionForAliasDetection) {
  List<int> input{1, 2, 3};
  List<int> destination{99};
  std::vector<int> visited;
  auto source = input | std::views::transform([&](int& value) -> int& {
                  visited.push_back(value);
                  return value;
                });
  destination.append_range(source);
  EXPECT_EQ(destination, (List<int>{99, 1, 2, 3}));
  EXPECT_EQ(visited, (std::vector<int>{1, 2, 3}));

  visited.clear();
  auto overlap = input | std::views::transform([&](int& value) -> int& {
                   visited.push_back(value);
                   return value;
                 });
  input.append_range(overlap);
  EXPECT_EQ(input, (List<int>{1, 2, 3, 1, 2, 3}));
  EXPECT_EQ(visited, (std::vector<int>{1, 2, 3}));
}

TEST(ListContract, AppendAndInsertSnapshotSelfAndBorrowedOverlap) {
  List<int> sequence{1, 2, 3};
  sequence.shrink_to_fit();
  sequence.append_range(sequence);
  EXPECT_EQ(sequence, (List<int>{1, 2, 3, 1, 2, 3}));
  sequence.shrink_to_fit();
  auto overlap = std::span(sequence.data() + 1, 3);
  sequence.append_range(overlap);
  EXPECT_EQ(sequence, (List<int>{1, 2, 3, 1, 2, 3, 2, 3, 1}));
  List<int> inserted{1, 2, 3};
  inserted.insert_range_at(1, inserted);
  EXPECT_EQ(inserted, (List<int>{1, 1, 2, 3, 2, 3}));
  Deque<int> deque{1, 2};
  deque.append_range(deque);
  EXPECT_EQ(deque, (Deque<int>{1, 2, 1, 2}));
  deque.prepend_range(deque);
  EXPECT_EQ(deque, (Deque<int>{1, 2, 1, 2, 1, 2, 1, 2}));
  LinkedList<int> linked{1, 2};
  auto borrowed = std::ranges::subrange(linked.begin(), linked.end());
  linked.append_range(borrowed);
  EXPECT_EQ(std::vector<int>(linked.begin(), linked.end()), (std::vector<int>{1, 2, 1, 2}));
}

TEST(ListContract, BoolProxyValuesRemainUsableWithoutReferenceBorrowing) {
  bool seed = false;
  List<bool> copied(2, seed);
  copied.push_back(seed);
  copied.insert(copied.cbegin(), true);
  EXPECT_EQ(copied, (List<bool>{true, false, false, false}));
  EXPECT_EQ(copied.index_of(seed), 1u);
  EXPECT_EQ(copied.remove(seed), 3u);
  EXPECT_EQ(copied, (List<bool>{true}));

  List<bool> bits{true, false};
  bits.append_range(bits);
  EXPECT_EQ(bits, (List<bool>{true, false, true, false}));
  auto source = bits | std::views::drop(1) | std::views::take(2);
  bits.append_range(source);
  EXPECT_EQ(bits, (List<bool>{true, false, true, false, false, true}));
  bits.transform([](bool value) { return !value; });
  EXPECT_EQ(bits.front_value(), std::optional<bool>(false));
  ASSERT_TRUE(bits.set(0, true));
  auto taken = bits.take_at(0);
  ASSERT_TRUE(taken);
  EXPECT_TRUE(*taken);
  EXPECT_TRUE(bits.contains(false));
}

TEST(ListContract, BoundedAppendRetainsPrefixAndDoesNotConsumeRejectedElement) {
  StaticList<std::unique_ptr<int>, 2> sequence;
  List<std::unique_ptr<int>> source;
  source.emplace_back(std::make_unique<int>(1));
  source.emplace_back(std::make_unique<int>(2));
  source.emplace_back(std::make_unique<int>(3));
  auto appended = sequence.append_range(std::move(source));
  ASSERT_FALSE(appended);
  EXPECT_TRUE(appended.error().is<StaticVectorSizeException>());
  ASSERT_EQ(sequence.size(), 2u);
  EXPECT_EQ(*sequence[0], 1);
  EXPECT_EQ(*sequence[1], 2);
  EXPECT_FALSE(source[0]);
  EXPECT_FALSE(source[1]);
  ASSERT_TRUE(source[2]);
  EXPECT_EQ(*source[2], 3);
  auto rejected = std::make_unique<int>(4);
  auto emplaced = sequence.emplace_back(std::move(rejected));
  ASSERT_FALSE(emplaced);
  ASSERT_TRUE(rejected);
  EXPECT_EQ(*rejected, 4);
}

TEST(ListContract, SmallAndBoundedStorageSnapshotBorrowedSources) {
  SmallList<int, 2> small{1, 2};
  small.append_range(std::span(small.data(), small.size()));
  EXPECT_EQ(std::vector<int>(small.begin(), small.end()), (std::vector<int>{1, 2, 1, 2}));
  auto bounded = StaticList<int, 5>::create({1, 2, 3});
  ASSERT_TRUE(bounded);
  auto appended = bounded->append_range(*bounded);
  ASSERT_FALSE(appended);
  EXPECT_EQ(std::vector<int>(bounded->begin(), bounded->end()), (std::vector<int>{1, 2, 3, 1, 2}));

  List<std::unique_ptr<int>> source;
  source.emplace_back(std::make_unique<int>(1));
  source.emplace_back(std::make_unique<int>(2));
  source.emplace_back(std::make_unique<int>(3));
  auto rejected = StaticList<std::unique_ptr<int>, 2>::create(std::from_range, std::move(source));
  ASSERT_FALSE(rejected);
  ASSERT_TRUE(source[0]);
  ASSERT_TRUE(source[1]);
  ASSERT_TRUE(source[2]);
}

TEST(ListContract, RangeOperationsRetainStatefulAllocatorOwnership) {
  using Allocator = SequenceAllocator<int>;
  using Vector = std::vector<int, Allocator>;
  List<int, Allocator> sequence(Vector({1, 2}, Allocator(7)));
  sequence.append_range(sequence);
  EXPECT_EQ(sequence.get_allocator().identity, 7);
  EXPECT_EQ(std::vector<int>(sequence.begin(), sequence.end()), (std::vector<int>{1, 2, 1, 2}));

  using Nodes = std::list<int, Allocator>;
  LinkedList<int, Allocator> destination(Nodes({1, 2}, Allocator(7)));
  LinkedList<int, Allocator> incoming(Nodes({3, 4}, Allocator(9)));
  destination.append_range(std::move(incoming));
  EXPECT_EQ(destination.get_allocator().identity, 7);
  EXPECT_EQ(std::vector<int>(destination.begin(), destination.end()), (std::vector<int>{1, 2, 3, 4}));
  LinkedList<int, Allocator> prefix(Nodes({-1, 0}, Allocator(11)));
  destination.prepend_range(std::move(prefix));
  EXPECT_EQ(destination.get_allocator().identity, 7);
  EXPECT_EQ(std::vector<int>(destination.begin(), destination.end()), (std::vector<int>{-1, 0, 1, 2, 3, 4}));
}

TEST(ListContract, NontrivialAppendConstructorsObserveOrderedDestinationGrowth) {
  struct observed_value {
    observed_value(int value, std::function<std::size_t()> const& size) : value(value), size(&size) {}
    observed_value(observed_value const& other)
        : value(other.value + static_cast<int>((*other.size)())), size(other.size) {}
    observed_value(observed_value&&) noexcept = default;
    observed_value& operator=(observed_value const&) = default;
    observed_value& operator=(observed_value&&) = default;
    int value;
    std::function<std::size_t()> const* size;
  };

  List<observed_value> destination;
  std::function<std::size_t()> size = [&] { return destination.size(); };
  std::array<observed_value, 3> source{observed_value(10, size), observed_value(20, size), observed_value(30, size)};
  destination.append_range(source);
  ASSERT_EQ(destination.size(), 3u);
  EXPECT_EQ(destination[0].value, 10);
  EXPECT_EQ(destination[1].value, 21);
  EXPECT_EQ(destination[2].value, 32);
  EXPECT_EQ(source[0].value, 10);
  EXPECT_EQ(source[1].value, 20);
  EXPECT_EQ(source[2].value, 30);
}
