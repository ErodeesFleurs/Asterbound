module;

import std;
import star.format;

namespace Star {

struct MathExceptionTag {
  static constexpr std::string_view name() { return "MathException"; }
};
using MathException = StarError<MathExceptionTag, StarException>;

namespace Constants {
  inline double constexpr rad2deg = 57.2957795130823208768;
  inline double constexpr deg2rad = 1 / rad2deg;
}

// Keep integer-only classification behind a reference/call boundary: inlining
// into a fast-math float-argument caller can fold even an exponent-mask test
// into "always finite".
template <typename T>
  requires (std::same_as<T, float> || std::same_as<T, double>)
[[gnu::noinline]] bool isFinite(T const& value) {
  static_assert(std::numeric_limits<T>::is_iec559);
  if constexpr (std::same_as<T, float>)
    return (std::bit_cast<std::uint32_t>(value) & 0x7f800000u) != 0x7f800000u;
  else
    return (std::bit_cast<std::uint64_t>(value) & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
}

template <typename T, typename T2>
  requires (!std::numeric_limits<T>::is_integer && !std::numeric_limits<T2>::is_integer && sizeof(T) >= sizeof(T2))
bool nearEqual(T x, T2 y, unsigned ulp) {
  auto epsilon = std::numeric_limits<T>::epsilon();
  return std::abs(x - y) <= epsilon * std::max(std::abs(x), (T)std::abs(y)) * ulp;
}

template <typename T, typename T2>
  requires (!std::numeric_limits<T>::is_integer && !std::numeric_limits<T2>::is_integer && sizeof(T) < sizeof(T2))
bool nearEqual(T x, T2 y, unsigned ulp) {
  return nearEqual(y, x, ulp);
}

template <typename T, typename T2>
  requires (std::numeric_limits<T>::is_integer && !std::numeric_limits<T2>::is_integer)
bool nearEqual(T x, T2 y, unsigned ulp) {
  return nearEqual(static_cast<double>(x), y, ulp);
}

template <typename T, typename T2>
  requires (!std::numeric_limits<T>::is_integer && std::numeric_limits<T2>::is_integer)
bool nearEqual(T x, T2 y, unsigned ulp) {
  return nearEqual(x, static_cast<double>(y), ulp);
}

template <typename T, typename T2>
  requires (std::numeric_limits<T>::is_integer && std::numeric_limits<T2>::is_integer)
bool nearEqual(T x, T2 y, unsigned) {
  return x == y;
}

template <typename T, typename T2>
bool nearEqual(T x, T2 y) {
  return nearEqual(x, y, 1);
}

template <typename T>
  requires (!std::numeric_limits<T>::is_integer)
bool nearZero(T x, unsigned ulp = 2) {
  return std::abs(x) <= std::numeric_limits<T>::min() * ulp;
}

template <typename T>
  requires std::numeric_limits<T>::is_integer
bool nearZero(T x) {
  return x == 0;
}

template <typename T>
constexpr T square(T const& x) {
  return x * x;
}

template <typename T>
constexpr T cube(T const& x) {
  return x * x * x;
}

template <typename Float> int ipart(Float f) { return static_cast<int>(std::floor(f)); }

template <typename Float>
Float fpart(Float f) {
  return f - ipart(f);
}

template <typename Float>
Float rfpart(Float f) {
  return 1.0 - fpart(f);
}

template <typename T, typename T2>
T clampMagnitude(T const& v, T2 const& mag) {
  if (v > mag)
    return mag;
  else if (v < -mag)
    return -mag;
  else
    return v;
}

// Legacy policy for bounds that may be reversed: the upper bound wins.
// Use std::clamp at call sites with proven ordered bounds.
template <typename T>
T clamp(T const val, T const min, T const max) {
  return std::min(std::max(val, min), max);
}

template <typename IntType, typename PowType>
IntType intPow(IntType i, PowType p) {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((p >= 0));
  }

  if (p == 0)
    return 1;
  if (p == 1)
    return i;

  IntType tmp = intPow(i, p / 2);
  if ((p % 2) == 0)
    return tmp * tmp;
  else
    return i * tmp * tmp;
}

template <typename Float>
Float sigmoid(Float x) {
  return 1 / (1 + std::exp(-x));
}

// returns a % m such that the answer is always positive.
// For example, -1 mod 10 is 9.
template <typename IntType>
IntType pmod(IntType a, IntType m) {
  IntType r = a % m;
  return r < 0 ? r + m : r;
}

// Same as pmod but for float like values.
template <typename Float>
Float pfmod(Float a, Float m) {
  if (m == 0)
    return a;

  return a - m * std::floor(a / m);
}

// Finds the *smallest* distance (in absolute value terms) from b to a (a - b)
// in a non-euclidean wrapping number line.  Suppose size is 100, wrapDiff(10,
// 109) would return 1, because 509 is congruent to the point 9.  On the other
// hand, wrapDiff(10, 111) would return -1, because 111 is congruent to the
// point 11.
template <typename Type>
Type wrapDiff(Type a, Type b, Type size) {
  a = pmod(a, size);
  b = pmod(b, size);

  Type diff = a - b;
  if (diff > size / 2)
    diff -= size;
  else if (diff < -size / 2)
    diff += size;

  return diff;
}

// Sampe as wrapDiff but for float like values
template <typename Type>
Type wrapDiffF(Type a, Type b, Type size) {
  a = pfmod(a, size);
  b = pfmod(b, size);

  Type diff = a - b;
  if (diff > size / 2)
    diff -= size;
  else if (diff < -size / 2)
    diff += size;

  return diff;
}

// like std::pow, except ignores sign, and the return value will match the sign
// of the value passed in.  ppow(-2, 2) == -4
template <typename Float>
Float ppow(Float val, Float pow) {
  return std::copysign(std::pow(std::fabs(val), pow), val);
}

// Returns angle wrapped around to the range [-pi, pi).
template <typename Float>
Float constrainAngle(Float angle) {
  angle = std::fmod((Float)(angle + std::numbers::pi), (Float)(std::numbers::pi * 2));
  if (angle < 0)
    angle += std::numbers::pi * 2;
  return angle - std::numbers::pi;
}

// Returns the closest angle movement to go from the given angle to the target
// angle, in radians.
template <typename Float>
Float angleDiff(Float angle, Float targetAngle) {
  double diff = std::fmod((Float)(targetAngle - angle + std::numbers::pi), (Float)(std::numbers::pi * 2));
  if (diff < 0)
    diff += std::numbers::pi * 2;
  return diff - std::numbers::pi;
}

// Approach the given goal value from the current value, at a maximum rate of
// change.  Rate should always be a positive value. (T must be signed).
template <typename T>
T approach(T goal, T current, T rate) {
  if (goal < current) {
    return std::max(current - rate, goal);
  } else if (goal > current) {
    return std::min(current + rate, goal);
  } else {
    return current;
  }
}

// Same as approach, specialied for angles, and always approaches from the
// closest absolute direction.
template <typename T>
T approachAngle(T goal, T current, T rate) {
  return constrainAngle(current + clampMagnitude<T>(angleDiff(current, goal), rate));
}

// Used in color conversion from floating point to uint8_t
inline std::uint8_t floatToByte(float val, bool doClamp = false) {
  if (doClamp)
    val = std::clamp(val, 0.0f, 1.0f);
  return static_cast<std::uint8_t>((val * 255.0f));
}

// Used in color conversion from uint8_t to normalized float.
inline float byteToFloat(std::uint8_t val) {
  return val / 255.0f;
}

// Turn a randomized floating point value from [0.0, 1.0] to [-1.0, 1.0]
template <typename Float>
Float randn(Float val) {
  return val * 2 - 1;
}

// Increments a value between min and max inclusive, cycling around to min when
// it would be incremented beyond max.  If the value is outside of the range,
// the next increment will start at min.
template <typename Integer>
Integer cycleIncrement(Integer val, Integer min, Integer max) {
  if (val < min || val >= max)
    return min;
  else
    return val + 1;
}

}

export module star.math_common;

export namespace Star {
  using ::Star::MathExceptionTag;
  using ::Star::MathException;
  using ::Star::isFinite;
  using ::Star::nearEqual;
  using ::Star::nearZero;
  using ::Star::square;
  using ::Star::cube;
  using ::Star::ipart;
  using ::Star::fpart;
  using ::Star::rfpart;
  using ::Star::clampMagnitude;
  using ::Star::clamp;
  using ::Star::intPow;
  using ::Star::sigmoid;
  using ::Star::pmod;
  using ::Star::pfmod;
  using ::Star::wrapDiff;
  using ::Star::wrapDiffF;
  using ::Star::ppow;
  using ::Star::constrainAngle;
  using ::Star::angleDiff;
  using ::Star::approach;
  using ::Star::approachAngle;
  using ::Star::floatToByte;
  using ::Star::byteToFloat;
  using ::Star::randn;
  using ::Star::cycleIncrement;
}

export namespace Star::Constants {
  using ::Star::Constants::rad2deg;
  using ::Star::Constants::deg2rad;
}
