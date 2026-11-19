import std;
import star.algorithm;
import star.unique_function;
import star.thread;
import star.string;
import star.listener;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

namespace {
using IntResult = std::expected<int, StarException>;

template <typename Function, typename... Args>
concept can_invoke_thread = requires(String const& name, Function&& function, Args&&... args) {
  Thread::invoke(name, std::forward<Function>(function), std::forward<Args>(args)...);
};

template <typename Return, typename Function>
concept can_create_thread = requires(String const& name, Function&& function) {
  ThreadFunction<Return>::create(std::forward<Function>(function), name);
};

struct ConvertibleResult {
  using value_type = int;
  operator IntResult() const;
};

struct Immobile {
  Immobile(Immobile&&) = delete;
};

struct UnstorableTask {
  std::expected<Immobile, StarException> operator()() const;
};

using WrongError = decltype([] { return std::expected<int, int>(42); });
struct BorrowedResult {
  IntResult& operator()() const;
};
struct RvalueTask {
  IntResult operator()() &&;
};
using ConvertibleTask = decltype([] { return ConvertibleResult{}; });
using OwnedArgument = decltype([](std::unique_ptr<int>& value) -> IntResult { return *value; });

static_assert(!can_invoke_thread<WrongError> && !can_create_thread<int, WrongError>);
static_assert(!can_invoke_thread<ConvertibleTask> && !can_create_thread<int, ConvertibleTask>);
static_assert(!can_invoke_thread<decltype([] { return 42; })>);
static_assert(!can_invoke_thread<BorrowedResult> && !can_create_thread<int, BorrowedResult>);
static_assert(!can_invoke_thread<RvalueTask> && !can_create_thread<int, RvalueTask>);
static_assert(!can_create_thread<void, decltype([] { return std::expected<void, int>{}; })>);
static_assert(!can_invoke_thread<UnstorableTask> && !can_create_thread<Immobile, UnstorableTask>);
static_assert(can_invoke_thread<OwnedArgument, std::unique_ptr<int>>);
static_assert(!can_invoke_thread<OwnedArgument, std::unique_ptr<int>&>);

struct ProbeThread final : Thread {
  using Thread::Thread;
  std::function<void()> probe;
  std::expected<void, StarException> run() override {
    probe();
    return {};
  }
};

// Unlike a type containing a latch, the concrete probe would be movable if
// Thread accidentally allowed moving the owner of a borrowed virtual run().
static_assert(!std::is_move_constructible_v<ProbeThread> && !std::is_move_assignable_v<ProbeThread>);
static_assert(!std::is_copy_constructible_v<ProbeThread> && !std::is_copy_assignable_v<ProbeThread>);
static_assert(std::movable<ThreadFunction<int>> && std::movable<ThreadFunction<void>>);

#if GTEST_HAS_DEATH_TEST
[[noreturn]] void run_bounded(std::invocable auto&& scenario) {
  std::thread([] {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::_Exit(124);
  }).detach();
  scenario();
  std::_Exit(testing::Test::HasFailure() ? 1 : 0);
}

void exercise_restart() {
  std::binary_semaphore entered(0), resume(0);
  unsigned runs = 0;
  ProbeThread thread("restart");
  thread.probe = [&] {
    ++runs;
    entered.release();
    resume.acquire();
  };
  EXPECT_EQ(std::as_const(thread).name(), "restart");
  for (unsigned i = 1; i <= 3; ++i) {
    EXPECT_TRUE(thread.is_joined());
    EXPECT_FALSE(thread.is_running());
    ASSERT_TRUE(thread.start().value_or(false));
    entered.acquire();
    EXPECT_TRUE(thread.is_running());
    EXPECT_FALSE(thread.is_joined());
    EXPECT_FALSE(thread.start().value_or(true));
    resume.release();
    while (thread.is_running())
      std::this_thread::yield();
    EXPECT_FALSE(thread.is_joined());
    EXPECT_FALSE(thread.start().value_or(true));
    EXPECT_TRUE(thread.join().value_or(false));
    EXPECT_FALSE(thread.join().value_or(true));
    EXPECT_EQ(runs, i);
  }
}

void exercise_concurrent_join() {
  ProbeThread thread("self-join");
  std::latch entered(1), joining(1), check(1);
  thread.probe = [&] {
    entered.count_down();
    check.wait();
    EXPECT_TRUE(thread.is_running());
    EXPECT_FALSE(thread.is_joined());
    EXPECT_FALSE(thread.join().has_value());
    EXPECT_FALSE(thread.is_joined());
  };
  ASSERT_TRUE(thread.start().value_or(false));
  entered.wait();
  auto joined = std::async(std::launch::async, [&] {
    joining.count_down();
    return thread.join();
  });
  joining.wait();
  EXPECT_EQ(joined.wait_for(std::chrono::milliseconds(25)), std::future_status::timeout);
  EXPECT_FALSE(thread.is_joined());
  check.count_down();
  EXPECT_TRUE(joined.get().value_or(false));
  EXPECT_TRUE(thread.is_joined());
  EXPECT_FALSE(thread.is_running());
}

void exercise_self_finish() {
  ThreadFunction<int> task;
  std::latch published(1), rejected(1);
  auto started = Thread::invoke("self-finish", [&]() -> IntResult {
    published.wait();
    EXPECT_FALSE(task.finish());
    EXPECT_FALSE(task.is_finished());
    rejected.count_down();
    return 17;
  });
  ASSERT_TRUE(started);
  task = std::move(*started);
  published.count_down();
  rejected.wait();
  EXPECT_EQ(task.finish().value_or(0), 17);
  EXPECT_FALSE(task.finish());
}

template <typename Return>
  requires (std::same_as<Return, int> || std::same_as<Return, void>)
void exercise_capture_release(bool fail) {
  auto const caller = std::this_thread::get_id();
  unsigned releases = 0;
  bool finishing = false;
  ThreadFunction<Return> task;
  auto cleanup = make_scope_exit([&] {
    EXPECT_EQ(std::this_thread::get_id(), caller);
    if (std::this_thread::get_id() != caller)
      return;
    ++releases;
    EXPECT_TRUE(finishing);
    if (!finishing)
      return;
    EXPECT_TRUE(task.is_finished());
    EXPECT_FALSE(task.is_running());
    auto nested = task.finish();
    if constexpr (std::is_void_v<Return>) {
      EXPECT_TRUE(nested);
    } else {
      ASSERT_FALSE(nested);
      EXPECT_FALSE(nested.error().template is<IOException>());
    }
  });
  auto started = Thread::invoke("capture-release",
      [cleanup = std::move(cleanup), fail]() -> std::expected<Return, StarException> {
        if (fail)
          return std::unexpected(IOException("capture-release", false));
        if constexpr (std::is_void_v<Return>)
          return {};
        else
          return 42;
      });
  ASSERT_TRUE(started);
  task = std::move(*started);
  while (task.is_running())
    std::this_thread::yield();
  EXPECT_FALSE(task.is_finished());
  EXPECT_EQ(releases, 0u);
  finishing = true;
  auto result = task.finish();
  finishing = false;
  EXPECT_EQ(releases, 1u);
  if (fail) {
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().template is<IOException>());
  } else {
    ASSERT_TRUE(result);
    if constexpr (!std::is_void_v<Return>) {
      EXPECT_EQ(*result, 42);
    }
  }
  EXPECT_EQ(task.finish().has_value(), std::is_void_v<Return>);
}

void exercise_result_move() {
  struct Value {
    explicit Value(std::function<void()>& observe) : observe(observe) {}
    Value(Value&& other) noexcept : observe(other.observe), payload(std::exchange(other.payload, 0)) {
      observe();
    }
    std::function<void()>& observe;
    int payload = 42;
  };
  std::function<void()> observe;
  ThreadFunction<Value> task;
  auto const caller = std::this_thread::get_id();
  bool armed = false;
  observe = [&] {
    if (std::this_thread::get_id() == caller && std::exchange(armed, false)) {
      EXPECT_FALSE(task.finish());
    }
  };
  auto started = Thread::invoke("result-move", [&]() -> std::expected<Value, StarException> {
    return Value(observe);
  });
  ASSERT_TRUE(started);
  task = std::move(*started);
  armed = true;
  auto result = task.finish();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->payload, 42);
  EXPECT_FALSE(armed);
  EXPECT_FALSE(task.finish());
}

void exercise_handle_retirement(bool replace) {
  auto const caller = std::this_thread::get_id();
  bool retiring = false;
  unsigned releases = 0;
  {
    ThreadFunction<int> task;
    auto outgoing = Thread::invoke("outgoing", [cleanup = make_scope_exit([&] {
      EXPECT_EQ(std::this_thread::get_id(), caller);
      if (std::this_thread::get_id() != caller || !retiring)
        return;
      ++releases;
      auto nested = task.finish();
      if (replace) {
        EXPECT_EQ(nested.value_or(-1), 22);
      } else {
        EXPECT_FALSE(nested);
      }
    })]() -> IntResult { return 11; });
    ASSERT_TRUE(outgoing);
    task = std::move(*outgoing);
    if (replace) {
      auto incoming = Thread::invoke("incoming", []() -> IntResult { return 22; });
      ASSERT_TRUE(incoming);
      retiring = true;
      task = std::move(*incoming);
      EXPECT_EQ(releases, 1u);
      EXPECT_FALSE(task.finish());
    } else {
      retiring = true;
    }
  }
  EXPECT_EQ(releases, 1u);
}
#endif
}

#if GTEST_HAS_DEATH_TEST
TEST(Thread, JoinedRunningAndStoppedBeforeRestart) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_EXIT(run_bounded(exercise_restart), testing::ExitedWithCode(0), "");
}

TEST(Thread, ExternalJoinDoesNotBlockWorkerQueryOrSelfJoin) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_EXIT(run_bounded(exercise_concurrent_join), testing::ExitedWithCode(0), "");
}

TEST(Thread, FailedSelfFinishPreservesTypedResult) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_EXIT(run_bounded(exercise_self_finish), testing::ExitedWithCode(0), "");
}

TEST(Thread, FinishReleasesCapturesOutsideJoinBeforeReturning) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_EXIT(run_bounded([] {
    for (bool fail : {false, true}) {
      exercise_capture_release<int>(fail);
      exercise_capture_release<void>(fail);
    }
  }), testing::ExitedWithCode(0), "");
}

TEST(Thread, MovingResultCannotReenterAndConsumeItTwice) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_EXIT(run_bounded(exercise_result_move), testing::ExitedWithCode(0), "");
}

TEST(Thread, RetiringCaptureSeesReplacementOrWithdrawnResult) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_EXIT(run_bounded([] {
    exercise_handle_retirement(false);
    exercise_handle_retirement(true);
  }), testing::ExitedWithCode(0), "");
}
#endif

TEST(Thread, InvokeErrors) {
  auto function = Thread::invoke("test", []() -> std::expected<void, StarException> {
    return std::unexpected(StarException("thread callback failure"));
  });
  ASSERT_TRUE(function);
  auto finished = function->finish();
  ASSERT_FALSE(finished);
  EXPECT_TRUE(finished.error().is<StarException>());
  EXPECT_NE(std::string_view(finished.error().what()).find("thread callback failure"), std::string_view::npos);
}

TEST(Thread, InvokeReturn) {
  auto task = Thread::invoke("test", []() -> std::expected<String, StarException> {
    return String("TestValue");
  });
  ASSERT_TRUE(task);
  auto value = task->finish();
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, String("TestValue"));
  auto consumed = task->finish();
  ASSERT_FALSE(consumed);
}

TEST(Thread, MoveOnlyCallableAndMemberInvocation) {
  auto owned = std::make_unique<int>(42);
  auto task = Thread::invoke("move-only", [value = std::move(owned)]() -> std::expected<int, StarException> { return *value; });
  ASSERT_TRUE(task);
  EXPECT_FALSE(owned);
  auto result = task->finish();
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, 42);
  struct Target {
    int value = 7;
    [[nodiscard]] std::expected<int, StarException> add(int amount) { return value += amount; }
  } target;
  auto member = Thread::invoke("member", &Target::add, std::ref(target), 5);
  ASSERT_TRUE(member);
  auto added = member->finish();
  ASSERT_TRUE(added);
  EXPECT_EQ(*added, 12);
  EXPECT_EQ(target.value, 12);
  auto empty = ThreadFunction<void>::create(UniqueFunction<std::expected<void, StarException>()>{}, "empty");
  ASSERT_TRUE(empty);
  EXPECT_FALSE(empty->finish());
}

TEST(Thread, InvokeUsesStoredLvaluesAndUnwrapsReferenceArguments) {
  struct Overloaded {
    IntResult operator()(int& n, std::unique_ptr<int>& amount) & { return n += *amount; }
    ConvertibleResult operator()(std::reference_wrapper<int>&, std::unique_ptr<int>&) & = delete;
    IntResult operator()(int&, std::unique_ptr<int>&) && = delete;
  };
  int n = 5;
  auto task = Thread::invoke("stored-args", Overloaded{}, std::ref(n), std::make_unique<int>(7));
  ASSERT_TRUE(task);
  EXPECT_EQ(task->finish().value_or(0), 12);
  EXPECT_EQ(n, 12);
}

TEST(Thread, NullTargetsLaunchWithCheckedTaskFailures) {
  struct Target {
    IntResult member(int);
  } target;
  auto member = Thread::invoke("null-member", decltype(&Target::member){}, std::ref(target), 1);
  ASSERT_TRUE(member);
  EXPECT_FALSE(member->finish());
  using Function = IntResult (*)(int);
  auto function = Thread::invoke("null-function", Function{}, 1);
  ASSERT_TRUE(function);
  EXPECT_FALSE(function->finish());
  auto typed = ThreadFunction<int>::create(nullptr, "null-typed");
  ASSERT_TRUE(typed);
  EXPECT_FALSE(typed->finish());
  auto empty = ThreadFunction<void>::create(nullptr, "null-void");
  ASSERT_TRUE(empty);
  EXPECT_FALSE(empty->finish());
}

TEST(Thread, UniqueCallableOwnershipAlignmentAndEmptyState) {
  struct Witness {
    std::reference_wrapper<int> deaths;
    ~Witness() { ++deaths.get(); }
  };
  int deaths = 0;
  {
    UniqueFunction<int()> small([owner = std::make_unique<Witness>(std::ref(deaths))] { return 42; });
    UniqueFunction<int()> moved(std::move(small));
    EXPECT_FALSE(small);
    auto initial = moved();
    ASSERT_TRUE(initial);
    EXPECT_EQ(*initial, 42);
    auto emptied = small();
    ASSERT_FALSE(emptied);
    struct alignas(64) Aligned {
      std::unique_ptr<Witness> owner;
      int operator()() {
        return reinterpret_cast<std::uintptr_t>(this) % alignof(Aligned) == 0 ? 7 : -1;
      }
    };
    UniqueFunction<int()> large(Aligned{std::make_unique<Witness>(std::ref(deaths))});
    auto aligned = large();
    ASSERT_TRUE(aligned);
    EXPECT_EQ(*aligned, 7);
    moved = std::move(large);
    EXPECT_EQ(deaths, 1);
    EXPECT_FALSE(large);
    auto transferred = moved();
    ASSERT_TRUE(transferred);
    EXPECT_EQ(*transferred, 7);
    moved = nullptr;
    EXPECT_EQ(deaths, 2);
    int (*empty)() = nullptr;
    UniqueFunction<int()> nullFunction(empty);
    EXPECT_FALSE(nullFunction);
    auto nullResult = nullFunction();
    ASSERT_FALSE(nullResult);
    int referenced = 1;
    UniqueFunction<int&()> reference([&]() -> int& { return referenced; });
    auto referenceResult = reference();
    ASSERT_TRUE(referenceResult);
    referenceResult->get() = 17;
    EXPECT_EQ(referenced, 17);
    struct ReferenceSource {
      std::string text;
      std::string& operator()() { return text; }
    };
    static_assert(!std::is_constructible_v<UniqueFunction<std::string_view const&()>, ReferenceSource>);
    struct ErrorTask {
      std::expected<void, StarException> operator()();
    };
    static_assert(!std::is_constructible_v<UniqueFunction<void()>, ErrorTask>);
  }
  EXPECT_EQ(deaths, 2);
}

TEST(Thread, UniqueCallableWithdrawsSourceBeforeTargetMoveAndDestruction) {
  struct Observation {
    UniqueFunction<void()>& owner;
    bool armed = false;
    unsigned moves = 0;
    unsigned destructions = 0;
    void observe() {
      EXPECT_FALSE(owner);
      if (!owner)
        owner = nullptr;
    }
  };
  struct Target {
    Observation& state;
    explicit Target(Observation& state) : state(state) {}
    Target(Target&& other) noexcept : state(other.state) {
      if (state.armed) {
        ++state.moves;
        state.observe();
      }
    }
    ~Target() {
      if (state.armed) {
        ++state.destructions;
        state.observe();
      }
    }
    void operator()() {}
  };
  UniqueFunction<void()> source;
  Observation observation{source};
  source = Target(observation);
  observation.armed = true;
  UniqueFunction<void()> moved(std::move(source));
  EXPECT_EQ(observation.moves, 1u);
  EXPECT_EQ(observation.destructions, 1u);
  observation.armed = false;
  source = std::move(moved);
  observation.armed = true;
  source = nullptr;
  EXPECT_EQ(observation.destructions, 2u);
}

TEST(Thread, UniqueCallableDiscardsOrdinaryValuesButNotExpectedErrors) {
  struct BusinessValue { using error_type = int; };
  int calls = 0;
  UniqueFunction<void()> work([&] {
    ++calls;
    return BusinessValue{};
  });
  EXPECT_TRUE(work());
  EXPECT_EQ(calls, 1);
  using Failure = std::expected<int, StarException>;
  Failure failure(std::unexpected(IOException("unique task", false)));
  auto borrow = [&]() -> Failure& { return failure; };
  static_assert(!std::constructible_from<UniqueFunction<void()>, decltype(borrow)>);
  UniqueFunction<Failure&()> reference(borrow);
  auto result = reference();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->get().error().is<IOException>());
  result->get() = 7;
  EXPECT_EQ(failure.value_or(0), 7);
}

TEST(Thread, ReadersWriterMutex) {
  ReadersWriterMutex mutex;
  std::shared_lock rl1(mutex);
  std::shared_lock rl2(mutex);
  std::unique_lock wl(mutex, std::defer_lock);
  EXPECT_FALSE(wl.try_lock());
  rl1.unlock();
  EXPECT_FALSE(wl.try_lock());
  rl2.unlock();
  EXPECT_TRUE(wl.try_lock());
}

TEST(Thread, ReadersWriterMutexPrefersQueuedWriter) {
  ReadersWriterMutex mutex;
  std::shared_lock initialReader(mutex);
  std::latch releaseWriter(1);
  std::atomic<unsigned> order = 0;
  std::atomic<unsigned> writerOrder = 0;
  std::atomic<unsigned> readerOrder = 0;
  std::thread writer([&] {
    std::unique_lock lock(mutex);
    writerOrder = ++order;
    releaseWriter.wait();
  });

  // A held reader prevents the writer from acquiring the lock. Once another
  // read attempt fails, the writer is queued and later readers must wait.
  bool writerQueued = false;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while ((deadline - std::chrono::steady_clock::now()).count() > 0) {
    if (!mutex.try_lock_shared()) {
      writerQueued = true;
      break;
    }
    mutex.unlock_shared();
    std::this_thread::yield();
  }
  EXPECT_TRUE(writerQueued);
  std::thread reader([&] {
    std::shared_lock lock(mutex);
    readerOrder = ++order;
  });
  initialReader.unlock();

  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (writerOrder == 0 && (deadline - std::chrono::steady_clock::now()).count() > 0)
    std::this_thread::yield();
  EXPECT_EQ(writerOrder.load(), 1u);
  EXPECT_EQ(readerOrder.load(), 0u);
  releaseWriter.count_down();
  writer.join();
  reader.join();
  EXPECT_EQ(readerOrder.load(), 2u);
}

TEST(Listener, DisconnectWaitsForInvocationAndPreventsLaterTriggers) {
  ListenerGroup group;
  std::latch entered(1);
  std::latch release(1);
  std::atomic<int> calls = 0;
  auto listener = std::make_shared<CallbackListener>([&]() -> std::expected<void, StarException> {
    ++calls;
    entered.count_down();
    release.wait();
    return {};
  });
  auto subscription = group.addListener(listener);
  std::thread triggering([&] { EXPECT_TRUE(group.trigger()); });
  entered.wait();
  std::latch disconnecting(1);
  auto disconnected = std::async(std::launch::async, [&] {
    disconnecting.count_down();
    subscription.disconnect();
  });
  disconnecting.wait();
  EXPECT_EQ(disconnected.wait_for(std::chrono::milliseconds(25)), std::future_status::timeout);
  release.count_down();
  triggering.join();
  disconnected.get();
  EXPECT_TRUE(group.trigger());
  EXPECT_EQ(calls.load(), 1);
}

TEST(Listener, ReentrantTriggerAndSelfDisconnect) {
  ListenerGroup group;
  ListenerSubscription subscription;
  int calls = 0;
  auto listener = std::make_shared<CallbackListener>([&]() -> std::expected<void, StarException> {
    if (++calls == 1)
      return group.trigger();
    subscription.disconnect();
    return {};
  });
  subscription = group.addListener(listener);
  ASSERT_TRUE(group.trigger());
  ASSERT_TRUE(group.trigger());
  EXPECT_EQ(calls, 2);
}

TEST(Listener, RegistryMutationFromCallbackAndErrorRelease) {
  ListenerGroup group;
  int calls = 0;
  bool failNow = true;
  auto listener = std::make_shared<CallbackListener>([&]() -> std::expected<void, StarException> {
    ++calls;
    // A callback may mutate the registry without holding its lock, including
    // adding and immediately cancelling another listener.
    auto tracker = std::make_shared<TrackerListener>();
    auto temporary = group.addListener(tracker);
    group.clearExpiredListeners();
    if (failNow)
      return std::unexpected(StarException("listener failure"));
    return {};
  });
  auto subscription = group.addListener(listener);
  auto failed = group.trigger();
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<StarException>());
  EXPECT_NE(std::string_view(failed.error().what()).find("listener failure"), std::string_view::npos);
  failNow = false;
  ASSERT_TRUE(group.trigger());
  EXPECT_EQ(calls, 2);
  auto disconnected = std::async(std::launch::async, [&] { subscription.disconnect(); });
  EXPECT_EQ(disconnected.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  disconnected.get();
  ASSERT_TRUE(group.trigger());
  EXPECT_EQ(calls, 2);
}

TEST(Listener, DuplicateOwnerIdentityAndScopedCancellation) {
  ListenerGroup group;
  int calls = 0;
  auto listener = std::make_shared<CallbackListener>([&]() -> std::expected<void, StarException> {
    ++calls;
    return {};
  });
  auto first = group.addListener(listener);
  // An aliasing shared_ptr must not create a second registration.
  std::shared_ptr<Listener> alias(listener, listener.get());
  auto duplicate = group.addListener(alias);
  ASSERT_TRUE(group.trigger());
  EXPECT_EQ(calls, 1);
  duplicate.disconnect();
  ASSERT_TRUE(group.trigger());
  EXPECT_EQ(calls, 1);
  {
    auto replacement = group.addListener(listener);
    ASSERT_TRUE(group.trigger());
    EXPECT_EQ(calls, 2);
    // Disconnecting an old registration cannot cancel its replacement.
    first.disconnect();
    ASSERT_TRUE(group.trigger());
    EXPECT_EQ(calls, 3);
  }
  ASSERT_TRUE(group.trigger());
  EXPECT_EQ(calls, 3);
}

TEST(Listener, TrackerCoalescesAndOwningCallbackDoesNotKeepOwnerAlive) {
  ListenerGroup group;
  auto tracker = std::make_shared<TrackerListener>();
  auto tracking = group.addListener(tracker);
  EXPECT_FALSE(tracker->pullTriggered());
  ASSERT_TRUE(group.trigger());
  ASSERT_TRUE(group.trigger());
  EXPECT_TRUE(tracker->pullTriggered());
  EXPECT_FALSE(tracker->pullTriggered());
  auto cache = std::make_shared<int>(0);
  std::weak_ptr<int> weakCache = cache;
  auto listener = std::make_shared<CallbackListener>([cache]() -> std::expected<void, StarException> {
    ++*cache;
    return {};
  });
  auto subscription = group.addListener(listener);
  cache.reset();
  ASSERT_TRUE(group.trigger());
  auto retainedCache = weakCache.lock();
  ASSERT_TRUE(retainedCache);
  EXPECT_EQ(*retainedCache, 1);
  retainedCache.reset();
  listener.reset();
  EXPECT_TRUE(weakCache.expired());
  group.clearExpiredListeners();
  group.clearAllListeners();
  EXPECT_TRUE(tracker->pullTriggered());
  ASSERT_TRUE(group.trigger());
  EXPECT_FALSE(tracker->pullTriggered());
}

TEST(Listener, ClearAllFromCallbackCancelsRemainingSnapshot) {
  ListenerGroup group;
  int calls = 0;
  auto first = std::make_shared<CallbackListener>([&]() -> std::expected<void, StarException> {
    ++calls;
    group.clearAllListeners();
    return {};
  });
  auto second = std::make_shared<CallbackListener>([&]() -> std::expected<void, StarException> {
    ++calls;
    group.clearAllListeners();
    return {};
  });
  auto firstSubscription = group.addListener(first);
  auto secondSubscription = group.addListener(second);
  ASSERT_TRUE(group.trigger());
  EXPECT_EQ(calls, 1);
  ASSERT_TRUE(group.trigger());
  EXPECT_EQ(calls, 1);
}

TEST(Listener, ConcurrentTriggersRemainSerial) {
  ListenerGroup group;
  std::latch entered(1);
  std::latch release(1);
  std::latch secondStarting(1);
  std::atomic<int> calls = 0;
  std::atomic<int> running = 0;
  std::atomic<bool> overlapped = false;
  auto listener = std::make_shared<CallbackListener>([&]() -> std::expected<void, StarException> {
    if (running.fetch_add(1) != 0)
      overlapped = true;
    if (calls.fetch_add(1) == 0) {
      entered.count_down();
      release.wait();
    }
    running.fetch_sub(1);
    return {};
  });
  auto subscription = group.addListener(listener);
  std::thread first([&] { EXPECT_TRUE(group.trigger()); });
  entered.wait();
  std::thread second([&] {
    secondStarting.count_down();
    EXPECT_TRUE(group.trigger());
  });
  secondStarting.wait();
  release.count_down();
  first.join();
  second.join();
  EXPECT_EQ(calls.load(), 2);
  EXPECT_FALSE(overlapped.load());
}
