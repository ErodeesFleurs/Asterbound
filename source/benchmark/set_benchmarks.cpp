#include <benchmark/benchmark.h>

import std;
import star.list;
import star.native_runtime;
import star.ordered_set;
import star.set;
import star.string;

namespace {

// Freeze this consumer for both builds. The PRIVATE old-source copy changes only
// .insert_range( to .addAll( and .take_first( to .takeFirst(.
// In particular, std::move below is intentional even when old OrderedHashSet
// resolves it to its copy constructor. No synthetic move API is supplied.
std::uint64_t runtime_salt;

template <typename T>
concept bench_key = std::same_as<T, int> || std::same_as<T, Star::String>;

template <typename S>
concept bench_set = std::ranges::forward_range<S const> && bench_key<typename S::value_type>
  && std::copy_constructible<S> && requires(S& set, S const& source,
                                           std::vector<typename S::value_type> const& input) {
    S(input.begin(), input.end());
    { source.contains(input.front()) } -> std::convertible_to<bool>;
    source.values();
    source.intersection(source);
    source.difference(source);
    set.insert_range(input);
    set.take_first();
  };

enum class operation { construct, copy, values, move, hit, miss, intersection, difference, duplicates, take_first };

template <bench_key T> T make_key(std::size_t index) {
  if constexpr (std::same_as<T, int>)
    return static_cast<int>(index + runtime_salt);
  else {
    auto bytes = std::format("/set/{:08}/{:08}/", runtime_salt, index);
    bytes.append(96, static_cast<char>('a' + index % 26));
    bytes[3] = '\0';
    bytes[8] = '\xc3';
    bytes[9] = '\xa9';
    return Star::String(std::string_view(bytes));
  }
}

template <typename T> void escape(T const& value) {
  benchmark::DoNotOptimize(std::addressof(value));
  benchmark::ClobberMemory();
}

template <bench_key T> void escape_key(T const& value) {
  if constexpr (std::same_as<T, Star::String>) {
    benchmark::DoNotOptimize(value.utf8().data());
    benchmark::DoNotOptimize(value.utf8().size());
  } else
    benchmark::DoNotOptimize(value);
}

// Independent standard-tree membership validates every complete value, including
// the long String suffix after NUL/UTF-8. Never use legacy HashSet equality.
// For ordered containers, compare the complete sequence as well as membership.
template <std::ranges::input_range R, bench_key T>
bool validate_values(R const& actual, std::vector<T> const& expected, bool ordered) {
  std::set<T> remaining(expected.begin(), expected.end());
  auto position = expected.begin();
  for (auto const& value : actual) {
    if (remaining.erase(value) != 1)
      return false;
    if (ordered && (position == expected.end() || value != *position))
      return false;
    if (position != expected.end())
      ++position;
  }
  return remaining.empty() && position == expected.end();
}

template <bench_set S>
bool validate_members(S const& actual, std::vector<typename S::value_type> const& expected,
                      std::vector<typename S::value_type> const& misses, bool ordered) {
  if (actual.size() != expected.size() || !validate_values(actual, expected, ordered))
    return false;
  for (auto const& value : expected)
    if (!actual.contains(value))
      return false;
  for (auto const& value : misses)
    if (actual.contains(value))
      return false;
  return true;
}

template <bench_set S, operation Kind, bool Ordered>
void measure_set(benchmark::State& state) {
  using key = typename S::value_type;
  auto count = static_cast<std::size_t>(state.range(0));
  std::vector<key> input, rhs_input, misses, duplicates;
  input.reserve(count);
  rhs_input.reserve(count);
  misses.reserve(count);
  duplicates.reserve(4 * count);
  // Odd multiplication permutes each power-of-two size; RHS reverses that
  // permutation and overlaps exactly half the keys, exercising ordered algebra.
  for (std::size_t i = 0; i < count; ++i) {
    input.push_back(make_key<key>((17 * i) % count));
    rhs_input.push_back(make_key<key>(count / 2 + (17 * (count - 1 - i)) % count));
    misses.push_back(make_key<key>(2 * count + i));
  }
  for (std::size_t i = 0; i < 4 * count; ++i)
    duplicates.push_back(input[i % count]);
  S source(input.begin(), input.end());
  S rhs(rhs_input.begin(), rhs_input.end());
  if (!validate_members(source, input, misses, Ordered)
      || !validate_members(rhs, rhs_input, misses, Ordered)) {
    state.SkipWithError("set fixture membership/full values/order differ from independent input");
    return;
  }
  std::vector<key> expected = input;
  if constexpr (Kind == operation::intersection || Kind == operation::difference) {
    expected.clear();
    std::set<key> lhs_members(input.begin(), input.end());
    std::set<key> rhs_members(rhs_input.begin(), rhs_input.end());
    auto const& sequence = Kind == operation::intersection ? rhs_input : input;
    for (auto const& value : sequence)
      if (Kind == operation::intersection ? lhs_members.contains(value) : !rhs_members.contains(value))
        expected.push_back(value);
  }

  auto validate_operation = [&]() -> bool {
    if constexpr (Kind == operation::values) {
      auto snapshot = source.values();
      S independent(source);
      auto owned_snapshot = independent.values();
      independent.clear();
      return validate_values(snapshot, input, Ordered) && validate_values(owned_snapshot, input, Ordered);
    } else if constexpr (Kind == operation::take_first) {
      S remaining(source);
      std::set<key> members(input.begin(), input.end());
      for (std::size_t i = 0; i < count; ++i) {
        auto value = remaining.take_first();
        if (!value || members.erase(*value) != 1 || (Ordered && *value != input[i]))
          return false;
        if (remaining.contains(*value) || remaining.size() != count - i - 1)
          return false;
      }
      return members.empty() && remaining.empty() && !remaining.take_first();
    } else if constexpr (Kind == operation::hit || Kind == operation::miss) {
      auto const& queries = Kind == operation::hit ? input : misses;
      for (auto const& query : queries)
        if (source.contains(query) != (Kind == operation::hit))
          return false;
      return true;
    } else {
      auto result = [&]() -> S {
        if constexpr (Kind == operation::construct)
          return S(input.begin(), input.end());
        else if constexpr (Kind == operation::copy)
          return S(source);
        else if constexpr (Kind == operation::move) {
          S reset(source);
          return S(std::move(reset));
        } else if constexpr (Kind == operation::intersection)
          return source.intersection(rhs);
        else if constexpr (Kind == operation::difference)
          return source.difference(rhs);
        else {
          S result;
          result.insert_range(duplicates);
          return result;
        }
      }();
      return validate_members(result, expected, misses, Ordered);
    }
  };
  if (!validate_operation()) {
    state.SkipWithError("set operation differs from independent full-value/membership/order expectations");
    return;
  }

  if constexpr (Kind == operation::move)
    state.SetLabel("one move-expression construction + result destruction timed; fixture copy/reset and moved-from destruction untimed; old ordered implementation copies");
  else if constexpr (Kind == operation::take_first)
    state.SetLabel("N checked take_first calls + extracted value destruction timed; fixture copy/reset/destruction untimed");
  else if constexpr (Kind == operation::hit || Kind == operation::miss)
    state.SetLabel("N owning-key queries timed; key construction/validation untimed");
  else if constexpr (Kind == operation::duplicates)
    state.SetLabel("empty construction + 4N insert_range attempts (N unique) + result destruction timed");
  else
    state.SetLabel("one named result construction/materialization + full destruction timed; fixtures/checks untimed");

  std::size_t last_hits = 0;
  for (auto _ : state) {
    (void)_;
    escape(source);
    escape(rhs);
    benchmark::DoNotOptimize(input.data());
    benchmark::DoNotOptimize(duplicates.data());
    if constexpr (Kind == operation::move || Kind == operation::take_first) {
      state.PauseTiming();
      std::optional<S> reset(std::in_place, source);
      state.ResumeTiming();
      if constexpr (Kind == operation::move) {
        S result(std::move(*reset));
        escape(result);
      } else {
        for (std::size_t i = 0; i < count; ++i) {
          auto value = reset->take_first();
          if (!value) {
            state.SkipWithError(value.error().what());
            return;
          }
          escape_key(*value);
        }
      }
      state.PauseTiming();
      reset.reset();
      state.ResumeTiming();
    } else if constexpr (Kind == operation::hit || Kind == operation::miss) {
      auto const& queries = Kind == operation::hit ? input : misses;
      benchmark::DoNotOptimize(queries.data());
      last_hits = 0;
      for (auto const& query : queries)
        last_hits += source.contains(query);
      benchmark::DoNotOptimize(last_hits);
    } else if constexpr (Kind == operation::values) {
      auto result = source.values();
      escape(result);
    } else {
      auto result = [&]() -> S {
        if constexpr (Kind == operation::construct)
          return S(input.begin(), input.end());
        else if constexpr (Kind == operation::copy)
          return S(source);
        else if constexpr (Kind == operation::intersection)
          return source.intersection(rhs);
        else if constexpr (Kind == operation::difference)
          return source.difference(rhs);
        else {
          S result;
          result.insert_range(duplicates);
          return result;
        }
      }();
      escape(result);
    }
  }
  if constexpr (Kind == operation::hit || Kind == operation::miss) {
    if (last_hits != (Kind == operation::hit ? count : 0))
      state.SkipWithError("timed contains batch returned incorrect hit count");
  }
  if (!validate_operation() || !validate_members(source, input, misses, Ordered))
    state.SkipWithError("post-timing set semantic/full-value/order validation failed");
  // Default ns reports one iteration. Items normalize batched lookups/extraction
  // and duplicate insertion attempts; materializing operations report N inputs.
  auto items = Kind == operation::duplicates ? 4 * count : count;
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(items));
}

template <bench_set S, bool Ordered = false>
void register_set(char const* family, char const* key_name) {
  auto add = [=](char const* name, void (*function)(benchmark::State&)) {
    auto path = std::format("Set/{}/{}/{}", family, key_name, name);
    benchmark::RegisterBenchmark(path.c_str(), function)->Arg(32)->Arg(1024);
  };
  add("RangeConstructDestroy", &measure_set<S, operation::construct, Ordered>);
  add("CopyConstructDestroy", &measure_set<S, operation::copy, Ordered>);
  add("OwningValuesConstructDestroy", &measure_set<S, operation::values, Ordered>);
  add("MoveConstructDestroyResetCopyUntimed", &measure_set<S, operation::move, Ordered>);
  add("ContainsHitBatch", &measure_set<S, operation::hit, Ordered>);
  add("ContainsMissBatch", &measure_set<S, operation::miss, Ordered>);
  add("IntersectionConstructDestroy", &measure_set<S, operation::intersection, Ordered>);
  add("DifferenceConstructDestroy", &measure_set<S, operation::difference, Ordered>);
  add("Duplicate4xInsertRangeConstructDestroy", &measure_set<S, operation::duplicates, Ordered>);
  add("TakeFirstDrainResetCopyUntimed", &measure_set<S, operation::take_first, Ordered>);
}

template <bench_key T> void register_key(char const* name) {
  register_set<Star::Set<T>>("Tree", name);
  register_set<Star::HashSet<T>>("FlatHash", name);
  register_set<Star::StableHashSet<T>>("StableHash", name);
  register_set<Star::OrderedHashSet<T>, true>("OrderedHash", name);
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
  auto runtime = Star::NativeRuntime::create();
  if (!runtime) {
    std::cerr << runtime.error().what() << '\n';
    return 1;
  }
  runtime_salt = static_cast<std::uint64_t>(argc + 73);
  benchmark::DoNotOptimize(runtime_salt);
  register_key<int>("Int");
  register_key<Star::String>("OwningLongString");
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
