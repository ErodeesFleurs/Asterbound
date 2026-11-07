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

// A shared completion signal carrying an owning failure diagnostic.
class WorkerPoolHandle {
public:
  bool done() const;
  [[nodiscard]] std::expected<bool, StarException> wait(unsigned millis) const;
  [[nodiscard]] std::expected<bool, StarException> poll() const;
  [[nodiscard]] std::expected<void, StarException> finish() const;

private:
  friend WorkerPool;
  struct Impl {
    std::mutex mutex;
    std::condition_variable condition;
    std::atomic<bool> done{false};
    std::optional<StarException> error;
  };
  WorkerPoolHandle(std::shared_ptr<Impl> impl);
  std::shared_ptr<Impl> m_impl;
};

// Results stay owned by the shared promise and can be observed repeatedly.
template <typename ResultType>
class WorkerPoolPromise {
public:
  bool done() const;
  [[nodiscard]] std::expected<bool, StarException> wait(unsigned millis) const;
  [[nodiscard]] std::expected<bool, StarException> poll() const;
  [[nodiscard]] std::expected<std::reference_wrapper<ResultType>, StarException> get();
  [[nodiscard]] std::expected<std::reference_wrapper<ResultType const>, StarException> get() const;

private:
  friend WorkerPool;
  struct Impl {
    std::mutex mutex;
    std::condition_variable condition;
    std::optional<ResultType> result;
    std::optional<StarException> error;
    std::atomic<bool> done{false};
  };
  WorkerPoolPromise(std::shared_ptr<Impl> impl);
  std::shared_ptr<Impl> m_impl;
};

class WorkerPool {
public:
  // A queue-only pool can execute accepted jobs synchronously in finish().
  WorkerPool(String name);
  [[nodiscard]] static std::expected<std::unique_ptr<WorkerPool>, StarException> create(String name, unsigned threadCount);
  ~WorkerPool();

  // Worker threads borrow this object's locks and queues: its address is fixed.
  WorkerPool(WorkerPool const&) = delete;
  WorkerPool& operator=(WorkerPool const&) = delete;
  WorkerPool(WorkerPool&&) = delete;
  WorkerPool& operator=(WorkerPool&&) = delete;

  [[nodiscard]] std::expected<void, StarException> start(unsigned threadCount);
  // Stop accepting work, fail queued jobs, and join running jobs.
  [[nodiscard]] std::expected<void, StarException> stop();
  // Drain accepted work and join workers. Do not call from this pool's jobs.
  [[nodiscard]] std::expected<void, StarException> finish();
  // Store the concrete callable and invoke it as an lvalue; only the queue erases it.
  template <typename Callable>
    requires (std::constructible_from<std::decay_t<Callable>, Callable&&>
        && std::invocable<std::decay_t<Callable>&>
        && std::same_as<std::invoke_result_t<std::decay_t<Callable>&>, std::expected<void, StarException>>)
  WorkerPoolHandle addWork(Callable&& work);
  template <typename ResultType, typename Callable>
    requires (std::constructible_from<std::decay_t<Callable>, Callable&&>
        && std::invocable<std::decay_t<Callable>&>
        && std::same_as<std::invoke_result_t<std::decay_t<Callable>&>, std::expected<ResultType, StarException>>)
  WorkerPoolPromise<ResultType> addProducer(Callable&& producer);
  std::size_t getWorkerCount() const;
  bool isWorkerThread() const;

private:
  class WorkerThread : public Thread {
  public:
    WorkerThread(WorkerPool& parent, std::stop_token stopToken);
    ~WorkerThread() override;
    [[nodiscard]] std::expected<void, StarException> run() override;
    WorkerPool& parent;
    std::stop_token stopToken;
    std::atomic<bool> waiting{false};
    std::thread::id id;
  };

  // Task failures belong to their completion state; only checked invocation of
  // the adapter itself can fail at the queue boundary.
  using QueuedWork = UniqueFunction<void(std::optional<StarException>)>;
  void queueWork(QueuedWork work);
  template <typename Callable>
  static auto invokeTask(Callable& callable) -> std::invoke_result_t<Callable&> {
    // A callable's arbitrary bool conversion is not an emptiness predicate.
    // UniqueFunction and other checked wrappers report their own empty state.
    if constexpr (std::is_pointer_v<Callable> || std::is_member_pointer_v<Callable>) {
      if (callable == nullptr)
        return std::unexpected(StarException("Cannot invoke an empty UniqueFunction"));
    }
    return std::invoke(callable);
  }
  // Require m_threadMutex; never join while m_workMutex is held.
  [[nodiscard]] std::expected<void, StarException> stopWorkers();
  [[nodiscard]] std::expected<void, StarException> joinWorkers();

  String m_name;
  std::mutex m_threadMutex;
  // One cancellation domain per worker generation; replace only after joining.
  std::stop_source m_workerStopSource{std::nostopstate};
  std::vector<std::unique_ptr<WorkerThread>> m_workerThreads;
  mutable std::mutex m_workMutex;
  std::condition_variable m_workCondition;
  std::deque<QueuedWork> m_pendingWork;
  std::thread::id m_finishingThread;
  bool m_acceptingWork = true;
};

template <typename ResultType>
bool WorkerPoolPromise<ResultType>::done() const {
  return m_impl->done.load(std::memory_order_acquire);
}

template <typename ResultType>
[[nodiscard]] std::expected<bool, StarException> WorkerPoolPromise<ResultType>::wait(unsigned millis) const {
  std::unique_lock locker(m_impl->mutex);
  if (millis != 0) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(millis);
    m_impl->condition.wait_until(locker, deadline,
        [this] { return m_impl->done.load(std::memory_order_acquire); });
  }
  if (m_impl->error)
    return std::unexpected(*m_impl->error);
  return m_impl->done.load(std::memory_order_acquire);
}

template <typename ResultType>
[[nodiscard]] std::expected<bool, StarException> WorkerPoolPromise<ResultType>::poll() const {
  return wait(0);
}

template <typename ResultType>
[[nodiscard]] std::expected<std::reference_wrapper<ResultType>, StarException> WorkerPoolPromise<ResultType>::get() {
  m_impl->done.wait(false, std::memory_order_acquire);
  if (m_impl->error)
    return std::unexpected(*m_impl->error);
  return std::reference_wrapper<ResultType>(*m_impl->result);
}

template <typename ResultType>
[[nodiscard]] std::expected<std::reference_wrapper<ResultType const>, StarException> WorkerPoolPromise<ResultType>::get() const {
  m_impl->done.wait(false, std::memory_order_acquire);
  if (m_impl->error)
    return std::unexpected(*m_impl->error);
  return std::reference_wrapper<ResultType const>(*m_impl->result);
}

template <typename ResultType>
WorkerPoolPromise<ResultType>::WorkerPoolPromise(std::shared_ptr<Impl> impl)
  : m_impl(std::move(impl)) {}

template <typename Callable>
  requires (std::constructible_from<std::decay_t<Callable>, Callable&&>
      && std::invocable<std::decay_t<Callable>&>
      && std::same_as<std::invoke_result_t<std::decay_t<Callable>&>, std::expected<void, StarException>>)
WorkerPoolHandle WorkerPool::addWork(Callable&& work) {
  auto state = std::make_shared<WorkerPoolHandle::Impl>();
  queueWork([state, work = std::forward<Callable>(work)](std::optional<StarException> cancelled) mutable {
    auto result = cancelled ? std::expected<void, StarException>(std::unexpected(std::move(*cancelled))) : invokeTask(work);
    std::lock_guard lock(state->mutex);
    if (!result)
      state->error.emplace(std::move(result.error()));
    state->done.store(true, std::memory_order_release);
    state->done.notify_all();
    state->condition.notify_all();
  });
  return state;
}

template <typename ResultType, typename Callable>
  requires (std::constructible_from<std::decay_t<Callable>, Callable&&>
      && std::invocable<std::decay_t<Callable>&>
      && std::same_as<std::invoke_result_t<std::decay_t<Callable>&>, std::expected<ResultType, StarException>>)
WorkerPoolPromise<ResultType> WorkerPool::addProducer(Callable&& producer) {
  auto state = std::make_shared<typename WorkerPoolPromise<ResultType>::Impl>();
  queueWork([state, producer = std::forward<Callable>(producer)](std::optional<StarException> cancelled) mutable {
    auto result = cancelled ? std::expected<ResultType, StarException>(std::unexpected(std::move(*cancelled))) : invokeTask(producer);
    std::lock_guard lock(state->mutex);
    if (result)
      state->result.emplace(std::move(*result));
    else
      state->error.emplace(std::move(result.error()));
    state->done.store(true, std::memory_order_release);
    state->done.notify_all();
    state->condition.notify_all();
  });
  return state;
}

}

export module star.worker_pool;

export namespace Star {
  using ::Star::WorkerPoolExceptionTag;
  using ::Star::WorkerPoolException;
  using ::Star::WorkerPoolHandle;
  using ::Star::WorkerPoolPromise;
  using ::Star::WorkerPool;
}
