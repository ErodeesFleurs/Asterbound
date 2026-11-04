import std;
import star.format;
import star.flat_hash_set;
import star.flat_hash_map;
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
struct CollisionHash {
  std::size_t operator()(int) const { return 0; }
};
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

TEST(FlatHashMap, RemoveAllValuesAcrossCollisionShift) {
  HashMap<int, int, CollisionHash> values;
  for (int key = 0; key < 40; ++key)
    values.set(key, key % 2);
  EXPECT_TRUE(values.removeValues(0));
  EXPECT_EQ(values.size(), 20u);
  for (int key = 0; key < 40; ++key)
    EXPECT_EQ(values.contains(key), key % 2 != 0);
  EXPECT_TRUE(values.removeValues(1));
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

TEST(FlatHashMap, BorrowedLookupPreservesBytesAndCaseRules) {
  String key(std::string_view("x\0中", 5));
  std::string bytes = "!" + key.utf8() + "?";
  StringView query(std::string_view(bytes).substr(1, key.utf8Size()));
  StringMap<int> values{{key, 7}};
  StableStringMap<int> stable{{key, 8}};
  EXPECT_EQ(hash<String>{}(key), hash<String>{}(query));
  auto value = values.get(query);
  ASSERT_TRUE(value);
  EXPECT_EQ(value->get(), 7);
  auto stableValue = stable.get(query);
  ASSERT_TRUE(stableValue);
  EXPECT_EQ(stableValue->get(), 8);
  values.maybeRef(query)->get() = 9;
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
    keys.append(Vec2I(i * 743202097, i * 205495087));
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
    cmp.append(k);
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
    values.append({Vec2I(i * 743202097, i * 205495087), i});
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
    values.append({Vec2I(i * 743202097, i * 205495087), i});

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

static_assert(MapContainer<Map<int, int>>);
static_assert(MapContainer<HashMap<int, int>>);
static_assert(MapContainer<StableHashMap<int, int>>);
static_assert(!MapContainer<std::map<int, int>>);
static_assert(!MapContainer<OrderedMap<int, int>>);
static_assert(!MapContainer<OrderedHashMap<int, int>>);
static_assert(SetContainer<Set<int>>);
static_assert(SetContainer<HashSet<int>>);
static_assert(SetContainer<StableHashSet<int>>);
static_assert(!SetContainer<std::set<int>>);
static_assert(!SetContainer<OrderedSet<int>>);
static_assert(!SetContainer<OrderedHashSet<int>>);
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
  auto copied = values.maybe(1);
  auto keys = values.keys();
  auto snapshot = values.values();
  auto pairs = values.pairs();
  for (auto& value : values.valueView())
    ++value;
  EXPECT_EQ(borrowed->get(), 11);
  ASSERT_TRUE(copied);
  EXPECT_EQ(*copied, 10);
  EXPECT_EQ(std::ranges::count(snapshot, 10), 1);
  EXPECT_EQ(std::ranges::count(pairs, 10, &std::pair<int, int>::second), 1);
  EXPECT_EQ(std::ranges::count(values.keyView(), 1), 1);
  EXPECT_EQ(keys.size(), 2u);
  EXPECT_EQ(values.value(9, 42), 42);
  EXPECT_FALSE(values.get(9));
  EXPECT_FALSE(values.at(9));
  EXPECT_FALSE(values.take(9));
  EXPECT_FALSE(values.maybe(9));
  EXPECT_FALSE(values.maybeTake(9));
  EXPECT_FALSE(values.add(1, 90));
  EXPECT_EQ(values.value(1), 11);

  MapType<int, int> incoming{{1, 50}, {3, 30}};
  EXPECT_FALSE(values.merge(incoming));
  EXPECT_EQ(values.value(1), 11);
  EXPECT_EQ(values.value(3), 30);
  EXPECT_FALSE(values.merge(incoming, true));
  EXPECT_EQ(values.value(1), 50);
  auto taken = values.take(1);
  ASSERT_TRUE(taken);
  EXPECT_EQ(*taken, 50);
  EXPECT_FALSE(values.contains(1));

  MapType<int, std::unique_ptr<int>> owners;
  owners.try_emplace(1, std::make_unique<int>(7));
  auto candidate = std::make_unique<int>(8);
  EXPECT_FALSE(owners.try_emplace(1, std::move(candidate)).second);
  ASSERT_TRUE(candidate);
  EXPECT_EQ(*candidate, 8);
  auto extracted = owners.take(1);
  ASSERT_TRUE(extracted);
  ASSERT_TRUE(*extracted);
  EXPECT_EQ(**extracted, 7);
  EXPECT_TRUE(owners.empty());

  MapType<int, int> printed{{1, 2}};
  std::ostringstream stream;
  stream << printed;
  EXPECT_EQ(stream.str(), "{ \"1\" : \"2\" }");
  EXPECT_EQ(std::format("{}", printed), stream.str());
}

template <typename Container>
void expectSetAlgebraContract() {
  Container values{1, 2, 3, 4};
  Container other{2, 4, 5};
  EXPECT_EQ(values.intersection(other), (Container{2, 4}));
  EXPECT_EQ(values.difference(other), (Container{1, 3}));
  EXPECT_EQ(values.combination(other), (Container{1, 2, 3, 4, 5}));
  EXPECT_TRUE(values.hasIntersection(other));
  EXPECT_FALSE(values.hasIntersection(Container{9}));
  auto snapshot = values.values();
  EXPECT_EQ(std::ranges::distance(values.valueView()), 4);
  EXPECT_FALSE(values.add(1));
  EXPECT_TRUE(values.remove(1));
  EXPECT_EQ(snapshot.size(), 4u);
  EXPECT_EQ(std::ranges::count(snapshot, 1), 1);
  while (!values.empty()) {
    auto taken = values.takeFirst();
    ASSERT_TRUE(taken);
    EXPECT_FALSE(values.contains(*taken));
  }
  EXPECT_FALSE(values.first());
  EXPECT_FALSE(values.last());
  EXPECT_FALSE(values.takeFirst());
  EXPECT_FALSE(values.takeLast());
  EXPECT_FALSE(values.maybeFirst());
  EXPECT_FALSE(values.maybeLast());
  EXPECT_FALSE(values.maybeTakeFirst());
  EXPECT_FALSE(values.maybeTakeLast());
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
  auto extracted = values.takeFirst();
  ASSERT_TRUE(extracted);
  EXPECT_EQ(extracted->get(), identity);
  EXPECT_TRUE(values.empty());
  values.add(std::move(*extracted));
  auto last = values.maybeTakeLast();
  ASSERT_TRUE(last);
  EXPECT_EQ(last->get(), identity);
  EXPECT_TRUE(values.empty());
}

struct FallibleSetValue {
  int value;
  std::reference_wrapper<std::vector<int>> visited;
  friend bool operator<(FallibleSetValue const& a, FallibleSetValue const& b) { return a.value < b.value; }
  friend std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& stream, FallibleSetValue const& value) {
    value.visited.get().push_back(value.value);
    if (value.value == 2)
      return std::unexpected(OutOfRangeException("first diagnostic error", false));
    stream << value.value;
    return std::ref(stream);
  }
};

}

TEST(MapComposition, OwnsStorageAndSeparatesBorrowedLookupFromSnapshots) {
  expectMapOwnershipContract<Map>();
  expectMapOwnershipContract<HashMap>();
  expectMapOwnershipContract<StableHashMap>();
}

TEST(MapComposition, StableStorageKeepsBorrowedValuesAcrossRehash) {
  StableHashMap<int, int> values{{1, 10}};
  auto borrowed = values.maybeRef(1);
  for (int key = 2; key < 200; ++key)
    values.set(key, key);
  values.reserve(1000);
  EXPECT_EQ(std::addressof(values.maybeRef(1)->get()), std::addressof(borrowed->get()));
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
  std::ostringstream stream;
  auto printed = stream << values;
  static_assert(std::same_as<decltype(printed), std::expected<std::reference_wrapper<std::ostream>, StarException>>);
  ASSERT_FALSE(printed);
  EXPECT_TRUE(printed.error().is<OutOfRangeException>());
  EXPECT_EQ(visited, (std::vector<int>{1, 2}));
  EXPECT_EQ(stream.str(), "{ \"1\" : \"1\", \"2\" : \"");
}

TEST(SetComposition, CheckedDiagnosticStopsAtFirstElementError) {
  std::vector<int> visited;
  Set<FallibleSetValue> values{{1, std::ref(visited)}, {2, std::ref(visited)}, {3, std::ref(visited)}};
  std::ostringstream stream;
  auto printed = stream << values;
  static_assert(std::same_as<decltype(printed), std::expected<std::reference_wrapper<std::ostream>, StarException>>);
  ASSERT_FALSE(printed);
  EXPECT_TRUE(printed.error().is<OutOfRangeException>());
  EXPECT_EQ(visited, (std::vector<int>{1, 2}));
  EXPECT_EQ(stream.str(), "(1, ");

  Set<int> ordered{1, 2};
  HashSet<int> flat{1};
  StableHashSet<int> stable{1};
  EXPECT_EQ(std::format("{}", ordered), "(1, 2)");
  EXPECT_EQ(std::format("{}", flat), "(1)");
  EXPECT_EQ(std::format("{}", stable), "(1)");
}
