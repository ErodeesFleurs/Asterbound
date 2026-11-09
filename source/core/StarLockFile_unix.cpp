#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
import std;
import star.format;
import star.string;
import star.lock_file;
import star.thread;

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
  auto acquire = [this](bool block) -> std::expected<bool, StarException> {
    int fd = ::open(m_filename.utf8_data(), O_RDONLY | O_CREAT, 0644);
    if (fd < 0)
      return std::unexpected(StarException::format("Could not open lock file {}, {}", m_filename, std::strerror(errno)));
    if (::flock(fd, LOCK_EX | (block ? 0 : LOCK_NB)) != 0) {
      int error = errno;
      if (::close(fd) < 0) {
        auto closeError = errno;
        if (error == EWOULDBLOCK)
          return std::unexpected(StarException::format("Could not close unavailable lock file {}, {}", m_filename, std::strerror(closeError)));
        if (auto written = Star::print(stderr, "Error closing failed lock acquisition: {}\n", std::strerror(closeError)); !written)
          return std::unexpected(StarException::format("Could not lock file {}, {}", m_filename, std::strerror(error)).with_context(
              Star::format("Could not report close failure ({}): {}", std::strerror(closeError), written.error().what())));
      }
      if (error == EWOULDBLOCK)
        return false;
      return std::unexpected(StarException::format("Could not lock file {}, {}", m_filename, std::strerror(error)));
    }
    m_handle = std::make_shared<int>(fd);
    return true;
  };
  if (timeout <= 0)
    return acquire(timeout < 0);
  auto start = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  auto timeoutDuration = std::chrono::milliseconds(timeout);
  while (true) {
    auto acquired = acquire(false);
    if (!acquired)
      return std::unexpected(std::move(acquired.error()));
    if (*acquired)
      return true;
    if (std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) - start > timeoutDuration)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(std::min(timeout / 4, MaximumSleepMillis)));
  }
}

std::expected<void, StarException> LockFile::unlock() {
  auto handle = std::exchange(m_handle, {});
  if (!handle)
    return {};
  int fd = *static_cast<int*>(handle.get());
  int unlinkError = ::unlink(m_filename.utf8_data()) < 0 ? errno : 0;
  int closeError = ::close(fd) < 0 ? errno : 0;
  if (unlinkError) {
    if (closeError) {
      if (auto written = Star::print(stderr, "Error closing lock file after unlink failure: {}\n", std::strerror(closeError)); !written)
        return std::unexpected(StarException::format("Could not unlink lock file {}, {}", m_filename, std::strerror(unlinkError)).with_context(
            Star::format("Could not report close failure ({}): {}", std::strerror(closeError), written.error().what())));
    }
    return std::unexpected(StarException::format("Could not unlink lock file {}, {}", m_filename, std::strerror(unlinkError)));
  }
  if (closeError)
    return std::unexpected(StarException::format("Could not close lock file {}, {}", m_filename, std::strerror(closeError)));
  return {};
}

bool LockFile::isLocked() const {
  return (bool)m_handle;
}

}
