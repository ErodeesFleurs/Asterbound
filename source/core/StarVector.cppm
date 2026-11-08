module;
import std;
import star.hash;
import star.optional;

import star.array;
import star.format;

namespace Star {

namespace vector_detail {
// A function parameter tests the implicit conversion used by initialization and
// return, unlike constructible_from (which also accepts explicit conversions).
template <typename T> void accept(T) noexcept;

template <typename T>
constexpr T squared_value(T const& value) noexcept(noexcept(accept<T>(value * value)))
  requires requires { accept<T>(value * value); }
{
  return value * value;
}

using std::abs;

template <std::floating_point T> constexpr T absolute(T const& value) noexcept(noexcept(std::fabs(value))) {
  return std::fabs(value);
}

template <std::unsigned_integral T> constexpr T const& absolute(T const& value) noexcept { return value; }

template <std::signed_integral T> constexpr T absolute(T const& value) noexcept {
  // As with std::abs, the positive result must be representable in T.
  return value < 0 ? -value : value;
}

template <typename T>
  requires(!std::is_arithmetic_v<T> && requires(T const& value) { abs(value); })
constexpr auto absolute(T const& value) noexcept(noexcept(abs(value))) -> decltype(abs(value)) {
  return abs(value);
}

// The old typed multiplies/divides converted their result to T before assignment.
// Keep that conversion, while constraining the actual expression rather than an
// unconstrained standard function object's declaration.
template <typename T> struct multiply_as {
  constexpr T operator()(T const& a, T const& b) const noexcept(noexcept(accept<T>(a * b)))
    requires requires { accept<T>(a * b); }
  {
    return a * b;
  }
};

template <typename T> struct divide_as {
  constexpr T operator()(T const& a, T const& b) const noexcept(noexcept(accept<T>(a / b)))
    requires requires { accept<T>(a / b); }
  {
    return a / b;
  }
};
}// namespace vector_detail

template <typename T, typename Function>
concept vector_transform = requires(Function& function, T& element) { element = std::invoke(function, element); };

template <typename T, typename Function>
concept vector_combine = requires(Function& function, T const& left, T const& right, T& result) {
  result = std::invoke(function, left, right);
};

// std::min/max's default comparator returns bool; merely checking the
// unconstrained algorithm declaration would accept unusable element types.
template <typename T>
concept vector_less = requires(T const& left, T const& right) { vector_detail::accept<bool>(left < right); };

template <typename T, std::size_t N> class Vector : public Array<T, N> {
  using Storage = std::array<T, N>;
  static constexpr bool returnable = std::is_default_constructible_v<Storage> && std::is_move_constructible_v<Storage>;
  static constexpr bool nothrow_returnable =
    std::is_nothrow_default_constructible_v<Storage> && std::is_nothrow_move_constructible_v<Storage>;

public:
  using Base = Array<T, N>;

  static constexpr Vector filled(T const& value) noexcept(nothrow_returnable
                                                          && noexcept(std::declval<Base&>().fill(value)))
    requires(returnable && requires(Base& base) { base.fill(value); })
  {
    Vector result;
    result.fill(value);
    return result;
  }

  template <typename T2>
  static constexpr Vector floor(Vector<T2, N> const& value)
      noexcept(nothrow_returnable && requires(T& result, T2 const& source) { { result = std::floor(source) } noexcept; })
      requires (returnable && requires(T& result, T2 const& source) { result = std::floor(source); })
  {
    Vector result;
    for (std::size_t i = 0; i < N; ++i)
      result[i] = std::floor(value[i]);
    return result;
  }

  template <typename T2>
  static constexpr Vector ceil(Vector<T2, N> const& value)
      noexcept(nothrow_returnable && requires(T& result, T2 const& source) { { result = std::ceil(source) } noexcept; })
      requires (returnable && requires(T& result, T2 const& source) { result = std::ceil(source); })
  {
    Vector result;
    for (std::size_t i = 0; i < N; ++i)
      result[i] = std::ceil(value[i]);
    return result;
  }

  template <typename T2>
  static constexpr Vector round(Vector<T2, N> const& value)
      noexcept(nothrow_returnable && requires(T& result, T2 const& source) { { result = std::round(source) } noexcept; })
      requires (returnable && requires(T& result, T2 const& source) { result = std::round(source); })
  {
    Vector result;
    for (std::size_t i = 0; i < N; ++i)
      result[i] = std::round(value[i]);
    return result;
  }

  template <typename Iterator>
  static constexpr Vector copy_from(Iterator input)
      noexcept(nothrow_returnable && std::is_nothrow_destructible_v<Iterator>
          && (N == 0 || requires(Iterator& p, T* output) { { std::ranges::copy_n(std::move(p), N, output) } noexcept; }))
      requires (returnable && (N == 0 || array_copy_iterator<T, Iterator>))
  {
    Vector result;
    if constexpr (N != 0)
      std::ranges::copy_n(std::move(input), N, result.begin());
    return result;
  }

  // Array owns the storage and value-initializes every coordinate.
  constexpr Vector() noexcept(std::is_nothrow_default_constructible_v<Base>)
    requires std::is_default_constructible_v<Base>
  = default;

  explicit constexpr Vector(T const& first) noexcept(std::is_nothrow_constructible_v<Base, T const&>)
    requires(N == 1 && std::is_constructible_v<Base, T const&>)
      : Base(first) {}

  template <typename... Rest>
  constexpr Vector(T const& first,
                   Rest const&... rest) noexcept(std::is_nothrow_constructible_v<Base, T const&, Rest const&...>)
    requires(sizeof...(Rest) > 0 && sizeof...(Rest) + 1 == N && std::is_constructible_v<Base, T const&, Rest const&...>)
      : Base(first, rest...) {}

  template <typename T2>
  explicit constexpr Vector(Array<T2, N> const& value) noexcept(
    std::is_nothrow_constructible_v<Base, Array<T2, N> const&>)
    requires std::is_constructible_v<Base, Array<T2, N> const&>
      : Base(value) {}

  template <typename T2, std::size_t M, typename T3>
  constexpr Vector(Array<T2, M> const& prefix, T3 const& last)
      noexcept(std::is_nothrow_default_constructible_v<Base>
          && (M == 0 || requires(T& result, T2 const& value) { { result = value } noexcept; })
          && requires(T& result) { { result = last } noexcept; })
      requires (N > 0 && M == N - 1 && std::is_default_constructible_v<Base>
          && (M == 0 || array_assignable<T, T2>) && array_assignable<T, T3>)
  {
    if constexpr (M != 0) {
      for (std::size_t i = 0; i < M; ++i)
        (*this)[i] = prefix[i];
    }
    (*this)[M] = last;
  }

  template <std::size_t N2>
  constexpr Vector<T, N2> to_size() const
      noexcept(std::is_nothrow_default_constructible_v<std::array<T, N2>>
          && std::is_nothrow_move_constructible_v<std::array<T, N2>>
          && (N == 0 || N2 == 0 || requires(T& result, T const& source) { { result = source } noexcept; }))
      requires (std::is_default_constructible_v<std::array<T, N2>>
          && std::is_move_constructible_v<std::array<T, N2>> && (N == 0 || N2 == 0 || array_assignable<T, T>))
  {
    Vector<T, N2> result;
    if constexpr (N != 0 && N2 != 0) {
      for (std::size_t i = 0; i < std::min(N, N2); ++i)
        result[i] = (*this)[i];
    }
    return result;
  }

  constexpr Vector<T, 2> vec2() const noexcept(noexcept(this->template to_size<2>()))
    requires requires { this->template to_size<2>(); }
  {
    return to_size<2>();
  }
  constexpr Vector<T, 3> vec3() const noexcept(noexcept(this->template to_size<3>()))
    requires requires { this->template to_size<3>(); }
  {
    return to_size<3>();
  }
  constexpr Vector<T, 4> vec4() const noexcept(noexcept(this->template to_size<4>()))
    requires requires { this->template to_size<4>(); }
  {
    return to_size<4>();
  }

  template <typename Function>
  constexpr Vector combine(Vector const& other, Function function) const
      noexcept(nothrow_returnable && std::is_nothrow_destructible_v<Function>
          && requires(Function& f, T const& left, T const& right, T& result) {
            { result = std::invoke(f, left, right) } noexcept;
          })
      requires (returnable && vector_combine<T, Function>)
  {
    Vector result;
    for (std::size_t i = 0; i < N; ++i)
      result[i] = std::invoke(function, (*this)[i], other[i]);
    return result;
  }

  constexpr Vector piecewise_multiply(Vector const& other) const
    noexcept(noexcept(combine(other, vector_detail::multiply_as<T>{})))
    requires requires { combine(other, vector_detail::multiply_as<T>{}); }
  {
    return combine(other, vector_detail::multiply_as<T>{});
  }

  constexpr Vector piecewise_divide(Vector const& other) const
    noexcept(noexcept(combine(other, vector_detail::divide_as<T>{})))
    requires requires { combine(other, vector_detail::divide_as<T>{}); }
  {
    return combine(other, vector_detail::divide_as<T>{});
  }

  constexpr Vector piecewise_min(Vector const& other) const
      noexcept(nothrow_returnable && requires(T& result, T const& a, T const& b) { { result = std::min(a, b) } noexcept; })
      requires (returnable && vector_less<T> && array_assignable<T, T>)
  {
    Vector result;
    for (std::size_t i = 0; i < N; ++i)
      result[i] = std::min((*this)[i], other[i]);
    return result;
  }

  constexpr Vector piecewise_max(Vector const& other) const
      noexcept(nothrow_returnable && requires(T& result, T const& a, T const& b) { { result = std::max(a, b) } noexcept; })
      requires (returnable && vector_less<T> && array_assignable<T, T>)
  {
    Vector result;
    for (std::size_t i = 0; i < N; ++i)
      result[i] = std::max((*this)[i], other[i]);
    return result;
  }

  constexpr Vector piecewise_clamp(Vector const& lower, Vector const& upper) const
      noexcept(nothrow_returnable && requires(T& result, T const& a, T const& b, T const& c) {
        { result = std::max(std::min(a, c), b) } noexcept;
      })
      requires (returnable && vector_less<T> && array_assignable<T, T>)
  {
    Vector result;
    for (std::size_t i = 0; i < N; ++i)
      result[i] = std::max(std::min((*this)[i], upper[i]), lower[i]);
    return result;
  }

  constexpr T min() const
      noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_move_constructible_v<T>
          && requires(T& result, T const& value) { { result = std::min(result, value) } noexcept; })
      requires (N > 0 && std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
          && vector_less<T> && array_assignable<T, T>)
  {
    T result = (*this)[0];
    for (std::size_t i = 1; i < N; ++i)
      result = std::min(result, (*this)[i]);
    return result;
  }

  constexpr T max() const
      noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_move_constructible_v<T>
          && requires(T& result, T const& value) { { result = std::max(result, value) } noexcept; })
      requires (N > 0 && std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
          && vector_less<T> && array_assignable<T, T>)
  {
    T result = (*this)[0];
    for (std::size_t i = 1; i < N; ++i)
      result = std::max(result, (*this)[i]);
    return result;
  }

  constexpr T sum() const
      noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_move_constructible_v<T>
          && requires(T& result, T const& value) { { result += value } noexcept; })
      requires (N > 0 && std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
          && requires(T& result, T const& value) { result += value; })
  {
    T result = (*this)[0];
    for (std::size_t i = 1; i < N; ++i)
      result += (*this)[i];
    return result;
  }

  constexpr T product() const
      noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_move_constructible_v<T>
          && requires(T& result, T const& value) { { result *= value } noexcept; })
      requires (N > 0 && std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
          && requires(T& result, T const& value) { result *= value; })
  {
    T result = (*this)[0];
    for (std::size_t i = 1; i < N; ++i)
      result *= (*this)[i];
    return result;
  }

  constexpr T magnitude_squared() const
      noexcept(std::is_nothrow_move_constructible_v<T> && requires(T& result, T const& value) {
        { vector_detail::accept<T>(0) } noexcept;
        { result += vector_detail::squared_value(value) } noexcept;
      })
      requires (std::is_move_constructible_v<T> && requires(T& result, T const& value) {
        vector_detail::accept<T>(0);
        result += vector_detail::squared_value(value);
      })
  {
    T result = 0;
    for (std::size_t i = 0; i < N; ++i)
      result += vector_detail::squared_value((*this)[i]);
    return result;
  }

  T magnitude() const noexcept(noexcept(vector_detail::accept<T>(std::sqrt(magnitude_squared()))))
    requires requires { vector_detail::accept<T>(std::sqrt(magnitude_squared())); }
  {
    return std::sqrt(magnitude_squared());
  }

  void normalize()
      noexcept(noexcept(std::declval<Vector const&>().magnitude()) && requires(Vector& self, T& m) {
        { (m != 0) ? true : false } noexcept;
        { self = self / m } noexcept;
      })
      requires requires(Vector& self, T& m) {
        self.magnitude();
        (m != 0) ? true : false;
        self = self / m;
      }
  {
    T m = magnitude();
    if (m != 0)
      *this = *this / m;
  }

  Vector normalized() const
      noexcept(std::is_nothrow_copy_constructible_v<Storage> && noexcept(magnitude())
          && requires(Vector const& self, T& m) { { (m != 0) ? true : false } noexcept; { self / m } noexcept; })
      requires (std::is_copy_constructible_v<Storage> && requires(Vector const& self, T& m) {
        self.magnitude(); (m != 0) ? true : false; self / m;
      })
  {
    T m = magnitude();
    if (m != 0)
      return *this / m;
    return *this;
  }

  T angle_between(Vector const& other) const
    noexcept(noexcept(vector_detail::accept<T>(std::acos(normalized() * other.normalized()))))
    requires requires { vector_detail::accept<T>(std::acos(normalized() * other.normalized())); }
  {
    return std::acos(normalized() * other.normalized());
  }

  T angle_between_normalized(Vector const& other) const
    noexcept(noexcept(vector_detail::accept<T>(std::acos(*this * other))))
    requires requires { vector_detail::accept<T>(std::acos(*this * other)); }
  {
    return std::acos(*this * other);
  }

  constexpr Vector project_onto_normalized(Vector const& other) const noexcept(noexcept(((*this) * other) * other))
    requires requires { ((*this) * other) * other; }
  {
    return ((*this) * other) * other;
  }

  constexpr Vector project_onto(Vector const& other) const
      noexcept(nothrow_returnable && requires(Vector const& self, T& m) {
        { self.magnitude_squared() } noexcept; { (m != 0) ? true : false } noexcept;
        { self.project_onto_normalized(self) / m } noexcept;
      })
      requires (returnable && requires(Vector const& self, T& m) {
        self.magnitude_squared(); (m != 0) ? true : false; self.project_onto_normalized(self) / m;
      })
  {
    T m = other.magnitude_squared();
    if (m != 0)
      return project_onto_normalized(other) / m;
    return Vector();
  }

  constexpr void negate() noexcept(requires(T& value) {
    { value = -value } noexcept;
  })
    requires requires(T& value) { value = -value; }
  {
    for (std::size_t i = 0; i < N; ++i)
      (*this)[i] = -(*this)[i];
  }

  constexpr Vector operator-() const
    noexcept(std::is_nothrow_copy_constructible_v<Storage> && std::is_nothrow_move_constructible_v<Storage>
             && noexcept(std::declval<Vector&>().negate()))
    requires(std::is_copy_constructible_v<Storage> && std::is_move_constructible_v<Storage>
             && requires(Vector& value) { value.negate(); })
  {
    auto result = *this;
    result.negate();
    return result;
  }

  constexpr void reverse() noexcept(std::is_nothrow_swappable_v<T>)
    requires std::is_swappable_v<T>
  {
    std::reverse(this->begin(), this->end());
  }

  constexpr Vector abs() const
      noexcept(nothrow_returnable && requires(T& result, T const& source) { { result = vector_detail::absolute(source) } noexcept; })
      requires (returnable && requires(T& result, T const& source) { result = vector_detail::absolute(source); })
  {
    Vector result;
    for (std::size_t i = 0; i < N; ++i)
      result[i] = vector_detail::absolute((*this)[i]);
    return result;
  }

  constexpr Vector floor() const noexcept(noexcept(floor(*this)))
    requires requires { floor(*this); }
  {
    return floor(*this);
  }
  constexpr Vector ceil() const noexcept(noexcept(ceil(*this)))
    requires requires { ceil(*this); }
  {
    return ceil(*this);
  }
  constexpr Vector round() const noexcept(noexcept(round(*this)))
    requires requires { round(*this); }
  {
    return round(*this);
  }

  constexpr void fill(T const& value) noexcept(noexcept(std::declval<Base&>().fill(value)))
    requires requires(Base& base) { base.fill(value); }
  {
    Base::fill(value);
  }

  constexpr void clamp(T const& lower, T const& upper) noexcept(requires(T& value) {
    { value = std::max(lower, std::min(upper, value)) } noexcept;
  })
    requires(vector_less<T> && array_assignable<T, T>)
  {
    for (std::size_t i = 0; i < N; ++i)
      (*this)[i] = std::max(lower, std::min(upper, (*this)[i]));
  }

  template <typename Function>
  constexpr void transform(Function&& function) noexcept(requires(Function& f, T& value) {
    { value = std::invoke(f, value) } noexcept;
  })
    requires vector_transform<T, Function>
  {
    for (auto& element : *this)
      element = std::invoke(function, element);
  }

  template <typename Function>
    requires(
      std::is_invocable_v<Function&, T const&>
      && std::is_object_v<std::remove_cvref_t<std::invoke_result_t<Function&, T const&>>>
      && std::is_default_constructible_v<std::array<std::remove_cvref_t<std::invoke_result_t<Function&, T const&>>, N>>
      && std::is_move_constructible_v<std::array<std::remove_cvref_t<std::invoke_result_t<Function&, T const&>>, N>>
      && requires(Function& function, T const& source,
                  std::remove_cvref_t<std::invoke_result_t<Function&, T const&>>&
                    result) { result = std::invoke(function, source); })
  constexpr auto transformed(Function&& function) const
    noexcept(std::is_nothrow_default_constructible_v<
               std::array<std::remove_cvref_t<std::invoke_result_t<Function&, T const&>>, N>>
             && std::is_nothrow_move_constructible_v<
               std::array<std::remove_cvref_t<std::invoke_result_t<Function&, T const&>>, N>>
             && requires(Function& f, T const& source,
                         std::remove_cvref_t<std::invoke_result_t<Function&, T const&>>& result) {
                  { result = std::invoke(f, source) } noexcept;
                }) -> Vector<std::remove_cvref_t<std::invoke_result_t<Function&, T const&>>, N> {
    Vector<std::remove_cvref_t<std::invoke_result_t<Function&, T const&>>, N> result;
    for (std::size_t i = 0; i < N; ++i)
      result[i] = std::invoke(function, (*this)[i]);
    return result;
  }

  constexpr Vector operator+(Vector const& other) const noexcept(noexcept(combine(other, std::plus<>{})))
    requires requires { combine(other, std::plus<>{}); }
  {
    return combine(other, std::plus<>{});
  }

  constexpr Vector operator-(Vector const& other) const noexcept(noexcept(combine(other, std::minus<>{})))
    requires requires { combine(other, std::minus<>{}); }
  {
    return combine(other, std::minus<>{});
  }

  constexpr T operator*(Vector const& other) const
      noexcept(std::is_nothrow_move_constructible_v<T> && requires(T& result, T const& a, T const& b) {
        { vector_detail::accept<T>(0) } noexcept; { result += a * b } noexcept;
      })
      requires (std::is_move_constructible_v<T> && requires(T& result, T const& a, T const& b) {
        vector_detail::accept<T>(0); result += a * b;
      })
  {
    T result = 0;
    for (std::size_t i = 0; i < N; ++i)
      result += (*this)[i] * other[i];
    return result;
  }

  constexpr Vector operator*(T scalar) const
      noexcept(nothrow_returnable && std::is_nothrow_destructible_v<T>
          && requires(T& result, T const& value, T& s) { { result = value * s } noexcept; })
      requires (returnable && requires(T& result, T const& value, T& s) { result = value * s; })
  {
    Vector result;
    for (std::size_t i = 0; i < N; ++i)
      result[i] = (*this)[i] * scalar;
    return result;
  }

  constexpr Vector operator/(T scalar) const
      noexcept(nothrow_returnable && std::is_nothrow_destructible_v<T>
          && requires(T& result, T const& value, T& s) { { result = value / s } noexcept; })
      requires (returnable && requires(T& result, T const& value, T& s) { result = value / s; })
  {
    Vector result;
    for (std::size_t i = 0; i < N; ++i)
      result[i] = (*this)[i] / scalar;
    return result;
  }

  // Preserve temporary calculation followed by whole-vector assignment. Custom
  // elements may observe all calculations before the first destination write.
  constexpr Vector& operator+=(Vector const& other) noexcept(noexcept(*this = *this + other))
    requires requires { *this = *this + other; }
  {
    return *this = *this + other;
  }
  constexpr Vector& operator-=(Vector const& other) noexcept(noexcept(*this = *this - other))
    requires requires { *this = *this - other; }
  {
    return *this = *this - other;
  }
  constexpr Vector& operator*=(T scalar) noexcept(noexcept(*this = *this * scalar))
    requires requires { *this = *this * scalar; }
  {
    return *this = *this * scalar;
  }
  constexpr Vector& operator/=(T scalar) noexcept(noexcept(*this = *this / scalar))
    requires requires { *this = *this / scalar; }
  {
    return *this = *this / scalar;
  }

  // Two-dimensional geometry. C++23 standard trigonometric functions are not
  // constexpr; integer-coordinate instantiations retain their legacy narrowing.
  static Vector with_angle(T angle, T magnitude = 1) noexcept(noexcept(Vector(std::cos(angle) * magnitude,
                                                                              std::sin(angle) * magnitude)))
    requires(N == 2 && requires { Vector(std::cos(angle) * magnitude, std::sin(angle) * magnitude); })
  {
    return Vector(std::cos(angle) * magnitude, std::sin(angle) * magnitude);
  }

  T angle() const noexcept(noexcept(vector_detail::accept<T>(std::atan2((*this)[1], (*this)[0]))))
    requires(N == 2 && requires(T const& value) { vector_detail::accept<T>(std::atan2(value, value)); })
  {
    return std::atan2((*this)[1], (*this)[0]);
  }

  static T
  angle_between_2(Vector const& first,
                  Vector const& second) noexcept(noexcept(vector_detail::accept<T>(second.angle() - first.angle())))
    requires(N == 2 && requires { vector_detail::accept<T>(second.angle() - first.angle()); })
  {
    // This is deliberately the raw difference, not a principal atan2 angle.
    return second.angle() - first.angle();
  }

  static T angle_formed_by_2(Vector const& a, Vector const& b,
                             Vector const& c) noexcept(noexcept(angle_between_2(b - a, b - c)))
    requires(N == 2 && requires { angle_between_2(b - a, b - c); })
  {
    return angle_between_2(b - a, b - c);
  }

  [[nodiscard]] static std::expected<T, StarException>
  angle_formed_by_2(Vector const& a, Vector const& b, Vector const& c,
                    std::function<std::expected<Vector, StarException>(Vector, Vector)> const& difference)
    requires(N == 2 && std::is_copy_constructible_v<Storage> && requires { angle_between_2(a, b); })
  {
    if (!difference)
      return std::unexpected(StarException("Vector::angle_formed_by_2 requires a difference function"));
    auto ba = difference(b, a);
    if (!ba)
      return std::unexpected(std::move(ba.error()));
    auto bc = difference(b, c);
    if (!bc)
      return std::unexpected(std::move(bc.error()));
    return angle_between_2(*ba, *bc);
  }

  Vector rotate(T angle) const
      noexcept(requires(T& cosine, T& sine, T const& x, T const& y) {
        { vector_detail::accept<T>(std::cos(angle)) } noexcept;
        { vector_detail::accept<T>(std::sin(angle)) } noexcept;
        { Vector(x * cosine - y * sine, x * sine + y * cosine) } noexcept;
      })
      requires (N == 2 && requires(T& cosine, T& sine, T const& x, T const& y) {
        vector_detail::accept<T>(std::cos(angle)); vector_detail::accept<T>(std::sin(angle));
        Vector(x * cosine - y * sine, x * sine + y * cosine);
      })
  {
    T cosine = std::cos(angle);
    T sine = std::sin(angle);
    return Vector((*this)[0] * cosine - (*this)[1] * sine, (*this)[0] * sine + (*this)[1] * cosine);
  }

  constexpr Vector rot90() const noexcept(noexcept(Vector(-y(), x())))
    requires(N == 2 && requires { Vector(-this->y(), this->x()); })
  {
    return Vector(-y(), x());
  }

  Vector to_polar() const noexcept(noexcept(Vector(angle(), magnitude())))
    requires(N == 2 && requires { Vector(angle(), magnitude()); })
  {
    return Vector(angle(), magnitude());
  }

  Vector to_cartesian() const
    noexcept(noexcept(Vector(std::cos((*this)[0]) * (*this)[1], std::sin((*this)[0]) * (*this)[1])))
    requires(N == 2 && requires(T const& a, T const& r) { Vector(std::cos(a) * r, std::sin(a) * r); })
  {
    return Vector(std::cos((*this)[0]) * (*this)[1], std::sin((*this)[0]) * (*this)[1]);
  }

  // Lvalues borrow read-only coordinates; temporaries return owning values.
  constexpr T const& x() const& noexcept
    requires(N >= 2)
  {
    return (*this)[0];
  }
  constexpr T const& y() const& noexcept
    requires(N >= 2)
  {
    return (*this)[1];
  }
  constexpr T const& z() const& noexcept
    requires(N >= 3)
  {
    return (*this)[2];
  }
  constexpr T const& w() const& noexcept
    requires(N >= 4)
  {
    return (*this)[3];
  }

  constexpr T x() && noexcept(std::is_nothrow_move_constructible_v<T>)
    requires(N >= 2 && std::is_move_constructible_v<T>)
  {
    return T(std::move((*this)[0]));
  }
  constexpr T y() && noexcept(std::is_nothrow_move_constructible_v<T>)
    requires(N >= 2 && std::is_move_constructible_v<T>)
  {
    return T(std::move((*this)[1]));
  }
  constexpr T z() && noexcept(std::is_nothrow_move_constructible_v<T>)
    requires(N >= 3 && std::is_move_constructible_v<T>)
  {
    return T(std::move((*this)[2]));
  }
  constexpr T w() && noexcept(std::is_nothrow_move_constructible_v<T>)
    requires(N >= 4 && std::is_move_constructible_v<T>)
  {
    return T(std::move((*this)[3]));
  }

  constexpr T x() const&& noexcept(std::is_nothrow_copy_constructible_v<T>)
    requires(N >= 2 && std::is_copy_constructible_v<T>)
  {
    return T((*this)[0]);
  }
  constexpr T y() const&& noexcept(std::is_nothrow_copy_constructible_v<T>)
    requires(N >= 2 && std::is_copy_constructible_v<T>)
  {
    return T((*this)[1]);
  }
  constexpr T z() const&& noexcept(std::is_nothrow_copy_constructible_v<T>)
    requires(N >= 3 && std::is_copy_constructible_v<T>)
  {
    return T((*this)[2]);
  }
  constexpr T w() const&& noexcept(std::is_nothrow_copy_constructible_v<T>)
    requires(N >= 4 && std::is_copy_constructible_v<T>)
  {
    return T((*this)[3]);
  }

  // A non-copyable temporary must not fall back to the borrowing const& overload.
  T x() const&&
    requires(N >= 2 && !std::is_copy_constructible_v<T>)
  = delete;
  T y() const&&
    requires(N >= 2 && !std::is_copy_constructible_v<T>)
  = delete;
  T z() const&&
    requires(N >= 3 && !std::is_copy_constructible_v<T>)
  = delete;
  T w() const&&
    requires(N >= 4 && !std::is_copy_constructible_v<T>)
  = delete;

  constexpr void set_x(T const& value) noexcept(noexcept((*this)[0] = value))
    requires(N >= 2 && array_assignable<T, T>)
  {
    (*this)[0] = value;
  }
  constexpr void set_y(T const& value) noexcept(noexcept((*this)[1] = value))
    requires(N >= 2 && array_assignable<T, T>)
  {
    (*this)[1] = value;
  }
  constexpr void set_z(T const& value) noexcept(noexcept((*this)[2] = value))
    requires(N >= 3 && array_assignable<T, T>)
  {
    (*this)[2] = value;
  }
  constexpr void set_w(T const& value) noexcept(noexcept((*this)[3] = value))
    requires(N >= 4 && array_assignable<T, T>)
  {
    (*this)[3] = value;
  }

  static Vector from_angles(T psi, T theta)
      noexcept(requires(T& cosine, T& x, T& y) {
        { T(std::cos(theta)) } noexcept; { T(std::cos(psi)) } noexcept; { T(std::sin(psi)) } noexcept;
        { x *= cosine } noexcept; { y *= cosine } noexcept; { Vector(x, y, T(-std::sin(theta))) } noexcept;
      })
      requires (N == 3 && requires(T& cosine, T& x, T& y) {
        T(std::cos(theta)); T(std::cos(psi)); T(std::sin(psi));
        x *= cosine; y *= cosine; Vector(x, y, T(-std::sin(theta)));
      })
  {
    T cosine = T(std::cos(theta));
    T x = T(std::cos(psi));
    T y = T(std::sin(psi));
    x *= cosine;
    y *= cosine;
    return Vector(x, y, T(-std::sin(theta)));
  }

  static Vector from_angles_enu(T psi, T theta) noexcept(noexcept(from_angles(psi, theta))
                                                         && noexcept(std::declval<Vector const&>().ned_to_enu()))
    requires(N == 3 && requires { from_angles(psi, theta).ned_to_enu(); })
  {
    Vector value = from_angles(psi, theta);
    return Vector(value.y(), value.x(), -value.z());
  }

  static constexpr T triple_scalar_product(Vector const& a, Vector const& b,
                                           Vector const& c) noexcept(noexcept(a * (b ^ c)))
    requires(N == 3 && requires { a * (b ^ c); })
  {
    return a * (b ^ c);
  }

  static T angle(Vector const& first, Vector const& second) noexcept(
    noexcept(vector_detail::accept<T>(std::acos(std::min(first.normalized() * second.normalized(), T(1))))))
    requires(
      N == 3 && vector_less<T>
      && requires { vector_detail::accept<T>(std::acos(std::min(first.normalized() * second.normalized(), T(1)))); })
  {
    return std::acos(std::min(first.normalized() * second.normalized(), T(1)));
  }

  T psi() const
      noexcept(noexcept(normalized()) && requires(T const& value) {
        { T(std::fabs(value)) } noexcept; { (T(std::fabs(value)) > 0.99999) ? true : false } noexcept;
        { vector_detail::accept<T>(0.0) } noexcept; { T(std::atan2(value, value)) } noexcept;
      })
      requires (N == 3 && requires(T const& value) {
        normalized(); T(std::fabs(value)); (T(std::fabs(value)) > 0.99999) ? true : false;
        vector_detail::accept<T>(0.0); T(std::atan2(value, value));
      })
  {
    Vector value = normalized();
    T absolute_z = T(std::fabs(value.z()));
    if (absolute_z > 0.99999)
      return 0.0;
    return T(std::atan2(value.y(), value.x()));
  }

  T theta() const
      noexcept(noexcept(normalized()) && requires(T const& value) {
        { vector_detail::accept<T>(std::fabs(value)) } noexcept;
        { (value > 0.99999) ? true : false } noexcept; { (value > 0.0) ? true : false } noexcept;
        { T(-std::numbers::pi / 2) } noexcept; { T(std::numbers::pi / 2) } noexcept;
        { vector_detail::accept<T>(std::asin(-value)) } noexcept;
      })
      requires (N == 3 && requires(T const& value) {
        normalized(); vector_detail::accept<T>(std::fabs(value));
        (value > 0.99999) ? true : false; (value > 0.0) ? true : false;
        T(-std::numbers::pi / 2); T(std::numbers::pi / 2); vector_detail::accept<T>(std::asin(-value));
      })
  {
    Vector value = normalized();
    T absolute_z = std::fabs(value.z());
    if (absolute_z > 0.99999)
      return value.z() > 0.0 ? T(-std::numbers::pi / 2) : T(std::numbers::pi / 2);
    return std::asin(-value.z());
  }

  Vector<T, 2> eulers() const
      noexcept(noexcept(normalized()) && requires(T& psi, T& theta, T const& value) {
        { T() } noexcept; { vector_detail::accept<T>(std::fabs(value)) } noexcept;
        { (value > 0.99999) ? true : false } noexcept; { (value > 0.0) ? true : false } noexcept;
        { psi = 0.0 } noexcept;
        { theta = value > 0.0 ? -std::numbers::pi / 2 : std::numbers::pi / 2 } noexcept;
        { psi = std::atan2(value, value) } noexcept; { theta = std::asin(-value) } noexcept;
        { Vector<T, 2>(psi, theta) } noexcept;
      })
      requires (N == 3 && std::is_default_constructible_v<T> && requires(T& psi, T& theta, T const& value) {
        normalized(); vector_detail::accept<T>(std::fabs(value));
        (value > 0.99999) ? true : false; (value > 0.0) ? true : false;
        psi = 0.0; theta = value > 0.0 ? -std::numbers::pi / 2 : std::numbers::pi / 2;
        psi = std::atan2(value, value); theta = std::asin(-value); Vector<T, 2>(psi, theta);
      })
  {
    T psi, theta;
    Vector value = normalized();
    T absolute_z = std::fabs(value.z());
    if (absolute_z > 0.99999) {
      psi = 0.0;
      theta = value.z() > 0.0 ? -std::numbers::pi / 2 : std::numbers::pi / 2;
    } else {
      psi = std::atan2(value.y(), value.x());
      theta = std::asin(-value.z());
    }
    return Vector<T, 2>(psi, theta);
  }

  T psi_enu() const
      noexcept(noexcept(normalized()) && requires(T const& value) {
        { vector_detail::accept<T>(std::fabs(value)) } noexcept;
        { (value > 0.99999) ? true : false } noexcept;
        { vector_detail::accept<T>(0.0) } noexcept; { vector_detail::accept<T>(std::atan2(value, value)) } noexcept;
      })
      requires (N == 3 && requires(T const& value) {
        normalized(); vector_detail::accept<T>(std::fabs(value)); (value > 0.99999) ? true : false;
        vector_detail::accept<T>(0.0); vector_detail::accept<T>(std::atan2(value, value));
      })
  {
    Vector value = normalized();
    T absolute_z = std::fabs(value.z());
    if (absolute_z > 0.99999)
      return 0.0;
    return std::atan2(value.x(), value.y());
  }

  T theta_enu() const
      noexcept(noexcept(normalized()) && requires(T const& value) {
        { vector_detail::accept<T>(std::fabs(value)) } noexcept;
        { (value > 0.99999) ? true : false } noexcept; { (value > 0.0) ? true : false } noexcept;
        { vector_detail::accept<T>(std::numbers::pi / 2) } noexcept;
        { vector_detail::accept<T>(-std::numbers::pi / 2) } noexcept;
        { vector_detail::accept<T>(std::asin(value)) } noexcept;
      })
      requires (N == 3 && requires(T const& value) {
        normalized(); vector_detail::accept<T>(std::fabs(value));
        (value > 0.99999) ? true : false; (value > 0.0) ? true : false;
        vector_detail::accept<T>(std::numbers::pi / 2); vector_detail::accept<T>(-std::numbers::pi / 2);
        vector_detail::accept<T>(std::asin(value));
      })
  {
    Vector value = normalized();
    T absolute_z = std::fabs(value.z());
    if (absolute_z > 0.99999)
      return value.z() > 0.0 ? std::numbers::pi / 2 : -std::numbers::pi / 2;
    return std::asin(value.z());
  }

  constexpr Vector ned_to_enu() const noexcept(noexcept(Vector(y(), x(), -z())))
    requires(N == 3 && requires { Vector(y(), x(), -z()); })
  {
    return Vector(y(), x(), -z());
  }

  constexpr Vector enu_to_ned() const noexcept(noexcept(ned_to_enu()))
    requires(N == 3 && requires { ned_to_enu(); })
  {
    return ned_to_enu();
  }

  using Base::empty;
  using Base::size;
};

using Vec2I = Vector<int, 2>;
using Vec2U = Vector<unsigned, 2>;
using Vec2F = Vector<float, 2>;
using Vec2D = Vector<double, 2>;
using Vec2B = Vector<std::uint8_t, 2>;
using Vec2S = Vector<std::size_t, 2>;
using Vec3I = Vector<int, 3>;
using Vec3U = Vector<unsigned, 3>;
using Vec3F = Vector<float, 3>;
using Vec3D = Vector<double, 3>;
using Vec3B = Vector<std::uint8_t, 3>;
using Vec3S = Vector<std::size_t, 3>;
using Vec4I = Vector<int, 4>;
using Vec4U = Vector<unsigned, 4>;
using Vec4F = Vector<float, 4>;
using Vec4D = Vector<double, 4>;
using Vec4B = Vector<std::uint8_t, 4>;
using Vec4S = Vector<std::size_t, 4>;

template <typename T, std::size_t N>
  requires array_streamable<T>
std::ostream& operator<<(std::ostream& output, Vector<T, N> const& vector) {
  output << '(';
  for (std::size_t i = 0; i < N; ++i) {
    if constexpr (requires { outputOptional(vector[i]); })
      output << outputOptional(vector[i]);
    else
      output << vector[i];
    if (i != N - 1)
      output << ", ";
  }
  output << ')';
  return output;
}

template <typename T, std::size_t N>
  requires requires(Vector<T, N>& vector, T& scalar) { vector * scalar; }
constexpr Vector<T, N> operator*(T scalar, Vector<T, N> vector) noexcept(noexcept(vector * scalar)) {
  return vector * scalar;
}

template <typename T, std::size_t N>
  requires requires(Vector<T, N>& vector) { vector.normalized(); }
Vector<T, N> vnorm(Vector<T, N> vector) noexcept(noexcept(vector.normalized())) {
  return vector.normalized();
}

template <typename T, std::size_t N>
  requires requires(Vector<T, N> const& vector) { vector.magnitude(); }
T vmag(Vector<T, N> const& vector) noexcept(noexcept(vector.magnitude())) {
  return vector.magnitude();
}

template <typename T, std::size_t N>
  requires requires(Vector<T, N> const& vector) { vector.magnitude_squared(); }
constexpr T vmag_squared(Vector<T, N> const& vector) noexcept(noexcept(vector.magnitude_squared())) {
  return vector.magnitude_squared();
}

template <typename T, std::size_t N>
  requires requires(Vector<T, N> const& a, Vector<T, N> const& b) { a.piecewise_min(b); }
constexpr Vector<T, N> vmin(Vector<T, N> const& a, Vector<T, N> const& b) noexcept(noexcept(a.piecewise_min(b))) {
  return a.piecewise_min(b);
}

template <typename T, std::size_t N>
  requires requires(Vector<T, N> const& a, Vector<T, N> const& b) { a.piecewise_max(b); }
constexpr Vector<T, N> vmax(Vector<T, N> const& a, Vector<T, N> const& b) noexcept(noexcept(a.piecewise_max(b))) {
  return a.piecewise_max(b);
}

template <typename T, std::size_t N>
  requires requires(Vector<T, N> const& a, Vector<T, N> const& b, Vector<T, N> const& c) { a.piecewise_clamp(b, c); }
constexpr Vector<T, N> vclamp(Vector<T, N> const& a, Vector<T, N> const& lower,
                              Vector<T, N> const& upper) noexcept(noexcept(a.piecewise_clamp(lower, upper))) {
  return a.piecewise_clamp(lower, upper);
}

template <typename VectorType>
  requires requires(VectorType const& a, VectorType const& b) {
    vector_detail::accept<VectorType>(a.piecewise_multiply(b));
  }
constexpr VectorType
vmult(VectorType const& a,
      VectorType const& b) noexcept(noexcept(vector_detail::accept<VectorType>(a.piecewise_multiply(b)))) {
  return a.piecewise_multiply(b);
}

template <typename VectorType>
  requires requires(VectorType const& a, VectorType const& b) {
    vector_detail::accept<VectorType>(a.piecewise_divide(b));
  }
constexpr VectorType
vdiv(VectorType const& a,
     VectorType const& b) noexcept(noexcept(vector_detail::accept<VectorType>(a.piecewise_divide(b)))) {
  return a.piecewise_divide(b);
}

// Cross products retain their original operand and result construction order.
template <typename T>
  requires requires(T& a, T& b) { Vector<T, 3>(a * b - a * b, a * b - a * b, a * b - a * b); }
constexpr Vector<T, 3> operator^(Vector<T, 3> a, Vector<T, 3> b) noexcept(
  noexcept(Vector<T, 3>(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]))) {
  return Vector<T, 3>(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
}

template <typename T>
  requires requires(T const& a, T const& b) { vector_detail::accept<T>(a * b - a * b); }
constexpr T operator^(Vector<T, 2> const& a,
                      Vector<T, 2> const& b) noexcept(noexcept(vector_detail::accept<T>(a[0] * b[1] - a[1] * b[0]))) {
  return a[0] * b[1] - a[1] * b[0];
}

template <typename T, std::size_t N> struct hash<Vector<T, N>> : hash<Array<T, N>> {};
}

export module star.vector;

export namespace Star {
  template <typename T, std::size_t N>
  struct hash<Vector<T, N>>;
}

export namespace Star {
using ::Star::vclamp;
using ::Star::vdiv;
using ::Star::Vec2B;
using ::Star::Vec2D;
using ::Star::Vec2F;
using ::Star::Vec2I;
using ::Star::Vec2S;
using ::Star::Vec2U;
using ::Star::Vec3B;
using ::Star::Vec3D;
using ::Star::Vec3F;
using ::Star::Vec3I;
using ::Star::Vec3S;
using ::Star::Vec3U;
using ::Star::Vec4B;
using ::Star::Vec4D;
using ::Star::Vec4F;
using ::Star::Vec4I;
using ::Star::Vec4S;
using ::Star::Vec4U;
using ::Star::Vector;
using ::Star::vector_combine;
using ::Star::vector_less;
using ::Star::vector_transform;
using ::Star::vmag;
using ::Star::vmag_squared;
using ::Star::vmax;
using ::Star::vmin;
using ::Star::vmult;
using ::Star::vnorm;
using ::Star::operator<<;
using ::Star::operator*;
using ::Star::operator^;
using ::Star::hash;
}
