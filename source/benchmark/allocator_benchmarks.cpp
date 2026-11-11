#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.format;
import star.block_allocator;
import star.hash;
import star.json;
import star.lru_cache;
import star.native_runtime;
import star.random;
import star.ttl_cache;
import star.string;

// GNU --wrap observes the live BlockAllocator's upstream new/delete requests,
// including its family/control-block/vector metadata. __real_* calls retain the
// production allocation boundary, including native aligned allocation/free and
// configured malloc implementations. No production allocator is copied here.
// Tracking is disabled during timing: both backends pay a TLS-pointer test and
// wrapper call, but no probe-table work. Direct malloc calls are not counted.
// Requested bytes exclude malloc metadata, alignment rounding and the fixed
// heap probe table. Live bytes mean requested bytes not yet deleted at the
// named snapshot; total bytes sum all requests, including subsequently deleted
// storage. Neither is RSS, usable heap size, or a fragmentation measurement.
namespace AllocationProbe {

struct Entry {
  void* pointer = nullptr;
  std::size_t bytes = 0;
  bool occupied = false;
  bool used = false;
};

struct Statistics {
  static constexpr std::size_t Capacity = 65536;
  std::array<Entry, Capacity> entries{};
  std::uint64_t calls = 0;
  std::uint64_t requestedBytes = 0;
  std::uint64_t liveBytes = 0;
  std::uint64_t peakBytes = 0;
  std::uint64_t liveCalls = 0;
  std::uint64_t peakLiveCalls = 0;
  bool overflow = false;

  std::size_t slot(void* pointer) const noexcept {
    return ((reinterpret_cast<std::uintptr_t>(pointer) >> 4) * 11400714819323198485ull)
        & (Capacity - 1);
  }

  void allocated(void* pointer, std::size_t bytes) noexcept {
    ++calls;
    peakLiveCalls = std::max(peakLiveCalls, ++liveCalls);
    requestedBytes += bytes;
    liveBytes += bytes;
    peakBytes = std::max(peakBytes, liveBytes);
    auto start = slot(pointer);
    for (std::size_t i = 0; i < Capacity; ++i) {
      auto& entry = entries[(start + i) & (Capacity - 1)];
      if (!entry.occupied) {
        entry = {.pointer = pointer, .bytes = bytes, .occupied = true, .used = true};
        return;
      }
    }
    overflow = true;
  }

  void deallocated(void* pointer) noexcept {
    auto start = slot(pointer);
    for (std::size_t i = 0; i < Capacity; ++i) {
      auto& entry = entries[(start + i) & (Capacity - 1)];
      if (!entry.used)
        return;
      if (entry.occupied && entry.pointer == pointer) {
        liveBytes -= entry.bytes;
        --liveCalls;
        entry.occupied = false;
        return;
      }
    }
  }
};

thread_local Statistics* active = nullptr;

// Keep Itanium size_t encodings in sync with this target's --wrap link options:
// LP64 ELF uses unsigned long (m); MinGW64 uses unsigned long long (y);
// supported 32-bit targets use unsigned int (j). No CRT aligned-alloc guesswork.
#if defined(_WIN64)
#define ALLOCATOR_SIZE_CODE "y"
#elif __SIZEOF_SIZE_T__ == 8
#define ALLOCATOR_SIZE_CODE "m"
#else
#define ALLOCATOR_SIZE_CODE "j"
#endif

thread_local unsigned nativeDepth = 0;

// Some native nothrow/array operators delegate to another wrapped operator.
// Count only the outer request, not both aliases for the same allocation.
#define WRAP_NEW(NAME, SYMBOL, PARAMETERS, ARGUMENTS, SPECIFICATION) \
  extern "C" void* real##NAME PARAMETERS SPECIFICATION asm("__real_" SYMBOL); \
  extern "C" void* wrap##NAME PARAMETERS SPECIFICATION asm("__wrap_" SYMBOL); \
  extern "C" void* wrap##NAME PARAMETERS SPECIFICATION { \
    auto* statistics = active; \
    if (!statistics) \
      return real##NAME ARGUMENTS; \
    bool outer = nativeDepth++ == 0; \
    auto* pointer = real##NAME ARGUMENTS; \
    --nativeDepth; \
    if (outer && pointer) \
      statistics->allocated(pointer, bytes); \
    return pointer; \
  }

#define WRAP_DELETE(NAME, SYMBOL, PARAMETERS, ARGUMENTS) \
  extern "C" void real##NAME PARAMETERS noexcept asm("__real_" SYMBOL); \
  extern "C" void wrap##NAME PARAMETERS noexcept asm("__wrap_" SYMBOL); \
  extern "C" void wrap##NAME PARAMETERS noexcept { \
    auto* statistics = active; \
    if (!statistics) { \
      real##NAME ARGUMENTS; \
      return; \
    } \
    bool outer = nativeDepth++ == 0; \
    if (outer && pointer) \
      statistics->deallocated(pointer); \
    real##NAME ARGUMENTS; \
    --nativeDepth; \
  }

WRAP_NEW(New, "_Znw" ALLOCATOR_SIZE_CODE, (std::size_t bytes), (bytes), )
WRAP_NEW(NewArray, "_Zna" ALLOCATOR_SIZE_CODE, (std::size_t bytes), (bytes), )
WRAP_NEW(NewAligned, "_Znw" ALLOCATOR_SIZE_CODE "St11align_val_t",
    (std::size_t bytes, std::align_val_t alignment), (bytes, alignment), )
WRAP_NEW(NewArrayAligned, "_Zna" ALLOCATOR_SIZE_CODE "St11align_val_t",
    (std::size_t bytes, std::align_val_t alignment), (bytes, alignment), )
WRAP_NEW(NewNothrow, "_Znw" ALLOCATOR_SIZE_CODE "RKSt9nothrow_t",
    (std::size_t bytes, std::nothrow_t const& tag), (bytes, tag), noexcept)
WRAP_NEW(NewArrayNothrow, "_Zna" ALLOCATOR_SIZE_CODE "RKSt9nothrow_t",
    (std::size_t bytes, std::nothrow_t const& tag), (bytes, tag), noexcept)
WRAP_NEW(NewAlignedNothrow, "_Znw" ALLOCATOR_SIZE_CODE "St11align_val_tRKSt9nothrow_t",
    (std::size_t bytes, std::align_val_t alignment, std::nothrow_t const& tag), (bytes, alignment, tag), noexcept)
WRAP_NEW(NewArrayAlignedNothrow, "_Zna" ALLOCATOR_SIZE_CODE "St11align_val_tRKSt9nothrow_t",
    (std::size_t bytes, std::align_val_t alignment, std::nothrow_t const& tag), (bytes, alignment, tag), noexcept)

WRAP_DELETE(Delete, "_ZdlPv", (void* pointer), (pointer))
WRAP_DELETE(DeleteArray, "_ZdaPv", (void* pointer), (pointer))
WRAP_DELETE(DeleteSized, "_ZdlPv" ALLOCATOR_SIZE_CODE, (void* pointer, std::size_t bytes), (pointer, bytes))
WRAP_DELETE(DeleteArraySized, "_ZdaPv" ALLOCATOR_SIZE_CODE, (void* pointer, std::size_t bytes), (pointer, bytes))
WRAP_DELETE(DeleteAligned, "_ZdlPvSt11align_val_t",
    (void* pointer, std::align_val_t alignment), (pointer, alignment))
WRAP_DELETE(DeleteArrayAligned, "_ZdaPvSt11align_val_t",
    (void* pointer, std::align_val_t alignment), (pointer, alignment))
WRAP_DELETE(DeleteSizedAligned, "_ZdlPv" ALLOCATOR_SIZE_CODE "St11align_val_t",
    (void* pointer, std::size_t bytes, std::align_val_t alignment), (pointer, bytes, alignment))
WRAP_DELETE(DeleteArraySizedAligned, "_ZdaPv" ALLOCATOR_SIZE_CODE "St11align_val_t",
    (void* pointer, std::size_t bytes, std::align_val_t alignment), (pointer, bytes, alignment))
WRAP_DELETE(DeleteNothrow, "_ZdlPvRKSt9nothrow_t",
    (void* pointer, std::nothrow_t const& tag), (pointer, tag))
WRAP_DELETE(DeleteArrayNothrow, "_ZdaPvRKSt9nothrow_t",
    (void* pointer, std::nothrow_t const& tag), (pointer, tag))
WRAP_DELETE(DeleteAlignedNothrow, "_ZdlPvSt11align_val_tRKSt9nothrow_t",
    (void* pointer, std::align_val_t alignment, std::nothrow_t const& tag), (pointer, alignment, tag))
WRAP_DELETE(DeleteArrayAlignedNothrow, "_ZdaPvSt11align_val_tRKSt9nothrow_t",
    (void* pointer, std::align_val_t alignment, std::nothrow_t const& tag), (pointer, alignment, tag))

#undef WRAP_NEW
#undef WRAP_DELETE
#undef ALLOCATOR_SIZE_CODE

}

namespace {

using Key = std::uint64_t;
using Value = std::array<std::uint64_t, 4>;
using Pair = std::pair<Key const, Value>;

Value valueFor(Key key) {
  return {key, key ^ 0xa5a5a5a5a5a5a5a5ull, key + 1, key * 17};
}

// OrderedMap/Lru/Ttl currently have no allocator-taking constructors. This
// benchmark-only injection adapter preserves an explicitly scoped LOCAL resource
// through allocator rebinding without changing the process's default resource.
// It adds no ownership or production API, and is never used outside Backend's
// scope. Direct std::list/map cases use polymorphic_allocator itself instead.
thread_local std::pmr::memory_resource* fixtureResource = nullptr;

template <typename T>
class FixturePoolAllocator {
public:
  using value_type = T;
  template <typename U> struct rebind { using other = FixturePoolAllocator<U>; };

  FixturePoolAllocator() noexcept : m_allocator(fixtureResource) {
    if (!fixtureResource)
      std::abort();
  }
  template <typename U>
  FixturePoolAllocator(FixturePoolAllocator<U> const& other) noexcept : m_allocator(other.resource()) {}

  T* allocate(std::size_t count) { return m_allocator.allocate(count); }
  void deallocate(T* pointer, std::size_t count) { m_allocator.deallocate(pointer, count); }
  std::pmr::memory_resource* resource() const noexcept { return m_allocator.resource(); }
  template <typename U>
  bool operator==(FixturePoolAllocator<U> const& other) const noexcept {
    return *resource() == *other.resource();
  }

private:
  std::pmr::polymorphic_allocator<T> m_allocator;
};

template <bool Pool>
struct Backend {
  explicit Backend(std::pmr::pool_options = {}) {}
};

template <>
struct Backend<true> {
  explicit Backend(std::pmr::pool_options options = {})
      : resource(options, std::pmr::new_delete_resource()) {}

  // Existing default cases continue to use pool_options{}. Bounded payload
  // cases supply options explicitly; both retain the native upstream boundary.
  std::pmr::unsynchronized_pool_resource resource;
  std::pmr::memory_resource* previous = std::exchange(fixtureResource, &resource);
  ~Backend() { fixtureResource = previous; }
};

template <bool Pool, typename T>
using NodeAllocator = std::conditional_t<Pool, std::pmr::polymorphic_allocator<T>, Star::BlockAllocator<T, 1024>>;

template <bool Pool, typename T>
using CacheAllocator = std::conditional_t<Pool, FixturePoolAllocator<T>, Star::BlockAllocator<T, 1024>>;

template <bool Pool, typename T>
auto nodeAllocator(Backend<Pool>& backend) {
  if constexpr (Pool)
    return NodeAllocator<Pool, T>(&backend.resource);
  else
    return NodeAllocator<Pool, T>();
}

enum class Shape { List, Map, Lru, HashLru, HashTtl, StatisticsPayload };

template <Shape Workload>
struct WorkloadInput {
  explicit WorkloadInput(std::size_t) {}
};

template <>
struct WorkloadInput<Shape::StatisticsPayload> {
  std::vector<Star::String> queries;

  explicit WorkloadInput(std::size_t size) {
    queries.reserve(2 * size);
    for (std::size_t i = 0; i < 2 * size; ++i)
      queries.emplace_back(std::format("benchmark.statistics.non_sso_event_name.{:06}", i));
  }
};

// The actual StatisticsDatabase ownership/payload shape without Root, assets,
// script execution, or database loading. String/shared_ptr and these three
// payload field types match StatEvent; this is not the production StatEvent type.
struct RepresentativeStatEvent {
  Star::String eventName;
  Star::StringList scripts;
  Star::Json config;
};

using StatisticsValue = std::shared_ptr<RepresentativeStatEvent>;
using StatisticsPair = std::pair<Star::String const, StatisticsValue>;

template <bool Bounded>
constexpr std::pmr::pool_options poolOptions() {
  if constexpr (Bounded) {
    // Budget the actual String/shared_ptr pair plus two list links, with room
    // up to 128 bytes for node alignment/size-class rounding. Report pair sizeof
    // below rather than assuming one ABI. Populated hash bucket arrays at
    // capacities 128/256/512 exceed this cutoff and go directly upstream.
    static_assert(sizeof(StatisticsPair) + 2 * sizeof(void*) <= 128);
    // Cap chunk growth, not total retained bytes: the resource may retain
    // multiple chunks until destruction. Never describe this as a hard limit.
    return {.max_blocks_per_chunk = 32, .largest_required_pool_block = 128};
  } else {
    return {};
  }
}

// Query the actual std::list node allocation size outside both timing and the
// upstream measurement pass. This uses the same value type/raw-pointer node
// representation as the cache's rebound allocator, without naming libc++ internals.
class StatisticsNodeProbe final : public std::pmr::memory_resource {
public:
  std::size_t nodeBytes = 0;

private:
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    nodeBytes = bytes;
    return std::pmr::new_delete_resource()->allocate(bytes, alignment);
  }
  void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
    std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
  }
  bool do_is_equal(std::pmr::memory_resource const& other) const noexcept override {
    return this == &other;
  }
};

std::size_t statisticsListNodeBytes() {
  StatisticsNodeProbe requests;
  std::pmr::list<StatisticsPair> node{std::pmr::polymorphic_allocator<StatisticsPair>{&requests}};
  node.emplace_back(Star::String{}, StatisticsValue{});
  return requests.nodeBytes;
}

template <bool Pool, Shape Workload>
struct Fixture;

template <bool Pool>
struct Fixture<Pool, Shape::List> {
  std::list<Pair, NodeAllocator<Pool, Pair>> entries;
  std::size_t capacity;
  Key next;

  Fixture(Backend<Pool>& backend, std::size_t size, WorkloadInput<Shape::List> const&)
      : entries(nodeAllocator<Pool, Pair>(backend)), capacity(size), next(size) {
    for (Key key = 0; key < size; ++key)
      entries.emplace_back(key, valueFor(key));
  }
  bool churn() {
    for (std::size_t i = 0; i < capacity; ++i) {
      auto key = next++ % (2 * capacity);
      benchmark::DoNotOptimize(entries.back().second);
      entries.pop_front();
      entries.emplace_back(key, valueFor(key));
    }
    return entries.size() == capacity && entries.back().second == valueFor((next - 1) % (2 * capacity));
  }
  void clear() { entries.clear(); }
};

template <bool Pool>
struct Fixture<Pool, Shape::Map> {
  std::map<Key, Value, std::less<Key>, NodeAllocator<Pool, Pair>> entries;
  std::size_t capacity;
  Key next;

  Fixture(Backend<Pool>& backend, std::size_t size, WorkloadInput<Shape::Map> const&)
      : entries(std::less<Key>{}, nodeAllocator<Pool, Pair>(backend)), capacity(size), next(size) {
    for (Key key = 0; key < size; ++key)
      entries.emplace(key, valueFor(key));
  }
  bool churn() {
    for (std::size_t i = 0; i < capacity; ++i) {
      auto hit = entries.find((next - 1) % (2 * capacity));
      if (hit == entries.end() || hit->second != valueFor(hit->first))
        return false;
      benchmark::DoNotOptimize(hit->second);
      entries.erase((next - capacity) % (2 * capacity));
      auto key = next++ % (2 * capacity);
      entries.emplace(key, valueFor(key));
    }
    return entries.size() == capacity;
  }
  void clear() { entries.clear(); }
};

template <bool Pool, Shape Workload>
  requires (Workload == Shape::Lru || Workload == Shape::HashLru || Workload == Shape::HashTtl)
struct Fixture<Pool, Workload> {
  using Allocator = CacheAllocator<Pool, Pair>;
  using TtlAllocator = CacheAllocator<Pool, std::pair<Key const, std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>, Value>>>;
  using Cache = std::conditional_t<Workload == Shape::Lru,
      Star::LruCache<Key, Value, std::less<Key>, Allocator>,
      std::conditional_t<Workload == Shape::HashLru,
          Star::HashLruCache<Key, Value, Star::hash<Key>, std::equal_to<Key>, Allocator>,
          Star::HashTtlCache<Key, Value, Star::hash<Key>, std::equal_to<Key>, TtlAllocator>>>;

  // Seeded local service, not process/global randomness. It outlives the TTL
  // cache. TTL is deliberately non-expiring: this measures node/cache churn,
  // not variable wall-clock expiration or random smear.
  Star::RandomService random{0x9e3779b97f4a7c15ull};
  Cache entries;
  std::size_t capacity;
  Key next;
  bool valid = true;

  static Cache makeCache(Star::RandomService& random, std::size_t size) {
    if constexpr (Workload == Shape::HashTtl)
      return Cache(random, std::chrono::milliseconds(86'400'000), std::chrono::milliseconds(0), size, false);
    else
      return Cache(size);
  }

  Fixture(Backend<Pool>&, std::size_t size, WorkloadInput<Workload> const&)
      : entries(makeCache(random, size)), capacity(size), next(size) {
    for (Key key = 0; key < size; ++key) {
      auto result = entries.get(key, valueFor);
      if (!result || result->get() != valueFor(key)) {
        valid = false;
        return;
      }
    }
  }
  bool churn() {
    if (!valid)
      return false;
    for (std::size_t i = 0; i < capacity; ++i) {
      auto hitKey = (next - 1) % (2 * capacity);
      auto hit = entries.get(hitKey, valueFor);
      if (!hit || hit->get() != valueFor(hitKey))
        return false;
      benchmark::DoNotOptimize(hit->get());
      auto key = next++ % (2 * capacity);
      auto miss = entries.get(key, valueFor);
      if (!miss || miss->get() != valueFor(key))
        return false;
      benchmark::DoNotOptimize(miss->get());
    }
    return entries.size() == capacity;
  }
  void clear() { entries.clear(); }
};

template <bool Pool>
struct Fixture<Pool, Shape::StatisticsPayload> {
  using Allocator = CacheAllocator<Pool, StatisticsPair>;
  Star::HashLruCache<Star::String, StatisticsValue, Star::hash<Star::String>,
      std::equal_to<Star::String>, Allocator> entries;
  std::span<Star::String const> queries;
  std::size_t capacity;
  std::size_t next;
  std::uint64_t produced = 0;
  bool valid = true;

  StatisticsValue produce(Star::String const& key) {
    ++produced;
    return std::make_shared<RepresentativeStatEvent>(RepresentativeStatEvent{
        key,
        Star::StringList{"/scripts/benchmark/statistics_event.lua"},
        Star::Json(Star::JsonObject{
            {"eventName", Star::Json(key)},
            {"scripts", Star::Json(Star::JsonArray{Star::Json("/scripts/benchmark/statistics_event.lua")})},
        }),
    });
  }

  Fixture(Backend<Pool>&, std::size_t size, WorkloadInput<Shape::StatisticsPayload> const& input)
      : entries(size), queries(input.queries), capacity(size), next(size) {
    auto producer = [&self = *this](Star::String const& key) { return self.produce(key); };
    for (std::size_t i = 0; i < size; ++i) {
      auto result = entries.get(queries[i], producer);
      if (!result || !result->get() || result->get()->eventName != queries[i]) {
        valid = false;
        return;
      }
    }
  }

  bool churn() {
    if (!valid)
      return false;
    auto producer = [&self = *this](Star::String const& key) { return self.produce(key); };
    auto before = produced;
    for (std::size_t i = 0; i < capacity; ++i) {
      auto const& hitKey = queries[(next - 1) % queries.size()];
      auto hit = entries.get(hitKey, producer);
      if (!hit || !hit->get() || hit->get()->eventName != hitKey)
        return false;
      // StatisticsDatabase returns an owning shared_ptr, not a cache-node
      // pointer. Exercise that copy/release too; no node or allocator escapes.
      auto returnedHit = hit->get();
      benchmark::DoNotOptimize(returnedHit);
      auto const& key = queries[next++ % queries.size()];
      auto miss = entries.get(key, producer);
      if (!miss || !miss->get() || miss->get()->eventName != key)
        return false;
      auto returnedMiss = miss->get();
      benchmark::DoNotOptimize(returnedMiss);
    }
    return entries.size() == capacity && produced - before == capacity;
  }

  void clear() { entries.clear(); }
};

struct MemoryResult {
  std::uint64_t coldCalls = 0;
  std::uint64_t coldBytes = 0;
  std::uint64_t coldLiveBytes = 0;
  std::uint64_t setupCalls = 0;
  std::uint64_t steadyCalls = 0;
  std::uint64_t steadyBytes = 0;
  std::uint64_t populatedBytes = 0;
  std::uint64_t clearBytes = 0;
  std::uint64_t containerDestroyedBytes = 0;
  std::uint64_t resourceDestroyedBytes = 0;
  std::pmr::pool_options effectivePoolOptions{};
};

template <bool Pool, Shape Workload, bool Bounded = false>
bool reportMemory(benchmark::State& state, std::size_t size, WorkloadInput<Workload> const& input) {
  std::size_t listNodeBytes = 0;
  if constexpr (Bounded) {
    listNodeBytes = statisticsListNodeBytes();
    if (listNodeBytes > poolOptions<Bounded>().largest_required_pool_block) {
      state.SkipWithError("Bounded pool cutoff does not cover the actual statistics list node");
      return false;
    }
  }
  // This independent, untimed pass counts one fill + four warm sweeps + one
  // steady sweep. No benchmark framework calls or diagnostics run while tracked.
  // Allocate the fixed-capacity table before tracking, off the small MinGW
  // default stack. Its storage and the prebuilt query names are not measured.
  auto probeStorage = std::make_unique<AllocationProbe::Statistics>();
  auto& statistics = *probeStorage;
  MemoryResult result;
  bool valid = true;
  AllocationProbe::active = &statistics;
  {
    Backend<Pool> backend{poolOptions<Bounded>()};
    if constexpr (Bounded)
      result.effectivePoolOptions = backend.resource.options();
    {
      Fixture<Pool, Workload> fixture(backend, size, input);
      result.coldCalls = statistics.calls;
      result.coldBytes = statistics.requestedBytes;
      result.coldLiveBytes = statistics.liveBytes;
      for (int i = 0; i < 4; ++i)
        valid = fixture.churn() && valid;
      result.setupCalls = statistics.calls;
      auto beforeBytes = statistics.requestedBytes;
      valid = fixture.churn() && valid;
      result.steadyCalls = statistics.calls - result.setupCalls;
      result.steadyBytes = statistics.requestedBytes - beforeBytes;
      result.populatedBytes = statistics.liveBytes;
      fixture.clear();
      result.clearBytes = statistics.liveBytes;
    }
    result.containerDestroyedBytes = statistics.liveBytes;
  }
  result.resourceDestroyedBytes = statistics.liveBytes;
  AllocationProbe::active = nullptr;
  if (!valid || statistics.overflow || result.resourceDestroyedBytes != 0 || statistics.liveCalls != 0) {
    state.SkipWithError("Allocator fixture failed, probe overflowed, or upstream storage outlived its owner");
    return false;
  }
  state.counters["cold_alloc_calls"] = static_cast<double>(result.coldCalls);
  state.counters["cold_requested_bytes"] = static_cast<double>(result.coldBytes);
  state.counters["cold_live_bytes"] = static_cast<double>(result.coldLiveBytes);
  state.counters["setup_alloc_calls"] = static_cast<double>(result.setupCalls);
  state.counters["steady_alloc_calls"] = static_cast<double>(result.steadyCalls);
  state.counters["steady_requested_bytes"] = static_cast<double>(result.steadyBytes);
  state.counters["total_requested_bytes"] = static_cast<double>(statistics.requestedBytes);
  state.counters["peak_live_bytes"] = static_cast<double>(statistics.peakBytes);
  state.counters["peak_live_allocations"] = static_cast<double>(statistics.peakLiveCalls);
  state.counters["probe_capacity"] = static_cast<double>(AllocationProbe::Statistics::Capacity);
  state.counters["populated_live_bytes"] = static_cast<double>(result.populatedBytes);
  state.counters["after_clear_live_bytes"] = static_cast<double>(result.clearBytes);
  state.counters["after_container_destroy_live_bytes"] = static_cast<double>(result.containerDestroyedBytes);
  state.counters["after_resource_destroy_live_bytes"] = static_cast<double>(result.resourceDestroyedBytes);
  if constexpr (Bounded) {
    state.counters["pool_max_blocks_per_chunk"] =
        static_cast<double>(result.effectivePoolOptions.max_blocks_per_chunk);
    state.counters["pool_largest_required_block"] =
        static_cast<double>(result.effectivePoolOptions.largest_required_pool_block);
    state.counters["statistics_pair_bytes"] = static_cast<double>(sizeof(StatisticsPair));
    state.counters["statistics_list_node_bytes"] = static_cast<double>(listNodeBytes);
  }
  if constexpr (Workload == Shape::StatisticsPayload)
    state.counters["logical_pair_bytes"] = static_cast<double>(size * sizeof(StatisticsPair));
  else
    state.counters["logical_pair_bytes"] = static_cast<double>(size * sizeof(Pair));
  return true;
}

template <bool Pool, Shape Workload, bool Cold, bool Bounded = false>
void BM_Allocator(benchmark::State& state) {
  static_assert(!Bounded || (Pool && Workload == Shape::StatisticsPayload));
  auto size = static_cast<std::size_t>(state.range(0));
  WorkloadInput<Workload> input(size);
  if (!reportMemory<Pool, Workload, Bounded>(state, size, input))
    return;
  if constexpr (Workload == Shape::StatisticsPayload) {
    state.SetLabel(Bounded
        ? "PMR chunk cap 32 / pooled cutoff 128B; String/shared_ptr<StatEvent-shaped payload>; no Root/assets/I/O"
        : (Pool
            ? "PMR; String/shared_ptr<StatEvent-shaped payload>; no Root/assets/I/O; prebuilt query names"
            : "Block; String/shared_ptr<StatEvent-shaped payload>; no Root/assets/I/O; prebuilt query names"));
  } else {
    state.SetLabel(Pool ? "local unsynchronized_pool_resource; default pool_options"
                        : "live BlockAllocator; block size 1024");
  }
  if constexpr (Cold) {
    // Includes family/resource construction, fill and complete destruction.
    for (auto _ : state) {
      (void)_;
      Backend<Pool> backend{poolOptions<Bounded>()};
      Fixture<Pool, Workload> fixture(backend, size, input);
      benchmark::DoNotOptimize(fixture);
      benchmark::ClobberMemory();
    }
  } else {
    Backend<Pool> backend{poolOptions<Bounded>()};
    Fixture<Pool, Workload> fixture(backend, size, input);
    for (int i = 0; i < 4; ++i) {
      if (!fixture.churn()) {
        state.SkipWithError("Allocator warmup failed");
        return;
      }
    }
    // One full capacity sweep per iteration. Caches have one MRU hit + one
    // miss/eviction per replacement; map has one hit + erase + insert; list
    // has a back read + front erase + back insert. Identical within each pair.
    for (auto _ : state) {
      (void)_;
      if (!fixture.churn()) {
        state.SkipWithError("Allocator steady-state workload failed");
        return;
      }
      benchmark::ClobberMemory();
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(size));
}

// Include below-block, exact-block, above-block and large cache scales. The
// 20,000 case matches CacheSelector's configured default; other cases expose
// fixed family overhead and large-cache retention instead of assuming one wins.
#define ALLOCATOR_CASE(POOL, SHAPE, COLD, NAME) \
  BENCHMARK_TEMPLATE(BM_Allocator, POOL, Shape::SHAPE, COLD) \
      ->Name(NAME)->Arg(16)->Arg(256)->Arg(1024)->Arg(1025)->Arg(20000)->Arg(65536)
#define ALLOCATOR_PAIR(SHAPE) \
  ALLOCATOR_CASE(false, SHAPE, true, "Allocator/" #SHAPE "/Block/Cold"); \
  ALLOCATOR_CASE(true, SHAPE, true, "Allocator/" #SHAPE "/Pmr/Cold"); \
  ALLOCATOR_CASE(false, SHAPE, false, "Allocator/" #SHAPE "/Block/Steady"); \
  ALLOCATOR_CASE(true, SHAPE, false, "Allocator/" #SHAPE "/Pmr/Steady")

ALLOCATOR_PAIR(List);
ALLOCATOR_PAIR(Map);
ALLOCATOR_PAIR(Lru);
ALLOCATOR_PAIR(HashLru);
ALLOCATOR_PAIR(HashTtl);

// Match StatisticsDatabase's default capacity and nearby working-set sizes.
// Bound this payload-heavy case separately: generic node cases still reach 64K.
BENCHMARK_TEMPLATE(BM_Allocator, false, Shape::StatisticsPayload, true)
    ->Name("Allocator/StatisticsPayload/Block/Cold")->Arg(128)->Arg(256)->Arg(512);
BENCHMARK_TEMPLATE(BM_Allocator, true, Shape::StatisticsPayload, true)
    ->Name("Allocator/StatisticsPayload/Pmr/Cold")->Arg(128)->Arg(256)->Arg(512);
BENCHMARK_TEMPLATE(BM_Allocator, false, Shape::StatisticsPayload, false)
    ->Name("Allocator/StatisticsPayload/Block/Steady")->Arg(128)->Arg(256)->Arg(512);
BENCHMARK_TEMPLATE(BM_Allocator, true, Shape::StatisticsPayload, false)
    ->Name("Allocator/StatisticsPayload/Pmr/Steady")->Arg(128)->Arg(256)->Arg(512);
BENCHMARK_TEMPLATE(BM_Allocator, true, Shape::StatisticsPayload, true, true)
    ->Name("Allocator/StatisticsPayload/PmrBounded/Cold")->Arg(128)->Arg(256)->Arg(512);
BENCHMARK_TEMPLATE(BM_Allocator, true, Shape::StatisticsPayload, false, true)
    ->Name("Allocator/StatisticsPayload/PmrBounded/Steady")->Arg(128)->Arg(256)->Arg(512);

#undef ALLOCATOR_PAIR
#undef ALLOCATOR_CASE

}

class CheckedReporter : public benchmark::ConsoleReporter {
public:
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& run : reports)
      failed |= run.skipped == benchmark::internal::SkippedWithError;
    benchmark::ConsoleReporter::ReportRuns(reports);
  }
  bool failed = false;
};

int main(int argc, char** argv) {
  auto nativeRuntime = Star::NativeRuntime::create();
  if (!nativeRuntime) {
    if (!Star::print(stderr, "{}\n", nativeRuntime.error().what()))
      return 1;
    return 1;
  }
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    benchmark::Shutdown();
    return 1;
  }
  CheckedReporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}
