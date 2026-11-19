import std;
import star.thread;
import star.format;
import star.thread_native;
import star.unique_function;
import star.string;
import star.logging;
import star.output_proxy;

namespace Star {

// Leave identity installed through native thread exit (including its TLS
// destructors). The state remains alive until that thread has been joined.
static constinit thread_local std::optional<std::reference_wrapper<NativeThread const>> current_worker;

static bool is_current_worker(NativeThread const& thread) noexcept {
  return current_worker && std::addressof(current_worker->get()) == std::addressof(thread);
}

void NativeThread::run_worker() {
  current_worker = std::cref(*this);
  m_execute(*this);
  m_phase.store(Phase::Finished, std::memory_order_release);
}

std::expected<void, StarException> NativeThread::launch() {
  // Publish first: a short task may finish before creation returns. Only a
  // failed create (no worker) can roll this publication back.
  m_phase.store(Phase::Running, std::memory_order_release);
  auto result = launch_native();
  if (!result)
    m_phase.store(Phase::Joined, std::memory_order_release);
  return result;
}

std::expected<bool, StarException> NativeThread::start() {
  // External start/join operations serialize; the worker itself must never
  // block on its joiner's lock, even while running its TLS destructors.
  if (is_current_worker(*this))
    return false;
  std::lock_guard lock(m_mutex);
  if (!is_joined())
    return false;
  auto started = launch();
  if (!started)
    return std::unexpected(std::move(started.error()));
  return true;
}

std::expected<bool, StarException> NativeThread::join() {
  if (is_current_worker(*this))
    return std::unexpected(StarException("Cannot join the current thread"));
  std::lock_guard lock(m_mutex);
  if (is_joined())
    return false;
  auto joined = join_native();
  if (!joined)
    return std::unexpected(std::move(joined.error()));
  // Native exit, including the worker's final publication, precedes restart.
  m_phase.store(Phase::Joined, std::memory_order_release);
  return true;
}

struct ThreadImpl final : NativeThread {
  ThreadImpl(Thread& owner, String const& name) : NativeThread(name, execute), owner(owner) {}

  static void execute(NativeThread& thread) {
    auto result = static_cast<ThreadImpl&>(thread).owner.run();
    if (!result)
      EmergencyLog::error("Thread {} failed: {}", thread.name, Star::output_exception(ExceptionDiagnostic(result.error()), true));
  }

  Thread& owner;
};

Thread::Thread(String const& name) : m_impl(std::make_unique<ThreadImpl>(*this, name)) {}

Thread::~Thread() {
  if (!is_joined()) {
    EmergencyLog::error("Thread {} destroyed without joining", m_impl->name);
    Star::fatal_error("Cannot destroy an unjoined thread", false);
  }
}

std::expected<bool, StarException> Thread::start() { return m_impl->start(); }
std::expected<bool, StarException> Thread::join() { return m_impl->join(); }
bool Thread::is_joined() const noexcept { return m_impl->is_joined(); }
bool Thread::is_running() const noexcept { return m_impl->is_running(); }
String Thread::name() const { return m_impl->name; }

struct ThreadFunctionImpl final : NativeThread {
  using TaskResult = std::expected<void, StarException>;
  using Task = UniqueFunction<TaskResult()>;

  ThreadFunctionImpl(Task&& function, String const& name) : NativeThread(name, execute), function(std::move(function)) {}

  static void execute(NativeThread& thread) {
    auto& task = static_cast<ThreadFunctionImpl&>(thread);
    task.result = task.function();
  }

  TaskResult finish() {
    auto joined = join();
    if (!joined)
      return std::unexpected(std::move(joined.error()));
    if (!*joined)
      return {};
    // Only the join winner owns this result and callback. Join has released
    // its lock, and a reentrant finish observes Joined, never the task error.
    auto observed = std::move(result);
    function = nullptr;
    return observed;
  }

  void finish_and_report(std::string_view operation) {
    if (is_joined())
      return;
    auto observed = finish();
    if (!observed)
      EmergencyLog::error("Thread function {} failed during {}: {}", name, operation,
          Star::output_exception(ExceptionDiagnostic(observed.error()), true));
    if (!is_joined())
      Star::fatal_error("Cannot discard an unjoined thread function", false);
  }

  Task function;
  TaskResult result;
};

ThreadFunction<void>::ThreadFunction() = default;
ThreadFunction<void>::ThreadFunction(ThreadFunction&&) noexcept = default;

std::expected<ThreadFunction<void>, StarException> ThreadFunction<void>::create_erased(
    UniqueFunction<std::expected<void, StarException>()> function, String const& name) {
  ThreadFunction result;
  result.m_impl = std::make_shared<ThreadFunctionImpl>(std::move(function), name);
  auto started = result.m_impl->launch();
  if (!started)
    return std::unexpected(std::move(started.error()));
  return result;
}

ThreadFunction<void>::~ThreadFunction() {
  // A retired capture may replace this handle while it is being destroyed.
  while (auto retired = std::move(m_impl))
    retired->finish_and_report("destruction");
}

ThreadFunction<void>& ThreadFunction<void>::operator=(ThreadFunction&& other) {
  if (this != &other) {
    if (auto retired = std::exchange(m_impl, std::move(other.m_impl)))
      retired->finish_and_report("reassignment");
  }
  return *this;
}

std::expected<void, StarException> ThreadFunction<void>::finish() {
  if (is_finished())
    return {};
  // A capture's destructor may itself release or replace the handle. Pin the
  // existing allocation until all of the erased target has been destroyed.
  auto impl = m_impl;
  return impl->finish();
}

bool ThreadFunction<void>::is_finished() const noexcept { return !m_impl || m_impl->is_joined(); }
bool ThreadFunction<void>::is_running() const noexcept { return m_impl && m_impl->is_running(); }
ThreadFunction<void>::operator bool() const noexcept { return !is_finished(); }
String ThreadFunction<void>::name() const { return m_impl ? m_impl->name : String(); }

#ifdef STAR_SYSTEM_WINDOWS
void lock_with_diagnostics(std::unique_lock<std::recursive_mutex>& lock) {
  lock.lock();
}
#endif

void Thread::sleep_precise(std::uint64_t millis) {
  using namespace std::chrono;
  using namespace std::chrono_literals;

  // Bound both the native timeout and conversion to steady_clock ticks.
  constexpr auto chunk = 24h;
  constexpr auto chunk_millis = duration_cast<milliseconds>(chunk).count();
  while (millis > chunk_millis) {
    std::this_thread::sleep_for(chunk);
    millis -= chunk_millis;
  }
  if (millis == 0)
    return;

  auto now = steady_clock::now();
  auto deadline = now + milliseconds(millis);
  do {
    auto remaining = deadline - now;
    if (remaining > 10ms)
      std::this_thread::sleep_for(remaining - 10ms);
    else
      sleep_tail(remaining);
    now = steady_clock::now();
  } while (now < deadline);
}

void ReadersWriterMutex::lock_shared() {
  std::unique_lock locker(m_mutex);
  if (m_writing || m_waiting_writers) {
    ++m_waiting_readers;
    m_read_cond.wait(locker, [&] { return !m_writing && !m_waiting_writers; });
    --m_waiting_readers;
  }
  m_readers++;
}

bool ReadersWriterMutex::try_lock_shared() {
  std::lock_guard locker(m_mutex);
  if (m_writing || m_waiting_writers)
    return false;
  m_readers++;
  return true;
}

void ReadersWriterMutex::unlock_shared() {
  std::lock_guard locker(m_mutex);
  if (--m_readers == 0 && m_waiting_writers)
    m_write_cond.notify_one();
}

void ReadersWriterMutex::lock() {
  std::unique_lock locker(m_mutex);
  if (m_readers || m_writing) {
    ++m_waiting_writers;
    m_write_cond.wait(locker, [&] { return !m_readers && !m_writing; });
    --m_waiting_writers;
  }
  m_writing = true;
}

bool ReadersWriterMutex::try_lock() {
  std::lock_guard locker(m_mutex);
  if (m_readers || m_writing)
    return false;
  m_writing = true;
  return true;
}

void ReadersWriterMutex::unlock() {
  std::lock_guard locker(m_mutex);
  m_writing = false;
  if (m_waiting_writers)
    m_write_cond.notify_one();
  else if (m_waiting_readers)
    m_read_cond.notify_all();
}

}
