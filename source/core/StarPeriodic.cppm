module;

import std;
import star.format;
import star.math_common;
import star.random;

namespace Star {

// Perform some action every X ticks.  Setting the tick count to 0 means never
// perform the action, 1 performs the action every call, 2 performs the action
// every other call, and so forth.
class Periodic {
public:
  Periodic()
    : Periodic(1) {}

  Periodic(unsigned everyXSteps)
    : m_counter(0), m_everyXSteps(everyXSteps) {}

  unsigned stepCount() const {
    return m_everyXSteps;
  }

  void setStepCount(unsigned everyXSteps) {
    m_everyXSteps = everyXSteps;
    if (everyXSteps != 0)
      m_counter = std::clamp<unsigned>(m_counter, 0, m_everyXSteps - 1);
    else
      m_counter = 0;
  }

  // Will the next tick() return true?
  bool ready() const {
    return m_everyXSteps != 0 && m_counter == 0;
  }

  bool tick() {
    if (m_everyXSteps == 0)
      return false;

    if (m_counter == 0) {
      m_counter = m_everyXSteps - 1;
      return true;
    } else {
      --m_counter;
      return false;
    }
  }

  template <typename Function>
  void tick(Function&& function) {
    if (tick())
      function();
  }

private:
  unsigned m_counter;
  unsigned m_everyXSteps;
};

// Perform some action with a given period over an amount of some value (like
// time) with optional randomness.
class RatePeriodic {
public:
  explicit RatePeriodic(RandomService& random)
    : m_random(random), m_period(1), m_noise(0), m_counter(1 + random.uniform_float() * 0), m_elapsed(0.0) {}

  [[nodiscard]] static std::expected<RatePeriodic, StarException> create(RandomService& random, double period, double noise = 0) {
    auto draw = random.uniform_float(-noise, noise);
    if (!draw)
      return std::unexpected(std::move(draw.error()));
    return RatePeriodic(random, period, noise, period + *draw);
  }

  double period() const {
    return m_period;
  }

  double noise() const {
    return m_noise;
  }

  template <typename Function>
  [[nodiscard]] std::expected<void, StarException> update(double amount, Function&& function) {
    double subAmount = std::min(amount, m_counter);
    m_counter -= subAmount;
    amount -= subAmount;
    m_elapsed += subAmount;

    if (m_counter <= 0.0) {
      auto draw = m_random.get().uniform_float(-m_noise, m_noise);
      if (!draw)
        return std::unexpected(std::move(draw.error()));
      m_counter = m_period + *draw;
      auto result = function(m_elapsed);
      if (!result)
        return std::unexpected(std::move(result.error()));
      m_elapsed = 0.0;
      if (amount > 0.0)
        return update(amount, std::forward<Function>(function));
    }
    return {};
  }

private:
  RatePeriodic(RandomService& random, double period, double noise, double counter)
    : m_random(random), m_period(period), m_noise(noise), m_counter(counter), m_elapsed(0.0) {}

  std::reference_wrapper<RandomService> m_random;
  double m_period;
  double m_noise;
  double m_counter;
  double m_elapsed;
};

}

export module star.periodic;

export namespace Star {
  using ::Star::Periodic;
  using ::Star::RatePeriodic;
}
