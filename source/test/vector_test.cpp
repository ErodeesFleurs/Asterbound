import std;

#include "gtest/gtest.h"

import star.array;
import star.byte_array;
import star.bytes;
import star.data_stream;
import star.data_stream_devices;
import star.data_stream_extra;
import star.format;
import star.hash;
import star.vector;

using namespace Star;

namespace {

struct unrelated {};

struct immobile {
  immobile() = default;
  immobile(immobile const&) = delete;
  immobile(immobile&&) = delete;
};

struct explicit_coordinate {
  int value = 0;
  explicit_coordinate() = default;
  explicit explicit_coordinate(explicit_coordinate const&) noexcept = default;
  explicit explicit_coordinate(explicit_coordinate&&) noexcept = default;
};

struct explicit_integer {
  explicit_integer() = default;
  explicit explicit_integer(int) {}
  explicit_integer& operator=(int) noexcept { return *this; }
};

struct non_default {
  non_default() = delete;
  explicit non_default(int) {}
};

struct unassignable {
  unassignable() = default;
  unassignable(unassignable const&) = default;
  unassignable& operator=(unassignable const&) = delete;
};

struct throwing_assignment {
  int value = 0;
  std::vector<int>* events = nullptr;
  bool destination = false;
  throwing_assignment() noexcept = default;
  throwing_assignment(throwing_assignment const&) noexcept = default;
  throwing_assignment& operator=(throwing_assignment const& other) noexcept(false) {
    if (destination && events)
      events->push_back(100 + other.value);
    value = other.value;
    events = other.events;
    return *this;
  }
  friend throwing_assignment operator+(throwing_assignment const& left,
                                       throwing_assignment const& right) noexcept(false) {
    throwing_assignment result;
    if (left.events)
      left.events->push_back(left.value);
    result.value = left.value + right.value;
    result.events = left.events;
    return result;
  }
};

struct throwing_default {
  throwing_default() noexcept(false) {}
};

struct const_lvalue_callback {
  std::unique_ptr<int> calls;
  std::vector<float>* seen;

  double operator()(float const& value) & {
    seen->push_back(value);
    return static_cast<double>(value) + 0.123456789012345 + (*calls)++;
  }
  double operator()(float&) & = delete;
  float operator()(float&&) & = delete;
  double operator()(float const&) && = delete;
};

struct mutable_lvalue_callback {
  std::unique_ptr<int> calls;
  Vec3I* source;
  std::vector<int>* seen;

  int operator()(int& value) & {
    EXPECT_EQ(&value, &(*source)[*calls]);
    seen->push_back(value);
    return value + ++*calls;
  }
  int operator()(int&) && = delete;
};

struct reference_callback {
  int slot = 0;
  constexpr int& operator()(int const& value) & noexcept {
    slot = value * 3;
    return slot;
  }
};

struct mutable_only_callback {
  int operator()(int&) const;
};

struct rvalue_only_callback {
  int operator()(int const&) &&;
};

struct void_callback {
  void operator()(int const&) const;
};

struct non_default_result_callback {
  non_default operator()(int const&) const;
};

struct unassignable_result_callback {
  unassignable const& operator()(int const&) const;
};

struct nothrow_callback {
  constexpr int operator()(int const& value) & noexcept { return value + 1; }
};

struct throwing_callback {
  int operator()(int const& value) & noexcept(false) { return value + 1; }
};

struct false_boolean_difference {
  int* calls;
  explicit operator bool() const noexcept { return false; }
  std::expected<Vec2D, StarException> operator()(Vec2D left, Vec2D right) & {
    ++*calls;
    return left - right;
  }
};

template <typename V, typename F>
concept can_transform = requires(V& value, F&& callback) { value.transform(std::forward<F>(callback)); };

template <typename V, typename F>
concept can_transformed = requires(V const& value, F&& callback) { value.transformed(std::forward<F>(callback)); };

template <typename V>
concept can_rotate = requires(V const& value) { value.rotate(0); };

template <typename V>
concept can_from_angles = requires { V::from_angles(0, 0); };

template <typename V>
concept can_polar = requires(V const& value) { value.to_polar(); };

template <typename V>
concept can_cartesian = requires(V const& value) { value.to_cartesian(); };

template <typename V>
concept has_x = requires(V const& value) { value.x(); };

template <typename V>
concept has_z = requires(V const& value) { value.z(); };

template <typename V>
concept has_w = requires(V const& value) { value.w(); };

template <typename V>
concept can_min = requires(V const& value) { value.min(); };

template <typename V>
concept can_max = requires(V const& value) { value.max(); };

template <typename V>
concept can_sum = requires(V const& value) { value.sum(); };

template <typename V>
concept can_product = requires(V const& value) { value.product(); };

template <typename V>
concept can_add = requires(V const& value) { value + value; };

template <typename V>
concept can_divide = requires(V const& value, typename V::value_type scalar) { value / scalar; };

template <typename V>
concept rejects_temporary_coordinates = !requires(V&& owner) { std::move(owner).x(); } && !requires(V&& owner) {
  std::move(owner).y();
} && !requires(V&& owner) { std::move(owner).z(); } && !requires(V&& owner) { std::move(owner).w(); };

static_assert(rejects_temporary_coordinates<Vector<immobile, 4>>);
static_assert(rejects_temporary_coordinates<Vector<immobile, 4> const>);
static_assert(rejects_temporary_coordinates<Vector<std::unique_ptr<int>, 4> const>);
static_assert(std::same_as<decltype(std::declval<Vector<std::unique_ptr<int>, 4>&&>().w()), std::unique_ptr<int>>);

static_assert(!std::constructible_from<Vec2I, int>);
static_assert(!std::constructible_from<Vec2I, int, int, int>);
static_assert(!std::constructible_from<Vector<int, 0>, int>);
static_assert(!std::constructible_from<Vec2I, int, unrelated>);
static_assert(!std::constructible_from<Vector<explicit_integer, 1>, int>);
static_assert(!std::constructible_from<Vector<unassignable, 1>, unassignable const&>);
static_assert(!std::constructible_from<Vector<non_default, 1>, non_default const&>);
static_assert(!std::constructible_from<Vec2I, Array<unrelated, 2> const&>);
static_assert(!std::constructible_from<Vec3I, Array<unrelated, 2> const&, int>);
static_assert(!std::constructible_from<Vec3I, Array<int, 2> const&, unrelated>);
static_assert(!std::constructible_from<Vector<int, 0>, Array<int, 0> const&, int>);
static_assert(!can_rotate<Vec3D>);
static_assert(!can_rotate<Vector<double, 1>>);
static_assert(!can_from_angles<Vec2D>);
static_assert(!can_from_angles<Vec4D>);
static_assert(!can_polar<Vec3D>);
static_assert(!can_cartesian<Vec3D>);
static_assert(!has_x<Vector<int, 1>>);
static_assert(!has_z<Vec2I>);
static_assert(!has_w<Vec3I>);
static_assert(!can_min<Vector<int, 0>>);
static_assert(!can_max<Vector<int, 0>>);
static_assert(!can_sum<Vector<int, 0>>);
static_assert(!can_product<Vector<int, 0>>);
static_assert(!can_add<Vector<unrelated, 2>>);
static_assert(!can_divide<Vector<unrelated, 2>>);
static_assert(!can_add<Vector<unrelated, 4>>);
static_assert(can_add<Vector<throwing_assignment, 4>>);
static_assert(!can_transformed<Vec3I, mutable_only_callback>);
static_assert(!can_transformed<Vec3I, rvalue_only_callback>);
static_assert(!can_transform<Vec3I, rvalue_only_callback>);
static_assert(!can_transformed<Vec3I, void_callback>);
static_assert(!can_transformed<Vec3I, non_default_result_callback>);
static_assert(!can_transformed<Vec3I, unassignable_result_callback>);
static_assert(std::same_as<decltype(std::declval<Vec2I&>().x()), int const&>);
static_assert(std::same_as<decltype(std::declval<Vec2I const&>().x()), int const&>);
static_assert(std::same_as<decltype(std::declval<Vec2I&&>().x()), int>);
static_assert(std::same_as<decltype(std::declval<Vec2I const&&>().x()), int>);
static_assert(
  std::same_as<decltype(std::declval<Vec3F const&>().transformed(std::declval<const_lvalue_callback>())), Vec3D>);
static_assert(std::same_as<decltype(std::declval<Vec3I const&>().transformed(reference_callback{})), Vec3I>);
static_assert(noexcept(Vec2I{}));
static_assert(noexcept(Vec2I(1, 2)));
static_assert(noexcept(std::declval<Vec2I const&>().to_size<4>()));
static_assert(noexcept(std::declval<Vec2I const&>() + std::declval<Vec2I const&>()));
static_assert(noexcept(std::declval<Vec2I const&>() * 2));
static_assert(noexcept(std::declval<Vec2I const&>().transformed(nothrow_callback{})));
static_assert(noexcept(std::declval<Vec2I&>().transform(nothrow_callback{})));
static_assert(!noexcept(Vector<throwing_default, 2>{}));
static_assert(!noexcept(Vector<throwing_assignment, 1>(std::declval<throwing_assignment const&>())));
static_assert(
  !noexcept(std::declval<Vector<throwing_assignment, 2>&>().fill(std::declval<throwing_assignment const&>())));
static_assert(!noexcept(std::declval<Vector<throwing_assignment, 2> const&>().to_size<3>()));
static_assert(!noexcept(std::declval<Vector<throwing_assignment, 2> const&>()
                        + std::declval<Vector<throwing_assignment, 2> const&>()));
static_assert(!noexcept(std::declval<Vec2I const&>().transformed(throwing_callback{})));
static_assert(!noexcept(std::declval<Vec2I&>().transform(throwing_callback{})));

constexpr bool constexpr_construction_fill_and_resize() {
  Vec3I zero;
  if (zero[0] != 0 || zero[1] != 0 || zero[2] != 0)
    return false;
  auto filled = Vec3I::filled(7);
  if (filled[0] != 7 || filled[1] != 7 || filled[2] != 7)
    return false;
  filled.fill(9);
  Array<short, 2> prefix(3, 5);
  Vec3I extended(prefix, 8);
  Vec2I converted(prefix);
  auto narrow_arguments = [](double first, double second) { return Vec2I(first, second); };
  auto converted_arguments = narrow_arguments(3.75, -2.75);
  Vector<int, 1> scalar(17);
  auto enlarged = converted.to_size<4>();
  auto shortened = extended.to_size<2>();
  auto empty = extended.to_size<0>();
  int const input[]{11, 13, 17};
  auto copied = Vec3I::copy_from(input);
  return filled[0] == 9 && filled[2] == 9 && extended[0] == 3 && extended[1] == 5 && extended[2] == 8
    && enlarged[0] == 3 && enlarged[1] == 5 && enlarged[2] == 0 && enlarged[3] == 0 && shortened[0] == 3
    && shortened[1] == 5 && empty.empty() && copied[0] == 11 && copied[2] == 17 && converted_arguments[0] == 3
    && converted_arguments[1] == -2 && scalar[0] == 17 && Vec2I(8, 13).x() == 8;
}

constexpr bool constexpr_arithmetic_and_callbacks() {
  Vec3I left(2, 4, 6);
  Vec3I right(1, 2, 3);
  auto added = left + right;
  auto subtracted = left - right;
  auto scaled = left * 3;
  auto divided = left / 2;
  auto negated = -right;
  auto multiplied = left.piecewise_multiply(right);
  auto quotients = left.piecewise_divide(right);
  left += right;
  left -= right;
  left *= 2;
  left /= 2;
  auto mapped = right.transformed(nothrow_callback{});
  right.transform(nothrow_callback{});
  return added[0] == 3 && added[2] == 9 && subtracted[1] == 2 && scaled[2] == 18 && divided[1] == 2 && negated[2] == -3
    && multiplied[2] == 18 && quotients[2] == 2 && left[2] == 6 && left * Vec3I(1, 2, 3) == 28 && left.sum() == 12
    && left.product() == 48 && left.min() == 2 && left.max() == 6 && mapped[0] == 2 && mapped[2] == 4 && right[2] == 4;
}

constexpr bool constexpr_float4_component_arithmetic() {
  Vec4F left(-3, 5, 0, 8);
  Vec4F const right(2, -4, 7, 0.5f);
  auto const added = left + right;
  auto const subtracted = left - right;
  auto const multiplied = left.piecewise_multiply(right);
  auto const scaled = left * -2.0f;
  auto const chained = (left + right).piecewise_multiply(right) - left * 2.0f;
  left += left;
  if (left[0] != -6 || left[1] != 10 || left[2] != 0 || left[3] != 16)
    return false;
  left -= left;
  left += right;
  left *= left[1];
  for (std::size_t i = 0; i != 4; ++i) {
    if (added[i] != std::array{-1.0f, 1.0f, 7.0f, 8.5f}[i]
        || subtracted[i] != std::array{-5.0f, 9.0f, -7.0f, 7.5f}[i]
        || multiplied[i] != std::array{-6.0f, -20.0f, 0.0f, 4.0f}[i]
        || scaled[i] != std::array{6.0f, -10.0f, 0.0f, -16.0f}[i]
        || chained[i] != std::array{4.0f, -14.0f, 49.0f, -11.75f}[i]
        || left[i] != std::array{-8.0f, 16.0f, -28.0f, -2.0f}[i])
      return false;
  }
  return true;
}

static_assert(constexpr_construction_fill_and_resize());
static_assert(constexpr_arithmetic_and_callbacks());
static_assert(constexpr_float4_component_arithmetic());

Vec4F runtime_float4(std::array<float, 4> const& values) {
  volatile float opaque[]{values[0], values[1], values[2], values[3]};
  return Vec4F(float(opaque[0]), float(opaque[1]), float(opaque[2]), float(opaque[3]));
}

void expect_float4_exact(Vec4F const& actual, std::array<float, 4> const& expected) {
  for (std::size_t i = 0; i != 4; ++i)
    EXPECT_EQ(actual[i], expected[i]) << "coordinate " << i;
}

template <std::floating_point T, std::size_t N>
void expect_vector_near(Vector<T, N> const& actual, Vector<T, N> const& expected, T tolerance) {
  for (std::size_t i = 0; i != N; ++i)
    EXPECT_NEAR(actual[i], expected[i], tolerance) << "coordinate " << i;
}

template <std::floating_point T> void polar_roundtrips() {
  using V = Vector<T, 2>;
  T const tolerance = T(64) * std::numeric_limits<T>::epsilon();
  for (auto const& cartesian : {V(3, 4), V(-5, 12), V(-3, -4), V(4, -3)}) {
    auto polar = cartesian.to_polar();
    EXPECT_NEAR(polar[0], std::atan2(cartesian[1], cartesian[0]), tolerance);
    EXPECT_NEAR(polar[1], cartesian.magnitude(), tolerance);
    expect_vector_near(polar.to_cartesian(), cartesian, tolerance);
  }
  expect_vector_near(V(0, 7).to_cartesian(), V(7, 0), tolerance);
}

template <std::floating_point T> void ned_and_enu_axes() {
  using V = Vector<T, 3>;
  T const half_pi = std::numbers::pi_v<T> / T(2);
  T const tolerance = T(32) * std::numeric_limits<T>::epsilon();
  V const north(1, 0, 0);
  V const east(0, 1, 0);
  V const down(0, 0, 1);
  expect_vector_near(V::from_angles(0, 0), north, tolerance);
  expect_vector_near(V::from_angles(half_pi, 0), east, tolerance);
  expect_vector_near(V::from_angles(0, half_pi), -down, tolerance);
  expect_vector_near(V::from_angles(0, -half_pi), down, tolerance);
  expect_vector_near(V::from_angles_enu(0, 0), east, tolerance);
  expect_vector_near(V::from_angles_enu(half_pi, 0), north, tolerance);
  expect_vector_near(V::from_angles_enu(0, half_pi), down, tolerance);
  expect_vector_near(V::from_angles_enu(0, -half_pi), -down, tolerance);
  EXPECT_NEAR(north.psi(), 0, tolerance);
  EXPECT_NEAR(east.psi(), half_pi, tolerance);
  EXPECT_NEAR(east.psi_enu(), 0, tolerance);
  EXPECT_NEAR(north.psi_enu(), half_pi, tolerance);
  for (T sign : {T(-1), T(1)}) {
    V pole(0, 0, sign * T(7));
    EXPECT_NEAR(pole.theta(), -sign * half_pi, tolerance);
    EXPECT_NEAR(pole.theta_enu(), sign * half_pi, tolerance);
    EXPECT_NEAR(pole.psi(), 0, tolerance);
    EXPECT_NEAR(pole.psi_enu(), 0, tolerance);
    auto eulers = pole.eulers();
    EXPECT_NEAR(eulers[0], 0, tolerance);
    EXPECT_NEAR(eulers[1], -sign * half_pi, tolerance);
  }
  V source(2, 3, 5);
  EXPECT_EQ(source.ned_to_enu(), V(3, 2, -5));
  EXPECT_EQ(source.ned_to_enu().enu_to_ned(), source);
  EXPECT_NEAR(V::angle(north, east), half_pi, tolerance);
  EXPECT_NEAR(north.angle_between(east), half_pi, tolerance);
  EXPECT_NEAR(north.angle_between_normalized(east), half_pi, tolerance);
}

}// namespace

TEST(VectorCoordinates, PolarCartesianRoundtripsFloatAndDouble) {
  polar_roundtrips<float>();
  polar_roundtrips<double>();
}

TEST(VectorCoordinates, NedEnuAxesAndSignedPolesFloatAndDouble) {
  ned_and_enu_axes<float>();
  ned_and_enu_axes<double>();
}

TEST(VectorCoordinates, FromAnglesRetainsDoublePrecisionAndRoundtrips) {
  double const psi = 0.123456789012345;
  double const theta = -0.234567890123456;
  auto ned = Vec3D::from_angles(psi, theta);
  Vec3D expected(std::cos(psi) * std::cos(theta), std::sin(psi) * std::cos(theta), -std::sin(theta));
  expect_vector_near(ned, expected, 2e-15);
  EXPECT_NEAR(ned.psi(), psi, 2e-15);
  EXPECT_NEAR(ned.theta(), theta, 2e-15);
  auto enu = Vec3D::from_angles_enu(psi, theta);
  expect_vector_near(enu, expected.ned_to_enu(), 2e-15);
  EXPECT_NEAR(enu.psi_enu(), psi, 2e-15);
  EXPECT_NEAR(enu.theta_enu(), theta, 2e-15);
}

TEST(VectorGeometry, AngleDifferenceRetainsRawBranchCutAndArgumentOrder) {
  Vec2D upper(-1, 1);
  Vec2D lower(-1, -1);
  double const turn = 3 * std::numbers::pi / 2;
  EXPECT_NEAR(Vec2D::angle_between_2(upper, lower), -turn, 1e-14);
  EXPECT_NEAR(Vec2D::angle_between_2(lower, upper), turn, 1e-14);
  EXPECT_NEAR(Vec2D::angle_formed_by_2(Vec2D(1, -1), Vec2D(0, 0), Vec2D(1, 1)), -turn, 1e-14);
}

TEST(VectorGeometry, ZeroNormalizationProjectionAndAnglePolicies) {
  Vec2D zero;
  Vec2D value(3, 4);
  EXPECT_EQ(zero.normalized(), zero);
  zero.normalize();
  EXPECT_EQ(zero, Vec2D(0, 0));
  EXPECT_EQ(value.project_onto(zero), zero);
  EXPECT_EQ(value.project_onto_normalized(zero), zero);
  EXPECT_EQ(zero.project_onto(value), zero);
  EXPECT_EQ(zero.to_polar(), zero);
  EXPECT_EQ(zero.to_cartesian(), zero);
  EXPECT_NEAR(zero.angle_between(value), std::numbers::pi / 2, 1e-14);
  EXPECT_NEAR(zero.angle_between_normalized(zero), std::numbers::pi / 2, 1e-14);
  EXPECT_NEAR(Vec3D::angle(Vec3D(), Vec3D(1, 0, 0)), std::numbers::pi / 2, 1e-14);
  EXPECT_EQ(Vec3D().theta(), 0.0);
  EXPECT_EQ(Vec3D().theta_enu(), 0.0);
}

TEST(VectorArithmetic, Float4RuntimeComponentsRetainExactFiniteValuesAcrossMagnitudes) {
  struct arithmetic_case {
    std::array<float, 4> left;
    std::array<float, 4> right;
    float scalar;
    std::array<float, 4> sum;
    std::array<float, 4> difference;
    std::array<float, 4> product;
    std::array<float, 4> scaled;
  };
  // Dyadic inputs and normal finite outputs keep these exact under fast-math.
  // Cover the smallest normal input, large exponents and integer precision edges.
  arithmetic_case const cases[]{
    {{-3.5f, 0.0f, 0x1p24f, 0x1p-60f}, {2.0f, -8.0f, -0x1p23f, 0x1p-60f}, -2.0f,
     {-1.5f, -8.0f, 0x1p23f, 0x1p-59f}, {-5.5f, 8.0f, 0x1.8p24f, 0.0f},
     {-7.0f, 0.0f, -0x1p47f, 0x1p-120f}, {7.0f, 0.0f, -0x1p25f, -0x1p-59f}},
    {{0x1p-126f, -0x1p120f, 8388607.0f, -0.25f}, {2.0f, 0.5f, 1.0f, 0.5f}, 2.0f,
     {2.0f, -0x1p120f, 8388608.0f, 0.25f}, {-2.0f, -0x1p120f, 8388606.0f, -0.75f},
     {0x1p-125f, -0x1p119f, 8388607.0f, -0.125f}, {0x1p-125f, -0x1p121f, 16777214.0f, -0.5f}}
  };
  for (auto const& input : cases) {
    auto const left = runtime_float4(input.left);
    auto const right = runtime_float4(input.right);
    volatile float opaque_scalar = input.scalar;
    float const scalar = opaque_scalar;
    expect_float4_exact(left + right, input.sum);
    expect_float4_exact(left - right, input.difference);
    expect_float4_exact(left.piecewise_multiply(right), input.product);
    expect_float4_exact(left * scalar, input.scaled);
    expect_float4_exact(left, input.left);
    expect_float4_exact(right, input.right);
  }
}

TEST(VectorArithmetic, FloatAlignedFloat4MembersPreserveNeighborsThroughAliasingAndChains) {
  struct alignas(16) guarded_vector {
    float before;
    Vec4F value;
    float after;
  };
  guarded_vector left{101.0f, runtime_float4({-3.0f, 5.0f, 0.0f, 8.0f}), 103.0f};
  guarded_vector right{107.0f, runtime_float4({2.0f, -4.0f, 7.0f, 0.5f}), 109.0f};
  guarded_vector output{113.0f, {}, 127.0f};
  for (auto const* holder : {&left, &right, &output}) {
    auto const address = reinterpret_cast<std::uintptr_t>(&holder->value);
    ASSERT_EQ(address % alignof(float), 0u);
    ASSERT_NE(address % 16, 0u);
  }
  auto expect_guards = [&] {
    EXPECT_EQ(left.before, 101.0f);
    EXPECT_EQ(left.after, 103.0f);
    EXPECT_EQ(right.before, 107.0f);
    EXPECT_EQ(right.after, 109.0f);
    EXPECT_EQ(output.before, 113.0f);
    EXPECT_EQ(output.after, 127.0f);
  };
  output.value = left.value + right.value;
  expect_float4_exact(output.value, {-1.0f, 1.0f, 7.0f, 8.5f});
  expect_guards();
  output.value = left.value - right.value;
  expect_float4_exact(output.value, {-5.0f, 9.0f, -7.0f, 7.5f});
  expect_guards();
  output.value = left.value.piecewise_multiply(right.value);
  expect_float4_exact(output.value, {-6.0f, -20.0f, 0.0f, 4.0f});
  expect_guards();
  output.value = left.value * -2.0f;
  expect_float4_exact(output.value, {6.0f, -10.0f, 0.0f, -16.0f});
  expect_guards();
  left.value += left.value;
  expect_float4_exact(left.value, {-6.0f, 10.0f, 0.0f, 16.0f});
  expect_guards();
  left.value -= left.value;
  expect_float4_exact(left.value, {0.0f, 0.0f, 0.0f, 0.0f});
  expect_guards();
  left.value = runtime_float4({-3.0f, 5.0f, 0.0f, 8.0f});
  left.value *= left.value[1];
  expect_float4_exact(left.value, {-15.0f, 25.0f, 0.0f, 40.0f});
  expect_guards();
  output.value = (left.value + right.value).piecewise_multiply(right.value) - left.value * 2.0f;
  expect_float4_exact(output.value, {4.0f, -134.0f, 49.0f, -59.75f});
  expect_float4_exact(left.value, {-15.0f, 25.0f, 0.0f, 40.0f});
  expect_float4_exact(right.value, {2.0f, -4.0f, 7.0f, 0.5f});
  expect_guards();
}

TEST(VectorArithmetic, CustomFloat4SizedElementsCalculateBeforeCompoundAssignment) {
  Vector<throwing_assignment, 4> source;
  std::vector<int> events;
  for (std::size_t i = 0; i != 4; ++i) {
    source[i].value = static_cast<int>(2 * i + 1);
    source[i].events = &events;
    source[i].destination = true;
  }
  source += source;
  EXPECT_EQ(events, (std::vector<int>{1, 3, 5, 7, 102, 106, 110, 114}));
  for (std::size_t i = 0; i != 4; ++i)
    EXPECT_EQ(source[i].value, static_cast<int>(4 * i + 2)) << "coordinate " << i;
}

TEST(VectorCallbacks, Float4CombineRetainsOrderedStatefulByValueInvocation) {
  auto const left = runtime_float4({-3.0f, 5.0f, 0.0f, 8.0f});
  auto const right = runtime_float4({2.0f, -4.0f, 7.0f, 0.5f});
  std::vector<std::pair<float, float>> seen;
  auto callback = [count = 0, &seen](float const& a, float const& b) mutable {
    seen.emplace_back(a, b);
    return a - b + ++count;
  };
  auto const result = left.combine(right, callback);
  expect_float4_exact(result, {-4.0f, 11.0f, -4.0f, 11.5f});
  EXPECT_EQ(seen, (std::vector<std::pair<float, float>>{{-3.0f, 2.0f}, {5.0f, -4.0f}, {0.0f, 7.0f}, {8.0f, 0.5f}}));
  EXPECT_EQ(callback(0.0f, 0.0f), 1.0f);
}

TEST(VectorArithmetic, AbsRetainsWideSignedIntegersAndUnsignedIdentity) {
  constexpr std::int64_t wide = 9007199254740993ll;
  Vector<std::int64_t, 3> signed_values(-wide, wide, -std::numeric_limits<std::int64_t>::max());
  auto absolute = signed_values.abs();
  EXPECT_EQ(absolute[0], wide);
  EXPECT_EQ(absolute[1], wide);
  EXPECT_EQ(absolute[2], std::numeric_limits<std::int64_t>::max());
  Vector<std::uint64_t, 3> unsigned_values(0, static_cast<std::uint64_t>(wide),
                                           std::numeric_limits<std::uint64_t>::max());
  EXPECT_EQ(unsigned_values.abs(), unsigned_values);
  EXPECT_EQ(Vec2D(-1.25, 2.5).abs(), Vec2D(1.25, 2.5));
}

TEST(VectorArithmetic, CrossAndTripleProductsRetainHandednessAndDeterminants) {
  Vec3I const north(1, 0, 0);
  Vec3I const east(0, 1, 0);
  EXPECT_EQ(north ^ east, Vec3I(0, 0, 1));
  EXPECT_EQ(east ^ north, Vec3I(0, 0, -1));
  EXPECT_EQ(Vec2I(2, 3) ^ Vec2I(5, 7), -1);
  EXPECT_EQ(Vec3I::triple_scalar_product(Vec3I(1, 2, 3), Vec3I(4, 5, 6), Vec3I(7, 8, 10)), -3);
}

TEST(VectorCoordinates, MovingTemporaryCoordinatesTransfersOwnershipWithoutBorrowing) {
  Vector<std::unique_ptr<int>, 4> source;
  source[0] = std::make_unique<int>(17);
  auto coordinate = std::move(source).x();
  EXPECT_FALSE(source[0]);
  ASSERT_TRUE(coordinate);
  EXPECT_EQ(*coordinate, 17);
}

TEST(VectorCoordinates, OwningCoordinatesSupportExplicitCopyAndMoveConstruction) {
  Vector<explicit_coordinate, 2> source;
  source[0].value = 23;
  auto moved = std::move(source).x();
  EXPECT_EQ(moved.value, 23);
  source[0].value = 41;
  auto copied = std::move(std::as_const(source)).x();
  EXPECT_EQ(copied.value, 41);
}

TEST(VectorCallbacks, TransformedInvokesMoveOnlyTemporaryAsLvalueWithConstElements) {
  Vec3F const source(0.25f, 1.5f, -2.0f);
  std::vector<float> seen;
  auto result = source.transformed(const_lvalue_callback{std::make_unique<int>(0), &seen});
  EXPECT_EQ(seen, (std::vector<float>{0.25f, 1.5f, -2.0f}));
  EXPECT_DOUBLE_EQ(result[0], 0.25 + 0.123456789012345);
  EXPECT_DOUBLE_EQ(result[1], 1.5 + 0.123456789012345 + 1);
  EXPECT_DOUBLE_EQ(result[2], -2.0 + 0.123456789012345 + 2);
  EXPECT_EQ(source, Vec3F(0.25f, 1.5f, -2.0f));
}

TEST(VectorCallbacks, TransformInvokesMoveOnlyTemporaryWithMutableSlotsInOrder) {
  Vec3I source(10, 20, 30);
  std::vector<int> seen;
  source.transform(mutable_lvalue_callback{std::make_unique<int>(0), &source, &seen});
  EXPECT_EQ(seen, (std::vector<int>{10, 20, 30}));
  EXPECT_EQ(source, Vec3I(11, 22, 33));
}

TEST(VectorCallbacks, ReferenceResultsOwnValuesAfterCallbackAndSourceExpire) {
  auto values = [] {
    Vec3I source(2, 5, 9);
    return source.transformed(reference_callback{});
  }();
  EXPECT_EQ(values, Vec3I(6, 15, 27));
  Vec2I source(7, 11);
  auto copied = source.transformed([](int const& value) -> int const& { return value; });
  source.fill(99);
  EXPECT_EQ(copied, Vec2I(7, 11));
}

TEST(VectorCallbacks, CheckedDifferenceInvokesBaThenBcAndPreservesRawAngle) {
  Vec2D const a(1, -1), b(0, 0), c(1, 1);
  std::vector<std::pair<Vec2D, Vec2D>> calls;
  std::function<std::expected<Vec2D, StarException>(Vec2D, Vec2D)> difference = [&](Vec2D left, Vec2D right) {
    calls.emplace_back(left, right);
    return std::expected<Vec2D, StarException>(left - right);
  };
  auto angle = Vec2D::angle_formed_by_2(a, b, c, difference);
  ASSERT_TRUE(angle);
  EXPECT_NEAR(*angle, -3 * std::numbers::pi / 2, 1e-14);
  EXPECT_EQ(calls, (std::vector<std::pair<Vec2D, Vec2D>>{{b, a}, {b, c}}));
}

TEST(VectorCallbacks, CheckedDifferenceStopsAtFirstErrorAndRetainsItsCategory) {
  for (int fail_on : {1, 2}) {
    int calls = 0;
    std::function<std::expected<Vec2D, StarException>(Vec2D, Vec2D)> difference =
      [&](Vec2D left, Vec2D right) -> std::expected<Vec2D, StarException> {
      if (++calls == fail_on) {
        if (fail_on == 1)
          return std::unexpected(IOException("first difference", false));
        return std::unexpected(OutOfRangeException("second difference", false));
      }
      return left - right;
    };
    auto angle = Vec2D::angle_formed_by_2(Vec2D(1, 2), Vec2D(3, 4), Vec2D(5, 6), difference);
    ASSERT_FALSE(angle);
    EXPECT_EQ(calls, fail_on);
    EXPECT_EQ(angle.error().is<IOException>(), fail_on == 1);
    EXPECT_EQ(angle.error().is<OutOfRangeException>(), fail_on == 2);
  }
}

TEST(VectorCallbacks, CheckedDifferenceRejectsEmptyFunctionAndNullFunctionPointer) {
  std::function<std::expected<Vec2D, StarException>(Vec2D, Vec2D)> empty;
  auto missing = Vec2D::angle_formed_by_2(Vec2D(1, 2), Vec2D(3, 4), Vec2D(5, 6), empty);
  EXPECT_FALSE(missing);
  using Difference = std::expected<Vec2D, StarException> (*)(Vec2D, Vec2D);
  Difference null_difference = nullptr;
  auto null_result = Vec2D::angle_formed_by_2(Vec2D(1, 2), Vec2D(3, 4), Vec2D(5, 6), null_difference);
  EXPECT_FALSE(null_result);
}

TEST(VectorCallbacks, CheckedDifferenceDoesNotTreatArbitraryCallableBoolAsEmptiness) {
  int calls = 0;
  false_boolean_difference difference{&calls};
  auto angle = Vec2D::angle_formed_by_2(Vec2D(1, -1), Vec2D(0, 0), Vec2D(1, 1), difference);
  ASSERT_TRUE(angle);
  EXPECT_EQ(calls, 2);
  EXPECT_NEAR(*angle, -3 * std::numbers::pi / 2, 1e-14);
}

TEST(VectorCompatibility, SerializationKeepsCoordinateBytesAndFollowingSentinel) {
  using V = Vector<std::uint16_t, 3>;
  V const source(0x1234, 0xabcd, 0x00ef);
  DataStreamBuffer stream;
  stream.set_byte_order(ByteOrder::BigEndian);
  ASSERT_TRUE(stream << source);
  ASSERT_TRUE(stream << std::uint8_t(0x7e));
  EXPECT_EQ(stream.data(), ByteArray({"\x12\x34\xab\xcd\x00\xef\x7e", 7}));
  ASSERT_TRUE(stream.seek(0));
  V decoded;
  std::uint8_t sentinel = 0;
  ASSERT_TRUE(stream >> decoded);
  ASSERT_TRUE(stream >> sentinel);
  EXPECT_EQ(decoded, source);
  EXPECT_EQ(sentinel, 0x7e);
  EXPECT_EQ(stream.pos(), 7u);
}

TEST(VectorCompatibility, HashRetainsOrderedArrayCoordinateCombination) {
  Vec3I value(17, -3, 29);
  std::size_t expected = 0;
  for (int coordinate : {17, -3, 29})
    hash_combine(expected, Star::hash<int>{}(coordinate));
  EXPECT_EQ(Star::hash<Vec3I>{}(value), expected);
  EXPECT_EQ(Star::hash<Vec3I>{}(value), (Star::hash<Array<int, 3>>{}(Array<int, 3>(17, -3, 29))));
}
