module;

import std;
import star.format;

namespace Star {

struct MathExceptionTag {
  static constexpr std::string_view name() { return "MathException"; }
};
using MathException = StarError<MathExceptionTag, StarException>;

namespace Constants {
  inline double constexpr radians_to_degrees = 57.2957795130823208768;
  inline double constexpr degrees_to_radians = 1 / radians_to_degrees;
}

namespace MathDetail {
  // Unevaluated calls check implicit return initialization, including copy elision.
  template <typename T> void implicit_result(T) noexcept;
}

template <typename T>
concept finite_classifiable = std::same_as<T, float> || std::same_as<T, double>;

// Keep integer-only classification behind a reference/call boundary: inlining
// into a fast-math float-argument caller can fold even an exponent-mask test
// into "always finite".
template <finite_classifiable T>
[[gnu::noinline]] bool is_finite(T const& value) {
  static_assert(std::numeric_limits<T>::is_iec559);
  if constexpr (std::same_as<T, float>)
    return (std::bit_cast<std::uint32_t>(value) & 0x7f800000u) != 0x7f800000u;
  else
    return (std::bit_cast<std::uint64_t>(value) & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
}

// Finite inputs; epsilon_multiple scales relative machine epsilon, not ULPs.
template <std::floating_point T, std::floating_point U>
constexpr bool near_equal(T x, U y, unsigned epsilon_multiple = 1) {
  using Common = std::common_type_t<T, U>;
  Common left = x, right = y;
  return std::abs(left - right) <= std::numeric_limits<Common>::epsilon()
    * std::max(std::abs(left), std::abs(right)) * epsilon_multiple;
}

template <std::integral T, std::integral U>
constexpr bool near_equal(T x, U y, unsigned = 1) noexcept {
  // Promotion preserves bool/character values and gives cmp_equal supported types.
  return std::cmp_equal(+x, +y);
}

template <std::floating_point T>
constexpr bool near_zero(T x, unsigned normal_multiple = 2) {
  return std::abs(x) <= std::numeric_limits<T>::min() * normal_multiple;
}

template <std::integral T>
constexpr bool near_zero(T x) noexcept {
  return x == 0;
}

template <typename T>
concept square_operand = requires(T const& value) { MathDetail::implicit_result<T>(value * value); };

template <square_operand T>
constexpr T square(T const& value) noexcept(noexcept(MathDetail::implicit_result<T>(value * value))) {
  return value * value;
}

// Finite inputs; floor semantics give a nonnegative fraction for negatives too.
template <std::floating_point T>
constexpr T fractional_part(T value) {
  if (value == 0)
    return value;
  return value - std::floor(value);
}

template <typename T, typename U>
concept magnitude_clampable = requires(T const& value, U const& magnitude) {
  value > magnitude ? 0 : 0;
  value < -magnitude ? 0 : 0;
  MathDetail::implicit_result<T>(magnitude);
  MathDetail::implicit_result<T>(-magnitude);
  MathDetail::implicit_result<T>(value);
};

template <typename T, typename U> requires magnitude_clampable<T, U>
constexpr T clamp_magnitude(T const& value, U const& magnitude)
  noexcept(noexcept(value > magnitude ? 0 : 0) && noexcept(value < -magnitude ? 0 : 0)
    && noexcept(MathDetail::implicit_result<T>(magnitude))
    && noexcept(MathDetail::implicit_result<T>(-magnitude)) && noexcept(MathDetail::implicit_result<T>(value))) {
  if (value > magnitude)
    return magnitude;
  else if (value < -magnitude)
    return -magnitude;
  else
    return value;
}

template <typename T>
concept clampable = requires(T const& value, T const& bound) {
  value < bound ? 0 : 0;
  MathDetail::implicit_result<T>(value);
};

// Legacy policy for bounds that may be reversed: the upper bound wins.
// Use std::clamp at call sites with proven ordered bounds.
template <clampable T>
constexpr T clamp(T const value, T const min, T const max)
  noexcept(noexcept(value < min ? 0 : 0) && noexcept(MathDetail::implicit_result<T>(value))) {
  T const& bounded = value < min ? min : value;
  return max < bounded ? max : bounded;
}

// Positive modulus is a precondition. The result is in [0, modulus).
template <std::integral T>
constexpr T positive_mod(T value, T modulus) {
  if constexpr (Star::DebugEnabled)
    Star::star_assert_failed((modulus > 0));
  T remainder = value % modulus;
  return remainder < 0 ? remainder + modulus : remainder;
}

// Positive modulus wraps; zero preserves the input. Retain floor, not fmod.
template <std::floating_point T>
constexpr T positive_mod(T value, T modulus) {
  if (modulus == 0)
    return value;
  return value - modulus * std::floor(value / modulus);
}

// Smallest signed displacement a-b on a wrapping line. Exact half-period ties
// retain their sign. Reducing first keeps signed subtraction representable.
template <std::signed_integral T>
constexpr T wrap_difference(T a, T b, T size) {
  a = positive_mod(a, size);
  b = positive_mod(b, size);
  T difference = a - b;
  if (difference > size / 2)
    difference -= size;
  else if (difference < -size / 2)
    difference += size;
  return difference;
}

template <std::floating_point T>
constexpr T wrap_difference(T a, T b, T size) {
  a = positive_mod(a, size);
  b = positive_mod(b, size);
  T difference = a - b;
  if (difference > size / 2)
    difference -= size;
  else if (difference < -size / 2)
    difference += size;
  return difference;
}

// Finite angle/intermediates; reduce without adding pi, retaining tiny angles.
// The half-open interval is [-pi, pi): positive pi maps to negative pi.
template <std::floating_point T>
constexpr T normalize_angle(T angle) {
  // The pinned std module rounds pi_v<long double> through double.
  constexpr T pi = static_cast<T>(3.141592653589793238462643383279502884L);
  constexpr T period = T{2} * pi;
  // Most callers already hold a normalized angle or have crossed one turn.
  if (angle >= pi) {
    T shifted = angle - period;
    if (shifted < pi)
      return shifted;
  } else if (angle < -pi) {
    T shifted = angle + period;
    if (shifted >= -pi)
      return shifted;
  } else {
    return angle;
  }
  angle = std::fmod(angle, period);
  if (angle >= pi)
    angle -= period;
  else if (angle < -pi)
    angle += period;
  return angle;
}

template <std::floating_point T>
constexpr T angle_difference(T current, T target) {
  return normalize_angle(target - current);
}

template <typename T>
concept approachable = requires(T& value, T& other, T const& bound) {
  value < other ? 0 : 0;
  value > other ? 0 : 0;
  bound < bound ? 0 : 0;
  MathDetail::implicit_result<T>(value - other);
  MathDetail::implicit_result<T>(value + other);
  MathDetail::implicit_result<T>(bound);
};

// Rate is nonnegative. Each branch clamps its step without requiring assignment.
template <approachable T>
constexpr T approach(T goal, T current, T rate)
  noexcept(noexcept(goal < current ? 0 : 0) && noexcept(goal > current ? 0 : 0)
    && noexcept(std::as_const(goal) < std::as_const(current) ? 0 : 0)
    && noexcept(MathDetail::implicit_result<T>(current - rate))
    && noexcept(MathDetail::implicit_result<T>(current + rate))
    && noexcept(MathDetail::implicit_result<T>(std::as_const(current)))) {
  if (goal < current) {
    T const stepped = current - rate;
    return stepped < std::as_const(goal) ? goal : stepped;
  } else if (goal > current) {
    T const stepped = current + rate;
    return std::as_const(goal) < stepped ? goal : stepped;
  } else {
    return std::as_const(current);
  }
}

// Approach an angle along the closest absolute direction.
template <std::floating_point T>
constexpr T approach_angle(T goal, T current, T rate) {
  return normalize_angle(current + clamp_magnitude(angle_difference(current, goal), rate));
}

// Unchecked conversion requires finite, representable input; clamping saturates.
inline std::uint8_t float_to_byte(float value, bool do_clamp = false) {
  if (do_clamp)
    value = std::clamp(value, 0.0f, 1.0f);
  return static_cast<std::uint8_t>(value * 255.0f);
}

constexpr float byte_to_float(std::uint8_t value) noexcept {
  return value / 255.0f;
}

// Increment within inclusive bounds; outside the range, restart at min.
template <std::integral T>
constexpr T cycle_increment(T value, T min, T max) noexcept {
  return value < min || value >= max ? min : value + 1;
}

}

export module star.math_common;

export namespace Star {
  using ::Star::MathExceptionTag;
  using ::Star::MathException;
  using ::Star::finite_classifiable;
  using ::Star::square_operand;
  using ::Star::magnitude_clampable;
  using ::Star::clampable;
  using ::Star::approachable;
  using ::Star::is_finite;
  using ::Star::near_equal;
  using ::Star::near_zero;
  using ::Star::square;
  using ::Star::fractional_part;
  using ::Star::clamp_magnitude;
  using ::Star::clamp;
  using ::Star::positive_mod;
  using ::Star::wrap_difference;
  using ::Star::normalize_angle;
  using ::Star::angle_difference;
  using ::Star::approach;
  using ::Star::approach_angle;
  using ::Star::float_to_byte;
  using ::Star::byte_to_float;
  using ::Star::cycle_increment;
}

export namespace Star::Constants {
  using ::Star::Constants::radians_to_degrees;
  using ::Star::Constants::degrees_to_radians;
}
