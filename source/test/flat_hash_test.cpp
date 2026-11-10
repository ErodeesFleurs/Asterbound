import star.text_formatter;
import std;
import star.format;
import star.flat_hash_set;
import star.flat_hash_map;
import star.flat_hash_table;
import star.random;
import star.vector;
import star.map;
import star.set;
import star.ordered_map;
import star.ordered_set;
import star.string;
import star.string_view;
import star.color;

#include "gtest/gtest.h"

import star.list;
using namespace Star;

namespace {
constexpr unsigned RandomSampleCount = 4096;

struct ParsedMapEntry {
  int key = 0;
  operator std::pair<int, int>() const { return {key, key * 2}; }
  friend std::istream& operator>>(std::istream& input, ParsedMapEntry& entry) {
    return input >> entry.key;
  }
};
struct RemappedMapEntry {
  int first;
  operator std::pair<int, int>() const { return {first + 100, first * 2}; }
};
struct CollisionHash {
  std::size_t operator()(int) const { return 0; }
};

template <std::size_t Alignment> struct alignas(Alignment) FlatMoveOnly {
  inline static int live_count = 0;
  int id;
  std::unique_ptr<int> ownership;

  explicit FlatMoveOnly(int value) : id(value), ownership(std::make_unique<int>(value)) { ++live_count; }
  FlatMoveOnly(FlatMoveOnly&& other) noexcept : id(other.id), ownership(std::move(other.ownership)) { ++live_count; }
  FlatMoveOnly(FlatMoveOnly const&) = delete;
  FlatMoveOnly& operator=(FlatMoveOnly const&) = delete;
  FlatMoveOnly& operator=(FlatMoveOnly&&) = delete;
  ~FlatMoveOnly() { --live_count; }
};

using FlatMoveKey = FlatMoveOnly<64>;
using FlatMoveValue = FlatMoveOnly<128>;

struct FlatWrapHash {
  using is_transparent = void;
  std::size_t operator()(int key) const { return 6 + key % 2; }
  std::size_t operator()(FlatMoveKey const& key) const { return (*this)(key.id); }
};

struct FlatMoveEqual {
  using is_transparent = void;
  bool operator()(FlatMoveKey const& left, FlatMoveKey const& right) const { return left.id == right.id; }
  bool operator()(FlatMoveKey const& left, int right) const { return left.id == right; }
};

struct FlatAllocatorState {
  int outstanding = 0;
  bool mismatched = false;
};

auto& flat_allocations() {
  static std::unordered_map<void*, FlatAllocatorState*> allocations;
  return allocations;
}

template <typename T> struct FlatTrackingAllocator {
  using value_type = T;
  using propagate_on_container_move_assignment = std::true_type;
  FlatAllocatorState* state = nullptr;

  FlatTrackingAllocator() = default;
  explicit FlatTrackingAllocator(FlatAllocatorState& owner) : state(&owner) {}
  template <typename U> FlatTrackingAllocator(FlatTrackingAllocator<U> const& other) : state(other.state) {}

  T* allocate(std::size_t count) {
    auto result = std::allocator<T>{}.allocate(count);
    flat_allocations().emplace(result, state);
    if (state)
      ++state->outstanding;
    return result;
  }
  void deallocate(T* memory, std::size_t count) {
    auto found = flat_allocations().find(memory);
    EXPECT_NE(found, flat_allocations().end());
    if (found != flat_allocations().end()) {
      if (auto owner = found->second) {
        owner->mismatched |= owner != state;
        --owner->outstanding;
      }
      flat_allocations().erase(found);
    }
    std::allocator<T>{}.deallocate(memory, count);
  }
  template <typename U> bool operator==(FlatTrackingAllocator<U> const& other) const { return state == other.state; }
};

struct FlatStatefulHash {
  int salt;
  explicit FlatStatefulHash(int value) : salt(value) {}
  std::size_t operator()(int value) const { return std::hash<int>{}(value % 1000) ^ salt; }
};

struct FlatStatefulEqual {
  int modulus;
  explicit FlatStatefulEqual(int value) : modulus(value) {}
  bool operator()(int left, int right) const { return left % modulus == right % modulus; }
};

struct FlatHashCounts {
  std::array<int, 160> calls{};
};

struct FlatCountingHash {
  FlatHashCounts* counts;
  std::size_t salt;

  std::size_t operator()(int key) const {
    ++counts->calls[key];
    return (6 + key % 2) ^ salt;
  }
};

struct FlatPairKey {
  int const& operator()(std::pair<int, int> const& value) const { return value.first; }
};

struct FlatContextualEqualResult {
  bool matches;
  explicit operator bool() const { return matches; }
};

struct FlatMutableEqual {
  FlatContextualEqualResult operator()(int& left, int const& right) { return {left == right}; }
};

struct FlatMutableHash {
  std::size_t operator()(int key) { return key; }
};

struct FlatMutablePairKey {
  int& operator()(std::pair<int, int>& value) { return value.first; }
};

template <typename Table, typename Query>
concept flat_lookup_available = requires(Table& table, Query const& query) { table.find(query); };
}// namespace

TEST(FlatHashMap, ProxyReferencesPreserveStructuredBindingsAndSnapshots) {
  using MapType = FlatHashMap<int, int, CollisionHash>;
  static_assert(std::forward_iterator<MapType::iterator>);
  static_assert(std::forward_iterator<MapType::const_iterator>);
  static_assert(std::ranges::forward_range<MapType>);
  static_assert(std::ranges::forward_range<MapType const>);
  static_assert(std::same_as<std::iter_value_t<MapType::iterator>, MapType::value_type>);
  static_assert(std::same_as<std::iter_reference_t<MapType::iterator>, std::pair<int const&, int&>>);
  static_assert(std::same_as<std::iter_reference_t<MapType::const_iterator>, std::pair<int const&, int const&>>);
  static_assert(!std::is_assignable_v<decltype((std::declval<MapType::reference>().first)), int>);

  MapType values{{1, 10}, {2, 20}, {3, 30}};
  auto first = values.begin();
  auto position = first;
  ++position;
  EXPECT_EQ(first->second, 10);
  for (auto&& [key, value] : values)
    value += key;
  auto borrowed = *values.find(2);
  borrowed.second = 99;
  EXPECT_EQ(values.find(2)->second, 99);
  EXPECT_EQ(std::as_const(values).find(2)->second, 99);

  std::vector<MapType::value_type> snapshot;
  std::ranges::copy(std::as_const(values), std::back_inserter(snapshot));
  std::vector<int> keys;
  std::ranges::copy(values | std::views::keys, std::back_inserter(keys));
  std::vector<int> mapped;
  std::ranges::copy(std::as_const(values) | std::views::values, std::back_inserter(mapped));
  values.clear();
  ASSERT_EQ(snapshot.size(), 3u);
  EXPECT_EQ(snapshot[0].second, 11);
  EXPECT_EQ(snapshot[1].second, 99);
  EXPECT_EQ(snapshot[2].second, 33);
  EXPECT_EQ(keys, (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(mapped, (std::vector<int>{11, 99, 33}));
}

TEST(FlatHashMap, NonassignableAlignedOwnershipSurvivesDisplacementRehashAndErase) {
  using MapType = FlatHashMap<FlatMoveKey, FlatMoveValue, FlatWrapHash, FlatMoveEqual>;
  static_assert(std::forward_iterator<MapType::iterator>);
  static_assert(std::forward_iterator<MapType::const_iterator>);
  EXPECT_EQ(FlatMoveKey::live_count, 0);
  EXPECT_EQ(FlatMoveValue::live_count, 0);
  {
    MapType values;
    for (int key : {1, 0, 2, 3})
      ASSERT_TRUE(values.try_emplace(FlatMoveKey(key), key * 10).second);
    values.reserve(100);
    for (int key = 4; key < 40; ++key)
      ASSERT_TRUE(values.try_emplace(FlatMoveKey(key), key * 10).second);
    FlatMoveKey duplicate_key(2);
    FlatMoveValue duplicate_value(999);
    auto duplicate = values.try_emplace(std::move(duplicate_key), std::move(duplicate_value));
    EXPECT_FALSE(duplicate.second);
    EXPECT_TRUE(duplicate_key.ownership);
    EXPECT_TRUE(duplicate_value.ownership);
    EXPECT_EQ(duplicate.first->second.id, 20);
    for (auto&& [key, value] : values) {
      EXPECT_EQ(reinterpret_cast<std::uintptr_t>(std::addressof(key)) % alignof(FlatMoveKey), 0u);
      EXPECT_EQ(reinterpret_cast<std::uintptr_t>(std::addressof(value)) % alignof(FlatMoveValue), 0u);
      ASSERT_TRUE(key.ownership);
      ASSERT_TRUE(value.ownership);
      EXPECT_EQ(*key.ownership, key.id);
      EXPECT_EQ(*value.ownership, key.id * 10);
    }
    for (int key = 0; key < 40; key += 2) {
      auto found = values.find(key);
      ASSERT_NE(found, values.end());
      values.erase(found);
    }
    for (int key = 0; key < 40; ++key)
      EXPECT_EQ(values.find(key) != values.end(), key % 2 != 0);
    MapType moved(std::move(values));
    EXPECT_TRUE(values.empty());
    EXPECT_EQ(values.begin(), values.end());
    EXPECT_EQ(moved.size(), 20u);
    moved.clear();
    EXPECT_EQ(FlatMoveKey::live_count, 1);
    EXPECT_EQ(FlatMoveValue::live_count, 1);
  }
  EXPECT_EQ(FlatMoveKey::live_count, 0);
  EXPECT_EQ(FlatMoveValue::live_count, 0);
}

TEST(FlatHashSet, NonassignableAlignedKeysSurviveCollisionRelocation) {
  EXPECT_EQ(FlatMoveKey::live_count, 0);
  {
    FlatHashSet<FlatMoveKey, FlatWrapHash, FlatMoveEqual> values;
    static_assert(std::same_as<decltype(*values.begin()), FlatMoveKey const&>);
    static_assert(std::same_as<decltype(*values.find(1)), FlatMoveKey const&>);
    for (int key : {1, 0, 2, 3})
      ASSERT_TRUE(values.insert(FlatMoveKey(key)).second);
    values.erase(values.find(0));
    values.reserve(100);
    for (int key : {1, 2, 3}) {
      auto found = values.find(key);
      ASSERT_NE(found, values.end());
      ASSERT_TRUE(found->ownership);
      EXPECT_EQ(*found->ownership, key);
    }
    values.clear();
  }
  EXPECT_EQ(FlatMoveKey::live_count, 0);
}

TEST(FlatHashMap, ReboundAllocatorsAndFunctorStateSurviveCopiesAndMoves) {
  FlatAllocatorState first_state;
  FlatAllocatorState second_state;
  using Allocator = FlatTrackingAllocator<int>;
  using MapType = FlatHashMap<int, int, FlatStatefulHash, FlatStatefulEqual, Allocator>;
  {
    MapType original(0, FlatStatefulHash(17), FlatStatefulEqual(1000), Allocator(first_state));
    for (int key = 0; key < 40; ++key)
      original.try_emplace(key, key * 10);
    original.reserve(200);
    MapType copy(original, Allocator(second_state));
    EXPECT_EQ(copy.get_allocator().state, &second_state);
    EXPECT_EQ(copy.hash_function().salt, 17);
    EXPECT_EQ(copy.key_eq().modulus, 1000);
    EXPECT_EQ(copy.find(1003)->second, 30);
    copy = copy;
    EXPECT_EQ(copy.size(), 40u);
    MapType moved(std::move(copy), Allocator(first_state));
    EXPECT_EQ(moved.get_allocator().state, &first_state);
    EXPECT_TRUE(copy.empty());
    EXPECT_EQ(copy.begin(), copy.end());
    EXPECT_EQ(moved.find(1003)->second, 30);
    MapType assigned(0, FlatStatefulHash(9), FlatStatefulEqual(1000), Allocator(second_state));
    assigned = original;
    EXPECT_EQ(assigned.get_allocator().state, &second_state);
    EXPECT_EQ(assigned.hash_function().salt, 17);
    EXPECT_EQ(assigned.find(1003)->second, 30);
    assigned = std::move(moved);
    EXPECT_EQ(assigned.get_allocator().state, &first_state);
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(assigned.find(1003)->second, 30);
  }
  EXPECT_EQ(first_state.outstanding, 0);
  EXPECT_EQ(second_state.outstanding, 0);
  EXPECT_FALSE(first_state.mismatched);
  EXPECT_FALSE(second_state.mismatched);
}

TEST(FlatHashMap, HashesEachInsertionOnceAndReusesStoredHashesAcrossGrowth) {
  FlatHashCounts counts;
  using MapType = FlatHashMap<int, int, FlatCountingHash>;
  MapType values(0, FlatCountingHash{&counts, 0x1000});

  // Keys share two target buckets and wrap at the initial capacity. The later
  // insertions cross several growth boundaries and displace live entries.
  for (int key = 0; key < 120; ++key) {
    auto inserted = key < 80 ? values.try_emplace(key, key * 10) : values.insert(MapType::value_type{key, key * 10});
    ASSERT_TRUE(inserted.second);
    EXPECT_EQ(inserted.first->first, key);
    EXPECT_EQ(inserted.first->second, key * 10);
    for (int previous = 0; previous <= key; ++previous)
      EXPECT_EQ(counts.calls[previous], 1);
  }

  auto before_reserve = counts.calls;
  values.reserve(1000);
  EXPECT_EQ(counts.calls, before_reserve);
  EXPECT_EQ(values.hash_function().salt, 0x1000u);
  EXPECT_EQ(values.size(), 120u);

  for (int key = 0; key < 120; ++key) {
    auto duplicate = values.try_emplace(key, -1);
    EXPECT_FALSE(duplicate.second);
    ASSERT_NE(duplicate.first, values.end());
    EXPECT_EQ(duplicate.first->first, key);
    EXPECT_EQ(duplicate.first->second, key * 10);
    EXPECT_EQ(counts.calls[key], 2);
  }
  EXPECT_EQ(values.find(150), values.end());
  EXPECT_EQ(counts.calls[150], 1);
  for (int key = 0; key < 120; ++key) {
    auto found = std::as_const(values).find(key);
    ASSERT_NE(found, values.end());
    EXPECT_EQ(found->second, key * 10);
    EXPECT_EQ(counts.calls[key], 3);
  }

  // Backshift deletion followed by reinsertion must retain the Robin Hood
  // lookup invariant as well as the mapped value attached to every key.
  for (int key = 0; key < 120; key += 3)
    values.erase(values.find(key));
  for (int key = 0; key < 120; key += 3) {
    auto before = counts.calls;
    auto inserted = values.try_emplace(key, key * 10);
    ASSERT_TRUE(inserted.second);
    EXPECT_EQ(inserted.first->second, key * 10);
    ++before[key];
    EXPECT_EQ(counts.calls, before);
  }
  for (auto&& [key, value] : values)
    EXPECT_EQ(value, key * 10);
  EXPECT_EQ(values.size(), 120u);
  for (int key = 0; key < 120; ++key) {
    auto found = values.find(key);
    ASSERT_NE(found, values.end());
    EXPECT_EQ(found->second, key * 10);
  }
}

TEST(FlatHashTable, InsertWithDoesNotConsumeFactoryOnDuplicate) {
  using Value = std::pair<int, int>;
  using Table = FlatHashTable<Value, int, FlatPairKey, FlatCountingHash, std::equal_to<int>, std::allocator<Value>>;
  FlatHashCounts counts;
  Table values(0, FlatPairKey{}, FlatCountingHash{&counts, 0x1000}, std::equal_to<int>{}, {});
  int factory_calls = 0;

  for (int key = 0; key < 40; ++key) {
    auto inserted = values.insert_with(key, [&] {
      ++factory_calls;
      return Value{key, key * 10};
    });
    ASSERT_TRUE(inserted.second);
    EXPECT_EQ(inserted.first->second, key * 10);
    EXPECT_EQ(counts.calls[key], 1);
  }
  EXPECT_EQ(factory_calls, 40);

  auto candidate = [owner = std::make_unique<int>(999), &factory_calls]() mutable {
    ++factory_calls;
    auto consumed = std::move(owner);
    return Value{2, *consumed};
  };
  auto duplicate = values.insert_with(2, std::move(candidate));
  EXPECT_FALSE(duplicate.second);
  EXPECT_EQ(duplicate.first->second, 20);
  EXPECT_EQ(factory_calls, 40);
  EXPECT_EQ(counts.calls[2], 2);
  auto unused = candidate();
  EXPECT_EQ(unused.second, 999);
  EXPECT_EQ(factory_calls, 41);

  auto before_reserve = counts.calls;
  values.reserve(1000);
  EXPECT_EQ(counts.calls, before_reserve);
  for (auto const& [key, value] : values)
    EXPECT_EQ(value, key * 10);
}

TEST(FlatHashTable, FactoryMayGrowTableAndInsertTheSameKey) {
  using Value = std::pair<int, int>;
  using Table = FlatHashTable<Value, int, FlatPairKey, CollisionHash, std::equal_to<int>, std::allocator<Value>>;
  Table values(0, {}, {}, {}, {});
  for (int key = 0; key != 5; ++key)
    values.insert({key, key * 10});

  auto result = values.insert_with(7, [&] {
    values.reserve(200);
    values.insert({7, 70});
    return Value{7, 999};
  });
  EXPECT_FALSE(result.second);
  EXPECT_EQ(result.first->second, 70);
  EXPECT_EQ(values.size(), 6u);
  for (int key = 0; key != 5; ++key) {
    auto found = values.find(key);
    ASSERT_NE(found, values.end());
    EXPECT_EQ(found->second, key * 10);
  }
}

TEST(FlatHashMap, OversizedReserveTerminatesBeforeCapacityWraps) {
  using MapType = FlatHashMap<int, int>;
  EXPECT_DEATH({
    MapType values;
    values.reserve(std::numeric_limits<std::size_t>::max());
  }, ".*");
}

TEST(FlatHashTable, LookupConstraintsMatchMutableCallsAndContextualEquality) {
  using Value = std::pair<int, int>;
  using Table = FlatHashTable<Value, int, FlatMutablePairKey, FlatMutableHash, FlatMutableEqual, std::allocator<Value>>;
  static_assert(flat_lookup_available<Table, int>);
  static_assert(flat_lookup_available<Table const, int>);
  static_assert(!flat_lookup_available<Table, std::monostate>);
  static_assert(!flat_lookup_available<Table const, std::monostate>);
  Table values(0, {}, {}, {}, {});
  ASSERT_TRUE(values.insert_with(7, [] { return Value{7, 70}; }).second);
  auto found = values.find(7);
  ASSERT_NE(found, values.end());
  EXPECT_EQ(found->second, 70);
  auto const_found = std::as_const(values).find(7);
  ASSERT_NE(const_found, std::as_const(values).end());
  EXPECT_EQ(const_found->second, 70);
  EXPECT_EQ(values.find(8), values.end());
}

TEST(FlatHashMap, SinglePassInputPreservesEveryElement) {
  std::istringstream input("1 2 3 4");
  FlatHashMap<int, int> values;
  values.insert(std::istream_iterator<ParsedMapEntry>(input), {});
  EXPECT_EQ(values.size(), 4u);
  for (int key = 1; key <= 4; ++key) {
    auto value = values.at(key);
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), key * 2);
  }
  std::istringstream setInput("1 2 3 4");
  FlatHashSet<int> keys(std::istream_iterator<int>(setInput), {});
  EXPECT_EQ(keys.size(), 4u);
  for (int key = 1; key <= 4; ++key)
    EXPECT_NE(keys.find(key), keys.end());
}

TEST(FlatHashMap, ConvertingEntriesUseTheMaterializedKey) {
  FlatHashMap<int, int, CollisionHash> values;
  auto inserted = values.insert(RemappedMapEntry{7});
  ASSERT_TRUE(inserted.second);
  EXPECT_EQ(inserted.first->first, 107);
  EXPECT_EQ(inserted.first->second, 14);
  auto duplicate = values.insert(RemappedMapEntry{7});
  EXPECT_FALSE(duplicate.second);
  EXPECT_EQ(duplicate.first->second, 14);
  EXPECT_EQ(values.find(7), values.end());
  EXPECT_EQ(values.size(), 1u);
}

TEST(FlatHashMap, RemoveAllValuesAcrossCollisionShift) {
  HashMap<int, int, CollisionHash> values;
  for (int key = 0; key < 40; ++key)
    values.set(key, key % 2);
  EXPECT_TRUE(values.erase_values(0));
  EXPECT_EQ(values.size(), 20u);
  for (int key = 0; key < 40; ++key)
    EXPECT_EQ(values.contains(key), key % 2 != 0);
  EXPECT_TRUE(values.erase_values(1));
  EXPECT_TRUE(values.empty());
}

TEST(FlatHashMap, TryEmplaceKeepsOwnershipOnDuplicate) {
  HashMap<int, std::unique_ptr<int>, CollisionHash> values;
  values.try_emplace(1, std::make_unique<int>(42));
  auto candidate = std::make_unique<int>(99);
  auto result = values.try_emplace(1, std::move(candidate));
  EXPECT_FALSE(result.second);
  ASSERT_TRUE(candidate);
  EXPECT_EQ(*candidate, 99);
  EXPECT_EQ(*result.first->second, 42);
  values.insert_or_assign(1, std::move(candidate));
  EXPECT_FALSE(candidate);
  auto stored = values.get(1);
  ASSERT_TRUE(stored);
  EXPECT_EQ(*stored->get(), 99);

  StringMap<int> keyed;
  keyed.try_emplace("present", 1);
  String key("present");
  auto assigned = keyed.insert_or_assign(std::move(key), 2);
  EXPECT_FALSE(assigned.second);
  EXPECT_EQ(key, "present");
  EXPECT_EQ(assigned.first->second, 2);
}

TEST(FlatHashMap, ExactPairDuplicateInsertionPreservesMoveOnlyOwnership) {
  FlatHashMap<int, std::unique_ptr<int>, CollisionHash> values;
  values.try_emplace(1, std::make_unique<int>(42));
  std::pair<int, std::unique_ptr<int>> candidate{1, std::make_unique<int>(99)};
  auto identity = candidate.second.get();
  auto duplicate = values.insert(std::move(candidate));
  EXPECT_FALSE(duplicate.second);
  EXPECT_EQ(candidate.second.get(), identity);
  EXPECT_EQ(*duplicate.first->second, 42);

  auto inserted = values.insert(std::pair<int, std::unique_ptr<int>>{2, std::move(candidate.second)});
  ASSERT_TRUE(inserted.second);
  EXPECT_EQ(inserted.first->second.get(), identity);
  EXPECT_FALSE(candidate.second);
}

TEST(FlatHashSet, DuplicateInsertionPreservesMoveOnlyKeyOwnership) {
  FlatHashSet<FlatMoveKey, FlatWrapHash, FlatMoveEqual> values;
  values.insert(FlatMoveKey(3));
  FlatMoveKey candidate(3);
  auto identity = candidate.ownership.get();
  auto duplicate = values.insert(std::move(candidate));
  EXPECT_FALSE(duplicate.second);
  EXPECT_EQ(candidate.ownership.get(), identity);
  EXPECT_EQ(duplicate.first->id, 3);
  ASSERT_TRUE(duplicate.first->ownership);
  EXPECT_EQ(*duplicate.first->ownership, 3);
  EXPECT_EQ(values.size(), 1u);
}

TEST(FlatHashMap, BorrowedLookupPreservesBytesAndCaseRules) {
  String key(std::string_view("x\0中", 5));
  std::string bytes = "!" + key.utf8() + "?";
  StringView query(std::string_view(bytes).substr(1, key.utf8_size()));
  StringMap<int> values{{key, 7}};
  StableStringMap<int> stable{{key, 8}};
  EXPECT_EQ(hash<String>{}(key), hash<String>{}(query));
  auto value = values.get(query);
  ASSERT_TRUE(value);
  EXPECT_EQ(value->get(), 7);
  auto stableValue = stable.get(query);
  ASSERT_TRUE(stableValue);
  EXPECT_EQ(stableValue->get(), 8);
  values.find_ref(query)->get() = 9;
  EXPECT_EQ(values.value(query), 9);
  auto maybeValue = values.maybe(query);
  ASSERT_TRUE(maybeValue);
  EXPECT_EQ(*maybeValue, 9);
  auto insensitiveKey = CaseInsensitiveString::create(String("MiXeD中"));
  ASSERT_TRUE(insensitiveKey);
  auto insensitiveQuery = CaseInsensitiveStringView::create(std::string_view("mixed中"));
  ASSERT_TRUE(insensitiveQuery);
  EXPECT_EQ(CaseInsensitiveStringHash()(*insensitiveKey), CaseInsensitiveStringHash()(*insensitiveQuery));
  EXPECT_TRUE(CaseInsensitiveStringCompare()(*insensitiveKey, *insensitiveQuery));
  EXPECT_TRUE(CaseInsensitiveStringCompare()(*insensitiveQuery, *insensitiveKey));
  CaseInsensitiveStringMap<int> insensitive{{*insensitiveKey, 10}};
  auto insensitiveValue = insensitive.get(*insensitiveQuery);
  ASSERT_TRUE(insensitiveValue);
  EXPECT_EQ(insensitiveValue->get(), 10);
  EXPECT_EQ(Color::create(StringView("RED")), Color::Red);
}

TEST(StringSet, BorrowedLookupPreservesFullByteExtent) {
  std::string bytes(192, 'x');
  bytes[11] = '\0';
  bytes[37] = static_cast<char>(0x80);
  bytes[111] = static_cast<char>(0xff);
  String key{std::string_view(bytes)};
  StringSet values{key};
  std::string padded = "!" + bytes + "?";
  std::string_view query(padded.data() + 1, bytes.size());
  StringView star_query(query);

  auto found = values.find(query);
  ASSERT_NE(found, values.end());
  EXPECT_EQ(std::addressof(*found), std::addressof(*values.begin()));
  EXPECT_EQ(found->utf8(), bytes);
  EXPECT_TRUE(values.contains(query));
  EXPECT_EQ(values.count(query), 1u);
  EXPECT_NE(values.find(star_query), values.end());
  EXPECT_TRUE(values.contains(star_query));
  EXPECT_EQ(values.count(star_query), 1u);
  auto const& const_values = values;
  EXPECT_NE(const_values.find(query), const_values.end());
  EXPECT_NE(const_values.find(star_query), const_values.end());
  EXPECT_TRUE(const_values.contains(query));
  EXPECT_TRUE(const_values.contains(star_query));
  EXPECT_EQ(const_values.count(query), 1u);
  EXPECT_EQ(const_values.count(star_query), 1u);

  // A C-string prefix is not the explicit-length key. Neither NUL nor high
  // bytes may terminate or normalize the hash/equality comparison.
  EXPECT_FALSE(values.contains(query.substr(0, 11)));
  padded[112] = static_cast<char>(0xfe);
  EXPECT_EQ(values.find(query), values.end());
  EXPECT_FALSE(const_values.contains(star_query));
  EXPECT_EQ(const_values.count(star_query), 0u);
  EXPECT_EQ(values.size(), 1u);
  EXPECT_EQ(values.begin()->utf8(), bytes);
}

TEST(FlatHashSet, Preset) {
  FlatHashSet<int> testSet = {42, 63};
  ASSERT_EQ(testSet.find(41), testSet.end());
  ASSERT_EQ(*testSet.find(42), 42);
  ASSERT_EQ(*testSet.find(63), 63);
  ASSERT_EQ(testSet.find(64), testSet.end());
  ASSERT_EQ(testSet.size(), 2u);

  testSet.erase(testSet.find(42));
  ASSERT_EQ(testSet.find(42), testSet.end());
  ASSERT_EQ(*testSet.find(63), 63);
  ASSERT_EQ(testSet.size(), 1u);

  testSet.erase(testSet.find(63));
  ASSERT_EQ(testSet.find(42), testSet.end());
  ASSERT_EQ(testSet.find(63), testSet.end());
  ASSERT_EQ(testSet.size(), 0u);

  testSet.insert(12);
  testSet.insert(24);
  ASSERT_EQ(*testSet.find(12), 12);
  ASSERT_EQ(testSet.size(), 2u);
  testSet.clear();

  ASSERT_EQ(testSet.find(12), testSet.end());
  ASSERT_EQ(testSet.size(), 0u);

  EXPECT_TRUE(testSet.insert(7).second);
  EXPECT_TRUE(testSet.insert(11).second);
  EXPECT_FALSE(testSet.insert(7).second);

  ASSERT_EQ(testSet.size(), 2u);

  FlatHashSet<int> testSet2(testSet.begin(), testSet.end());
  ASSERT_EQ(testSet, testSet2);

  ASSERT_EQ(testSet.erase(testSet.begin(), testSet.end()), testSet.end());
  ASSERT_EQ(testSet.size(), 0u);

  ASSERT_NE(testSet, testSet2);

  FlatHashSet<int> testSet3(testSet.begin(), testSet.end());
  ASSERT_EQ(testSet3.size(), 0u);

  testSet2 = testSet;
  ASSERT_EQ(testSet, testSet2);
}

TEST(FlatHashSet, Random) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  List<Vec2I> keys;
  for (unsigned i = 0; i < RandomSampleCount; ++i)
    keys.push_back(Vec2I(i * 743202097, i * 205495087));
  random.shuffle(keys);

  FlatHashSet<Vec2I> testSet;
  for (std::size_t i = 0; i < keys.size(); ++i)
    testSet.insert(keys[i]);

  random.shuffle(keys);
  for (std::size_t i = 0; i < keys.size() / 2; ++i)
    testSet.erase(keys[i]);

  random.shuffle(keys);
  for (std::size_t i = 0; i < keys.size() / 3; ++i)
    testSet.insert(keys[i]);

  random.shuffle(keys);
  for (std::size_t i = 0; i < keys.size() / 2; ++i)
    testSet.erase(keys[i]);

  random.shuffle(keys);
  for (auto k : keys) {
    auto i = testSet.find(k);
    if (i != testSet.end()) {
      ASSERT_TRUE(*i == k);
    }
  }

  random.shuffle(keys);
  for (auto k : keys) {
    testSet.insert(k);
    ASSERT_TRUE(testSet.find(k) != testSet.end());
  }

  List<Vec2I> cmp;
  for (auto const& k : testSet)
    cmp.push_back(k);
  cmp.sort();
  keys.sort();
  ASSERT_TRUE(cmp == keys);

  random.shuffle(keys);
  for (auto k : keys) {
    testSet.erase(k);
    ASSERT_TRUE(testSet.find(k) == testSet.end());
  }

  ASSERT_TRUE(testSet.empty());
}

TEST(FlatHashMap, Preset) {
  FlatHashMap<int, int> testMap = {{42, 42}, {63, 63}};
  ASSERT_EQ(testMap.find(41), testMap.end());
  ASSERT_EQ(testMap.find(42)->second, 42);
  ASSERT_EQ(testMap.find(63)->second, 63);
  ASSERT_EQ(testMap.find(64), testMap.end());
  ASSERT_EQ(testMap.size(), 2u);

  testMap.erase(testMap.find(42));
  ASSERT_EQ(testMap.find(42), testMap.end());
  ASSERT_EQ(testMap.find(63)->second, 63);
  ASSERT_EQ(testMap.size(), 1u);

  testMap.erase(testMap.find(63));
  ASSERT_EQ(testMap.find(42), testMap.end());
  ASSERT_EQ(testMap.find(63), testMap.end());
  ASSERT_EQ(testMap.size(), 0u);

  testMap.insert({12, 12});
  testMap.insert({24, 24});
  ASSERT_EQ(testMap.find(12)->second, 12);
  ASSERT_EQ(testMap.size(), 2u);
  testMap.clear();

  ASSERT_EQ(testMap.find(12), testMap.end());
  ASSERT_EQ(testMap.size(), 0u);

  EXPECT_TRUE(testMap.insert({7, 7}).second);
  EXPECT_TRUE(testMap.insert({11, 11}).second);
  EXPECT_FALSE(testMap.insert({7, 7}).second);

  ASSERT_EQ(testMap.size(), 2u);

  FlatHashMap<int, int> testMap2(testMap.begin(), testMap.end());
  ASSERT_EQ(testMap, testMap2);

  ASSERT_EQ(testMap.erase(testMap.begin(), testMap.end()), testMap.end());
  ASSERT_EQ(testMap.size(), 0u);

  ASSERT_NE(testMap, testMap2);

  FlatHashMap<int, int> testMap3(testMap.begin(), testMap.end());
  ASSERT_EQ(testMap3.size(), 0u);

  testMap2 = testMap;
  ASSERT_EQ(testMap, testMap2);
}

TEST(FlatHashMap, Random) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  List<std::pair<Vec2I, int>> values;
  for (unsigned i = 0; i < RandomSampleCount; ++i)
    values.push_back({Vec2I(i * 743202097, i * 205495087), i});
  random.shuffle(values);

  FlatHashMap<Vec2I, int> testMap;
  for (auto v : values)
    testMap.insert(v);

  random.shuffle(values);
  for (std::size_t i = 0; i < values.size() / 2; ++i)
    testMap.erase(values[i].first);

  random.shuffle(values);
  for (std::size_t i = 0; i < values.size() / 3; ++i)
    testMap.insert(values[i]);

  random.shuffle(values);
  for (std::size_t i = 0; i < values.size() / 2; ++i)
    testMap.erase(values[i].first);

  random.shuffle(values);
  for (auto v : values) {
    auto i = testMap.find(v.first);
    if (i != testMap.end()) {
      ASSERT_TRUE(i->second == v.second);
    }
  }

  random.shuffle(values);
  for (auto v : values) {
    ASSERT_TRUE(testMap.insert(v).first->second == v.second);
    auto value = testMap.at(v.first);
    ASSERT_TRUE(value);
    ASSERT_EQ(value->get(), v.second);
  }

  random.shuffle(values);
  for (auto v : values) {
    ASSERT_EQ(testMap.erase(v.first), 1u);
    ASSERT_TRUE(testMap.find(v.first) == testMap.end());
  }

  ASSERT_TRUE(testMap.empty());
}

TEST(FlatHashMap, EraseWhileIterating) {
  List<std::pair<Vec2I, int>> values;
  for (unsigned i = 0; i < RandomSampleCount; ++i)
    values.push_back({Vec2I(i * 743202097, i * 205495087), i});

  FlatHashMap<Vec2I, int> testMap;
  for (auto v : values)
    testMap.insert(v);

  for (auto it = testMap.begin(); it != testMap.end();) {
    if (it->second % 3 == 0)
      it = testMap.erase(it);
    else
      ++it;
  }

  ASSERT_EQ(testMap.size(), values.size() - (values.size() + 2) / 3);
  for (auto const& value : values) {
    auto found = testMap.find(value.first);
    if (value.second % 3 == 0)
      ASSERT_EQ(found, testMap.end());
    else {
      ASSERT_NE(found, testMap.end());
      EXPECT_EQ(found->second, value.second);
    }
  }
}

TEST(FlatHashMap, NonDefaultAlignedValueLifetime) {
  struct alignas(32) Payload {
    Payload() = delete;
    explicit Payload(std::shared_ptr<int> value) : value(std::move(value)) {}
    std::shared_ptr<int> value;
  };
  struct CollisionHash {
    std::size_t operator()(int) const { return 0; }
  };
  using Map = FlatHashMap<int, Payload, CollisionHash>;

  std::array<std::weak_ptr<int>, 32> lifetimes;
  Map original;
  for (int i = 0; i < 32; ++i) {
    auto value = std::make_shared<int>(i);
    lifetimes[i] = value;
    ASSERT_TRUE(original.insert({i, Payload(std::move(value))}).second);
  }
  original.reserve(128);
  for (auto const& entry : original) {
    EXPECT_EQ(*entry.second.value, entry.first);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&entry.second) % alignof(Payload), 0u);
    EXPECT_EQ(lifetimes[entry.first].use_count(), 1);
  }

  Map copy = original;
  copy.reserve(256);
  for (int i = 0; i < 32; ++i) {
    ASSERT_NE(copy.find(i), copy.end());
    EXPECT_EQ(*copy.find(i)->second.value, i);
    EXPECT_EQ(lifetimes[i].use_count(), 2);
  }
  ASSERT_EQ(original.erase(0), 1u);
  EXPECT_EQ(lifetimes[0].use_count(), 1);

  Map moved = std::move(copy);
  ASSERT_EQ(moved.size(), 32u);
  moved.clear();
  EXPECT_TRUE(lifetimes[0].expired());
  for (int i = 1; i < 32; ++i)
    EXPECT_EQ(lifetimes[i].use_count(), 1);

  moved = original;
  for (int i = 1; i < 32; ++i)
    EXPECT_EQ(lifetimes[i].use_count(), 2);
  original.clear();
  for (int i = 1; i < 32; ++i)
    EXPECT_EQ(lifetimes[i].use_count(), 1);
  moved = original;
  for (auto const& lifetime : lifetimes)
    EXPECT_TRUE(lifetime.expired());

  auto reused = std::make_shared<int>(99);
  lifetimes[0] = reused;
  ASSERT_TRUE(original.insert({99, Payload(std::move(reused))}).second);
  ASSERT_NE(original.find(99), original.end());
  EXPECT_EQ(*original.find(99)->second.value, 99);
  original.clear();
  EXPECT_TRUE(lifetimes[0].expired());
}

namespace {

static_assert(map_container<Map<int, int>>);
static_assert(map_container<HashMap<int, int>>);
static_assert(map_container<StableHashMap<int, int>>);
static_assert(!map_container<std::map<int, int>>);
static_assert(!map_container<OrderedMap<int, int>>);
static_assert(!map_container<OrderedHashMap<int, int>>);
static_assert(set_container<Set<int>>);
static_assert(set_container<HashSet<int>>);
static_assert(set_container<StableHashSet<int>>);
static_assert(!set_container<std::set<int>>);
static_assert(!set_container<OrderedSet<int>>);
static_assert(!set_container<OrderedHashSet<int>>);
static_assert(std::same_as<HashMap<int, int>::Storage,
    FlatHashMap<int, int, hash<int>, std::equal_to<>, std::allocator<std::pair<int const, int>>>>);
static_assert(std::same_as<StableHashMap<int, int>::Storage,
    std::unordered_map<int, int, hash<int>, std::equal_to<>, std::allocator<std::pair<int const, int>>>>);
static_assert(!std::derived_from<Map<int, int>, Map<int, int>::Storage>);
static_assert(!std::derived_from<HashMap<int, int>, HashMap<int, int>::Storage>);
static_assert(!std::derived_from<StableHashMap<int, int>, StableHashMap<int, int>::Storage>);
static_assert(!std::derived_from<Set<int>, Set<int>::Storage>);
static_assert(!std::derived_from<HashSet<int>, HashSet<int>::Storage>);
static_assert(!std::derived_from<StableHashSet<int>, StableHashSet<int>::Storage>);

template <template <typename...> class MapType>
void expectMapOwnershipContract() {
  MapType<int, int> values{{1, 10}, {1, 99}, {2, 20}};
  EXPECT_EQ(values.size(), 2u);
  auto borrowed = values.get(1);
  ASSERT_TRUE(borrowed);
  EXPECT_EQ(borrowed->get(), 10);
  auto const& constValues = values;
  static_assert(std::same_as<decltype(constValues.get(1)), std::expected<std::reference_wrapper<int const>, StarException>>);
  static_assert(std::same_as<decltype(values.get(1)), std::expected<std::reference_wrapper<int>, StarException>>);
  using Container = MapType<int, int>;
  static_assert(!requires(Container&& owner) { std::move(owner).get(1); });
  static_assert(!requires(Container const&& owner) { std::move(owner).get(1); });
  static_assert(!requires(Container&& owner) { std::move(owner).at(1); });
  static_assert(!requires(Container&& owner) { std::move(owner).find(1); });
  static_assert(!requires(Container&& owner) { std::move(owner).find_ref(1); });
  static_assert(!requires(Container&& owner) { std::move(owner).key_view(); });
  static_assert(!requires(Container&& owner) { std::move(owner).value_view(); });
  auto copied = values.maybe(1);
  auto keys = values.keys();
  auto snapshot = values.values();
  auto pairs = values.pairs();
  for (auto& value : values.value_view())
    ++value;
  EXPECT_EQ(borrowed->get(), 11);
  ASSERT_TRUE(copied);
  EXPECT_EQ(*copied, 10);
  EXPECT_EQ(std::ranges::count(snapshot, 10), 1);
  EXPECT_EQ(std::ranges::count(pairs, 10, &std::pair<int, int>::second), 1);
  EXPECT_EQ(std::ranges::count(values.key_view(), 1), 1);
  EXPECT_EQ(keys.size(), 2u);
  EXPECT_EQ(values.value(9, 42), 42);
  EXPECT_FALSE(values.get(9));
  EXPECT_FALSE(values.at(9));
  EXPECT_FALSE(values.extract_value(9));
  EXPECT_FALSE(values.maybe(9));
  EXPECT_FALSE(values.try_extract_value(9));
  EXPECT_FALSE(values.add(1, 90));
  EXPECT_EQ(values.value(1), 11);
  EXPECT_TRUE(values.contains_value(11));
  EXPECT_FALSE(values.contains_value(99));
  auto key = values.key_of(11);
  ASSERT_TRUE(key);
  EXPECT_EQ(*key, 1);

  MapType<int, int> incoming{{1, 50}, {3, 30}};
  EXPECT_FALSE(values.merge(incoming));
  EXPECT_EQ(values.value(1), 11);
  EXPECT_EQ(values.value(3), 30);
  EXPECT_FALSE(values.merge(incoming, true));
  EXPECT_EQ(values.value(1), 50);
  auto taken = values.extract_value(1);
  ASSERT_TRUE(taken);
  EXPECT_EQ(*taken, 50);
  EXPECT_FALSE(values.contains(1));

  MapType<int, std::unique_ptr<int>> owners;
  owners.try_emplace(1, std::make_unique<int>(7));
  auto candidate = std::make_unique<int>(8);
  EXPECT_FALSE(owners.try_emplace(1, std::move(candidate)).second);
  ASSERT_TRUE(candidate);
  EXPECT_EQ(*candidate, 8);
  auto extracted = owners.extract_value(1);
  ASSERT_TRUE(extracted);
  ASSERT_TRUE(*extracted);
  EXPECT_EQ(**extracted, 7);
  EXPECT_TRUE(owners.empty());

  MapType<int, int> printed{{1, 2}};
  EXPECT_EQ(std::format("{}", printed), "{ \"1\" : \"2\" }");
}

template <template <typename...> class MapType> void expect_map_reference_slots() {
  int first = 10;
  int second = 20;
  using Slot = std::reference_wrapper<int>;
  MapType<int, Slot> values{{1, std::ref(first)}};
  auto slot = values.get(1);
  auto optional_slot = values.find_ref(1);
  ASSERT_TRUE(slot);
  ASSERT_TRUE(optional_slot);
  static_assert(std::same_as<decltype(slot), std::expected<std::reference_wrapper<Slot>, StarException>>);
  auto const& constant = values;
  static_assert(
    std::same_as<decltype(constant.get(1)), std::expected<std::reference_wrapper<Slot const>, StarException>>);
  static_assert(std::same_as<decltype(constant.find_ref(1)), std::optional<std::reference_wrapper<Slot const>>>);
  EXPECT_EQ(std::addressof(slot->get()), std::addressof(optional_slot->get()));
  auto snapshot = values.values();
  optional_slot->get() = std::ref(second);
  EXPECT_EQ(std::addressof(slot->get().get()), std::addressof(second));
  EXPECT_EQ(std::addressof(snapshot[0].get()), std::addressof(first));
  values.clear();
  EXPECT_EQ(snapshot[0].get(), 10);
}

template <typename T> struct CompositionAllocator {
  using value_type = T;
  int identity = 0;
  CompositionAllocator() = default;
  explicit CompositionAllocator(int identity) : identity(identity) {}
  template <typename U> CompositionAllocator(CompositionAllocator<U> const& other) : identity(other.identity) {}
  T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }
  void deallocate(T* pointer, std::size_t count) { std::allocator<T>{}.deallocate(pointer, count); }
  template <typename U> bool operator==(CompositionAllocator<U> const& other) const {
    return identity == other.identity;
  }
};

struct CompositionCompare {
  bool descending = false;
  bool operator()(int left, int right) const { return descending ? left > right : left < right; }
};
struct CompositionHash {
  int salt = 0;
  std::size_t operator()(int key) const { return hash<int>{}(key) ^ hash<int>{}(salt); }
};
struct CompositionEqual {
  int identity = 0;
  bool operator()(int left, int right) const { return left == right; }
};

template <typename Container> void expect_map_allocator_construction(Container const& original) {
  using Allocator = typename Container::allocator_type;
  Container copied(original, Allocator(2));
  EXPECT_EQ(copied, original);
  EXPECT_EQ(copied.get_allocator().identity, 2);
  Container moved(std::move(copied), Allocator(3));
  EXPECT_EQ(moved, original);
  EXPECT_EQ(moved.get_allocator().identity, 3);
  if constexpr (requires { moved.key_comp(); }) {
    EXPECT_TRUE(moved.key_comp().descending);
    EXPECT_EQ(moved.begin()->first, 2);
  } else {
    EXPECT_EQ(moved.hash_function().salt, 91);
    EXPECT_EQ(moved.key_eq().identity, 7);
  }
  moved = {{4, 40}};
  EXPECT_EQ(moved.size(), 1u);
  EXPECT_EQ(moved.value(4), 40);
  EXPECT_EQ(moved.get_allocator().identity, 3);
  Container allocated({{5, 50}}, Allocator(4));
  EXPECT_EQ(allocated.value(5), 50);
  EXPECT_EQ(allocated.get_allocator().identity, 4);
  if constexpr (requires { original.key_comp(); }) {
    Container ranged(original.begin(), original.end(), original.key_comp(), Allocator(5));
    EXPECT_EQ(ranged, original);
    EXPECT_EQ(ranged.get_allocator().identity, 5);
  } else {
    Container ranged(original.begin(), original.end(), 0, original.hash_function(), original.key_eq(), Allocator(5));
    EXPECT_EQ(ranged, original);
    EXPECT_EQ(ranged.get_allocator().identity, 5);
  }
}

template <typename Container>
void expectSetAlgebraContract() {
  Container values{1, 2, 3, 4};
  static_assert(std::same_as<decltype(*values.begin()), typename Container::value_type const&>);
  static_assert(std::same_as<std::ranges::range_reference_t<decltype(values.value_view())>, typename Container::value_type const&>);
  Container other{2, 4, 5};
  EXPECT_EQ(values.intersection(other), (Container{2, 4}));
  EXPECT_EQ(values.difference(other), (Container{1, 3}));
  EXPECT_EQ(values.combination(other), (Container{1, 2, 3, 4, 5}));
  EXPECT_TRUE(values.has_intersection(other));
  EXPECT_FALSE(values.has_intersection(Container{9}));
  auto snapshot = values.values();
  EXPECT_EQ(std::ranges::distance(values.value_view()), 4);
  EXPECT_FALSE(values.add(1));
  EXPECT_TRUE(values.remove(1));
  EXPECT_EQ(snapshot.size(), 4u);
  EXPECT_EQ(std::ranges::count(snapshot, 1), 1);
  while (!values.empty()) {
    auto taken = values.take_first();
    ASSERT_TRUE(taken);
    EXPECT_FALSE(values.contains(*taken));
  }
  EXPECT_FALSE(values.first());
  EXPECT_FALSE(values.last());
  EXPECT_FALSE(values.take_first());
  EXPECT_FALSE(values.take_last());
  EXPECT_FALSE(values.maybe_first());
  EXPECT_FALSE(values.maybe_last());
  EXPECT_FALSE(values.maybe_take_first());
  EXPECT_FALSE(values.maybe_take_last());
}

struct SetRecord {
  int key;
  int revision;
  bool operator==(SetRecord const&) const = default;
};
struct SetRecordLess {
  bool operator()(SetRecord const& a, SetRecord const& b) const { return a.key < b.key; }
};
struct SetRecordHash {
  std::size_t operator()(SetRecord const& value) const { return hash<int>{}(value.key); }
};
struct SetRecordEqual {
  bool operator()(SetRecord const& a, SetRecord const& b) const { return a.key == b.key; }
};

template <typename Container>
void expectSetReplacementContract() {
  Container values;
  EXPECT_TRUE(values.add(SetRecord{1, 10}));
  EXPECT_FALSE(values.add(SetRecord{1, 20}));
  EXPECT_EQ(values.begin()->revision, 10);
  EXPECT_TRUE(values.replace(SetRecord{1, 30}));
  EXPECT_EQ(values.begin()->revision, 30);
  EXPECT_FALSE(values.replace(SetRecord{2, 40}));
  EXPECT_EQ(values.size(), 2u);
}

struct OwnedIntLess {
  bool operator()(std::unique_ptr<int> const& a, std::unique_ptr<int> const& b) const { return *a < *b; }
};
struct OwnedIntHash {
  std::size_t operator()(std::unique_ptr<int> const& value) const { return hash<int>{}(*value); }
};
struct OwnedIntEqual {
  bool operator()(std::unique_ptr<int> const& a, std::unique_ptr<int> const& b) const { return *a == *b; }
};

template <typename Container>
void expectSetOwnershipTransfer() {
  Container values;
  auto owner = std::make_unique<int>(7);
  auto identity = owner.get();
  EXPECT_TRUE(values.add(std::move(owner)));
  EXPECT_FALSE(owner);
  auto extracted = values.take_first();
  ASSERT_TRUE(extracted);
  EXPECT_EQ(extracted->get(), identity);
  EXPECT_TRUE(values.empty());
  values.add(std::move(*extracted));
  auto last = values.maybe_take_last();
  ASSERT_TRUE(last);
  EXPECT_EQ(last->get(), identity);
  EXPECT_TRUE(values.empty());
}

struct FallibleSetValue {
  int value;
  std::reference_wrapper<std::vector<int>> visited;
  friend bool operator<(FallibleSetValue const& a, FallibleSetValue const& b) { return a.value < b.value; }
  friend std::expected<void, StarException> write_text(TextBuffer& output, FallibleSetValue const& value) {
    value.visited.get().push_back(value.value);
    if (value.value == 2)
      return std::unexpected(OutOfRangeException("first diagnostic error", false));
    append_text(output, value.value);
    return {};
  }
};

}

TEST(MapComposition, OwnsStorageAndSeparatesBorrowedLookupFromSnapshots) {
  expectMapOwnershipContract<Map>();
  expectMapOwnershipContract<HashMap>();
  expectMapOwnershipContract<StableHashMap>();
}

TEST(MapComposition, BorrowingReferenceWrappersKeepsSlotIdentity) {
  expect_map_reference_slots<Map>();
  expect_map_reference_slots<HashMap>();
  expect_map_reference_slots<StableHashMap>();
}

TEST(MapComposition, ConstructorsPreserveAllocatorAndComparisonPolicies) {
  using Allocator = CompositionAllocator<std::pair<int const, int>>;
  Map<int, int, CompositionCompare, Allocator> ordered({{1, 10}, {2, 20}}, CompositionCompare{true}, Allocator(1));
  HashMap<int, int, CompositionHash, CompositionEqual, Allocator> flat({{1, 10}, {2, 20}}, 8, CompositionHash{91},
                                                                       CompositionEqual{7}, Allocator(1));
  StableHashMap<int, int, CompositionHash, CompositionEqual, Allocator> stable(
    {{1, 10}, {2, 20}}, 8, CompositionHash{91}, CompositionEqual{7}, Allocator(1));
  expect_map_allocator_construction(ordered);
  expect_map_allocator_construction(flat);
  expect_map_allocator_construction(stable);
}

TEST(MapComposition, StableStorageKeepsBorrowedValuesAcrossRehash) {
  StableHashMap<int, int> values{{1, 10}};
  auto borrowed = values.find_ref(1);
  for (int key = 2; key < 200; ++key)
    values.set(key, key);
  values.reserve(1000);
  EXPECT_EQ(std::addressof(values.find_ref(1)->get()), std::addressof(borrowed->get()));
  EXPECT_EQ(borrowed->get(), 10);
}

TEST(SetComposition, OrderedAndHashAlgebraPreserveMembership) {
  expectSetAlgebraContract<Set<int>>();
  expectSetAlgebraContract<HashSet<int, CollisionHash>>();
  expectSetAlgebraContract<StableHashSet<int, CollisionHash>>();
  Set<int, std::greater<int>> descending{1, 2, 3, 4};
  Set<int, std::greater<int>> other{2, 4, 5};
  EXPECT_EQ(descending.intersection(other), (Set<int, std::greater<int>>{4, 2}));
  EXPECT_EQ(descending.difference(other), (Set<int, std::greater<int>>{3, 1}));
  ASSERT_TRUE(descending.first());
  EXPECT_EQ(*descending.first(), 4);
  EXPECT_EQ(*descending.last(), 1);
}

TEST(SetComposition, ReplacementUsesEquivalenceRatherThanObjectEquality) {
  expectSetReplacementContract<Set<SetRecord, SetRecordLess>>();
  expectSetReplacementContract<HashSet<SetRecord, SetRecordHash, SetRecordEqual>>();
  expectSetReplacementContract<StableHashSet<SetRecord, SetRecordHash, SetRecordEqual>>();
}

TEST(SetComposition, TakeTransfersMoveOnlyValues) {
  expectSetOwnershipTransfer<Set<std::unique_ptr<int>, OwnedIntLess>>();
  expectSetOwnershipTransfer<HashSet<std::unique_ptr<int>, OwnedIntHash, OwnedIntEqual>>();
  expectSetOwnershipTransfer<StableHashSet<std::unique_ptr<int>, OwnedIntHash, OwnedIntEqual>>();
}

TEST(MapComposition, CheckedDiagnosticStopsAtFirstValueError) {
  std::vector<int> visited;
  Map<int, FallibleSetValue> values{{1, {1, std::ref(visited)}}, {2, {2, std::ref(visited)}}, {3, {3, std::ref(visited)}}};
  TextBuffer output;
  auto printed = write_text(output, values);
  static_assert(std::same_as<decltype(printed), std::expected<void, StarException>>);
  ASSERT_FALSE(printed);
  EXPECT_TRUE(printed.error().is<OutOfRangeException>());
  EXPECT_EQ(visited, (std::vector<int>{1, 2}));
  EXPECT_EQ(output.view(), "{ \"1\" : \"1\", \"2\" : \"");
}

TEST(SetComposition, CheckedDiagnosticStopsAtFirstElementError) {
  std::vector<int> visited;
  Set<FallibleSetValue> values{{1, std::ref(visited)}, {2, std::ref(visited)}, {3, std::ref(visited)}};
  TextBuffer output;
  auto printed = write_text(output, values);
  static_assert(std::same_as<decltype(printed), std::expected<void, StarException>>);
  ASSERT_FALSE(printed);
  EXPECT_TRUE(printed.error().is<OutOfRangeException>());
  EXPECT_EQ(visited, (std::vector<int>{1, 2}));
  EXPECT_EQ(output.view(), "(1, ");

  Set<int> ordered{1, 2};
  HashSet<int> flat{1};
  StableHashSet<int> stable{1};
  EXPECT_EQ(std::format("{}", ordered), "(1, 2)");
  EXPECT_EQ(std::format("{}", flat), "(1)");
  EXPECT_EQ(std::format("{}", stable), "(1)");
}
