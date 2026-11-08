import std;
import star.string;
import star.format;
import star.time;
import star.thread;

namespace Star {
namespace Time {

// Implemented by the platform adapter; calendar conversion can fail.
std::expected<std::tm, StarException> localCalendarTime(std::time_t time);

std::expected<std::chrono::local_time<std::chrono::milliseconds>, StarException>
localTime(std::chrono::system_clock::time_point time) {
  auto wholeSeconds = std::chrono::floor<std::chrono::seconds>(time);
  auto calendar = localCalendarTime(std::chrono::system_clock::to_time_t(wholeSeconds));
  if (!calendar)
    return std::unexpected(std::move(calendar.error()));
  auto date = std::chrono::year(calendar->tm_year + 1900)
      / std::chrono::month(calendar->tm_mon + 1) / std::chrono::day(calendar->tm_mday);
  return std::chrono::local_days(date)
      + std::chrono::hours(calendar->tm_hour)
      + std::chrono::minutes(calendar->tm_min)
      + std::chrono::seconds(calendar->tm_sec)
      + std::chrono::duration_cast<std::chrono::milliseconds>(time - wholeSeconds);
}

double timeSinceEpoch() {
  return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::int64_t millisecondsSinceEpoch() {
  auto elapsed = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed + std::chrono::microseconds(500)).count();
}

double monotonicTime() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::int64_t monotonicMilliseconds() {
  auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed + std::chrono::microseconds(500)).count();
}


String printDuration(double time) {
  String hours;
  String minutes;
  String seconds;
  String milliseconds;

  if (time >= 3600) {
    int numHours = static_cast<int>(time) / 3600;
    hours = Star::format("{} hour{}", numHours, numHours == 1 ? "" : "s");
  }
  if (time >= 60) {
    int numMinutes = static_cast<int>((time / 60)) % 60;
    minutes = Star::format("{} minute{}", numMinutes, numMinutes == 1 ? "" : "s");
  }
  if (time >= 1) {
    int numSeconds = static_cast<int>(time) % 60;
    seconds = Star::format("{} second{}", numSeconds, numSeconds == 1 ? "" : "s");
  }

  int numMilliseconds = std::round(std::fmod(time, 1.0) * 1000);
  milliseconds = Star::format("{} millisecond{}", numMilliseconds, numMilliseconds == 1 ? "" : "s");

  String result;
  for (auto component : {std::cref(hours), std::cref(minutes), std::cref(seconds), std::cref(milliseconds)}) {
    if (component.get().empty())
      continue;
    if (!result.empty())
      result += ", ";
    result += component.get();
  }
  return result;
}


}

Clock::Clock() : Clock(true) {}

Clock::Clock(bool start) {
  m_elapsed = std::chrono::steady_clock::duration::zero();
  m_running = false;
  if (start)
    Clock::start();
}

Clock::Clock(Clock const& clock) {
  operator=(clock);
}

Clock& Clock::operator=(Clock const& clock) {
  m_elapsed = clock.m_elapsed;
  m_lastUpdate = clock.m_lastUpdate;
  m_running = clock.m_running;

  return *this;
}

void Clock::reset() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  updateElapsed();
  m_elapsed = std::chrono::steady_clock::duration::zero();
}

void Clock::stop() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_lastUpdate.reset();
  m_running = false;
}

void Clock::start() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_running = true;
  updateElapsed();
}

bool Clock::running() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_running;
}

double Clock::time() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  updateElapsed();
  return std::chrono::duration<double>(m_elapsed).count();
}

std::int64_t Clock::milliseconds() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  updateElapsed();
  return std::chrono::duration_cast<std::chrono::milliseconds>(m_elapsed + std::chrono::microseconds(500)).count();
}

void Clock::setTime(double time) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  updateElapsed();
  auto ticks = std::chrono::duration<double, std::chrono::steady_clock::period>(std::chrono::duration<double>(time));
  m_elapsed = std::chrono::steady_clock::duration(
      static_cast<std::chrono::steady_clock::rep>(std::round(ticks.count())));
}

void Clock::setMilliseconds(std::int64_t millis) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  updateElapsed();
  m_elapsed = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(millis));
}

void Clock::adjustTime(double timeAdjustment) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  setTime(std::max<double>(0.0, time() + timeAdjustment));
}

void Clock::adjustMilliseconds(std::int64_t millisAdjustment) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  setMilliseconds(milliseconds() + millisAdjustment);
}

void Clock::updateElapsed() const {
  if (!m_running)
    return;

  auto currentTime = std::chrono::steady_clock::now();

  if (m_lastUpdate)
    m_elapsed += currentTime - *m_lastUpdate;

  m_lastUpdate = currentTime;
}

Timer Timer::withTime(double timeLeft, bool start) {
  Timer timer;
  timer.setTime(-timeLeft);
  if (start)
    timer.start();
  return timer;
}

Timer Timer::withMilliseconds(std::int64_t millis, bool start) {
  Timer timer;
  timer.setMilliseconds(-millis);
  if (start)
    timer.start();
  return timer;
}

Timer::Timer() : Clock(false) {
  setTime(0.0);
}

Timer::Timer(Timer const& timer) = default;

Timer& Timer::operator=(Timer const& timer) = default;

void Timer::restart(double timeLeft) {
  Clock::setTime(-timeLeft);
  Clock::start();
}

void Timer::restartWithMilliseconds(std::int64_t millisecondsLeft) {
  Clock::setMilliseconds(-millisecondsLeft);
  Clock::start();
}

double Timer::timeLeft(bool negative) const {
  double timeLeft = -Clock::time();
  if (!negative)
    timeLeft = std::max(0.0, timeLeft);
  return timeLeft;
}

std::int64_t Timer::millisecondsLeft(bool negative) const {
  std::int64_t millisLeft = -Clock::milliseconds();
  if (!negative)
    millisLeft = std::max<std::int64_t>(0, millisLeft);
  return millisLeft;
}

bool Timer::timeUp() const {
  return Clock::time() >= 0.0;
}

}
