import std;
import star.unique_function;
import star.thread;
import star.string;
import star.listener;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

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
  auto functionRet = Thread::invoke("test", []() -> std::expected<String, StarException> {
    return String("TestValue");
  });
  ASSERT_TRUE(functionRet);
  auto value = functionRet->finish();
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, String("TestValue"));
  auto consumed = functionRet->finish();
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
  auto emptyFinish = empty->finish();
  ASSERT_FALSE(emptyFinish);
}

TEST(Thread, UniqueCallableOwnershipAlignmentAndEmptyState) {
  struct Witness {
    std::reference_wrapper<int> deaths;
    ~Witness() { ++deaths.get(); }
  };
  int deaths = 0;
  {
    UniqueFunction<int()> small([owner = std::unique_ptr<Witness>(new Witness{std::ref(deaths)})] { return 42; });
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
    UniqueFunction<int()> large(Aligned{std::unique_ptr<Witness>(new Witness{std::ref(deaths)})});
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
