module;

#include <benchmark/benchmark.h>

import std;
import star.format;
import star.root;

namespace Star {

struct PerformanceBenchmarkSession {
  Root& root;
  bool failed = false;

  void fail(benchmark::State& state, std::string_view message) {
    failed = true;
    state.SkipWithError(std::string(message).c_str());
  }

  void fail(benchmark::State& state, StarException const& error) { fail(state, std::string_view(error.what())); }
};

[[nodiscard]] std::expected<void, StarException>
registerCollisionPerformanceBenchmarks(PerformanceBenchmarkSession& session);
[[nodiscard]] std::expected<void, StarException>
registerLightingPerformanceBenchmarks(PerformanceBenchmarkSession& session);
[[nodiscard]] std::expected<void, StarException>
registerNetworkPerformanceBenchmarks(PerformanceBenchmarkSession& session);
[[nodiscard]] std::expected<void, StarException>
register_liquid_performance_benchmarks(PerformanceBenchmarkSession& session);
[[nodiscard]] std::expected<void, StarException>
registerAssetsPerformanceBenchmarks(PerformanceBenchmarkSession& session);

}// namespace Star

export module star.performance_benchmarks;

export namespace Star {
using ::Star::PerformanceBenchmarkSession;
using ::Star::registerAssetsPerformanceBenchmarks;
using ::Star::registerCollisionPerformanceBenchmarks;
using ::Star::registerLightingPerformanceBenchmarks;
using ::Star::register_liquid_performance_benchmarks;
using ::Star::registerNetworkPerformanceBenchmarks;
}// namespace Star
