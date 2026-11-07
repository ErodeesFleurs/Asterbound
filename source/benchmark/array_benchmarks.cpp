#include <benchmark/benchmark.h>

import std;
import star.array;
import star.native_runtime;
import star.string;

namespace {

using Star::Array;
using Star::String;

// The same callable harness measures genuine old/current Array implementations.
// For a private historical consumer, substitute to_size -> toSize and
// copy_from -> copyFrom only. Do not copy the implementation into this file
// or add production API aliases.
// The PrivateCandidate cases return std::array, never a new Array interface.
struct operation_counts {
  std::uint64_t defaults = 0;
  std::uint64_t conversions = 0;
  std::uint64_t copy_constructions = 0;
  std::uint64_t move_constructions = 0;
  std::uint64_t copy_assignments = 0;
  std::uint64_t move_assignments = 0;
  std::uint64_t destructions = 0;
};

struct assignment_diagnostic {
  static inline operation_counts counts;

  assignment_diagnostic() { ++counts.defaults; }
  assignment_diagnostic(int v) : value(v) { ++counts.conversions; }
  assignment_diagnostic(assignment_diagnostic const& other) : value(other.value) { ++counts.copy_constructions; }
  assignment_diagnostic(assignment_diagnostic&& other) noexcept : value(other.value) {
    other.value = -1;
    ++counts.move_constructions;
  }
  assignment_diagnostic& operator=(assignment_diagnostic const& other) {
    value = other.value;
    ++counts.copy_assignments;
    return *this;
  }
  assignment_diagnostic& operator=(assignment_diagnostic&& other) noexcept {
    value = other.value;
    other.value = -1;
    ++counts.move_assignments;
    return *this;
  }
  assignment_diagnostic& operator=(int v) {
    value = v;
    ++counts.copy_assignments;
    return *this;
  }
  ~assignment_diagnostic() { ++counts.destructions; }
  bool operator==(assignment_diagnostic const& other) const { return value == other.value; }

  int value = 0;
};

enum class operation {
  default_construct,
  arguments,
  filled,
  copy_from,
  cross_construct,
  cross_assign,
  shrink,
  grow,
  private_direct_copy,
  temporary_arguments,
  private_direct_move
};

template <typename T>
using source_element =
  std::conditional_t<std::same_as<T, String>, std::string,
                     std::conditional_t<std::same_as<T, assignment_diagnostic>, int, std::int16_t>>;

template <typename T> T fixture_value(std::size_t index) {
  if constexpr (std::same_as<T, String> || std::same_as<T, std::string>) {
    auto text = std::format("/items/materials/fixture-{:03}/", index);
    text.append(80 + index % 7, static_cast<char>('a' + index % 26));
    text.push_back('\0');
    text += "/owned-payload";
    return T(std::move(text));
  } else {
    return T(static_cast<int>(index * 7 + 3));
  }
}

template <typename T, std::size_t N, std::size_t... I>
Array<T, N> argument_copy(Array<T, N> const& source, std::index_sequence<I...>) {
  return Array<T, N>(source[I]...);
}

// These candidates intentionally have different class-element semantics:
// direct initialization bypasses default construction followed by assignment.
// Keep them private measurements, not an API or a semantic-equivalence claim.
template <typename T, std::size_t N, std::size_t... I>
std::array<T, N> direct_copy(Array<T, N> const& source, std::index_sequence<I...>) {
  return std::array<T, N>{source[I]...};
}

template <typename T, std::size_t N, std::size_t... I>
std::array<T, N> direct_move(Array<T, N>& source, std::index_sequence<I...>) {
  return std::array<T, N>{std::move(source[I])...};
}

template <typename Result, typename Expected>
bool validate(benchmark::State& state, Result const& result, Expected const& expected) {
  if (result.size() != expected.size()) {
    state.SkipWithError("Array result size differs from independently prepared expectation");
    return false;
  }
  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (!(result[i] == expected[i])) {
      state.SkipWithError("Array result element differs from independently prepared expectation");
      return false;
    }
  }
  return true;
}

template <typename T, typename Factory, typename Expected>
void measure(benchmark::State& state, Factory&& factory, Expected const& expected) {
  // Both full-result checks and fixture setup are outside the State range.
  // The owning result's entire lifetime, including destruction, is timed.
  assignment_diagnostic::counts = {};
  {
    auto result = factory();
    if (!validate(state, result, expected))
      return;
  }
  auto counts = assignment_diagnostic::counts;
  if constexpr (std::same_as<T, assignment_diagnostic>) {
    state.SetLabel(std::format("diagnostic only; per call default={}, convert={}, copy-ctor={}, move-ctor={}, "
                               "copy-assign={}, move-assign={}, destroy={}",
                               counts.defaults, counts.conversions, counts.copy_constructions,
                               counts.move_constructions, counts.copy_assignments, counts.move_assignments,
                               counts.destructions));
  } else {
    state.SetLabel("full data checked untimed; construction + destruction timed");
  }
  for (auto _ : state) {
    (void)_;
    // Input fixtures escape through the callable; the barriers prevent hoisting
    // a constant result and retain element stores and owning destructors.
    benchmark::ClobberMemory();
    auto result = factory();
    benchmark::DoNotOptimize(result.data());
    benchmark::ClobberMemory();
  }
  {
    auto result = factory();
    if (!validate(state, result, expected))
      return;
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(expected.size()));
}

template <typename T, std::size_t N, operation Kind> void array_operation(benchmark::State& state) {
  Array<T, N> input;
  Array<source_element<T>, N> cross_input;
  for (std::size_t i = 0; i < N; ++i) {
    input[i] = fixture_value<T>(i);
    cross_input[i] = fixture_value<source_element<T>>(i);
  }
  constexpr auto result_size = Kind == operation::shrink ? N / 2 : Kind == operation::grow ? N + N / 2 : N;
  std::array<T, result_size> expected{};
  for (std::size_t i = 0; i < result_size; ++i) {
    if constexpr (Kind == operation::default_construct) {
      // The independently value-initialized standard array is the expectation.
    } else if constexpr (Kind == operation::filled) {
      expected[i] = fixture_value<T>(0);
    } else {
      if (i < N)
        expected[i] = fixture_value<T>(i);
    }
  }
  auto factory = [&] {
    if constexpr (Kind == operation::default_construct) {
      return Array<T, N>();
    } else if constexpr (Kind == operation::arguments) {
      return argument_copy(input, std::make_index_sequence<N>{});
    } else if constexpr (Kind == operation::filled) {
      return Array<T, N>::filled(input[0]);
    } else if constexpr (Kind == operation::copy_from) {
      return Array<T, N>::copy_from(input.begin(), N);
    } else if constexpr (Kind == operation::cross_construct) {
      return Array<T, N>(cross_input);
    } else if constexpr (Kind == operation::cross_assign) {
      Array<T, N> result;
      result = cross_input;
      return result;
    } else if constexpr (Kind == operation::shrink || Kind == operation::grow) {
      return input.template to_size<result_size>();
    } else if constexpr (Kind == operation::private_direct_copy) {
      return direct_copy(input, std::make_index_sequence<N>{});
    } else if constexpr (Kind == operation::temporary_arguments) {
      auto temporary = input;
      return argument_copy(temporary, std::make_index_sequence<N>{});
    } else if constexpr (Kind == operation::private_direct_move) {
      // Both move-candidate and TemporaryArguments pay the same source copy
      // and temporary destruction; there is no untimed replenishment shortcut.
      auto temporary = input;
      return direct_move(temporary, std::make_index_sequence<N>{});
    }
  };
  measure<T>(state, factory, expected);
}

template <typename T, std::size_t N, operation Kind> void register_case(char const* type, char const* name) {
  auto full_name = std::format("Array/{}/{}/{}", type, name, N);
  benchmark::RegisterBenchmark(full_name.c_str(), &array_operation<T, N, Kind>);
}

template <typename T, std::size_t N> void register_family(char const* type) {
  register_case<T, N, operation::default_construct>(type, "DefaultConstructDestroy");
  register_case<T, N, operation::arguments>(type, "ArgumentsConstructDestroy");
  register_case<T, N, operation::filled>(type, "FilledConstructDestroy");
  register_case<T, N, operation::copy_from>(type, "CopyFromConstructDestroy");
  register_case<T, N, operation::cross_construct>(type, "CrossTypeConstructDestroy");
  register_case<T, N, operation::cross_assign>(type, "CrossTypeAssignConstructDestroy");
  register_case<T, N, operation::shrink>(type, "ToSizeShrinkConstructDestroy");
  register_case<T, N, operation::grow>(type, "ToSizeGrowConstructDestroy");
  register_case<T, N, operation::private_direct_copy>(type, "PrivateCandidate/DirectCopyConstructDestroy");
  register_case<T, N, operation::temporary_arguments>(type, "TemporaryArgumentsConstructDestroy");
  register_case<T, N, operation::private_direct_move>(type, "PrivateCandidate/TemporaryDirectMoveConstructDestroy");
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
    std::cerr << native_runtime.error().what() << '\n';
    return 1;
  }
  register_family<std::int64_t, 2>("Scalar");
  register_family<std::int64_t, 4>("Scalar");
  register_family<std::int64_t, 16>("Scalar");
  register_family<std::int64_t, 64>("Scalar");
  register_family<String, 4>("OwningString");
  register_family<String, 16>("OwningString");
  register_family<assignment_diagnostic, 4>("AssignmentDiagnostic");
  register_family<assignment_diagnostic, 16>("AssignmentDiagnostic");
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
