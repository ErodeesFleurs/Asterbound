import std;
import star.flat_hash_set;
import star.flat_hash_map;
import star.flat_hash_table;

#include "gtest/gtest.h"

using namespace Star;

namespace {
struct StorageHash {
  int start = 0;
  int spread = 1;
  std::size_t operator()(int key) const { return start + key % spread; }
};

struct AllocationState {
  int allocations = 0;
  int outstanding = 0;
  AllocationState* selected = nullptr;
};

auto& storage_allocations() {
  static std::unordered_map<void*, AllocationState*> owners;
  return owners;
}

template <typename T, bool Propagate = false>
struct StorageAllocator {
  using value_type = T;
  using propagate_on_container_copy_assignment = std::bool_constant<Propagate>;
  using propagate_on_container_move_assignment = std::bool_constant<Propagate>;
  template <typename U> struct rebind { using other = StorageAllocator<U, Propagate>; };
  AllocationState* state = nullptr;

  StorageAllocator() = default;
  explicit StorageAllocator(AllocationState& owner) : state(&owner) {}
  template <typename U> StorageAllocator(StorageAllocator<U, Propagate> const& other) : state(other.state) {}
  StorageAllocator select_on_container_copy_construction() const {
    return state && state->selected ? StorageAllocator(*state->selected) : *this;
  }
  T* allocate(std::size_t count) {
    auto memory = std::allocator<T>{}.allocate(count);
    storage_allocations().emplace(memory, state);
    if (state) {
      ++state->allocations;
      ++state->outstanding;
    }
    return memory;
  }
  void deallocate(T* memory, std::size_t count) {
    auto found = storage_allocations().find(memory);
    EXPECT_NE(found, storage_allocations().end());
    if (found != storage_allocations().end()) {
      EXPECT_EQ(found->second, state);
      if (found->second)
        --found->second->outstanding;
      storage_allocations().erase(found);
    }
    std::allocator<T>{}.deallocate(memory, count);
  }
  template <typename U> bool operator==(StorageAllocator<U, Propagate> const& rhs) const { return state == rhs.state; }
};

struct ModuloHash {
  int modulus;
  int salt;
  std::size_t operator()(int key) const { return (key % modulus) ^ salt; }
};
struct ModuloEqual {
  int modulus;
  bool operator()(int left, int right) const { return left % modulus == right % modulus; }
};

struct ContextualBool {
  bool value;
  explicit operator bool() const { return value; }
  // Equality must use contextual conversion, not call an arbitrary operator!.
  bool operator!() const { return value; }
};
struct Revision {
  int key;
  int revision;
  friend ContextualBool operator!=(Revision const& left, Revision const& right) {
    return {left.key != right.key || left.revision != right.revision};
  }
};
struct RevisionHash {
  std::size_t operator()(Revision const&) const { return 0; }
};
struct RevisionEqual {
  bool operator()(Revision const& left, Revision const& right) const { return left.key == right.key; }
};

struct OwnedKey {
  int key;
  std::unique_ptr<int> payload;
  explicit OwnedKey(int value) : key(value), payload(std::make_unique<int>(value * 10)) {}
  OwnedKey(OwnedKey&& other) noexcept : key(other.key), payload(std::move(other.payload)) {}
  OwnedKey(OwnedKey const&) = delete;
  OwnedKey& operator=(OwnedKey const&) = delete;
  OwnedKey& operator=(OwnedKey&&) = delete;
};
struct OwnedHash {
  using is_transparent = void;
  std::size_t operator()(OwnedKey const& value) const { return (*this)(value.key); }
  std::size_t operator()(int value) const { return 6 + value % 2; }
};
struct OwnedEqual {
  using is_transparent = void;
  bool operator()(OwnedKey const& left, OwnedKey const& right) const { return left.key == right.key; }
  bool operator()(OwnedKey const& left, int right) const { return left.key == right; }
};

struct Query { int key; };
struct MutableHash {
  using is_transparent = void;
  std::size_t operator()(int value) { return value; }
  std::size_t operator()(Query const& value) { return value.key; }
};
struct MutableEqual {
  using is_transparent = void;
  ContextualBool operator()(int const& left, int const& right) { return {left == right}; }
  ContextualBool operator()(int const& left, Query const& right) { return {left == right.key}; }
};
struct MutableGetKey {
  int& operator()(std::pair<int, int>& value) { return value.first; }
};
struct MutableStoredEqual {
  ContextualBool operator()(int& left, Query const& right) { return {left == right.key}; }
  ContextualBool operator()(int& left, int& right) { return {left == right}; }
};
struct PairKey {
  int const& operator()(std::pair<int, int> const& value) const { return value.first; }
};

struct ConstructOnly {
  int key;
  explicit ConstructOnly(int value) : key(value) {}
  ConstructOnly(ConstructOnly const& other) : key(other.key) {}
  ConstructOnly(ConstructOnly&& other) noexcept : key(other.key) {}
  ConstructOnly& operator=(ConstructOnly const&) = delete;
  ConstructOnly& operator=(ConstructOnly&&) = delete;
};
struct ConstructOnlyHash {
  std::size_t operator()(ConstructOnly const&) const { return 0; }
};
struct ConstructOnlyEqual {
  bool operator()(ConstructOnly const& left, ConstructOnly const& right) const { return left.key == right.key; }
};
struct RecursiveRecord {
  RecursiveRecord() = default;
  int marker = 0;
  FlatHashMap<int, RecursiveRecord> children;
};

TEST(SetStorage, SharedTableDefersRecursiveMappedValueCopyChecks) {
  RecursiveRecord original;
  auto child = original.children.emplace(3, RecursiveRecord{}).first;
  child->second.marker = 10;
  child->second.children.emplace(4, RecursiveRecord{});
  auto copied = original;
  original.children.find(3)->second.marker = 20;
  auto copied_child = copied.children.find(3);
  ASSERT_NE(copied_child, copied.children.end());
  EXPECT_EQ(copied_child->second.marker, 10);
  auto const& grandchildren = copied_child->second.children;
  EXPECT_NE(grandchildren.find(4), grandchildren.end());
}

} // namespace

TEST(SetStorage, EveryCollisionRangePreservesSurvivorsAndReturnedIdentity) {
  using Storage = FlatHashSet<int, StorageHash, std::equal_to<int>, StorageAllocator<int>>;
  AllocationState state;
  {
    // Five elements fit in eight buckets. Enumerate every small home-bucket
    // arrangement, including split clusters and wrap through the end sentinel.
    for (int start = 0; start != 8; ++start) {
      for (int spread = 1; spread != 9; ++spread) {
        for (int rotation = 0; rotation != 5; ++rotation) {
          Storage original(0, StorageHash{start, spread}, {}, StorageAllocator<int>(state));
          for (int key = 0; key != 5; ++key)
            original.insert((key + rotation) % 5);
          std::vector<int> order(original.begin(), original.end());
          for (std::size_t first = 0; first <= order.size(); ++first) {
            for (std::size_t last = first; last <= order.size(); ++last) {
              SCOPED_TRACE(::testing::Message() << start << '/' << spread << '/' << rotation << '/' << first << '/' << last);
              Storage values(original);
              auto begin = std::next(values.begin(), first);
              auto end = std::next(values.begin(), last);
              auto allocations = state.allocations;
              auto result = values.erase(begin, end);
              EXPECT_EQ(state.allocations, allocations);
              EXPECT_EQ(values.size(), order.size() - (last - first));
              if (last == order.size())
                EXPECT_EQ(result, values.end());
              else {
                ASSERT_NE(result, values.end());
                EXPECT_EQ(*result, order[last]);
              }
              for (std::size_t index = 0; index != order.size(); ++index)
                EXPECT_EQ(values.contains(order[index]), index < first || index >= last);
              std::set<int> iterated(values.begin(), values.end());
              EXPECT_EQ(iterated.size(), values.size());
              for (int key : values)
                EXPECT_TRUE(values.contains(key));
            }
          }
        }
      }
    }
  }
  EXPECT_EQ(state.outstanding, 0);
}

TEST(SetStorage, WrappedMoveOnlyRangesRelocateWithoutCopyOrAllocation) {
  using Storage = FlatHashSet<OwnedKey, OwnedHash, OwnedEqual, StorageAllocator<OwnedKey>>;
  static_assert(!std::is_copy_constructible_v<Storage>);
  static_assert(!std::is_copy_assignable_v<Storage>);
  static_assert(std::same_as<Storage::iterator, Storage::const_iterator>);
  static_assert(std::same_as<Storage::reference, OwnedKey const&>);
  static_assert(std::same_as<Storage::pointer, OwnedKey const*>);
  static_assert(std::same_as<std::iter_reference_t<Storage::iterator>, OwnedKey const&>);
  static_assert(std::forward_iterator<Storage::iterator>);
  AllocationState state;
  {
    for (std::size_t first = 0; first <= 5; ++first) {
      for (std::size_t last = first; last <= 5; ++last) {
        Storage values(0, OwnedHash{}, OwnedEqual{}, StorageAllocator<OwnedKey>(state));
        for (int key = 0; key != 5; ++key)
          values.emplace(key);
        std::vector<int> order;
        std::array<int const*, 5> identities{};
        for (auto const& value : values) {
          order.push_back(value.key);
          identities[value.key] = value.payload.get();
        }
        auto begin = std::next(values.begin(), first);
        auto end = std::next(values.begin(), last);
        auto allocations = state.allocations;
        auto result = values.erase(begin, end);
        EXPECT_EQ(state.allocations, allocations);
        EXPECT_EQ(values.size(), 5 - (last - first));
        if (last == 5)
          EXPECT_EQ(result, values.end());
        else {
          ASSERT_NE(result, values.end());
          EXPECT_EQ(result->key, order[last]);
        }
        for (std::size_t index = 0; index != order.size(); ++index) {
          auto found = values.find(order[index]);
          if (index >= first && index < last)
            EXPECT_EQ(found, values.end());
          else {
            ASSERT_NE(found, values.end());
            EXPECT_EQ(found->payload.get(), identities[order[index]]);
          }
        }
      }
    }
  }
  EXPECT_EQ(state.outstanding, 0);
}

TEST(SetStorage, EmptyAndFullRangesIncludeUnallocatedStorage) {
  FlatHashSet<int> values;
  EXPECT_EQ(values.erase(values.begin(), values.end()), values.end());
  values.reserve(32);
  EXPECT_EQ(values.erase(values.begin(), values.end()), values.end());
  values.insert({1, 2, 3});
  EXPECT_EQ(values.erase(values.begin(), values.end()), values.end());
  EXPECT_TRUE(values.empty());
  values.insert(4);
  EXPECT_TRUE(values.contains(4));
}

TEST(SetStorage, CopyPreservesPoliciesAndHonorsAllocatorSelectionAndPropagation) {
  AllocationState source_state;
  AllocationState selected_state;
  AllocationState destination_state;
  source_state.selected = &selected_state;
  using Storage = FlatHashSet<int, ModuloHash, ModuloEqual, StorageAllocator<int>>;
  {
    Storage original(0, ModuloHash{10, 7}, ModuloEqual{10}, StorageAllocator<int>(source_state));
    original.insert({1, 2, 3});
    auto original_first = original.begin();
    auto const& aliased_original = original;
    original = aliased_original;
    EXPECT_EQ(original.size(), 3u);
    EXPECT_EQ(original.begin(), original_first);
    EXPECT_TRUE(original.contains(11));

    Storage copy(original);
    EXPECT_EQ(copy.get_allocator().state, &selected_state);
    EXPECT_EQ(copy.hash_function().modulus, 10);
    EXPECT_EQ(copy.hash_function().salt, 7);
    EXPECT_EQ(copy.key_eq().modulus, 10);
    EXPECT_TRUE(copy.contains(11));

    Storage explicit_copy(original, StorageAllocator<int>(destination_state));
    EXPECT_EQ(explicit_copy.get_allocator().state, &destination_state);
    EXPECT_TRUE(explicit_copy.contains(12));

    Storage assigned(0, ModuloHash{100, 91}, ModuloEqual{100}, StorageAllocator<int>(destination_state));
    assigned.insert(99);
    assigned = original;
    EXPECT_EQ(assigned.get_allocator().state, &destination_state);
    EXPECT_EQ(assigned.hash_function().salt, 7);
    EXPECT_EQ(assigned.key_eq().modulus, 10);
    EXPECT_TRUE(assigned.contains(13));
    EXPECT_FALSE(assigned.contains(99));
  }
  EXPECT_EQ(source_state.outstanding, 0);
  EXPECT_EQ(selected_state.outstanding, 0);
  EXPECT_EQ(destination_state.outstanding, 0);

  using Propagating = FlatHashSet<int, ModuloHash, ModuloEqual, StorageAllocator<int, true>>;
  {
    Propagating source(0, ModuloHash{10, 7}, ModuloEqual{10}, StorageAllocator<int, true>(source_state));
    source.insert(1);
    Propagating destination(0, ModuloHash{100, 91}, ModuloEqual{100}, StorageAllocator<int, true>(destination_state));
    destination.insert(9);
    destination = source;
    EXPECT_EQ(destination.get_allocator().state, &source_state);
    EXPECT_TRUE(destination.contains(11));
  }
  EXPECT_EQ(source_state.outstanding, 0);
  EXPECT_EQ(destination_state.outstanding, 0);
}

TEST(SetStorage, CopyConstructionAndAssignmentDoNotRequireKeyAssignment) {
  using Storage = FlatHashSet<ConstructOnly, ConstructOnlyHash, ConstructOnlyEqual>;
  static_assert(std::is_copy_constructible_v<Storage>);
  static_assert(std::is_copy_assignable_v<Storage>);
  Storage source;
  source.emplace(1);
  source.emplace(2);
  Storage copied(source);
  Storage assigned;
  assigned.emplace(7);
  assigned = source;
  EXPECT_EQ(copied.size(), 2u);
  EXPECT_EQ(assigned.size(), 2u);
  EXPECT_NE(copied.find(ConstructOnly(1)), copied.end());
  EXPECT_NE(assigned.find(ConstructOnly(2)), assigned.end());
}

TEST(SetStorage, UnorderedEqualityChecksCompleteValuesWithContextualInequality) {
  using Storage = FlatHashSet<Revision, RevisionHash, RevisionEqual>;
  Storage left{{1, 10}, {2, 20}, {3, 30}};
  Storage reversed{{3, 30}, {2, 20}, {1, 10}};
  EXPECT_TRUE(left == reversed);
  EXPECT_FALSE(left != reversed);
  Storage changed{{3, 31}, {2, 20}, {1, 10}};
  EXPECT_FALSE(left == changed);
  EXPECT_TRUE(left != changed);
}

TEST(SetStorage, ExtractTransfersOwnershipWithConstPublicKeys) {
  using Storage = FlatHashSet<OwnedKey, OwnedHash, OwnedEqual, StorageAllocator<OwnedKey>>;
  AllocationState source_state;
  AllocationState destination_state;
  {
    Storage source(0, OwnedHash{}, OwnedEqual{}, StorageAllocator<OwnedKey>(source_state));
    for (int key = 0; key != 5; ++key)
      source.emplace(key);
    auto position = source.find(2);
    ASSERT_NE(position, source.end());
    auto identity = position->payload.get();
    auto owned = source.extract_value(position);
    EXPECT_EQ(owned.key, 2);
    EXPECT_EQ(owned.payload.get(), identity);
    EXPECT_FALSE(source.contains(2));
    EXPECT_EQ(source.size(), 4u);

    Storage destination(0, OwnedHash{}, OwnedEqual{}, StorageAllocator<OwnedKey>(destination_state));
    destination.emplace(9);
    destination = std::move(source);
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(destination.size(), 4u);
    EXPECT_EQ(destination.get_allocator().state, &destination_state);
    for (int key : {0, 1, 3, 4}) {
      auto found = destination.find(key);
      ASSERT_NE(found, destination.end());
      ASSERT_TRUE(found->payload);
      EXPECT_EQ(*found->payload, key * 10);
    }
    Storage moved(std::move(destination));
    EXPECT_TRUE(destination.empty());
    EXPECT_EQ(moved.get_allocator().state, &destination_state);
    EXPECT_EQ(moved.size(), 4u);
    Storage allocator_moved(std::move(moved), StorageAllocator<OwnedKey>(source_state));
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(allocator_moved.get_allocator().state, &source_state);
    EXPECT_EQ(allocator_moved.size(), 4u);
    for (int key : {0, 1, 3, 4})
      EXPECT_TRUE(allocator_moved.contains(key));
  }
  EXPECT_EQ(source_state.outstanding, 0);
  EXPECT_EQ(destination_state.outstanding, 0);
}

TEST(SetStorage, DuplicateAndSelfRangeInsertionNeverInvalidateBorrowedKeys) {
  using Storage = FlatHashSet<int, StorageHash, std::equal_to<int>, StorageAllocator<int>>;
  AllocationState state;
  {
    Storage values(0, StorageHash{6, 2}, {}, StorageAllocator<int>(state));
    values.insert({0, 1, 2, 3, 4}); // Next new key would grow eight buckets.
    auto position = values.begin();
    auto address = std::addressof(*position);
    auto allocations = state.allocations;
    EXPECT_FALSE(values.insert(*position).second);
    values.insert(values.begin(), values.end());
    values.insert_range(values);
    values.insert_range(std::ranges::subrange(values.begin(), values.end()));
    values.insert_range(values | std::views::filter([](int key) { return key % 2 == 0; }));
    EXPECT_EQ(values.size(), 5u);
    EXPECT_EQ(state.allocations, allocations);
    EXPECT_EQ(std::addressof(*position), address);
    for (int key = 0; key != 5; ++key)
      EXPECT_TRUE(values.contains(key));
  }
  EXPECT_EQ(state.outstanding, 0);
}

TEST(SetStorage, MapDuplicateSelfRangesDoNotGrowOrInvalidateProxies) {
  using Storage = FlatHashMap<int, int, StorageHash, std::equal_to<int>, StorageAllocator<int>>;
  AllocationState state;
  {
    Storage values(0, StorageHash{6, 2}, {}, StorageAllocator<int>(state));
    for (int key = 0; key != 5; ++key)
      values.emplace(key, key * 10);
    auto borrowed = *values.begin();
    auto identity = std::addressof(borrowed.second);
    auto allocations = state.allocations;
    values.insert(values.begin(), values.end());
    EXPECT_EQ(values.size(), 5u);
    EXPECT_EQ(state.allocations, allocations);
    EXPECT_EQ(std::addressof(values.find(borrowed.first)->second), identity);
  }
  EXPECT_EQ(state.outstanding, 0);
}

TEST(SetStorage, FromRangeAndInsertionConsumeDistinctSentinelInputOnce) {
  std::istringstream first("1 2 1 3 4");
  FlatHashSet<int> values(std::from_range, std::ranges::istream_view<int>(first));
  EXPECT_EQ(values.size(), 4u);
  for (int key : {1, 2, 3, 4})
    EXPECT_TRUE(values.contains(key));
  std::istringstream second("4 5 6 5");
  auto input = std::ranges::istream_view<int>(second);
  values.insert(input.begin(), input.end());
  EXPECT_EQ(values.size(), 6u);
  std::istringstream third("6 7 8 7");
  values.insert_range(std::ranges::istream_view<int>(third));
  EXPECT_EQ(values.size(), 8u);
  auto distinct = std::views::iota(0) | std::views::take_while([](int key) { return key < 5; });
  FlatHashSet<int> constructed(distinct.begin(), distinct.end());
  EXPECT_EQ(constructed.size(), 5u);
  EXPECT_TRUE(constructed.contains(0));
  EXPECT_TRUE(constructed.contains(4));

  std::vector<OwnedKey> owned;
  owned.emplace_back(1);
  owned.emplace_back(2);
  FlatHashSet<OwnedKey, OwnedHash, OwnedEqual> moved(std::from_range,
      std::ranges::subrange(std::make_move_iterator(owned.begin()), std::make_move_iterator(owned.end())));
  EXPECT_EQ(moved.size(), 2u);
  EXPECT_FALSE(owned[0].payload);
  EXPECT_FALSE(owned[1].payload);
}

TEST(SetStorage, SharedTransparentLookupRetainsMutableAndContextualCallables) {
  static_assert(transparent_hash_lookup<MutableHash, MutableEqual, int, Query>);
  FlatHashSet<int, MutableHash, MutableEqual> values;
  values.insert(4);
  EXPECT_NE(values.find(Query{4}), values.end());
  EXPECT_EQ(std::as_const(values).count(Query{4}), 1u);
  EXPECT_FALSE(values.contains(Query{5}));

  FlatHashMap<int, int, MutableHash, MutableEqual> map;
  map.emplace(4, 40);
  auto found = std::as_const(map).find(Query{4});
  ASSERT_NE(found, map.end());
  EXPECT_EQ(found->second, 40);

  using Table = FlatHashTable<std::pair<int, int>, int, MutableGetKey, MutableHash,
      MutableStoredEqual, std::allocator<std::pair<int, int>>>;
  Table table(0, {}, {}, {}, {});
  table.insert({4, 40});
  auto entry = table.find(Query{4});
  ASSERT_NE(entry, table.end());
  EXPECT_EQ(entry->second, 40);
}

TEST(SetStorage, MapAndTableRangeEraseShareTheCorrectedCollisionAlgorithm) {
  FlatHashMap<int, int, StorageHash> map(0, StorageHash{6, 2});
  for (int key = 0; key != 5; ++key)
    map.emplace(key, key * 10);
  std::vector<int> order;
  for (auto&& entry : map)
    order.push_back(entry.first);
  auto last = std::next(map.begin(), 3);
  auto result = map.erase(std::next(map.begin()), last);
  ASSERT_NE(result, map.end());
  EXPECT_EQ(result->first, order[3]);
  EXPECT_EQ(map.size(), 3u);
  for (std::size_t index = 0; index != order.size(); ++index) {
    auto found = map.find(order[index]);
    if (index == 1 || index == 2)
      EXPECT_EQ(found, map.end());
    else {
      ASSERT_NE(found, map.end());
      EXPECT_EQ(found->second, order[index] * 10);
    }
  }

  using Table = FlatHashTable<std::pair<int, int>, int, PairKey, StorageHash,
      std::equal_to<int>, std::allocator<std::pair<int, int>>>;
  Table table(0, {}, StorageHash{6, 2}, {}, {});
  for (int key = 0; key != 5; ++key)
    table.insert({key, key * 10});
  auto first_entry = table.begin();
  ++first_entry;
  auto last_entry = first_entry;
  ++last_entry;
  ++last_entry;
  auto expected = last_entry->first;
  auto next = table.erase(first_entry, last_entry);
  ASSERT_NE(next, table.end());
  EXPECT_EQ(next->first, expected);
  EXPECT_EQ(table.size(), 3u);
}

TEST(SetStorage, StandardPolicyInspectionIsConstAndConditionallyNoexcept) {
  using Storage = FlatHashSet<int, StorageHash, std::equal_to<int>, StorageAllocator<int>>;
  static_assert(noexcept(std::declval<Storage const&>().get_allocator()));
  static_assert(noexcept(std::declval<Storage const&>().hash_function()));
  static_assert(noexcept(std::declval<Storage const&>().key_eq()));
  using Map = FlatHashMap<int, int, StorageHash>;
  static_assert(noexcept(std::declval<Map const&>().get_allocator()));
  static_assert(noexcept(std::declval<Map const&>().hash_function()));
  static_assert(noexcept(std::declval<Map const&>().key_eq()));
  Storage const values(0, StorageHash{6, 2});
  EXPECT_EQ(values.hash_function().start, 6);
  EXPECT_EQ(values.hash_function().spread, 2);
}

TEST(SetStorage, SingleEraseReturnsOriginalSuccessorAcrossWrappingClusters) {
  for (int start : {0, 6, 7}) {
    for (int count = 1; count <= 5; ++count) {
      for (int ordinal = 0; ordinal < count; ++ordinal) {
        FlatHashSet<int, StorageHash> values(0, StorageHash{start, 1});
        for (int key = 0; key < count; ++key)
          values.insert(key);
        auto position = std::next(values.cbegin(), ordinal);
        int erased = *position;
        auto next = position;
        ++next;
        std::optional<int> next_key;
        if (next != values.cend())
          next_key = *next;
        auto returned = values.erase(position);
        ASSERT_EQ(values.size(), static_cast<std::size_t>(count - 1));
        if (next_key) {
          ASSERT_NE(returned, values.end());
          EXPECT_EQ(*returned, *next_key);
        } else {
          EXPECT_EQ(returned, values.end());
        }
        for (int key = 0; key < count; ++key)
          EXPECT_EQ(values.contains(key), key != erased);
      }
    }
  }
}
