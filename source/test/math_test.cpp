import std;
import star.math_common;
import star.interpolation;
import star.vector;
import star.matrix3;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

namespace {

template <typename T, typename U>
concept has_near_equal = requires(T a, U b) { near_equal(a, b); near_equal(a, b, 2); };

template <typename T>
concept has_positive_mod = requires(T a) { positive_mod(a, a); };

template <typename T>
concept has_wrap_difference = requires(T a) { wrap_difference(a, a, a); };

template <typename T>
concept has_fractional_part = requires(T a) { fractional_part(a); };

template <typename T>
concept has_angle_operations = requires(T a) {
  normalize_angle(a); angle_difference(a, a); approach_angle(a, a, a);
};

template <typename... T>
concept has_finite_classification = requires(T const&... values) { classify_finite(values...); };

struct NotNumeric {};

struct ContextualBool {
  bool value;
  explicit constexpr operator bool() const noexcept { return value; }
};

struct AlgebraicScalar {
  int value;
  constexpr AlgebraicScalar(int value) noexcept : value(value) {}
  AlgebraicScalar(AlgebraicScalar const&) noexcept = default;
  AlgebraicScalar(AlgebraicScalar&&) = delete;
  AlgebraicScalar& operator=(AlgebraicScalar const&) = delete;
  AlgebraicScalar& operator=(AlgebraicScalar&&) = delete;
  friend constexpr AlgebraicScalar operator*(AlgebraicScalar const& a, AlgebraicScalar const& b) noexcept {
    return AlgebraicScalar(a.value * b.value);
  }
  friend constexpr AlgebraicScalar operator+(AlgebraicScalar const& a, AlgebraicScalar const& b) noexcept {
    return AlgebraicScalar(a.value + b.value);
  }
  friend constexpr AlgebraicScalar operator-(AlgebraicScalar const& a, AlgebraicScalar const& b) noexcept {
    return AlgebraicScalar(a.value - b.value);
  }
  friend constexpr ContextualBool operator<(AlgebraicScalar const& a, AlgebraicScalar const& b) noexcept {
    return {a.value < b.value};
  }
  friend constexpr ContextualBool operator>(AlgebraicScalar const& a, AlgebraicScalar const& b) noexcept {
    return {a.value > b.value};
  }
};

struct ExplicitScalar {
  explicit constexpr ExplicitScalar(int) noexcept {}
};
// These operators participate only in unevaluated constraint checks.
[[maybe_unused]] int operator*(ExplicitScalar const&, ExplicitScalar const&) noexcept;

struct ExplicitInterpolationValue {
  explicit ExplicitInterpolationValue(double) {}
};
[[maybe_unused]] double operator*(ExplicitInterpolationValue const&, double);

struct CopyOnlyEndpoint {
  int value;
  explicit CopyOnlyEndpoint(int value) : value(value) {}
  CopyOnlyEndpoint(CopyOnlyEndpoint const&) = default;
  CopyOnlyEndpoint(CopyOnlyEndpoint&&) = delete;
  CopyOnlyEndpoint& operator=(CopyOnlyEndpoint const&) = delete;
};

struct SamplePosition {
  double position;
};

using SamplePositionIterator = std::array<SamplePosition, 4>::const_iterator;

struct LowerPositionCompare {
  int calls = 0;
  LowerPositionCompare() = default;
  LowerPositionCompare(LowerPositionCompare const&) = delete;
  ContextualBool operator()(SamplePosition const& sample, double position) {
    ++calls;
    return {sample.position < position};
  }
};

struct UpperPositionCompare {
  int calls = 0;
  UpperPositionCompare() = default;
  UpperPositionCompare(UpperPositionCompare const&) = delete;
  ContextualBool operator()(double position, SamplePosition const& sample) {
    ++calls;
    return {position < sample.position};
  }
};

struct SamplePositionGetter {
  int calls = 0;
  SamplePositionGetter() = default;
  SamplePositionGetter(SamplePositionGetter const&) = delete;
  double operator()(SamplePosition const& sample) {
    ++calls;
    return sample.position;
  }
};

struct MutableLinearWeights {
  int calls = 0;
  MutableLinearWeights() = default;
  MutableLinearWeights(MutableLinearWeights const&) = delete;
  std::array<double, 2> operator()(double x) {
    ++calls;
    return {1 - x, x};
  }
};

struct MutableCubicWeights {
  int calls = 0;
  MutableCubicWeights() = default;
  MutableCubicWeights(MutableCubicWeights const&) = delete;
  auto operator()(double x) {
    ++calls;
    return Cubic4WeightOperator<double>{}(x);
  }
};

struct InvalidWeights {
  void operator()(double) {}
};

static_assert(lerp_compatible<float, int> && lerp_compatible<float, Vec2F>);
static_assert(lerp_compatible<AlgebraicScalar, AlgebraicScalar>);
static_assert(!lerp_compatible<double, ExplicitInterpolationValue> && !lerp_compatible<double, NotNumeric>);
static_assert(!cubic4_compatible<double, NotNumeric> && !hermite2_compatible<double, NotNumeric>
              && !quintic2_compatible<double, NotNumeric> && !sin_ease_compatible<double, NotNumeric>);
static_assert(!angle_lerp_compatible<double, int> && angle_lerp_compatible<float, float>);
static_assert(step_compatible<int, CopyOnlyEndpoint> && half_step_compatible<double, CopyOnlyEndpoint>);
static_assert(!step_compatible<NotNumeric, int> && !half_step_compatible<NotNumeric, int>);
static_assert(linear_sequence_interpolatable<std::array<double, 2>, double, MutableLinearWeights>);
static_assert(cubic_sequence_interpolatable<std::array<double, 4>, double, MutableCubicWeights>);
static_assert(!linear_sequence_interpolatable<std::array<double, 2>, double, InvalidWeights>);
static_assert(!cubic_sequence_interpolatable<std::array<double, 4>, double, InvalidWeights>);
static_assert(lower_inverse_interpolatable<SamplePositionIterator, double, LowerPositionCompare, SamplePositionGetter>);
static_assert(upper_inverse_interpolatable<SamplePositionIterator, double, UpperPositionCompare, SamplePositionGetter>);
static_assert(
  !lower_inverse_interpolatable<SamplePositionIterator, double, UpperPositionCompare, SamplePositionGetter>);
static_assert(
  !upper_inverse_interpolatable<SamplePositionIterator, double, LowerPositionCompare, SamplePositionGetter>);
static_assert(
  !lower_inverse_interpolatable<std::forward_list<double>::iterator, double, std::less<double>, std::identity>);
static_assert(lerp(0.25, 2.0, 10.0) == 4.0);
static_assert(hermite2(0.25, 3.0, 11.0) == 4.25);
static_assert(quintic2(0.25, 3.0, 11.0) == 3.828125);
static_assert(cubic4(0.25, 2.0, 5.0, -3.0, 11.0) == 3.2578125);
static_assert(LinearWeightOperator<double>{}(0.25)[0] == 0.75);
static_assert(Cubic4WeightOperator<double>{}(0.25)[0] == -0.0703125);
static_assert(get_bound4(0.25, std::size_t{2}, BoundMode::Clamp).i3 == 1);
static_assert(noexcept(lerp(std::declval<AlgebraicScalar const&>(), std::declval<AlgebraicScalar const&>(),
                            std::declval<AlgebraicScalar const&>())));

static_assert(has_near_equal<int, unsigned> && has_near_equal<float, long double>);
static_assert(!has_near_equal<int, double> && !has_near_equal<double, int>);
static_assert(!has_near_equal<NotNumeric, NotNumeric>);
static_assert(has_positive_mod<int> && has_positive_mod<unsigned> && has_positive_mod<long double>);
static_assert(!has_positive_mod<NotNumeric>);
static_assert(has_wrap_difference<int> && has_wrap_difference<long double>);
static_assert(!has_wrap_difference<unsigned> && !has_wrap_difference<bool>);
static_assert(has_fractional_part<float> && !has_fractional_part<int> && !has_fractional_part<NotNumeric>);
static_assert(has_angle_operations<long double> && !has_angle_operations<int>);
static_assert(finite_classifiable<float> && finite_classifiable<double> && !finite_classifiable<long double>);
static_assert(has_finite_classification<float, double> && has_finite_classification<double, double, double>);
static_assert(!has_finite_classification<> && !has_finite_classification<float, int> && !has_finite_classification<long double>);
static_assert(!square_operand<NotNumeric> && !square_operand<ExplicitScalar>);
static_assert(!clampable<NotNumeric> && !approachable<NotNumeric>);
static_assert(square_operand<AlgebraicScalar> && clampable<AlgebraicScalar> && approachable<AlgebraicScalar>);
static_assert(magnitude_clampable<AlgebraicScalar, int>);
static_assert(noexcept(square(std::declval<AlgebraicScalar const&>())));
static_assert(noexcept(clamp_magnitude(std::declval<AlgebraicScalar const&>(), 2)));
static_assert(noexcept(clamp(std::declval<AlgebraicScalar const&>(), std::declval<AlgebraicScalar const&>(),
  std::declval<AlgebraicScalar const&>())));
static_assert(noexcept(approach(std::declval<AlgebraicScalar const&>(), std::declval<AlgebraicScalar const&>(),
  std::declval<AlgebraicScalar const&>())));
static_assert(square(4) == 16 && clamp(4, 1, 3) == 3 && approach(8, 2, 3) == 5);

// Precise caller flags do not change the imported std::acos wrapper's FP mode.
// Bypass that wrapper and round a long-double oracle to the tested type.
#pragma float_control(precise, on, push)
template <std::floating_point T>
[[gnu::noinline]] void expect_angle_boundaries() {
  T pi = static_cast<T>(__builtin_acosl(-1.0L));
  T below = std::nextafter(pi, T{0});
  T above = std::nextafter(pi, T{4});
  EXPECT_EQ(normalize_angle(pi), -pi);
  EXPECT_EQ(normalize_angle(-pi), -pi);
  EXPECT_EQ(normalize_angle(below), below);
  EXPECT_LT(normalize_angle(above), T{0});
  EXPECT_GT(normalize_angle(std::nextafter(-pi, T{-4})), T{0});
  EXPECT_EQ(angle_difference(T{0}, pi), -pi);
  EXPECT_EQ(angle_difference(T{0}, -pi), -pi);
  T tiny = std::numeric_limits<T>::epsilon() / T{8};
  EXPECT_EQ(normalize_angle(tiny), tiny);
  EXPECT_EQ(normalize_angle(-tiny), -tiny);
  EXPECT_EQ(angle_difference(T{0}, tiny), tiny);
  EXPECT_EQ(angle_difference(tiny, T{0}), -tiny);
  T period = T{2} * pi;
  T turn_boundary = T{3} * pi;
  for (T value : {T{-16}, T{-8}, T{8}, T{16}, std::nextafter(turn_boundary, T{0}),
         std::nextafter(-turn_boundary, T{-16}), std::numeric_limits<T>::max()}) {
    T expected = std::remainder(value, period);
    if (expected >= pi)
      expected -= period;
    EXPECT_EQ(normalize_angle(value), expected);
  }
}
#pragma float_control(pop)

}

TEST(Math, IntegralNearEqualPreservesValuesAcrossSignednessAndPromotions) {
  EXPECT_FALSE(near_equal(-1, std::numeric_limits<unsigned>::max()));
  EXPECT_FALSE(near_equal(-1LL, std::numeric_limits<std::uint64_t>::max()));
  EXPECT_TRUE(near_equal(42, 42u));
  EXPECT_TRUE(near_equal(static_cast<signed char>(7), static_cast<unsigned char>(7)));
  EXPECT_FALSE(near_equal(static_cast<signed char>(-1), static_cast<unsigned char>(255)));
  EXPECT_TRUE(near_equal(true, 1u));
  EXPECT_TRUE(near_equal(false, '\0'));
  EXPECT_FALSE(near_equal(true, 2));
  EXPECT_TRUE(near_equal('A', L'A'));
  EXPECT_TRUE(near_equal(u'A', U'A'));
  EXPECT_TRUE(near_equal(char8_t{65}, 65));
  EXPECT_FALSE(near_equal(char32_t{0xffffffffu}, -1));
}

TEST(Math, FloatingNearEqualUsesCommonTypeAndRelativeEpsilon) {
  double adjacent = std::nextafter(1.0, 2.0);
  EXPECT_TRUE(near_equal(1.0f, adjacent));
  EXPECT_TRUE(near_equal(adjacent, 1.0f));
  EXPECT_FALSE(near_equal(1.0f, 1.0 + 4 * std::numeric_limits<double>::epsilon()));
  EXPECT_FALSE(near_equal(0.0, std::numeric_limits<double>::min()));
  EXPECT_TRUE(near_equal(0.0, -0.0));
}

TEST(Math, NearZeroRetainsMinimumNormalPolicy) {
  volatile std::uint64_t subnormal_bits = 1;
  double subnormal = std::bit_cast<double>(static_cast<std::uint64_t>(subnormal_bits));
  EXPECT_TRUE(near_zero(subnormal));
  EXPECT_TRUE(near_zero(-subnormal));
  EXPECT_TRUE(near_zero(std::numeric_limits<double>::min() * 2));
  EXPECT_FALSE(near_zero(std::numeric_limits<double>::min() * 4));
  EXPECT_TRUE(near_zero(0));
  EXPECT_FALSE(near_zero(1u));
  EXPECT_TRUE(near_zero(false));
  EXPECT_FALSE(near_zero(std::numeric_limits<double>::epsilon()));
}

TEST(Math, FractionalPartHandlesNegativeAndHugeFiniteInputs) {
  EXPECT_EQ(fractional_part(-1.25), 0.75);
  EXPECT_EQ(fractional_part(-0.25f), 0.75f);
  EXPECT_EQ(fractional_part(3.25L), 0.25L);
  EXPECT_EQ(fractional_part(-4.0), 0.0);
  EXPECT_EQ(fractional_part(std::numeric_limits<double>::max()), 0.0);
  EXPECT_EQ(fractional_part(-std::numeric_limits<double>::max()), 0.0);
  // Preserve the original signed-zero result through production fast-math.
  // Volatile input bits and object representations avoid signbit folding.
  for (auto bits : {0ull, 0x8000000000000000ull}) {
    volatile std::uint64_t input = bits;
    double result = fractional_part(std::bit_cast<double>(static_cast<std::uint64_t>(input)));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(result), bits);
  }
  for (auto bits : {0u, 0x80000000u}) {
    volatile std::uint32_t input = bits;
    float result = fractional_part(std::bit_cast<float>(static_cast<std::uint32_t>(input)));
    EXPECT_EQ(std::bit_cast<std::uint32_t>(result), bits);
  }
}

TEST(Math, SignedModuloAndWrappingPreserveHalfPeriodAndZeroPolicies) {
  EXPECT_EQ(positive_mod(-1, 10), 9);
  EXPECT_EQ(positive_mod(-20, 10), 0);
  EXPECT_EQ(positive_mod(static_cast<signed char>(-8), static_cast<signed char>(7)), 6);
  int max = std::numeric_limits<int>::max();
  EXPECT_EQ(positive_mod(std::numeric_limits<int>::min(), max), max - 1);
  EXPECT_EQ(wrap_difference(std::numeric_limits<int>::min(), max, max), -1);
  EXPECT_EQ(wrap_difference(5, 0, 10), 5);
  EXPECT_EQ(wrap_difference(0, 5, 10), -5);
  EXPECT_EQ(wrap_difference(10, 109, 100), 1);
  EXPECT_EQ(wrap_difference(10, 111, 100), -1);
  EXPECT_EQ(wrap_difference(static_cast<signed char>(-8), static_cast<signed char>(8),
    static_cast<signed char>(10)), 4);
  EXPECT_EQ(positive_mod(-1.25, 1.0), 0.75);
  EXPECT_EQ(positive_mod(-1.25, 0.0), -1.25);
  EXPECT_EQ(wrap_difference(5.0, 0.0, 10.0), 5.0);
  EXPECT_EQ(wrap_difference(0.0, 5.0, 10.0), -5.0);
  EXPECT_EQ(wrap_difference(3.0, 7.0, 0.0), -4.0);
}

TEST(Math, TypedAnglesPreserveIntervalAndTinyDisplacements) {
  expect_angle_boundaries<float>();
  expect_angle_boundaries<double>();
  expect_angle_boundaries<long double>();
  long double current = 1.0L;
  long double target = current + 8 * std::numeric_limits<long double>::epsilon();
  EXPECT_EQ(angle_difference(current, target), target - current);
  EXPECT_EQ(approach_angle(0.5, 0.0, 0.125), 0.125);
  EXPECT_EQ(approach_angle(-0.5, 0.0, 0.125), -0.125);
}

TEST(Math, AlgebraicHelpersAcceptCopyOnlyAndContextualComparisons) {
  AlgebraicScalar value{7}, negative{-7}, low{2}, high{5}, rate{2};
  EXPECT_EQ(square(value).value, 49);
  EXPECT_EQ(clamp_magnitude(value, 3).value, 3);
  EXPECT_EQ(clamp_magnitude(negative, 3).value, -3);
  EXPECT_EQ(clamp_magnitude(low, 3).value, 2);
  EXPECT_EQ(clamp(value, low, high).value, 5);
  EXPECT_EQ(clamp(low, high, rate).value, 2);
  EXPECT_EQ(approach(value, low, rate).value, 4);
  EXPECT_EQ(approach(low, value, rate).value, 5);
  EXPECT_EQ(approach(high, value, value).value, 5);
  EXPECT_EQ(approach(value, value, rate).value, 7);
}

TEST(Math, FiniteBoundaryClassificationSurvivesFastMath) {
  for (auto value : {0.0, -0.0, 1.0, -1.0, std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min()}) {
    EXPECT_TRUE(is_finite(value));
    EXPECT_EQ(classify_finite(value, value, value), (std::array{true, true, true}));
  }
  for (auto bits : {0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000001ull, 0xfff8000000000001ull}) {
    volatile std::uint64_t input = bits;
    auto value = std::bit_cast<double>(static_cast<std::uint64_t>(input));
    EXPECT_FALSE(is_finite(value));
    EXPECT_EQ(classify_finite(value, 1.0, value), (std::array{false, true, false}));
    EXPECT_EQ(classify_finite(1.0, value, 1.0), (std::array{true, false, true}));
  }
  for (auto value : {0.0f, -0.0f, 1.0f, -1.0f, std::numeric_limits<float>::max(), std::numeric_limits<float>::denorm_min()}) {
    EXPECT_TRUE(is_finite(value));
    EXPECT_EQ(classify_finite(value, value), (std::array{true, true}));
  }
  for (auto bits : {0x7f800000u, 0xff800000u, 0x7fc00001u, 0xffc00001u}) {
    volatile std::uint32_t input = bits;
    auto value = std::bit_cast<float>(static_cast<std::uint32_t>(input));
    EXPECT_FALSE(is_finite(value));
    EXPECT_EQ(classify_finite(value, 1.0f, value), (std::array{false, true, false}));
    EXPECT_EQ(classify_finite(1.0f, value, 1.0f), (std::array{true, false, true}));
  }
}

TEST(Math, CycleIncrement) {
  int a = 0;
  a = cycle_increment(a, 10, 13);
  ASSERT_EQ(a, 10);
  a = cycle_increment(a, 10, 13);
  ASSERT_EQ(a, 11);
  a = cycle_increment(a, 10, 13);
  ASSERT_EQ(a, 12);
  a = cycle_increment(a, 10, 13);
  ASSERT_EQ(a, 13);
  a = cycle_increment(a, 10, 13);
  ASSERT_EQ(a, 10);
  int b = 14;
  b = cycle_increment(b, 10, 13);
  ASSERT_EQ(b, 10);
}

TEST(Math, GenericLerpPreservesArithmetic) {
  EXPECT_EQ(lerp(0.25f, Vec2F(2.0f, -4.0f), Vec2F(10.0f, 4.0f)), Vec2F(4.0f, -2.0f));
  EXPECT_EQ(lerp(2.0f, Vec2F(2.0f, -4.0f), Vec2F(10.0f, 4.0f)), Vec2F(18.0f, 12.0f));
  EXPECT_EQ(lerp(0.25f, -3, 3), -1);
}

TEST(Interpolation, SelectedEndpointOwnsCopyOnlyAndTemporaryValues) {
  CopyOnlyEndpoint a{3}, b{9};
  auto first = step(5, 4, a, b);
  EXPECT_EQ(first.value, 3);
  auto second = half_step(0.5, a, b);
  EXPECT_EQ(second.value, 9);
  auto temporary = half_step(0.25, CopyOnlyEndpoint{7}, CopyOnlyEndpoint{12});
  EXPECT_EQ(temporary.value, 7);
  a.value = 21;
  b.value = 34;
  EXPECT_EQ(first.value, 3);
  EXPECT_EQ(second.value, 9);
  AlgebraicScalar zero{0}, one{1}, low{3}, high{9};
  EXPECT_EQ(lerp(zero, low, high).value, 3);
  EXPECT_EQ(lerp(one, low, high).value, 9);
}

TEST(Interpolation, BoundsAreSafeForShortSignedAndSizeExtents) {
  auto check = []<typename Index>() {
    for (auto mode : {BoundMode::Clamp, BoundMode::Wrap, BoundMode::Extrapolate}) {
      for (Index extent : {Index{0}, Index{1}, Index{2}, Index{3}}) {
        for (double x : {-8.25, -1.0, -0.25, 0.0, 0.75, 1.5, 2.75, 8.25}) {
          auto linear = get_bound2(x, extent, mode);
          auto cubic = get_bound4(x, extent, mode);
          if (extent <= 1) {
            EXPECT_EQ(linear.i0, Index{0});
            EXPECT_EQ(linear.i1, Index{0});
            EXPECT_EQ(cubic.i0, Index{0});
            EXPECT_EQ(cubic.i1, Index{0});
            EXPECT_EQ(cubic.i2, Index{0});
            EXPECT_EQ(cubic.i3, Index{0});
            EXPECT_EQ(cubic.offset, 0);
          } else {
            EXPECT_LT(cubic.i0, extent);
            EXPECT_LT(cubic.i1, extent);
            EXPECT_LT(cubic.i2, extent);
            EXPECT_LT(cubic.i3, extent);
            EXPECT_GE(cubic.i0, Index{0});
            EXPECT_GE(cubic.i1, Index{0});
            EXPECT_GE(cubic.i2, Index{0});
            EXPECT_GE(cubic.i3, Index{0});
            EXPECT_EQ(cubic.i1, linear.i0);
            EXPECT_EQ(cubic.i2, linear.i1);
            if (mode != BoundMode::Wrap) {
              EXPECT_EQ(cubic.i0, linear.i0);
              EXPECT_EQ(cubic.i3, linear.i1);
            }
            EXPECT_EQ(cubic.offset, linear.offset);
          }
        }
      }
    }
    auto wrapped = get_bound2(-0.25, Index{3}, BoundMode::Wrap);
    EXPECT_EQ(wrapped.i0, Index{2});
    EXPECT_EQ(wrapped.i1, Index{0});
    EXPECT_EQ(wrapped.offset, 0.75);
    auto outside = get_bound2(-2, Index{3}, BoundMode::Extrapolate);
    EXPECT_EQ(outside.i0, Index{0});
    EXPECT_EQ(outside.i1, Index{1});
    EXPECT_EQ(outside.offset, -2);
  };
  check.operator()<int>();
  check.operator()<std::size_t>();
  auto pair_start = get_bound4(0.25, 2, BoundMode::Wrap);
  EXPECT_EQ(pair_start.i0, 1);
  EXPECT_EQ(pair_start.i1, 0);
  EXPECT_EQ(pair_start.i2, 1);
  EXPECT_EQ(pair_start.i3, 0);
  EXPECT_EQ(pair_start.offset, 0.25);
  auto pair_end = get_bound4(1.25, std::size_t{2}, BoundMode::Wrap);
  EXPECT_EQ(pair_end.i0, 0u);
  EXPECT_EQ(pair_end.i1, 1u);
  EXPECT_EQ(pair_end.i2, 0u);
  EXPECT_EQ(pair_end.i3, 1u);
  auto triple_start = get_bound4(0.25, 3, BoundMode::Wrap);
  EXPECT_EQ(triple_start.i0, 2);
  EXPECT_EQ(triple_start.i1, 0);
  EXPECT_EQ(triple_start.i2, 1);
  EXPECT_EQ(triple_start.i3, 2);
  EXPECT_EQ(get_bound4(3.0, -1, BoundMode::Wrap).i3, 0);
}

TEST(Interpolation, CubicBoundsRetainFullWindowAndExtrapolationOffsets) {
  auto start = get_bound4(-0.5, std::size_t{5}, BoundMode::Clamp);
  EXPECT_EQ(start.i0, 0u);
  EXPECT_EQ(start.i1, 1u);
  EXPECT_EQ(start.i2, 2u);
  EXPECT_EQ(start.i3, 3u);
  EXPECT_EQ(start.offset, -1);
  auto end = get_bound4(8.0, std::size_t{5}, BoundMode::Clamp);
  EXPECT_EQ(end.i0, 1u);
  EXPECT_EQ(end.i1, 2u);
  EXPECT_EQ(end.i2, 3u);
  EXPECT_EQ(end.i3, 4u);
  EXPECT_EQ(end.offset, 2);
  EXPECT_EQ(get_bound4(-0.5, 5, BoundMode::Extrapolate).offset, -1.5);
  EXPECT_EQ(get_bound4(4.5, 5, BoundMode::Extrapolate).offset, 2.5);
  auto wrapped = get_bound4(-0.25, 5, BoundMode::Wrap);
  EXPECT_EQ(wrapped.i0, 3);
  EXPECT_EQ(wrapped.i1, 4);
  EXPECT_EQ(wrapped.i2, 0);
  EXPECT_EQ(wrapped.i3, 1);
  EXPECT_EQ(wrapped.offset, 0.75);
}

TEST(Interpolation, ShortNonPeriodicCubicSequencesUseLinearValuesWithoutCallingCustomWeights) {
  std::array<double, 0> empty{};
  std::array<double, 1> sole{7};
  std::array<double, 2> pair{2, 10};
  std::array<double, 3> triple{2, 10, -6};
  MutableCubicWeights weights;
  for (auto mode : {BoundMode::Clamp, BoundMode::Extrapolate}) {
    EXPECT_EQ(list_interpolate4(empty, -3.25, weights, mode), 0);
    EXPECT_EQ(list_interpolate4(sole, -3.25, weights, mode), 7);
    EXPECT_EQ(list_interpolate4(pair, 0.25, weights, mode), 4);
    EXPECT_EQ(list_interpolate4(triple, 1.5, weights, mode), 2);
  }
  EXPECT_EQ(list_interpolate4(pair, -1.0, weights), 2);
  EXPECT_EQ(list_interpolate4(pair, 3.0, weights), 10);
  EXPECT_EQ(list_interpolate4(pair, -1.0, weights, BoundMode::Extrapolate), -6);
  EXPECT_EQ(list_interpolate4(pair, 3.0, weights, BoundMode::Extrapolate), 26);
  EXPECT_EQ(list_interpolate4(triple, 3.0, weights, BoundMode::Extrapolate), -22);
  EXPECT_EQ(weights.calls, 0);
  std::array<Vec2F, 2> vectors{Vec2F{2, -4}, Vec2F{10, 4}};
  EXPECT_EQ(list_interpolate4(vectors, 0.25f, Cubic4WeightOperator<float>{}), Vec2F(4, -2));
  std::array<int, 2> integers{-3, 3};
  EXPECT_EQ(list_interpolate4(integers, 0.25, weights), -1);
}

TEST(Interpolation, ShortPeriodicCubicSequencesKeepNonAffineCubicWeights) {
  std::array<double, 2> pair{2, 10};
  std::array<double, 3> triple{2, 10, -6};
  MutableCubicWeights weights;
  EXPECT_EQ(list_interpolate4(pair, 0.25, weights, BoundMode::Wrap), 3.25);
  EXPECT_EQ(list_interpolate4(pair, -0.25, weights, BoundMode::Wrap), 3.25);
  EXPECT_EQ(list_interpolate4(pair, 1.0, weights, BoundMode::Wrap), 10);
  EXPECT_EQ(list_interpolate4(triple, 0.25, weights, BoundMode::Wrap), 4.5625);
  EXPECT_EQ(list_interpolate4(triple, -0.25, weights, BoundMode::Wrap), -0.5625);
  EXPECT_EQ(list_interpolate4(triple, 3.25, weights, BoundMode::Wrap), 4.5625);
  std::array<double, 0> empty{};
  std::array<double, 1> sole{7};
  EXPECT_EQ(list_interpolate4(empty, -3.25, weights, BoundMode::Wrap), 0);
  EXPECT_EQ(list_interpolate4(sole, -3.25, weights, BoundMode::Wrap), 7);
}

TEST(Interpolation, NonAffineCubicWeightsAndScalarEndpointsAgree) {
  std::array<double, 4> values{2, 5, -3, 11};
  auto weights = Cubic4WeightOperator<double>{}(0.25);
  EXPECT_EQ(weights[0], -0.0703125);
  EXPECT_EQ(weights[1], 0.8671875);
  EXPECT_EQ(weights[2], 0.2265625);
  EXPECT_EQ(weights[3], -0.0234375);
  EXPECT_EQ(weights[0] + weights[1] + weights[2] + weights[3], 1);
  EXPECT_EQ(cubic4(0.25, 2.0, 5.0, -3.0, 11.0), 3.2578125);
  EXPECT_EQ(cubic4(0.0, 2.0, 5.0, -3.0, 11.0), 5);
  EXPECT_EQ(cubic4(1.0, 2.0, 5.0, -3.0, 11.0), -3);
  MutableCubicWeights callback;
  EXPECT_EQ(list_interpolate4(values, 1.25, callback), 3.2578125);
  EXPECT_EQ(list_interpolate4(values, 1.0, callback), 5);
  EXPECT_EQ(list_interpolate4(values, 2.0, callback), -3);
  auto below = Cubic4WeightOperator<double>{true}(-0.5);
  auto above = Cubic4WeightOperator<double>{true}(1.5);
  EXPECT_EQ(below[0], 0.5);
  EXPECT_EQ(below[1], 0.5);
  EXPECT_EQ(below[2], 0);
  EXPECT_EQ(below[3], 0);
  EXPECT_EQ(above[0], 0);
  EXPECT_EQ(above[1], 0);
  EXPECT_EQ(above[2], 0.5);
  EXPECT_EQ(above[3], 0.5);
  EXPECT_EQ(hermite2(0.25, 3.0, 11.0), 4.25);
  EXPECT_EQ(quintic2(0.25, 3.0, 11.0), 3.828125);
  EXPECT_EQ(hermite2(0.0, 3.0, 11.0), 3);
  EXPECT_EQ(hermite2(1.0, 3.0, 11.0), 11);
  EXPECT_EQ(quintic2(0.0, 3.0, 11.0), 3);
  EXPECT_EQ(quintic2(1.0, 3.0, 11.0), 11);
  EXPECT_EQ(lerp_with_limit(std::optional{4.0}, 0.25, 2.0, 10.0), 10);
  EXPECT_EQ(lerp_with_limit(std::optional<double>{}, 0.25, 2.0, 10.0), 4);
  EXPECT_NEAR(sin_ease(0.5, 2.0, 10.0), 6, 1e-12);
  EXPECT_NEAR(angle_lerp(0.5, 3.0, -3.0), std::numbers::pi, 1e-12);
}

TEST(Interpolation, InverseMappingHonorsDirectionalMutableCallbacksAndDuplicatePolicy) {
  std::array<SamplePosition, 4> samples{{{0}, {2}, {2}, {6}}};
  LowerPositionCompare lower;
  UpperPositionCompare upper;
  SamplePositionGetter getter;
  EXPECT_EQ(inverse_linear_interpolate_lower(samples.begin(), samples.end(), 2.0, lower, getter), 1);
  EXPECT_EQ(inverse_linear_interpolate_upper(samples.begin(), samples.end(), 2.0, upper, getter), 2);
  EXPECT_EQ(inverse_linear_interpolate_lower(samples.begin(), samples.end(), -2.0, lower, getter), -1);
  EXPECT_EQ(inverse_linear_interpolate_upper(samples.begin(), samples.end(), 10.0, upper, getter), 4);
  std::array<double, 3> start{0, 0, 1}, end{0, 1, 1};
  EXPECT_EQ(inverse_linear_interpolate_lower(start.begin(), start.end(), 0.0), 0);
  EXPECT_EQ(inverse_linear_interpolate_upper(start.begin(), start.end(), 0.0), 1);
  EXPECT_EQ(inverse_linear_interpolate_lower(end.begin(), end.end(), 1.0), 1);
  EXPECT_EQ(inverse_linear_interpolate_upper(end.begin(), end.end(), 1.0), 2);
  EXPECT_EQ(inverse_linear_interpolate_lower(start.begin(), start.begin(), 4.0), 0);
  EXPECT_EQ(inverse_linear_interpolate_upper(start.begin(), start.begin() + 1, 4.0), 0);
}

TEST(Interpolation, ParametricShortCubicAndLinearAcceptBorrowedMutableWeights) {
  std::array<double, 3> positions{0, 2, 10}, values{2, 10, -6};
  MutableLinearWeights linear;
  EXPECT_EQ(parametric_interpolate2(positions, values, 1.0, linear, BoundMode::Clamp), 6);
  MutableCubicWeights cubic;
  EXPECT_EQ(parametric_interpolate4(positions, values, 1.0, cubic, BoundMode::Clamp), 6);
  EXPECT_EQ(parametric_interpolate4(positions, values, 6.0, cubic, BoundMode::Clamp), 2);
  EXPECT_EQ(parametric_interpolate4(positions, values, -2.0, cubic, BoundMode::Extrapolate), -6);
  EXPECT_EQ(parametric_interpolate4(positions, values, 18.0, cubic, BoundMode::Extrapolate), -22);
  EXPECT_EQ(cubic.calls, 0);
  EXPECT_EQ(parametric_interpolate4(positions, values, 11.0, cubic, BoundMode::Wrap), -6.1484375);
}

TEST(Math, ClampedFloatToBytePreservesSaturationAndTruncation) {
  EXPECT_EQ(float_to_byte(-1.0f, true), 0);
  EXPECT_EQ(float_to_byte(0.0f, true), 0);
  EXPECT_EQ(float_to_byte(0.5f, true), 127);
  EXPECT_EQ(float_to_byte(1.0f, true), 255);
  EXPECT_EQ(float_to_byte(2.0f, true), 255);
}

TEST(Math, LegacyClampPreservesReversedBounds) {
  // Runtime-configured ranges can put the upper bound below the lower bound.
  // These calls intentionally retain the project policy rather than std::clamp.
  EXPECT_EQ(clamp(-5, 0, -2), -2);
  EXPECT_EQ(clamp(1, 0, -2), -2);
  EXPECT_EQ(clamp(-5.0f, 0.0f, -2.0f), -2.0f);
  EXPECT_EQ(clamp(1.0f, 0.0f, -2.0f), -2.0f);
}

TEST(Matrix3, RangeCopyPreservesRowMajorValuesAndDestinationBounds) {
  std::array<int, 9> input{1, 2, 3, 4, 5, 6, 7, 8, 9};
  Matrix3<int> matrix(input);
  input[3] = 99;
  EXPECT_EQ(2, matrix[0][1]);
  EXPECT_EQ(4, matrix[1][0]);

  std::array<int, 11> output;
  output.fill(-1);
  matrix.copy_to(std::span(output).subspan<1, 9>());
  EXPECT_EQ(output, (std::array<int, 11>{-1, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1}));
}

namespace {

template <typename M>
concept has_matrix_inverse = requires(M const& matrix) { matrix.inverse(); };

template <typename M>
concept has_matrix_sum = requires(M const& matrix) { matrix + matrix; };

template <typename M>
concept has_matrix_product = requires(M const& matrix) { matrix * matrix; };

struct StoredOnly {
  int value = 0;
};

struct AddOnly {
  int value = 0;
  AddOnly() = default;
  explicit constexpr AddOnly(int value) : value(value) {}
  AddOnly(AddOnly const&) = delete;
  AddOnly& operator=(AddOnly const&) = delete;
  AddOnly(AddOnly&&) = default;
  AddOnly& operator=(AddOnly&&) = default;
  friend constexpr AddOnly operator+(AddOnly const& left, AddOnly const& right) {
    return AddOnly(left.value + right.value);
  }
};

struct CopyOnly {
  int value = 0;
  CopyOnly() = default;
  explicit constexpr CopyOnly(int value) : value(value) {}
  CopyOnly(CopyOnly const&) = default;
  CopyOnly& operator=(CopyOnly const&) = default;
  CopyOnly& operator=(CopyOnly&&) = delete;
  friend constexpr CopyOnly operator+(CopyOnly const& left, CopyOnly const& right) {
    return CopyOnly(left.value + right.value);
  }
  friend constexpr CopyOnly operator*(CopyOnly const& left, CopyOnly const& right) {
    return CopyOnly(left.value * right.value);
  }
};

struct ColumnSource {
  int value = 0;
  ColumnSource() = default;
  ColumnSource(ColumnSource const&) = delete;
  ColumnSource& operator=(ColumnSource const&) = delete;
  ColumnSource(ColumnSource&&) = default;
  ColumnSource& operator=(ColumnSource&&) = default;
};

struct ColumnDestination {
  int value = 0;
  ColumnDestination() = default;
  explicit ColumnDestination(ColumnSource const& source) : value(source.value) {}
  ColumnDestination(ColumnDestination const&) = delete;
  ColumnDestination& operator=(ColumnDestination const&) = delete;
  ColumnDestination(ColumnDestination&&) = default;
  ColumnDestination& operator=(ColumnDestination&&) = default;
};

static_assert(std::is_default_constructible_v<Matrix3<StoredOnly>>);
static_assert(!has_matrix_sum<Matrix3<StoredOnly>>);
static_assert(!has_matrix_product<Matrix3<StoredOnly>>);
static_assert(has_matrix_sum<Matrix3<AddOnly>>);
static_assert(!has_matrix_product<Matrix3<AddOnly>>);
static_assert(!has_matrix_product<Matrix3<CopyOnly>>);
static_assert(matrix_product<CopyOnly, CopyOnly>);
static_assert(matrix_point_transform<CopyOnly, CopyOnly, CopyOnly>);
static_assert(matrix_direction_transform<CopyOnly, CopyOnly, CopyOnly>);
static_assert(has_matrix_inverse<Mat3F> && has_matrix_inverse<Mat3D>);
static_assert(!has_matrix_inverse<Matrix3<int>>);
static_assert(std::same_as<decltype(std::declval<Mat3D&>()[0]), Vec3D&>);
static_assert(std::same_as<decltype(std::declval<Mat3D const&>()[0]), Vec3D const&>);
static_assert(std::same_as<decltype(std::declval<Mat3D&&>()[0]), Vec3D>);
static_assert(std::same_as<decltype(std::declval<Mat3D const&&>()[0]), Vec3D>);
static_assert(std::same_as<decltype(std::declval<Mat3D const&>().column(0)), Vec3D>);
static_assert(std::same_as<decltype(std::declval<Mat3D const&>().trace()), double>);
static_assert(sizeof(Mat3F) == 9 * sizeof(float));
static_assert(sizeof(Mat3D) == 9 * sizeof(double));
static_assert(Matrix3<int>::identity().trace() == 3);
static_assert(Matrix3<int>().trace() == 0);
static_assert(noexcept(std::declval<Mat3F&>()[0]));
static_assert(noexcept(std::declval<Mat3F const&>().copy_to(std::declval<std::span<float, 9>>())));

void expect_matrix_near(Mat3D const& actual, Mat3D const& expected, double tolerance = 1e-12) {
  for (std::size_t r = 0; r != 3; ++r)
    for (std::size_t c = 0; c != 3; ++c)
      EXPECT_NEAR(actual[r][c], expected[r][c], tolerance) << r << ',' << c;
}

}

TEST(Matrix3, ScalarCapabilitiesAreOperationSpecific) {
  Matrix3<StoredOnly> stored;
  stored[1][2].value = 17;
  EXPECT_EQ(stored[1][2].value, 17);

  Matrix3<AddOnly> first, second;
  first[0][2].value = 4;
  second[0][2].value = 7;
  auto sum = first + second;
  EXPECT_EQ(sum[0][2].value, 11);

  Vector<ColumnSource, 3> source;
  source[0].value = 3;
  source[1].value = 5;
  source[2].value = 7;
  Matrix3<ColumnDestination> destination;
  destination.set_column(1, source);
  EXPECT_EQ(destination[0][1].value, 3);
  EXPECT_EQ(destination[1][1].value, 5);
  EXPECT_EQ(destination[2][1].value, 7);
}

TEST(Matrix3, ValueTransformsSupportCopyOnlyCoordinatesWithoutMoveAssignment) {
  Matrix3<CopyOnly> matrix;
  matrix[0][0].value = 2;
  matrix[0][1].value = 3;
  matrix[0][2].value = 5;
  matrix[1][0].value = -1;
  matrix[1][1].value = 4;
  matrix[1][2].value = 7;
  matrix[2][2].value = 1;
  Vector<CopyOnly, 2> input;
  input[0].value = 2;
  input[1].value = 3;
  auto point = matrix.transform_point(input);
  auto direction = matrix.transform_direction(input);
  auto multiplied = matrix * input;
  EXPECT_EQ(point[0].value, 18);
  EXPECT_EQ(point[1].value, 17);
  EXPECT_EQ(direction[0].value, 13);
  EXPECT_EQ(direction[1].value, 10);
  EXPECT_EQ(multiplied[0].value, 18);
  EXPECT_EQ(multiplied[1].value, 17);
  Vector<CopyOnly, 3> homogeneous;
  homogeneous[0].value = 2;
  homogeneous[1].value = 3;
  homogeneous[2].value = 1;
  auto product = matrix * homogeneous;
  EXPECT_EQ(product[0].value, 18);
  EXPECT_EQ(product[1].value, 17);
  EXPECT_EQ(product[2].value, 1);
}

TEST(Matrix3, ValueAccessAndCrossTypeConversionPreserveCoordinates) {
  Matrix3<int> original(1, 2, 3, 4, 5, 6, 7, 8, 9);
  Mat3D converted(original);
  EXPECT_EQ(converted.row(1), Vec3D(4, 5, 6));
  EXPECT_EQ(std::as_const(converted).column(2), Vec3D(3, 6, 9));
  EXPECT_EQ(converted.trace(), 15);
  EXPECT_EQ(converted.diagonal(), Vec3D(1, 5, 9));
  auto row = converted.row(0);
  row[0] = -1;
  EXPECT_EQ(converted[0][0], 1);
  Mat3F assigned;
  assigned = converted;
  EXPECT_EQ(assigned[2], Vec3F(7, 8, 9));
  assigned.set_row(2, Vec3D(10.25, 11.5, 12.75));
  EXPECT_EQ(assigned[2], Vec3F(10.25f, 11.5f, 12.75f));
  assigned.set_column(0, Vec3D(13, 14, 15));
  EXPECT_EQ(assigned.column(0), Vec3F(13, 14, 15));
  auto saved_temporary_row = Mat3D::identity()[1];
  EXPECT_EQ(saved_temporary_row, Vec3D(0, 1, 0));
  EXPECT_EQ(transpose(original), (Matrix3<int>(1, 4, 7, 2, 5, 8, 3, 6, 9)));
  EXPECT_EQ(original[0], (Vector<int, 3>(1, 2, 3)));
}

TEST(Matrix3, GeneralProductsAndSelfMultiplicationUseAllSourceValues) {
  Matrix3<int> a(2, 1, 3, 0, 3, 1, 1, 0, 4);
  Matrix3<int> b(1, 2, 0, 3, 1, 2, 2, 0, 1);
  EXPECT_EQ(a * b, (Matrix3<int>(11, 5, 5, 11, 3, 7, 9, 2, 4)));
  EXPECT_EQ((a * Vector<int, 3>(2, 3, 4)), (Vector<int, 3>(19, 13, 18)));
  a *= a;
  EXPECT_EQ(a, (Matrix3<int>(7, 5, 19, 1, 9, 7, 6, 1, 19)));
}

TEST(Matrix3, AliasedScalarAndColumnInputsAreSnapshottedBeforeWrites) {
  Matrix3<int> original(2, 4, 6, 8, 10, 12, 14, 16, 18);
  auto multiplied = original;
  multiplied *= multiplied[0][0];
  EXPECT_EQ(multiplied, (Matrix3<int>(4, 8, 12, 16, 20, 24, 28, 32, 36)));
  auto divided = original;
  divided /= divided[0][0];
  EXPECT_EQ(divided, (Matrix3<int>(1, 2, 3, 4, 5, 6, 7, 8, 9)));
  original.set_column(1, original[0]);
  EXPECT_EQ(original, (Matrix3<int>(2, 2, 6, 8, 4, 12, 14, 6, 18)));
}

TEST(Matrix3, CheckedInverseMatchesIndependentCofactorsAndPreservesSource) {
  Mat3D matrix(2, 1, 3, 0, 3, 1, 1, 0, 4);
  auto original = matrix;
  Mat3D expected(12.0/16, -4.0/16, -8.0/16, 1.0/16, 5.0/16, -2.0/16, -3.0/16, 1.0/16, 6.0/16);
  auto inverted = inverse(matrix);
  ASSERT_TRUE(inverted);
  expect_matrix_near(*inverted, expected);
  EXPECT_EQ(matrix, original);
  expect_matrix_near(matrix * *inverted, Mat3D::identity());
  expect_matrix_near(*inverted * matrix, Mat3D::identity());
  ASSERT_TRUE(matrix.invert());
  expect_matrix_near(matrix, expected);

  Mat3D pivoting(0, 1, 0, 1, 0, 0, 0, 0, -2);
  auto pivoted = pivoting.inverse();
  ASSERT_TRUE(pivoted);
  expect_matrix_near(*pivoted, Mat3D(0, 1, 0, 1, 0, 0, 0, 0, -0.5));
}

TEST(Matrix3, CheckedInverseHandlesRepresentableExtremeScalesAndTranslation) {
  for (double scale : {1e-150, -1e-150, 1e150, -1e150}) {
    Mat3D matrix(scale, 0, 0, 0, scale, 0, 0, 0, scale);
    auto result = matrix.inverse();
    ASSERT_TRUE(result);
    for (std::size_t i = 0; i != 3; ++i)
      EXPECT_NEAR((*result)[i][i] / (1 / scale), 1, 1e-14);
    expect_matrix_near(matrix * *result, Mat3D::identity());
  }
  Mat3D disparate(1e-200, 0, 0, 0, 1e200, 0, 0, 0, 1);
  auto result = disparate.inverse();
  ASSERT_TRUE(result);
  EXPECT_NEAR((*result)[0][0] / 1e200, 1, 1e-14);
  EXPECT_NEAR((*result)[1][1] / 1e-200, 1, 1e-14);
  expect_matrix_near(disparate * *result, Mat3D::identity());
  auto translation = Mat3D::translation(Vec2D(1e200, -1e200)).inverse();
  ASSERT_TRUE(translation);
  EXPECT_EQ((*translation)[0][2], -1e200);
  EXPECT_EQ((*translation)[1][2], 1e200);
}

TEST(Matrix3, SingularAndNonfiniteCheckedFailuresPreserveTarget) {
  for (Mat3D matrix : {Mat3D(), Mat3D(1, 2, 3, 2, 4, 6, 0, 0, 1)}) {
    auto saved = matrix;
    auto result = matrix.invert();
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<MathException>());
    EXPECT_EQ(matrix, saved);
  }
  for (auto bits : {0x7ff0000000000000ull, 0x7ff8000000000001ull}) {
    volatile std::uint64_t input = bits;
    Mat3D matrix = Mat3D::identity();
    matrix[1][2] = std::bit_cast<double>(static_cast<std::uint64_t>(input));
    std::array<double, 9> before, after;
    matrix.copy_to(before);
    EXPECT_FALSE(matrix.invert());
    EXPECT_FALSE(matrix.orthonormalize());
    EXPECT_FALSE(matrix.is_orthogonal(1e-10));
    matrix.copy_to(after);
    for (std::size_t i = 0; i != 9; ++i)
      EXPECT_EQ(std::bit_cast<std::uint64_t>(before[i]), std::bit_cast<std::uint64_t>(after[i]));
  }
  Mat3D overflowing(1e-310, 0, 0, 0, 1, 0, 0, 0, 1);
  auto saved = overflowing;
  EXPECT_FALSE(overflowing.invert());
  EXPECT_EQ(overflowing, saved);
}

TEST(Matrix3, OrthogonalityChecksFullGramMatrixAndAcceptsReflections) {
  EXPECT_FALSE(Mat3D(2, 0, 0, 0, 0.5, 0, 0, 0, 1).is_orthogonal(1e-12));
  EXPECT_FALSE(Mat3D(1, 1, 0, 0, 1, 0, 0, 0, 1).is_orthogonal(1e-12));
  Mat3D reflection(-1, 0, 0, 0, 1, 0, 0, 0, 1);
  EXPECT_TRUE(reflection.is_orthogonal(0));
  EXPECT_TRUE(Mat3D::rotation(0.7).is_orthogonal(1e-12));
  EXPECT_FALSE(reflection.is_orthogonal(-1));
  auto result = reflection.orthonormalized();
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, reflection);
  EXPECT_EQ(result->determinant(), -1);
}

TEST(Matrix3, OrthonormalizationProjectsThirdRowAgainstBothEarlierRows) {
  Mat3D matrix(1, 0, 0, 1, 1, 0, 1, 1, 1);
  auto original = matrix;
  auto result = orthonormalized(matrix);
  ASSERT_TRUE(result);
  expect_matrix_near(*result, Mat3D::identity());
  EXPECT_EQ(matrix, original);
  ASSERT_TRUE(matrix.orthonormalize());
  expect_matrix_near(matrix, Mat3D::identity());
  for (Mat3D invalid : {Mat3D(), Mat3D(1, 0, 0, 2, 0, 0, 0, 0, 1),
      Mat3D(1, 1, 1, 1, 1, 1, 0, 0, 1),
      Mat3D(1, 2, 3, 2, 4, 6, 3, 6, 9),
      Mat3D(1, 2, 3, 2, 0, 1, 3, 2, 4)}) {
    auto saved = invalid;
    EXPECT_FALSE(invalid.orthonormalize());
    EXPECT_EQ(invalid, saved);
  }
  Mat3D nearly_dependent(1, 0, 0, 1, 1e-8, 0, 0, 0, 1);
  EXPECT_TRUE(nearly_dependent.orthonormalized());
  EXPECT_TRUE(nearly_dependent.orthonormalized(0));
  EXPECT_TRUE(nearly_dependent.orthonormalized(1e-9));
  EXPECT_TRUE(orthonormalized(nearly_dependent, 1e-9));
  EXPECT_FALSE(orthonormalized(nearly_dependent, 1e-6));
  auto saved = nearly_dependent;
  EXPECT_FALSE(nearly_dependent.orthonormalize(1e-6));
  EXPECT_EQ(nearly_dependent, saved);
  EXPECT_FALSE(nearly_dependent.orthonormalize(-1));
  EXPECT_EQ(nearly_dependent, saved);
  Mat3D large(1e200, 0, 0, 1e200, 1e200, 0, 1e200, 1e200, -1e200);
  auto large_result = large.orthonormalized();
  ASSERT_TRUE(large_result);
  expect_matrix_near(*large_result, Mat3D(1, 0, 0, 0, 1, 0, 0, 0, -1));
}

TEST(Matrix3, AffineKernelsPreserveGeneralThirdRowAndLeftMultiplication) {
  Mat3D matrix(2, 1, 3, 0, 3, 1, 1, 0, 4);
  auto translated = matrix;
  translated.translate(Vec2D(5, -2));
  EXPECT_EQ(translated, Mat3D(7, 1, 23, -2, 3, -7, 1, 0, 4));
  auto scaled = matrix;
  scaled.scale(Vec2D(2, 3), Vec2D(1, 2));
  EXPECT_EQ(scaled, Mat3D(3, 2, 2, -4, 9, -13, 1, 0, 4));
  auto rotated = matrix;
  rotated.rotate(std::numbers::pi / 2, Vec2D(1, 2));
  expect_matrix_near(rotated, Mat3D(3, -3, 11, 3, 1, 7, 1, 0, 4));
}

TEST(Matrix3, MixedTransformsConvertOnlyFinalCoordinatesAndIgnoreDirectionTranslation) {
  Matrix3<int> integral(2, 3, 5, -1, 4, 7, 9, 8, 6);
  EXPECT_EQ(integral.transform_point(Vec2D(0.25, 0.5)), Vec2D(7, 8.75));
  EXPECT_EQ(integral.transform_direction(Vec2D(0.25, 0.5)), Vec2D(2, 1.75));
  EXPECT_EQ(integral * Vec2D(0.25, 0.5), (Vector<int, 2>(7, 8)));
  Mat3D fractional(0.75, 0.75, 0.75, -0.75, 0.75, -0.75, 0, 0, 1);
  EXPECT_EQ(fractional.transform_point(Vector<int, 2>(1, 1)), (Vector<int, 2>(2, 0)));
  EXPECT_EQ(fractional.transform_direction(Vector<int, 2>(1, 1)), (Vector<int, 2>(1, 0)));
  Mat3D rotation = Mat3D::rotation(0.4);
  rotation.translate(Vec2D(1e200, -1e200));
  EXPECT_NEAR(rotation.transform_angle(0.3), 0.7, 1e-14);
  EXPECT_FLOAT_EQ(rotation.transform_angle(0.3f), static_cast<float>(0.4 + static_cast<double>(0.3f)));
  Mat3D reflection(-1, 0, 100, 0, 1, 200, 0, 0, 1);
  EXPECT_NEAR(reflection.transform_angle(0.0), std::numbers::pi, 1e-14);
}

TEST(Matrix3, UncheckedGeometryRetainsLongDoublePrecision) {
  using Matrix = Matrix3<long double>;
  static_assert(!has_matrix_inverse<Matrix>);
  long double epsilon = std::numeric_limits<long double>::epsilon();
  long double angle = 1.0L + 16 * epsilon;
  auto rotation = Matrix::rotation(angle);
  EXPECT_LE(std::fabs(rotation[1][0] - std::sin(angle)), 2 * epsilon);
  EXPECT_LE(std::fabs(rotation[0][0] - std::cos(angle)), 2 * epsilon);
  Matrix rotated = Matrix::identity();
  rotated.rotate(angle);
  EXPECT_LE(std::fabs(rotated[1][0] - std::sin(angle)), 2 * epsilon);
  EXPECT_LE(std::fabs(rotated[0][0] - std::cos(angle)), 2 * epsilon);
  auto identity = Matrix::identity();
  static_assert(std::same_as<decltype(identity.transform_angle(angle)), long double>);
  EXPECT_LE(std::fabs(identity.transform_angle(angle) - angle), 4 * epsilon);
  auto offset = Matrix::rotation(0.125L);
  EXPECT_LE(std::fabs(offset.transform_angle(angle) - (angle + 0.125L)), 4 * epsilon);
  if constexpr (std::numeric_limits<long double>::digits > std::numeric_limits<double>::digits) {
    long double rounded_angle = static_cast<long double>(static_cast<double>(angle));
    EXPECT_GT(std::fabs(rotation[1][0] - std::sin(rounded_angle)), 2 * epsilon);
    EXPECT_GT(std::fabs(identity.transform_angle(angle) - rounded_angle), 4 * epsilon);
  }
}
