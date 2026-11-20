#include <windows.h>
import std;
import star.format;
import star.string;
import star.lock_file;
import star.string_windows;

namespace Star {

std::expected<bool, StarException> LockFile::lock(std::int64_t timeout_ms) {
  if (owns_lock())
    return true;
  RetryWindow retry(timeout_ms);
  auto path = stringToUtf16(m_filename);
  if (!path)
    return std::unexpected(std::move(path.error()));
  do {
    auto handle = CreateFileW(path->get(), GENERIC_READ, 0, nullptr, OPEN_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
      m_handle = reinterpret_cast<Handle>(handle);
      return true;
    }
    auto error = GetLastError();
    if (error != ERROR_SHARING_VIOLATION)
      return std::unexpected(StarException::format("Could not open lock file {}, error code {}", m_filename, error));
  } while (retry.wait());
  return false;
}

std::expected<void, StarException> LockFile::close_native(Handle handle) const {
  if (!CloseHandle(reinterpret_cast<HANDLE>(handle))) {
    auto error = GetLastError();
    return std::unexpected(StarException::format("Could not close lock file {}, error code {}", m_filename, error));
  }
  return {};
}
}
