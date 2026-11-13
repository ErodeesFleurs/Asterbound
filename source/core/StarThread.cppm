module;

import std;
import star.unique_function;
import star.format;
import star.string;
import star.type_identity;

namespace Star {

struct ThreadImpl;
struct ThreadFunctionImpl;

template <typename Return>
class ThreadFunction;

class Thread {
public:
  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<Thread>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return type_name<Thread>(); }
  // Sleep a more precise amount of time, but uses more resources to do so.
  // Should be less likely to sleep much longer than the given amount of time.
  static void sleepPrecise(unsigned millis);

  template <typename Function, typename... Args>
  [[nodiscard]] static auto invoke(String const& name, Function&& f, Args&&... args)
      -> std::expected<ThreadFunction<typename std::invoke_result_t<std::decay_t<Function>&, std::decay_t<Args>&...>::value_type>, StarException>;

  Thread(String const& name);
  Thread(Thread&&);
  // Derived destructors must stop and join while their borrowed state is alive.
  // Destroying an unjoined thread is a fatal lifetime violation.
  virtual ~Thread();

  Thread& operator=(Thread&&);

  // Start a thread that is currently in the joined state.  Returns true if the
  // thread was joined and is now started, false if the thread was not joined.
  [[nodiscard]] std::expected<bool, StarException> start();

  // Wait for a thread to finish and re-join with it. On success isJoined() is
  // true. Returns true if joined here, false if the thread was already joined.
  [[nodiscard]] std::expected<bool, StarException> join();

  // Returns false when this thread been started without being joined.  This is
  // subtlely different than "!isRunning()", in that the thread could have
  // completed its work, but a thread *must* be joined before being restarted.
  bool isJoined() const;

  // Returns false before start() has been called, true immediately after
  // start() has been called, and false once the run() method returns.
  bool isRunning() const;

  String name();

protected:
  [[nodiscard]] virtual std::expected<void, StarException> run() = 0;

private:
  std::unique_ptr<ThreadImpl> m_impl;
};

// A one-shot thread whose owning diagnostic is returned by finish().
template <>
class ThreadFunction<void> {
public:
  ThreadFunction();
  ThreadFunction(ThreadFunction&&);
  [[nodiscard]] static std::expected<ThreadFunction, StarException> create(
      UniqueFunction<std::expected<void, StarException>()> function, String const& name);
  // Destruction joins and reports any unobserved failure to the emergency log.
  ~ThreadFunction();

  ThreadFunction& operator=(ThreadFunction&&);
  [[nodiscard]] std::expected<void, StarException> finish();

  // Returns whether the ThreadFunction::finish method been called and the
  // ThreadFunction has stopped.  Also returns true when the ThreadFunction has
  // been default constructed.
  bool isFinished() const;
  // Returns false if the thread function has stopped running, whether or not
  // finish() has been called.
  bool isRunning() const;

  // Equivalent to !isFinished()
  explicit operator bool() const;

  String name();

private:
  std::unique_ptr<ThreadFunctionImpl> m_impl;
};

template <typename Return>
class ThreadFunction {
public:
  ThreadFunction();
  ThreadFunction(ThreadFunction&&);
  [[nodiscard]] static std::expected<ThreadFunction, StarException> create(
      UniqueFunction<std::expected<Return, StarException>()> function, String const& name);

  ~ThreadFunction();

  ThreadFunction& operator=(ThreadFunction&&);

  // Moves out the result once; subsequent consumption returns a diagnostic.
  [[nodiscard]] std::expected<Return, StarException> finish();

  bool isFinished() const;
  bool isRunning() const;

  explicit operator bool() const;

  String name();

private:
  ThreadFunction<void> m_function;
  std::shared_ptr<std::optional<Return>> m_return;
};

// Acquire an unlocked standard guard, retaining the native long-wait stack
// diagnostic on platforms that previously provided it.
void lockWithDiagnostics(std::unique_lock<std::recursive_mutex>& lock);

class ReadersWriterMutex {
  // Writer-preferring shared locking, including multiple read acquisitions by
  // one thread. Standard guards manage ownership; std::shared_mutex cannot
  // preserve these scheduling and shared-ownership contracts.
public:
  ReadersWriterMutex();

  void lock_shared();
  bool try_lock_shared();
  void unlock_shared();

  void lock();
  bool try_lock();
  void unlock();

private:
  std::mutex m_mutex;
  std::condition_variable m_readCond;
  std::condition_variable m_writeCond;
  unsigned m_readers;
  unsigned m_writers;
  unsigned m_readWaiters;
  unsigned m_writeWaiters;
};


class SpinLock {
public:
  SpinLock();

  void lock();
  bool try_lock();
  void unlock();

private:
  std::atomic_flag m_lock;
};

template <typename Function, typename... Args>
[[nodiscard]] auto Thread::invoke(String const& name, Function&& f, Args&&... args)
    -> std::expected<ThreadFunction<typename std::invoke_result_t<std::decay_t<Function>&, std::decay_t<Args>&...>::value_type>, StarException> {
  using Return = typename std::invoke_result_t<std::decay_t<Function>&, std::decay_t<Args>&...>::value_type;
  return ThreadFunction<Return>::create(
      [function = std::forward<Function>(f), arguments = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        return std::apply([&function](auto&... saved) {
          return std::invoke(function, saved...);
        }, arguments);
      }, name);
}

template <typename Return>
ThreadFunction<Return>::ThreadFunction() {}

template <typename Return>
ThreadFunction<Return>::ThreadFunction(ThreadFunction&&) = default;

template <typename Return>
[[nodiscard]] std::expected<ThreadFunction<Return>, StarException> ThreadFunction<Return>::create(
    UniqueFunction<std::expected<Return, StarException>()> function, String const& name) {
  ThreadFunction result;
  result.m_return = std::make_shared<std::optional<Return>>();
  auto thread = ThreadFunction<void>::create(
      [function = std::move(function), retValue = result.m_return]() mutable -> std::expected<void, StarException> {
        auto value = function();
        if (!value)
          return std::unexpected(std::move(value.error()));
        retValue->emplace(std::move(*value));
        return {};
      }, name);
  if (!thread)
    return std::unexpected(std::move(thread.error()));
  result.m_function = std::move(*thread);
  return result;
}

template <typename Return>
ThreadFunction<Return>::~ThreadFunction() = default;

template <typename Return>
ThreadFunction<Return>& ThreadFunction<Return>::operator=(ThreadFunction&&) = default;

template <typename Return>
[[nodiscard]] std::expected<Return, StarException> ThreadFunction<Return>::finish() {
  auto finished = m_function.finish();
  if (!finished)
    return std::unexpected(std::move(finished.error()));
  if (!m_return || !*m_return)
    return std::unexpected(StarException("Thread function has no unconsumed result"));
  Return result = std::move(**m_return);
  m_return->reset();
  return result;
}

template <typename Return>
bool ThreadFunction<Return>::isFinished() const {
  return m_function.isFinished();
}

template <typename Return>
bool ThreadFunction<Return>::isRunning() const {
  return m_function.isRunning();
}

template <typename Return>
ThreadFunction<Return>::operator bool() const {
  return !isFinished();
}

template <typename Return>
String ThreadFunction<Return>::name() {
  return m_function.name();
}

inline SpinLock::SpinLock() {
  m_lock.clear();
}

inline void SpinLock::lock() {
  while (m_lock.test_and_set(std::memory_order_acquire))
    ;
}

inline void SpinLock::unlock() {
  m_lock.clear(std::memory_order_release);
}

inline bool SpinLock::try_lock() {
  return !m_lock.test_and_set(std::memory_order_acquire);
}

}

export module star.thread;

export namespace Star {
  using ::Star::ThreadImpl;
  using ::Star::ThreadFunctionImpl;
  using ::Star::Thread;
  using ::Star::ThreadFunction;
  template <>
  class ThreadFunction<void>;
  using ::Star::lockWithDiagnostics;
  using ::Star::ReadersWriterMutex;
  using ::Star::SpinLock;
}
