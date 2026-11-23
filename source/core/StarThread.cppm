module;

import std;
import star.unique_function;
import star.format;
import star.string;
import star.type_identity;

namespace Star {

struct ThreadImpl;
struct ThreadFunctionImpl;

template <typename Value>
concept thread_value = std::same_as<Value, void> || (std::is_object_v<Value> && !std::is_array_v<Value>
    && std::constructible_from<Value, Value&&>);

template <thread_value Return>
class ThreadFunction;

template <typename Result>
concept thread_result = requires { typename Result::value_type; } && thread_value<typename Result::value_type>
    && std::same_as<Result, std::expected<typename Result::value_type, StarException>>;

template <typename Function, typename... Args>
using ThreadInvokeResult = std::invoke_result_t<std::decay_t<Function>&, std::unwrap_ref_decay_t<Args>&...>;

template <typename Function, typename... Args>
concept thread_invocable = std::constructible_from<std::decay_t<Function>, Function&&>
    && (std::constructible_from<std::unwrap_ref_decay_t<Args>, Args&&> && ...)
    && (sizeof...(Args) == 0 || (std::is_move_constructible_v<std::decay_t<Function>>
        && (std::is_move_constructible_v<std::unwrap_ref_decay_t<Args>> && ...)))
    && thread_result<ThreadInvokeResult<Function, Args...>>;

template <typename Function, typename Return>
concept thread_task_for = std::is_null_pointer_v<std::remove_cvref_t<Function>>
    || (thread_invocable<Function> && std::same_as<ThreadInvokeResult<Function>, std::expected<Return, StarException>>);

class Thread {
public:
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<Thread>(*this, query); }
  virtual std::string_view dynamic_type_name() const noexcept { return type_name<Thread>(); }
  // Wait against a monotonic deadline with a shorter, platform-specific tail.
  static void sleep_precise(std::uint64_t millis);

  template <typename Function, typename... Args>
    requires thread_invocable<Function, Args...>
  [[nodiscard]] static auto invoke(String const& name, Function&& function, Args&&... args);

  Thread(String const& name);
  Thread(Thread const&) = delete;
  Thread(Thread&&) = delete;
  // Derived destructors must stop and join while their borrowed state is alive.
  // Destroying an unjoined thread is a fatal lifetime violation.
  virtual ~Thread();

  Thread& operator=(Thread const&) = delete;
  Thread& operator=(Thread&&) = delete;

  // Start a thread that is currently in the joined state.  Returns true if the
  // thread was joined and is now started, false if the thread was not joined.
  [[nodiscard]] std::expected<bool, StarException> start();

  // Returns true if joined here, false if already joined. A worker cannot join
  // itself. Native join failures leave the thread unjoined for a retry.
  [[nodiscard]] std::expected<bool, StarException> join();

  // These observations never wait for a concurrent join. After run() returns,
  // both are false until join() reclaims the native thread.
  bool is_joined() const noexcept;
  bool is_running() const noexcept;

  String name() const;

protected:
  [[nodiscard]] virtual std::expected<void, StarException> run() = 0;

private:
  friend struct ThreadImpl;
  static void sleep_tail(std::chrono::steady_clock::duration remaining);
  std::unique_ptr<ThreadImpl> m_impl;
};

// A one-shot thread whose owning diagnostic is returned by finish().
template <>
class ThreadFunction<void> {
public:
  ThreadFunction();
  ThreadFunction(ThreadFunction&&) noexcept;
  template <thread_task_for<void> Function = std::nullptr_t>
  [[nodiscard]] static std::expected<ThreadFunction, StarException> create(Function&& function, String const& name) {
    return create_erased(std::forward<Function>(function), name);
  }
  // Destruction joins and reports any unobserved failure to the emergency log.
  ~ThreadFunction();

  ThreadFunction& operator=(ThreadFunction&&);
  // The successful joiner releases the callback here (including on task
  // failure), not on the worker. Native join failures leave it intact.
  [[nodiscard]] std::expected<void, StarException> finish();

  bool is_finished() const noexcept;
  bool is_running() const noexcept;
  explicit operator bool() const noexcept;

  String name() const;

private:
  [[nodiscard]] static std::expected<ThreadFunction, StarException> create_erased(
      UniqueFunction<std::expected<void, StarException>()> function, String const& name);
  std::shared_ptr<ThreadFunctionImpl> m_impl;
};

template <thread_value Return>
class ThreadFunction {
public:
  ThreadFunction() = default;
  ThreadFunction(ThreadFunction&&) noexcept = default;

  template <thread_task_for<Return> Function = std::nullptr_t>
  [[nodiscard]] static std::expected<ThreadFunction, StarException> create(Function&& function, String const& name);

  // The native entry also owns the task. Withdraw this owner's access before
  // joining and running capture destructors that may reenter this handle.
  ~ThreadFunction() { m_result.reset(); }
  ThreadFunction& operator=(ThreadFunction&& other) {
    if (this != std::addressof(other)) {
      // Defer old target destruction until both incoming fields are installed.
      auto previous = std::exchange(m_result, std::move(other.m_result));
      m_function = std::move(other.m_function);
    }
    return *this;
  }

  // A single consumer moves out the result. Later calls return a diagnostic.
  [[nodiscard]] std::expected<Return, StarException> finish();
  bool is_finished() const noexcept { return m_function.is_finished(); }
  bool is_running() const noexcept { return m_function.is_running(); }
  explicit operator bool() const noexcept { return static_cast<bool>(m_function); }
  String name() const { return m_function.name(); }

private:
  struct ResultState {
    // The wrapper also permits values such as std::nullopt_t.
    struct Value {
      explicit Value(Return&& result) : value(std::move(result)) {}
      Return value;
    };
    std::optional<Value> stored;
  };

  static std::expected<ThreadFunction, StarException> attach(
      std::expected<ThreadFunction<void>, StarException>&& native, std::shared_ptr<ResultState> result) {
    if (!native)
      return std::unexpected(std::move(native.error()));
    return ThreadFunction(std::move(*native), std::move(result));
  }

  ThreadFunction(ThreadFunction<void>&& native, std::shared_ptr<ResultState>&& result)
    : m_result(std::move(result)), m_function(std::move(native)) {}

  // Keep the (withdrawn) result member alive during native destruction.
  std::shared_ptr<ResultState> m_result;
  ThreadFunction<void> m_function;
};

// Acquire an unlocked standard guard, retaining the native long-wait stack
// diagnostic on platforms that previously provided it.
void lock_with_diagnostics(std::unique_lock<std::recursive_mutex>& lock);
void lock_with_diagnostics(std::unique_lock<std::mutex>& lock);

class ReadersWriterMutex {
  // Multiple acquisitions are possible while no writer waits. A queued writer
  // blocks all new readers, including a thread that already holds a read lock.
public:
  void lock_shared();
  bool try_lock_shared();
  void unlock_shared();

  void lock();
  bool try_lock();
  void unlock();

private:
  std::mutex m_mutex;
  std::condition_variable m_read_cond;
  std::condition_variable m_write_cond;
  unsigned m_readers = 0;
  unsigned m_waiting_readers = 0;
  unsigned m_waiting_writers = 0;
  bool m_writing = false;
};


class SpinLock {
public:
  void lock();
  bool try_lock();
  void unlock();

private:
  std::atomic_flag m_lock;
};

template <typename Function, typename... Args>
  requires thread_invocable<Function, Args...>
[[nodiscard]] auto Thread::invoke(String const& name, Function&& function, Args&&... args) {
  using Return = typename ThreadInvokeResult<Function, Args...>::value_type;
  if constexpr (sizeof...(Args) == 0) {
    return ThreadFunction<Return>::create(std::forward<Function>(function), name);
  } else {
    using Target = std::remove_reference_t<Function>;
    if constexpr (std::is_pointer_v<Target> || std::is_member_pointer_v<Target>) {
      if (function == nullptr)
        return ThreadFunction<Return>::create(nullptr, name);
    }
    return ThreadFunction<Return>::create(
        [function = std::decay_t<Function>(std::forward<Function>(function)),
         arguments = std::make_tuple(std::forward<Args>(args)...)]() mutable {
          return std::apply(function, arguments);
        }, name);
  }
}

template <thread_value Return>
template <thread_task_for<Return> Function>
std::expected<ThreadFunction<Return>, StarException> ThreadFunction<Return>::create(Function&& function, String const& name) {
  if constexpr (std::is_null_pointer_v<std::remove_cvref_t<Function>>) {
    return attach(ThreadFunction<void>::create(nullptr, name), {});
  } else {
    if constexpr (std::is_pointer_v<std::remove_reference_t<Function>>) {
      if (function == nullptr)
        return create(nullptr, name);
    }

    // One allocation for the concrete target and value; only its owner crosses
    // the erased native boundary. Neither is released on the worker.
    struct Task : ResultState {
      explicit Task(Function&& target) : function(std::forward<Function>(target)) {}
      std::decay_t<Function> function;
    };
    auto task = std::make_shared<Task>(std::forward<Function>(function));
    auto native = ThreadFunction<void>::create([task] -> std::expected<void, StarException> {
      auto value = std::invoke(task->function);
      if (!value)
        return std::unexpected(std::move(value.error()));
      task->stored.emplace(std::move(*value));
      return {};
    }, name);
    return attach(std::move(native), std::move(task));
  }
}

template <thread_value Return>
std::expected<Return, StarException> ThreadFunction<Return>::finish() {
  auto finished = m_function.finish();
  if (!finished) {
    if (m_function.is_finished()) // Task error, not a retryable native join error.
      m_result.reset();
    return std::unexpected(std::move(finished.error()));
  }
  // Claim before user-defined value moves or capture destructors can reenter.
  auto result = std::move(m_result);
  if (!result || !result->stored)
    return std::unexpected(StarException("Thread function has no unconsumed result"));
  return std::expected<Return, StarException>(std::in_place, std::move(result->stored->value));
}

inline void SpinLock::lock() {
  while (!try_lock())
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
  using ::Star::lock_with_diagnostics;
  using ::Star::ReadersWriterMutex;
  using ::Star::SpinLock;
}
