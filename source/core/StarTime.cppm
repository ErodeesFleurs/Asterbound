module;
import std;
import star.string;
import star.format;

namespace Star {

namespace Time {
  double time_since_epoch();
  std::int64_t milliseconds_since_epoch();

  double monotonic_time();
  std::int64_t monotonic_milliseconds();

  // Signed duration rounded to whole milliseconds; hours do not wrap at 24.
  String print_duration(double time);

  // Convert a system timestamp to local civil time without a tzdb dependency.
  [[nodiscard]] std::expected<std::chrono::local_time<std::chrono::milliseconds>, StarException>
  local_time(std::chrono::system_clock::time_point time);
}

// Thread-safe elapsed steady time. Only reset, set and adjust can move it backwards.
class Clock {
public:
  explicit Clock(bool start = true);

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
  void set_time(double time);
  void set_milliseconds(std::int64_t millis);

  // Warp the clock backwards or forwards
  void adjust_time(double adjustment);
  void adjust_milliseconds(std::int64_t adjustment);

private:
  friend class Timer;
  using Duration = std::chrono::steady_clock::duration;
  Clock(Duration initial, bool start);
  [[nodiscard]] std::unique_lock<std::mutex> acquire() const;
  Duration current_elapsed() const;
  Duration settle();
  void set_elapsed(Duration value);
  void restart_elapsed(Duration value);
  mutable std::mutex mutex;
  Duration elapsed{};
  std::optional<std::chrono::steady_clock::time_point> started_at;
};

// An instance of Clock that counts down a given amount of time
class Timer : private Clock {
public:
  static Timer with_time(double time_left, bool start = true);
  static Timer with_milliseconds(std::int64_t millis, bool start = true);

  // Constructs a stopped timer whose time is up.
  Timer();
private:
  Timer(Duration initial, bool start);
public:

  // Start the timer with the given time left.
  void restart(double time_left);
  void restart_with_milliseconds(std::int64_t milliseconds_left);

  // Time remaining on the timer.  If negative is true, will return negative
  // time values after the timer is up, if false it stops at zero.
  double time_left(bool negative = false) const;
  std::int64_t milliseconds_left(bool negative = false) const;

  // Is the time remaining <= 0.0?
  bool time_up() const;

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
  using ::Star::Time::time_since_epoch;
  using ::Star::Time::milliseconds_since_epoch;
  using ::Star::Time::monotonic_time;
  using ::Star::Time::monotonic_milliseconds;
  using ::Star::Time::print_duration;
  using ::Star::Time::local_time;
}
