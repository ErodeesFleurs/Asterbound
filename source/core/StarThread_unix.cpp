#include <stdio.h>

#include <limits.h>
#include <pthread.h>
#include <time.h>
#ifdef STAR_SYSTEM_FREEBSD
#include <pthread_np.h>
#endif
import std;
import star.thread_native;
import star.thread;
import star.string;
import star.format;

#ifdef MAXCOMLEN
static constexpr std::size_t max_thread_name_length = MAXCOMLEN;
#else
static constexpr std::size_t max_thread_name_length = 16;
#endif

namespace Star {

static void* run_thread(void* data) {
  auto& thread = *static_cast<NativeThread*>(data);
#ifdef STAR_SYSTEM_MACOS
  char native_name[max_thread_name_length];
  std::snprintf(native_name, sizeof(native_name), "%s", thread.name.utf8().c_str());
  pthread_setname_np(native_name);
#endif
  thread.run_worker();
  return nullptr;
}

std::expected<void, StarException> NativeThread::launch_native() {
  if (int error = pthread_create(&m_handle, nullptr, run_thread, this))
    return std::unexpected(StarException::format("Failed to create thread, error {}", error));

#ifndef STAR_SYSTEM_MACOS
  char native_name[max_thread_name_length];
  std::snprintf(native_name, sizeof(native_name), "%s", name.utf8().c_str());
#ifdef STAR_SYSTEM_FREEBSD
  pthread_set_name_np(m_handle, native_name);
#elif defined(STAR_SYSTEM_NETBSD)
  pthread_setname_np(m_handle, "%s", native_name);
#else
  pthread_setname_np(m_handle, native_name);
#endif
#endif
  return {};
}

std::expected<void, StarException> NativeThread::join_native() {
  if (int error = pthread_join(m_handle, nullptr))
    return std::unexpected(StarException::format("Failed to join thread, error {}", error));
  return {};
}

template <typename Mutex>
concept diagnostic_mutex = std::same_as<Mutex, std::mutex> || std::same_as<Mutex, std::recursive_mutex>;

template <diagnostic_mutex Mutex>
static void lock_native(std::unique_lock<Mutex>& lock) {
#ifndef STAR_SYSTEM_MACOS
  timespec deadline;
  clock_gettime(CLOCK_REALTIME, &deadline);
  deadline.tv_sec += 15;
  auto& mutex = *lock.mutex();
  if (pthread_mutex_timedlock(mutex.native_handle(), &deadline) == 0) {
    lock = std::unique_lock<Mutex>(mutex, std::adopt_lock);
    return;
  }
  Star::print_stack("Mutex lock is taking too long");
#endif
  lock.lock();
}

void lock_with_diagnostics(std::unique_lock<std::recursive_mutex>& lock) { lock_native(lock); }
void lock_with_diagnostics(std::unique_lock<std::mutex>& lock) {
  if (!lock.try_lock()) lock_native(lock);
}

void Thread::sleep_tail(std::chrono::steady_clock::duration remaining) {
  // Bisect the final wait, but let the OS sleep the last millisecond rather
  // than spending it in a succession of ever shorter system calls.
  std::this_thread::sleep_for(remaining > std::chrono::milliseconds(1) ? remaining / 2 : remaining);
}

}
