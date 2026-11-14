module;

import std;
import star.text_formatter;
import star.format;
import star.array;
import star.math_common;
import star.vector;

namespace Star {

// Constraints follow the scalar expressions used by each operation. Merely
// storing a scalar does not require arithmetic, equality or regularity.
template <typename T>
concept matrix_result = std::is_default_constructible_v<Array<Vector<T, 3>, 3>>
  && std::is_move_constructible_v<Array<Vector<T, 3>, 3>>;

template <typename T, typename U>
concept matrix_convertible = requires(T& destination, U const& source) { destination = static_cast<T>(source); };

template <typename T, typename U>
concept matrix_product = requires(T const& a, U const& b) {
  static_cast<T>(a * b + a * b + a * b);
};

template <typename R, typename T, typename U>
concept matrix_point_transform = requires(T const& a, U const& b) {
  static_cast<R>(a * b + a * b + a);
};

template <typename R, typename T, typename U>
concept matrix_direction_transform = requires(T const& a, U const& b) {
  static_cast<R>(a * b + a * b);
};

template <typename T>
concept matrix_checked_scalar = std::same_as<T, float> || std::same_as<T, double>;

template <typename T>
class Matrix3 {
public:
  using Vec3 = Vector<T, 3>;
  using Vec2 = Vector<T, 2>;
  using Rows = Array<Vec3, 3>;

  constexpr Matrix3() noexcept(std::is_nothrow_default_constructible_v<Rows>)
    requires std::is_default_constructible_v<Rows>
  = default;

  constexpr Matrix3(T const& a, T const& b, T const& c, T const& d, T const& e, T const& f,
      T const& g, T const& h, T const& i)
    noexcept(std::is_nothrow_constructible_v<Vec3, T const&, T const&, T const&>
      && std::is_nothrow_constructible_v<Rows, Vec3 const&, Vec3 const&, Vec3 const&>)
    requires (std::is_constructible_v<Vec3, T const&, T const&, T const&>
      && std::is_constructible_v<Rows, Vec3 const&, Vec3 const&, Vec3 const&>)
    : m_rows(Vec3(a, b, c), Vec3(d, e, f), Vec3(g, h, i)) {}

  constexpr Matrix3(Vec3 const& a, Vec3 const& b, Vec3 const& c)
    noexcept(std::is_nothrow_constructible_v<Rows, Vec3 const&, Vec3 const&, Vec3 const&>)
    requires std::is_constructible_v<Rows, Vec3 const&, Vec3 const&, Vec3 const&>
    : m_rows(a, b, c) {}

  explicit constexpr Matrix3(std::span<T const, 9> values)
    noexcept(noexcept(Matrix3(values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[8])))
    requires std::is_constructible_v<Vec3, T const&, T const&, T const&>
      && std::is_constructible_v<Rows, Vec3 const&, Vec3 const&, Vec3 const&>
    : Matrix3(values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[8]) {}

  template <typename U>
  constexpr Matrix3(Matrix3<U> const& source)
    noexcept(std::is_nothrow_default_constructible_v<Rows>
      && requires(T& destination, U const& value) { { destination = static_cast<T>(value) } noexcept; })
    requires (std::is_default_constructible_v<Rows> && matrix_convertible<T, U>)
  {
    *this = source;
  }

  template <typename U>
  constexpr Matrix3& operator=(Matrix3<U> const& source)
    noexcept(requires(T& destination, U const& value) { { destination = static_cast<T>(value) } noexcept; })
    requires matrix_convertible<T, U>
  {
    for (std::size_t r = 0; r != 3; ++r)
      for (std::size_t c = 0; c != 3; ++c)
        m_rows[r][c] = static_cast<T>(source[r][c]);
    return *this;
  }

  // Row-major indexing. Temporary matrices never expose dangling row borrows.
  [[nodiscard]] constexpr Vec3& operator[](std::size_t i) & noexcept { return m_rows[i]; }
  [[nodiscard]] constexpr Vec3 const& operator[](std::size_t i) const& noexcept { return m_rows[i]; }
  [[nodiscard]] constexpr Vec3 operator[](std::size_t i) && noexcept(std::is_nothrow_move_constructible_v<Vec3>)
    requires std::is_move_constructible_v<Vec3>
  { return std::move(m_rows[i]); }
  [[nodiscard]] constexpr Vec3 operator[](std::size_t i) const&& noexcept(std::is_nothrow_copy_constructible_v<Vec3>)
    requires std::is_copy_constructible_v<Vec3>
  { return m_rows[i]; }

  constexpr void copy_to(std::span<T, 9> destination) const
    noexcept(requires(T& output, T const& value) { { output = value } noexcept; })
    requires requires(T& output, T const& value) { output = value; }
  {
    // Rows are separate arrays: no pointer arithmetic crosses their bounds.
    for (std::size_t r = 0; r != 3; ++r)
      for (std::size_t c = 0; c != 3; ++c)
        destination[r * 3 + c] = m_rows[r][c];
  }

  [[nodiscard]] constexpr Vec3 row(std::size_t i) const noexcept(std::is_nothrow_copy_constructible_v<Vec3>)
    requires std::is_copy_constructible_v<Vec3>
  { return m_rows[i]; }

  [[nodiscard]] constexpr Vec3 column(std::size_t i) const
    noexcept(std::is_nothrow_constructible_v<Vec3, T const&, T const&, T const&>)
    requires std::is_constructible_v<Vec3, T const&, T const&, T const&>
  { return Vec3(m_rows[0][i], m_rows[1][i], m_rows[2][i]); }

  template <typename U>
  constexpr void set_row(std::size_t i, Vector<U, 3> const& value)
    noexcept(noexcept(Vec3(value)) && std::is_nothrow_move_assignable_v<Vec3>)
    requires (std::is_constructible_v<Vec3, Vector<U, 3> const&> && std::is_move_assignable_v<Vec3>)
  { m_rows[i] = Vec3(value); }

  template <typename U>
  constexpr void set_column(std::size_t i, Vector<U, 3> const& value)
    noexcept(std::is_nothrow_constructible_v<T, U const&> && std::is_nothrow_move_assignable_v<T>)
    requires (std::is_constructible_v<T, U const&> && std::is_move_assignable_v<T>)
  {
    T first(value[0]), second(value[1]), third(value[2]);
    m_rows[0][i] = std::move(first);
    m_rows[1][i] = std::move(second);
    m_rows[2][i] = std::move(third);
  }

  [[nodiscard]] static constexpr Matrix3 identity()
    requires requires { Matrix3(T(1), T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(1)); }
  { return Matrix3(T(1), T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(1)); }

  [[nodiscard]] static Matrix3 rotation(T angle, Vec2 const& point = Vec2()) requires std::floating_point<T> {
    T s = std::sin(angle), c = std::cos(angle);
    return Matrix3(c, -s, point[0] - c * point[0] + s * point[1],
      s, c, point[1] - s * point[0] - c * point[1], 0, 0, 1);
  }

  [[nodiscard]] static constexpr Matrix3 translation(Vec2 const& point)
    requires requires { Matrix3(T(1), T(0), point[0], T(0), T(1), point[1], T(0), T(0), T(1)); }
  { return Matrix3(T(1), T(0), point[0], T(0), T(1), point[1], T(0), T(0), T(1)); }

  [[nodiscard]] static constexpr Matrix3 scaling(Vec2 const& factor, Vec2 const& point = Vec2())
    requires requires { Matrix3(factor[0], T(0), T(point[0] - point[0] * factor[0]), T(0), factor[1],
      T(point[1] - point[1] * factor[1]), T(0), T(0), T(1)); }
  { return Matrix3(factor[0], T(0), T(point[0] - point[0] * factor[0]), T(0), factor[1],
      T(point[1] - point[1] * factor[1]), T(0), T(0), T(1)); }

  [[nodiscard]] static constexpr Matrix3 scaling(T factor, Vec2 const& point = Vec2())
    requires requires { scaling(Vec2::filled(factor), point); }
  { return scaling(Vec2::filled(factor), point); }

  [[nodiscard]] constexpr T determinant() const
    noexcept(requires(T const& a) { { static_cast<T>(a*a*a-a*a*a+a*a*a-a*a*a+a*a*a-a*a*a) } noexcept; })
    requires requires(T const& a) { static_cast<T>(a*a*a-a*a*a+a*a*a-a*a*a+a*a*a-a*a*a); }
  {
    return static_cast<T>(m_rows[0][0] * m_rows[1][1] * m_rows[2][2] - m_rows[0][0] * m_rows[2][1] * m_rows[1][2]
      + m_rows[1][0] * m_rows[2][1] * m_rows[0][2] - m_rows[1][0] * m_rows[0][1] * m_rows[2][2]
      + m_rows[2][0] * m_rows[0][1] * m_rows[1][2] - m_rows[2][0] * m_rows[1][1] * m_rows[0][2]);
  }

  [[nodiscard]] constexpr T trace() const
    noexcept(requires(T const& a) { { static_cast<T>(a + a + a) } noexcept; })
    requires requires(T const& a) { static_cast<T>(a + a + a); }
  { return static_cast<T>(m_rows[0][0] + m_rows[1][1] + m_rows[2][2]); }

  [[nodiscard]] constexpr Vec3 diagonal() const
    noexcept(std::is_nothrow_constructible_v<Vec3, T const&, T const&, T const&>)
    requires std::is_constructible_v<Vec3, T const&, T const&, T const&>
  { return Vec3(m_rows[0][0], m_rows[1][1], m_rows[2][2]); }

  constexpr void transpose() noexcept(std::is_nothrow_swappable_v<T>) requires std::is_swappable_v<T> {
    std::swap(m_rows[1][0], m_rows[0][1]);
    std::swap(m_rows[2][0], m_rows[0][2]);
    std::swap(m_rows[2][1], m_rows[1][2]);
  }

  // Partial-pivot Gauss-Jordan avoids determinant under/overflow and preserves
  // disparate scales. A zero computed pivot is singular; there is no implicit
  // epsilon cutoff. Nonfinite input, intermediates or output are errors.
  [[nodiscard]] std::expected<Matrix3, StarException> inverse() const requires matrix_checked_scalar<T> {
    for (auto const& row : m_rows)
      for (T value : row)
        if (!isFinite(value))
          return std::unexpected(MathException("Cannot invert a nonfinite matrix"));
    Matrix3 working = *this;
    Matrix3 result = identity();
    for (std::size_t column = 0; column != 3; ++column) {
      std::size_t pivot = column;
      for (std::size_t row = column + 1; row != 3; ++row)
        if (std::fabs(working[row][column]) > std::fabs(working[pivot][column]))
          pivot = row;
      T divisor = working[pivot][column];
      if (divisor == 0)
        return std::unexpected(MathException("Cannot invert a singular matrix"));
      if (pivot != column) {
        std::swap(working[pivot], working[column]);
        std::swap(result[pivot], result[column]);
      }
      for (std::size_t c = 0; c != 3; ++c) {
        working[column][c] /= divisor;
        result[column][c] /= divisor;
      }
      working[column][column] = 1;
      for (std::size_t row = 0; row != 3; ++row) {
        if (row == column)
          continue;
        T factor = working[row][column];
        for (std::size_t c = 0; c != 3; ++c) {
          working[row][c] -= factor * working[column][c];
          result[row][c] -= factor * result[column][c];
        }
        working[row][column] = 0;
      }
      for (std::size_t r = 0; r != 3; ++r)
        for (std::size_t c = 0; c != 3; ++c)
          if (!isFinite(working[r][c]) || !isFinite(result[r][c]))
            return std::unexpected(MathException("Matrix inverse arithmetic is not finite"));
    }
    return result;
  }

  [[nodiscard]] std::expected<void, StarException> invert() requires matrix_checked_scalar<T> {
    auto result = inverse();
    if (!result)
      return std::unexpected(std::move(result.error()));
    *this = std::move(*result);
    return {};
  }

  [[nodiscard]] bool is_orthogonal(T tolerance) const requires matrix_checked_scalar<T> {
    if (!isFinite(tolerance) || tolerance < 0)
      return false;
    for (auto const& row : m_rows)
      for (T value : row)
        if (!isFinite(value))
          return false;
    for (std::size_t r = 0; r != 3; ++r)
      for (std::size_t c = r; c != 3; ++c) {
        T dot = m_rows[r][0]*m_rows[c][0] + m_rows[r][1]*m_rows[c][1] + m_rows[r][2]*m_rows[c][2];
        if (!isFinite(dot) || std::fabs(dot - T(r == c)) > tolerance)
          return false;
      }
    return true;
  }

  // Modified Gram-Schmidt with reorthogonalization on magnitude-scaled rows.
  // Residual tolerance is relative to each input row's largest component.
  // The public default rejects roundoff-sized rank loss; zero disables that
  // input cutoff, but the output must still meet the orthonormal accuracy bound.
  [[nodiscard]] std::expected<Matrix3, StarException> orthonormalized(
      T residual_tolerance = 16 * std::numeric_limits<T>::epsilon()) const
    requires matrix_checked_scalar<T>
  {
    if (!isFinite(residual_tolerance) || residual_tolerance < 0)
      return std::unexpected(MathException("Invalid orthonormalization tolerance"));
    Matrix3 result;
    for (std::size_t r = 0; r != 3; ++r) {
      T magnitude = 0;
      for (T value : m_rows[r]) {
        if (!isFinite(value))
          return std::unexpected(MathException("Cannot orthonormalize a nonfinite matrix"));
        magnitude = std::max(magnitude, std::fabs(value));
      }
      if (magnitude == 0)
        return std::unexpected(MathException("Cannot orthonormalize a degenerate matrix"));
      Vec3 residual = m_rows[r] / magnitude;
      for (std::size_t pass = 0; pass != 2; ++pass)
        for (std::size_t previous = 0; previous != r; ++previous) {
          T dot = residual[0]*result[previous][0] + residual[1]*result[previous][1] + residual[2]*result[previous][2];
          for (std::size_t c = 0; c != 3; ++c)
            residual[c] -= dot * result[previous][c];
        }
      T length = std::hypot(residual[0], residual[1], residual[2]);
      if (!isFinite(length) || length <= residual_tolerance)
        return std::unexpected(MathException("Cannot orthonormalize a degenerate matrix"));
      result[r] = residual / length;
    }
    // Roundoff must not turn rank-deficient input into successful parallel
    // output. This is an output accuracy bound, not an input rank threshold.
    if (!result.is_orthogonal(64 * std::numeric_limits<T>::epsilon()))
      return std::unexpected(MathException("Cannot produce an orthonormal matrix"));
    return result;
  }

  [[nodiscard]] std::expected<void, StarException> orthonormalize(
      T residual_tolerance = 16 * std::numeric_limits<T>::epsilon())
    requires matrix_checked_scalar<T>
  {
    auto result = orthonormalized(residual_tolerance);
    if (!result)
      return std::unexpected(std::move(result.error()));
    *this = std::move(*result);
    return {};
  }

  // Affine left multiplication still includes the complete third row. All
  // coefficients and source rows are saved before writes, including aliases.
  void rotate(T angle, Vec2 const& point = Vec2()) requires std::floating_point<T> {
    T s = std::sin(angle), c = std::cos(angle);
    T x = point[0] - c * point[0] + s * point[1];
    T y = point[1] - s * point[0] - c * point[1];
    Vec3 first = m_rows[0], second = m_rows[1];
    for (std::size_t i = 0; i != 3; ++i) {
      m_rows[0][i] = c * first[i] - s * second[i] + x * m_rows[2][i];
      m_rows[1][i] = s * first[i] + c * second[i] + y * m_rows[2][i];
    }
  }

  constexpr void translate(Vec2 const& point)
    requires requires(T& output, T const& value) { output = static_cast<T>(value + value * value); }
  {
    for (std::size_t i = 0; i != 3; ++i) {
      m_rows[0][i] = static_cast<T>(m_rows[0][i] + point[0] * m_rows[2][i]);
      m_rows[1][i] = static_cast<T>(m_rows[1][i] + point[1] * m_rows[2][i]);
    }
  }

  constexpr void scale(Vec2 const& factor, Vec2 const& point = Vec2())
    requires requires(T& output, T const& value) {
      T(value - value * value);
      output = static_cast<T>(value * value + value * value);
    }
  {
    T x = T(point[0] - point[0] * factor[0]), y = T(point[1] - point[1] * factor[1]);
    for (std::size_t i = 0; i != 3; ++i) {
      m_rows[0][i] = static_cast<T>(factor[0] * m_rows[0][i] + x * m_rows[2][i]);
      m_rows[1][i] = static_cast<T>(factor[1] * m_rows[1][i] + y * m_rows[2][i]);
    }
  }

  constexpr void scale(T factor, Vec2 const& point = Vec2())
    requires requires { scale(Vec2::filled(factor), point); }
  { scale(Vec2::filled(factor), point); }

  // Mixed transforms evaluate the usual T*U expression, narrowing only once
  // to the requested output coordinate. No implicit Vector conversion needed.
  template <typename U>
  [[nodiscard]] constexpr Vector<U, 2> transform_point(Vector<U, 2> const& point) const
    requires (matrix_point_transform<U, T, U>
      && std::is_constructible_v<Vector<U, 2>, U const&, U const&>)
  {
    return Vector<U, 2>(static_cast<U>(m_rows[0][0]*point[0] + m_rows[0][1]*point[1] + m_rows[0][2]),
      static_cast<U>(m_rows[1][0]*point[0] + m_rows[1][1]*point[1] + m_rows[1][2]));
  }

  template <typename U>
  [[nodiscard]] constexpr Vector<U, 2> transform_direction(Vector<U, 2> const& direction) const
    requires (matrix_direction_transform<U, T, U>
      && std::is_constructible_v<Vector<U, 2>, U const&, U const&>)
  {
    return Vector<U, 2>(static_cast<U>(m_rows[0][0]*direction[0] + m_rows[0][1]*direction[1]),
      static_cast<U>(m_rows[1][0]*direction[0] + m_rows[1][1]*direction[1]));
  }

  template <std::floating_point U>
  [[nodiscard]] U transform_angle(U angle) const requires std::floating_point<T> {
    using Calculation = std::common_type_t<T, U>;
    Calculation input = static_cast<Calculation>(angle);
    auto direction = transform_direction(Vector<Calculation, 2>(std::cos(input), std::sin(input)));
    return static_cast<U>(std::atan2(direction[1], direction[0]));
  }

  [[nodiscard]] constexpr bool operator==(Matrix3 const& other) const
    noexcept(noexcept(std::declval<Rows const&>() == std::declval<Rows const&>()))
    requires requires(Rows const& a, Rows const& b) { a == b; }
  { return m_rows == other.m_rows; }

  constexpr Matrix3& operator*=(T const& scalar)
    requires (matrix_result<T> && std::is_move_assignable_v<Rows>
      && requires(T& output, T const& value) { output = static_cast<T>(value * value); })
  {
    *this = *this * scalar;
    return *this;
  }

  constexpr Matrix3& operator/=(T const& scalar)
    requires (matrix_result<T> && std::is_move_assignable_v<Rows>
      && requires(T& output, T const& value) { output = static_cast<T>(value / value); })
  {
    *this = *this / scalar;
    return *this;
  }

  [[nodiscard]] constexpr Matrix3 operator*(T const& scalar) const
    requires (matrix_result<T>
      && requires(T& output, T const& value) { output = static_cast<T>(value * value); })
  {
    Matrix3 result;
    for (std::size_t r = 0; r != 3; ++r)
      for (std::size_t c = 0; c != 3; ++c)
        result[r][c] = static_cast<T>(m_rows[r][c] * scalar);
    return result;
  }

  [[nodiscard]] constexpr Matrix3 operator/(T const& scalar) const
    requires (matrix_result<T>
      && requires(T& output, T const& value) { output = static_cast<T>(value / value); })
  {
    Matrix3 result;
    for (std::size_t r = 0; r != 3; ++r)
      for (std::size_t c = 0; c != 3; ++c)
        result[r][c] = static_cast<T>(m_rows[r][c] / scalar);
    return result;
  }

  [[nodiscard]] constexpr Matrix3 operator-() const
    requires (matrix_result<T> && requires(T& output, T const& value) { output = static_cast<T>(-value); })
  {
    Matrix3 result;
    for (std::size_t r = 0; r != 3; ++r)
      for (std::size_t c = 0; c != 3; ++c)
        result[r][c] = static_cast<T>(-m_rows[r][c]);
    return result;
  }

  template <typename U>
  constexpr Matrix3& operator+=(Matrix3<U> const& other)
    requires requires(T& output, U const& value) { output += value; }
  {
    for (std::size_t r = 0; r != 3; ++r)
      for (std::size_t c = 0; c != 3; ++c)
        m_rows[r][c] += other[r][c];
    return *this;
  }

  template <typename U>
  constexpr Matrix3& operator-=(Matrix3<U> const& other)
    requires requires(T& output, U const& value) { output -= value; }
  {
    for (std::size_t r = 0; r != 3; ++r)
      for (std::size_t c = 0; c != 3; ++c)
        m_rows[r][c] -= other[r][c];
    return *this;
  }

  template <typename U>
  [[nodiscard]] constexpr Matrix3 operator+(Matrix3<U> const& other) const
    requires (matrix_result<T> && requires(T& output, T const& left, U const& right) { output = static_cast<T>(left + right); })
  {
    Matrix3 result;
    for (std::size_t r = 0; r != 3; ++r)
      for (std::size_t c = 0; c != 3; ++c)
        result[r][c] = static_cast<T>(m_rows[r][c] + other[r][c]);
    return result;
  }

  template <typename U>
  [[nodiscard]] constexpr Matrix3 operator-(Matrix3<U> const& other) const
    requires (matrix_result<T> && requires(T& output, T const& left, U const& right) { output = static_cast<T>(left - right); })
  {
    Matrix3 result;
    for (std::size_t r = 0; r != 3; ++r)
      for (std::size_t c = 0; c != 3; ++c)
        result[r][c] = static_cast<T>(m_rows[r][c] - other[r][c]);
    return result;
  }

  template <typename U>
  [[nodiscard]] constexpr Matrix3 operator*(Matrix3<U> const& other) const
    requires (matrix_result<T> && matrix_product<T, U> && std::is_assignable_v<T&, T>)
  {
    Matrix3 result;
    for (std::size_t r = 0; r != 3; ++r)
      for (std::size_t c = 0; c != 3; ++c)
        result[r][c] = static_cast<T>(m_rows[r][0]*other[0][c] + m_rows[r][1]*other[1][c] + m_rows[r][2]*other[2][c]);
    return result;
  }

  template <typename U>
  constexpr Matrix3& operator*=(Matrix3<U> const& other)
    requires (matrix_result<T> && matrix_product<T, U> && std::is_assignable_v<T&, T>
      && std::is_move_assignable_v<Rows>)
  { *this = *this * other; return *this; }

  template <typename U>
  [[nodiscard]] constexpr Vec3 operator*(Vector<U, 3> const& vector) const
    requires (matrix_product<T, U> && std::is_constructible_v<Vec3, T const&, T const&, T const&>)
  {
    return Vec3(static_cast<T>(m_rows[0][0]*vector[0] + m_rows[0][1]*vector[1] + m_rows[0][2]*vector[2]),
      static_cast<T>(m_rows[1][0]*vector[0] + m_rows[1][1]*vector[1] + m_rows[1][2]*vector[2]),
      static_cast<T>(m_rows[2][0]*vector[0] + m_rows[2][1]*vector[1] + m_rows[2][2]*vector[2]));
  }

  template <typename U>
  [[nodiscard]] constexpr Vec2 operator*(Vector<U, 2> const& vector) const
    requires (matrix_point_transform<T, T, U> && std::is_constructible_v<Vec2, T const&, T const&>)
  {
    return Vec2(static_cast<T>(m_rows[0][0]*vector[0] + m_rows[0][1]*vector[1] + m_rows[0][2]),
      static_cast<T>(m_rows[1][0]*vector[0] + m_rows[1][1]*vector[1] + m_rows[1][2]));
  }

private:
  Rows m_rows;
};

using Mat3F = Matrix3<float>;
using Mat3D = Matrix3<double>;

template <typename T>
[[nodiscard]] constexpr T determinant(Matrix3<T> const& matrix)
  noexcept(noexcept(matrix.determinant())) requires requires { matrix.determinant(); }
{ return matrix.determinant(); }

template <typename T>
[[nodiscard]] constexpr Matrix3<T> transpose(Matrix3<T> matrix)
  noexcept(noexcept(matrix.transpose()) && std::is_nothrow_move_constructible_v<Matrix3<T>>)
  requires (std::is_move_constructible_v<Matrix3<T>> && requires { matrix.transpose(); })
{ matrix.transpose(); return matrix; }

template <typename T>
[[nodiscard]] auto inverse(Matrix3<T> const& matrix) requires requires { matrix.inverse(); }
{ return matrix.inverse(); }

template <typename T>
[[nodiscard]] auto orthonormalized(Matrix3<T> const& matrix,
    T residual_tolerance = 16 * std::numeric_limits<T>::epsilon())
  requires requires { matrix.orthonormalized(residual_tolerance); }
{ return matrix.orthonormalized(residual_tolerance); }

template <typename T>
[[nodiscard]] constexpr Matrix3<T> operator*(T const& scalar, Matrix3<T> const& matrix)
  requires requires { matrix * scalar; }
{ return matrix * scalar; }

template <typename T>
  requires nonfallible_text_writable<T>
void write_text(TextBuffer& output, Matrix3<T> const& matrix) {
  for (std::size_t row = 0; row != 3; ++row) {
    if (row != 0)
      output.push_back('\n');
    for (std::size_t column = 0; column != 3; ++column) {
      if (column != 0)
        output.push_back(' ');
      append_text(output, matrix[row][column]);
    }
  }
}

}

export module star.matrix3;

export template <typename T>
  requires Star::nonfallible_text_writable<T>
struct std::formatter<Star::Matrix3<T>> : Star::TextFormatter {};

export namespace Star {
  using ::Star::matrix_result;
  using ::Star::matrix_convertible;
  using ::Star::matrix_product;
  using ::Star::matrix_point_transform;
  using ::Star::matrix_direction_transform;
  using ::Star::matrix_checked_scalar;
  using ::Star::Matrix3;
  using ::Star::Mat3F;
  using ::Star::Mat3D;
  using ::Star::determinant;
  using ::Star::transpose;
  using ::Star::inverse;
  using ::Star::orthonormalized;
  using ::Star::operator*;
  using ::Star::write_text;
}
