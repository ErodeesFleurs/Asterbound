
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <locale.h>
import std;
import star.unique_function;
import star.thread;
import star.string;
import star.format;
import star.output_proxy;
import star.logging;

import star.algorithm;
namespace Star {


struct ThreadImpl {
  static DWORD WINAPI runThread(void* data) {
    auto& state = *static_cast<ThreadImpl*>(data);
    unsigned long exceptionStackSize = 131072;
    SetThreadStackGuarantee(&exceptionStackSize);
    auto result = state.function();
    if (!result)
      EmergencyLog::error("Thread {} failed: {}", state.name, outputException(ExceptionDiagnostic(result.error()), true));
    state.stopped = true;
    return 0;
  }

  ThreadImpl(UniqueFunction<std::expected<void, StarException>()> function, String name)
    : function(std::move(function)), name(std::move(name)), thread(INVALID_HANDLE_VALUE), stopped(true) {}

  [[nodiscard]] std::expected<bool, StarException> start() {
    std::lock_guard mutexLocker(mutex);
    if (thread != INVALID_HANDLE_VALUE)
      return false;

    stopped = false;
    if (thread == INVALID_HANDLE_VALUE)
      thread = CreateThread(NULL, 0, runThread, (void*)this, 0, NULL);
    if (thread == NULL)
      thread = INVALID_HANDLE_VALUE;
    if (thread == INVALID_HANDLE_VALUE) {
      stopped = true;
      return std::unexpected(StarException(strf("Failed to create thread, error {}", GetLastError())));
    }
    return true;
  }

  [[nodiscard]] std::expected<bool, StarException> join() {
    std::lock_guard mutexLocker(mutex);
    if (thread == INVALID_HANDLE_VALUE)
      return false;
    if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0)
      return std::unexpected(StarException(strf("Failed to join thread, error {}", GetLastError())));
    if (!CloseHandle(thread))
      return std::unexpected(StarException(strf("Failed to close joined thread, error {}", GetLastError())));
    thread = INVALID_HANDLE_VALUE;
    return true;
  }

  UniqueFunction<std::expected<void, StarException>()> function;
  String name;
  HANDLE thread;
  std::atomic<bool> stopped;

private:
  ThreadImpl(ThreadImpl const&);
  ThreadImpl& operator=(ThreadImpl const&);

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

void Thread::sleepPrecise(unsigned msecs) {
  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  auto deadline = now + std::chrono::milliseconds(msecs);

  while (deadline - now > std::chrono::milliseconds(10)) {
    Sleep((deadline - now - std::chrono::milliseconds(10)).count());
    now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  }

  while (deadline > now) {
    if (deadline - now >= std::chrono::milliseconds(2))
      Sleep(((deadline - now) / 2).count());
    else
      Sleep(0);
    now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  }
}

Thread::Thread(String const& name) {
  m_impl.reset(new ThreadImpl([this]() { return run(); }, name));
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
  return m_impl->thread == INVALID_HANDLE_VALUE;
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
  return !m_impl || m_impl->thread == INVALID_HANDLE_VALUE;
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
