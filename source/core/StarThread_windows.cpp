#include <windows.h>
import std;
import star.thread_native;
import star.thread;
import star.format;

namespace Star {

static DWORD WINAPI run_thread(void* data) {
  unsigned long exception_stack_size = 131072;
  SetThreadStackGuarantee(&exception_stack_size);
  static_cast<NativeThread*>(data)->run_worker();
  return 0;
}

std::expected<void, StarException> NativeThread::launch_native() {
  auto handle = CreateThread(nullptr, 0, run_thread, this, 0, nullptr);
  if (!handle)
    return std::unexpected(StarException::format("Failed to create thread, error {}", GetLastError()));
  m_handle = reinterpret_cast<std::uintptr_t>(handle);
  return {};
}

std::expected<void, StarException> NativeThread::join_native() {
  auto handle = reinterpret_cast<HANDLE>(m_handle);
  auto status = WaitForSingleObject(handle, INFINITE);
  if (status == WAIT_FAILED)
    return std::unexpected(StarException::format("Failed to join thread, error {}", GetLastError()));
  if (status != WAIT_OBJECT_0)
    return std::unexpected(StarException::format("Unexpected thread wait status {}", status));
  if (!CloseHandle(handle))
    return std::unexpected(StarException::format("Failed to close joined thread, error {}", GetLastError()));
  m_handle = 0;
  return {};
}

void Thread::sleep_tail(std::chrono::steady_clock::duration remaining) {
  // At most 10ms remain. A sub-2ms tail gives up the time slice with Sleep(0).
  Sleep(static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(remaining / 2).count()));
}

}
