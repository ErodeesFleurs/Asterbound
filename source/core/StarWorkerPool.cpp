import std;
import star.unique_function;
import star.string;
import star.format;
import star.output_proxy;
import star.thread;
import star.worker_pool;
import star.logging;

namespace Star {

bool WorkerPoolHandle::done() const {
  return m_state->done();
}

std::expected<bool, StarException> WorkerPoolHandle::wait(unsigned millis) const {
  return m_state->wait(millis);
}

std::expected<bool, StarException> WorkerPoolHandle::poll() const {
  return m_state->wait(0);
}

std::expected<void, StarException> WorkerPoolHandle::finish() const {
  return m_state->finish();
}

WorkerPoolHandle::WorkerPoolHandle(std::shared_ptr<CompletionState> state) : m_state(std::move(state)) {}

namespace {

// A callback may drain another pool on this thread. Keep both identities
// until the inner scope ends; a single "current pool" would allow self-join.
class PoolCallbackScope {
  using Link = std::optional<std::reference_wrapper<PoolCallbackScope const>>;
  static inline thread_local constinit Link s_active;
  WorkerPool const& m_pool;
  Link m_previous;

public:
  explicit PoolCallbackScope(WorkerPool const& pool) noexcept
    : m_pool(pool), m_previous(s_active) {
    s_active = std::cref(*this);
  }
  PoolCallbackScope(PoolCallbackScope const&) = delete;
  PoolCallbackScope& operator=(PoolCallbackScope const&) = delete;
  ~PoolCallbackScope() { s_active = m_previous; }

  static bool contains(WorkerPool const& pool) noexcept {
    for (auto scope = s_active; scope; scope = scope->get().m_previous) {
      if (std::addressof(scope->get().m_pool) == std::addressof(pool))
        return true;
    }
    return false;
  }
};

void log_pool_failure(String const& name, std::string_view operation, StarException const& error) {
  EmergencyLog::error("Worker pool {} {} failed: {}", name, operation, Star::output_exception(ExceptionDiagnostic(error), true));
}

void record_failure(std::optional<StarException>& first, String const& name, std::string_view operation, StarException&& error) {
  if (!first)
    first.emplace(std::move(error));
  else
    log_pool_failure(name, operation, error);
}

}

WorkerPool::WorkerPool(String name) : m_name(std::move(name)) {}

std::expected<std::unique_ptr<WorkerPool>, StarException> WorkerPool::create(String name, unsigned thread_count) {
  auto result = std::make_unique<WorkerPool>(std::move(name));
  auto started = result->start(thread_count);
  if (!started)
    return std::unexpected(std::move(started.error()));
  return result;
}

WorkerPool::~WorkerPool() {
  // A callback may already hold the lifecycle lock or be the worker another
  // thread is joining. Neither case can safely destroy this pool.
  if (is_worker_thread())
    Star::fatal_error("Cannot destroy a worker pool from one of its jobs", false);
  std::lock_guard lifecycle_lock(m_lifecycle_mutex);
  auto result = stop_workers();
  if (!result)
    log_pool_failure(m_name, "destruction", result.error());
  if (!m_worker_threads.empty())
    Star::fatal_error("Cannot destroy a worker pool with unjoined workers", false);
}

std::expected<void, StarException> WorkerPool::start(unsigned thread_count) {
  if (is_worker_thread())
    return std::unexpected(WorkerPoolException("Cannot reconfigure a worker pool from one of its jobs"));
  std::lock_guard lifecycle_lock(m_lifecycle_mutex);
  if (thread_count == 0)
    return stop_workers();

  if (!m_worker_threads.empty()) {
    {
      std::lock_guard work_lock(m_work_mutex);
      m_worker_stop_source.request_stop();
    }
    m_work_condition.notify_all();
    auto joined = join_workers();
    if (!joined)
      return joined;
  }

  std::optional<StarException> failure;
  {
    std::lock_guard work_lock(m_work_mutex);
    m_worker_stop_source = std::stop_source{};
    m_worker_threads.reserve(thread_count);
    for (unsigned i = 0; i < thread_count; ++i) {
      auto worker = std::make_unique<WorkerThread>(*this, m_worker_stop_source.get_token());
      auto started = worker->start();
      if (!started) {
        failure.emplace(std::move(started.error()));
        break;
      }
      m_worker_threads.push_back(std::move(worker));
    }
    m_accepting_work = !failure;
    // No worker from a partially started generation may begin a queued job.
    if (failure)
      m_worker_stop_source.request_stop();
  }
  if (failure) {
    auto stopped = stop_workers();
    if (!stopped)
      log_pool_failure(m_name, "rollback", stopped.error());
    return std::unexpected(std::move(*failure));
  }
  return {};
}

std::expected<void, StarException> WorkerPool::stop() {
  if (is_worker_thread())
    return std::unexpected(WorkerPoolException("Cannot stop a worker pool from one of its jobs"));
  std::lock_guard lifecycle_lock(m_lifecycle_mutex);
  return stop_workers();
}

std::expected<void, StarException> WorkerPool::stop_workers() {
  PoolCallbackScope callbacks(*this);
  std::deque<QueuedWork> cancelled;
  {
    std::lock_guard work_lock(m_work_mutex);
    m_accepting_work = false;
    m_worker_stop_source.request_stop();
    cancelled.swap(m_pending_work);
  }
  if (!m_worker_threads.empty())
    m_work_condition.notify_all();

  std::optional<StarException> failure;
  if (!cancelled.empty()) {
    std::optional<StarException> reason(WorkerPoolException("Worker pool stopped before work began"));
    do {
      auto result = cancelled.front()(reason);
      if (!result)
        record_failure(failure, m_name, "cancellation", std::move(result.error()));
      cancelled.pop_front();
    } while (!cancelled.empty());
  }
  return join_workers(std::move(failure));
}

std::expected<void, StarException> WorkerPool::join_workers(std::optional<StarException> work_failure) {
  std::optional<StarException> failure;
  if (!m_worker_threads.empty()) {
    for (auto const& worker : m_worker_threads) {
      auto result = worker->join();
      if (!result)
        record_failure(failure, m_name, "join", std::move(result.error()));
    }
    std::lock_guard work_lock(m_work_mutex);
    if (failure)
      std::erase_if(m_worker_threads, [](std::unique_ptr<WorkerThread> const& worker) { return worker->is_joined(); });
    else
      m_worker_threads.clear();
  }
  if (failure) {
    if (work_failure)
      log_pool_failure(m_name, "queued work", *work_failure);
    return std::unexpected(std::move(*failure));
  }
  if (work_failure)
    return std::unexpected(std::move(*work_failure));
  return {};
}

std::expected<void, StarException> WorkerPool::finish() {
  if (is_worker_thread())
    return std::unexpected(WorkerPoolException("Cannot finish a worker pool from one of its jobs"));
  std::lock_guard lifecycle_lock(m_lifecycle_mutex);
  PoolCallbackScope callbacks(*this);
  std::optional<StarException> failure;
  auto execute = [&](QueuedWork& work) {
    auto result = work({});
    if (!result)
      record_failure(failure, m_name, "completion", std::move(result.error()));
  };

  std::unique_lock work_lock(m_work_mutex);
  m_accepting_work = false;
  if (m_worker_threads.empty()) {
    std::deque<QueuedWork> pending;
    pending.swap(m_pending_work);
    work_lock.unlock();
    while (!pending.empty()) {
      execute(pending.front());
      pending.pop_front();
    }
  } else {
    // Leave the remaining queue available while the caller executes each job.
    while (!m_pending_work.empty()) {
      {
        auto work = take_work();
        work_lock.unlock();
        execute(work);
      }
      std::this_thread::yield();
      work_lock.lock();
    }
    m_worker_stop_source.request_stop();
    work_lock.unlock();
    m_work_condition.notify_all();
  }
  return join_workers(std::move(failure));
}

std::size_t WorkerPool::worker_count() const {
  std::lock_guard lock(m_work_mutex);
  return m_worker_threads.size();
}

bool WorkerPool::is_worker_thread() const {
  return PoolCallbackScope::contains(*this);
}

bool WorkerPool::can_parallelize() const {
  // This is a scheduling hint, not a reservation against a concurrent stop.
  return !is_worker_thread() && worker_count() != 0;
}

WorkerPool::WorkerThread::WorkerThread(WorkerPool& parent, std::stop_token stop_token)
  : Thread(Star::format("WorkerThread for WorkerPool '{}'", parent.m_name)),
    parent(parent), stop_token(std::move(stop_token)) {}

WorkerPool::QueuedWork WorkerPool::take_work() {
  auto work = std::move(m_pending_work.front());
  m_pending_work.pop_front();
  return work;
}

std::expected<void, StarException> WorkerPool::WorkerThread::run() {
  PoolCallbackScope callbacks(parent);
  std::unique_lock work_lock(parent.m_work_mutex);
  for (;;) {
    parent.m_work_condition.wait(work_lock, [&] { return !parent.m_pending_work.empty() || stop_token.stop_requested(); });
    if (stop_token.stop_requested())
      return {};
    {
      auto work = parent.take_work();
      work_lock.unlock();
      auto result = work({});
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
    work_lock.lock();
  }
}

void WorkerPool::queue_work(QueuedWork work) {
  std::unique_lock work_lock(m_work_mutex);
  if (m_accepting_work) {
    m_pending_work.push_back(std::move(work));
    bool notify = !m_worker_threads.empty();
    work_lock.unlock();
    if (notify)
      m_work_condition.notify_one();
  } else {
    work_lock.unlock();
    PoolCallbackScope callbacks(*this);
    auto result = work(WorkerPoolException("Cannot submit work to a stopped worker pool"));
    if (!result)
      log_pool_failure(m_name, "cancellation", result.error());
    work = nullptr; // Release the queue owner before restoring the outer identity.
  }
}

}
