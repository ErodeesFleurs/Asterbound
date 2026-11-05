module;
import std;
import star.string;
import star.format;

namespace Star {

namespace Time {
  double timeSinceEpoch();
  std::int64_t millisecondsSinceEpoch();

  double monotonicTime();
  std::int64_t monotonicMilliseconds();

  // Pretty print a duration of time (In days, hours, minutes, seconds, and milliseconds)
  String printDuration(double time);

  // Convert a system timestamp to local civil time without a tzdb dependency.
  [[nodiscard]] std::expected<std::chrono::local_time<std::chrono::milliseconds>, StarException>
  localTime(std::chrono::system_clock::time_point time);
}

// Keeps track of elapsed real time since a given moment.  Guaranteed
// monotonically increasing and thread safe.
class Clock {
public:
  explicit Clock();
  explicit Clock(bool start);

  Clock(Clock const& clock);

  Clock& operator=(Clock const& clock);

  // Resets clock to 0 time
  void reset();

  void stop();
  void start();

  bool running() const;

  double time() const;
  std::int64_t milliseconds() const;

  // Override actual elapsed time with the given time.
  void setTime(double time);
  void setMilliseconds(std::int64_t millis);

  // Warp the clock backwards or forwards
  void adjustTime(double timeAdjustment);
  void adjustMilliseconds(std::int64_t millisAdjustment);

private:
  void updateElapsed() const;

  mutable std::recursive_mutex m_mutex;
  mutable std::chrono::steady_clock::duration m_elapsed;
  mutable std::optional<std::chrono::steady_clock::time_point> m_lastUpdate;
  bool m_running;
};

// An instance of Clock that counts down a given amount of time
class Timer : private Clock {
public:
  static Timer withTime(double timeLeft, bool start = true);
  static Timer withMilliseconds(std::int64_t millis, bool start = true);

  // Constructs a stopped timer whose time is up.
  Timer();
  Timer(Timer const& timer);
  Timer& operator=(Timer const& timer);

  // Start the timer with the given time left.
  void restart(double timeLeft);
  void restartWithMilliseconds(std::int64_t millisecondsLeft);

  // Time remaining on the timer.  If negative is true, will return negative
  // time values after the timer is up, if false it stops at zero.
  double timeLeft(bool negative = false) const;
  std::int64_t millisecondsLeft(bool negative = false) const;

  // Is the time remaining <= 0.0?
  bool timeUp() const;

  using Clock::stop;
  using Clock::start;
  using Clock::running;
};

}

export module star.time;

export namespace Star {
  using ::Star::Clock;
  using ::Star::Timer;
}

export namespace Star::Time {
  using ::Star::Time::timeSinceEpoch;
  using ::Star::Time::millisecondsSinceEpoch;
  using ::Star::Time::monotonicTime;
  using ::Star::Time::monotonicMilliseconds;
  using ::Star::Time::printDuration;
  using ::Star::Time::localTime;
}
