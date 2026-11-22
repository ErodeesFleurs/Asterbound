import std;
import star.string;
import star.format;
import star.time;
import star.thread;

namespace Star {
namespace Time {

// Implemented by the platform adapter; calendar conversion can fail.
std::expected<std::tm, StarException> local_calendar_time(std::time_t time);

std::expected<std::chrono::local_time<std::chrono::milliseconds>, StarException>
local_time(std::chrono::system_clock::time_point time) {
  auto whole_seconds = std::chrono::floor<std::chrono::seconds>(time);
  auto calendar = local_calendar_time(std::chrono::system_clock::to_time_t(whole_seconds));
  if (!calendar)
    return std::unexpected(std::move(calendar.error()));
  auto date = std::chrono::year(calendar->tm_year + 1900)
      / std::chrono::month(calendar->tm_mon + 1) / std::chrono::day(calendar->tm_mday);
  return std::chrono::local_days(date)
      + std::chrono::hours(calendar->tm_hour)
      + std::chrono::minutes(calendar->tm_min)
      + std::chrono::seconds(calendar->tm_sec)
      + std::chrono::duration_cast<std::chrono::milliseconds>(time - whole_seconds);
}

double time_since_epoch() {
  return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::int64_t milliseconds_since_epoch() {
  auto elapsed = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed + std::chrono::microseconds(500)).count();
}

double monotonic_time() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::int64_t monotonic_milliseconds() {
  auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed + std::chrono::microseconds(500)).count();
}


String print_duration(double seconds) {
  auto bits = std::bit_cast<std::uint64_t>(seconds);
  auto magnitude = bits & 0x7fffffffffffffffULL;
  // Compare the representation: fast-math may assume floating operands are finite.
  if (magnitude >= std::bit_cast<std::uint64_t>(0x1p64 / 1000.0))
    return String("duration out of range");
  auto total = static_cast<std::uint64_t>(std::round(std::bit_cast<double>(magnitude) * 1000));
  std::string result;
  if ((bits >> 63) && total) result += '-';
  auto append = [&](std::uint64_t value, std::string_view unit) {
    std::format_to(std::back_inserter(result), "{} {}{}", value, unit, value == 1 ? "" : "s");
  };
  if (total >= 3600000) { append(total / 3600000, "hour"); result += ", "; }
  if (total >= 60000) { append(total / 60000 % 60, "minute"); result += ", "; }
  if (total >= 1000) { append(total / 1000 % 60, "second"); result += ", "; }
  append(total % 1000, "millisecond");
  return String(std::move(result));
}


}

Clock::Clock(bool start) : Clock(Duration::zero(), start) {}
Clock::Clock(Duration initial, bool start) : elapsed(initial) {
  if (start) started_at = std::chrono::steady_clock::now();
}
std::unique_lock<std::mutex> Clock::acquire() const {
  std::unique_lock lock(mutex, std::defer_lock);
  lock_with_diagnostics(lock);
  return lock;
}
Clock::Clock(Clock const& other) {
  auto lock = other.acquire();
  elapsed = other.elapsed;
  started_at = other.started_at;
}
Clock& Clock::operator=(Clock const& other) {
  if (this != &other) {
    std::scoped_lock lock(mutex, other.mutex);
    elapsed = other.elapsed;
    started_at = other.started_at;
  }
  return *this;
}
Clock::Duration Clock::current_elapsed() const {
  return started_at ? elapsed + (std::chrono::steady_clock::now() - *started_at) : elapsed;
}
void Clock::set_elapsed(Duration value) {
  elapsed = value;
  if (started_at) started_at = std::chrono::steady_clock::now();
}
Clock::Duration Clock::settle() {
  if (started_at) {
    auto now = std::chrono::steady_clock::now();
    elapsed += now - *started_at;
    started_at = now;
  }
  return elapsed;
}
void Clock::restart_elapsed(Duration value) {
  auto lock = acquire();
  elapsed = value;
  started_at = std::chrono::steady_clock::now();
}
void Clock::reset() { set_milliseconds(0); }
void Clock::stop() {
  auto lock = acquire();
  elapsed = current_elapsed();
  started_at.reset();
}
void Clock::start() {
  auto lock = acquire();
  if (!started_at) started_at = std::chrono::steady_clock::now();
}
bool Clock::running() const {
  auto lock = acquire();
  return started_at.has_value();
}
double Clock::time() const {
  auto lock = acquire();
  return std::chrono::duration<double>(current_elapsed()).count();
}
static std::int64_t rounded_milliseconds(std::chrono::steady_clock::duration value) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(value + std::chrono::microseconds(500)).count();
}
std::int64_t Clock::milliseconds() const {
  auto lock = acquire();
  return rounded_milliseconds(current_elapsed());
}
static std::chrono::steady_clock::duration clock_duration(double seconds) {
  using Duration = std::chrono::steady_clock::duration;
  auto ticks = std::chrono::duration<double, Duration::period>(std::chrono::duration<double>(seconds));
  return Duration(static_cast<Duration::rep>(std::round(ticks.count())));
}
void Clock::set_time(double seconds) {
  auto lock = acquire();
  set_elapsed(clock_duration(seconds));
}
void Clock::set_milliseconds(std::int64_t millis) {
  auto lock = acquire();
  set_elapsed(std::chrono::milliseconds(millis));
}
void Clock::adjust_time(double delta) {
  auto lock = acquire();
  elapsed = clock_duration(std::max(0.0, std::chrono::duration<double>(settle()).count() + delta));
}
void Clock::adjust_milliseconds(std::int64_t delta) {
  auto lock = acquire();
  elapsed = std::chrono::milliseconds(rounded_milliseconds(settle()) + delta);
}
Timer Timer::with_time(double seconds, bool start) {
  return Timer(clock_duration(-seconds), start);
}
Timer Timer::with_milliseconds(std::int64_t millis, bool start) {
  return Timer(std::chrono::milliseconds(-millis), start);
}
Timer::Timer() : Clock(false) {}
Timer::Timer(Duration initial, bool start) : Clock(initial, start) {}
void Timer::restart(double seconds) { restart_elapsed(clock_duration(-seconds)); }
void Timer::restart_with_milliseconds(std::int64_t millis) { restart_elapsed(std::chrono::milliseconds(-millis)); }

double Timer::time_left(bool negative) const {
  double time_left = -Clock::time();
  if (!negative)
    time_left = std::max(0.0, time_left);
  return time_left;
}

std::int64_t Timer::milliseconds_left(bool negative) const {
  auto left = -Clock::milliseconds();
  return negative ? left : std::max<std::int64_t>(0, left);
}

bool Timer::time_up() const {
  auto lock = acquire();
  return current_elapsed() >= Duration::zero();
}

}
