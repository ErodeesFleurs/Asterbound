#include <benchmark/benchmark.h>

import std;
import star.format;
import star.native_runtime;
import star.random;
import star.string;
#ifdef ASTERBOUND_LRU_BEFORE
import star.lru_before;
import star.ttl_before;
#else
import star.lru_cache;
import star.ttl_cache;
#endif

namespace {
#ifdef ASTERBOUND_LRU_BEFORE
namespace cache_api = Star::LruBefore;
#else
namespace cache_api = Star;
#endif
using Star::String;

struct payload {
  String name;
  std::array<std::uint64_t, 8> words;
  bool operator==(payload const&) const = default;
};
using owned_value = std::shared_ptr<payload const>;

struct fingerprint {
  std::uint64_t value = 14695981039346656037ull;
  void add(std::uint64_t number) {
    for (unsigned shift = 0; shift != 64; shift += 8)
      value = (value ^ ((number >> shift) & 255)) * 1099511628211ull;
  }
  void add(String const& text) {
    add(text.utf8_size());
    for (unsigned char byte : text.utf8())
      value = (value ^ byte) * 1099511628211ull;
  }
  void add(payload const& object) {
    add(object.name);
    for (auto word : object.words)
      add(word);
  }
};

struct entry {
  String key;
  owned_value value;
  owned_value replacement;
};

struct fixture {
  std::vector<entry> entries;
  std::vector<std::size_t> permutation;
  fingerprint hash;

  explicit fixture(std::size_t capacity) {
    entries.reserve(capacity * 2);
    permutation.reserve(capacity);
    for (std::size_t i = 0; i != capacity * 2; ++i) {
      String key(std::format("statistics/event/{:08}/persistent-name", i));
      payload original{key, {}};
      payload replacement{String(std::format("updated/event/{:08}/persistent-name", i)), {}};
      for (std::size_t word = 0; word != original.words.size(); ++word) {
        original.words[word] = (i + 1) * 0x9e3779b97f4a7c15ull ^ (word + 7) * 0xd6e8feb86659fd93ull;
        replacement.words[word] = original.words[word] ^ 0xa5a5f00dcafebeefull;
      }
      hash.add(key);
      hash.add(original);
      hash.add(replacement);
      entries.push_back({std::move(key), std::make_shared<payload const>(std::move(original)),
                         std::make_shared<payload const>(std::move(replacement))});
    }
    // 17 is coprime to every registered capacity, including 1000 and 20000.
    for (std::size_t i = 0; i != capacity; ++i) {
      permutation.push_back(i * 17 % capacity);
      hash.add(permutation.back());
    }
  }
};

enum class operation { hit, peek, full_miss, assign, insert_trim, shrink };

template <bool Hashed>
using lru_cache =
  std::conditional_t<Hashed, cache_api::HashLruCache<String, owned_value>, cache_api::LruCache<String, owned_value>>;

// No payload allocation is hidden in the producer: fixtures own immutable
// objects before timing. Cache node/key allocation and shared ownership copies
// remain inside the measured operation, using the production BlockAllocator.
template <typename Cache>
concept fixture_cache =
  requires(Cache& cache, Cache const& observed, String const& key, owned_value value, std::size_t capacity) {
    requires std::same_as<typename Cache::key_type, String>;
    requires std::same_as<typename Cache::mapped_type, owned_value>;
    cache.clear();
    cache.set_capacity(capacity);
    cache.insert_or_assign(key, value);
    observed.keys();
    { observed.size() } -> std::same_as<std::size_t>;
    { observed.find_ref(key) } -> std::same_as<std::optional<std::reference_wrapper<owned_value const>>>;
  };

template <operation Op>
concept lru_workload = Op == operation::hit || Op == operation::peek || Op == operation::full_miss
  || Op == operation::assign || Op == operation::insert_trim || Op == operation::shrink;

void consume(owned_value const& value) {
  auto returned = value;
  benchmark::DoNotOptimize(returned);
  benchmark::DoNotOptimize(returned->words[3]);
}

template <fixture_cache Cache> bool fill(Cache& cache, fixture const& input, std::size_t capacity) {
  cache.clear();
  cache.set_capacity(capacity);
  for (std::size_t i = 0; i != capacity; ++i) {
    auto const& item = input.entries[i];
    if constexpr (std::same_as<decltype(cache.insert_or_assign(item.key, item.value)), void>) {
      cache.insert_or_assign(item.key, item.value);
    } else {
      if (!cache.insert_or_assign(item.key, item.value))
        return false;
    }
  }
  return true;
}

// Full ordered keys AND every byte/word of every resident payload are checked.
// Expected order comes from the workload's complete sweeps, not another LRU.
// TTL timestamps are deliberately excluded: they are not consumer-visible data.
template <fixture_cache Cache>
bool validate(Cache const& cache, fixture const& input, std::span<std::size_t const> expected, bool replacement,
              fingerprint& content) {
  auto keys = cache.keys();
  if (cache.size() != expected.size() || keys.size() != expected.size())
    return false;
  content.add(expected.size());
  for (std::size_t i = 0; i != expected.size(); ++i) {
    auto const& item = input.entries[expected[i]];
    auto const& wanted = replacement ? item.replacement : item.value;
    if (keys[i] != item.key)
      return false;
    auto actual = cache.find_ref(item.key);
    if (!actual || !actual->get() || *actual->get() != *wanted)
      return false;
    content.add(keys[i]);
    content.add(*actual->get());
  }
  return true;
}

void label(benchmark::State& state, fixture const& input, fingerprint content, std::size_t operations,
           std::string_view detail) {
  state.SetLabel(std::format(
    "fixture-fnv1a={:016x}; complete-content-fnv1a={:016x}; batch={}; ns/batch (divide by batch for amortized "
    "ns/item); "
    "String/shared_ptr<const payload>; prebuilt keys/payloads; node/key allocation+shared_ptr copy/release included; "
    "payload allocation, setup, reset, complete validation and destruction excluded; BlockAllocator/1024; {}",
    input.hash.value, content.value, operations, detail));
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(operations));
}

template <operation Op, fixture_cache Cache>
  requires lru_workload<Op>
bool apply(Cache& cache, fixture const& input, std::size_t capacity, std::size_t window, std::uint64_t& produced) {
  if constexpr (Op == operation::shrink) {
    cache.set_capacity(capacity / 2);
  } else {
    for (std::size_t i = 0; i != capacity; ++i) {
      auto index = (Op == operation::hit || Op == operation::assign || Op == operation::peek) ? input.permutation[i]
                                                                                              : window * capacity + i;
      auto const& item = input.entries[index];
      if constexpr (Op == operation::hit || Op == operation::full_miss) {
        auto result = cache.get(item.key, [&](String const&) {
          ++produced;
          return item.value;
        });
        if (!result)
          return false;
        consume(result->get());
      } else if constexpr (Op == operation::peek) {
        auto result = std::as_const(cache).find_ref(item.key);
        if (!result)
          return false;
        consume(result->get());
      } else if constexpr (Op == operation::assign) {
        cache.insert_or_assign(item.key, item.replacement);
      } else if constexpr (Op == operation::insert_trim) {
        cache.insert_or_assign(item.key, item.value);
      }
    }
    if constexpr (Op == operation::insert_trim)
      cache.set_capacity(capacity);// set is deliberately not a hard limit.
  }
  benchmark::DoNotOptimize(cache);
  benchmark::ClobberMemory();
  return true;
}

template <operation Op>
  requires lru_workload<Op>
std::vector<std::size_t> expected_order(fixture const& input, std::size_t capacity, std::size_t window) {
  if constexpr (Op == operation::hit || Op == operation::assign)
    return input.permutation;
  std::vector<std::size_t> result;
  auto start = Op == operation::shrink ? capacity / 2 : window * capacity;
  auto count = Op == operation::shrink ? capacity / 2 : capacity;
  for (std::size_t i = 0; i != count; ++i)
    result.push_back(start + i);
  return result;
}

template <bool Hashed, operation Op>
  requires lru_workload<Op>
void BM_Lru(benchmark::State& state) {
  auto runtime = Star::NativeRuntime::create();
  if (!runtime) {
    state.SkipWithError(runtime.error().what());
    return;
  }
  auto capacity = static_cast<std::size_t>(state.range(0));
  fixture input(capacity);
  lru_cache<Hashed> cache(capacity);
  if (!fill(cache, input, capacity)) {
    state.SkipWithError("LRU fixture fill failed");
    return;
  }
  constexpr bool alternating = Op == operation::full_miss || Op == operation::insert_trim;
  std::size_t window = alternating ? 1 : 0;
  std::uint64_t produced = 0;
  auto check = [&] {
    fingerprint content;
    auto expected = expected_order<Op>(input, capacity, window);
    if (!validate(std::as_const(cache), input, expected, Op == operation::assign, content)) {
      state.SkipWithError("Complete LRU content/order differs from sweep contract");
      return false;
    }
    return true;
  };
  if (!apply<Op>(cache, input, capacity, window, produced) || !check()
      || produced != (Op == operation::full_miss ? capacity : 0)) {
    state.SkipWithError("LRU untimed sweep or producer count failed");
    return;
  }
  if (!fill(cache, input, capacity)) {
    state.SkipWithError("LRU reset failed");
    return;
  }
  produced = 0;
  window = 0;
  for (auto _ : state) {
    (void)_;
    if constexpr (Op == operation::shrink) {
      state.PauseTiming();
      bool ready = fill(cache, input, capacity);
      state.ResumeTiming();
      if (!ready) {
        state.SkipWithError("LRU shrink reset failed");
        return;
      }
    }
    if constexpr (alternating)
      window ^= 1;
    if (!apply<Op>(cache, input, capacity, window, produced)) {
      state.SkipWithError("LRU timed sweep failed");
      return;
    }
  }
  if (!check())
    return;
  auto expected_produced = Op == operation::full_miss ? state.iterations() * capacity : 0;
  if (produced != expected_produced) {
    state.SkipWithError("LRU timed producer count differs");
    return;
  }
  // Calibration chooses different iteration counts before/after. Validate the
  // actual timed state above, then canonicalize alternating rows OFF the clock
  // so the reported complete-content hash is directly comparable across runs.
  if constexpr (alternating) {
    if (window == 0) {
      window = 1;
      if (!apply<Op>(cache, input, capacity, window, produced)) {
        state.SkipWithError("LRU untimed canonical sweep failed");
        return;
      }
    }
  }
  fingerprint content;
  auto expected = expected_order<Op>(input, capacity, window);
  if (!validate(std::as_const(cache), input, expected, Op == operation::assign, content)) {
    state.SkipWithError("LRU final complete validation failed");
    return;
  }
  label(state, input, content, Op == operation::shrink ? capacity / 2 : capacity,
        Op == operation::insert_trim ? "batch inserts capacity entries then one explicit trim"
          : Op == operation::shrink  ? "one set_capacity call; batch counts evicted entries; untimed refill"
                                     : "one complete capacity sweep; hits/assign use a coprime permutation");
}

enum class ttl_operation { hit_no_refresh, hit_refresh, cleanup_keep, cleanup_drop };

template <ttl_operation Op>
concept ttl_workload = Op == ttl_operation::hit_no_refresh || Op == ttl_operation::hit_refresh
  || Op == ttl_operation::cleanup_keep || Op == ttl_operation::cleanup_drop;

template <ttl_operation Op>
  requires ttl_workload<Op>
void BM_Ttl(benchmark::State& state) {
  auto runtime = Star::NativeRuntime::create();
  if (!runtime) {
    state.SkipWithError(runtime.error().what());
    return;
  }
  auto capacity = static_cast<std::size_t>(state.range(0));
  fixture input(capacity);
  Star::RandomService random(0x5a17);
  constexpr bool cleanup = Op == ttl_operation::cleanup_keep || Op == ttl_operation::cleanup_drop;
  cache_api::HashTtlCache<String, owned_value> cache(
    random, cleanup ? std::chrono::milliseconds(-1) : std::chrono::milliseconds(86400000),
    std::chrono::milliseconds(cleanup ? 0 : 17), capacity, Op == ttl_operation::hit_refresh);
  if (!fill(cache, input, capacity)) {
    state.SkipWithError("TTL fixture fill failed");
    return;
  }
  std::uint64_t produced = 0;
  auto sweep = [&] {
    if constexpr (Op == ttl_operation::cleanup_keep) {
      cache.cleanup([](String const&, owned_value const&) { return true; });
    } else if constexpr (Op == ttl_operation::cleanup_drop) {
      cache.cleanup();
    } else {
      return apply<operation::hit>(cache, input, capacity, 0, produced);
    }
    return true;
  };
  std::vector<std::size_t> expected;
  if constexpr (!cleanup)
    expected = input.permutation;
  else if constexpr (Op == ttl_operation::cleanup_keep)
    for (std::size_t i = 0; i != capacity; ++i)
      expected.push_back(i);
  auto check = [&](fingerprint& content) {
    if (produced != 0 || !validate(std::as_const(cache), input, expected, false, content)) {
      state.SkipWithError("Complete TTL content/order or hit producer count differs");
      return false;
    }
    return true;
  };
  fingerprint preflight;
  if (!sweep() || !check(preflight))
    return;
  if (!fill(cache, input, capacity)) {
    state.SkipWithError("TTL reset failed");
    return;
  }
  for (auto _ : state) {
    (void)_;
    if constexpr (Op == ttl_operation::cleanup_drop) {
      state.PauseTiming();
      bool ready = fill(cache, input, capacity);
      state.ResumeTiming();
      if (!ready) {
        state.SkipWithError("TTL cleanup reset failed");
        return;
      }
    }
    if (!sweep()) {
      state.SkipWithError("TTL hit/cleanup failed");
      return;
    }
    benchmark::DoNotOptimize(cache);
    benchmark::ClobberMemory();
  }
  fingerprint content;
  if (!check(content))
    return;
  label(state, input, content, capacity,
        cleanup ? "TTL seed=23063; smear=0ms; ttl=-1ms; one cleanup call/batch; refresh-filter keep or no-filter erase"
                : "TTL seed=23063; smear=17ms; ttl=24h; refresh on/off named in row; no read-time expiry");
}

#define LRU_CASE(HASHED, OP, NAME)                                                                                     \
  BENCHMARK_TEMPLATE(BM_Lru, HASHED, operation::OP)->Name("Lru/" NAME)->Arg(64)->Arg(256)->Arg(1000)->Arg(20000)
#define LRU_FAMILY(HASHED, NAME)                                                                                       \
  LRU_CASE(HASHED, hit, NAME "/Hit");                                                                                  \
  LRU_CASE(HASHED, peek, NAME "/ConstPeek");                                                                           \
  LRU_CASE(HASHED, full_miss, NAME "/FullMiss");                                                                       \
  LRU_CASE(HASHED, assign, NAME "/InsertOrAssignUpdate");                                                              \
  LRU_CASE(HASHED, insert_trim, NAME "/InsertThenTrim");                                                               \
  LRU_CASE(HASHED, shrink, NAME "/ShrinkHalf")

LRU_FAMILY(false, "Ordered");
LRU_FAMILY(true, "Hash");
#undef LRU_FAMILY
#undef LRU_CASE

BENCHMARK_TEMPLATE(BM_Ttl, ttl_operation::hit_no_refresh)->Name("Ttl/Hash/HitRefreshOff")->Arg(256);
BENCHMARK_TEMPLATE(BM_Ttl, ttl_operation::hit_refresh)->Name("Ttl/Hash/HitRefreshOn")->Arg(256);
BENCHMARK_TEMPLATE(BM_Ttl, ttl_operation::cleanup_keep)->Name("Ttl/Hash/CleanupKeep")->Arg(256);
BENCHMARK_TEMPLATE(BM_Ttl, ttl_operation::cleanup_drop)->Name("Ttl/Hash/CleanupDrop")->Arg(256);
}// namespace

BENCHMARK_MAIN();
