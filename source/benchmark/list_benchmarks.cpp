#include <benchmark/benchmark.h>

import std;
import star.list;
import star.native_runtime;
import star.string;

namespace {

std::uint64_t runtime_salt;

template <typename T>
concept bench_element = std::same_as<T, std::int64_t> || std::same_as<T, Star::String>;

enum class operation { append, reserved_append, chunks, move_append, slice, stride_slice, filtered, sorted };

template <bench_element T> T make_element(std::size_t index) {
  if constexpr (std::same_as<T, std::int64_t>)
    return static_cast<std::int64_t>(index + runtime_salt);
  else {
    auto bytes = std::string(80, 'x') + std::format("/asset/{:06}", index + runtime_salt);
    bytes[3] = '\0';
    bytes[8] = '\xc3';
    bytes[9] = '\xa9';
    return Star::String(std::string_view(bytes));
  }
}

template <bench_element T> bool keep_element(T const& value) {
  if constexpr (std::same_as<T, std::int64_t>)
    return value % 2 == 0;
  else
    return value.utf8().back() % 2 == 0;
}

template <operation Operation, bench_element T>
Star::List<T> materialize(Star::List<T> const& source, std::vector<T> const& input) {
  if constexpr (Operation == operation::slice)
    return source.slice();
  else if constexpr (Operation == operation::stride_slice)
    return source.slice(0, {}, 3);
  else if constexpr (Operation == operation::filtered)
    return source.filtered([](T const& value) { return keep_element(value); });
  else if constexpr (Operation == operation::sorted)
    return source.sorted();
  else {
    Star::List<T> result;
    if constexpr (Operation == operation::reserved_append)
      result.reserve(input.size());
    if constexpr (Operation == operation::chunks) {
      std::span<T const> view(input);
      for (std::size_t offset = 0; offset < view.size(); offset += 16)
        result.append_range(view.subspan(offset, std::min<std::size_t>(16, view.size() - offset)));
    } else if constexpr (Operation == operation::move_append) {
      auto owned_input = input;
      result.append_range(std::move(owned_input));
    } else
      result.append_range(input);
    return result;
  }
}

template <operation Operation, bench_element T> void list_materialize(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  std::vector<T> input;
  input.reserve(count);
  for (std::size_t i = 0; i < count; ++i)
    input.push_back(make_element<T>(i));
  Star::List<T> source(input.begin(), input.end());
  std::vector<T> expected;
  for (std::size_t i = 0; i < input.size(); ++i) {
    if constexpr (Operation == operation::stride_slice) {
      if (i % 3 != 0)
        continue;
    } else if constexpr (Operation == operation::filtered) {
      if (!keep_element(input[i]))
        continue;
    }
    expected.push_back(input[i]);
  }
  if constexpr (Operation == operation::sorted)
    std::sort(expected.begin(), expected.end());
  auto actual = materialize<Operation>(source, input);
  if (actual.size() != expected.size() || !std::equal(actual.begin(), actual.end(), expected.begin())) {
    state.SkipWithError("materialized values/order differ from independent expected values");
    return;
  }
  state.SetLabel(Operation == operation::move_append
                   ? "source copy + move append + full construction/destruction timed; fixture/validation untimed"
                   : "result construction/materialization/destruction timed; fixture/validation untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    auto result = materialize<Operation>(source, input);
    benchmark::DoNotOptimize(result.data());
    benchmark::DoNotOptimize(result.size());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(expected.size()));
}

template <bench_element T> void register_element(char const* element) {
  auto add = [element](char const* operation_name, void (*function)(benchmark::State&)) {
    auto name = std::format("List/{}/{}", element, operation_name);
    benchmark::RegisterBenchmark(name.c_str(), function)->Arg(64)->Arg(1024);
  };
  add("AppendCopyConstructDestroy", &list_materialize<operation::append, T>);
  add("ReservedAppendCopyConstructDestroy", &list_materialize<operation::reserved_append, T>);
  add("AppendChunks16ConstructDestroy", &list_materialize<operation::chunks, T>);
  add("SourceCopyMoveAppendConstructDestroy", &list_materialize<operation::move_append, T>);
  add("SliceCopyConstructDestroy", &list_materialize<operation::slice, T>);
  add("Stride3SliceCopyConstructDestroy", &list_materialize<operation::stride_slice, T>);
  add("FilteredCopyConstructDestroy", &list_materialize<operation::filtered, T>);
  add("SortedCopyConstructDestroy", &list_materialize<operation::sorted, T>);
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
  register_element<std::int64_t>("Int64");
  register_element<Star::String>("OwningString");
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
