module;
import std;
import star.unique_function;
import star.string;
import star.format;
import star.thread;

namespace Star {

struct WorkerPoolExceptionTag {
  static constexpr std::string_view name() { return "WorkerPoolException"; }
};
using WorkerPoolException = StarError<WorkerPoolExceptionTag, StarException>;

class WorkerPool;

template <typename Callable, typename ResultType>
concept worker_invocable_for = requires(Callable& callable) {
  { std::invoke(callable) } -> std::same_as<std::expected<ResultType, StarException>>;
};

template <typename Callable, typename ResultType>
concept worker_task_for = std::constructible_from<std::decay_t<Callable>, Callable&&>
    && worker_invocable_for<std::decay_t<Callable>, ResultType>;

template <typename ResultType>
concept worker_result = std::is_object_v<ResultType> && !std::is_array_v<ResultType>
    && std::constructible_from<ResultType, ResultType&&>;

namespace WorkerPoolDetail {
// A single publisher initializes the result/error before releasing ready.
// Readers never inspect either field while pending. Only timed waiters need
// the mutex: pairing the ready transition with it prevents lost CV wakeups.
struct CompletionState {
  mutable std::mutex mutex;
  mutable std::condition_variable condition;
  std::atomic<bool> ready{false};
  std::optional<StarException> error;

  void publish() {
    {
      std::lock_guard lock(mutex);
      ready.store(true, std::memory_order_release);
    }
    ready.notify_all();
    condition.notify_all();
  }

  bool done() const { return ready.load(std::memory_order_acquire); }

  [[nodiscard]] std::expected<bool, StarException> wait(unsigned millis) const {
    if (!done()) {
      if (millis == 0)
        return false;
      std::unique_lock lock(mutex);
      if (!condition.wait_for(lock, std::chrono::milliseconds(millis), [this] { return done(); }))
        return false;
    }
    if (error)
      return std::unexpected(*error);
    return true;
  }

  [[nodiscard]] std::expected<void, StarException> finish() const {
    if (!done())
      ready.wait(false, std::memory_order_acquire);
    if (error)
      return std::unexpected(*error);
    return {};
  }
};
}

// A shared completion signal carrying an owning failure diagnostic.
class WorkerPoolHandle {
public:
  bool done() const;
  [[nodiscard]] std::expected<bool, StarException> wait(unsigned millis) const;
  [[nodiscard]] std::expected<bool, StarException> poll() const;
  [[nodiscard]] std::expected<void, StarException> finish() const;

private:
  friend WorkerPool;
  using CompletionState = WorkerPoolDetail::CompletionState;
  explicit WorkerPoolHandle(std::shared_ptr<CompletionState> state);
  std::shared_ptr<CompletionState> m_state;
};

// Results stay owned by the shared promise and can be observed repeatedly.
template <worker_result ResultType>
class WorkerPoolPromise {
public:
  bool done() const { return m_state->done(); }
  [[nodiscard]] std::expected<bool, StarException> wait(unsigned millis) const { return m_state->wait(millis); }
  [[nodiscard]] std::expected<bool, StarException> poll() const { return m_state->wait(0); }

  [[nodiscard]] std::expected<std::reference_wrapper<ResultType>, StarException> get() & {
    if (auto ready = m_state->finish(); !ready)
      return std::unexpected(std::move(ready.error()));
    return std::reference_wrapper<ResultType>(*m_state->result);
  }
  [[nodiscard]] std::expected<std::reference_wrapper<ResultType const>, StarException> get() const & {
    if (auto ready = m_state->finish(); !ready)
      return std::unexpected(std::move(ready.error()));
    return std::reference_wrapper<ResultType const>(*m_state->result);
  }
  void get() && = delete;
  void get() const && = delete;

private:
  friend WorkerPool;
  struct State : WorkerPoolDetail::CompletionState {
    std::optional<ResultType> result;
  };
  explicit WorkerPoolPromise(std::shared_ptr<State> state) : m_state(std::move(state)) {}
  std::shared_ptr<State> m_state;
};

class WorkerPool {
public:
  // A queue-only pool can execute accepted jobs synchronously in finish().
  WorkerPool(String name);
  [[nodiscard]] static std::expected<std::unique_ptr<WorkerPool>, StarException> create(String name, unsigned thread_count);
  ~WorkerPool();

  // Worker threads borrow this object's locks and queues: its address is fixed.
  WorkerPool(WorkerPool const&) = delete;
  WorkerPool& operator=(WorkerPool const&) = delete;
  WorkerPool(WorkerPool&&) = delete;
  WorkerPool& operator=(WorkerPool&&) = delete;

  // Lifecycle operations reject reentry from this pool's execution, cancellation,
  // or capture cleanup, including work drained on the caller's thread.
  [[nodiscard]] std::expected<void, StarException> start(unsigned thread_count);
  // Stop accepting work, fail queued jobs, and join running jobs.
  [[nodiscard]] std::expected<void, StarException> stop();
  // Drain accepted work and join workers. Do not call from this pool's jobs.
  [[nodiscard]] std::expected<void, StarException> finish();
  template <worker_task_for<void> Callable>
  WorkerPoolHandle add_work(Callable&& work);
  template <worker_result ResultType, worker_task_for<ResultType> Callable>
  WorkerPoolPromise<ResultType> add_producer(Callable&& producer);
  std::size_t worker_count() const;
  bool is_worker_thread() const;
  bool can_parallelize() const;

private:
  struct WorkerThread final : Thread {
    WorkerThread(WorkerPool& parent, std::stop_token stop_token);
    [[nodiscard]] std::expected<void, StarException> run() override;
    WorkerPool& parent;
    std::stop_token stop_token;
  };

  // Task failures belong to their completion state; only checked invocation of
  // the adapter itself can fail at the queue boundary.
  using QueuedWork = UniqueFunction<void(std::optional<StarException>)>;
  void queue_work(QueuedWork work);
  template <typename ResultType, std::derived_from<WorkerPoolDetail::CompletionState> State, worker_task_for<ResultType> Callable>
  std::shared_ptr<State> submit(Callable&& callable);
  template <typename ResultType, worker_invocable_for<ResultType> Callable>
  static std::expected<ResultType, StarException> invoke_task(Callable& callable) {
    // A callable's arbitrary bool conversion is not an emptiness predicate.
    // UniqueFunction and other checked wrappers report their own empty state.
    if constexpr (std::is_pointer_v<Callable> || std::is_member_pointer_v<Callable>) {
      if (callable == nullptr)
        return std::unexpected(StarException("Cannot invoke an empty UniqueFunction"));
    }
    return std::invoke(callable);
  }
  // Requires m_work_mutex and a nonempty queue. Only a completion-state
  // owner moves; user callables never move or die in the queue lock.
  QueuedWork take_work();
  // Require m_lifecycle_mutex; never join while m_work_mutex is held.
  [[nodiscard]] std::expected<void, StarException> stop_workers();
  [[nodiscard]] std::expected<void, StarException> join_workers(std::optional<StarException> work_failure = {});

  String m_name;
  std::mutex m_lifecycle_mutex;
  // One cancellation domain per worker generation; replace only after joining.
  std::stop_source m_worker_stop_source{std::nostopstate};
  std::vector<std::unique_ptr<WorkerThread>> m_worker_threads;
  mutable std::mutex m_work_mutex;
  std::condition_variable m_work_condition;
  std::deque<QueuedWork> m_pending_work;
  bool m_accepting_work = true;
};

template <typename ResultType, std::derived_from<WorkerPoolDetail::CompletionState> State, worker_task_for<ResultType> Callable>
std::shared_ptr<State> WorkerPool::submit(Callable&& callable) {
  // One allocation owns the result, synchronization, and concrete callable.
  // Only the shared owner moves in the queue, never user-defined objects.
  struct Submission : State {
    explicit Submission(Callable&& value) : task(std::in_place, std::forward<Callable>(value)) {}
    std::optional<std::decay_t<Callable>> task;
  };
  auto state = std::make_shared<Submission>(std::forward<Callable>(callable));
  queue_work([state](std::optional<StarException> cancelled) {
    auto outcome = cancelled ? std::expected<ResultType, StarException>(std::unexpected(std::move(*cancelled)))
                             : invoke_task<ResultType>(*state->task);
    if (!outcome)
      state->error.emplace(std::move(outcome.error()));
    else if constexpr (!std::is_void_v<ResultType>)
      state->result.emplace(std::move(*outcome));
    // The queue holds the state through both notifications. User destructors
    // run outside all completion locks and can observe the finished result.
    state->publish();
    state->task.reset();
  });
  return state;
}

template <worker_task_for<void> Callable>
WorkerPoolHandle WorkerPool::add_work(Callable&& work) {
  return WorkerPoolHandle(submit<void, WorkerPoolDetail::CompletionState>(std::forward<Callable>(work)));
}

template <worker_result ResultType, worker_task_for<ResultType> Callable>
WorkerPoolPromise<ResultType> WorkerPool::add_producer(Callable&& producer) {
  using Promise = WorkerPoolPromise<ResultType>;
  return Promise(submit<ResultType, typename Promise::State>(std::forward<Callable>(producer)));
}

}

export module star.worker_pool;

export namespace Star {
  using ::Star::WorkerPoolExceptionTag;
  using ::Star::WorkerPoolException;
  using ::Star::worker_task_for;
  using ::Star::WorkerPoolHandle;
  using ::Star::WorkerPoolPromise;
  using ::Star::WorkerPool;
}
