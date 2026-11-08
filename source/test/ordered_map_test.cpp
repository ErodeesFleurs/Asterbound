import std;
import star.format;
import star.ordered_map;
import star.map;
import star.list;
import star.lru_cache;
import star.ttl_cache;
import star.random;
import star.json;
import star.unicode;

#include "gtest/gtest.h"

using namespace Star;

TEST(OrderedMap, Insert) {
  OrderedMap<int, int> map;
  map.insert({3, 3});
  map.insert({2, 2});
  map.insert({1, 1});

  {
    List<int> comp = {3, 2, 1};
    EXPECT_EQ(comp, map.keys());
  }

  {
    List<int> comp = {3, 2, 1};
    List<int> keys;
    for (auto i = map.begin(); i != map.end(); ++i)
      keys.push_back(i->first);
    EXPECT_EQ(comp, keys);
  }

  {
    List<int> comp = {1, 2, 3};
    List<int> keys;
    for (auto i = map.rbegin(); i != map.rend(); ++i)
      keys.push_back(i->first);
    EXPECT_EQ(comp, keys);
  }
}

TEST(OrderedMap, PairSnapshotsPreserveKeysValuesAndOwnership) {
  auto check = []<typename Map>() {
    Map map{{3, 7}, {1, 7}, {2, 9}};
    auto pairs = map.pairs();
    auto keys = map.keys();
    auto values = map.values();
    map.clear();
    EXPECT_EQ(pairs, (List<std::pair<int, int>>{{3, 7}, {1, 7}, {2, 9}}));
    EXPECT_EQ(keys, (List<int>{3, 1, 2}));
    EXPECT_EQ(values, (List<int>{7, 7, 9}));
  };
  check.template operator()<OrderedMap<int, int>>();
  check.template operator()<OrderedHashMap<int, int>>();
}

TEST(OrderedMap, KeyOfUsesCurrentOrderAndPreservesMissingErrors) {
  auto check = []<typename Map>() {
    Map map{{3, 7}, {1, 7}, {2, 9}};
    auto first = map.key_of(7);
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, 3);
    map.sort_by_key();
    auto sorted = map.key_of(7);
    ASSERT_TRUE(sorted);
    EXPECT_EQ(*sorted, 1);
    auto missing = map.key_of(8);
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().template is<MapException>());
    map.clear();
    auto empty = map.key_of(7);
    ASSERT_FALSE(empty);
    EXPECT_TRUE(empty.error().template is<MapException>());
  };
  check.template operator()<OrderedMap<int, int>>();
  check.template operator()<OrderedHashMap<int, int>>();
}

TEST(OrderedMap, KeyOfOnlyRendersDiagnosticsAfterMissing) {
  Json malformed(std::string("\xff", 1));
  OrderedHashMap<int, Json> map{{4, malformed}};
  auto found = map.key_of(malformed);
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, 4);
  map.clear();
  auto missing = map.key_of(malformed);
  ASSERT_FALSE(missing);
  EXPECT_TRUE(missing.error().is<UnicodeException>());
}

TEST(OrderedMap, Getters) {
  OrderedMap<int, int> map;
  map[1] = 1;
  map[2] = 2;
  map[3] = 3;

  auto first = map.get(1);
  auto second = map.get(2);
  auto third = map.get(3);
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  ASSERT_TRUE(third);
  EXPECT_EQ(first->get(), 1);
  EXPECT_EQ(second->get(), 2);
  EXPECT_EQ(third->get(), 3);

  EXPECT_EQ(std::addressof(map.find_ref(3)->get()), std::addressof(third->get()));
}

TEST(OrderedMap, ConstGetters) {
  OrderedHashMap<int, int> const map{
    {1, 1},
    {2, 2},
    {3, 3}
  };

  auto first = map.get(1);
  auto second = map.get(2);
  auto third = map.get(3);
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  ASSERT_TRUE(third);
  EXPECT_EQ(first->get(), 1);
  EXPECT_EQ(second->get(), 2);
  EXPECT_EQ(third->get(), 3);

  EXPECT_EQ(std::addressof(map.find_ref(3)->get()), std::addressof(third->get()));

  EXPECT_EQ(map.value(4, 4), 4);
  EXPECT_EQ(map.maybe(5), std::optional<int>());
}

TEST(OrderedMap, MalformedDiagnosticPreservesFailureAndLookupState) {
  OrderedHashMap<Json, int> map;
  map.insert({Json(7), 42});
  Json malformed(std::string("\xff", 1));

  auto rejected = map.get(malformed);
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<UnicodeException>());
  EXPECT_FALSE(map.find_ref(malformed));

  auto recovered = map.get(Json(7));
  ASSERT_TRUE(recovered);
  EXPECT_EQ(recovered->get(), 42);
}

TEST(OrderedMap, Sorting) {
  OrderedMap<int, int> map{
    {1, 5},
    {3, 3},
    {2, 4},
    {5, 1},
    {4, 2}
  };

  EXPECT_EQ(map.keys(), List<int>({1, 3, 2, 5, 4}));
  map.sort_by_key();
  EXPECT_EQ(map.keys(), List<int>({1, 2, 3, 4, 5}));
  map.sort_by_value();
  EXPECT_EQ(map.keys(), List<int>({5, 4, 3, 2, 1}));
}

TEST(OrderedMap, Removing) {
  OrderedHashMap<int, int> map{
    {5, 5},
    {4, 4},
    {3, 3},
    {2, 2},
    {1, 1}
  };

  map.remove(3);
  map.remove(1);
  EXPECT_EQ(map.keys(), List<int>({5, 4, 2}));
}

TEST(OrderedMap, TakeKeepsRemainingLookupsOrderAndIndicesValid) {
  auto check = []<typename Map>() {
    Map map{{5, 50}, {4, 40}, {3, 30}, {2, 20}, {1, 10}};
    auto taken = map.extract_value(3);
    ASSERT_TRUE(taken);
    EXPECT_EQ(*taken, 30);
    EXPECT_FALSE(map.contains(3));
    EXPECT_EQ(map.keys(), (List<int>{5, 4, 2, 1}));
    EXPECT_EQ(map.size(), 4u);
    auto missing = map.extract_value(3);
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().template is<MapException>());

    ASSERT_TRUE(map.move_to_front(2));
    ASSERT_TRUE(map.move_to_back(5));
    map.erase(map.find(4));
    EXPECT_EQ(map.keys(), (List<int>{2, 1, 5}));
    for (std::size_t index = 0; auto key : {2, 1, 5}) {
      auto value = map.get(key);
      ASSERT_TRUE(value);
      EXPECT_EQ(value->get(), key * 10);
      EXPECT_EQ(map.index_of(key), std::optional<std::size_t>(index));
      auto indexed_key = map.key_at(index);
      auto indexed_value = map.value_at(index);
      ASSERT_TRUE(indexed_key);
      ASSERT_TRUE(indexed_value);
      EXPECT_EQ(indexed_key->get(), key);
      EXPECT_EQ(indexed_value->get(), key * 10);
      ++index;
    }
    EXPECT_FALSE(map.index_of(3));
    EXPECT_FALSE(map.index_of(4));
    EXPECT_FALSE(map.key_at(map.size()));
    EXPECT_FALSE(map.value_at(map.size()));
    map.insert(3, 300);
    EXPECT_EQ(map.keys(), (List<int>{2, 1, 5, 3}));
    auto replacement = map.get(3);
    ASSERT_TRUE(replacement);
    EXPECT_EQ(replacement->get(), 300);
  };
  check.template operator()<OrderedMap<int, int>>();
  check.template operator()<OrderedHashMap<int, int>>();
}

TEST(OrderedMap, ExtractPreservesCollidingSuccessor) {
  struct ConstantHash {
    std::size_t operator()(int) const { return 0; }
  };
  OrderedHashMap<int, int, ConstantHash> map{{1, 10}, {2, 20}};
  auto extracted = map.extract_value(1);
  ASSERT_TRUE(extracted);
  EXPECT_EQ(*extracted, 10);
  ASSERT_EQ(map.keys(), (List<int>{2}));
  EXPECT_FALSE(map.contains(1));
  auto successor = map.get(2);
  ASSERT_TRUE(successor);
  EXPECT_EQ(successor->get(), 20);
}

TEST(OrderedMap, TryEmplaceDoesNotConsumeDuplicateArguments) {
  auto check = []<typename Map>() {
    Map map;
    int const key = 7;
    auto original = std::make_unique<int>(70);
    auto inserted = map.try_emplace(key, std::move(original));
    ASSERT_TRUE(inserted.second);
    EXPECT_FALSE(original);
    EXPECT_EQ(*inserted.first->second, 70);

    auto duplicate = std::make_unique<int>(700);
    auto rejected = map.try_emplace(7, std::move(duplicate));
    EXPECT_FALSE(rejected.second);
    EXPECT_EQ(rejected.first, inserted.first);
    ASSERT_TRUE(duplicate);
    EXPECT_EQ(*duplicate, 700);
    EXPECT_EQ(*rejected.first->second, 70);
    EXPECT_EQ(map.keys(), (List<int>{7}));
  };
  check.template operator()<OrderedMap<int, std::unique_ptr<int>>>();
  check.template operator()<OrderedHashMap<int, std::unique_ptr<int>>>();
}

TEST(OrderedMap, TryEmplaceConstructsImmovableMappedValuesInPlace) {
  struct Immovable {
    explicit Immovable(int initial) : value(initial) {}
    Immovable(Immovable const&) = delete;
    Immovable(Immovable&&) = delete;
    int value;
  };
  auto check = []<typename Map>() {
    Map map;
    auto inserted = map.try_emplace(3, 30);
    ASSERT_TRUE(inserted.second);
    EXPECT_EQ(inserted.first->second.value, 30);
    auto duplicate = map.try_emplace(3, 300);
    EXPECT_FALSE(duplicate.second);
    EXPECT_EQ(duplicate.first->second.value, 30);
  };
  check.template operator()<OrderedMap<int, Immovable>>();
  check.template operator()<OrderedHashMap<int, Immovable>>();
}

TEST(OrderedMap, MoveOnlyInsertionSetAndExtractionPreserveOrder) {
  auto check = []<typename Map>() {
    Map map;
    map.insert(2, std::make_unique<int>(20));
    map.insert(typename Map::value_type(3, std::make_unique<int>(30)));
    map.insert_front(1, std::make_unique<int>(10));
    map.insert_front(typename Map::value_type(0, std::make_unique<int>(0)));
    auto added = map.add(4, std::make_unique<int>(40));
    ASSERT_TRUE(added);
    auto stable_node = map.find(2);
    auto& updated = map.set(2, std::make_unique<int>(200));
    EXPECT_EQ(*updated, 200);
    EXPECT_EQ(map.find(2), stable_node);
    map.set(5, std::make_unique<int>(50));
    EXPECT_EQ(map.keys(), (List<int>{0, 1, 2, 3, 4, 5}));

    auto taken = map.extract_value(2);
    ASSERT_TRUE(taken);
    ASSERT_TRUE(*taken);
    EXPECT_EQ(**taken, 200);
    auto pair = map.try_extract_value(3);
    ASSERT_TRUE(pair);
    EXPECT_EQ(pair->first, 3);
    ASSERT_TRUE(pair->second);
    EXPECT_EQ(*pair->second, 30);
    EXPECT_FALSE(map.try_extract_value(3));
    auto first = map.extract_front();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->first, 0);
    ASSERT_TRUE(first->second);
    EXPECT_EQ(*first->second, 0);
    EXPECT_EQ(map.keys(), (List<int>{1, 4, 5}));

    auto moved = map.insert(map.begin(), typename Map::value_type(5, std::make_unique<int>(500)));
    EXPECT_EQ(moved, map.begin());
    EXPECT_EQ(*moved->second, 500);
    EXPECT_EQ(map.keys(), (List<int>{5, 1, 4}));
    EXPECT_EQ(map.index_of(5), std::optional<std::size_t>(0));
    auto retained = map.get(4);
    ASSERT_TRUE(retained);
    EXPECT_EQ(*retained->get(), 40);
    EXPECT_EQ(map.index_of(4), std::optional<std::size_t>(2));
  };
  check.template operator()<OrderedMap<int, std::unique_ptr<int>>>();
  check.template operator()<OrderedHashMap<int, std::unique_ptr<int>>>();
}

TEST(OrderedMap, FirstExtractionHandlesEmptyAndSingleEntry) {
  auto check = []<typename Map>() {
    Map map;
    auto empty = map.extract_front();
    ASSERT_FALSE(empty);
    EXPECT_TRUE(empty.error().template is<MapException>());
    map.try_emplace(8, std::make_unique<int>(80));
    auto pair = map.try_extract_value(8);
    ASSERT_TRUE(pair);
    EXPECT_EQ(pair->first, 8);
    EXPECT_EQ(*pair->second, 80);
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.find(8), map.end());
    map.try_emplace(9, std::make_unique<int>(90));
    auto first = map.extract_front();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->first, 9);
    EXPECT_EQ(*first->second, 90);
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.find(9), map.end());
  };
  check.template operator()<OrderedMap<int, std::unique_ptr<int>>>();
  check.template operator()<OrderedHashMap<int, std::unique_ptr<int>>>();
}

TEST(LruCache, HitsRetainEntriesAndDoNotProduce) {
  HashLruCache<int, int> cache(1);
  unsigned calls = 0;
  auto produce = [&](int const& key) { ++calls; return key * 10; };
  auto first = cache.get(7, produce);
  ASSERT_TRUE(first);
  EXPECT_EQ(first->get(), 70);
  auto hit = cache.get(7, produce);
  ASSERT_TRUE(hit);
  EXPECT_EQ(hit->get(), 70);
  EXPECT_EQ(calls, 1u);
  cache.setMaxSize(2);
  cache.set(8, 80);
  auto second = cache.get(8, produce);
  ASSERT_TRUE(second);
  EXPECT_EQ(second->get(), 80);
  EXPECT_EQ(cache.currentSize(), 2u);
  EXPECT_EQ(cache.keys(), (List<int>{7, 8}));
}

TEST(LruCache, MissEvictsLeastRecentAndPreservesFailureOrdering) {
  HashLruCache<int, int> cache(2);
  cache.set(1, 10);
  cache.set(2, 20);
  auto hit = cache.get(1, [](int const&) { return -1; });
  ASSERT_TRUE(hit);
  EXPECT_EQ(hit->get(), 10);
  auto miss = cache.get(3, [](int const& key) { return key * 10; });
  ASSERT_TRUE(miss);
  EXPECT_EQ(miss->get(), 30);
  EXPECT_EQ(cache.keys(), (List<int>{1, 3}));
  auto failure = cache.get(4, [](int const&) -> std::expected<int, StarException> {
    return std::unexpected(StarException("producer"));
  });
  ASSERT_FALSE(failure);
  EXPECT_TRUE(failure.error().is<StarException>());
  EXPECT_NE(std::string_view(failure.error().what()).find("producer"), std::string_view::npos);
  EXPECT_EQ(cache.keys(), (List<int>{3}));
  cache.set(5, 50);
  cache.setMaxSize(0);
  EXPECT_EQ(cache.maxSize(), 1u);
  EXPECT_EQ(cache.keys(), (List<int>{5}));
}

TEST(LruCache, BorrowsMutableNoncopyableCallbacks) {
  struct Producer {
    std::unique_ptr<int> calls = std::make_unique<int>(0);
    int operator()(int const& key) { ++*calls; return key * 10; }
  } producer;
  HashLruCache<int, int> cache(2);
  auto first = cache.get(1, producer);
  ASSERT_TRUE(first);
  EXPECT_EQ(first->get(), 10);
  auto hit = cache.get(1, producer);
  ASSERT_TRUE(hit);
  EXPECT_EQ(hit->get(), 10);
  EXPECT_EQ(*producer.calls, 1);
  cache.set(2, 20);
  auto filter = [state = std::make_unique<int>(0)](int const& key, int& value) mutable {
    ++*state;
    value += 1;
    return key == 1;
  };
  cache.removeWhere(filter);
  ASSERT_TRUE(cache.find_ref(2));
  EXPECT_EQ(cache.find_ref(2)->get(), 21);
  EXPECT_EQ(cache.keys(), (List<int>{2}));
}

TEST(TtlCache, MoveOnlyProducerAndOptionalRefreshFilters) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  HashTtlCache<int, int> cache(random, std::chrono::milliseconds(-1), std::chrono::milliseconds(0), 4, false);
  auto produced = cache.get(1, [value = std::make_unique<int>(42)](int const&) mutable { return *value; });
  ASSERT_TRUE(produced);
  EXPECT_EQ(produced->get(), 42);
  struct Refresh {
    explicit operator bool() const { return false; }
    bool operator()(int const&, int const&) { return true; }
  } refresh;
  cache.cleanup(refresh);
  auto retained = cache.find_ref(1);
  ASSERT_TRUE(retained);
  ASSERT_TRUE(*retained);
  EXPECT_EQ((*retained)->get(), 42);
  std::function<bool(int const&, int const&)> empty;
  cache.cleanup(empty);
  EXPECT_TRUE(cache.keys().empty());
  ASSERT_TRUE(cache.set(2, 20));
  bool (*nullFilter)(int const&, int const&) = nullptr;
  cache.cleanup(nullFilter);
  EXPECT_TRUE(cache.keys().empty());
  ASSERT_TRUE(cache.set(3, 30));
  cache.cleanup({});
  EXPECT_TRUE(cache.keys().empty());
}

TEST(TtlCache, FailedSmearRetainsProducedEntryAndRetriesZeroTimestamp) {
  RandomService random(123);
  HashTtlCache<int, int> cache(random, std::chrono::milliseconds(10000), std::chrono::milliseconds(-1), 4, false);
  unsigned calls = 0;
  auto produce = [&](int const&) { ++calls; return 42; };

  EXPECT_FALSE(cache.get(1, produce));
  EXPECT_EQ(cache.currentSize(), 1u);
  auto retained = std::as_const(cache).find_ref(1);
  ASSERT_TRUE(retained);
  EXPECT_EQ(retained->get(), 42);
  EXPECT_FALSE(cache.get(1, produce));
  EXPECT_EQ(calls, 1u);

  cache.setTimeSmear(std::chrono::milliseconds(0));
  auto initialized = cache.get(1, produce);
  ASSERT_TRUE(initialized);
  EXPECT_EQ(initialized->get(), 42);
  cache.setTimeSmear(std::chrono::milliseconds(-1));
  auto hit = cache.get(1, produce);
  ASSERT_TRUE(hit);
  EXPECT_EQ(hit->get(), 42);
  EXPECT_EQ(calls, 1u);

  cache.setTtlUpdateEnabled(true);
  EXPECT_FALSE(cache.find_ref(1));
  EXPECT_FALSE(cache.set(1, 99));
  EXPECT_FALSE(cache.set(2, 20));
  retained = std::as_const(cache).find_ref(1);
  ASSERT_TRUE(retained);
  EXPECT_EQ(retained->get(), 42);
  EXPECT_FALSE(std::as_const(cache).find_ref(2));
}

TEST(TtlCache, CleanupRefreshInitializesTimestampWithoutSamplingSmear) {
  RandomService random(123);
  HashTtlCache<int, int> cache(random, std::chrono::milliseconds(-1), std::chrono::milliseconds(-1), 4, false);
  unsigned calls = 0;
  auto produce = [&](int const&) { ++calls; return 42; };
  EXPECT_FALSE(cache.get(1, produce));

  cache.cleanup([](int const&, int const&) { return true; });
  auto hit = cache.get(1, produce);
  ASSERT_TRUE(hit);
  EXPECT_EQ(hit->get(), 42);
  EXPECT_EQ(calls, 1u);
  cache.cleanup();
  EXPECT_EQ(cache.currentSize(), 0u);
}
