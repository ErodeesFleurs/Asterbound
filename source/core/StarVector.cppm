module;
import std;
import star.hash;
import star.optional;

import star.array;
import star.math_common;
import star.format;

namespace Star {

template <typename T, std::size_t N>
class Vector : public Array<T, N> {
public:
  using Base = Array<T, N>;

  template <std::size_t P, typename T2 = void> using Enable2D = std::enable_if_t<P == 2 && N == P, T2>;

  template <std::size_t P, typename T2 = void> using Enable3D = std::enable_if_t<P == 3 && N == P, T2>;

  template <std::size_t P, typename T2 = void> using Enable4D = std::enable_if_t<P == 4 && N == P, T2>;

  template <std::size_t P, typename T2 = void> using Enable2DOrHigher = std::enable_if_t<P >= 2 && N == P, T2>;

  template <std::size_t P, typename T2 = void> using Enable3DOrHigher = std::enable_if_t<P >= 3 && N == P, T2>;

  template <std::size_t P, typename T2 = void> using Enable4DOrHigher = std::enable_if_t<P >= 4 && N == P, T2>;

  static Vector filled(T const& t);

  template <typename T2>
  static Vector floor(Vector<T2, N> const& v);

  template <typename T2>
  static Vector ceil(Vector<T2, N> const& v);

  template <typename T2>
  static Vector round(Vector<T2, N> const& v);

  template <typename Iterator>
  static Vector copyFrom(Iterator p);

  // Is zero-initialized (from Array)
  Vector();

  explicit Vector(T const& e1);

  template <typename... TN>
  Vector(T const& e1, TN const&... rest);

  template <typename T2>
  explicit Vector(Array<T2, N> const& v);

  template <typename T2, typename T3>
  Vector(Array<T2, N - 1> const& u, T3 const& v);

  template <std::size_t N2>
  Vector<T, N2> toSize() const;
  Vector<T, 2> vec2() const;
  Vector<T, 3> vec3() const;
  Vector<T, 4> vec4() const;

  Vector piecewiseMultiply(Vector const& v2) const;
  Vector piecewiseDivide(Vector const& v2) const;

  Vector piecewiseMin(Vector const& v2) const;
  Vector piecewiseMax(Vector const& v2) const;
  Vector piecewiseClamp(Vector const& min, Vector const& max) const;

  T min() const;
  T max() const;

  T sum() const;
  T product() const;

  template <typename Function>
  Vector combine(Vector const& v, Function f) const;

  // Outputs angles in the range [0, pi]
  T angleBetween(Vector const& v) const;

  // Angle between two normalized vectors.
  T angleBetweenNormalized(Vector const& v) const;

  T magnitudeSquared() const;
  T magnitude() const;

  void normalize();
  Vector normalized() const;

  Vector projectOnto(Vector const& v) const;

  Vector projectOntoNormalized(Vector const& v) const;

  void negate();

  // Reverses order of components of vector
  void reverse();

  Vector abs() const;
  Vector floor() const;
  Vector ceil() const;
  Vector round() const;

  void fill(T const& v);
  void clamp(T const& min, T const& max);

  template <typename Function>
  void transform(Function&& function);

  template <typename Function>
  Vector<decltype(std::declval<Function>()(std::declval<T>())), N> transformed(Function&& function) const;

  Vector operator-() const;

  Vector operator+(Vector const& v) const;
  Vector operator-(Vector const& v) const;
  T operator*(Vector const& v) const;
  Vector operator*(T s) const;
  Vector operator/(T s) const;
  Vector& operator+=(Vector const& v);
  Vector& operator-=(Vector const& v);
  Vector& operator*=(T s);
  Vector& operator/=(T s);

  // Vector2

  // Return vector rotated to given angle
  template <std::size_t P = N>
  static Enable2D<P, Vector> withAngle(T angle, T magnitude = 1);

  template <std::size_t P = N>
  static Enable2D<P, T> angleBetween2(Vector const& u, Vector const& v);
  template <std::size_t P = N>
  static Enable2D<P, T> angleFormedBy2(Vector const& a, Vector const& b, Vector const& c);
  template <std::size_t P = N>
  [[nodiscard]] static Enable2D<P, std::expected<T, StarException>> angleFormedBy2(Vector const& a, Vector const& b, Vector const& c,
      std::function<std::expected<Vector, StarException>(Vector, Vector)> const& diff);

  template <std::size_t P = N>
  Enable2D<P, Vector> rotate(T angle) const;

  // Faster than rotate(std::numbers::pi/2).
  template <std::size_t P = N>
  Enable2D<P, Vector> rot90() const;

  // Angle of vector on 2d plane, in the range [-pi, pi]
  template <std::size_t P = N>
  Enable2D<P, T> angle() const;

  // Returns polar coordinates of this cartesian vector
  template <std::size_t P = N>
  Enable2D<P, Vector> toPolar() const;

  // Returns cartesian coordinates of this polar vector
  template <std::size_t P = N>
  Enable2D<P, Vector> toCartesian() const;

  template <std::size_t P = N>
  Enable2DOrHigher<P, T> const& x() const;
  template <std::size_t P = N>
  Enable2DOrHigher<P, T> const& y() const;

  template <std::size_t P = N>
  Enable2DOrHigher<P> setX(T const& t);
  template <std::size_t P = N>
  Enable2DOrHigher<P> setY(T const& t);

  // Vector3

  template <std::size_t P = N>
  static Enable3D<P, Vector> fromAngles(T psi, T theta);
  template <std::size_t P = N>
  static Enable3D<P, Vector> fromAnglesEnu(T psi, T theta);
  template <std::size_t P = N>
  static Enable3D<P, T> tripleScalarProduct(Vector const& u, Vector const& v, Vector const& w);
  template <std::size_t P = N>
  static Enable3D<P, T> angle(Vector const& v1, Vector const& v2);

  template <std::size_t P = N>
  Enable3D<P, T> psi() const;
  template <std::size_t P = N>
  Enable3D<P, T> theta() const;
  template <std::size_t P = N>
  Enable3D<P, Vector<T, 2>> eulers() const;

  template <std::size_t P = N>
  Enable3D<P, T> psiEnu() const;
  template <std::size_t P = N>
  Enable3D<P, T> thetaEnu() const;

  template <std::size_t P = N>
  Enable3D<P, Vector> nedToEnu() const;
  template <std::size_t P = N>
  Enable3D<P, Vector> enuToNed() const;

  template <std::size_t P = N>
  Enable3DOrHigher<P, T> const& z() const;

  template <std::size_t P = N>
  Enable3DOrHigher<P> setZ(T const& t);

  // Vector4

  template <std::size_t P = N>
  Enable4DOrHigher<P, T> const& w() const;

  template <std::size_t P = N>
  Enable4DOrHigher<P> setW(T const& t);

  using Base::size;
  using Base::empty;
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
std::ostream& operator<<(std::ostream& os, Vector<T, N> const& v);

template <typename T, std::size_t N>
Vector<T, N> operator*(T s, Vector<T, N> v);

template <typename T, std::size_t N>
Vector<T, N> vnorm(Vector<T, N> v);

template <typename T, std::size_t N>
T vmag(Vector<T, N> const& v);

template <typename T, std::size_t N>
T vmagSquared(Vector<T, N> const& v);

template <typename T, std::size_t N>
Vector<T, N> vmin(Vector<T, N> const& a, Vector<T, N> const& b);

template <typename T, std::size_t N>
Vector<T, N> vmax(Vector<T, N> const& a, Vector<T, N> const& b);

template <typename T, std::size_t N>
Vector<T, N> vclamp(Vector<T, N> const& a, Vector<T, N> const& min, Vector<T, N> const& max);

template <typename VectorType>
VectorType vmult(VectorType const& a, VectorType const& b);

template <typename VectorType>
VectorType vdiv(VectorType const& a, VectorType const& b);

// Returns the cross product
template <typename T>
Vector<T, 3> operator^(Vector<T, 3> v1, Vector<T, 3> v2);

// Returns the cross product / determinant
template <typename T>
T operator^(Vector<T, 2> const& v1, Vector<T, 2> const& v2);

template <typename T, std::size_t N>
struct hash<Vector<T, N>> : hash<Array<T, N>> {};

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::filled(T const& t) {
  Vector v;
  for (std::size_t i = 0; i < N; ++i)
    v[i] = t;
  return v;
}

template <typename T, std::size_t N>
template <typename T2>
Vector<T, N> Vector<T, N>::floor(Vector<T2, N> const& v) {
  Vector vec;
  for (std::size_t i = 0; i < N; ++i)
    vec[i] = std::floor(v[i]);
  return vec;
}

template <typename T, std::size_t N>
template <typename T2>
Vector<T, N> Vector<T, N>::ceil(Vector<T2, N> const& v) {
  Vector vec;
  for (std::size_t i = 0; i < N; ++i)
    vec[i] = std::ceil(v[i]);
  return vec;
}

template <typename T, std::size_t N>
template <typename T2>
Vector<T, N> Vector<T, N>::round(Vector<T2, N> const& v) {
  Vector vec;
  for (std::size_t i = 0; i < N; ++i)
    vec[i] = std::round(v[i]);
  return vec;
}

template <typename T, std::size_t N>
template <typename Iterator>
Vector<T, N> Vector<T, N>::copyFrom(Iterator p) {
  Vector v;
  std::ranges::copy_n(std::move(p), N, v.begin());
  return v;
}

template <typename T, std::size_t N> Vector<T, N>::Vector() = default;

template <typename T, std::size_t N>
Vector<T, N>::Vector(T const& e1)
  : Base(e1) {}

template <typename T, std::size_t N>
template <typename... TN>
Vector<T, N>::Vector(T const& e1, TN const&... rest)
  : Base(e1, rest...) {}

template <typename T, std::size_t N>
template <typename T2>
Vector<T, N>::Vector(Array<T2, N> const& v)
  : Base(v) {}

template <typename T, std::size_t N>
template <typename T2, typename T3>
Vector<T, N>::Vector(Array<T2, N - 1> const& u, T3 const& v) {
  for (std::size_t i = 0; i < N - 1; ++i) {
    Base::operator[](i) = u[i];
  }
  Base::operator[](N - 1) = v;
}

template <typename T, std::size_t N>
template <std::size_t N2>
Vector<T, N2> Vector<T, N>::toSize() const {
  Vector<T, N2> r;
  std::ranges::copy_n(this->begin(), std::min(N2, N), r.begin());
  return r;
}

template <typename T, std::size_t N>
Vector<T, 2> Vector<T, N>::vec2() const {
  return toSize<2>();
}

template <typename T, std::size_t N>
Vector<T, 3> Vector<T, N>::vec3() const {
  return toSize<3>();
}

template <typename T, std::size_t N>
Vector<T, 4> Vector<T, N>::vec4() const {
  return toSize<4>();
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::piecewiseMultiply(Vector const& v2) const {
  return combine(v2, std::multiplies<T>());
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::piecewiseDivide(Vector const& v2) const {
  return combine(v2, std::divides<T>());
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::piecewiseMin(Vector const& v2) const {
  Vector r;
  for (std::size_t i = 0; i < N; ++i)
    r[i] = std::min((*this)[i], v2[i]);
  return r;
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::piecewiseMax(Vector const& v2) const {
  Vector r;
  for (std::size_t i = 0; i < N; ++i)
    r[i] = std::max((*this)[i], v2[i]);
  return r;
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::piecewiseClamp(Vector const& min, Vector const& max) const {
  Vector r;
  for (std::size_t i = 0; i < N; ++i)
    r[i] = std::max(std::min((*this)[i], max[i]), min[i]);
  return r;
}

template <typename T, std::size_t N>
T Vector<T, N>::min() const {
  T s = (*this)[0];
  for (std::size_t i = 1; i < N; ++i)
    s = std::min(s, (*this)[i]);
  return s;
}

template <typename T, std::size_t N>
T Vector<T, N>::max() const {
  T s = (*this)[0];
  for (std::size_t i = 1; i < N; ++i)
    s = std::max(s, (*this)[i]);
  return s;
}

template <typename T, std::size_t N>
T Vector<T, N>::sum() const {
  T s = (*this)[0];
  for (std::size_t i = 1; i < N; ++i)
    s += (*this)[i];
  return s;
}

template <typename T, std::size_t N>
T Vector<T, N>::product() const {
  T p = (*this)[0];
  for (std::size_t i = 1; i < N; ++i)
    p *= (*this)[i];
  return p;
}

template <typename T, std::size_t N>
template <typename Function>
Vector<T, N> Vector<T, N>::combine(Vector const& v, Function f) const {
  Vector r;
  for (std::size_t i = 0; i < N; ++i)
    r[i] = f((*this)[i], v[i]);
  return r;
}

template <typename T, std::size_t N>
T Vector<T, N>::angleBetween(Vector const& v) const {
  return std::acos(this->normalized() * v.normalized());
}

template <typename T, std::size_t N>
T Vector<T, N>::angleBetweenNormalized(Vector const& v) const {
  return std::acos(*this * v);
}

template <typename T, std::size_t N>
T Vector<T, N>::magnitudeSquared() const {
  T m = 0;
  for (std::size_t i = 0; i < N; ++i)
    m += square((*this)[i]);
  return m;
}

template <typename T, std::size_t N>
T Vector<T, N>::magnitude() const {
  return std::sqrt(magnitudeSquared());
}

template <typename T, std::size_t N>
void Vector<T, N>::normalize() {
  T m = magnitude();
  if (m != 0)
    *this = (*this) / m;
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::normalized() const {
  T m = magnitude();
  if (m != 0)
    return (*this) / m;
  else
    return *this;
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::projectOnto(Vector const& v) const {
  T m = v.magnitudeSquared();
  if (m != 0)
    return projectOntoNormalized(v) / m;
  else
    return Vector();
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::projectOntoNormalized(Vector const& v) const {
  return ((*this) * v) * v;
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::operator-() const {
  auto v = *this;
  v.negate();
  return v;
}

template <typename T, std::size_t N>
void Vector<T, N>::negate() {
  for (std::size_t i = 0; i < N; ++i)
    (*this)[i] = -(*this)[i];
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::abs() const {
  Vector v;
  for (std::size_t i = 0; i < N; ++i)
    v[i] = std::fabs((*this)[i]);
  return v;
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::floor() const {
  return floor(*this);
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::ceil() const {
  return ceil(*this);
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::round() const {
  return round(*this);
}

template <typename T, std::size_t N>
void Vector<T, N>::reverse() {
  std::reverse(Base::begin(), Base::end());
}

template <typename T, std::size_t N>
void Vector<T, N>::fill(T const& v) {
  Base::fill(v);
}

template <typename T, std::size_t N>
void Vector<T, N>::clamp(T const& min, T const& max) {
  for (std::size_t i = 0; i < N; ++i)
    (*this)[i] = std::max(min, std::min(max, (*this)[i]));
}

template <typename T, std::size_t N>
template <typename Function>
void Vector<T, N>::transform(Function&& function) {
  for (auto& e : *this)
    e = function(e);
}

template <typename T, std::size_t N>
template <typename Function>
Vector<decltype(std::declval<Function>()(std::declval<T>())), N> Vector<T, N>::transformed(Function&& function) const {
  Vector<decltype(std::declval<Function>()(std::declval<T>())), N> result;
  for (std::size_t i = 0; i < N; ++i)
    result[i] = function((*this)[i]);
  return result;
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::operator+(Vector const& v) const {
  Vector r;
  for (std::size_t i = 0; i < N; ++i)
    r[i] = (*this)[i] + v[i];
  return r;
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::operator-(Vector const& v) const {
  Vector r;
  for (std::size_t i = 0; i < N; ++i)
    r[i] = (*this)[i] - v[i];
  return r;
}

template <typename T, std::size_t N>
T Vector<T, N>::operator*(Vector const& v) const {
  T sum = 0;
  for (std::size_t i = 0; i < N; ++i)
    sum += (*this)[i] * v[i];
  return sum;
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::operator*(T s) const {
  Vector r;
  for (std::size_t i = 0; i < N; ++i)
    r[i] = (*this)[i] * s;
  return r;
}

template <typename T, std::size_t N>
Vector<T, N> Vector<T, N>::operator/(T s) const {
  Vector r;
  for (std::size_t i = 0; i < N; ++i)
    r[i] = (*this)[i] / s;
  return r;
}

template <typename T, std::size_t N>
Vector<T, N>& Vector<T, N>::operator+=(Vector const& v) {
  return (*this = *this + v);
}

template <typename T, std::size_t N>
Vector<T, N>& Vector<T, N>::operator-=(Vector const& v) {
  return (*this = *this - v);
}

template <typename T, std::size_t N>
Vector<T, N>& Vector<T, N>::operator*=(T s) {
  return (*this = *this * s);
}

template <typename T, std::size_t N>
Vector<T, N>& Vector<T, N>::operator/=(T s) {
  return (*this = *this / s);
}

// Vector2

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::withAngle(T angle, T magnitude) -> Enable2D<P, Vector<T, N>> {
  return Vector(std::cos(angle) * magnitude, std::sin(angle) * magnitude);
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::angleBetween2(Vector const& v1, Vector const& v2) -> Enable2D<P, T> {
  // TODO: Inefficient
  return v2.angle() - v1.angle();
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::angleFormedBy2(Vector const& a, Vector const& b, Vector const& c) -> Enable2D<P, T> {
  return angleBetween2(b - a, b - c);
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::angleFormedBy2(
    Vector const& a, Vector const& b, Vector const& c,
    std::function<std::expected<Vector, StarException>(Vector, Vector)> const& diff)
    -> Enable2D<P, std::expected<T, StarException>> {
  if (!diff)
    return std::unexpected(StarException("Vector::angleFormedBy2 requires a difference function"));
  auto ba = diff(b, a);
  if (!ba)
    return std::unexpected(std::move(ba.error()));
  auto bc = diff(b, c);
  if (!bc)
    return std::unexpected(std::move(bc.error()));
  return angleBetween2(*ba, *bc);
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::angle() const -> Enable2D<P, T> {
  return std::atan2(Base::operator[](1), Base::operator[](0));
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::rotate(T a) const -> Enable2D<P, Vector<T, N>> {
  // TODO: Need a Matrix2
  T cosa = std::cos(a);
  T sina = std::sin(a);
  return Vector(
      Base::operator[](0) * cosa - Base::operator[](1) * sina, Base::operator[](0) * sina + Base::operator[](1) * cosa);
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::rot90() const -> Enable2D<P, Vector<T, N>> {
  return Vector(-y(), x());
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::toPolar() const -> Enable2D<P, Vector<T, N>> {
  return Vector(angle(), Base::magnitude());
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::toCartesian() const -> Enable2D<P, Vector<T, N>> {
  return vec2d(std::sin((*this)[0]) * (*this)[1], std::cos((*this)[0]) * (*this)[1]);
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::x() const -> Enable2DOrHigher<P, T> const & {
  return Base::operator[](0);
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::y() const -> Enable2DOrHigher<P, T> const & {
  return Base::operator[](1);
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::setX(T const& t) -> Enable2DOrHigher<P> {
  Base::operator[](0) = t;
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::setY(T const& t) -> Enable2DOrHigher<P> {
  Base::operator[](1) = t;
}

// Vector3

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::tripleScalarProduct(Vector const& a, Vector const& b, Vector const& c) -> Enable3D<P, T> {
  return a * (b ^ c);
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::theta() const -> Enable3D<P, T> {
  Vector<T, N> vn = norm(*this);
  T tmp = std::fabs(vn.z());
  if (tmp > 0.99999) {
    return tmp > 0.0 ? T(-std::numbers::pi / 2) : T(std::numbers::pi / 2);
  } else {
    return std::asin(-vn.z());
  }
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::psi() const -> Enable3D<P, T> {
  Vector<T, N> vn = norm(*this);
  T tmp = T(std::fabs(vn.z()));
  if (tmp > 0.99999) {
    return 0.0;
  } else {
    return T(std::atan2(vn.y(), vn.x()));
  }
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::thetaEnu() const -> Enable3D<P, T> {
  Vector<T, N> vn = norm(*this);
  T tmp = std::fabs(vn.z());
  if (tmp > 0.99999) {
    return tmp > 0.0 ? -std::numbers::pi / 2 : std::numbers::pi / 2;
  } else {
    return std::asin(vn.z());
  }
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::psiEnu() const -> Enable3D<P, T> {
  Vector<T, N> vn = norm(*this);
  T tmp = std::fabs(vn.z());
  if (tmp > 0.99999) {
    return 0.0;
  } else {
    return std::atan2(vn.x(), vn.y());
  }
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::eulers() const -> Enable3D<P, Vector<T, 2>> {
  T psi, theta;
  Vector<T, N> vn = norm(*this);
  T tmp = std::fabs(vn.z());
  if (tmp > 0.99999) {
    psi = 0.0;
    theta = tmp > 0.0 ? -std::numbers::pi / 2 : std::numbers::pi / 2;
  } else {
    psi = std::atan2(vn.y(), vn.x());
    theta = std::asin(-vn.z());
  }
  return Vector<T, 2>(psi, theta);
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::fromAngles(T psi, T theta) -> Enable3D<P, Vector<T, N>> {
  Vec3F nv;
  T cosTheta = T(std::cos(theta));

  nv.x() = T(std::cos(psi));
  nv.y() = T(std::sin(psi));
  nv.x() *= cosTheta;
  nv.y() *= cosTheta;
  nv.z() = T(-std::sin(theta));
  return nv;
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::fromAnglesEnu(T psi, T theta) -> Enable3D<P, Vector<T, N>> {
  Vector nv = fromAngles(psi, theta);
  return Vector(nv.y(), nv.x(), -nv.z());
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::angle(Vector const& v1, Vector const& v2) -> Enable3D<P, T> {
  return std::acos(std::min(norm(v1) * norm(v2), 1.0));
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::nedToEnu() const -> Enable3D<P, Vector<T, N>> {
  return Vector(y(), x(), -z());
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::enuToNed() const -> Enable3D<P, Vector<T, N>> {
  return Vector(y(), x(), -z());
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::z() const -> Enable3DOrHigher<P, T> const & {
  return Base::operator[](2);
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::setZ(T const& t) -> Enable3DOrHigher<P> {
  Base::operator[](2) = t;
}

// Vector4

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::w() const -> Enable4DOrHigher<P, T> const & {
  return Base::operator[](3);
}

template <typename T, std::size_t N>
template <std::size_t P>
auto Vector<T, N>::setW(T const& t) -> Enable4DOrHigher<P> {
  Base::operator[](3) = t;
}

// Free Functions

template <typename T, std::size_t N>
std::ostream& operator<<(std::ostream& os, Vector<T, N> const& v) {
  os << '(';
  for (std::size_t i = 0; i < N; ++i) {
    if constexpr (requires { outputOptional(v[i]); })
      os << outputOptional(v[i]);
    else
      os << v[i];
    if (i != N - 1)
      os << ", ";
  }
  os << ')';
  return os;
}

template <typename T, std::size_t N>
Vector<T, N> operator*(T s, Vector<T, N> v) {
  return v * s;
}

template <typename T, std::size_t N>
Vector<T, N> vnorm(Vector<T, N> v) {
  return v.normalized();
}

template <typename T, std::size_t N>
T vmag(Vector<T, N> const& v) {
  return v.magnitude();
}

template <typename T, std::size_t N>
T vmagSquared(Vector<T, N> const& v) {
  return v.magnitudeSquared();
}

template <typename T, std::size_t N>
Vector<T, N> vmin(Vector<T, N> const& a, Vector<T, N> const& b) {
  return a.piecewiseMin(b);
}

template <typename T, std::size_t N>
Vector<T, N> vmax(Vector<T, N> const& a, Vector<T, N> const& b) {
  return a.piecewiseMax(b);
}

template <typename T, std::size_t N>
Vector<T, N> vclamp(Vector<T, N> const& a, Vector<T, N> const& min, Vector<T, N> const& max) {
  return a.piecewiseClamp(min, max);
}

template <typename VectorType>
VectorType vmult(VectorType const& a, VectorType const& b) {
  return a.piecewiseMultiply(b);
}

template <typename VectorType>
VectorType vdiv(VectorType const& a, VectorType const& b) {
  return a.piecewiseDivide(b);
}

template <typename T>
Vector<T, 3> operator^(Vector<T, 3> v1, Vector<T, 3> v2) {
  return Vector<T, 3>(v1[1] * v2[2] - v1[2] * v2[1], v1[2] * v2[0] - v1[0] * v2[2], v1[0] * v2[1] - v1[1] * v2[0]);
}

template <typename T>
T operator^(Vector<T, 2> const& v1, Vector<T, 2> const& v2) {
  return v1[0] * v2[1] - v1[1] * v2[0];
}

}

export module star.vector;

export namespace Star {
  template <typename T, std::size_t N>
  struct hash<Vector<T, N>>;
}


export namespace Star {
  using ::Star::Vector;
  using ::Star::Vec2I;
  using ::Star::Vec2U;
  using ::Star::Vec2F;
  using ::Star::Vec2D;
  using ::Star::Vec2B;
  using ::Star::Vec2S;
  using ::Star::Vec3I;
  using ::Star::Vec3U;
  using ::Star::Vec3F;
  using ::Star::Vec3D;
  using ::Star::Vec3B;
  using ::Star::Vec3S;
  using ::Star::Vec4I;
  using ::Star::Vec4U;
  using ::Star::Vec4F;
  using ::Star::Vec4D;
  using ::Star::Vec4B;
  using ::Star::Vec4S;
  using ::Star::vnorm;
  using ::Star::vmag;
  using ::Star::vmagSquared;
  using ::Star::vmin;
  using ::Star::vmax;
  using ::Star::vclamp;
  using ::Star::vmult;
  using ::Star::vdiv;
  using ::Star::operator<<;
  using ::Star::operator*;
  using ::Star::operator^;
  using ::Star::hash;
}
