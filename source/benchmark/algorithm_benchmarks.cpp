#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.algorithm;
import star.format;
import star.input_event;
import star.list;
import star.benchmark_runner;
import star.string;

namespace {

std::uint64_t runtime_salt;

struct OwningRecord {
  std::int64_t identity = 0;
  Star::String payload;

  bool operator==(OwningRecord const&) const = default;
};

template <typename T>
concept bench_element = std::same_as<T, std::int64_t> || std::same_as<T, OwningRecord>;

enum class order_pattern { ascending, descending, shuffled, equal_keys };
enum class release_pattern { alternating, prefix, absent };

template <bench_element T>
std::int64_t identity_of(T const& value) {
  if constexpr (std::same_as<T, std::int64_t>)
    return value;
  else
    return value.identity;
}

template <bench_element T>
T make_element(std::int64_t identity) {
  if constexpr (std::same_as<T, std::int64_t>)
    return identity;
  else {
    auto bytes = std::string(96, 'x') + std::format("/asset/{:08}", identity);
    bytes[3] = '\0';
    bytes[8] = '\xc3';
    bytes[9] = '\xa9';
    return {identity, Star::String(std::string_view(bytes))};
  }
}

template <bench_element T>
std::vector<T> make_sort_input(std::size_t count, order_pattern pattern) {
  std::vector<T> input;
  input.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    auto key = pattern == order_pattern::equal_keys ? 0 : i / (count / 8);
    auto identity = static_cast<std::int64_t>((runtime_salt * count + i) * 8 + key);
    input.push_back(make_element<T>(identity));
  }
  if (pattern == order_pattern::descending)
    std::reverse(input.begin(), input.end());
  else if (pattern == order_pattern::shuffled) {
    std::mt19937_64 random(runtime_salt);
    std::shuffle(input.begin(), input.end(), random);
  }
  return input;
}

template <bool Stable, bool Checked, bench_element T>
std::expected<void, Star::StarException> sort_values(Star::List<T>& values) {
  auto getter = [](T const& value)
      -> std::conditional_t<Checked, std::expected<std::uint64_t, Star::StarException>, std::uint64_t> {
    return static_cast<std::uint64_t>(identity_of(value)) & 7;
  };
  if constexpr (Checked) {
    if constexpr (Stable)
      return Star::stable_sort_by_key(values, getter);
    else
      return Star::sort_by_key(values, getter);
  } else {
    if constexpr (Stable)
      Star::stable_sort_by_key(values, getter);
    else
      Star::sort_by_key(values, getter);
    return {};
  }
}

template <bool Stable, bench_element T>
bool validate_sort(Star::List<T> const& actual, std::vector<T> const& input) {
  if (actual.size() != input.size())
    return false;
  auto expected = input;
  std::stable_sort(expected.begin(), expected.end(), [](T const& a, T const& b) {
    return identity_of(a) % 8 < identity_of(b) % 8;
  });
  if constexpr (Stable) {
    // Every owning payload and duplicate-key identity must retain its exact position.
    return std::equal(actual.begin(), actual.end(), expected.begin());
  } else {
    // Unstable ties have no prescribed identity order; validate the entire legal
    // key order and, independently, every original identity and owning payload.
    for (std::size_t i = 0; i < actual.size(); ++i)
      if (identity_of(actual[i]) % 8 != identity_of(expected[i]) % 8)
        return false;
    std::vector<T> actual_values(actual.begin(), actual.end());
    auto by_identity = [](T const& a, T const& b) { return identity_of(a) < identity_of(b); };
    std::sort(actual_values.begin(), actual_values.end(), by_identity);
    std::sort(expected.begin(), expected.end(), by_identity);
    return actual_values == expected;
  }
}

template <bool Stable, bool Checked, bench_element T>
void cached_key_sort(benchmark::State& state, order_pattern pattern) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_sort_input<T>(count, pattern);
  Star::List<T> source(input.begin(), input.end());
  {
    auto actual = source;
    auto sorted = sort_values<Stable, Checked>(actual);
    if (!sorted) {
      state.SkipWithError(sorted.error().what());
      return;
    }
    if (!validate_sort<Stable>(actual, input)) {
      state.SkipWithError("computed sort full values/key order/stable identities differ from independent expectation");
      return;
    }
  }
  state.SetLabel("source owning copy + cached-key sort + result/sort-work destruction timed; fixture/complete validation untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    auto result = source;
    auto sorted = sort_values<Stable, Checked>(result);
    if (!sorted) {
      state.SkipWithError(sorted.error().what());
      break;
    }
    benchmark::DoNotOptimize(result.data());
    benchmark::DoNotOptimize(result.size());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

std::vector<Star::KeyDownEvent> make_release_input(std::size_t count, release_pattern pattern) {
  constexpr std::array surviving_keys{Star::Key::B, Star::Key::C, Star::Key::D};
  constexpr std::array modifiers{Star::KeyMod::NoMod, Star::KeyMod::LShift,
                               Star::KeyMod::LCtrl, Star::KeyMod::LAlt};
  std::vector<Star::KeyDownEvent> input;
  input.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    bool released = pattern == release_pattern::alternating ? i % 2 == 0
                  : pattern == release_pattern::prefix ? i < count / 2 : false;
    input.push_back({released ? Star::Key::A : surviving_keys[i % surviving_keys.size()],
                     modifiers[(i / 3) % modifiers.size()]});
  }
  return input;
}

template <bool Member>
void release_key(Star::List<Star::KeyDownEvent>& events) {
  auto keep = [](Star::KeyDownEvent const& event) { return event.key != Star::Key::A; };
  if constexpr (Member)
    events.filter(keep);
  else
    Star::filter(events, keep);
}

template <bool Member>
void key_release(benchmark::State& state, release_pattern pattern) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_release_input(count, pattern);
  Star::List<Star::KeyDownEvent> source(input.begin(), input.end());
  std::vector<Star::KeyDownEvent> expected;
  for (auto const& event : input)
    if (event.key != Star::Key::A)
      expected.push_back(event);
  {
    auto actual = source;
    release_key<Member>(actual);
    if (actual.size() != expected.size()
        || !std::equal(actual.begin(), actual.end(), expected.begin(), [](auto const& a, auto const& b) {
             return a.key == b.key && a.mods == b.mods;
           })) {
      state.SkipWithError("key release survivor full values/order differ from independent expectation");
      return;
    }
  }
  state.SetLabel("held-key source copy + keep-predicate filter + result destruction timed; fixture/complete validation untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    auto result = source;
    release_key<Member>(result);
    benchmark::DoNotOptimize(result.data());
    benchmark::DoNotOptimize(result.size());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

template <bool Stable, bool Checked, bench_element T>
void register_sort(std::string_view element, std::string_view operation) {
  constexpr std::array patterns{
    std::pair{order_pattern::ascending, std::string_view("AscendingDuplicateKeys")},
    std::pair{order_pattern::descending, std::string_view("DescendingDuplicateKeys")},
    std::pair{order_pattern::shuffled, std::string_view("ShuffledDuplicateKeys")},
    std::pair{order_pattern::equal_keys, std::string_view("AllEqualKeys")}};
  for (auto const& [pattern, label] : patterns) {
    auto name = std::format("Algorithm/{}/{}/{}/SourceCopySortDestroy", element, operation, label);
    benchmark::RegisterBenchmark(name.c_str(), [pattern](benchmark::State& state) {
      cached_key_sort<Stable, Checked, T>(state, pattern);
    })->Arg(32)->Arg(1024);
  }
}

template <bench_element T>
void register_element(std::string_view element) {
  register_sort<false, false, T>(element, "SortByKey");
  register_sort<true, false, T>(element, "StableSortByKey");
  register_sort<false, true, T>(element, "CheckedSortByKey");
  register_sort<true, true, T>(element, "CheckedStableSortByKey");
}

template <bool Member>
void register_key_release(std::string_view operation) {
  constexpr std::array patterns{
    std::pair{release_pattern::alternating, std::string_view("AlternatingHalfReleased")},
    std::pair{release_pattern::prefix, std::string_view("PrefixHalfReleased")},
    std::pair{release_pattern::absent, std::string_view("NoMatchingKey")}};
  for (auto const& [pattern, label] : patterns) {
    auto name = std::format("Algorithm/KeyRelease/{}/{}/SourceCopyFilterDestroy", operation, label);
    benchmark::RegisterBenchmark(name.c_str(), [pattern](benchmark::State& state) {
      key_release<Member>(state, pattern);
    })->Arg(32)->Arg(1024);
  }
}

}// namespace

int main(int argc, char** argv) {
  return Star::run_benchmarks(argc, argv, [](int argc) {
    runtime_salt = static_cast<std::uint64_t>(argc + 73);
    benchmark::DoNotOptimize(runtime_salt);
    register_element<std::int64_t>("Int64");
    register_element<OwningRecord>("OwningStringRecord");
    register_key_release<false>("ImmediateFreeFilter");
    register_key_release<true>("LinearMemberFilter");
    return 0;
  });
}
