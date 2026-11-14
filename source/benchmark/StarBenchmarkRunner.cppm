module;

#include <benchmark/benchmark.h>
#include <cstdio>

import std;
import star.format;
import star.native_runtime;

namespace Star {

struct BenchmarkRunOptions {
  bool require_matches = true;
  bool check_benchmark_errors = true;
  bool report_runtime_error = true;
};

// Preparation is invoked as an lvalue under NativeRuntime, before Benchmark
// consumes argc/argv. No type erasure, callback pointer, or allocation is needed.
template <typename Preparation>
concept benchmark_preparation = requires(Preparation& prepare, int argc) {
  { std::invoke(prepare, argc) } -> std::same_as<int>;
};

// Root/UI hosts retain their own setup and teardown ordering. This guard only
// owns the Google Benchmark process lifetime, never Root or workload state.
class BenchmarkLifetime {
public:
  BenchmarkLifetime(int& argc, char** argv) { benchmark::Initialize(&argc, argv); }
  BenchmarkLifetime(BenchmarkLifetime const&) = delete;
  BenchmarkLifetime& operator=(BenchmarkLifetime const&) = delete;
  ~BenchmarkLifetime() {
    benchmark::ClearRegisteredBenchmarks();
    benchmark::Shutdown();
  }
};

namespace {
class CheckedBenchmarkReporter final : public benchmark::ConsoleReporter {
public:
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& run : reports)
      failed |= run.skipped == benchmark::internal::SkippedWithError;
    benchmark::ConsoleReporter::ReportRuns(reports);
  }

  bool failed = false;
};
}// namespace

// Deliberately does not clear registrations: Root/UI hosts must release their
// borrowed callbacks before destroying the corresponding session and Root.
int run_registered_benchmarks(bool require_matches = true, bool check_errors = true) {
  if (!check_errors) {
    auto matched = benchmark::RunSpecifiedBenchmarks();
    return require_matches && matched == 0 ? 1 : 0;
  }
  CheckedBenchmarkReporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  return reporter.failed || (require_matches && matched == 0) ? 1 : 0;
}

int run_benchmark_command_line(int argc, char** argv, BenchmarkRunOptions options) {
  // Core hosts historically retain registrations until process teardown.
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    benchmark::Shutdown();
    return 1;
  }
  auto result = run_registered_benchmarks(options.require_matches, options.check_benchmark_errors);
  benchmark::Shutdown();
  return result;
}

template <benchmark_preparation Preparation>
int run_benchmarks(int argc, char** argv, Preparation&& prepare, BenchmarkRunOptions options = {}) {
  auto runtime = NativeRuntime::create();
  if (!runtime) {
    if (options.report_runtime_error)
      (void)Star::print(stderr, "{}\n", runtime.error().what());
    return 1;
  }
  if (auto result = std::invoke(prepare, argc); result != 0)
    return result;
  return run_benchmark_command_line(argc, argv, options);
}

int run_benchmarks(int argc, char** argv, BenchmarkRunOptions options = {}) {
  return run_benchmarks(argc, argv, [](int) { return 0; }, options);
}

}// namespace Star

export module star.benchmark_runner;

export namespace Star {
using ::Star::BenchmarkLifetime;
using ::Star::benchmark_preparation;
using ::Star::BenchmarkRunOptions;
using ::Star::run_benchmarks;
using ::Star::run_registered_benchmarks;
}// namespace Star
