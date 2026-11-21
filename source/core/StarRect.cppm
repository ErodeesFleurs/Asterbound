module;
import std;
import star.text_formatter;
import star.list;

import star.array;
import star.math_common;
import star.vector;
import star.line;

namespace Star {

namespace rect_detail {
template <typename T>
concept aspect_scalar = std::floating_point<T> || (std::integral<T> && !std::same_as<T, bool>);

// A signed result must hold the difference of any two endpoints in either direction.
template <typename T>
concept displacement_scalar = std::floating_point<T> || (std::integral<T>
    && std::numeric_limits<T>::digits + std::is_signed_v<T> <= std::numeric_limits<std::intmax_t>::digits);
}

// Axis aligned box that can be used as a bounding volume.
template <typename T, std::size_t N>
class Box {
public:
  using Coord = Vector<T, N>;
  using Line = Star::Line<T, N>;
  using LineIntersectResult = typename Line::IntersectResult;

  struct IntersectResult {
    // Whether or not the two objects intersect
    bool intersects;
    // Directed displacement along the selected axis. Integer differences can be
    // negative even for unsigned boxes, and can exceed a signed coordinate's range.
    using Scalar = std::conditional_t<std::integral<T>, std::intmax_t, T>;
    Vector<Scalar, N> overlap;
    // Whether or not the intersection is touching only.  No overlap.
    bool glances;
  };

  static Box null();
  static Box inf();

  // Returns an integral aligned box that at least contains the given floating
  // point box.
  template <typename B>
    requires requires(B const& b) { Vector<T, N>::floor(b.min()); Vector<T, N>::ceil(b.max()); }
  static Box integral(B const& box);

  // Returns an integral aligned box that is equal to the given box rounded to
  // the nearest whole number (does not necessarily contain the given box).
  template <typename B>
    requires requires(B const& b) { Vector<T, N>::round(b.min()); Vector<T, N>::round(b.max()); }
  static Box round(B const& box);

  template <typename... TN>
    requires ((std::same_as<TN, Vector<T, N>> || std::same_as<TN, Box<T, N>>) && ...)
  static Box bound_box_of(TN const&... list);

  template <typename Collection>
    requires std::ranges::input_range<Collection const> && std::constructible_from<Vector<T, N>, std::ranges::range_reference_t<Collection const>>
  static Box bound_box_of_points(Collection const& collection);

  static Box with_size(Coord const& min, Coord const& size);
  // Symmetric about center. An odd integral size loses its last unit; both
  // resulting endpoints must be representable.
  static Box with_center(Coord const& center, Coord const& size);

  Box() = default;
  constexpr Box(Coord const& min, Coord const& max);

  template <typename T2>
  explicit Box(Box<T2, N> const& b);

  // Is equal to null()
  bool is_null() const;

  // One or more dimensions are of negative magnitude
  bool is_negative() const;

  // One or more dimensions are of zero or negative magnitude
  bool is_empty() const;

  // Sets the bounding box equal to one containing the given bounding box and
  // the current one.
  void combine(Box const& box);
  Box combined(Box const& box) const;

  // Sets the bounding box equal to one containing the current bounding box and
  // the given point.
  void combine(Coord const& point);
  Box combined(Coord const& point) const;

  // Sets the bounding box equal to the intersection of this one and the given
  // one.  If there is no intersection than the box becomes negative in that
  // dimension.
  void limit(Box const& box);
  Box limited(Box const& box) const;

  // If any range has a min < max, swap them to make it non-null.
  void make_positive();

  // Sets any empty (or negative) dimensions in the bounding box to the
  // corresponding range in the given bounding box.  If the bounding box is not
  // empty in any dimension, then this has no effect.
  void range_set_if_empty(Box const& b);

  Coord size() const;
  T size(std::size_t dim) const;

  // Fit a positive aspect ratio about the center (shrink selects the smaller
  // scale). Integral quotients truncate, as do half-extents in with_center.
  // Return false without mutation for reversed bounds, nonpositive components
  // or integer results that cannot be represented.
  bool set_aspect(Coord const& aspect, bool shrink = false) requires (N > 0 && rect_detail::aspect_scalar<T>);
  bool make_cube() requires (N > 0 && rect_detail::aspect_scalar<T>);

  Coord center() const;
  // Translate by the difference between c and center(). The integer endpoints
  // after translation must be representable; the translation itself need not be.
  void set_center(Coord const& c);

  void translate(Coord const& c);
  Box translated(Coord const& c) const;

  // Translate the Box the minimum distance so that it includes the given point
  void translate_to_include(Coord const& coord, Coord const& padding = Coord());

  Vector<T, 2> range(std::size_t dim) const;
  void set_range(std::size_t dim, Vector<T, 2> v);
  void combine_range(std::size_t dim, Vector<T, 2> v);
  void limit_range(std::size_t dim, Vector<T, 2> v);

  // Expand from center. For integer coordinates, the original size, scaled
  // size and final endpoints must be representable, as for size()/with_center().
  void expand(T factor);
  Box expanded(T factor) const;

  // Same integer representability preconditions as scalar expand().
  void expand(Coord const& factor);
  Box expanded(Vector<T, N> const& factor) const;

  // Scale around origin.
  void scale(T factor);
  Box scaled(T factor) const;

  // Scale around origin.
  void scale(Coord const& factor);
  Box scaled(Vector<T, N> const& factor) const;

  // Increase all dimensions by a constant amount on all sides
  void pad(T amount);
  Box padded(T amount) const;

  // Increase all dimensions by a constant amount
  void pad(Coord const& amount);
  Box padded(Vector<T, N> const& amount) const;

  // Opposite of pad
  void trim(T amount);
  Box trimmed(T amount) const;

  // Opposite of pad
  void trim(Coord const& amount);
  Box trimmed(Vector<T, N> const& amount) const;

  // Flip around some dimension (may make box have negative volume)
  void flip(std::size_t dimension);
  Box flipped(std::size_t dimension) const;

  constexpr Coord const& min() const & { return m_min; }
  constexpr Coord& min() & { return m_min; }
  constexpr Coord min() && { return std::move(m_min); }
  constexpr Coord min() const && { return m_min; }
  constexpr Coord const& max() const & { return m_max; }
  constexpr Coord& max() & { return m_max; }
  constexpr Coord max() && { return std::move(m_max); }
  constexpr Coord max() const && { return m_max; }

  void set_min(Coord const& c);
  void set_max(Coord const& c);

  T volume() const;
  Box overlap(Box const& b) const;

  // Use intersects() for comparison-only queries, including integral coordinates
  // too wide for a directed difference in intmax_t.
  IntersectResult intersection(Box const& b) const requires (N > 0 && rect_detail::displacement_scalar<T>);
  bool intersects(Box const& b, bool include_edges = true) const;

  bool contains(Coord const& p, bool include_edges = true) const;
  bool contains(Box const& b, bool include_edges = true) const;

  // A version of contains that includes the min edges but not the max edges,
  // useful to select based on adjoining boxes without overlap.
  bool belongs(Coord const& p) const;

  bool contains_epsilon(Coord const& p, unsigned epsilons = 2) const;
  bool contains_epsilon(Box const& b, unsigned epsilons = 2) const;

  constexpr bool operator==(Box const&) const = default;

  // Find Coord inside box nearest to
  Coord nearest_coord_to(Coord const& c) const;

  // Find the coord in normalized space for this rect, so that 0 is the minimum
  // and 1 is the maximum.
  Coord normal(Coord const& coord) const;

  // The invers of normal, find the real space position of this normalized
  // coordinate.
  Coord eval(Coord const& normalized_coord) const;

  // 2D Only

  constexpr Box(T min_x, T min_y, T max_x, T max_y) requires (N == 2);

  T x_min() const requires (N == 2);
  T x_max() const requires (N == 2);
  T y_min() const requires (N == 2);
  T y_max() const requires (N == 2);

  void set_x_min(T x_min) requires (N == 2);
  void set_x_max(T x_max) requires (N == 2);
  void set_y_min(T y_min) requires (N == 2);
  void set_y_max(T y_max) requires (N == 2);

  T width() const requires (N == 2);
  T height() const requires (N == 2);

  void translate(T x, T y) requires (N == 2);
  void translate_to_include(T x, T y, T x_padding = 0, T y_padding = 0) requires (N == 2);
  void scale(T x, T y) requires (N == 2);
  void expand(T x, T y) requires (N == 2);
  void flip_horizontal() requires (N == 2);
  void flip_vertical() requires (N == 2);

  Array<Line, 4> edges() const requires (N == 2);
  bool intersects(Line const& l) const requires (N == 2);
  bool intersects_circle(Coord const& position, T radius) const requires (N == 2);
  LineIntersectResult edge_intersection(Line const& l) const requires (N == 2);

  // Regions in this rectangle but not in rect, ordered left, right, bottom, top.
  SmallList<Box, 4> subtract(Box const& rect) const requires (N == 2);

protected:
  Coord m_min;
  Coord m_max;
};

template <typename T, std::size_t N>
  requires nonfallible_text_writable<Vector<T, N>>
void write_text(TextBuffer& output, Box<T, N> const& box);

template<typename T>
using Rect = Box<T, 2>;

using RectI = Rect<int>;
using RectU = Rect<unsigned>;
using RectF = Rect<float>;
using RectD = Rect<double>;

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::null() {
  return Box(Coord::filled(std::numeric_limits<T>::max()), Coord::filled(std::numeric_limits<T>::lowest()));
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::inf() {
  return Box(Coord::filled(std::numeric_limits<T>::lowest()), Coord::filled(std::numeric_limits<T>::max()));
}

template <typename T, std::size_t N>
template <typename B>
  requires requires(B const& b) { Vector<T, N>::floor(b.min()); Vector<T, N>::ceil(b.max()); }
Box<T, N> Box<T, N>::integral(B const& box) {
  return Box(Coord::floor(box.min()), Coord::ceil(box.max()));
}

template <typename T, std::size_t N>
template <typename B>
  requires requires(B const& b) { Vector<T, N>::round(b.min()); Vector<T, N>::round(b.max()); }
Box<T, N> Box<T, N>::round(B const& box) {
  return Box(Coord::round(box.min()), Coord::round(box.max()));
}

template <typename T, std::size_t N>
template <typename... TN>
  requires ((std::same_as<TN, Vector<T, N>> || std::same_as<TN, Box<T, N>>) && ...)
Box<T, N> Box<T, N>::bound_box_of(TN const&... list) {
  Box b = null();
  (b.combine(list), ...);
  return b;
}

template <typename T, std::size_t N>
template <typename Collection>
  requires std::ranges::input_range<Collection const> && std::constructible_from<Vector<T, N>, std::ranges::range_reference_t<Collection const>>
Box<T, N> Box<T, N>::bound_box_of_points(Collection const& collection) {
  Box b = null();
  for (auto&& point : collection)
    b.combine(Coord(std::forward<decltype(point)>(point)));
  return b;
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::with_size(Coord const& min, Coord const& size) {
  return Box(min, min + size);
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::with_center(Coord const& center, Coord const& size) {
  auto half = size / 2;
  return Box(center - half, center + half);
}

template <typename T, std::size_t N>
constexpr Box<T, N>::Box(Coord const& min, Coord const& max)
  : m_min(min), m_max(max) {}

template <typename T, std::size_t N>
template <typename T2>
Box<T, N>::Box(Box<T2, N> const& b)
  : m_min(b.min()), m_max(b.max()) {}

template <typename T, std::size_t N>
bool Box<T, N>::is_null() const {
  return m_min == Coord::filled(std::numeric_limits<T>::max())
      && m_max == Coord::filled(std::numeric_limits<T>::lowest());
}

template <typename T, std::size_t N>
bool Box<T, N>::is_negative() const {
  for (std::size_t i = 0; i < N; ++i) {
    if (m_max[i] < m_min[i])
      return true;
  }
  return false;
}

template <typename T, std::size_t N>
bool Box<T, N>::is_empty() const {
  for (std::size_t i = 0; i < N; ++i) {
    if (m_max[i] <= m_min[i])
      return true;
  }
  return false;
}

template <typename T, std::size_t N>
void Box<T, N>::combine(Box const& box) {
  m_min = box.m_min.piecewise_min(m_min);
  m_max = box.m_max.piecewise_max(m_max);
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::combined(Box const& box) const {
  auto b = *this;
  b.combine(box);
  return b;
}

template <typename T, std::size_t N>
void Box<T, N>::combine(Coord const& point) {
  m_min = m_min.piecewise_min(point);
  m_max = m_max.piecewise_max(point);
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::combined(Coord const& point) const {
  auto b = *this;
  b.combine(point);
  return b;
}

template <typename T, std::size_t N>
void Box<T, N>::limit(Box const& box) {
  m_min = m_min.piecewise_max(box.m_min);
  m_max = m_max.piecewise_min(box.m_max);
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::limited(Box const& box) const {
  auto b = *this;
  b.limit(box);
  return b;
}

template <typename T, std::size_t N>
void Box<T, N>::make_positive() {
  for (std::size_t i = 0; i < N; ++i) {
    if (m_max[i] < m_min[i])
      std::swap(m_max[i], m_min[i]);
  }
}

template <typename T, std::size_t N>
void Box<T, N>::range_set_if_empty(Box const& b) {
  for (std::size_t i = 0; i < N; ++i) {
    if (m_max[i] <= m_min[i])
      set_range(i, b.range(i));
  }
}

template <typename T, std::size_t N>
bool Box<T, N>::make_cube() requires (N > 0 && rect_detail::aspect_scalar<T>) {
  return set_aspect(Coord::filled(1));
}

template <typename T, std::size_t N>
auto Box<T, N>::size() const -> Coord {
  return m_max - m_min;
}

template <typename T, std::size_t N>
T Box<T, N>::size(std::size_t dim) const {
  return m_max[dim] - m_min[dim];
}

template <typename T, std::size_t N>
bool Box<T, N>::set_aspect(Coord const& aspect, bool shrink) requires (N > 0 && rect_detail::aspect_scalar<T>) {
  for (std::size_t i = 0; i < N; ++i)
    if (!(aspect[i] > 0) || m_max[i] < m_min[i])
      return false;

  if constexpr (std::integral<T>) {
    // An ordered signed axis can span more than max(T) units. Its unsigned
    // difference still gives the exact width, even across zero.
    using U = std::make_unsigned_t<T>;
    U scale = shrink ? std::numeric_limits<U>::max() : U{};
    for (std::size_t i = 0; i < N; ++i) {
      U width = static_cast<U>(m_max[i]) - static_cast<U>(m_min[i]);
      U ratio = width / static_cast<U>(aspect[i]);
      scale = shrink ? std::min(scale, ratio) : std::max(scale, ratio);
    }
    auto middle = center();
    Coord half;
    for (std::size_t i = 0; i < N; ++i) {
      U component = static_cast<U>(aspect[i]);
      if (scale > std::numeric_limits<U>::max() / component)
        return false;
      half[i] = static_cast<T>((scale * component) / 2);
      if (middle[i] < std::numeric_limits<T>::lowest() + half[i] || middle[i] > std::numeric_limits<T>::max() - half[i])
        return false;
    }
    m_min = middle - half;
    m_max = middle + half;
  } else {
    auto ratios = size().piecewise_divide(aspect);
    auto scale = shrink ? ratios.min() : ratios.max();
    *this = with_center(center(), Coord::filled(scale).piecewise_multiply(aspect));
  }
  return true;
}

template <typename T, std::size_t N>
auto Box<T, N>::center() const -> Coord {
  if constexpr (std::integral<T>) {
    Coord result;
    for (std::size_t i = 0; i < N; ++i) {
      auto a = m_min[i], b = m_max[i];
      // Shared bits plus half the differing bits gives floor((a+b)/2)
      // without forming the sum. Correct negative odd sums toward zero.
      auto differing = a ^ b;
      auto lower = (a & b) + (differing >> 1);
      if constexpr (std::signed_integral<T>)
        lower += lower < 0 && (differing & 1);
      result[i] = static_cast<T>(lower);
    }
    return result;
  } else {
    return (m_min + m_max) / 2;
  }
}

template <typename T, std::size_t N>
void Box<T, N>::set_center(Coord const& c) {
  if constexpr (std::signed_integral<T>) {
    using U = std::make_unsigned_t<T>;
    auto middle = center();
    for (std::size_t i = 0; i < N; ++i) {
      U delta = static_cast<U>(c[i]) - static_cast<U>(middle[i]);
      m_min[i] = static_cast<T>(static_cast<U>(m_min[i]) + delta);
      m_max[i] = static_cast<T>(static_cast<U>(m_max[i]) + delta);
    }
  } else {
    translate(c - center());
  }
}

template <typename T, std::size_t N>
void Box<T, N>::translate(Coord const& c) {
  m_min += c;
  m_max += c;
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::translated(Coord const& c) const {
  auto b = *this;
  b.translate(c);
  return b;
}

template <typename T, std::size_t N>
void Box<T, N>::translate_to_include(Coord const& coord, Coord const& padding) {
  Coord translation;
  for (std::size_t i = 0; i < N; ++i) {
    if (coord[i] < m_min[i] + padding[i])
      translation[i] = coord[i] - m_min[i] - padding[i];
    else if (coord[i] > m_max[i] - padding[i])
      translation[i] = coord[i] - m_max[i] + padding[i];
  }
  translate(translation);
}

template <typename T, std::size_t N>
Vector<T, 2> Box<T, N>::range(std::size_t dim) const {
  return {m_min[dim], m_max[dim]};
}

template <typename T, std::size_t N>
void Box<T, N>::set_range(std::size_t dim, Vector<T, 2> v) {
  m_min[dim] = v[0];
  m_max[dim] = v[1];
}

template <typename T, std::size_t N>
void Box<T, N>::combine_range(std::size_t dim, Vector<T, 2> v) {
  m_min[dim] = std::min(m_min[dim], v[0]);
  m_max[dim] = std::max(m_max[dim], v[1]);
}

template <typename T, std::size_t N>
void Box<T, N>::limit_range(std::size_t dim, Vector<T, 2> v) {
  m_min[dim] = std::max(m_min[dim], v[0]);
  m_max[dim] = std::min(m_max[dim], v[1]);
}

template <typename T, std::size_t N>
void Box<T, N>::expand(T factor) {
  *this = expanded(factor);
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::expanded(T factor) const {
  return with_center(center(), size() * factor);
}

template <typename T, std::size_t N>
void Box<T, N>::expand(Coord const& factor) {
  *this = expanded(factor);
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::expanded(Coord const& factor) const {
  return with_center(center(), size().piecewise_multiply(factor));
}

template <typename T, std::size_t N>
void Box<T, N>::scale(T factor) {
  for (std::size_t i = 0; i < N; ++i)
    set_range(i, range(i) * factor);
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::scaled(T factor) const {
  auto b = *this;
  b.scale(factor);
  return b;
}

template <typename T, std::size_t N>
void Box<T, N>::scale(Coord const& factor) {
  for (std::size_t i = 0; i < N; ++i)
    set_range(i, range(i) * factor[i]);
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::scaled(Coord const& factor) const {
  auto b = *this;
  b.scale(factor);
  return b;
}

template <typename T, std::size_t N>
void Box<T, N>::pad(T amount) {
  for (std::size_t i = 0; i < N; ++i) {
    m_min[i] -= amount;
    m_max[i] += amount;
  }
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::padded(T amount) const {
  auto b = *this;
  b.pad(amount);
  return b;
}

template <typename T, std::size_t N>
void Box<T, N>::pad(Coord const& amount) {
  for (std::size_t i = 0; i < N; ++i) {
    m_min[i] -= amount[i];
    m_max[i] += amount[i];
  }
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::padded(Coord const& amount) const {
  auto b = *this;
  b.pad(amount);
  return b;
}

template <typename T, std::size_t N>
void Box<T, N>::trim(T amount) {
  pad(-amount);
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::trimmed(T amount) const {
  auto b = *this;
  b.trim(amount);
  return b;
}

template <typename T, std::size_t N>
void Box<T, N>::trim(Coord const& amount) {
  pad(-amount);
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::trimmed(Coord const& amount) const {
  auto b = *this;
  b.trim(amount);
  return b;
}

template <typename T, std::size_t N>
void Box<T, N>::flip(std::size_t dimension) {
  std::swap(m_min[dimension], m_max[dimension]);
}

template <typename T, std::size_t N>
Box<T, N> Box<T, N>::flipped(std::size_t dimension) const {
  auto b = *this;
  b.flip(dimension);
  return b;
}

template <typename T, std::size_t N>
auto Box<T, N>::normal(Coord const& coord) const -> Coord {
  return (coord - m_min).piecewise_divide(m_max - m_min);
}

template <typename T, std::size_t N>
auto Box<T, N>::eval(Coord const& normalized_coord) const -> Coord {
  return normalized_coord.piecewise_multiply(m_max - m_min) + m_min;
}

template <typename T, std::size_t N>
void Box<T, N>::set_min(Coord const& c) {
  m_min = c;
}

template <typename T, std::size_t N>
void Box<T, N>::set_max(Coord const& c) {
  m_max = c;
}

template <typename T, std::size_t N>
T Box<T, N>::volume() const {
  return size().product();
}

template <typename T, std::size_t N>
auto Box<T, N>::overlap(Box const& b) const -> Box {
  return limited(b);
}

template <typename T, std::size_t N>
auto Box<T, N>::intersection(Box const& b) const -> IntersectResult requires (N > 0 && rect_detail::displacement_scalar<T>) {
  using D = typename IntersectResult::Scalar;
  D depth = static_cast<D>(m_max[0]) - static_cast<D>(b.m_min[0]);
  std::size_t axis = 0;
  bool negative = true;
  for (std::size_t i = 0; i < N; ++i) {
    D left = static_cast<D>(m_max[i]) - static_cast<D>(b.m_min[i]);
    D right = static_cast<D>(b.m_max[i]) - static_cast<D>(m_min[i]);
    if (left < depth) {
      depth = left;
      axis = i;
      negative = true;
    }
    if (right < depth) {
      depth = right;
      axis = i;
      negative = false;
    }
  }
  IntersectResult result{depth > 0, {}, depth == 0};
  D magnitude = depth < 0 ? -depth : depth;
  result.overlap[axis] = negative ? -magnitude : magnitude;
  return result;
}

template <typename T, std::size_t N>
bool Box<T, N>::intersects(Box const& b, bool include_edges) const {
  for (std::size_t i = 0; i < N; ++i) {
    if (include_edges) {
      if (m_max[i] < b.m_min[i] || b.m_max[i] < m_min[i])
        return false;
    } else {
      if (m_max[i] <= b.m_min[i] || b.m_max[i] <= m_min[i])
        return false;
    }
  }
  return true;
}

template <typename T, std::size_t N>
bool Box<T, N>::contains(Coord const& p, bool include_edges) const {
  for (std::size_t i = 0; i < N; ++i) {
    if (include_edges) {
      if (p[i] < m_min[i] || p[i] > m_max[i])
        return false;
    } else {
      if (p[i] <= m_min[i] || p[i] >= m_max[i])
        return false;
    }
  }
  return true;
}

template <typename T, std::size_t N>
bool Box<T, N>::contains(Box const& b, bool include_edges) const {
  return contains(b.min(), include_edges) && contains(b.max(), include_edges);
}

template <typename T, std::size_t N>
bool Box<T, N>::belongs(Coord const& p) const {
  for (std::size_t i = 0; i < N; ++i) {
    if (p[i] < m_min[i] || p[i] >= m_max[i])
      return false;
  }

  return true;
}

template <typename T, std::size_t N>
bool Box<T, N>::contains_epsilon(Coord const& p, unsigned epsilons) const {
  for (std::size_t i = 0; i < N; ++i) {
    if (p[i] < m_min[i] || p[i] > m_max[i])
      return false;
    if (near_equal(p[i], m_min[i], epsilons) || near_equal(p[i], m_max[i], epsilons))
      return false;
  }
  return true;
}

template <typename T, std::size_t N>
bool Box<T, N>::contains_epsilon(Box const& b, unsigned epsilons) const {
  return contains_epsilon(b.min(), epsilons) && contains_epsilon(b.max(), epsilons);
}

template <typename T, std::size_t N>
  requires nonfallible_text_writable<Vector<T, N>>
void write_text(TextBuffer& output, Box<T, N> const& box) {
  output.append("Box{min:");
  append_text(output, box.min());
  output.append(" max:");
  append_text(output, box.max());
  output.push_back('}');
}

template <typename T, std::size_t N>
constexpr Box<T, N>::Box(T min_x, T min_y, T max_x, T max_y) requires (N == 2)
  : Box(Coord(min_x, min_y), Coord(max_x, max_y)) {}

template <typename T, std::size_t N>
auto Box<T, N>::x_min() const -> T requires (N == 2) {
  return min()[0];
}

template <typename T, std::size_t N>
auto Box<T, N>::x_max() const -> T requires (N == 2) {
  return max()[0];
}

template <typename T, std::size_t N>
auto Box<T, N>::y_min() const -> T requires (N == 2) {
  return min()[1];
}

template <typename T, std::size_t N>
auto Box<T, N>::y_max() const -> T requires (N == 2) {
  return max()[1];
}

template <typename T, std::size_t N>
auto Box<T, N>::set_x_min(T x_min) -> void requires (N == 2) {
  m_min[0] = x_min;
}

template <typename T, std::size_t N>
auto Box<T, N>::set_x_max(T x_max) -> void requires (N == 2) {
  m_max[0] = x_max;
}

template <typename T, std::size_t N>
auto Box<T, N>::set_y_min(T y_min) -> void requires (N == 2) {
  m_min[1] = y_min;
}

template <typename T, std::size_t N>
auto Box<T, N>::set_y_max(T y_max) -> void requires (N == 2) {
  m_max[1] = y_max;
}

template <typename T, std::size_t N>
auto Box<T, N>::width() const -> T requires (N == 2) {
  return size(0);
}

template <typename T, std::size_t N>
auto Box<T, N>::height() const -> T requires (N == 2) {
  return size(1);
}

template <typename T, std::size_t N>
auto Box<T, N>::translate(T x, T y) -> void requires (N == 2) {
  translate(Coord(x, y));
}

template <typename T, std::size_t N>
auto Box<T, N>::translate_to_include(T x, T y, T x_padding, T y_padding) -> void requires (N == 2) {
  translate_to_include(Coord(x, y), Coord(x_padding, y_padding));
}

template <typename T, std::size_t N>
auto Box<T, N>::scale(T x, T y) -> void requires (N == 2) {
  scale(Coord(x, y));
}

template <typename T, std::size_t N>
auto Box<T, N>::expand(T x, T y) -> void requires (N == 2) {
  expand(Coord(x, y));
}

template <typename T, std::size_t N>
auto Box<T, N>::flip_horizontal() -> void requires (N == 2) {
  flip(0);
}

template <typename T, std::size_t N>
auto Box<T, N>::flip_vertical() -> void requires (N == 2) {
  flip(1);
}

template <typename T, std::size_t N>
auto Box<T, N>::edges() const -> Array<Line, 4> requires (N == 2) {
  Array<Line, 4> res;
  res[0] = {min(), {min()[0], max()[1]}};
  res[1] = {min(), {max()[0], min()[1]}};
  res[2] = {{min()[0], max()[1]}, max()};
  res[3] = {{max()[0], min()[1]}, max()};
  return res;
}

template <typename T, std::size_t N>
auto Box<T, N>::intersects(Line const& l) const -> bool requires (N == 2) {
  if (contains(l.min()) || contains(l.max()))
    return true;

  for (auto const& edge : edges()) {
    if (l.intersects(edge))
      return true;
  }
  return false;
}

template <typename T, std::size_t N>
auto Box<T, N>::intersects_circle(Coord const& position, T radius) const -> bool requires (N == 2) {
  if (contains(position))
    return true;
  if (radius < T{})
    return false;

  // Take the difference before converting integral coordinates. Even opposite
  // signed extremes have a representable unsigned distance.
  auto distance = [](T a, T b) {
    if constexpr (std::integral<T>) {
      using U = std::make_unsigned_t<T>;
      return static_cast<U>(a < b ? U(b) - U(a) : U(a) - U(b));
    } else {
      return std::abs(a - b);
    }
  };
  auto [left, right] = std::minmax(m_min[0], m_max[0]);
  auto [bottom, top] = std::minmax(m_min[1], m_max[1]);
  auto dx = distance(position[0], std::clamp(position[0], left, right));
  auto dy = distance(position[1], std::clamp(position[1], bottom, top));
  auto r = static_cast<decltype(dx)>(radius);
  if (dx == 0 && dy == 0) {
    // contains() already accepted ordered boxes. A reversed box has only
    // its four edges, not a filled interior.
    return std::min({distance(position[0], left), distance(position[0], right),
        distance(position[1], bottom), distance(position[1], top)}) <= r;
  }
  if (dx == 0 || dy == 0)
    return std::max(dx, dy) <= r;
  using Metric = std::conditional_t<std::integral<T>, long double, T>;
  return std::hypot(static_cast<Metric>(dx), static_cast<Metric>(dy)) <= static_cast<Metric>(r);
}

// returns the closest intersection point (from l.min())
template <typename T, std::size_t N>
auto Box<T, N>::edge_intersection(Line const& l) const -> LineIntersectResult requires (N == 2) {
  Array<LineIntersectResult, 4> candidates;
  std::size_t num_candidates = 0;

  for (auto const& edge : edges()) {
    auto res = l.intersection(edge);
    if (res.intersects)
      candidates[num_candidates++] = res;
  }

  if (num_candidates == 0)
    return {};

  if (num_candidates == 2) {
    if (candidates[1].t < candidates[0].t)
      std::swap(candidates[0], candidates[1]);
  } else if (num_candidates > 2) {
    // Corners and coincident edges can tie. Keep the original four-edge
    // traversal and sort here: the selected point and t are observable.
    std::sort(candidates.begin(), candidates.begin() + num_candidates,
        [](LineIntersectResult const& a, LineIntersectResult const& b) { return a.t < b.t; });
  }

  auto& first = candidates[0];
  if (num_candidates <= 2) {
    if (contains(l.min(), false) || contains(l.max(), false)
        || (num_candidates == 2 && contains(l.min()) && !candidates[1].glances))
      first.glances = false;
    if (num_candidates == 2)
      first.coincides |= candidates[1].coincides;
  } else if (num_candidates == 3) {
    // Either the line runs along a side or it crosses a corner and a side.
    first.coincides |= candidates[1].coincides || candidates[2].coincides;
    first.glances = first.coincides;
  } else {
    first.glances = false;
    first.coincides = false;
  }
  return first;
}

template <typename T, std::size_t N>
auto Box<T, N>::subtract(Box const& rect) const -> SmallList<Box, 4> requires (N == 2) {
  SmallList<Box, 4> regions;
  auto cut = overlap(rect);
  if (cut.is_empty()) {
    regions.push_back(*this);
    return regions;
  }

  // The vertical strips own the corners; the horizontal strips span only
  // the clipped width. Weather samples these regions in emission order.
  if (x_min() < cut.x_min())
    regions.emplace_back(x_min(), y_min(), cut.x_min(), y_max());
  if (cut.x_max() < x_max())
    regions.emplace_back(cut.x_max(), y_min(), x_max(), y_max());
  if (y_min() < cut.y_min())
    regions.emplace_back(cut.x_min(), y_min(), cut.x_max(), cut.y_min());
  if (cut.y_max() < y_max())
    regions.emplace_back(cut.x_min(), cut.y_max(), cut.x_max(), y_max());

  return regions;
}

template <typename T, std::size_t N>
auto Box<T, N>::nearest_coord_to(Coord const& c) const -> Coord {
  Coord result = c;
  for (std::size_t i = 0; i < N; ++i)
    result[i] = clamp(result[i], m_min[i], m_max[i]);
  return result;
}

}

export module star.rect;

export template <typename T, std::size_t N>
  requires Star::nonfallible_text_writable<Star::Vector<T, N>>
struct std::formatter<Star::Box<T, N>> : Star::TextFormatter {};

export namespace Star {
  using ::Star::Box;
  using ::Star::Rect;
  using ::Star::RectI;
  using ::Star::RectU;
  using ::Star::RectF;
  using ::Star::RectD;
  using ::Star::write_text;
}
