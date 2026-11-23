#include <stdio.h>
import std;
import star.format;
import star.string;
import star.lock_file;

namespace Star {
namespace {
void report_unlock_error(std::expected<void, StarException> result, std::string_view operation) {
  if (!result) {
    if (auto written = Star::print(*stderr, "Error unlocking file during {}: {}\n", operation, result.error().what()); !written)
      std::_Exit(1);
  }
}
}

LockFile::LockFile(String filename) : m_filename(std::move(filename)) {}

LockFile::LockFile(LockFile&& other) noexcept
  : m_filename(std::move(other.m_filename)), m_handle(std::exchange(other.m_handle, invalid_handle)) {}

LockFile& LockFile::operator=(LockFile&& other) noexcept {
  if (this != std::addressof(other)) {
    report_unlock_error(unlock(), "move assignment");
    m_filename = std::move(other.m_filename);
    m_handle = std::exchange(other.m_handle, invalid_handle);
  }
  return *this;
}

LockFile::~LockFile() {
  report_unlock_error(unlock(), "destruction");
}

std::expected<std::optional<LockFile>, StarException> LockFile::try_acquire(String filename, std::int64_t timeout_ms) {
  LockFile file(std::move(filename));
  auto acquired = file.lock(timeout_ms);
  if (!acquired)
    return std::unexpected(std::move(acquired.error()));
  if (!*acquired)
    return std::nullopt;
  return std::optional(std::move(file));
}

std::expected<void, StarException> LockFile::unlock() {
  if (!owns_lock())
    return {};
  return close_native(std::exchange(m_handle, invalid_handle));
}

LockFile::RetryWindow::RetryWindow(std::int64_t timeout_ms) : m_deadline(Clock::time_point::max()) {
  using namespace std::chrono;
  constexpr auto max_interval = milliseconds(25);
  if (timeout_ms <= 0) {
    m_interval = timeout_ms == 0 ? milliseconds::zero() : max_interval;
    return;
  }
  auto now = Clock::now();
  auto budget = milliseconds(timeout_ms);
  if (budget < floor<milliseconds>(m_deadline - now))
    m_deadline = now + budget;
  m_interval = std::clamp(budget / 4, milliseconds(1), max_interval);
}

bool LockFile::RetryWindow::wait() const {
  if (m_interval.count() == 0)
    return false;
  auto now = Clock::now();
  if (now >= m_deadline)
    return false;
  std::this_thread::sleep_for(std::min<Clock::duration>(m_interval, m_deadline - now));
  return Clock::now() < m_deadline;
}
}
