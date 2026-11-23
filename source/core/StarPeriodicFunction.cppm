module;

import std;
import star.format;
import star.math_common;
import star.random;

namespace Star {

namespace PeriodicFunctionDetail {
  template <typename T> void implicit_result(T);

  template <typename Operator, typename Float>
  concept weight_operator = requires(Operator& op, Float const& time, std::invoke_result_t<Operator&, Float>& weights) {
    op(Float(time));
    implicit_result<Float>(time * weights[0] + time * weights[1]);
  };
}

// Repeating, periodic function with optional period and magnitude variance.
// Each cycle of the function will randomize the min and max values of the
// function by the magnitude variance, and the period by the period variance.
// Can approximate a randomized sin wave, triangle wave, square wave, etc based
// on the weight operator provided to the value method.
template <finite_classifiable Float>
class PeriodicFunction {
public:
  explicit PeriodicFunction(RandomService& random)
    : PeriodicFunction(random, HalfCycle{Float{0.5}, 0}, 0, 1, 0) {}

  // Keep unchecked floating-point inputs behind references: the optimized
  // by-value ABI may assume they are finite before validation runs.
  [[nodiscard]] static std::expected<PeriodicFunction, StarException> create(RandomService& random, Float const& period,
      Float const& min = 0, Float const& max = 1, Float const& period_variance = 0, Float const& magnitude_variance = 0);

  void update(Float delta) {
    m_timer -= delta;

    // Advance only once, even when delta spans multiple cycles.
    if (m_timer <= 0) {
      m_source = m_target;
      m_target = sample(m_target_mode ? m_max : m_min, m_magnitude_variance);
      m_target_mode = !m_target_mode;
      // Stock flickering lights may have variance larger than the period.
      m_timer_max = std::max(Float{0}, sample(m_half_cycle.period, m_half_cycle.variance));
      m_timer = std::max(Float{0}, m_timer + m_timer_max);
    }
  }

  template <typename WeightOperator> requires PeriodicFunctionDetail::weight_operator<WeightOperator, Float>
  Float value(WeightOperator&& weight_operator) const {
    // Time counts down from the full sampled duration to zero.
    // A sampled zero-duration half-cycle is already at its target.
    auto&& weights = weight_operator(m_timer_max == 0 ? Float{0} : m_timer / m_timer_max);
    return m_target * weights[0] + m_source * weights[1];
  }

private:
  struct HalfCycle {
    Float period;
    Float variance;
  };

  PeriodicFunction(RandomService& random, HalfCycle half_cycle, Float min, Float max, Float magnitude_variance)
    : m_random(random), m_half_cycle(half_cycle), m_min(min), m_max(max), m_magnitude_variance(magnitude_variance),
      m_timer_max(half_cycle.period), m_source(sample(max, magnitude_variance)), m_target(sample(min, magnitude_variance)) {}

  Float sample(Float center, Float variance) {
#ifdef __clang__
#pragma clang fp reassociate(off)
#endif
    return center + (m_random.get().uniform_float() * 2 - 1) * variance;
  }

  std::reference_wrapper<RandomService> m_random;
  HalfCycle m_half_cycle;
  Float m_min;
  Float m_max;
  Float m_magnitude_variance;
  Float m_timer_max;
  Float m_timer = 0;
  Float m_source;
  Float m_target;
  bool m_target_mode = true;
};

// Check the rounded half-period and the largest reachable sample before drawing.
#pragma float_control(precise, on, push)
template <finite_classifiable Float>
std::expected<PeriodicFunction<Float>, StarException> PeriodicFunction<Float>::create(
    RandomService& random, Float const& period, Float const& min, Float const& max, Float const& period_variance, Float const& magnitude_variance) {
  auto finite = classify_finite(period, min, max, period_variance, magnitude_variance);
  if (!std::ranges::all_of(finite, std::identity{}))
    return std::unexpected(MathException("Periodic function parameters must be finite"));
  if (period <= 0 || period_variance < 0 || magnitude_variance < 0)
    return std::unexpected(MathException("Periodic function requires a positive period and nonnegative variances"));

  HalfCycle half_cycle{period / 2, period_variance / 2};
  // The named min/max are alternating phase endpoints; stock lights can run
  // from bright to dim. Order them only for the sample overflow check.
  auto [lower, upper] = std::minmax(min, max);
  auto bounds = classify_finite(half_cycle.period + half_cycle.variance, lower - magnitude_variance, upper + magnitude_variance);
  if (half_cycle.period <= 0 || !std::ranges::all_of(bounds, std::identity{}))
    return std::unexpected(MathException("Periodic function requires a representable half-period and finite sample bounds"));
  return PeriodicFunction(random, half_cycle, min, max, magnitude_variance);
}
#pragma float_control(pop)

}

export module star.periodic_function;

export namespace Star {
  using ::Star::PeriodicFunction;
}
