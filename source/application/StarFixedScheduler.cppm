module;
import std;
import star.math_common;

export module star.fixed_scheduler;

export namespace Star {

// Owner-thread deadline bookkeeping only; callers supply a monotonic clock in
// seconds. No sleeping, application callbacks, or wall-clock reads live here.
class FixedScheduler {
public:
  static constexpr double MaximumDebt = 0.25;
  static constexpr double MaximumSleep = 0.05;

  explicit FixedScheduler(double updateRate = 60.0, double renderRate = 0.0)
    : m_updatePeriod(1.0 / updateRate), m_renderPeriod(renderRate > 0.0 ? 1.0 / renderRate : 0.0) {}

  // Start with no logic debt and an immediately eligible render. There is no
  // previous simulation sample until the first update, so initial alpha is 1.
  void reset(double now) {
    m_lastObserved = now;
    m_nextUpdate = now + m_updatePeriod;
    m_nextRender = now;
    ++m_updateEpoch;
    m_hasUpdated = false;
    m_initialized = true;
    ++m_epoch;
  }

  void setUpdateRate(double rate, double now) {
    if (!isFinite(rate) || rate <= 0.0)
      return;
    double period = 1.0 / rate;
    if (period != m_updatePeriod) {
      m_updatePeriod = period;
      reset(now);
    }
  }

  void setRenderRate(double rate, double now) {
    if (!isFinite(rate) || rate < 0.0)
      return;
    double period = rate > 0.0 ? 1.0 / rate : 0.0;
    if (period != m_renderPeriod) {
      observe(now);
      m_renderPeriod = period;
      m_nextRender = now;
      ++m_epoch;
    }
  }

  // Reserve 0..maxFrameSkip+1 fixed updates. If the bounded batch cannot pay
  // the debt, discard whole overdue ticks while retaining the fractional phase
  // and invalidate presentation history. Never turn a stall into unbounded work.
  unsigned updatesDue(double now, unsigned maxFrameSkip) {
    observe(now);
    if (now < m_nextUpdate)
      return 0;
    double due = std::floor((now - m_nextUpdate) / m_updatePeriod) + 1.0;
    unsigned limit = maxFrameSkip == std::numeric_limits<unsigned>::max() ? maxFrameSkip : maxFrameSkip + 1;
    unsigned count = static_cast<unsigned>(std::min(due, static_cast<double>(limit)));
    m_nextUpdate += due * m_updatePeriod;
    m_hasUpdated = true;
    if (due > count)
      ++m_epoch;
    return count;
  }

  // Consume one render deadline at frame START, not after a potentially
  // blocking present. Missed render slots are skipped rather than caught up.
  bool beginRender(double now) {
    observe(now);
    if (m_renderPeriod > 0.0 && now < m_nextRender)
      return false;
    if (m_renderPeriod > 0.0)
      m_nextRender += (std::floor((now - m_nextRender) / m_renderPeriod) + 1.0) * m_renderPeriod;
    return true;
  }

  float interpolation(double now) const {
    if (!m_hasUpdated)
      return 1.0f;
    return static_cast<float>(std::clamp(1.0 - (m_nextUpdate - now) / m_updatePeriod, 0.0, 1.0));
  }

  // Re-read the clock after present. If present already crossed either
  // deadline this returns zero, so VSync cannot incur a second frame delay.
  // Bound idle waits so events, audio errors, and quit remain serviced even
  // when both configured rates are low.
  double sleepTime(double now) {
    observe(now);
    double next = m_renderPeriod > 0.0 ? std::min(m_nextUpdate, m_nextRender) : now;
    return std::clamp(next - now, 0.0, MaximumSleep);
  }

  // While a sealed frame is being presented, only simulation can become due.
  // An unlimited render rate must not turn that backpressure into a busy loop.
  double updateSleepTime(double now) {
    observe(now);
    return std::clamp(m_nextUpdate - now, 0.0, MaximumSleep);
  }

  std::uint64_t epoch() const { return m_epoch; }
  std::uint64_t updateEpoch() const { return m_updateEpoch; }

private:
  void observe(double now) {
    if (!m_initialized) {
      reset(now);
    } else if (now < m_lastObserved || now - m_lastObserved > MaximumDebt
        || now - m_nextUpdate > MaximumDebt) {
      reset(now);
      // Retain one due tick: a sequence of slow presents must not repeatedly
      // postpone the next update and starve simulation/input-driven menus.
      m_nextUpdate = now;
    }
    m_lastObserved = now;
  }

  double m_updatePeriod;
  double m_renderPeriod;
  double m_lastObserved = 0.0;
  double m_nextUpdate = 0.0;
  double m_nextRender = 0.0;
  std::uint64_t m_epoch = 0;
  std::uint64_t m_updateEpoch = 0;
  bool m_initialized = false;
  bool m_hasUpdated = false;
};

}
