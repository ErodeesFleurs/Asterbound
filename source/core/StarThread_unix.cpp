#include <stdio.h>

#include <limits.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include <pthread.h>
#ifdef STAR_SYSTEM_FREEBSD
#include <pthread_np.h>
#endif
#include <sys/time.h>
#include <errno.h>
import std;
import star.unique_function;
import star.thread;
import star.string;
import star.format;
import star.output_proxy;
import star.logging;

#ifdef MAXCOMLEN
static constexpr std::size_t max_thread_name_length = MAXCOMLEN;
#else
static constexpr std::size_t max_thread_name_length = 16;
#endif

import star.algorithm;
namespace Star {

template <typename State>
concept native_thread_state = std::same_as<State, ThreadImpl> || std::same_as<State, ThreadFunctionImpl>;

template <native_thread_state State>
struct NativeThread {
  static void* run_thread(void* data) {
    auto& state = *static_cast<State*>(data);
#ifdef STAR_SYSTEM_MACOS
    char tname[max_thread_name_length];
    std::snprintf(tname, sizeof(tname), "%s", state.name.utf8_data());
    pthread_setname_np(tname);
#endif
    state.execute();
    state.stopped = true;
    return nullptr;
  }

  explicit NativeThread(String name) : name(std::move(name)) {}

  // Only launch unpublished or joined state; a restart holds mutex.
  [[nodiscard]] std::expected<bool, StarException> launch() {
    stopped = false;
    joined = false;
    int ret = pthread_create(&pthread, nullptr, run_thread, static_cast<State*>(this));
    if (ret != 0) {
      stopped = true;
      joined = true;
      return std::unexpected(StarException(Star::format("Failed to create thread, error {}", ret)));
    }

    // ensure the name is under the max allowed
    char tname[max_thread_name_length];
    std::snprintf(tname, sizeof(tname), "%s", name.utf8_data());

#ifdef STAR_SYSTEM_FREEBSD
    pthread_set_name_np(pthread, tname);
#elif defined(STAR_SYSTEM_NETBSD)
    pthread_setname_np(pthread, "%s", tname);
#elif not defined STAR_SYSTEM_MACOS
    pthread_setname_np(pthread, tname);
#endif
    return true;
  }

  [[nodiscard]] std::expected<bool, StarException> join() {
    std::lock_guard lock(mutex);
    if (joined)
      return false;
    int ret = pthread_join(pthread, nullptr);
    if (ret != 0)
      return std::unexpected(StarException(Star::format("Failed to join thread, error {}", ret)));
    joined = true;
    return true;
  }

  String name;
  pthread_t pthread;
  std::atomic<bool> stopped{true};
  bool joined = true;
  std::mutex mutex;
};

struct ThreadImpl final : NativeThread<ThreadImpl> {
  ThreadImpl(Thread& owner, String name) : NativeThread(std::move(name)), owner(owner) {}

  void execute() {
    auto result = owner.get().run();
    if (!result)
      EmergencyLog::error("Thread {} failed: {}", name, Star::output_exception(ExceptionDiagnostic(result.error()), true));
  }

  std::reference_wrapper<Thread> owner;
};

struct ThreadFunctionImpl final : NativeThread<ThreadFunctionImpl> {
  ThreadFunctionImpl(UniqueFunction<std::expected<void, StarException>()>&& function, String name)
    : NativeThread(std::move(name)), function(std::move(function)) {}

  void execute() {
    auto result = function();
    if (!result)
      error.emplace(std::move(result.error()));
  }

  UniqueFunction<std::expected<void, StarException>()> function;
  std::optional<StarException> error;
};

void lockWithDiagnostics(std::unique_lock<std::recursive_mutex>& lock) {
#ifndef STAR_SYSTEM_MACOS
  timespec deadline;
  clock_gettime(CLOCK_REALTIME, &deadline);
  deadline.tv_sec += 15;
  auto& mutex = *lock.mutex();
  if (pthread_mutex_timedlock(mutex.native_handle(), &deadline) == 0) {
    lock = std::unique_lock<std::recursive_mutex>(mutex, std::adopt_lock);
    return;
  }
  Star::print_stack("Recursive mutex lock is taking too long");
#endif
  lock.lock();
}

void Thread::sleepPrecise(unsigned msecs) {
  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  auto deadline = now + std::chrono::milliseconds(msecs);

  while (deadline - now > std::chrono::milliseconds(10)) {
    usleep(std::chrono::duration_cast<std::chrono::microseconds>(deadline - now - std::chrono::milliseconds(10)).count());
    now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  }

  while (deadline > now) {
    usleep(std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count() / 2);
    now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  }
}

Thread::Thread(String const& name)
  : m_impl(std::make_unique<ThreadImpl>(*this, name)) {}

Thread::Thread(Thread&&) = default;

Thread::~Thread() {
  if (m_impl && !isJoined()) {
    EmergencyLog::error("Thread {} destroyed without joining", name());
    Star::fatal_error("Cannot destroy an unjoined thread", false);
  }
}

Thread& Thread::operator=(Thread&& other) {
  if (this != &other) {
    if (m_impl && !isJoined()) {
      EmergencyLog::error("Thread {} overwritten without joining", name());
      Star::fatal_error("Cannot overwrite an unjoined thread", false);
    }
    m_impl = std::move(other.m_impl);
  }
  return *this;
}

std::expected<bool, StarException> Thread::start() {
  std::lock_guard lock(m_impl->mutex);
  if (!m_impl->joined)
    return false;
  m_impl->owner = *this;
  return m_impl->launch();
}

std::expected<bool, StarException> Thread::join() {
  return m_impl->join();
}

String Thread::name() {
  return m_impl->name;
}

bool Thread::isJoined() const {
  return m_impl->joined;
}

bool Thread::isRunning() const {
  return !m_impl->stopped;
}

ThreadFunction<void>::ThreadFunction() {}

ThreadFunction<void>::ThreadFunction(ThreadFunction&&) = default;

std::expected<ThreadFunction<void>, StarException> ThreadFunction<void>::create(
    UniqueFunction<std::expected<void, StarException>()> function, String const& name) {
  ThreadFunction result;
  result.m_impl = std::make_unique<ThreadFunctionImpl>(std::move(function), name);
  auto started = result.m_impl->launch();
  if (!started)
    return std::unexpected(std::move(started.error()));
  return result;
}

ThreadFunction<void>::~ThreadFunction() {
  auto result = finish();
  if (!result)
    EmergencyLog::error("Thread function {} failed during destruction: {}", name(), Star::output_exception(ExceptionDiagnostic(result.error()), true));
  if (!isFinished())
    Star::fatal_error("Cannot destroy an unjoined thread function", false);
}

ThreadFunction<void>& ThreadFunction<void>::operator=(ThreadFunction&& other) {
  if (this != &other) {
    auto result = finish();
    if (!result)
      EmergencyLog::error("Thread function {} failed during reassignment: {}", name(), Star::output_exception(ExceptionDiagnostic(result.error()), true));
    if (!isFinished())
      Star::fatal_error("Cannot overwrite an unjoined thread function", false);
    m_impl = std::move(other.m_impl);
  }
  return *this;
}

std::expected<void, StarException> ThreadFunction<void>::finish() {
  if (m_impl) {
    auto joined = m_impl->join();
    if (!joined)
      return std::unexpected(std::move(joined.error()));
    if (m_impl->error) {
      auto error = std::move(*m_impl->error);
      m_impl->error.reset();
      return std::unexpected(std::move(error));
    }
  }
  return {};
}

bool ThreadFunction<void>::isFinished() const {
  return !m_impl || m_impl->joined;
}

bool ThreadFunction<void>::isRunning() const {
  return m_impl && !m_impl->stopped;
}

ThreadFunction<void>::operator bool() const {
  return !isFinished();
}

String ThreadFunction<void>::name() {
  if (m_impl)
    return m_impl->name;
  else
    return "";
}

}
