import std;
import star.format;
import star.ordered_map;
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
      keys.append(i->first);
    EXPECT_EQ(comp, keys);
  }

  {
    List<int> comp = {1, 2, 3};
    List<int> keys;
    for (auto i = map.rbegin(); i != map.rend(); ++i)
      keys.append(i->first);
    EXPECT_EQ(comp, keys);
  }
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

  EXPECT_EQ(std::addressof(map.maybeRef(3)->get()), std::addressof(third->get()));
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

  EXPECT_EQ(std::addressof(map.maybeRef(3)->get()), std::addressof(third->get()));

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
  EXPECT_FALSE(map.maybeRef(malformed));

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
  map.sortByKey();
  EXPECT_EQ(map.keys(), List<int>({1, 2, 3, 4, 5}));
  map.sortByValue();
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
  ASSERT_TRUE(cache.maybeRef(2));
  EXPECT_EQ(cache.maybeRef(2)->get(), 21);
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
  auto retained = cache.maybeRef(1);
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
  auto retained = std::as_const(cache).maybeRef(1);
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
  EXPECT_FALSE(cache.maybeRef(1));
  EXPECT_FALSE(cache.set(1, 99));
  EXPECT_FALSE(cache.set(2, 20));
  retained = std::as_const(cache).maybeRef(1);
  ASSERT_TRUE(retained);
  EXPECT_EQ(retained->get(), 42);
  EXPECT_FALSE(std::as_const(cache).maybeRef(2));
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
