module;
import std;
export module star.tick_rate_monitor;

export namespace Star {

// Positive, finite binary64 even under the production fast-math flags.
[[nodiscard]] inline bool valid_tick_interval(double value) noexcept {
  volatile std::uint64_t representation = std::bit_cast<std::uint64_t>(value);
  auto bits = representation;
  return bits != 0 && bits < 0x7ff0000000000000ULL;
}
class TickRateMonitor {
  friend class TickRateApproacher;
  using Seconds = std::chrono::duration<double>;
  using TimePoint = std::chrono::time_point<std::chrono::steady_clock, Seconds>;
  static TimePoint now() noexcept { return std::chrono::time_point_cast<Seconds>(std::chrono::steady_clock::now()); }
  double window_seconds;
  TimePoint last_tick;
  double ticks = 0;

  double remaining(TimePoint time) const noexcept {
    if (time == last_tick) return ticks;
    if (time < last_tick) return 0;
    return std::max(0.0, ticks - (time - last_tick).count() * (ticks / window_seconds));
  }
public:
  explicit TickRateMonitor(double window) noexcept : window_seconds(window), last_tick(now()) {}
  [[nodiscard]] double window() const noexcept { return window_seconds; }
  void reset() noexcept { ticks = 0; last_tick = now(); }
  double tick(unsigned count = 1, TimePoint time = now()) noexcept {
    ticks = remaining(time) + count;
    last_tick = time;
    return ticks / window_seconds;
  }
  [[nodiscard]] double rate(TimePoint time = now()) const noexcept { return remaining(time) / window_seconds; }
};

class TickRateApproacher {
  TickRateMonitor monitor;
  double target_hz;
public:
  TickRateApproacher(double target_rate, double window) noexcept : monitor(window), target_hz(target_rate) {}
  [[nodiscard]] double window() const noexcept { return monitor.window(); }
  void set_window(double seconds) noexcept {
    if (seconds == window()) return;
    monitor.window_seconds = seconds;
    reset();
  }
  [[nodiscard]] double target_rate() const noexcept { return target_hz; }
  void set_target_rate(double rate) noexcept { target_hz = rate; }
  void reset() noexcept {
    monitor.last_tick = TickRateMonitor::now();
    monitor.ticks = target_hz * window();
  }
  double tick(unsigned count = 1) noexcept { return monitor.tick(count); }
  [[nodiscard]] double rate() const noexcept { return monitor.rate(); }
  [[nodiscard]] double spare_time(double sampled_rate) const noexcept { return (sampled_rate - target_hz) * window() / target_hz; }
  [[nodiscard]] double spare_time() const noexcept { return spare_time(rate()); }
};
}
