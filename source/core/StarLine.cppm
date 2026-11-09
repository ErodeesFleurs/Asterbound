module;
import std;
import star.text_formatter;
import star.hash;

import star.math_common;
import star.vector;
import star.matrix3;

namespace Star {

template <typename T>
concept line_scalar = (std::integral<T> || std::floating_point<T>) && !std::same_as<std::remove_cv_t<T>, bool>;

template <line_scalar T, std::size_t N>
  requires(N > 0)
class Line {
public:
  using VectorType = Vector<T, N>;

  struct IntersectResult {
    // Whether or not the two objects intersect
    bool intersects;
    // Where the intersection is (minimum value if intersection occurs in more
    // than one point.)
    VectorType point;
    // T value where intersection occurs, 0 is min, 1 is max
    T t;
    // Whether or not the two lines, if they were infinite lines, are the exact
    // same line
    bool coincides;
    // Whether or not the intersection is a glancing one, meaning the other
    // line isn't actually skewered, it's just barely touching Coincidental
    // lines are always glancing intersections.
    bool glances;
  };

  constexpr Line() = default;

  template <line_scalar T2>
    requires std::constructible_from<VectorType, Vector<T2, N> const&>
  explicit Line(Line<T2, N> const& line) : m_min(line.min()), m_max(line.max()) {}

  constexpr Line(VectorType const& a, VectorType const& b) : m_min(a), m_max(b) {}

  VectorType direction() const { return diff().normalized(); }

  T length() const { return diff().magnitude(); }

  T angle() const
    requires(N == 2)
  {
    return diff().angle();
  }

  VectorType eval(T t) const { return m_min + diff() * t; }

  VectorType diff() const { return (m_max - m_min); }

  VectorType center() const { return (m_min + m_max) / 2; }

  void set_center(VectorType c) { translate(c - center()); }

  VectorType& min() { return m_min; }

  VectorType& max() { return m_max; }

  VectorType const& min() const { return m_min; }

  VectorType const& max() const { return m_max; }

  bool make_positive() {
    if (m_max < m_min) {
      reverse();
      return true;
    }
    return false;
  }

  void reverse() { std::swap(m_min, m_max); }

  Line reversed() const { return Line(m_max, m_min); }

  void translate(VectorType const& trans) {
    m_min += trans;
    m_max += trans;
  }

  Line translated(VectorType const& trans) const { return Line(m_min + trans, m_max + trans); }

  void scale(VectorType const& s, VectorType const& c = VectorType()) {
    m_min = vmult(m_min - c, s) + c;
    m_max = vmult(m_max - c, s) + c;
  }

  void scale(T s, VectorType const& c = VectorType()) { scale(VectorType::filled(s), c); }

  bool operator==(Line const& rhs) const { return std::tie(m_min, m_max) == std::tie(rhs.m_min, rhs.m_max); }

  bool operator<(Line const& rhs) const { return std::tie(m_min, m_max) < std::tie(rhs.m_min, rhs.m_max); }

  IntersectResult intersection(Line const& other, bool infinite = false) const
    requires(N == 2)
  {
    return intersection_impl<true>(other, infinite);
  }

  bool intersects(Line const& other, bool infinite = false) const
    requires(N == 2)
  {
    return intersection_impl<false>(other, infinite);
  }

  // Returns t value for closest point on the line.  t value is *not* clamped
  // from 0.0 to 1.0
  T line_projection(VectorType const& l2) const
    requires(N == 2)
  {
    VectorType d = diff();
    return ((l2[0] - min()[0]) * d[0] + (l2[1] - min()[1]) * d[1]) / d.magnitude_squared();
  }

  T distance_to(VectorType const& l, bool infinite = false) const
    requires(N == 2)
  {
    auto t = line_projection(l);
    if (!infinite)
      t = std::clamp<T>(t, 0, 1);
    return vmag(l - eval(t));
  }

  void rotate(T angle, VectorType const& rotationCenter = VectorType())
    requires(N == 2)
  {
    auto rotMatrix = Mat3F::rotation(angle, rotationCenter);
    min() = rotMatrix.transformVec2(min());
    max() = rotMatrix.transformVec2(max());
  }

  template <line_scalar T2>
  void transform(Matrix3<T2> const& transform)
    requires(N == 2)
  {
    min() = transform.transformVec2(min());
    max() = transform.transformVec2(max());
  }

  template <line_scalar T2>
  Line transformed(Matrix3<T2> const& transform) const
    requires(N == 2)
  {
    return Line(transform.transformVec2(min()), transform.transformVec2(max()));
  }

  void flip_horizontal(T horizontalPos)
    requires(N == 2)
  {
    m_min[0] = horizontalPos + (horizontalPos - m_min[0]);
    m_max[0] = horizontalPos + (horizontalPos - m_max[0]);
  }

  void flip_vertical(T verticalPos)
    requires(N == 2)
  {
    m_min[1] = verticalPos + (verticalPos - m_min[1]);
    m_max[1] = verticalPos + (verticalPos - m_max[1]);
  }

private:
  template <bool Full>
  std::conditional_t<Full, IntersectResult, bool> intersection_impl(Line const& other, bool infinite) const
    requires(N == 2)
  {
    // Endpoint order is observable for coincident lines; do not normalize it here.
    auto const& a = m_min;
    auto const& b = m_max;
    auto const& c = other.m_min;
    auto const& d = other.m_max;
    auto ab = diff();
    auto cd = other.diff();
    T denom = ab ^ cd;
    [[maybe_unused]] std::conditional_t<Full, IntersectResult, bool> result;
    if (nearZero(denom)) {
      T xNumer = (a ^ b) * cd[0] - (c ^ d) * ab[0];
      T yNumer = (a ^ b) * cd[1] - (c ^ d) * ab[1];
      if (nearZero(xNumer) && nearZero(yNumer)) {
        bool hit = infinite || (a >= c && a <= d) || (c >= a && c <= b);
        if constexpr (!Full) {
          return hit;
        } else {
          result.intersects = hit;
          if (hit)
            result.point = infinite ? VectorType::filled(-std::numeric_limits<T>::max()) : (a < c ? c : a);
          if (a < c) {
            if (c[0] != a[0])
              result.t = (c[0] - a[0]) / ab[0];
            else
              result.t = (c[1] - a[1]) / ab[1];
          } else if (a > d) {
            if (d[0] != a[0])
              result.t = (d[0] - a[0]) / ab[0];
            else
              result.t = (d[1] - a[1]) / ab[1];
          } else {
            result.t = 0;
          }
          result.coincides = true;
          result.glances = hit;
        }
      } else {
        if constexpr (!Full) {
          return false;
        } else {
          result.intersects = false;
          result.t = std::numeric_limits<T>::max();
          result.point = VectorType();
          result.coincides = false;
          result.glances = false;
        }
      }
    } else {
      T ta = ((c - a) ^ cd) / denom;
      T tb = ((c - a) ^ ab) / denom;
      bool hit = infinite || (ta >= 0 && ta <= 1.0 && tb >= 0 && tb <= 1.0);
      if constexpr (!Full) {
        return hit;
      } else {
        result.intersects = hit;
        result.t = ta;
        result.point = VectorType(ta * (b[0] - a[0]) + a[0], ta * (b[1] - a[1]) + a[1]);
        result.coincides = false;
        result.glances =
          !infinite && hit && (nearZero(ta) || nearEqual(ta, 1.0f) || nearZero(tb) || nearEqual(tb, 1.0f));
      }
    }
    if constexpr (Full)
      return result;
  }

  VectorType m_min;
  VectorType m_max;
};

using Line2F = Line<float, 2>;
using Line2D = Line<double, 2>;
using Line2I = Line<int, 2>;

template <line_scalar T, std::size_t N>
  requires nonfallible_text_writable<Vector<T, N>>
void write_text(TextBuffer& output, Line<T, N> const& line) {
  output.push_back('[');
  append_text(output, line.min());
  output.append(", ");
  append_text(output, line.max());
  output.push_back(']');
}

template <line_scalar T, std::size_t N> struct hash<Line<T, N>> {
  std::size_t operator()(Line<T, N> const& line) const {
    std::size_t hashval = 0;
    hash_combine(hashval, vectorHasher(line.min()));
    hash_combine(hashval, vectorHasher(line.max()));
    return hashval;
  }
  Star::hash<typename Line<T, N>::VectorType> vectorHasher;
};

}// namespace Star

export module star.line;

export namespace Star {
template <line_scalar T, std::size_t N> struct hash<Line<T, N>>;
}

export template <Star::line_scalar T, std::size_t N>
  requires Star::nonfallible_text_writable<Star::Vector<T, N>>
struct std::formatter<Star::Line<T, N>> : Star::TextFormatter {};

export namespace Star {
using ::Star::hash;
using ::Star::Line;
using ::Star::Line2D;
using ::Star::Line2F;
using ::Star::Line2I;
using ::Star::line_scalar;
using ::Star::write_text;
}// namespace Star
