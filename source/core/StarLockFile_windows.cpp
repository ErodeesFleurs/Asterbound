#include <windows.h>
#include <stdio.h>
import std;
import star.format;
import star.string;
import star.lock_file;
import star.thread;
import star.string_windows;

namespace Star {

std::int64_t const LockFile::MaximumSleepMillis;

std::expected<std::optional<LockFile>, StarException> LockFile::acquireLock(String const& filename, std::int64_t timeout) {
  LockFile file(filename);
  auto locked = file.lock(timeout);
  if (!locked)
    return std::unexpected(std::move(locked.error()));
  if (*locked)
    return std::optional<LockFile>(std::move(file));
  return std::optional<LockFile>();
}

LockFile::LockFile(String const& filename) : m_filename(filename) {}

LockFile::LockFile(LockFile&& file)
  : m_filename(std::move(file.m_filename)), m_handle(std::move(file.m_handle)) {}

LockFile::~LockFile() {
  auto result = unlock();
  if (!result) {
    if (auto written = Star::print(stderr, "Error unlocking file during destruction: {}\n", result.error().what()); !written)
      std::_Exit(1);
  }
}

LockFile& LockFile::operator=(LockFile&& file) {
  if (this != &file) {
    auto released = unlock();
    if (!released) {
      if (auto written = Star::print(stderr, "Error unlocking file during move assignment: {}\n", released.error().what()); !written)
        std::_Exit(1);
    }
    m_filename = std::move(file.m_filename);
    m_handle = std::move(file.m_handle);
  }
  return *this;
}

std::expected<bool, StarException> LockFile::lock(std::int64_t timeout) {
  if (m_handle)
    return true;
  auto path = stringToUtf16(m_filename);
  if (!path)
    return std::unexpected(std::move(path.error()));
  auto acquire = [&]() -> std::expected<bool, StarException> {
    auto handle = CreateFileW(path->get(), GENERIC_READ, 0, nullptr, OPEN_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
      auto error = GetLastError();
      if (error == ERROR_SHARING_VIOLATION)
        return false;
      return std::unexpected(StarException::format("Could not open lock file {}, error code {}", m_filename, error));
    }
    m_handle = std::make_shared<HANDLE>(handle);
    return true;
  };
  if (timeout == 0)
    return acquire();
  auto start = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  auto timeoutDuration = std::chrono::milliseconds(timeout);
  while (true) {
    auto acquired = acquire();
    if (!acquired)
      return std::unexpected(std::move(acquired.error()));
    if (*acquired)
      return true;
    if (timeout > 0 && std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) - start > timeoutDuration)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(
        timeout < 0 ? MaximumSleepMillis : std::min(timeout / 4, MaximumSleepMillis)));
  }
}

std::expected<void, StarException> LockFile::unlock() {
  auto handle = std::exchange(m_handle, {});
  if (handle && !CloseHandle(*static_cast<HANDLE*>(handle.get())))
    return std::unexpected(StarException::format("Could not close lock file {}, error code {}", m_filename, GetLastError()));
  return {};
}

bool LockFile::isLocked() const {
  return (bool)m_handle;
}

}
