#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.benchmark_runner;
import star.worker_pool;
import star.string;
import star.format;
import star.unique_function;
import star.thread;
import star.lru_cache;

namespace {

template <std::size_t capture_words, bool producer, bool single_worker>
bool run_batch(benchmark::State& state, Star::WorkerPool& pool,
    std::array<std::uint64_t, capture_words> const& payload,
    std::vector<std::uint64_t>& outputs,
    std::vector<std::conditional_t<producer, Star::WorkerPoolPromise<std::uint64_t>, Star::WorkerPoolHandle>>& completions,
    std::uint64_t expected) {
  for (std::size_t i = 0; i < outputs.size(); ++i) {
    // Pass concrete lambdas directly: explicitly erasing here would conceal the
    // submission API's allocation difference on the old and new implementations.
    if constexpr (producer) {
      completions.push_back(pool.add_producer<std::uint64_t>([payload]() -> std::expected<std::uint64_t, Star::StarException> {
        benchmark::DoNotOptimize(payload);
        return std::accumulate(payload.begin(), payload.end(), std::uint64_t{0});
      }));
    } else {
      completions.push_back(pool.add_work([payload, &output = outputs[i]]() -> std::expected<void, Star::StarException> {
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
  if constexpr (!single_worker) {
    if (!finish())
      return false;
  }
  for (std::size_t i = 0; i < completions.size(); ++i) {
    if constexpr (producer) {
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
  if constexpr (single_worker)
    return finish();
  return true;
}

template <std::size_t capture_words, bool producer, bool single_worker>
void worker_submission(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  std::array<std::uint64_t, capture_words> payload;
  std::iota(payload.begin(), payload.end(), std::uint64_t{1});
  auto expected = std::accumulate(payload.begin(), payload.end(), std::uint64_t{0});
  std::vector<std::uint64_t> outputs(count);
  std::vector<std::conditional_t<producer, Star::WorkerPoolPromise<std::uint64_t>, Star::WorkerPoolHandle>> completions;
  completions.reserve(count);

  // Semantic preflight, fixture storage and native thread initialization are
  // outside the measured interval. Use exactly the measured submission path.
  auto make_pool = [&]() -> std::unique_ptr<Star::WorkerPool> {
    auto pool = std::make_unique<Star::WorkerPool>("worker-benchmark");
    if constexpr (single_worker) {
      auto started = pool->start(1);
      if (!started) {
        state.SkipWithError(started.error().what());
        return nullptr;
      }
    }
    return pool;
  };
  auto preflight = make_pool();
  if (!preflight || !run_batch<capture_words, producer, single_worker>(state, *preflight, payload, outputs, completions, expected))
    return;
  preflight.reset();
  completions.clear();
  for (auto _ : state) {
    state.PauseTiming();
    completions.clear();
    std::fill(outputs.begin(), outputs.end(), 0);
    auto pool = make_pool();
    state.ResumeTiming();
    if (!pool)
      break;
    bool success = run_batch<capture_words, producer, single_worker>(state, *pool, payload, outputs, completions, expected);
    state.PauseTiming();
    pool.reset();
    state.ResumeTiming();
    if (!success)
      break;
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

BENCHMARK_TEMPLATE(worker_submission, 1, false, false)
    ->Name("BM_WorkerSubmission<1, false, false>")->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(worker_submission, 128, false, false)
    ->Name("BM_WorkerSubmission<128, false, false>")->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(worker_submission, 1, true, false)
    ->Name("BM_WorkerSubmission<1, true, false>")->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(worker_submission, 128, true, false)
    ->Name("BM_WorkerSubmission<128, true, false>")->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(worker_submission, 1, false, true)
    ->Name("BM_WorkerSubmission<1, false, true>")->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(worker_submission, 128, false, true)
    ->Name("BM_WorkerSubmission<128, false, true>")->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(worker_submission, 1, true, true)
    ->Name("BM_WorkerSubmission<1, true, true>")->Arg(256)->UseRealTime();
BENCHMARK_TEMPLATE(worker_submission, 128, true, true)
    ->Name("BM_WorkerSubmission<128, true, true>")->Arg(256)->UseRealTime();

void worker_active_finish(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  std::vector<std::size_t> outputs(count);
  for (auto _ : state) {
    state.PauseTiming();
    std::fill(outputs.begin(), outputs.end(), 0);
    {
      std::latch running(1), draining(1);
      Star::WorkerPool pool("active-finish-benchmark");
      auto started = pool.start(1);
      if (!started) {
        state.SkipWithError(started.error().what());
        return;
      }
      // The worker owns the first job; only the finishing caller can begin
      // the second. After rendezvous they compete for the rest of the queue.
      pool.add_work([&]() -> std::expected<void, Star::StarException> {
        running.count_down();
        draining.wait();
        outputs[0] = 1;
        return {};
      });
      running.wait();
      pool.add_work([&]() -> std::expected<void, Star::StarException> {
        outputs[1] = 2;
        draining.count_down();
        return {};
      });
      for (std::size_t i = 2; i < count; ++i) {
        pool.add_work([&, i]() -> std::expected<void, Star::StarException> {
          outputs[i] = i + 1;
          return {};
        });
      }
      state.ResumeTiming();
      auto finished = pool.finish();
      state.PauseTiming();
      if (!finished) {
        state.SkipWithError(finished.error().what());
        return;
      }
      if (!std::ranges::equal(outputs, std::views::iota(std::size_t{1}, count + 1))) {
        state.SkipWithError("Active finish did not publish all job outputs");
        return;
      }
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

BENCHMARK(worker_active_finish)->Arg(256)->UseRealTime();

enum class PollState { pending, success, failure };

template <bool producer, PollState poll_state>
void worker_poll(benchmark::State& state) {
  // No worker can complete a pending job. For terminal states finish it once,
  // outside the timer, then measure repeated observations of the same job.
  Star::WorkerPool pool("poll-benchmark");
  using Result = std::conditional_t<producer, int, void>;
  auto task = []() -> std::expected<Result, Star::StarException> {
    if constexpr (poll_state == PollState::failure)
      return std::unexpected(Star::StarException("poll benchmark failure"));
    else
      return {};
  };
  auto completion = [&] {
    if constexpr (producer)
      return pool.add_producer<int>(task);
    else
      return pool.add_work(task);
  }();
  if constexpr (poll_state != PollState::pending) {
    auto finished = pool.finish();
    if (!finished) {
      state.SkipWithError(finished.error().what());
      return;
    }
  }
  auto check = completion.poll();
  if constexpr (poll_state == PollState::failure) {
    if (check || !std::string_view(check.error().what()).contains("poll benchmark failure")) {
      state.SkipWithError("Poll did not return the job's failure");
      return;
    }
  } else if (!check || *check != (poll_state == PollState::success)) {
    state.SkipWithError("Poll did not return the expected readiness");
    return;
  }
  for (auto _ : state) {
    auto result = completion.poll();
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK_TEMPLATE(worker_poll, false, PollState::pending)->Name("worker_poll/handle/pending");
BENCHMARK_TEMPLATE(worker_poll, false, PollState::success)->Name("worker_poll/handle/success");
BENCHMARK_TEMPLATE(worker_poll, false, PollState::failure)->Name("worker_poll/handle/failure");
BENCHMARK_TEMPLATE(worker_poll, true, PollState::pending)->Name("worker_poll/promise/pending");
BENCHMARK_TEMPLATE(worker_poll, true, PollState::success)->Name("worker_poll/promise/success");
BENCHMARK_TEMPLATE(worker_poll, true, PollState::failure)->Name("worker_poll/promise/failure");

using TimedFunction = Star::UniqueFunction<std::uint64_t(std::uint64_t)>;

struct InlineAdder {
  std::uint64_t increment;
  std::uint64_t operator()(std::uint64_t value) const noexcept { return value + increment; }
};

struct alignas(64) HeapAdder : InlineAdder {};

template <typename Target>
  requires std::constructible_from<TimedFunction, Target>
bool check_transfer(benchmark::State& state, Target target) {
  TimedFunction source(std::move(target));
  TimedFunction destination(std::move(source));
  auto result = destination(41);
  if (source || !result || *result != 42) {
    state.SkipWithError("Callable move did not transfer its target");
    return false;
  }
  return true;
}

void inline_construct_move(benchmark::State& state) {
  if (!check_transfer(state, InlineAdder{1}))
    return;
  for (auto _ : state) {
    TimedFunction source(InlineAdder{1});
    benchmark::DoNotOptimize(source);
    TimedFunction destination(std::move(source));
    benchmark::DoNotOptimize(destination);
  }
}

void heap_transfer(benchmark::State& state) {
  if (!check_transfer(state, HeapAdder{{1}}))
    return;
  TimedFunction source(HeapAdder{{1}});
  // Allocate once: each iteration measures two transfers, not allocation.
  for (auto _ : state) {
    benchmark::DoNotOptimize(source);
    TimedFunction destination(std::move(source));
    benchmark::DoNotOptimize(destination);
    source = std::move(destination);
  }
  auto result = source(41);
  if (!result || *result != 42)
    state.SkipWithError("Heap target lost during round trip");
  state.SetItemsProcessed(2 * state.iterations());
}

void hot_invocation(benchmark::State& state) {
  TimedFunction callable(InlineAdder{1});
  auto check = callable(41);
  if (!check || *check != 42) {
    state.SkipWithError("Incorrect callable result");
    return;
  }
  benchmark::DoNotOptimize(callable);
  for (auto _ : state) {
    auto result = callable(41);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(inline_construct_move);
BENCHMARK(heap_transfer);
BENCHMARK(hot_invocation);

class CountingThread final : public Star::Thread {
public:
  explicit CountingThread(bool blocked = false) : Thread("thread-bench"), release(blocked ? 1 : 0) {}

  std::latch release;
  std::int64_t runs = 0;

private:
  std::expected<void, Star::StarException> run() override {
    release.wait();
    ++runs;
    return {};
  }
};

void thread_start_join(benchmark::State& state) {
  CountingThread thread;
  for (auto _ : state) {
    auto started = thread.start();
    if (!started || !*started) {
      state.SkipWithError(started ? "Thread did not restart" : started.error().what());
      return;
    }
    auto joined = thread.join();
    if (!joined || !*joined) {
      state.SkipWithError(joined ? "Thread was not joined" : joined.error().what());
      return;
    }
  }
  if (thread.runs != state.iterations() || !thread.is_joined() || thread.is_running())
    state.SkipWithError("Joined thread did not publish exactly one run per start");
}

void thread_query(benchmark::State& state) {
  CountingThread thread(true);
  auto started = thread.start();
  if (!started || !*started) {
    state.SkipWithError(started ? "Thread did not start" : started.error().what());
    return;
  }
  // Hold the worker in run(): even the baseline's non-atomic joined flag is
  // stable throughout this measurement. Never race a baseline join or restart.
  if (!thread.is_running() || thread.is_joined()) {
    state.SkipWithError("Gated thread did not report running and unjoined");
  } else {
    for (auto _ : state) {
      benchmark::ClobberMemory();
      auto joined = thread.is_joined();
      auto running = thread.is_running();
      benchmark::DoNotOptimize(joined);
      benchmark::DoNotOptimize(running);
    }
  }
  thread.release.count_down();
  auto joined = thread.join();
  if (!joined || !*joined || thread.runs != 1 || !thread.is_joined() || thread.is_running())
    state.SkipWithError("Gated thread did not join cleanly");
  state.SetItemsProcessed(2 * state.iterations());
}

void thread_rw_contention(benchmark::State& state) {
  static Star::ReadersWriterMutex mutex;
  static std::uint64_t version, inverse;
  std::uint64_t mismatch = 0;
  // Google Benchmark synchronizes all threads at both loop boundaries;
  // setup and checking are not part of the measured lock operations.
  if (state.thread_index() == 0) {
    version = 0;
    inverse = ~version;
    for (auto _ : state) {
      std::unique_lock guard(mutex);
      inverse = ~++version;
    }
  } else {
    for (auto _ : state) {
      std::shared_lock guard(mutex);
      mismatch |= version ^ ~inverse;
    }
  }
  if (version != static_cast<std::uint64_t>(state.iterations()) || mismatch != 0)
    state.SkipWithError("RW lock did not publish coherent writes");
  state.SetItemsProcessed(state.iterations());
}

struct IndexCacheFixture {
  static constexpr unsigned key_count = 16;
  Star::SpinLock lock;
  Star::LruCache<unsigned, std::shared_ptr<unsigned const>> cache{key_count};

  IndexCacheFixture() {
    for (unsigned key = 0; key < key_count; ++key)
      cache.insert_or_assign(key, std::make_shared<unsigned const>(key + 1));
  }

  // Match BTreeImpl::load_index's hot hit: touch MRU and copy the shared
  // index under the spin lock, then consume and release it outside the lock.
  std::shared_ptr<unsigned const> load(unsigned key) {
    std::lock_guard guard(lock);
    auto hit = cache.find_ref(key);
    return hit ? hit->get() : nullptr;
  }
};

void thread_spin_index_cache(benchmark::State& state) {
  static IndexCacheFixture fixture;
  constexpr auto count = IndexCacheFixture::key_count;
  auto offset = static_cast<unsigned>(state.thread_index());
  std::uint64_t sum = 0;
  for (auto _ : state) {
    for (unsigned i = 0; i < count; ++i) {
      auto value = fixture.load((i + offset) % count);
      if (value)
        sum += *value;
    }
  }
  if (sum != static_cast<std::uint64_t>(state.iterations()) * (count * (count + 1) / 2))
    state.SkipWithError("Incorrect BTree hot-index checksum");
  state.SetItemsProcessed(state.iterations() * count);
}

constexpr int sleep_samples = 101;

void thread_sleep_precise(benchmark::State& state) {
  using namespace std::chrono;
  auto requested = milliseconds(state.range(0));
  std::array<double, sleep_samples> overshoot_us;
  std::size_t samples = 0;
  for (auto _ : state) {
    auto begin = steady_clock::now();
    Star::Thread::sleep_precise(static_cast<unsigned>(requested.count()));
    overshoot_us[samples++] = duration<double, std::micro>(steady_clock::now() - begin - requested).count();
  }
  // The json cpu_time field is caller-thread CPU per sleep (unit: us).
  // Preserve negative errors: the old millisecond-rounded deadline can wake early.
  auto measured = std::span(overshoot_us).first(samples);
  std::ranges::sort(measured);
  auto percentile = [&](std::size_t p) { return measured[(p * samples + 99) / 100 - 1]; };
  state.counters["overshoot_p50_us"] = percentile(50);
  state.counters["overshoot_p95_us"] = percentile(95);
  state.counters["early_samples"] = std::ranges::lower_bound(measured, 0.0) - measured.begin();
  state.counters["samples"] = samples;
  if (percentile(50) < -duration<double, std::micro>(requested).count() / 2)
    state.SkipWithError("Median sleep did not reach half the requested duration");
}

BENCHMARK(thread_start_join)->UseRealTime();
BENCHMARK(thread_query);
BENCHMARK(thread_rw_contention)->Threads(4)->UseRealTime();
BENCHMARK(thread_spin_index_cache)->Threads(1)->Threads(4)->UseRealTime();
BENCHMARK(thread_sleep_precise)->Arg(1)->Arg(16)->Iterations(sleep_samples)->UseRealTime()->Unit(benchmark::kMicrosecond);

}

int main(int argc, char** argv) {
  return Star::run_benchmarks(argc, argv);
}
