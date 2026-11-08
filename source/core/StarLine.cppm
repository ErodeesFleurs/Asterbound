module;
import std;
import star.stream_formatter;
import star.hash;

import star.math_common;
import star.vector;
import star.matrix3;

namespace Star {

template <typename T, std::size_t N>
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

  Line() {}

  template <typename T2>
  explicit Line(Line<T2, N> const& line)
    : m_min(line.min()), m_max(line.max()) {}

  Line(VectorType const& a, VectorType const& b)
    : m_min(a), m_max(b) {}

  VectorType direction() const {
    return diff().normalized();
  }

  T length() const {
    return diff().magnitude();
  }

  T angle() const {
    return diff().angle();
  }

  VectorType eval(T t) const {
    return m_min + diff() * t;
  }

  VectorType diff() const {
    return (m_max - m_min);
  }

  VectorType center() const {
    return (m_min + m_max) / 2;
  }

  void setCenter(VectorType c) {
    return translate(c - center());
  }

  VectorType& min() {
    return m_min;
  }

  VectorType& max() {
    return m_max;
  }

  VectorType const& min() const {
    return m_min;
  }

  VectorType const& max() const {
    return m_max;
  }

  VectorType midpoint() const {
    return (m_max + m_min) / 2;
  }

  bool makePositive() {
    bool changed = false;
    for (unsigned i = 0; i < N; i++) {
      if (m_min[i] < m_max[i]) {
        break;
      } else if (m_min[i] > m_max[i]) {
        std::swap(m_min, m_max);
        changed = true;
        break;
      }
    }
    return changed;
  }

  void reverse() {
    std::swap(m_min, m_max);
  }

  Line reversed() {
    return Line(m_max, m_min);
  }

  void translate(VectorType const& trans) {
    m_min += trans;
    m_max += trans;
  }

  Line translated(VectorType const& trans) {
    return Line(m_min + trans, m_max + trans);
  }

  void scale(VectorType const& s, VectorType const& c = VectorType()) {
    m_min = vmult(m_min - c, s) + c;
    m_max = vmult(m_max - c, s) + c;
  }

  void scale(T s, VectorType const& c = VectorType()) {
    scale(VectorType::filled(s), c);
  }

  bool operator==(Line const& rhs) const {
    return std::tie(m_min, m_max) == std::tie(rhs.m_min, rhs.m_max);
  }

  bool operator<(Line const& rhs) const {
    return std::tie(m_min, m_max) < std::tie(rhs.m_min, rhs.m_max);
  }

  // Line2

  template <std::size_t P = N>
    requires (P == 2 && N == P)
  IntersectResult intersection(Line const& line2, bool infinite = false) const {
    Line l1 = *this;
    Line l2 = line2;
    // Warning to others, do not make the lines positive, because points of
    // intersection for coincidental lines are determined by the first point
    // And makePositive() changes the order of points.  This causes headaches
    // later on
    // l1.makePositive();
    // l2.makePositive();
    VectorType a = l1.min();
    VectorType b = l1.max();
    VectorType c = l2.min();
    VectorType d = l2.max();

    VectorType ab = diff();
    VectorType cd = l2.diff();

    T denom = ab ^ cd;
    T xNumer = (a ^ b) * cd[0] - (c ^ d) * ab[0];
    T yNumer = (a ^ b) * cd[1] - (c ^ d) * ab[1];

    IntersectResult isect;
    if (nearZero(denom)) { // the lines are parallel unless
      if (nearZero(xNumer) && nearZero(yNumer)) { // the lines are coincidental
        isect.intersects = infinite || (a >= c && a <= d) || (c >= a && c <= b);
        if (isect.intersects) {
          // returns the minimum intersection point
          if (infinite) {
            isect.point = VectorType::filled(-std::numeric_limits<T>::max());
          } else {
            isect.point = a < c ? c : a;
          }
        }
        if (a < c) {
          if (c[0] != a[0]) {
            isect.t = (c[0] - a[0]) / ab[0];
          } else {
            isect.t = (c[1] - a[1]) / ab[1];
          }
        } else if (a > d) {
          if (d[0] != a[0]) {
            isect.t = (d[0] - a[0]) / ab[0];
          } else {
            isect.t = (d[1] - a[1]) / ab[1];
          }
        } else {
          isect.t = 0;
        }
        isect.coincides = true;
        isect.glances = isect.intersects;
      } else {
        isect.intersects = false;
        isect.t = std::numeric_limits<T>::max();
        isect.point = VectorType();
        isect.coincides = false;
        isect.glances = false;
      }
    } else {
      T ta = ((c - a) ^ cd) / denom;
      T tb = ((c - a) ^ ab) / denom;

      isect.intersects = infinite || (ta >= 0 && ta <= 1.0 && tb >= 0 && tb <= 1.0);
      isect.t = ta;
      isect.point = VectorType(ta * (b[0] - a[0]) + a[0], ta * (b[1] - a[1]) + a[1]);
      isect.coincides = false;
      isect.glances = !infinite && isect.intersects && (nearZero(ta) || nearEqual(ta, 1.0f) || nearZero(tb) || nearEqual(tb, 1.0f));
    }
    return isect;
  }

  template <std::size_t P = N>
    requires (P == 2 && N == P)
  bool intersects(Line const& l2, bool infinite = false) const {
    return intersection(l2, infinite).intersects;
  }

  // Returns t value for closest point on the line.  t value is *not* clamped
  // from 0.0 to 1.0
  template <std::size_t P = N>
    requires (P == 2 && N == P)
  T lineProjection(VectorType const& l2) const {
    VectorType d = diff();
    return ((l2[0] - min()[0]) * d[0] + (l2[1] - min()[1]) * d[1]) / d.magnitude_squared();
  }

  template <std::size_t P = N>
    requires (P == 2 && N == P)
  T distanceTo(VectorType const& l, bool infinite = false) const {
    auto t = lineProjection(l);
    if (!infinite)
      t = std::clamp<T>(t, 0, 1);
    return vmag(l - eval(t));
  }

  template <std::size_t P = N>
    requires (P == 2 && N == P)
  void rotate(T angle, VectorType const& rotationCenter = VectorType()) {
    auto rotMatrix = Mat3F::rotation(angle, rotationCenter);
    min() = rotMatrix.transformVec2(min());
    max() = rotMatrix.transformVec2(max());
  }

  template <typename T2, std::size_t P = N>
    requires (P == 2 && N == P)
  void transform(Matrix3<T2> const& transform) {
    min() = transform.transformVec2(min());
    max() = transform.transformVec2(max());
  }

  template <typename T2, std::size_t P = N>
    requires (P == 2 && N == P)
  Line transformed(Matrix3<T2> const& transform) const {
    return Line(transform.transformVec2(min()), transform.transformVec2(max()));
  }

  template <std::size_t P = N>
    requires (P == 2 && N == P)
  void flipHorizontal(T horizontalPos) {
    m_min[0] = horizontalPos + (horizontalPos - m_min[0]);
    m_max[0] = horizontalPos + (horizontalPos - m_max[0]);
  }

  template <std::size_t P = N>
    requires (P == 2 && N == P)
  void flipVertical(T verticalPos) {
    m_min[1] = verticalPos + (verticalPos - m_min[1]);
    m_max[1] = verticalPos + (verticalPos - m_max[1]);
  }

private:
  VectorType m_min;
  VectorType m_max;
};

using Line2F = Line<float, 2>;
using Line2D = Line<double, 2>;
using Line2I = Line<int, 2>;

template <typename T, std::size_t N>
std::ostream& operator<<(std::ostream& os, Line<T, N> const& l) {
  os << '[' << l.min() << ", " << l.max() << ']';
  return os;
}

template <typename T, std::size_t N>
struct hash<Line<T, N>> {
  std::size_t operator()(Line<T, N> const& line) const {
    std::size_t hashval = 0;
    hash_combine(hashval, vectorHasher(line.min()));
    hash_combine(hashval, vectorHasher(line.max()));
    return hashval;
  }
  Star::hash<typename Line<T, N>::VectorType> vectorHasher;
};

}

export module star.line;

export namespace Star {
  template <typename T, std::size_t N>
  struct hash<Line<T, N>>;
}


export template <typename T, std::size_t N>
struct std::formatter<Star::Line<T, N>> : Star::StreamFormatter {};

export namespace Star {
  using ::Star::Line;
  using ::Star::Line2F;
  using ::Star::Line2D;
  using ::Star::Line2I;
  using ::Star::operator<<;
  using ::Star::hash;
}
