#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
import std;
import star.format;
import star.string;
import star.lock_file;

namespace Star {

std::expected<bool, StarException> LockFile::lock(std::int64_t timeout_ms) {
  if (owns_lock())
    return true;
  RetryWindow retry(timeout_ms);
  int fd;
  do {
    fd = ::open(m_filename.utf8_data(), O_RDONLY | O_CREAT | O_CLOEXEC, 0644);
  } while (fd < 0 && errno == EINTR && timeout_ms < 0);
  if (fd < 0)
    return std::unexpected(StarException::format("Could not open lock file {}, {}", m_filename, std::strerror(errno)));

  int flags = LOCK_EX | (timeout_ms < 0 ? 0 : LOCK_NB);
  int error;
  for (;;) {
    if (::flock(fd, flags) == 0) {
      m_handle = fd;
      return true;
    }
    error = errno;
    if (error == EINTR && timeout_ms < 0)
      continue;
    if (error != EWOULDBLOCK || !retry.wait())
      break;
  }
  auto closed = close_native(fd);
  if (error != EWOULDBLOCK) {
    auto failure = StarException::format("Could not lock file {}, {}", m_filename, std::strerror(error));
    if (!closed)
      failure = std::move(failure).with_related(closed.error(), "Closing lock file after failed acquisition");
    return std::unexpected(std::move(failure));
  }
  if (!closed)
    return std::unexpected(std::move(closed.error()));
  return false;
}

std::expected<void, StarException> LockFile::close_native(Handle fd) const {
  // Never unlink: a contender may already have opened this inode.
  if (::close(fd) < 0)
    return std::unexpected(StarException::format("Could not close lock file {}, {}", m_filename, std::strerror(errno)));
  return {};
}

}
