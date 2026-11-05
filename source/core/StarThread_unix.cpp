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
static constexpr std::size_t MaxThreadNameLength = MAXCOMLEN;
#else
static constexpr std::size_t MaxThreadNameLength = 16;
#endif

import star.algorithm;
namespace Star {

struct ThreadImpl {
  static void* runThread(void* data) {
    auto& state = *static_cast<ThreadImpl*>(data);
#ifdef STAR_SYSTEM_MACOS
    char tname[MaxThreadNameLength];
    std::snprintf(tname, sizeof(tname), "%s", state.name.utf8Ptr());
    pthread_setname_np(tname);
#endif
    auto result = state.function();
    if (!result)
      EmergencyLog::error("Thread {} failed: {}", state.name, outputException(ExceptionDiagnostic(result.error()), true));
    state.stopped = true;
    return nullptr;
  }

  ThreadImpl(UniqueFunction<std::expected<void, StarException>()> function, String name)
    : function(std::move(function)), name(std::move(name)), stopped(true), joined(true) {}

  [[nodiscard]] std::expected<bool, StarException> start() {
    std::lock_guard mutexLocker(mutex);
    if (!joined)
      return false;

    stopped = false;
    joined = false;
    int ret = pthread_create(&pthread, NULL, &runThread, (void*)this);
    if (ret != 0) {
      stopped = true;
      joined = true;
      return std::unexpected(StarException(strf("Failed to create thread, error {}", ret)));
    }

    // ensure the name is under the max allowed
    char tname[MaxThreadNameLength];
    std::snprintf(tname, sizeof(tname), "%s", name.utf8Ptr());

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
    std::lock_guard mutexLocker(mutex);
    if (joined)
      return false;
    int ret = pthread_join(pthread, NULL);
    if (ret != 0)
      return std::unexpected(StarException(strf("Failed to join thread, error {}", ret)));
    joined = true;
    return true;
  }

  UniqueFunction<std::expected<void, StarException>()> function;
  String name;
  pthread_t pthread;
  std::atomic<bool> stopped;
  bool joined;
  std::mutex mutex;
};

struct ThreadFunctionImpl : ThreadImpl {
  ThreadFunctionImpl(UniqueFunction<std::expected<void, StarException>()> function, String name)
    : ThreadImpl(wrapFunction(std::move(function)), std::move(name)) {}

  UniqueFunction<std::expected<void, StarException>()> wrapFunction(
      UniqueFunction<std::expected<void, StarException>()> function) {
    return [function = std::move(function), this]() mutable -> std::expected<void, StarException> {
      auto result = function();
      if (!result)
        error = std::move(result.error());
      return {};
    };
  }

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
  printStack("Recursive mutex lock is taking too long");
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

Thread::Thread(String const& name) {
  m_impl.reset(new ThreadImpl([this]() {
      return run();
    }, name));
}

Thread::Thread(Thread&&) = default;

Thread::~Thread() {
  if (m_impl && !isJoined()) {
    EmergencyLog::error("Thread {} destroyed without joining", name());
    fatalError("Cannot destroy an unjoined thread", false);
  }
}

Thread& Thread::operator=(Thread&& other) {
  if (this != &other) {
    if (m_impl && !isJoined()) {
      EmergencyLog::error("Thread {} overwritten without joining", name());
      fatalError("Cannot overwrite an unjoined thread", false);
    }
    m_impl = std::move(other.m_impl);
  }
  return *this;
}

std::expected<bool, StarException> Thread::start() {
  return m_impl->start();
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
  auto started = result.m_impl->start();
  if (!started)
    return std::unexpected(std::move(started.error()));
  return result;
}

ThreadFunction<void>::~ThreadFunction() {
  auto result = finish();
  if (!result)
    EmergencyLog::error("Thread function {} failed during destruction: {}", name(), outputException(ExceptionDiagnostic(result.error()), true));
  if (!isFinished())
    fatalError("Cannot destroy an unjoined thread function", false);
}

ThreadFunction<void>& ThreadFunction<void>::operator=(ThreadFunction&& other) {
  if (this != &other) {
    auto result = finish();
    if (!result)
      EmergencyLog::error("Thread function {} failed during reassignment: {}", name(), outputException(ExceptionDiagnostic(result.error()), true));
    if (!isFinished())
      fatalError("Cannot overwrite an unjoined thread function", false);
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
