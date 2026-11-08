import std;
import star.ordered_set;
import star.list;

#include "gtest/gtest.h"

using namespace Star;

namespace {

struct OrderedKey {
  int key;
  int payload = 0;
  static inline int copies = 0;
  static inline int moves = 0;

  explicit OrderedKey(int key, int payload = 0) : key(key), payload(payload) {}
  OrderedKey(OrderedKey const& other) : key(other.key), payload(other.payload) { ++copies; }
  OrderedKey(OrderedKey&& other) noexcept : key(other.key), payload(other.payload) { ++moves; }
  bool operator==(OrderedKey const&) const = default;
  bool operator<(OrderedKey const& other) const { return key < other.key; }
};

struct OrderedCompare {
  int modulus = 100;
  bool descending = false;
  bool operator()(OrderedKey const& left, OrderedKey const& right) const {
    return descending ? left.key % modulus > right.key % modulus : left.key % modulus < right.key % modulus;
  }
};

struct OrderedHash {
  int modulus = 100;
  std::size_t operator()(OrderedKey const& value) const { return value.key % modulus; }
};

struct OrderedEqual {
  int modulus = 100;
  bool operator()(OrderedKey const& left, OrderedKey const& right) const { return left.key % modulus == right.key % modulus; }
};

struct MoveOnlyKey {
  std::unique_ptr<int> key;
  explicit MoveOnlyKey(int value) : key(std::make_unique<int>(value)) {}
  MoveOnlyKey(MoveOnlyKey&&) = default;
  MoveOnlyKey& operator=(MoveOnlyKey&&) = default;
  MoveOnlyKey(MoveOnlyKey const&) = delete;
  MoveOnlyKey& operator=(MoveOnlyKey const&) = delete;
  bool operator<(MoveOnlyKey const& other) const { return *key < *other.key; }
};

struct MoveOnlyHash {
  std::size_t operator()(MoveOnlyKey const&) const { return 0; }
};

struct MoveOnlyEqual {
  bool operator()(MoveOnlyKey const& left, MoveOnlyKey const& right) const { return *left.key == *right.key; }
};

struct CopyOnlyKey {
  int key;
  explicit CopyOnlyKey(int value) : key(value) {}
  CopyOnlyKey(CopyOnlyKey const&) = default;
  CopyOnlyKey(CopyOnlyKey&&) = delete;
  bool operator<(CopyOnlyKey const& other) const { return key < other.key; }
};

struct OrderedAllocations {
  static inline std::map<void*, int> owners;
  static inline int wrong_owner = 0;
};

template <typename T, bool CopyPropagates = false, bool MovePropagates = false>
struct OrderedAllocator {
  using value_type = T;
  using propagate_on_container_copy_assignment = std::bool_constant<CopyPropagates>;
  using propagate_on_container_move_assignment = std::bool_constant<MovePropagates>;
  using is_always_equal = std::false_type;
  template <typename U>
  struct rebind { using other = OrderedAllocator<U, CopyPropagates, MovePropagates>; };

  int owner = 0;
  OrderedAllocator() = default;
  explicit OrderedAllocator(int owner) : owner(owner) {}
  template <typename U>
  OrderedAllocator(OrderedAllocator<U, CopyPropagates, MovePropagates> const& other) : owner(other.owner) {}

  T* allocate(std::size_t count) {
    auto pointer = std::allocator<T>{}.allocate(count);
    OrderedAllocations::owners.emplace(pointer, owner);
    return pointer;
  }
  void deallocate(T* pointer, std::size_t count) {
    auto found = OrderedAllocations::owners.find(pointer);
    if (found == OrderedAllocations::owners.end() || found->second != owner)
      ++OrderedAllocations::wrong_owner;
    else
      OrderedAllocations::owners.erase(found);
    std::allocator<T>{}.deallocate(pointer, count);
  }
  OrderedAllocator select_on_container_copy_construction() const { return OrderedAllocator(owner + 100); }
  template <typename U>
  bool operator==(OrderedAllocator<U, CopyPropagates, MovePropagates> const& other) const { return owner == other.owner; }
};

template <typename T>
concept OrderedFirst = requires(T&& value) { std::forward<T>(value).first(); };
template <typename T>
concept OrderedLast = requires(T&& value) { std::forward<T>(value).last(); };
template <typename T>
concept OrderedAt = requires(T&& value) { std::forward<T>(value).at(0); };
template <typename T>
concept OrderedView = requires(T&& value) { std::forward<T>(value).value_view(); };
template <typename T>
concept OrderedBadRange = requires(T& value) { value.insert_range(1); };
template <typename T>
concept OrderedBadSort = requires(T& value) { value.sort([](int) {}); };

static_assert(OrderedFirst<OrderedSet<int>&> && OrderedFirst<OrderedSet<int> const&>);
static_assert(!OrderedFirst<OrderedSet<int>> && !OrderedFirst<OrderedSet<int> const>);
static_assert(!OrderedLast<OrderedSet<int>> && !OrderedLast<OrderedSet<int> const>);
static_assert(!OrderedAt<OrderedSet<int>> && !OrderedAt<OrderedSet<int> const>);
static_assert(!OrderedView<OrderedSet<int>> && !OrderedView<OrderedSet<int> const>);
static_assert(!OrderedBadRange<OrderedSet<int>> && !OrderedBadSort<OrderedSet<int>>);
static_assert(std::same_as<decltype(std::declval<OrderedSet<int> const&>().empty()), bool>);
static_assert(!std::is_copy_constructible_v<OrderedSet<MoveOnlyKey>>);

// Both map policies must retain references to the transferred linked nodes.
template <typename Set>
void check_allocator_moves() {
  using Allocator = typename Set::allocator_type;
  int wrong_owner = OrderedAllocations::wrong_owner;
  auto allocations = OrderedAllocations::owners.size();
  {
    Set original(Allocator(1));
    original.emplace(3);
    original.emplace(1);
    original.emplace(2);
    auto address = std::addressof(*original.begin());
    OrderedKey::copies = OrderedKey::moves = 0;
    Set moved(std::move(original));
    EXPECT_TRUE(original.empty());
    EXPECT_EQ(std::addressof(*moved.begin()), address);
    EXPECT_EQ(OrderedKey::copies, 0);
    EXPECT_EQ(OrderedKey::moves, 0);
    EXPECT_TRUE(moved.contains(OrderedKey(1)));

    Set equal(std::move(moved), Allocator(1));
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(std::addressof(*equal.begin()), address);
    EXPECT_EQ(OrderedKey::copies, 0);
    EXPECT_EQ(OrderedKey::moves, 0);

    Set destination(Allocator(1));
    destination.emplace(9);
    destination = std::move(equal);
    EXPECT_TRUE(equal.empty());
    EXPECT_EQ(std::addressof(*destination.begin()), address);
    EXPECT_EQ(OrderedKey::copies, 0);
    EXPECT_EQ(OrderedKey::moves, 0);

    Set unequal(std::move(destination), Allocator(2));
    EXPECT_TRUE(destination.empty());
    EXPECT_EQ(unequal.get_allocator().owner, 2);
    EXPECT_EQ(OrderedKey::copies, 0);
    EXPECT_EQ(OrderedKey::moves, 3);
    EXPECT_TRUE(unequal.contains(OrderedKey(1)));
    EXPECT_TRUE(unequal.remove(OrderedKey(1)));
    EXPECT_EQ(unequal.size(), 2u);

    Set assigned(Allocator(3));
    assigned.emplace(8);
    OrderedKey::copies = OrderedKey::moves = 0;
    assigned = std::move(unequal);
    EXPECT_TRUE(unequal.empty());
    EXPECT_EQ(assigned.get_allocator().owner, 3);
    EXPECT_EQ(OrderedKey::copies, 0);
    EXPECT_EQ(OrderedKey::moves, 2);
    EXPECT_TRUE(assigned.contains(OrderedKey(3)));
    EXPECT_TRUE(assigned.remove(OrderedKey(2)));
    EXPECT_EQ(assigned.size(), 1u);
    assigned = std::move(assigned);
    EXPECT_TRUE(assigned.contains(OrderedKey(3)));
  }
  EXPECT_EQ(OrderedAllocations::wrong_owner, wrong_owner);
  EXPECT_EQ(OrderedAllocations::owners.size(), allocations);
}

template <typename Set>
void check_move_only() {
  Set set;
  EXPECT_TRUE(set.emplace(1).second);
  EXPECT_TRUE(set.add(MoveOnlyKey(2)));
  EXPECT_FALSE(set.insert(MoveOnlyKey(1)).second);
  EXPECT_TRUE(set.add_front(MoveOnlyKey(3)));
  EXPECT_FALSE(set.add_back(MoveOnlyKey(3)));
  auto first = set.take_first();
  ASSERT_TRUE(first);
  EXPECT_EQ(*first->key, 1);
  auto last = set.take_last();
  ASSERT_TRUE(last);
  EXPECT_EQ(*last->key, 3);
  EXPECT_TRUE(set.contains(MoveOnlyKey(2)));
  auto view = set.value_view();
  set.erase_all(view);
  EXPECT_TRUE(set.empty());
  EXPECT_FALSE(set.take_first());
}

}

TEST(OrderedSet, Insert) {
  OrderedSet<int> set{5, 4};
  set.insert(3);
  set.insert(2);
  set.insert(1);
  EXPECT_EQ(set.values(), List<int>({5, 4, 3, 2, 1}));
}

TEST(OrderedSet, AssignClear) {
  OrderedHashSet<int> set{1, 2, 3};
  OrderedHashSet<int> copy = set;
  auto const& aliased_set = set;
  set = aliased_set;
  EXPECT_EQ(set.values(), List<int>({1, 2, 3}));
  set.clear();
  EXPECT_TRUE(set.empty());
  EXPECT_EQ(copy.values(), List<int>({1, 2, 3}));
}

TEST(OrderedSet, AddReplace) {
  OrderedHashSet<int> set;
  EXPECT_TRUE(set.add(4));
  EXPECT_TRUE(set.add(6));
  EXPECT_TRUE(set.add(1));
  EXPECT_FALSE(set.add(1));
  EXPECT_TRUE(set.add(2));
  EXPECT_TRUE(set.add(3));
  EXPECT_FALSE(set.add(2));
  EXPECT_TRUE(set.replace(4));
  EXPECT_FALSE(set.replace(5));
  EXPECT_FALSE(set.add_back(6));
  EXPECT_EQ(set.values(), List<int>({1, 2, 3, 4, 5, 6}));
  EXPECT_TRUE(set.add_front(9));
  EXPECT_FALSE(set.add_front(5));
  EXPECT_EQ(set.values(), List<int>({5, 9, 1, 2, 3, 4, 6}));
  set.move_to_back(set.begin());
  set.move_to_front(std::prev(set.end()));
  EXPECT_EQ(set.values(), List<int>({5, 9, 1, 2, 3, 4, 6}));
}

TEST(OrderedSet, ReverseIterateAndSort) {
  OrderedHashSet<int> set{5, 4, 3, 2, 1};
  List<int> values;
  for (auto position = set.rbegin(); position != set.rend(); ++position)
    values.push_back(*position);
  EXPECT_EQ(values, List<int>({1, 2, 3, 4, 5}));
  set.sort();
  EXPECT_EQ(set.values(), List<int>({1, 2, 3, 4, 5}));
  struct Compare {
    std::unique_ptr<int> state = std::make_unique<int>(1);
    bool operator()(int left, int right) & { return *state && left > right; }
  };
  set.sort(Compare{});
  EXPECT_EQ(set.values(), List<int>({5, 4, 3, 2, 1}));
  EXPECT_TRUE(set.contains(3));
  EXPECT_EQ(set.index_of(3), 2u);
  EXPECT_FALSE(set.index_of(7));
}

TEST(OrderedSet, AliasedReplacementAndPayloadOrder) {
  OrderedHashSet<OrderedKey, OrderedHash, OrderedEqual> set;
  set.emplace(1, 10);
  set.emplace(2, 20);
  auto first = set.first();
  ASSERT_TRUE(first);
  EXPECT_TRUE(set.replace(first->get()));
  EXPECT_EQ(set.begin()->key, 2);
  EXPECT_EQ(std::prev(set.end())->payload, 10);
  EXPECT_FALSE(set.add_front(OrderedKey(1, 99)));
  EXPECT_EQ(set.begin()->payload, 10);
  EXPECT_FALSE(set.add_back(OrderedKey(1, 88)));
  EXPECT_EQ(std::prev(set.end())->payload, 10);
  EXPECT_TRUE(set.replace(OrderedKey(1, 42)));
  EXPECT_EQ(std::prev(set.end())->payload, 42);
}

TEST(OrderedSet, AliasedRangesAndSnapshots) {
  OrderedHashSet<int> set{1, 2, 3, 4};
  set.insert_range(set);
  EXPECT_EQ(set.size(), 4u);
  set.insert_range(set.value_view() | std::views::transform([](int value) { return value + 10; }));
  EXPECT_EQ(set.values(), List<int>({1, 2, 3, 4, 11, 12, 13, 14}));
  auto snapshot = set.values();
  auto odds = set.value_view() | std::views::filter([](int value) { return value % 2 != 0; });
  set.erase_all(odds);
  EXPECT_EQ(set.values(), List<int>({2, 4, 12, 14}));
  auto repeated = std::views::iota(0, 3) | std::views::transform([&](int) -> int const& { return *set.begin(); });
  set.erase_all(repeated);
  EXPECT_EQ(set.values(), List<int>({4, 12, 14}));
  set.erase_all(set.value_view() | std::views::reverse | std::views::take(2));
  EXPECT_EQ(set.values(), List<int>({4}));
  set.erase_all(set);
  EXPECT_TRUE(set.empty());
  EXPECT_EQ(snapshot, List<int>({1, 2, 3, 4, 11, 12, 13, 14}));
}

TEST(OrderedSet, SnapshotCopiesDirectlyAndOutlivesOwner) {
  List<OrderedKey> snapshot;
  {
    OrderedSet<OrderedKey> set;
    for (int key = 0; key != 32; ++key)
      set.emplace(key);
    OrderedKey::copies = OrderedKey::moves = 0;
    snapshot = set.values();
    EXPECT_EQ(OrderedKey::copies, 32);
    EXPECT_EQ(OrderedKey::moves, 0);
  }
  ASSERT_EQ(snapshot.size(), 32u);
  EXPECT_EQ(snapshot.begin()->key, 0);
  EXPECT_EQ(std::prev(snapshot.end())->key, 31);
}

TEST(OrderedSet, StatefulTreePoliciesAndRepresentatives) {
  using Set = OrderedSet<OrderedKey, OrderedCompare>;
  Set set(OrderedCompare{10, true});
  set.emplace(1, 10);
  set.emplace(2, 20);
  set.emplace(3, 30);
  Set copy(set);
  EXPECT_EQ(copy.key_comp().modulus, 10);
  EXPECT_TRUE(copy.key_comp().descending);
  EXPECT_TRUE(copy.contains(OrderedKey(11)));
  Set assigned;
  assigned = set;
  EXPECT_TRUE(assigned.contains(OrderedKey(12)));
  EXPECT_TRUE(assigned.key_comp().descending);

  Set right(OrderedCompare{100, false});
  right.emplace(13, 300);
  right.emplace(11, 100);
  auto intersection = set.intersection(right);
  ASSERT_EQ(intersection.size(), 2u);
  EXPECT_EQ(intersection.begin()->key, 13);
  EXPECT_EQ(intersection.begin()->payload, 300);
  EXPECT_EQ(std::prev(intersection.end())->key, 11);
  EXPECT_EQ(intersection.key_comp().modulus, 10);
  EXPECT_TRUE(intersection.key_comp().descending);
  EXPECT_TRUE(intersection.contains(OrderedKey(3)));

  auto difference = set.difference(right);
  EXPECT_EQ(difference.size(), 3u);
  EXPECT_EQ(difference.begin()->key, 1);
  EXPECT_EQ(difference.key_comp().modulus, 10);
}

TEST(OrderedSet, StatefulHashPoliciesAndRepresentatives) {
  using Set = OrderedHashSet<OrderedKey, OrderedHash, OrderedEqual>;
  Set set(OrderedHash{10}, OrderedEqual{10});
  set.emplace(1, 10);
  set.emplace(2, 20);
  set.emplace(3, 30);
  Set copy(set);
  EXPECT_EQ(copy.hash_function().modulus, 10);
  EXPECT_EQ(copy.key_eq().modulus, 10);
  EXPECT_TRUE(copy.contains(OrderedKey(11)));
  Set assigned;
  assigned = set;
  EXPECT_TRUE(assigned.contains(OrderedKey(12)));

  Set right;
  right.emplace(13, 300);
  right.emplace(11, 100);
  auto intersection = set.intersection(right);
  ASSERT_EQ(intersection.size(), 2u);
  EXPECT_EQ(intersection.begin()->key, 13);
  EXPECT_EQ(intersection.begin()->payload, 300);
  EXPECT_EQ(std::prev(intersection.end())->key, 11);
  EXPECT_EQ(intersection.hash_function().modulus, 10);
  EXPECT_EQ(intersection.key_eq().modulus, 10);
  EXPECT_TRUE(intersection.contains(OrderedKey(3)));
  auto difference = set.difference(right);
  EXPECT_EQ(difference.size(), 3u);
  EXPECT_EQ(difference.hash_function().modulus, 10);
  EXPECT_EQ(difference.key_eq().modulus, 10);
  Set moved(std::move(copy));
  EXPECT_TRUE(copy.empty());
  EXPECT_EQ(moved.hash_function().modulus, 10);
  EXPECT_EQ(moved.key_eq().modulus, 10);
  EXPECT_TRUE(moved.contains(OrderedKey(11)));
  assigned = std::move(moved);
  EXPECT_TRUE(moved.empty());
  EXPECT_EQ(assigned.hash_function().modulus, 10);
  EXPECT_TRUE(assigned.contains(OrderedKey(12)));
}

TEST(OrderedSet, AllocatorAwareMoves) {
  using Allocator = OrderedAllocator<OrderedKey>;
  check_allocator_moves<OrderedSet<OrderedKey, OrderedCompare, Allocator>>();
  check_allocator_moves<OrderedHashSet<OrderedKey, OrderedHash, OrderedEqual, Allocator>>();
}

TEST(OrderedSet, AllocatorCopySelectionPropagationAndResults) {
  using Allocator = OrderedAllocator<OrderedKey, true, false>;
  using Set = OrderedHashSet<OrderedKey, OrderedHash, OrderedEqual, Allocator>;
  auto allocations = OrderedAllocations::owners.size();
  int wrong_owner = OrderedAllocations::wrong_owner;
  {
    Set source(OrderedHash{10}, OrderedEqual{10}, Allocator(7));
    source.emplace(1);
    Set selected(source);
    EXPECT_EQ(selected.get_allocator().owner, 107);
    EXPECT_TRUE(selected.contains(OrderedKey(11)));
    Set explicit_copy(source, Allocator(8));
    EXPECT_EQ(explicit_copy.get_allocator().owner, 8);
    Set assigned(Allocator(9));
    assigned.emplace(2);
    assigned = source;
    EXPECT_EQ(assigned.get_allocator().owner, 7);
    EXPECT_TRUE(assigned.contains(OrderedKey(11)));
    auto intersection = source.intersection(selected);
    EXPECT_EQ(intersection.get_allocator().owner, 7);
    auto difference = source.difference(Set{});
    EXPECT_EQ(difference.get_allocator().owner, 7);
  }
  EXPECT_EQ(OrderedAllocations::wrong_owner, wrong_owner);
  EXPECT_EQ(OrderedAllocations::owners.size(), allocations);
}

TEST(OrderedSet, NonpropagatingCopyRetainsDestinationAllocator) {
  using Allocator = OrderedAllocator<OrderedKey>;
  using Set = OrderedSet<OrderedKey, OrderedCompare, Allocator>;
  auto allocations = OrderedAllocations::owners.size();
  int wrong_owner = OrderedAllocations::wrong_owner;
  {
    Set source(OrderedCompare{10, true}, Allocator(1));
    source.emplace(1);
    Set destination(Allocator(2));
    destination.emplace(9);
    destination = source;
    EXPECT_EQ(destination.get_allocator().owner, 2);
    EXPECT_EQ(destination.key_comp().modulus, 10);
    EXPECT_TRUE(destination.key_comp().descending);
    EXPECT_TRUE(destination.contains(OrderedKey(11)));
    auto intersection = source.intersection(destination);
    EXPECT_EQ(intersection.get_allocator().owner, 1);
    auto difference = source.difference(Set{});
    EXPECT_EQ(difference.get_allocator().owner, 1);
    EXPECT_EQ(difference.key_comp().modulus, 10);
  }
  EXPECT_EQ(OrderedAllocations::wrong_owner, wrong_owner);
  EXPECT_EQ(OrderedAllocations::owners.size(), allocations);
}

TEST(OrderedSet, UnequalAllocatorMoveOnlyKeys) {
  using Allocator = OrderedAllocator<MoveOnlyKey>;
  using Set = OrderedHashSet<MoveOnlyKey, MoveOnlyHash, MoveOnlyEqual, Allocator>;
  auto allocations = OrderedAllocations::owners.size();
  int wrong_owner = OrderedAllocations::wrong_owner;
  {
    Set source(Allocator(1));
    source.emplace(1);
    source.emplace(2);
    source.emplace(3);
    Set destination(std::move(source), Allocator(2));
    EXPECT_TRUE(source.empty());
    EXPECT_TRUE(destination.contains(MoveOnlyKey(2)));
    EXPECT_TRUE(destination.remove(MoveOnlyKey(1)));
    Set assigned(Allocator(3));
    assigned = std::move(destination);
    EXPECT_TRUE(destination.empty());
    EXPECT_EQ(assigned.get_allocator().owner, 3);
    auto first = assigned.take_first();
    ASSERT_TRUE(first);
    EXPECT_EQ(*first->key, 2);
    EXPECT_TRUE(assigned.contains(MoveOnlyKey(3)));
  }
  EXPECT_EQ(OrderedAllocations::wrong_owner, wrong_owner);
  EXPECT_EQ(OrderedAllocations::owners.size(), allocations);
}

TEST(OrderedSet, PropagatingMoveRetainsNodes) {
  using Allocator = OrderedAllocator<OrderedKey, false, true>;
  using Set = OrderedSet<OrderedKey, OrderedCompare, Allocator>;
  auto allocations = OrderedAllocations::owners.size();
  int wrong_owner = OrderedAllocations::wrong_owner;
  {
    Set source(OrderedCompare{10, true}, Allocator(1));
    source.emplace(1);
    auto address = std::addressof(*source.begin());
    Set destination(Allocator(2));
    destination.emplace(2);
    OrderedKey::copies = OrderedKey::moves = 0;
    destination = std::move(source);
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(destination.get_allocator().owner, 1);
    EXPECT_EQ(std::addressof(*destination.begin()), address);
    EXPECT_EQ(OrderedKey::copies, 0);
    EXPECT_EQ(OrderedKey::moves, 0);
    EXPECT_TRUE(destination.contains(OrderedKey(11)));
  }
  EXPECT_EQ(OrderedAllocations::wrong_owner, wrong_owner);
  EXPECT_EQ(OrderedAllocations::owners.size(), allocations);
}

TEST(OrderedSet, MoveOnlyKeysAndCollisionExtraction) {
  check_move_only<OrderedSet<MoveOnlyKey>>();
  check_move_only<OrderedHashSet<MoveOnlyKey, MoveOnlyHash, MoveOnlyEqual>>();
}

TEST(OrderedSet, CopyOnlyKeys) {
  OrderedSet<CopyOnlyKey> set;
  CopyOnlyKey one(1);
  EXPECT_TRUE(set.add(one));
  set.emplace(2);
  EXPECT_TRUE(set.replace(one));
  auto first = set.take_first();
  ASSERT_TRUE(first);
  EXPECT_EQ(first->key, 2);
  auto last = set.take_last();
  ASSERT_TRUE(last);
  EXPECT_EQ(last->key, 1);
  EXPECT_TRUE(set.empty());
}

TEST(OrderedSet, CheckedAccessAndRemoval) {
  OrderedSet<int> set;
  EXPECT_FALSE(set.first());
  EXPECT_FALSE(set.last());
  EXPECT_FALSE(set.at(0));
  EXPECT_FALSE(set.remove_first());
  EXPECT_FALSE(set.remove_last());
  EXPECT_FALSE(set.take_first());
  EXPECT_FALSE(set.take_last());
  set.insert_range(List<int>{3, 1, 2});
  auto first = set.first();
  auto last = set.last();
  auto middle = set.at(1);
  ASSERT_TRUE(first);
  ASSERT_TRUE(last);
  ASSERT_TRUE(middle);
  EXPECT_EQ(first->get(), 3);
  EXPECT_EQ(last->get(), 2);
  EXPECT_EQ(middle->get(), 1);
  EXPECT_FALSE(set.at(3));
  ASSERT_TRUE(set.remove_first());
  ASSERT_TRUE(set.remove_last());
  EXPECT_EQ(set.values(), List<int>({1}));
}

TEST(OrderedSet, DistinctSentinelAndSinglePassRanges) {
  int input[] = {4, 3, 4, 2, 1};
  auto range = std::ranges::subrange(std::counted_iterator(input, 5), std::default_sentinel);
  OrderedHashSet<int> set(range.begin(), range.end());
  EXPECT_EQ(set.values(), List<int>({4, 3, 2, 1}));
  std::istringstream stream("7 8 7 9");
  auto single_pass = std::ranges::istream_view<int>(stream);
  set.insert_range(single_pass);
  EXPECT_EQ(set.values(), List<int>({4, 3, 2, 1, 7, 8, 9}));
  OrderedSet<int> from_range(std::from_range, range);
  EXPECT_EQ(from_range.values(), List<int>({4, 3, 2, 1}));
}

TEST(OrderedSet, WrapperValuedKeysBorrowSlotsWithoutUnwrapping) {
  using Wrapped = std::reference_wrapper<int>;
  struct WrappedHash {
    std::size_t operator()(Wrapped const& value) const { return value.get(); }
  };
  struct WrappedEqual {
    bool operator()(Wrapped const& a, Wrapped const& b) const { return a.get() == b.get(); }
  };
  int first_value = 1, last_value = 2;
  OrderedHashSet<Wrapped, WrappedHash, WrappedEqual> values;
  EXPECT_TRUE(values.add(std::ref(first_value)));
  EXPECT_TRUE(values.add(std::ref(last_value)));
  EXPECT_TRUE(values.contains(std::ref(first_value)));
  auto first = values.first();
  auto last = values.last();
  auto indexed = values.at(1);
  ASSERT_TRUE(first);
  ASSERT_TRUE(last);
  ASSERT_TRUE(indexed);
  EXPECT_EQ(std::addressof(first->get()), std::addressof(*values.begin()));
  EXPECT_EQ(std::addressof(first->get().get()), std::addressof(first_value));
  EXPECT_EQ(std::addressof(last->get().get()), std::addressof(last_value));
  EXPECT_EQ(std::addressof(indexed->get()), std::addressof(last->get()));
  auto copied = values;
  auto moved = std::move(values);
  EXPECT_EQ(std::addressof(first->get()), std::addressof(*moved.begin()));
  copied.erase_all(std::array{std::ref(first_value)});
  EXPECT_FALSE(copied.contains(std::ref(first_value)));
  EXPECT_TRUE(copied.contains(std::ref(last_value)));
  auto extracted = moved.take_first();
  ASSERT_TRUE(extracted);
  EXPECT_EQ(std::addressof(extracted->get()), std::addressof(first_value));
  EXPECT_TRUE(moved.contains(std::ref(last_value)));
}
