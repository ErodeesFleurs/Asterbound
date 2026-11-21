module;

import std;
import star.text_formatter;
import star.format;
import star.list;

import star.math_common;
import star.algorithm;
import star.vector;
import star.matrix3;
import star.line;
import star.rect;

namespace Star {

template <typename T>
concept polygon_coordinate = std::floating_point<T>
  || (std::integral<T> && !std::same_as<T, bool> && sizeof(T) <= sizeof(std::int32_t));

template <typename DataType>
class Polygon {
public:
  using Vertex = Vector<DataType, 2>;
  using Line = Star::Line<DataType, 2>;
  using Rect = Star::Box<DataType, 2>;
  using Measure = std::conditional_t<std::integral<DataType>, double, DataType>;

  struct IntersectResult {
    // Whether or not the two objects intersect
    bool intersects;
    // How much *this* poly must be moved in order to make them not intersect
    // anymore
    Vertex overlap;
  };

  struct LineIntersectResult {
    // Point of intersection
    Vertex point;
    // t value at the point of intersection of the line that was checked
    DataType along;
    // Side that the line first intersected, if the line starts inside the
    // polygon, this will not be set.
    std::optional<std::size_t> intersected_side;
  };

  using VertexList = List<Vertex>;
  using iterator = typename VertexList::iterator;
  using const_iterator = typename VertexList::const_iterator;

  static Polygon convex_hull(VertexList points) requires polygon_coordinate<DataType>;
  static Polygon clip(Polygon input, Polygon const& convex_clip) requires polygon_coordinate<DataType>;

  Polygon() = default;

  template <typename Other>
    requires std::constructible_from<Vertex, Vector<Other, 2> const&>
  explicit Polygon(Box<Other, 2> const& rect)
    : vertices_(rect_vertices(Vertex(rect.min()), Vertex(rect.max()))) {}

  template <typename Other>
    requires std::constructible_from<Vertex, Vector<Other, 2> const&>
  explicit Polygon(Polygon<Other> const& other) {
    vertices_.reserve(other.sides());
    for (auto const& point : other)
      vertices_.emplace_back(point);
  }

  // A single point is a valid operand for point-against-polygon SAT.
  explicit Polygon(Vertex const& point) : vertices_{point} {}

  // Supply the boundary in counterclockwise order.
  explicit Polygon(VertexList vertices) : vertices_(std::move(vertices)) {}
  Polygon(std::initializer_list<Vertex> vertices) : vertices_(vertices) {}

  bool empty() const noexcept { return vertices_.empty(); }
  bool is_convex() const requires polygon_coordinate<DataType>;
  Measure signed_area() const requires polygon_coordinate<DataType>;
  void deduplicate_vertices(Measure max_distance) requires polygon_coordinate<DataType>;

  void add(Vertex const& point) { vertices_.push_back(point); }
  // Wrapped indexing (remove, side, vertex, normal) requires a nonempty polygon.
  void remove(std::size_t i) { vertices_.erase(vertices_.begin() + wrapped_index(i)); }
  void clear() { vertices_.clear(); }

  // Borrows are invalidated by the same operations as the underlying vertex list.
  VertexList const& vertices() const & noexcept { return vertices_; }
  VertexList& vertices() & noexcept { return vertices_; }
  VertexList const& vertices() const && = delete;
  VertexList take_vertices() && noexcept { return std::move(vertices_); }

  std::size_t sides() const noexcept { return vertices_.size(); }
  Line side(std::size_t i) const { return side_at(wrapped_index(i)); }

  Measure distance(Vertex const& point) const requires polygon_coordinate<DataType>;

  void translate(Vertex offset)
    requires requires(Vertex& v) { v += offset; }
  {
    for (auto& v : vertices_)
      v += offset;
  }

  void set_center(Vertex const& point) requires polygon_coordinate<DataType> { translate(point - center()); }

  void rotate(DataType angle, Vertex origin = {})
    requires requires(Vertex& v) { v = (v - origin).rotate(angle) + origin; }
  {
    for (auto& v : vertices_)
      v = (v - origin).rotate(angle) + origin;
  }

  void scale(Vertex factors, Vertex origin = {})
    requires requires(Vertex& v) { v = vmult(v - origin, factors) + origin; }
  {
    for (auto& v : vertices_)
      v = vmult(v - origin, factors) + origin;
  }

  void scale(DataType factor, Vertex origin = {})
    requires requires { scale(Vertex::filled(factor), origin); }
  { scale(Vertex::filled(factor), origin); }

  void flip_horizontal(DataType axis = DataType()) requires polygon_coordinate<DataType> {
    scale(Vertex(-1, 1), Vertex(axis, 0));
    std::ranges::reverse(vertices_);
  }
  void flip_vertical(DataType axis = DataType()) requires polygon_coordinate<DataType> {
    scale(Vertex(1, -1), Vertex(0, axis));
    std::ranges::reverse(vertices_);
  }

  template <typename Other>
  void transform(Matrix3<Other> const& matrix)
    requires requires(Vertex& v) { v = matrix.transform_point(v); }
  {
    for (auto& v : vertices_)
      v = matrix.transform_point(v);
  }

  // Unlike vertex(), operator[] does not wrap.
  Vertex const& operator[](std::size_t i) const & { return vertices_[checked_index(i)]; }
  Vertex& operator[](std::size_t i) & { return vertices_[checked_index(i)]; }
  Vertex const& operator[](std::size_t) const && = delete;
  bool operator==(Polygon const&) const = default;

  iterator begin() & noexcept { return vertices_.begin(); }
  const_iterator begin() const & noexcept { return vertices_.begin(); }
  const_iterator begin() const && = delete;
  iterator end() & noexcept { return vertices_.end(); }
  const_iterator end() const & noexcept { return vertices_.end(); }
  const_iterator end() const && = delete;

  Vertex const& vertex(std::size_t i) const & { return vertices_[wrapped_index(i)]; }
  Vertex const& vertex(std::size_t) const && = delete;
  Vertex normal(std::size_t i) const requires std::floating_point<DataType> {
    return side(i).diff().rot90().normalized();
  }

  // Vertex arithmetic mean, not an area centroid. Both centers are zero when empty.
  Vertex center() const requires polygon_coordinate<DataType>;
  // Half the bounding width above its bottom, or center() if the box is wider than tall.
  Vertex bottom_center() const requires polygon_coordinate<DataType>;
  Rect bounding_box() const;
  int winding_number(Vertex const& point) const requires polygon_coordinate<DataType>;
  bool contains(Vertex const& point) const requires polygon_coordinate<DataType>;

  // Displacement queries use strict overlap; zero overlap is touching, not penetration.
  IntersectResult sat_intersection(Polygon const& other) const requires std::floating_point<DataType>;
  // choose_sign permits separation opposite to direction as well as along it.
  IntersectResult directional_sat_intersection(Polygon const& other, Vertex const& direction, bool choose_sign) const
    requires std::floating_point<DataType>;
  std::optional<LineIntersectResult> line_intersection(Line const& line) const requires std::floating_point<DataType>;
  bool intersects(Polygon const& other) const requires polygon_coordinate<DataType>;
  bool intersects(Line const& line) const requires polygon_coordinate<DataType>;
  // Strict SAT, including degenerate rectangles, exactly as intersects(Polygon(rect)).
  bool intersects_rect(Rect const& rect) const requires polygon_coordinate<DataType>;

private:
  static std::array<Vertex, 4> rect_vertices(Vertex const& low, Vertex const& high) {
    return {low, Vertex(high[0], low[1]), high, Vertex(low[0], high[1])};
  }

  std::size_t wrapped_index(std::size_t i) const {
    auto count = sides();
    if (i < count)
      return i;
    if (count == 0)
      Star::fatal_error("Cannot index an empty polygon", false);
    return i % count;
  }

  std::size_t checked_index(std::size_t i) const {
    if (i >= sides())
      Star::fatal_error("Polygon vertex index out of range", false);
    return i;
  }

  Line side_at(std::size_t i) const {
    return {vertices_[i], vertices_[i + 1 == sides() ? 0 : i + 1]};
  }

  VertexList vertices_;
};

template <typename DataType>
  requires nonfallible_text_writable<Vector<DataType, 2>>
void write_text(TextBuffer& output, Polygon<DataType> const& poly);

using PolyI = Polygon<int>;
using PolyF = Polygon<float>;
using PolyD = Polygon<double>;

namespace polygon_detail {

// Widen before subtraction as well as multiplication: a difference of
// 32-bit coordinates needs 33 signed bits.
template <polygon_coordinate T>
using Determinant = std::conditional_t<std::integral<T>, __int128, T>;

template <polygon_coordinate T>
constexpr auto orientation(Vector<T, 2> const& origin, Vector<T, 2> const& a, Vector<T, 2> const& b) -> Determinant<T> {
  using Wide = Determinant<T>;
  return (Wide(a[0]) - origin[0]) * (Wide(b[1]) - origin[1])
       - (Wide(a[1]) - origin[1]) * (Wide(b[0]) - origin[0]);
}

} // namespace polygon_detail

template <typename DataType>
Polygon<DataType> Polygon<DataType>::convex_hull(VertexList points) requires polygon_coordinate<DataType> {
  std::sort(points.begin(), points.end());
  points.erase(std::unique(points.begin(), points.end()), points.end());
  if (points.size() < 3)
    return Polygon(std::move(points));

  VertexList hull;
  hull.reserve(points.size() + 1);
  auto append = [&](Vertex const& point, std::size_t base) {
    while (hull.size() >= base + 2 && polygon_detail::orientation(hull[hull.size() - 2], hull.back(), point) <= 0)
      hull.pop_back();
    hull.push_back(point);
  };
  for (auto const& point : points)
    append(point, 0);
  auto upper_base = hull.size() - 1;
  for (auto i = points.size() - 1; i != 0; --i)
    append(points[i - 1], upper_base);
  hull.pop_back(); // The last point closes the loop.
  return Polygon(std::move(hull));
}

template <typename DataType>
Polygon<DataType> Polygon<DataType>::clip(Polygon input, Polygon const& boundary) requires polygon_coordinate<DataType> {
  if (input.empty() || boundary.sides() < 3)
    return {};

  // A point or a collinear boundary has no interior half-plane intersection.
  std::size_t corner = 2;
  while (corner < boundary.sides()
      && polygon_detail::orientation(boundary.vertices_.front(), boundary.vertices_[corner - 1], boundary.vertices_[corner]) == 0)
    ++corner;
  if (corner == boundary.sides())
    return {};

  VertexList scratch;
  scratch.reserve(input.sides() + 1);
  for (std::size_t i = 0; i < boundary.sides(); ++i) {
    auto const& a = boundary.vertices_[i];
    auto const& b = boundary.vertices_[i + 1 == boundary.sides() ? 0 : i + 1];
    if (a == b)
      continue;

    auto const& vertices = input.vertices_;
    auto last = vertices.size() - 1;
    auto previous = last;
    auto last_side = polygon_detail::orientation(a, b, vertices[last]);
    auto previous_side = last_side;
    scratch.clear();
    for (std::size_t j = 0; j <= last; ++j) {
      auto const& point = vertices[j];
      auto side = j == last ? last_side : polygon_detail::orientation(a, b, point);
      if ((side > 0) != (previous_side > 0)) {
        auto const& start = vertices[previous];
        if (side == 0) {
          scratch.push_back(point);
        } else if (previous_side == 0) {
          scratch.push_back(start);
        } else if constexpr (std::integral<DataType>) {
          // Truncate only the final integer vertex, not the interpolation ratio.
          auto coord = [&](std::size_t k) {
            return static_cast<DataType>((previous_side * point[k] - side * start[k]) / (previous_side - side));
          };
          scratch.emplace_back(coord(0), coord(1));
        } else {
          auto t = previous_side / (previous_side - side);
          scratch.push_back(start + (point - start) * t);
        }
      }
      if (side > 0)
        scratch.push_back(point);
      previous = j;
      previous_side = side;
    }
    if (scratch.empty())
      return {};
    input.vertices_.swap(scratch);
  }
  return input;
}

template <typename DataType>
bool Polygon<DataType>::is_convex() const requires polygon_coordinate<DataType> {
  if (sides() < 3)
    return true;

  std::size_t previous = sides() - 1;
  while (previous && vertices_[previous] == vertices_.front())
    --previous;
  std::size_t current = 0;
  for (std::size_t next = 1; next < sides(); ++next) {
    if (vertices_[next] == vertices_[current])
      continue;
    if (polygon_detail::orientation(vertices_[previous], vertices_[current], vertices_[next]) < 0)
      return false;
    previous = current;
    current = next;
  }
  return polygon_detail::orientation(vertices_[previous], vertices_[current], vertices_.front()) >= 0;
}

template <typename DataType>
auto Polygon<DataType>::signed_area() const -> Measure requires polygon_coordinate<DataType> {
  // Triangulate relative to a vertex so translating a small polygon far
  // from the origin does not make its area a difference of huge products.
  polygon_detail::Determinant<DataType> twice_area = 0;
  for (std::size_t i = 2; i < sides(); ++i)
    twice_area += polygon_detail::orientation(vertices_.front(), vertices_[i - 1], vertices_[i]);
  return static_cast<Measure>(twice_area) * Measure(0.5);
}

template <typename DataType>
void Polygon<DataType>::deduplicate_vertices(Measure max_distance) requires polygon_coordinate<DataType> {
  if (vertices_.size() < 2)
    return;

  auto limit = max_distance * max_distance;
  auto distinct = [&](Vertex const& a, Vertex const& b) {
    auto dx = Measure(a[0]) - Measure(b[0]);
    auto dy = Measure(a[1]) - Measure(b[1]);
    return dx * dx + dy * dy > limit;
  };
  std::size_t kept = 1;
  for (std::size_t i = 1; i < vertices_.size(); ++i) {
    if (distinct(vertices_[kept - 1], vertices_[i])) {
      if (kept != i)
        vertices_[kept] = vertices_[i];
      ++kept;
    }
  }
  if (kept > 1 && !distinct(vertices_.front(), vertices_[kept - 1]))
    --kept;
  vertices_.resize(kept);
}

template <typename DataType>
auto Polygon<DataType>::distance(Vertex const& point) const -> Measure requires polygon_coordinate<DataType> {
  if (contains(point))
    return 0;
  Measure nearest = std::numeric_limits<Measure>::max();
  using Position = Vector<Measure, 2>;
  Position query(point);
  for (std::size_t i = 0; i < sides(); ++i) {
    Position first(vertices_[i]);
    Position next(vertices_[i + 1 == sides() ? 0 : i + 1]);
    auto distance = first == next ? (query - first).magnitude()
                                  : Star::Line<Measure, 2>(first, next).distance_to(query);
    nearest = std::min(nearest, distance);
  }
  return nearest;
}

template <typename DataType>
auto Polygon<DataType>::center() const -> Vertex requires polygon_coordinate<DataType> {
  if (empty())
    return {};
  Vector<Measure, 2> sum;
  for (auto const& point : vertices_)
    sum += Vector<Measure, 2>(point);
  return Vertex(sum / static_cast<Measure>(sides()));
}

template <typename DataType>
auto Polygon<DataType>::bottom_center() const -> Vertex requires polygon_coordinate<DataType> {
  if (empty())
    return {};
  auto bounds = Rect::null();
  Vector<Measure, 2> sum;
  for (auto const& point : vertices_) {
    sum += Vector<Measure, 2>(point);
    bounds.combine(point);
  }
  Measure width = static_cast<Measure>(bounds.max()[0]) - bounds.min()[0];
  Measure height = static_cast<Measure>(bounds.max()[1]) - bounds.min()[1];
  Measure count = static_cast<Measure>(sides());
  return Vertex(static_cast<DataType>(sum[0] / count),
      static_cast<DataType>(width > height ? sum[1] / count : bounds.min()[1] + width / 2));
}

template <typename DataType>
auto Polygon<DataType>::bounding_box() const -> Rect {
  auto bounds = Rect::null();
  for (auto const& point : vertices_)
    bounds.combine(point);
  return bounds;
}

template <typename DataType>
int Polygon<DataType>::winding_number(Vertex const& point) const requires polygon_coordinate<DataType> {
  int winding = 0;
  for (std::size_t i = 0; i < sides(); ++i) {
    auto const& a = vertices_[i];
    auto const& b = vertices_[i + 1 == sides() ? 0 : i + 1];
    if (a[1] <= point[1]) {
      if (b[1] > point[1] && polygon_detail::orientation(a, b, point) > 0)
        ++winding;
    } else if (b[1] <= point[1] && polygon_detail::orientation(a, b, point) < 0) {
      --winding;
    }
  }
  return winding;
}

template <typename DataType>
bool Polygon<DataType>::contains(Vertex const& point) const requires polygon_coordinate<DataType> {
  return winding_number(point) != 0;
}

namespace polygon_detail {

// The order and signs of these axes are observable: equal depths keep the first
// axis, and movement also uses the signed correction for directional contacts.
template <polygon_coordinate T, std::size_t Extent, typename Visitor>
  requires std::invocable<Visitor&, Vector<Determinant<T>, 2> const&, Determinant<T>>
    && std::same_as<std::invoke_result_t<Visitor&, Vector<Determinant<T>, 2> const&, Determinant<T>>, bool>
bool visit_sat_projections(std::span<Vector<T, 2> const> left, std::span<Vector<T, 2> const, Extent> right, Visitor&& visit) {
  if (left.empty() || right.empty())
    return false;

  using Scalar = Determinant<T>;
  using Axis = Vector<Scalar, 2>;
  Axis first_axis;
  bool spans_plane = false;
  auto visit_edges = [&](auto polygon, bool reverse) {
    Axis previous(polygon.back());
    for (auto const& vertex : polygon) {
      Axis current(vertex);
      Axis axis = (previous - current).rot90();
      previous = current;
      if (axis == Axis{})
        continue;

      // The difference of points or parallel segments has no 2D interior.
      // Test the raw directions, not independently rounded unit normals.
      if (!spans_plane) {
        if (first_axis == Axis{})
          first_axis = axis;
        else {
          using Product = std::conditional_t<std::same_as<Scalar, float>, double, Scalar>;
          spans_plane = Product(first_axis[0]) * axis[1] != Product(first_axis[1]) * axis[0];
        }
      }
      if constexpr (std::floating_point<T>)
        axis = axis.normalized();
      if (reverse)
        axis = -axis;

      Scalar low = std::numeric_limits<Scalar>::max();
      Scalar high = std::numeric_limits<Scalar>::lowest();
      for (auto const& v : left)
        low = std::min(low, axis[0] * v[0] + axis[1] * v[1]);
      for (auto const& v : right)
        high = std::max(high, axis[0] * v[0] + axis[1] * v[1]);
      if (!visit(axis, high - low))
        return false;
    }
    return true;
  };
  return visit_edges(left, true) && visit_edges(right, false) && spans_plane;
}

template <polygon_coordinate T, std::size_t Extent>
bool sat_intersects(std::span<Vector<T, 2> const> left, std::span<Vector<T, 2> const, Extent> right) {
  return visit_sat_projections(left, right, [](Vector<Determinant<T>, 2> const&, Determinant<T> depth) {
    return depth > 0;
  });
}

// Inclusive segment contact, unlike strict SAT penetration. Keep collinear
// endpoints in query order so that reversing a ray returns its new first hit.
template <bool Locate, polygon_coordinate T>
  requires (!Locate || std::floating_point<T>)
auto segment_contact(Line<T, 2> const& line, Line<T, 2> const& edge)
    -> std::conditional_t<Locate, std::optional<T>, bool> {
  auto const& a = line.min();
  auto const& b = line.max();
  auto const& c = edge.min();
  auto const& d = edge.max();
  auto same_side = [](Determinant<T> x, Determinant<T> y) {
    return (x > 0 && y > 0) || (x < 0 && y < 0);
  };

  auto c_side = orientation(a, b, c);
  auto d_side = orientation(a, b, d);
  if (same_side(c_side, d_side))
    return {};
  if (c_side == 0 && d_side == 0) {
    if (a == b) {
      if (orientation(c, d, a) != 0)
        return {};
      for (std::size_t i = 0; i < 2; ++i)
        if (a[i] < std::min(c[i], d[i]) || a[i] > std::max(c[i], d[i]))
          return {};
      if constexpr (Locate)
        return T{0};
      else
        return true;
    }
    auto i = a[0] != b[0] ? 0 : 1;
    auto [low, high] = std::minmax(c[i], d[i]);
    if (std::max(a[i], b[i]) < low || std::min(a[i], b[i]) > high)
      return {};
    if constexpr (Locate)
      return (std::clamp(a[i], low, high) - a[i]) / (b[i] - a[i]);
    else
      return true;
  }

  auto a_side = orientation(c, d, a);
  auto b_side = orientation(c, d, b);
  if (same_side(a_side, b_side))
    return {};
  if constexpr (Locate)
    return a_side / (a_side - b_side);
  else
    return true;
}

} // namespace polygon_detail

template <typename DataType>
auto Polygon<DataType>::sat_intersection(Polygon const& other) const -> IntersectResult
  requires std::floating_point<DataType>
{
  auto shortest = std::numeric_limits<DataType>::max();
  Vertex separation;
  if (!polygon_detail::visit_sat_projections(std::span{vertices_}, std::span{other.vertices_}, [&](Vertex const& axis, DataType depth) {
        if (depth < shortest) {
          shortest = depth;
          separation = axis;
        }
        return true;
      }))
    return {};
  return {shortest > 0, separation * shortest};
}

template <typename DataType>
auto Polygon<DataType>::directional_sat_intersection(Polygon const& other, Vertex const& direction, bool choose_sign) const
    -> IntersectResult requires std::floating_point<DataType> {
  if (direction == Vertex{})
    return {};

  auto shortest = std::numeric_limits<DataType>::max();
  Vertex separation;
  auto visit = [&](Vertex const& axis, DataType depth) {
    if (depth <= 0) {
      if (depth < shortest) {
        shortest = depth;
        separation = axis;
      }
    } else {
      auto dot = direction * axis;
      if (dot != 0) {
        auto projected = depth / dot;
        auto distance = choose_sign ? std::abs(projected) : projected;
        if (distance >= 0 && distance < shortest) {
          shortest = distance;
          separation = choose_sign ? direction * (projected / distance) : direction;
        }
      }
    }
    return true;
  };
  if (!polygon_detail::visit_sat_projections(std::span{vertices_}, std::span{other.vertices_}, visit) || separation == Vertex{})
    return {};
  return {shortest > 0, separation * shortest};
}

template <typename DataType>
auto Polygon<DataType>::line_intersection(Line const& line) const -> std::optional<LineIntersectResult>
  requires std::floating_point<DataType>
{
  if (contains(line.min()))
    return LineIntersectResult{line.min(), DataType{0}, {}};

  std::optional<LineIntersectResult> nearest;
  for (std::size_t i = 0; i < vertices_.size(); ++i) {
    auto along = polygon_detail::segment_contact<true>(line, side_at(i));
    if (along && (!nearest || *along < nearest->along)) {
      nearest = LineIntersectResult{line.eval(*along), *along, i};
      if (*along == 0)
        break;
    }
  }
  return nearest;
}

template <typename DataType>
bool Polygon<DataType>::intersects(Polygon const& other) const requires polygon_coordinate<DataType> {
  return polygon_detail::sat_intersects(std::span{vertices_}, std::span{other.vertices_});
}

template <typename DataType>
bool Polygon<DataType>::intersects_rect(Rect const& rect) const requires polygon_coordinate<DataType> {
  auto const corners = rect_vertices(rect.min(), rect.max());
  return polygon_detail::sat_intersects(std::span{vertices_}, std::span{corners});
}

template <typename DataType>
bool Polygon<DataType>::intersects(Line const& line) const requires polygon_coordinate<DataType> {
  if (contains(line.min()))
    return true;
  for (std::size_t i = 0; i < vertices_.size(); ++i)
    if (polygon_detail::segment_contact<false>(line, side_at(i)))
      return true;
  return false;
}

template <typename DataType>
  requires nonfallible_text_writable<Vector<DataType, 2>>
void write_text(TextBuffer& output, Polygon<DataType> const& poly) {
  output.append("[Poly: ");
  for (auto i = poly.begin(); i != poly.end(); ++i) {
    if (i != poly.begin())
      output.append(", ");
    append_text(output, *i);
  }
  output.push_back(']');
}

}

export module star.poly;

export template <typename DataType>
  requires Star::nonfallible_text_writable<Star::Vector<DataType, 2>>
struct std::formatter<Star::Polygon<DataType>> : Star::TextFormatter {};

export namespace Star {
  using ::Star::polygon_coordinate;
  using ::Star::Polygon;
  using ::Star::PolyI;
  using ::Star::PolyF;
  using ::Star::PolyD;
  using ::Star::write_text;
}
