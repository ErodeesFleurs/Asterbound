import std;
import star.set;
import star.list;
import star.data_stream_devices;
import star.data_stream_extra;
import star.byte_array;

#include "gtest/gtest.h"

using namespace Star;

namespace {
struct StatefulOrder {
  int modulus = 100;
  bool descending = false;
  bool operator()(int a, int b) const {
    return descending ? a % modulus > b % modulus : a % modulus < b % modulus;
  }
};
struct StatefulHash {
  int modulus = 100;
  int salt = 0;
  std::size_t operator()(int value) const { return (value % modulus) ^ salt; }
};
struct StatefulEqual {
  int modulus = 100;
  bool operator()(int a, int b) const { return a % modulus == b % modulus; }
};
template <typename T> struct PolicyAllocator {
  using value_type = T;
  int identity = 0;
  PolicyAllocator() = default;
  explicit PolicyAllocator(int id) : identity(id) {}
  template <typename U> PolicyAllocator(PolicyAllocator<U> const& other) : identity(other.identity) {}
  PolicyAllocator select_on_container_copy_construction() const { return PolicyAllocator(identity + 100); }
  T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }
  void deallocate(T* data, std::size_t count) { std::allocator<T>{}.deallocate(data, count); }
  template <typename U> bool operator==(PolicyAllocator<U> const& other) const { return identity == other.identity; }
};
struct Record {
  int key;
  int revision;
  bool operator==(Record const&) const = default;
};
struct RecordLess {
  bool operator()(Record const& a, Record const& b) const { return a.key < b.key; }
};
struct RecordHash {
  std::size_t operator()(Record const& value) const { return value.key; }
};
struct RecordEqual {
  bool operator()(Record const& a, Record const& b) const { return a.key == b.key; }
};
struct ZeroHash {
  std::size_t operator()(int) const { return 0; }
};
struct ExplicitKey {
  int key;
  explicit ExplicitKey(int value) : key(value) {}
  auto operator<=>(ExplicitKey const&) const = default;
};
struct ExplicitHash {
  std::size_t operator()(ExplicitKey const& value) const { return value.key; }
};

template <typename S>
concept temporary_value_view = requires(S value) { std::move(value).value_view(); };
template <typename S>
concept temporary_find = requires(S value) { std::move(value).find(1); };
template <typename S>
concept can_insert_integer = requires(S& value) { value.insert_range(std::array{1, 2}); };
static_assert(!temporary_value_view<Set<int>> && !temporary_find<Set<int>>);
static_assert(!temporary_value_view<HashSet<int>> && !temporary_find<HashSet<int>>);
static_assert(!temporary_value_view<StableHashSet<int>> && !temporary_find<StableHashSet<int>>);
static_assert(!can_insert_integer<Set<std::unique_ptr<int>>>);
static_assert(!std::copy_constructible<Set<std::unique_ptr<int>>>);
static_assert(!std::copy_constructible<HashSet<std::unique_ptr<int>>>);
static_assert(!std::copy_constructible<StableHashSet<std::unique_ptr<int>>>);
static_assert(std::movable<Set<std::unique_ptr<int>>> && std::movable<HashSet<std::unique_ptr<int>>>
              && std::movable<StableHashSet<std::unique_ptr<int>>>);

template <typename S> void expect_single_pass_ranges() {
  std::istringstream stream("3 1 3 2");
  auto values = S::from(std::views::istream<int>(stream));
  ASSERT_EQ(values.size(), 3u);
  for (int key : {1, 2, 3}) EXPECT_TRUE(values.contains(key));
  std::istringstream more("4 2 5 4");
  values.insert_range(std::views::istream<int>(more));
  ASSERT_EQ(values.size(), 5u);
  for (int key : {1, 2, 3, 4, 5}) EXPECT_TRUE(values.contains(key));
  std::istringstream tagged("8 6 8 7");
  S from_range(std::from_range, std::views::istream<int>(tagged));
  ASSERT_EQ(from_range.size(), 3u);
  for (int key : {6, 7, 8}) EXPECT_TRUE(from_range.contains(key));
}

template <typename S> void expect_borrowed_bulk_operations() {
  S values{1, 2, 3, 4, 5};
  auto snapshot = values.values();
  values.insert_range(values.value_view());
  EXPECT_EQ(values.size(), 5u);
  auto first = std::next(values.cbegin());
  auto last = std::next(first, 2);
  std::array removed{*first, *std::next(first)};
  values.erase_all(std::ranges::subrange(first, last));
  ASSERT_EQ(values.size(), 3u);
  for (auto key : snapshot)
    EXPECT_EQ(values.contains(key), std::ranges::find(removed, key) == removed.end());
  values.erase_all(values.value_view());
  EXPECT_TRUE(values.empty());
  EXPECT_EQ(snapshot.size(), 5u);
  values.insert_range(snapshot);
  values.erase_all(values);
  EXPECT_TRUE(values.empty());
}

template <typename S> void expect_hash_result_policy() {
  S left(0, StatefulHash{10, 31}, StatefulEqual{10}, PolicyAllocator<int>(7));
  left.insert_range(std::array{1, 2, 3});
  S right(0, StatefulHash{10, 63}, StatefulEqual{10}, PolicyAllocator<int>(9));
  right.insert_range(std::array{11, 4});
  auto common = left.intersection(right);
  ASSERT_EQ(common.size(), 1u);
  EXPECT_EQ(*common.begin(), 11);
  EXPECT_TRUE(common.contains(1));
  auto remainder = left.difference(right);
  ASSERT_EQ(remainder.size(), 2u);
  EXPECT_TRUE(remainder.contains(2));
  EXPECT_TRUE(remainder.contains(3));
  auto combined = left.combination(right);
  ASSERT_EQ(combined.size(), 4u);
  for (auto const* result : {&common, &remainder, &combined}) {
    EXPECT_EQ(result->hash_function().modulus, 10);
    EXPECT_EQ(result->hash_function().salt, 31);
    EXPECT_EQ(result->key_eq().modulus, 10);
    EXPECT_EQ(result->get_allocator().identity, 7);
  }
  auto copy = left;
  EXPECT_EQ(copy.get_allocator().identity, 107);
  EXPECT_TRUE(copy.contains(11));
}

template <typename S> void expect_set_wire_prefix() {
  S decoded{std::uint8_t(0x7f)};
  auto read = DataStreamBuffer::deserialize(decoded, std::span<char const>("\x04\x10\x10\x20", 4));
  ASSERT_FALSE(read);
  ASSERT_EQ(decoded.size(), 2u);
  EXPECT_TRUE(decoded.contains(0x10));
  EXPECT_TRUE(decoded.contains(0x20));
  EXPECT_FALSE(decoded.contains(0x7f));
}
} // namespace

TEST(SetContract, StatefulOrderMismatchPreservesLeftOrderAndPolicy) {
  Set<int, StatefulOrder, PolicyAllocator<int>> left({1, 2, 3, 4}, StatefulOrder{100, true}, PolicyAllocator<int>(7));
  Set<int, StatefulOrder, PolicyAllocator<int>> right({2, 4, 5}, StatefulOrder{100, false}, PolicyAllocator<int>(9));
  auto common = left.intersection(right);
  auto remainder = left.difference(right);
  EXPECT_EQ(common.values(), (List<int>{4, 2}));
  EXPECT_EQ(remainder.values(), (List<int>{3, 1}));
  EXPECT_TRUE(common.key_comp().descending);
  EXPECT_EQ(common.get_allocator().identity, 7);
  EXPECT_EQ(remainder.get_allocator().identity, 7);
  EXPECT_EQ(left.combination(right).get_allocator().identity, 7);
}

TEST(SetContract, StatefulOrderEquivalenceUsesLeftRepresentatives) {
  Set<int, StatefulOrder> left({1, 2}, StatefulOrder{10, false});
  Set<int, StatefulOrder> right({11}, StatefulOrder{100, false});
  EXPECT_EQ(left.intersection(right).values(), (List<int>{1}));
  EXPECT_EQ(left.difference(right).values(), (List<int>{2}));
}

TEST(SetContract, HashAlgebraRetainsPoliciesAndAllocator) {
  expect_hash_result_policy<HashSet<int, StatefulHash, StatefulEqual, PolicyAllocator<int>>>();
  expect_hash_result_policy<StableHashSet<int, StatefulHash, StatefulEqual, PolicyAllocator<int>>>();
  StableHashSet<int> left, right{2, 3};
  left.max_load_factor(0.25f);
  left.insert_range(std::array{1, 2});
  EXPECT_FLOAT_EQ(left.intersection(right).max_load_factor(), 0.25f);
  EXPECT_FLOAT_EQ(left.difference(right).max_load_factor(), 0.25f);
}

TEST(SetContract, IntersectionRepresentativeSelectionRemainsStorageSpecific) {
  Set<Record, RecordLess> tree_left{{1, 10}, {2, 20}}, tree_right{{1, 99}, {3, 30}};
  auto tree_common = tree_left.intersection(tree_right);
  ASSERT_EQ(tree_common.size(), 1u);
  EXPECT_EQ(tree_common.begin()->revision, 10);
  HashSet<Record, RecordHash, RecordEqual> flat_left{{1, 10}, {2, 20}}, flat_right{{1, 99}, {3, 30}};
  auto flat_common = flat_left.intersection(flat_right);
  ASSERT_EQ(flat_common.size(), 1u);
  EXPECT_EQ(flat_common.begin()->revision, 99);
  StableHashSet<Record, RecordHash, RecordEqual> stable_left{{1, 10}, {2, 20}}, stable_right{{1, 99}, {3, 30}};
  auto stable_common = stable_left.intersection(stable_right);
  ASSERT_EQ(stable_common.size(), 1u);
  EXPECT_EQ(stable_common.begin()->revision, 99);
}

TEST(SetContract, SinglePassConstructionAndInsertion) {
  expect_single_pass_ranges<Set<int>>();
  expect_single_pass_ranges<HashSet<int>>();
  expect_single_pass_ranges<StableHashSet<int>>();
}

TEST(SetContract, IteratorRangesAllowExplicitElementConstruction) {
  auto check = []<set_container S>() {
    auto values = S::from(std::array{3, 1, 3});
    values.insert_range(std::array{2, 4, 2});
    auto distinct = std::views::iota(5) | std::views::take_while([](int value) { return value < 7; });
    values.insert_range(distinct);
    ASSERT_EQ(values.size(), 6u);
    for (int key = 1; key <= 6; ++key)
      EXPECT_TRUE(values.contains(ExplicitKey(key)));
  };
  check.operator()<Set<ExplicitKey>>();
  check.operator()<HashSet<ExplicitKey, ExplicitHash>>();
  check.operator()<StableHashSet<ExplicitKey, ExplicitHash>>();
}

TEST(SetContract, SupportedBorrowedRangesSurviveBulkMutation) {
  expect_borrowed_bulk_operations<Set<int>>();
  expect_borrowed_bulk_operations<HashSet<int, ZeroHash>>();
  expect_borrowed_bulk_operations<StableHashSet<int, ZeroHash>>();
}

TEST(SetContract, OwningEndpointsSurviveTemporaryOwner) {
  auto first = Set<int>{2, 1}.first();
  auto last = Set<int>{2, 1}.last();
  ASSERT_TRUE(first);
  ASSERT_TRUE(last);
  EXPECT_EQ(*first, 1);
  EXPECT_EQ(*last, 2);
  auto flat = HashSet<int>{7}.maybe_first();
  ASSERT_TRUE(flat);
  EXPECT_EQ(*flat, 7);
}

TEST(SetContract, WireOrderDuplicatesAndTruncatedPrefix) {
  Set<std::uint8_t> sorted{0x20, 0x10};
  auto encoded = DataStreamBuffer::serialize(sorted);
  ASSERT_TRUE(encoded);
  EXPECT_EQ(*encoded, ByteArray({"\x02\x10\x20", 3}));
  expect_set_wire_prefix<Set<std::uint8_t>>();
  expect_set_wire_prefix<HashSet<std::uint8_t>>();
  expect_set_wire_prefix<StableHashSet<std::uint8_t>>();
}
