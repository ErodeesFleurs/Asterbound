import std;
import star.algorithm;
import star.list;
import star.worker_pool;
import star.unique_function;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

namespace {
struct ReleaseOnExit {
  std::atomic<bool>& released;
  void open() {
    released.store(true);
    released.notify_all();
  }
  ~ReleaseOnExit() { open(); }
};
}

TEST(WorkerPoolTest, All) {
  int counter = 0;
  std::mutex counter_mutex;
  std::atomic<bool> started{false};
  std::atomic<bool> released{false};

  auto inc_counter = [&]() -> std::expected<void, StarException> {
    started.store(true);
    started.notify_all();
    released.wait(false);
    std::lock_guard locker(counter_mutex);
    counter += 1;
    return {};
  };

  Deque<WorkerPoolHandle> handles;

  WorkerPool worker_pool("WorkerPoolTest");
  // Keep the gate alive through pool teardown, but release it before any join
  // even when a fatal assertion exits the test early.
  ReleaseOnExit release_on_exit{released};

  for (std::size_t i = 0; i < 10; ++i)
    handles.push_back(worker_pool.add_work(inc_counter));

  ASSERT_TRUE(worker_pool.start(10));
  started.wait(false);
  EXPECT_EQ(counter, 0);

  for (std::size_t i = 0; i < 90; ++i)
    handles.push_back(worker_pool.add_work(inc_counter));

  release_on_exit.open();

  while (handles.size() > 20) {
    auto handle = handles.take_front();
    ASSERT_TRUE(handle);
    ASSERT_TRUE(handle->finish());
  }

  ASSERT_TRUE(worker_pool.finish());

  EXPECT_EQ(counter, 100);
}

TEST(WorkerPoolTest, MoveOnlyTasksPublishResultsToAllWaiters) {
  struct Payload {
    explicit Payload(int value) : value(value) {}
    Payload(Payload&&) = default;
    int value;
  };
  std::atomic<bool> release{false};
  WorkerPool pool("move-only");
  ASSERT_TRUE(pool.start(2));
  ReleaseOnExit release_on_exit{release};
  auto result = pool.add_producer<Payload>(
      [value = Payload(42), &release]() mutable -> std::expected<Payload, StarException> {
        release.wait(false);
        return std::move(value);
      });
  EXPECT_EQ(result.poll(), false);
  EXPECT_EQ(result.wait(0), false);
  EXPECT_EQ(result.wait(1), false);
  std::array<int, 4> observations{};
  std::vector<std::thread> waiters;
  std::latch observing(std::ssize(observations));
  for (std::size_t i = 0; i < observations.size(); ++i) {
    waiters.emplace_back([&, i, observer = result] {
      observing.count_down();
      if (i % 2 == 0) {
        ASSERT_EQ(observer.wait(5000), true);
      }
      auto value = observer.get();
      ASSERT_TRUE(value);
      observations[i] = value->get().value;
    });
  }
  observing.wait();
  release_on_exit.open();
  for (auto& waiter : waiters)
    waiter.join();
  for (auto value : observations)
    EXPECT_EQ(value, 42);
  EXPECT_TRUE(result.done());
  EXPECT_EQ(result.poll(), true);
  auto value = result.get();
  ASSERT_TRUE(value);
  EXPECT_EQ(value->get().value, 42);
  auto empty = pool.add_work(UniqueFunction<std::expected<void, StarException>()>{});
  auto empty_finish = empty.finish();
  ASSERT_FALSE(empty_finish);
  auto repeated_finish = empty.finish();
  ASSERT_FALSE(repeated_finish);
  EXPECT_STREQ(repeated_finish.error().what(), empty_finish.error().what());
  auto empty_producer = pool.add_producer<int>(UniqueFunction<std::expected<int, StarException>()>{});
  auto empty_value = empty_producer.get();
  ASSERT_FALSE(empty_value);
  auto recovery = pool.add_producer<int>([]() -> std::expected<int, StarException> { return 19; });
  auto recovered_value = recovery.get();
  ASSERT_TRUE(recovered_value);
  EXPECT_EQ(recovered_value->get(), 19);
  ASSERT_TRUE(pool.finish());
}

namespace {
struct StoredLvalueTask {
  std::unique_ptr<int> value;
  std::reference_wrapper<int> observed;
  explicit operator bool() const { return false; }
  std::expected<void, StarException> operator()() & {
    observed.get() = *value;
    return {};
  }
  std::expected<void, StarException> operator()() && = delete;
};

struct StoredLvalueProducer {
  std::unique_ptr<int> value;
  std::expected<int, StarException> operator()() & { return *value; }
  std::expected<int, StarException> operator()() && = delete;
};

template <typename Callable>
concept accepts_work = requires(WorkerPool& pool, Callable&& callable) {
  pool.add_work(std::forward<Callable>(callable));
};

template <typename Callable>
concept accepts_int_producer = requires(WorkerPool& pool, Callable&& callable) {
  pool.add_producer<int>(std::forward<Callable>(callable));
};

static_assert(accepts_work<StoredLvalueTask>);
static_assert(!accepts_work<StoredLvalueTask&>);
static_assert(accepts_int_producer<StoredLvalueProducer>);
static_assert(!accepts_int_producer<StoredLvalueProducer&>);
static_assert(!accepts_work<decltype([] {})>);
static_assert(!accepts_work<StoredLvalueProducer>);
static_assert(!accepts_int_producer<decltype([] { return 1; })>);
static_assert(!accepts_int_producer<decltype([]() -> std::expected<long, StarException> { return 1; })>);

template <typename Promise>
concept borrows_result = requires(Promise&& promise) { std::forward<Promise>(promise).get(); };

using IntPromise = WorkerPoolPromise<int>;
static_assert(std::same_as<decltype(std::declval<IntPromise&>().get()),
    std::expected<std::reference_wrapper<int>, StarException>>);
static_assert(std::same_as<decltype(std::declval<IntPromise const&>().get()),
    std::expected<std::reference_wrapper<int const>, StarException>>);
static_assert(!borrows_result<IntPromise> && !borrows_result<IntPromise const>);
}

TEST(WorkerPoolTest, ConcreteMoveOnlyCallablesAreInvokedAsStoredLvalues) {
  WorkerPool pool("stored-lvalue");
  int observed = 0;
  auto task = pool.add_work(StoredLvalueTask{std::make_unique<int>(23), std::ref(observed)});
  auto producer = pool.add_producer<int>(StoredLvalueProducer{std::make_unique<int>(47)});
  ASSERT_TRUE(pool.finish());
  ASSERT_TRUE(task.finish());
  EXPECT_EQ(observed, 23);
  auto result = producer.get();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->get(), 47);
}

TEST(WorkerPoolTest, NullFunctionPointersPublishRepeatableErrorsAndDrainContinues) {
  WorkerPool pool("null-functions");
  std::expected<void, StarException> (*work)() = nullptr;
  std::expected<int, StarException> (*producer)() = nullptr;
  auto task = pool.add_work(work);
  auto result = pool.add_producer<int>(producer);
  StarException diagnostic("user task failed");
  auto failed = pool.add_work([diagnostic]() -> std::expected<void, StarException> {
    return std::unexpected(diagnostic);
  });
  int observed = 0;
  auto recovery = pool.add_work([&]() -> std::expected<void, StarException> {
    observed = 1;
    return {};
  });
  EXPECT_EQ(task.poll(), false);
  EXPECT_EQ(recovery.wait(0), false);
  ASSERT_TRUE(pool.finish());
  auto failure = task.finish();
  ASSERT_FALSE(failure);
  auto observer = task;
  EXPECT_TRUE(observer.done());
  EXPECT_FALSE(observer.wait(0));
  auto repeated_failure = observer.finish();
  ASSERT_FALSE(repeated_failure);
  EXPECT_STREQ(failure.error().what(), repeated_failure.error().what());
  auto value = result.get();
  ASSERT_FALSE(value);
  auto const result_observer = result;
  EXPECT_TRUE(result_observer.done());
  EXPECT_FALSE(result_observer.poll());
  auto repeated_value = result_observer.get();
  ASSERT_FALSE(repeated_value);
  EXPECT_STREQ(value.error().what(), repeated_value.error().what());
  auto checked = failed.poll();
  ASSERT_FALSE(checked);
  EXPECT_STREQ(checked.error().what(), diagnostic.what());
  EXPECT_EQ(recovery.poll(), true);
  ASSERT_TRUE(recovery.finish());
  EXPECT_EQ(observed, 1);
}

TEST(WorkerPoolTest, CancelledConcreteCallablesNeverRun) {
  WorkerPool pool("cancelled-callables");
  int calls = 0;
  auto task = pool.add_work([value = std::make_unique<int>(1), &calls]() -> std::expected<void, StarException> {
    calls += *value;
    return {};
  });
  auto producer = pool.add_producer<int>([value = std::make_unique<int>(2), &calls]() -> std::expected<int, StarException> {
    calls += *value;
    return *value;
  });
  ASSERT_TRUE(pool.stop());
  EXPECT_FALSE(task.finish());
  EXPECT_FALSE(producer.get());
  EXPECT_EQ(calls, 0);
}

#if GTEST_HAS_DEATH_TEST
namespace {
enum class WorkPath { Worker, Drain, Cancel, Reject };

void expect_job_context(WorkerPool& pool, unsigned workers) {
  EXPECT_EQ(pool.worker_count(), workers);
  EXPECT_TRUE(pool.is_worker_thread());
  EXPECT_FALSE(pool.can_parallelize());
  EXPECT_FALSE(pool.start(1));
  EXPECT_FALSE(pool.stop());
  EXPECT_FALSE(pool.finish());
}

void exercise_capture_cleanup(WorkPath path) {
  std::latch destroyed(1);
  int calls = 0;
  auto const workers = path == WorkPath::Worker ? 1u : 0u;
  auto const submitter = std::this_thread::get_id();
  WorkerPool pool("capture-cleanup");
  if (workers != 0) {
    ASSERT_TRUE(pool.start(workers));
  } else if (path == WorkPath::Reject) {
    ASSERT_TRUE(pool.stop());
  }
  EXPECT_FALSE(pool.is_worker_thread());
  EXPECT_EQ(pool.can_parallelize(), workers != 0);
  auto task = pool.add_work([&, cleanup = make_scope_exit([&] {
    EXPECT_EQ(std::this_thread::get_id() != submitter, path == WorkPath::Worker);
    // Submission takes the queue mutex even if the pool's queries are lock-free.
    // The sole worker cannot have started this new job while still in cleanup.
    auto nested = pool.add_work([]() -> std::expected<void, StarException> { return {}; });
    EXPECT_EQ(nested.done(), path != WorkPath::Worker);
    expect_job_context(pool, workers);
    destroyed.count_down();
  })]() -> std::expected<void, StarException> {
    ++calls;
    expect_job_context(pool, workers);
    return {};
  });
  if (path == WorkPath::Drain) {
    ASSERT_TRUE(pool.finish());
  } else if (path == WorkPath::Cancel) {
    ASSERT_TRUE(pool.stop());
  }
  destroyed.wait(); // Completion alone need not mean the capture has been released.
  bool const should_run = path == WorkPath::Worker || path == WorkPath::Drain;
  EXPECT_EQ(task.finish().has_value(), should_run);
  EXPECT_EQ(calls, should_run ? 1 : 0);
  EXPECT_FALSE(pool.is_worker_thread());
}

void exercise_cooperative_drain() {
  std::latch busy(1), resume(1), tail_ran(1);
  auto const caller = std::this_thread::get_id();
  WorkerPool nested("nested-drain");
  WorkerPool pool("cooperative-drain");
  ASSERT_TRUE(pool.start(1));
  auto running = pool.add_work([&]() -> std::expected<void, StarException> {
    busy.count_down();
    resume.wait();
    return {};
  });
  busy.wait();
  auto inner = nested.add_work([&]() -> std::expected<void, StarException> {
    EXPECT_TRUE(nested.is_worker_thread());
    expect_job_context(pool, 1); // Draining another pool must not hide the outer identity.
    return {};
  });
  auto head = pool.add_work([&]() -> std::expected<void, StarException> {
    EXPECT_EQ(std::this_thread::get_id(), caller);
    EXPECT_TRUE(nested.finish());
    EXPECT_FALSE(nested.is_worker_thread());
    EXPECT_TRUE(pool.is_worker_thread());
    resume.count_down();
    tail_ran.wait(); // A threaded finisher must leave work for the actual worker.
    return {};
  });
  auto tail = pool.add_work([&]() -> std::expected<void, StarException> {
    EXPECT_NE(std::this_thread::get_id(), caller);
    tail_ran.count_down();
    return {};
  });
  ASSERT_TRUE(pool.finish());
  EXPECT_TRUE(running.finish());
  EXPECT_TRUE(head.finish());
  EXPECT_TRUE(tail.finish());
  EXPECT_TRUE(inner.finish());
  EXPECT_FALSE(pool.is_worker_thread());
  EXPECT_FALSE(pool.can_parallelize());
}
}

TEST(WorkerPoolTest, CaptureDestructionAndNestedDrainsCanReenterWithoutDeadlock) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_EXIT({
    // A stuck lock or join must fail only this child, on every supported OS.
    std::thread([] {
      std::this_thread::sleep_for(std::chrono::seconds(10));
      std::_Exit(124);
    }).detach();
    for (auto path : {WorkPath::Worker, WorkPath::Drain, WorkPath::Cancel, WorkPath::Reject})
      exercise_capture_cleanup(path);
    exercise_cooperative_drain();
    std::_Exit(testing::Test::HasFailure() ? 1 : 0);
  }, testing::ExitedWithCode(0), "");
}
#endif

TEST(WorkerPoolTest, StopCancelsQueuedWorkAndRestartUsesFreshWorkers) {
  std::atomic<bool> entered{false};
  std::atomic<bool> released{false};
  int queued_calls = 0;
  WorkerPool pool("stop-restart");
  ReleaseOnExit release_on_exit{released};
  ASSERT_TRUE(pool.start(1));
  auto running = pool.add_producer<int>([&]() -> std::expected<int, StarException> {
    entered.store(true);
    entered.notify_all();
    released.wait(false);
    return 41;
  });
  entered.wait(false);
  auto queued = pool.add_producer<int>([&]() -> std::expected<int, StarException> {
    ++queued_calls;
    return 43;
  });
  std::optional<std::expected<void, StarException>> stopped;
  std::thread stopper([&] { stopped.emplace(pool.stop()); });
  auto cancelled = queued.wait(5000);
  release_on_exit.open();
  stopper.join();
  EXPECT_FALSE(cancelled);
  ASSERT_TRUE(stopped);
  ASSERT_TRUE(*stopped);
  EXPECT_FALSE(queued.get());
  EXPECT_EQ(queued_calls, 0);
  auto completed = running.get();
  ASSERT_TRUE(completed);
  EXPECT_EQ(completed->get(), 41);
  auto rejected = pool.add_producer<int>([]() -> std::expected<int, StarException> { return 47; });
  EXPECT_FALSE(rejected.get());

  ASSERT_TRUE(pool.start(2));
  auto fresh = pool.add_producer<int>([]() -> std::expected<int, StarException> { return 53; });
  auto ready = fresh.wait(5000);
  EXPECT_TRUE(ready && *ready);
  ASSERT_TRUE(pool.finish());
  auto value = fresh.get();
  ASSERT_TRUE(value);
  EXPECT_EQ(value->get(), 53);
}

TEST(WorkerPoolTest, ReconfigurationRetiresOnlyThePreviousWorkerGeneration) {
  WorkerPool pool("reconfigure");
  ASSERT_TRUE(pool.start(1));
  auto before = pool.add_producer<int>([]() -> std::expected<int, StarException> { return 59; });
  auto before_ready = before.wait(5000);
  EXPECT_TRUE(before_ready && *before_ready);
  ASSERT_TRUE(pool.start(3));
  auto after = pool.add_producer<int>([]() -> std::expected<int, StarException> { return 61; });
  auto after_ready = after.wait(5000);
  EXPECT_TRUE(after_ready && *after_ready);
  ASSERT_TRUE(pool.finish());
  auto before_value = before.get();
  auto after_value = after.get();
  ASSERT_TRUE(before_value);
  ASSERT_TRUE(after_value);
  EXPECT_EQ(before_value->get(), 59);
  EXPECT_EQ(after_value->get(), 61);
}
