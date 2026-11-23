import std;
import star.format;
import star.list;
import star.poly;
import star.world_geometry;

import star.vector;
import star.line;
import star.rect;
import star.math_common;

namespace Star {

std::function<float(float, float)> WorldGeometry::x_diff_function() const {
  if (width() == 0) {
    return [](float x1, float x2) -> float { return x1 - x2; };
  } else {
    unsigned world_width = width();
    return [world_width](float x1, float x2) -> float { return wrap_difference<float>(x1, x2, world_width); };
  }
}

std::function<Vec2F(Vec2F, Vec2F)> WorldGeometry::diff_function() const {
  if (width() == 0) {
    return [](Vec2F const& a, Vec2F const& b) -> Vec2F { return a - b; };
  } else {
    unsigned world_width = width();
    return [world_width](Vec2F const& a, Vec2F const& b) -> Vec2F {
      return Vec2F(wrap_difference<float>(a[0], b[0], world_width), a[1] - b[1]);
    };
  }
}

std::function<float(float, float, float)> WorldGeometry::x_lerp_function(std::optional<float> discontinuity_threshold) const {
  if (width() == 0) {
    return [](float, float min, float) -> float { return min; };
  } else {
    unsigned world_width = width();
    return [discontinuity_threshold, world_width](float offset, float min, float max) -> float {
      float distance = wrap_difference<float>(max, min, world_width);
      if (discontinuity_threshold && std::abs(distance) > *discontinuity_threshold)
        return min + distance;
      return min + offset * distance;
    };
  }
}

std::function<Vec2F(float, Vec2F, Vec2F)> WorldGeometry::lerp_function(std::optional<float> discontinuity_threshold) const {
  if (width() == 0) {
    return [](float, Vec2F const& min, Vec2F const&) -> Vec2F { return min; };
  } else {
    unsigned world_width = width();
    return [discontinuity_threshold, world_width](float offset, Vec2F const& min, Vec2F const& max) -> Vec2F {
      Vec2F distance = Vec2F(wrap_difference<float>(max[0], min[0], world_width), max[1] - min[1]);
      if (discontinuity_threshold && distance.magnitude() > *discontinuity_threshold)
        return min + distance;
      return min + offset * distance;
    };
  }
}

namespace {

template <typename T>
concept interval_coordinate = std::same_as<T, int> || std::same_as<T, float>;

enum class IntervalSource { Rectangle, Region };

template <IntervalSource source, interval_coordinate T, std::invocable<T, T> MakePiece>
auto split_x_interval(WorldGeometry const& geometry, T min, T max, MakePiece make_piece)
    -> std::expected<StaticList<std::invoke_result_t<MakePiece, T, T>, 2>, StarException> {
  using Pieces = StaticList<std::invoke_result_t<MakePiece, T, T>, 2>;
  using Wide = std::conditional_t<std::same_as<T, int>, std::int64_t, float>;
  Wide width = geometry.width();
  if (width == 0)
    return Pieces::create({make_piece(min, max)});

  Wide length = static_cast<Wide>(max) - min;
  if constexpr (Star::debug_enabled) {
    if constexpr (source == IntervalSource::Region)
      Star::star_assert_failed((max >= min));
    Star::star_assert_failed((length <= width));
  }
  Wide start;
  if constexpr (std::same_as<T, int>)
    start = geometry.index(min);
  else
    start = geometry.xwrap(min);

  // Rectangles translate their size; floating-point regions historically add
  // the old end before subtracting the old start. Keep both rounding orders.
  Wide end;
  if constexpr (source == IntervalSource::Region && std::same_as<T, float>)
    end = start + max - min;
  else
    end = start + length;

  bool crosses = end > width;
  if (crosses)
    end -= width;
  if constexpr (std::same_as<T, int>) {
    if (!std::in_range<int>(start) || !std::in_range<int>(end) || (crosses && !std::in_range<int>(width)))
      return std::unexpected(OutOfRangeException("Wrapped interval boundary is outside the integer coordinate range"));
  }
  if (crosses)
    return Pieces::create({make_piece(static_cast<T>(start), static_cast<T>(width)), make_piece(T{}, static_cast<T>(end))});
  return Pieces::create({make_piece(static_cast<T>(start), static_cast<T>(end))});
}

template <interval_coordinate T>
auto split_rectangle(WorldGeometry const& geometry, Rect<T> const& box)
    -> std::expected<StaticList<Rect<T>, 2>, StarException> {
  if (box.is_null() || geometry.width() == 0)
    return StaticList<Rect<T>, 2>::create({box});
  T top = box.y_max();
  if constexpr (std::same_as<T, float>)
    top = box.y_min() + (top - box.y_min());
  return split_x_interval<IntervalSource::Rectangle>(geometry, box.x_min(), box.x_max(),
      [&](T min, T max) { return Rect<T>(min, box.y_min(), max, top); });
}

}

std::expected<StaticList<RectF, 2>, StarException> WorldGeometry::split_rect(RectF const& bbox) const {
  return split_rectangle(*this, bbox);
}

std::expected<StaticList<RectF, 2>, StarException> WorldGeometry::split_rect(RectF bbox, Vec2F const& position) const {
  bbox.translate(position);
  return split_rect(bbox);
}

std::expected<StaticList<RectI, 2>, StarException> WorldGeometry::split_rect(RectI bbox) const {
  return split_rectangle(*this, bbox);
}

std::expected<StaticList<Line2F, 2>, StarException> WorldGeometry::split_line(Line2F line, bool preserve_direction) const {
  if (width() == 0)
    return StaticList<Line2F, 2>::create({line});

  bool swap_direction = line.make_positive() && preserve_direction;
  Vec2F wrapped_min = xwrap(line.min());
  Line2F wrapped_line(wrapped_min, wrapped_min + line.diff());
  float world_width = static_cast<float>(width());
  if (wrapped_line.max()[0] > world_width) {
    Vec2F intersection = wrapped_line.intersection(Line2F(Vec2F(world_width, 0), Vec2F(size())), true).point;
    Vec2F end = wrapped_line.max() - Vec2F(world_width, 0);
    Vec2F left(0, intersection[1]);
    Vec2F right(world_width, intersection[1]);
    if (swap_direction)
      return StaticList<Line2F, 2>::create({Line2F(end, left), Line2F(right, wrapped_min)});
    return StaticList<Line2F, 2>::create({Line2F(wrapped_min, right), Line2F(left, end)});
  }
  if (swap_direction)
    wrapped_line.reverse();
  return StaticList<Line2F, 2>::create({wrapped_line});
}

std::expected<StaticList<Line2F, 2>, StarException> WorldGeometry::split_line(Line2F line, Vec2F const& position, bool preserve_direction) const {
  line.translate(position);
  return split_line(line, preserve_direction);
}

namespace {

using PolygonStorage = std::array<PolyF, 2>;

// Empty span means no translation or splitting was needed. Only the caller
// requesting an owning result needs to copy (or move) that original polygon.
std::span<PolyF> split_polygon(PolyF const& poly, float width, PolygonStorage& storage) {
  if (width == 0)
    return {};
  auto const& vertices = poly.vertices();
  auto outside = std::ranges::find_if(vertices, [width](Vec2F const& v) { return v[0] < 0 || v[0] > width; });
  if (outside == vertices.end())
    return {};

  // The edge entering the first outside vertex may cross the seam. All
  // earlier edges are canonical; copy that prefix instead of scanning it again.
  auto first_edge = outside == vertices.begin() ? outside : std::prev(outside);
  auto& prefix = storage[0].vertices();
  prefix.insert(prefix.end(), vertices.begin(), first_edge);
  bool selected = false;
  Vec2F shift(width, 0);
  for (std::size_t i = first_edge - vertices.begin(); i < vertices.size(); ++i) {
    Line2F edge = poly.side(i);
    auto const& start = edge.min();
    float end_x = edge.max()[0];
    bool left = (start[0] < 0) != (end_x < 0);
    bool right = (start[0] > width) != (end_x > width);
    if (left || right) {
      float seam = left ? 0 : width;
      float other = left ? width : 0;
      bool starts_outside = left ? start[0] < 0 : start[0] > width;
      float y = edge.intersection(Line2F({seam, 0}, {seam, 1}), true).point[1];
      if (starts_outside) {
        storage[selected].add(left ? start + shift : start - shift);
        std::swap(seam, other);
      } else {
        storage[selected].add(start);
      }
      storage[selected].add({seam, y});
      selected = !selected;
      storage[selected].add({other, y});
    } else if (start[0] < 0) {
      storage[selected].add(start + shift);
    } else if (start[0] > width) {
      storage[selected].add(start - shift);
    } else {
      storage[selected].add(start);
    }
  }
  return std::span(storage).first(storage[1].empty() ? 1 : 2);
}

std::span<PolyF const> polygon_parts(PolyF const& poly, float width, PolygonStorage& storage) {
  auto parts = split_polygon(poly, width, storage);
  if (parts.empty())
    return {std::addressof(poly), 1};
  return parts;
}

template <typename Poly>
  requires std::same_as<std::remove_cvref_t<Poly>, PolyF>
std::expected<StaticList<PolyF, 2>, StarException> split_owned_polygon(Poly&& poly, float width) {
  PolygonStorage storage;
  auto parts = split_polygon(poly, width, storage);
  if (!parts.empty())
    return StaticList<PolyF, 2>::create(std::make_move_iterator(parts.begin()), std::make_move_iterator(parts.end()));
  StaticList<PolyF, 2> result;
  if (auto added = result.push_back(std::forward<Poly>(poly)); !added)
    return std::unexpected(std::move(added.error()));
  return result;
}

}

std::expected<StaticList<PolyF, 2>, StarException> WorldGeometry::split_poly(PolyF const& poly) const {
  return split_owned_polygon(poly, width());
}

std::expected<StaticList<PolyF, 2>, StarException> WorldGeometry::split_poly(PolyF poly, Vec2F const& position) const {
  poly.translate(position);
  return split_owned_polygon(std::move(poly), width());
}

std::expected<StaticList<Vec2I, 2>, StarException> WorldGeometry::split_x_region(Vec2I const& x_region) const {
  return split_x_interval<IntervalSource::Region>(*this, x_region[0], x_region[1],
      [](int min, int max) { return Vec2I(min, max); });
}

std::expected<StaticList<Vec2F, 2>, StarException> WorldGeometry::split_x_region(Vec2F const& x_region) const {
  return split_x_interval<IntervalSource::Region>(*this, x_region[0], x_region[1],
      [](float min, float max) { return Vec2F(min, max); });
}

std::expected<bool, StarException> WorldGeometry::rect_contains(RectF const& rect, Vec2F const& pos) const {
  auto wrapped_pos = xwrap(pos);
  auto pieces = split_rect(rect);
  if (!pieces) return std::unexpected(std::move(pieces.error()));
  for (auto const& r : *pieces) {
    if (r.contains(wrapped_pos))
      return true;
  }
  return false;
}

std::expected<bool, StarException> WorldGeometry::rect_intersects_rect(RectF const& rect1, RectF const& rect2) const {
  auto first = split_rect(rect1);
  if (!first) return std::unexpected(std::move(first.error()));
  auto second = split_rect(rect2);
  if (!second) return std::unexpected(std::move(second.error()));
  for (auto const& r1 : *first) {
    for (auto const& r2 : *second) {
      if (r1.intersects(r2))
        return true;
    }
  }
  return false;
}

RectF WorldGeometry::rect_overlap(RectF const& rect1, RectF const& rect2) const {
  return rect1.overlap(RectF::with_size(nearest_to(rect1.min(), rect2.min()), rect2.size()));
}

std::expected<bool, StarException> WorldGeometry::poly_contains(PolyF const& poly, Vec2F const& pos) const {
  auto wrapped_pos = xwrap(pos);
  PolygonStorage storage;
  for (auto const& p : polygon_parts(poly, width(), storage)) {
    if (p.contains(wrapped_pos))
      return true;
  }
  return false;
}

std::expected<float, StarException> WorldGeometry::poly_overlap_area(PolyF const& poly1, PolyF const& poly2) const {
  float area = 0.0f;
  PolygonStorage first_storage, second_storage;
  auto first = polygon_parts(poly1, width(), first_storage);
  auto second = polygon_parts(poly2, width(), second_storage);
  for (auto const& p1 : first) {
    for (auto const& p2 : second)
      area += PolyF::clip(p1, p2).signed_area();
  }
  return area;
}

std::expected<bool, StarException> WorldGeometry::line_intersects_rect(Line2F const& line, RectF const& rect) const {
  auto lines = split_line(line, false);
  if (!lines) return std::unexpected(std::move(lines.error()));
  auto boxes = split_rect(rect);
  if (!boxes) return std::unexpected(std::move(boxes.error()));
  for (auto const& l : *lines) {
    for (auto const& box : *boxes) {
      if (box.intersects(l))
        return true;
    }
  }
  return false;
}

std::expected<bool, StarException> WorldGeometry::line_intersects_poly(Line2F const& line, PolyF const& poly) const {
  auto lines = split_line(line, false);
  if (!lines) return std::unexpected(std::move(lines.error()));
  PolygonStorage storage;
  auto polys = polygon_parts(poly, width(), storage);
  for (auto const& a : *lines) {
    for (auto const& b : polys) {
      if (b.intersects(a))
        return true;
    }
  }

  return false;
}

std::expected<bool, StarException> WorldGeometry::poly_intersects_poly(PolyF const& poly_a, PolyF const& poly_b) const {
  PolygonStorage first_storage, second_storage;
  auto first = polygon_parts(poly_a, width(), first_storage);
  auto second = polygon_parts(poly_b, width(), second_storage);
  for (auto const& a : first) {
    for (auto const& b : second) {
      if (b.intersects(a))
        return true;
    }
  }

  return false;
}

std::expected<bool, StarException> WorldGeometry::rect_intersects_circle(RectF const& rect, Vec2F const& center, float radius) const {
  if (rect.contains(center))
    return true;
  // A wrapped copy of the center may be inside without hitting any edge.
  // Unlike splitting the rectangle, this also handles a full period or more.
  if (width() != 0 && center[1] >= rect.y_min() && center[1] <= rect.y_max()) {
    double extent = static_cast<double>(rect.x_max()) - rect.x_min();
    double period = static_cast<float>(width());
    if (extent >= period)
      return true;
    double offset = static_cast<double>(xwrap(center[0])) - xwrap(rect.x_min());
    if ((offset < 0 ? offset + period : offset) <= extent)
      return true;
  }
  for (auto const& e : rect.edges()) {
    auto intersects = line_intersects_circle(e, center, radius);
    if (!intersects) return std::unexpected(std::move(intersects.error()));
    if (*intersects)
      return true;
  }
  return false;
}

std::expected<bool, StarException> WorldGeometry::line_intersects_circle(Line2F const& line, Vec2F const& center, float radius) const {
  auto lines = split_line(line, false);
  if (!lines) return std::unexpected(std::move(lines.error()));
  for (auto const& segment : *lines) {
    if (segment.distance_to(nearest_to(segment.center(), center)) <= radius)
      return true;
  }
  return false;
}

std::expected<std::optional<Vec2F>, StarException> WorldGeometry::line_intersects_poly_at(Line2F const& line, PolyF const& poly) const {
  auto lines = split_line(line, true);
  if (!lines) return std::unexpected(std::move(lines.error()));
  PolygonStorage storage;
  auto polys = polygon_parts(poly, width(), storage);
  for (auto const& a : *lines) {
    for (auto const& b : polys) {
      if (auto intersection = b.line_intersection(a))
        return intersection->point;
    }
  }

  return {};
}

float WorldGeometry::poly_distance(PolyF const& poly, Vec2F const& point) const {
  return poly.distance(nearest_to(poly.center(), point));
}

Vec2F WorldGeometry::nearest_coord_in_box(RectF const& box, Vec2F const& pos) const {
  RectF centered(box);
  auto offset = centered.center();
  auto relative = diff(pos, offset);
  centered.set_center({});
  return centered.nearest_coord_to(relative) + offset;
}

Vec2F WorldGeometry::diff_to_nearest_coord_in_box(RectF const& box, Vec2F const& pos) const {
  return diff(pos, nearest_coord_in_box(box, pos));
}

}
