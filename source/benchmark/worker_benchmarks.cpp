#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.native_runtime;
import star.worker_pool;
import star.string;
import star.format;

namespace {

template <std::size_t CaptureWords, bool Producer, bool SingleWorker>
bool runBatch(benchmark::State& state, Star::WorkerPool& pool,
    std::array<std::uint64_t, CaptureWords> const& payload,
    std::vector<std::uint64_t>& outputs,
    std::vector<std::conditional_t<Producer, Star::WorkerPoolPromise<std::uint64_t>, Star::WorkerPoolHandle>>& completions,
    std::uint64_t expected) {
  for (std::size_t i = 0; i < outputs.size(); ++i) {
    // Pass concrete lambdas directly: explicitly erasing here would conceal the
    // submission API's allocation difference on the old and new implementations.
    if constexpr (Producer) {
      completions.push_back(pool.addProducer<std::uint64_t>([payload]() -> std::expected<std::uint64_t, Star::StarException> {
        benchmark::DoNotOptimize(payload);
        return std::accumulate(payload.begin(), payload.end(), std::uint64_t{0});
      }));
    } else {
      completions.push_back(pool.addWork([payload, &output = outputs[i]]() -> std::expected<void, Star::StarException> {
        benchmark::DoNotOptimize(payload);
        output = std::accumulate(payload.begin(), payload.end(), std::uint64_t{0});
        return {};
      }));
    }
  }
  // Queue-only pools need finish() to execute their jobs. A threaded batch
  // waits on each completion first, so finish() cannot steal the worker's jobs.
  auto finish = [&] {
    auto result = pool.finish();
    if (!result) {
      state.SkipWithError(result.error().what());
      return false;
    }
    return true;
  };
  if constexpr (!SingleWorker) {
    if (!finish())
      return false;
  }
  for (std::size_t i = 0; i < completions.size(); ++i) {
    if constexpr (Producer) {
      auto result = completions[i].get();
      if (!result) {
        state.SkipWithError(result.error().what());
        return false;
      }
      outputs[i] = result->get();
    } else {
      auto result = completions[i].finish();
      if (!result) {
        state.SkipWithError(result.error().what());
        return false;
      }
    }
    if (outputs[i] != expected) {
      state.SkipWithError("Worker callable returned an incorrect payload checksum");
      return false;
    }
    benchmark::DoNotOptimize(outputs[i]);
  }
  if constexpr (SingleWorker)
    return finish();
  return true;
}

template <std::size_t CaptureWords, bool Producer, bool SingleWorker>
void BM_WorkerSubmission(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  std::array<std::uint64_t, CaptureWords> payload;
  std::iota(payload.begin(), payload.end(), std::uint64_t{1});
  auto expected = std::accumulate(payload.begin(), payload.end(), std::uint64_t{0});
  std::vector<std::uint64_t> outputs(count);
  std::vector<std::conditional_t<Producer, Star::WorkerPoolPromise<std::uint64_t>, Star::WorkerPoolHandle>> completions;
  completions.reserve(count);

  // Semantic preflight, fixture storage and native thread initialization are
  // outside the measured interval. Use exactly the measured submission path.
  auto makePool = [&]() -> std::unique_ptr<Star::WorkerPool> {
    auto pool = std::make_unique<Star::WorkerPool>("worker-benchmark");
    if constexpr (SingleWorker) {
      auto started = pool->start(1);
      if (!started) {
        state.SkipWithError(started.error().what());
        return nullptr;
      }
    }
    return pool;
  };
  auto preflight = makePool();
  if (!preflight || !runBatch<CaptureWords, Producer, SingleWorker>(state, *preflight, payload, outputs, completions, expected))
    return;
  preflight.reset();
  completions.clear();
  for (auto _ : state) {
    state.PauseTiming();
    completions.clear();
    std::fill(outputs.begin(), outputs.end(), 0);
    auto pool = makePool();
    state.ResumeTiming();
    if (!pool)
      break;
    bool success = runBatch<CaptureWords, Producer, SingleWorker>(state, *pool, payload, outputs, completions, expected);
    state.PauseTiming();
    pool.reset();
    state.ResumeTiming();
    if (!success)
      break;
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

BENCHMARK_TEMPLATE(BM_WorkerSubmission, 1, false, false)->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(BM_WorkerSubmission, 128, false, false)->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(BM_WorkerSubmission, 1, true, false)->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(BM_WorkerSubmission, 128, true, false)->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(BM_WorkerSubmission, 1, false, true)->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(BM_WorkerSubmission, 128, false, true)->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(BM_WorkerSubmission, 1, true, true)->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(BM_WorkerSubmission, 128, true, true)->Arg(256)->UseRealTime();

class CheckedReporter : public benchmark::ConsoleReporter {
public:
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& run : reports)
      failed |= run.skipped == benchmark::internal::SkippedWithError;
    benchmark::ConsoleReporter::ReportRuns(reports);
  }
  bool failed = false;
};

}

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
