#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.flat_hash_map;
import star.flat_hash_set;
import star.format;
import star.hash;
import star.native_runtime;
import star.string;

// Private before-build adaptation changes only these two flat module imports
// and this alias. Keep the consumer, fixtures and validators identical.
namespace flat_storage = Star;

namespace {

std::uint64_t runtime_salt;

using string_map = flat_storage::FlatHashMap<Star::String, std::uint64_t>;
using expensive_value = std::vector<std::uint64_t>;
using expensive_map = flat_storage::FlatHashMap<Star::String, expensive_value>;
using owning_set = flat_storage::FlatHashSet<Star::String>;

struct inline_payload {
  std::array<std::uint64_t, 32> words;
  bool operator==(inline_payload const&) const = default;
};

struct constant_hash {
  std::size_t operator()(std::uint64_t) const { return 7; }
};

using inline_map = flat_storage::FlatHashMap<std::uint64_t, inline_payload>;
using collision_map = flat_storage::FlatHashMap<std::uint64_t, inline_payload, constant_hash>;

template <typename T>
concept benchmark_map = std::same_as<T, string_map> || std::same_as<T, expensive_map>
  || std::same_as<T, inline_map> || std::same_as<T, collision_map>;

template <benchmark_map Map>
using expected_entries = std::vector<std::pair<typename Map::key_type, typename Map::mapped_type>>;

bool require(benchmark::State& state, bool condition, char const* message) {
  if (!condition)
    state.SkipWithError(message);
  return condition;
}

// Independent ordered membership also examines every iterated complete value;
// size alone would not detect duplicate iteration or a wrong mapped payload.
template <benchmark_map Map>
bool validate_map(Map const& actual, expected_entries<Map> const& expected) {
  std::map<typename Map::key_type, typename Map::mapped_type> remaining(expected.begin(), expected.end());
  if (remaining.size() != expected.size() || actual.size() != expected.size())
    return false;
  for (auto const& entry : actual) {
    auto found = remaining.find(entry.first);
    if (found == remaining.end() || found->second != entry.second)
      return false;
    remaining.erase(found);
  }
  if (!remaining.empty())
    return false;
  for (auto const& entry : expected) {
    auto found = actual.find(entry.first);
    if (found == actual.end() || found->first != entry.first || found->second != entry.second)
      return false;
  }
  return true;
}

bool validate_set(owning_set const& actual, std::vector<Star::String> const& expected) {
  std::set<Star::String> remaining(expected.begin(), expected.end());
  if (actual.size() != expected.size() || remaining.size() != expected.size())
    return false;
  for (auto const& key : actual)
    if (remaining.erase(key) != 1)
      return false;
  if (!remaining.empty())
    return false;
  for (auto const& key : expected) {
    auto found = actual.find(key);
    if (found == actual.end() || *found != key)
      return false;
  }
  return true;
}

Star::String make_key(std::size_t index, std::size_t extra_bytes = 48) {
  auto bytes = std::format("/flat/{:08}/{:08}/", runtime_salt, index);
  bytes.push_back('\0');
  bytes += "\xc3\xa9/";
  bytes.append(extra_bytes, static_cast<char>('a' + index % 26));
  return Star::String(std::string_view(bytes));
}

inline_payload make_payload(std::size_t index) {
  inline_payload result;
  for (std::size_t i = 0; i != result.words.size(); ++i)
    result.words[i] = (index + 1) * 0x9e3779b97f4a7c15ULL + i * 17 + runtime_salt;
  return result;
}

expected_entries<inline_map> inline_entries(std::size_t count, std::size_t offset = 0,
                                          std::size_t stride = 1) {
  expected_entries<inline_map> result;
  result.reserve(count);
  for (std::size_t i = 0; i != count; ++i) {
    auto index = offset + i * stride;
    result.emplace_back(index + runtime_salt, make_payload(index));
  }
  return result;
}

template <benchmark_map Map>
std::size_t fill_map(Map& map, expected_entries<Map> const& input) {
  std::size_t inserted = 0;
  for (auto const& entry : input)
    inserted += map.try_emplace(entry.first, entry.second).second;
  return inserted;
}

void count_items(benchmark::State& state, std::size_t count) {
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

template <bool Reserved, bool Ordinary = false>
void new_string_insert(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  expected_entries<string_map> input;
  input.reserve(count);
  for (std::size_t i = 0; i != count; ++i)
    input.emplace_back(make_key(i), i + runtime_salt);
  std::vector<string_map::value_type> candidates;
  if constexpr (Ordinary) {
    candidates.reserve(count);
    for (auto const& entry : input)
      candidates.emplace_back(entry.first, entry.second);
  }
  auto create = [&] {
    string_map result;
    if constexpr (Reserved)
      result.reserve(count);
    std::size_t inserted = 0;
    if constexpr (Ordinary) {
      for (string_map::value_type const& candidate : candidates)
        inserted += result.insert(candidate).second;
    } else {
      inserted = fill_map(result, input);
    }
    benchmark::DoNotOptimize(inserted);
    return result;
  };
  {
    auto probe = create();
    if (!require(state, validate_map(probe, input), "new insertion lost full keys or values"))
      return;
  }
  state.SetLabel("empty owner + optional reserve + N insertions + destruction timed; input owners untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    auto owner = create();
    benchmark::DoNotOptimize(owner);
    benchmark::ClobberMemory();
  }
  auto probe = create();
  if (!require(state, validate_map(probe, input), "post-run new insertion differs from fixture"))
    return;
  count_items(state, count);
}

template <bool Hinted>
void expensive_duplicate(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  expected_entries<expensive_map> expected;
  std::vector<expensive_map::value_type> candidates;
  expected.reserve(count);
  candidates.reserve(count);
  for (std::size_t i = 0; i != count; ++i) {
    expensive_value resident(512, i + runtime_salt);
    resident.back() ^= 0xdeadbeef;
    expected.emplace_back(make_key(i), std::move(resident));
    // Deliberately different duplicate values must never replace residents.
    candidates.emplace_back(expected.back().first, expensive_value(512, i + runtime_salt + 999));
  }
  expensive_map owner;
  fill_map(owner, expected);
  auto duplicate = [&] {
    std::uint64_t observed = 0;
    // const is essential: mutable value_type could select the forwarding API.
    for (expensive_map::value_type const& candidate : candidates) {
      if constexpr (Hinted) {
        auto found = owner.insert(owner.cend(), candidate);
        observed += found->second.back();
      } else {
        auto result = owner.insert(candidate);
        observed += result.first->second.back() + result.second;
      }
    }
    return observed;
  };
  duplicate();
  if (!require(state, validate_map(owner, expected), "duplicate map insert changed full membership or payloads"))
    return;
  state.SetLabel("N const lvalue duplicate inserts; 4096-byte candidate payload; all owners/destruction untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    auto observed = duplicate();
    benchmark::DoNotOptimize(observed);
    benchmark::ClobberMemory();
  }
  if (!require(state, validate_map(owner, expected), "post-run duplicate map membership or payload mismatch"))
    return;
  count_items(state, count);
}

template <bool Range>
void owning_set_duplicates(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  std::vector<Star::String> input;
  input.reserve(count);
  for (std::size_t i = 0; i != count; ++i)
    input.push_back(make_key(i, 1024));
  owning_set owner;
  owner.insert(input.cbegin(), input.cend());
  auto duplicate = [&] {
    if constexpr (Range)
      owner.insert(input.cbegin(), input.cend());
    else {
      std::size_t inserted = 0;
      for (auto const& key : input)
        inserted += owner.insert(key).second;
      benchmark::DoNotOptimize(inserted);
    }
    benchmark::DoNotOptimize(owner);
  };
  duplicate();
  if (!require(state, validate_set(owner, input), "duplicate set insert changed full-byte keys"))
    return;
  state.SetLabel("N duplicate const owning String keys; single/range insertion only; owners/destruction untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    duplicate();
    benchmark::ClobberMemory();
  }
  if (!require(state, validate_set(owner, input), "post-run duplicate set full membership mismatch"))
    return;
  count_items(state, count);
}

void collision_rehash(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto input = inline_entries(count);
  collision_map fixture;
  fill_map(fixture, input);
  collision_map owner;
  if (!require(state, validate_map(fixture, input), "collision fixture lost full keys or payloads"))
    return;
  owner = fixture;
  owner.reserve(count * 4);
  if (!require(state, validate_map(owner, input), "collision reserve changed full membership or payloads"))
    return;
  state.SetLabel("reserve 4N only; all keys share full hash; copy/setup/destruction untimed; items=N relocated residents");
  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    owner = fixture;
    state.ResumeTiming();
    benchmark::ClobberMemory();
    owner.reserve(count * 4);
    benchmark::DoNotOptimize(owner);
    benchmark::ClobberMemory();
  }
  if (!require(state, validate_map(owner, input), "post-run collision rehash lost full keys or payloads"))
    return;
  count_items(state, count);
}

template <bool Hit>
void string_lookup(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  expected_entries<string_map> expected;
  std::vector<Star::String> queries;
  expected.reserve(count);
  queries.reserve(count);
  for (std::size_t i = 0; i != count; ++i) {
    expected.emplace_back(make_key(i), i + runtime_salt);
    queries.push_back(make_key(Hit ? i : count + i));
  }
  string_map owner;
  fill_map(owner, expected);
  auto validate = [&] {
    if (!validate_map(owner, expected))
      return false;
    for (std::size_t i = 0; i != count; ++i) {
      auto found = owner.find(queries[i]);
      if constexpr (Hit) {
        if (found == owner.end() || found->first != queries[i] || found->second != i + runtime_salt)
          return false;
      } else if (found != owner.end())
        return false;
    }
    return true;
  };
  if (!require(state, validate(), "lookup fixture full membership or expected hit/miss mismatch"))
    return;
  state.SetLabel("N find operations; table and query/key owners/destruction untimed; full-byte String keys");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    std::uint64_t observed = 0;
    for (auto const& query : queries) {
      auto found = owner.find(query);
      observed += found == owner.end() ? 1 : found->second;
    }
    benchmark::DoNotOptimize(observed);
  }
  if (!require(state, validate(), "post-run lookup full membership or hit/miss mismatch"))
    return;
  count_items(state, count);
}

void inline_churn(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto first = inline_entries(count);
  auto second = inline_entries(count, count);
  inline_map owner;
  fill_map(owner, first);
  auto churn = [&](expected_entries<inline_map> const& old_entries,
                   expected_entries<inline_map> const& new_entries) {
    std::size_t operations = 0;
    for (std::size_t i = 0; i != count; ++i) {
      operations += owner.erase(old_entries[i].first);
      operations += owner.try_emplace(new_entries[i].first, new_entries[i].second).second;
    }
    return operations;
  };
  if (!require(state, validate_map(owner, first), "churn initial full membership mismatch"))
    return;
  auto operations = churn(first, second);
  if (!require(state, operations == 2 * count && validate_map(owner, second), "churn forward full membership mismatch"))
    return;
  operations = churn(second, first);
  if (!require(state, operations == 2 * count && validate_map(owner, first), "churn reverse full membership mismatch"))
    return;
  bool alternate = false;
  state.SetLabel("interleaved erase + insertion; 256-byte inline payload; owners/destruction untimed; items=2N operations");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    operations = alternate ? churn(second, first) : churn(first, second);
    alternate = !alternate;
    benchmark::DoNotOptimize(operations);
    benchmark::ClobberMemory();
  }
  if (!require(state, validate_map(owner, alternate ? second : first), "post-run churn full membership mismatch"))
    return;
  count_items(state, 2 * count);
}

void retained_clear_reuse(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto burst = inline_entries(count * 16);
  auto input = inline_entries(count);
  inline_map owner;
  fill_map(owner, burst);
  if (!require(state, validate_map(owner, burst), "clear/reuse burst full membership mismatch"))
    return;
  owner.clear();
  if (!require(state, validate_map(owner, expected_entries<inline_map>{}), "clear retained live entries"))
    return;
  fill_map(owner, input);
  if (!require(state, validate_map(owner, input), "clear/reuse initial full membership mismatch"))
    return;
  state.SetLabel("clear N + refill N in capacity retained from 16N burst; owners/destruction untimed; items=N reused entries");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    owner.clear();
    auto inserted = fill_map(owner, input);
    benchmark::DoNotOptimize(inserted);
    benchmark::DoNotOptimize(owner);
    benchmark::ClobberMemory();
  }
  if (!require(state, validate_map(owner, input), "post-run clear/reuse full membership mismatch"))
    return;
  count_items(state, count);
}

void sparse_iteration(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto burst = inline_entries(count * 16);
  auto expected = inline_entries(count, 0, 16);
  inline_map owner;
  fill_map(owner, burst);
  for (std::size_t i = 0; i != burst.size(); ++i)
    if (i % 16 != 0)
      owner.erase(burst[i].first);
  if (!require(state, validate_map(owner, expected), "sparse iteration initial full membership mismatch"))
    return;
  state.SetLabel("visit N survivors after 16N burst/erase; retained buckets; owners/destruction untimed; items=N visited entries");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    std::uint64_t observed = 0;
    for (auto const& entry : owner)
      observed += entry.first ^ entry.second.words.front() ^ entry.second.words.back();
    benchmark::DoNotOptimize(observed);
  }
  if (!require(state, validate_map(owner, expected), "post-run sparse iteration full membership mismatch"))
    return;
  count_items(state, count);
}

class checked_reporter : public benchmark::ConsoleReporter {
public:
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& run : reports)
      failed |= run.skipped == benchmark::internal::SkippedWithError;
    benchmark::ConsoleReporter::ReportRuns(reports);
  }
  bool failed = false;
};

}// namespace

int main(int argc, char** argv) {
  auto native_runtime = Star::NativeRuntime::create();
  if (!native_runtime) {
    if (!Star::print(stderr, "{}\n", native_runtime.error().what()))
      return 1;
    return 1;
  }
  // Independent of command-line filters and executable path in before/after.
  runtime_salt = 73;
  benchmark::DoNotOptimize(runtime_salt);
  benchmark::RegisterBenchmark("FlatHash/StringMap/NewInsertGrowConstructDestroy", &new_string_insert<false>)
    ->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/StringMap/NewInsertReservedConstructDestroy", &new_string_insert<true>)
    ->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/StringMap/NewConstInsertGrowConstructDestroy", &new_string_insert<false, true>)
    ->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/StringMap/NewConstInsertReservedConstructDestroy", &new_string_insert<true, true>)
    ->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/StringMap/ExpensiveDuplicateConstInsert", &expensive_duplicate<false>)
    ->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/StringMap/ExpensiveDuplicateConstHintInsert", &expensive_duplicate<true>)
    ->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/OwningSet/DuplicateConstSingleInsert", &owning_set_duplicates<false>)
    ->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/OwningSet/DuplicateConstRangeInsert", &owning_set_duplicates<true>)
    ->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/CollisionMap/ReserveRehashOnly", &collision_rehash)->Arg(64)->Arg(256);
  benchmark::RegisterBenchmark("FlatHash/StringMap/FindHit", &string_lookup<true>)->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/StringMap/FindMiss", &string_lookup<false>)->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/InlineMap/EraseReinsertChurn", &inline_churn)->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/InlineMap/RetainedCapacityClearReuse", &retained_clear_reuse)
    ->Arg(128)->Arg(1024);
  benchmark::RegisterBenchmark("FlatHash/InlineMap/SparseIteration", &sparse_iteration)->Arg(128)->Arg(1024);
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    benchmark::Shutdown();
    return 1;
  }
  checked_reporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}
