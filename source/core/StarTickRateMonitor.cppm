module;
import std;

export module star.tick_rate_monitor;

export namespace Star {

// Monitors the rate at which 'tick()' is called in wall-clock seconds.
class TickRateMonitor {
public:
  // 'window' controls the dropoff at which 'rate' will approach zero if tick
  // is not called, measured in seconds.
  TickRateMonitor(double window);

  double window() const;

  // Resets to a zero tick-rate state
  void reset();

  // Ticks the given number of times, returns the current rate.
  double tick(unsigned count = 1);

  // Returns the rate as of the *current* time, not the time of the last tick.
  double rate() const;

private:
  std::chrono::duration<double> m_window;
  // Retain absolute double-seconds quantization before subtracting tick times.
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> m_lastTick;
  double m_ticks;
};

// Helps tick at as close as possible to a given tick rate
class TickRateApproacher {
public:
  TickRateApproacher(double targetTickRate, double window);

  // The TickRateMonitor window influences how long the TickRateApproacher will
  // try and speed up or slow down the tick rate to match the target tick rate.
  // It should be chosen so that it is not so short that the actual target rate
  // drifts, but not too long so that the rate returns to normal quickly enough
  // with outliers.
  double window() const;
  // Setting the window to a new value will reset the TickRateApproacher
  void setWindow(double window);

  double targetTickRate() const;
  void setTargetTickRate(double targetTickRate);

  // Resets such that the current tick rate is assumed to be perfectly at the
  // target.
  void reset();

  double tick(unsigned count = 1);
  double rate() const;

  // How many ticks we currently should perform, so that if each tick happened
  // instantly, we would be as close to the target tick rate as possible.  If
  // we are ahead, may be negative.
  double ticksBehind();

  // The negative of ticksBehind, is positive for how many ticks ahead we
  // currently are.
  double ticksAhead();

  // How much spare time we have until the tick rate will begin to be behind
  // the target tick rate.
  double spareTime();

private:
  TickRateMonitor m_tickRateMonitor;
  double m_targetTickRate;
};

}

namespace Star {

TickRateMonitor::TickRateMonitor(double window) : m_window(window) {
  reset();
}

double TickRateMonitor::window() const {
  return m_window.count();
}

void TickRateMonitor::reset() {
  m_lastTick = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()) - m_window;
  m_ticks = 0;
}

double TickRateMonitor::tick(unsigned count) {
  auto currentTime = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());

  if (m_lastTick > currentTime) {
    m_lastTick = currentTime - m_window;
    m_ticks = 0;
  } else if (m_lastTick < currentTime) {
    auto timePast = currentTime - m_lastTick;
    double rate = m_ticks / m_window.count();
    m_ticks = std::max(0.0, m_ticks - timePast.count() * rate);
    m_lastTick = currentTime;
  }

  m_ticks += count;

  return m_ticks / m_window.count();
}

double TickRateMonitor::rate() const {
  return TickRateMonitor(*this).tick(0);
}

TickRateApproacher::TickRateApproacher(double targetTickRate, double window)
  : m_tickRateMonitor(window), m_targetTickRate(targetTickRate) {}

double TickRateApproacher::window() const {
  return m_tickRateMonitor.window();
}

void TickRateApproacher::setWindow(double window) {
  if (window != m_tickRateMonitor.window()) {
    m_tickRateMonitor = TickRateMonitor(window);
    tick(m_targetTickRate * window);
  }
}

double TickRateApproacher::targetTickRate() const {
  return m_targetTickRate;
}

void TickRateApproacher::setTargetTickRate(double targetTickRate) {
  m_targetTickRate = targetTickRate;
}

void TickRateApproacher::reset() {
  setWindow(window());
}

double TickRateApproacher::tick(unsigned count) {
  return m_tickRateMonitor.tick(count);
}

double TickRateApproacher::rate() const {
  return m_tickRateMonitor.rate();
}

double TickRateApproacher::ticksBehind() {
  return (m_targetTickRate - m_tickRateMonitor.rate()) * window();
}

double TickRateApproacher::ticksAhead() {
  return -ticksBehind();
}

double TickRateApproacher::spareTime() {
  return ticksAhead() / m_targetTickRate;
}

}
