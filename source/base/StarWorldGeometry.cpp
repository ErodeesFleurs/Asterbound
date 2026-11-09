import std;
import star.format;
import star.list;
import star.poly;
import star.world_geometry;

import star.array;
import star.vector;
import star.line;
import star.rect;
import star.math_common;
import star.algorithm;
namespace Star {

std::function<float(float, float)> WorldGeometry::xDiffFunction() const {
  if (m_size[0] == 0) {
    return [](float x1, float x2) -> float { return x1 - x2; };
  } else {
    unsigned xsize = m_size[0];
    return [xsize](float x1, float x2) -> float { return wrapDiffF<float>(x1, x2, xsize); };
  }
}

std::function<Vec2F(Vec2F, Vec2F)> WorldGeometry::diffFunction() const {
  if (m_size[0] == 0) {
    return [](Vec2F const& a, Vec2F const& b) -> Vec2F { return a - b; };
  } else {
    unsigned xsize = m_size[0];
    return [xsize](Vec2F const& a, Vec2F const& b) -> Vec2F {
      return Vec2F(wrapDiffF<float>(a[0], b[0], xsize), a[1] - b[1]);
    };
  }
}

std::function<float(float, float, float)> WorldGeometry::xLerpFunction(std::optional<float> discontinuityThreshold) const {
  if (m_size[0] == 0) {
    return [](float, float min, float) -> float { return min; };
  } else {
    unsigned xsize = m_size[0];
    return [discontinuityThreshold, xsize](float offset, float min, float max) -> float {
      float distance = wrapDiffF<float>(max, min, xsize);
      if (discontinuityThreshold && std::abs(distance) > *discontinuityThreshold)
        return min + distance;
      return min + offset * distance;
    };
  }
}

std::function<Vec2F(float, Vec2F, Vec2F)> WorldGeometry::lerpFunction(std::optional<float> discontinuityThreshold) const {
  if (m_size[0] == 0) {
    return [](float, Vec2F const& min, Vec2F const&) -> Vec2F { return min; };
  } else {
    unsigned xsize = m_size[0];
    return [discontinuityThreshold, xsize](float offset, Vec2F const& min, Vec2F const& max) -> Vec2F {
      Vec2F distance = Vec2F(wrapDiffF<float>(max[0], min[0], xsize), max[1] - min[1]);
      if (discontinuityThreshold && distance.magnitude() > *discontinuityThreshold)
        return min + distance;
      return min + offset * distance;
    };
  }
}

std::expected<StaticList<RectF, 2>, StarException> WorldGeometry::splitRect(RectF const& bbox) const {
  if (bbox.isNull() || m_size[0] == 0)
    return StaticList<RectF, 2>::create({bbox});

  Vec2F minWrap = xwrap(bbox.min());
  RectF bboxWrap = RectF(minWrap, minWrap + bbox.size());

  // This does not work for ranges greater than m_size[0] wide!
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((bbox.xMax() - bbox.xMin() <= static_cast<float>(m_size[0])));
  }

  // Since min is wrapped, we're only checking to see if max is on the other
  // side of the wrap point
  if (bboxWrap.xMax() > m_size[0]) {
    return StaticList<RectF, 2>::create({RectF(bboxWrap.xMin(), bboxWrap.yMin(), m_size[0], bboxWrap.yMax()),
        RectF(0, bboxWrap.yMin(), bboxWrap.xMax() - m_size[0], bboxWrap.yMax())});
  } else {
    return StaticList<RectF, 2>::create({bboxWrap});
  }
}

std::expected<StaticList<RectF, 2>, StarException> WorldGeometry::splitRect(RectF bbox, Vec2F const& position) const {
  bbox.translate(position);
  return splitRect(bbox);
}

std::expected<StaticList<RectI, 2>, StarException> WorldGeometry::splitRect(RectI const bbox) const {
  if (bbox.isNull() || m_size[0] == 0)
    return StaticList<RectI, 2>::create({bbox});

  Vec2I minWrap = xwrap(bbox.min());
  RectI bboxWrap = RectI(minWrap, minWrap + bbox.size());

  // This does not work for ranges greater than m_size[0] wide!
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((bbox.xMax() - bbox.xMin() <= static_cast<int>(m_size[0])));
  }

  // Since min is wrapped, we're only checking to see if max is on the other
  // side of the wrap point
  if (bboxWrap.xMax() > static_cast<int>(m_size[0])) {
    return StaticList<RectI, 2>::create({RectI(bboxWrap.xMin(), bboxWrap.yMin(), m_size[0], bboxWrap.yMax()),
        RectI(0, bboxWrap.yMin(), bboxWrap.xMax() - m_size[0], bboxWrap.yMax())});
  } else {
    return StaticList<RectI, 2>::create({bboxWrap});
  }
}

std::expected<StaticList<Line2F, 2>, StarException> WorldGeometry::splitLine(Line2F line, bool preserveDirection) const {
  if (m_size[0] == 0)
    return StaticList<Line2F, 2>::create({line});

  bool swapDirection = line.make_positive() && preserveDirection;
  Vec2F minWrap = xwrap(line.min());

  // diff is safe because we're looking for the line gnostic diff
  Line2F lineWrap = Line2F(minWrap, minWrap + line.diff());

  // Since min is wrapped, we're only checking to see if max is on the other
  // side of the wrap point
  if (lineWrap.max()[0] > m_size[0]) {
    Vec2F intersection = lineWrap.intersection(Line2F(Vec2F(m_size[0], 0), Vec2F(m_size)), true).point;
    if (swapDirection)
      return StaticList<Line2F, 2>::create({Line2F(lineWrap.max() - Vec2F(m_size[0], 0), Vec2F(0, intersection[1])),
          Line2F(Vec2F(m_size[0], intersection[1]), lineWrap.min())});
    else
      return StaticList<Line2F, 2>::create({Line2F(lineWrap.min(), Vec2F(m_size[0], intersection[1])),
          Line2F(Vec2F(0, intersection[1]), lineWrap.max() - Vec2F(m_size[0], 0))});
  } else {
    if (swapDirection)
      lineWrap.reverse();
    return StaticList<Line2F, 2>::create({lineWrap});
  }
}

std::expected<StaticList<Line2F, 2>, StarException> WorldGeometry::splitLine(Line2F line, Vec2F const& position, bool preserveDirection) const {
  line.translate(position);
  return splitLine(line, preserveDirection);
}

std::expected<StaticList<PolyF, 2>, StarException> WorldGeometry::splitPoly(PolyF const& poly) const {
  if (poly.isNull() || m_size[0] == 0)
    return StaticList<PolyF, 2>::create({poly});

  Array<PolyF, 2> res;
  bool polySelect = false;

  Line2F worldBoundRight = {Vec2F(m_size[0], 0), Vec2F(m_size[0], 1)};
  Line2F worldBoundLeft = {Vec2F(0, 0), Vec2F(0, 1)};

  for (unsigned i = 0; i < poly.sides(); i++) {
    Line2F segment = poly.side(i);
    if ((segment.min()[0] < 0) ^ (segment.max()[0] < 0)) {
      Vec2F worldCorrect = {static_cast<float>(m_size[0]), 0};
      Vec2F intersect = segment.intersection(worldBoundLeft, true).point;
      if (segment.min()[0] < 0) {
        res[polySelect].add(segment.min() + worldCorrect);
        res[polySelect].add(Vec2F(m_size[0], intersect[1]));
        polySelect = !polySelect;
        res[polySelect].add(Vec2F(0, intersect[1]));
      } else {
        res[polySelect].add(segment.min());
        res[polySelect].add(Vec2F(0, intersect[1]));
        polySelect = !polySelect;
        res[polySelect].add(Vec2F(m_size[0], intersect[1]));
      }
    } else if ((segment.min()[0] > m_size[0]) ^ (segment.max()[0] > m_size[0])) {
      Vec2F worldCorrect = {static_cast<float>(m_size[0]), 0};
      Vec2F intersect = segment.intersection(worldBoundRight, true).point;
      if (segment.min()[0] > m_size[0]) {
        res[polySelect].add(segment.min() - worldCorrect);
        res[polySelect].add(Vec2F(0, intersect[1]));
        polySelect = !polySelect;
        res[polySelect].add(Vec2F(m_size[0], intersect[1]));
      } else {
        res[polySelect].add(segment.min());
        res[polySelect].add(Vec2F(m_size[0], intersect[1]));
        polySelect = !polySelect;
        res[polySelect].add(Vec2F(0, intersect[1]));
      }
    } else {
      if (segment.min()[0] < 0) {
        res[polySelect].add(segment.min() + Vec2F(static_cast<float>(m_size[0]), 0));
      } else if (segment.min()[0] > m_size[0]) {
        res[polySelect].add(segment.min() - Vec2F(static_cast<float>(m_size[0]), 0));
      } else {
        res[polySelect].add(segment.min());
      }
    }
  }

  if (res[1].isNull())
    return StaticList<PolyF, 2>::create({res[0]});
  if (res[0].isNull())
    return StaticList<PolyF, 2>::create({res[1]});
  else
    return StaticList<PolyF, 2>::create({res[0], res[1]});
}

std::expected<StaticList<PolyF, 2>, StarException> WorldGeometry::splitPoly(PolyF poly, Vec2F const& position) const {
  poly.translate(position);
  return splitPoly(poly);
}

std::expected<StaticList<Vec2I, 2>, StarException> WorldGeometry::splitXRegion(Vec2I const& xRegion) const {
  if (m_size[0] == 0)
    return StaticList<Vec2I, 2>::create({xRegion});

  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((xRegion[1] >= xRegion[0]));
  }

  // This does not work for ranges greater than m_size[0] wide!
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((xRegion[1] - xRegion[0] <= static_cast<int>(m_size[0])));
  }

  int x1 = xwrap(xRegion[0]);
  int x2 = x1 + xRegion[1] - xRegion[0];

  if (x2 > static_cast<int>(m_size[0])) {
    return StaticList<Vec2I, 2>::create({Vec2I(x1, m_size[0]), Vec2I(0.0f, x2 - m_size[0])});
  } else {
    return StaticList<Vec2I, 2>::create({Vec2I(x1, x2)});
  }
}

std::expected<StaticList<Vec2F, 2>, StarException> WorldGeometry::splitXRegion(Vec2F const& xRegion) const {
  if (m_size[0] == 0)
    return StaticList<Vec2F, 2>::create({xRegion});

  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((xRegion[1] >= xRegion[0]));
  }

  // This does not work for ranges greater than m_size[0] wide!
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((xRegion[1] - xRegion[0] <= static_cast<float>(m_size[0])));
  }

  float x1 = xwrap(xRegion[0]);
  float x2 = x1 + xRegion[1] - xRegion[0];

  if (x2 > m_size[0]) {
    return StaticList<Vec2F, 2>::create({Vec2F(x1, m_size[0]), Vec2F(0.0f, x2 - m_size[0])});
  } else {
    return StaticList<Vec2F, 2>::create({Vec2F(x1, x2)});
  }
}

std::expected<bool, StarException> WorldGeometry::rectContains(RectF const& rect, Vec2F const& pos) const {
  auto wpos = xwrap(pos);
  auto pieces = splitRect(rect);
  if (!pieces) return std::unexpected(std::move(pieces.error()));
  for (auto const& r : *pieces) {
    if (r.contains(wpos))
      return true;
  }
  return false;
}

std::expected<bool, StarException> WorldGeometry::rectIntersectsRect(RectF const& rect1, RectF const& rect2) const {
  auto first = splitRect(rect1);
  if (!first) return std::unexpected(std::move(first.error()));
  for (auto const& r1 : *first) {
    auto second = splitRect(rect2);
    if (!second) return std::unexpected(std::move(second.error()));
    for (auto const& r2 : *second) {
      if (r1.intersects(r2))
        return true;
    }
  }
  return false;
}

RectF WorldGeometry::rectOverlap(RectF const& rect1, RectF const& rect2) const {
  return rect1.overlap(RectF::withSize(nearestTo(rect1.min(), rect2.min()), rect2.size()));
}

std::expected<bool, StarException> WorldGeometry::polyContains(PolyF const& poly, Vec2F const& pos) const {
  auto wpos = xwrap(pos);
  auto pieces = splitPoly(poly);
  if (!pieces) return std::unexpected(std::move(pieces.error()));
  for (auto const& p : *pieces) {
    if (p.contains(wpos))
      return true;
  }
  return false;
}

std::expected<float, StarException> WorldGeometry::polyOverlapArea(PolyF const& poly1, PolyF const& poly2) const {
  float area = 0.0f;
  auto first = splitPoly(poly1);
  if (!first) return std::unexpected(std::move(first.error()));
  for (auto const& p1 : *first) {
    auto second = splitPoly(poly2);
    if (!second) return std::unexpected(std::move(second.error()));
    for (auto const& p2 : *second)
      area += PolyF::clip(p1, p2).convexArea();
  }
  return area;
}

std::expected<bool, StarException> WorldGeometry::lineIntersectsRect(Line2F const& line, RectF const& rect) const {
  auto lines = splitLine(line, false);
  if (!lines) return std::unexpected(std::move(lines.error()));
  for (auto l : *lines) {
    auto boxes = splitRect(rect);
    if (!boxes) return std::unexpected(std::move(boxes.error()));
    for (auto box : *boxes) {
      if (box.intersects(l)) {
        return true;
      }
    }
  }
  return false;
}

std::expected<bool, StarException> WorldGeometry::lineIntersectsPoly(Line2F const& line, PolyF const& poly) const {
  auto lines = splitLine(line, false);
  if (!lines) return std::unexpected(std::move(lines.error()));
  for (auto a : *lines) {
    auto polys = splitPoly(poly);
    if (!polys) return std::unexpected(std::move(polys.error()));
    for (auto const& b : *polys) {
      if (b.intersects(a)) {
        return true;
      }
    }
  }

  return false;
}

std::expected<bool, StarException> WorldGeometry::polyIntersectsPoly(PolyF const& polyA, PolyF const& polyB) const {
  auto first = splitPoly(polyA);
  if (!first) return std::unexpected(std::move(first.error()));
  for (auto const& a : *first) {
    auto second = splitPoly(polyB);
    if (!second) return std::unexpected(std::move(second.error()));
    for (auto const& b : *second) {
      if (b.intersects(a))
        return true;
    }
  }

  return false;
}

std::expected<bool, StarException> WorldGeometry::rectIntersectsCircle(RectF const& rect, Vec2F const& center, float radius) const {
  if (rect.contains(center))
    return true;
  for (auto const& e : rect.edges()) {
    auto intersects = lineIntersectsCircle(e, center, radius);
    if (!intersects) return std::unexpected(std::move(intersects.error()));
    if (*intersects)
      return true;
  }
  return false;
}

std::expected<bool, StarException> WorldGeometry::lineIntersectsCircle(Line2F const& line, Vec2F const& center, float radius) const {
  auto lines = splitLine(line, false);
  if (!lines) return std::unexpected(std::move(lines.error()));
  for (auto const& sline : *lines) {
    if (sline.distance_to(nearestTo(sline.center(), center)) <= radius)
      return true;
  }
  return false;
}

std::expected<std::optional<Vec2F>, StarException> WorldGeometry::lineIntersectsPolyAt(Line2F const& line, PolyF const& poly) const {
  auto lines = splitLine(line, true);
  if (!lines) return std::unexpected(std::move(lines.error()));
  for (auto a : *lines) {
    auto polys = splitPoly(poly);
    if (!polys) return std::unexpected(std::move(polys.error()));
    for (auto const& b : *polys) {
      if (auto intersection = b.lineIntersection(a))
        return intersection->point;
    }
  }

  return {};
}

float WorldGeometry::polyDistance(PolyF const& poly, Vec2F const& point) const {
  auto spoint = nearestTo(poly.center(), point);
  return poly.distance(spoint);
}

Vec2F WorldGeometry::nearestCoordInBox(RectF const& box, Vec2F const& pos) const {
  RectF t(box);
  auto offset = t.center();
  auto r = diff(pos, offset);
  t.setCenter({});
  return t.nearestCoordTo(r) + offset;
}

Vec2F WorldGeometry::diffToNearestCoordInBox(RectF const& box, Vec2F const& pos) const {
  RectF t(box);
  auto offset = t.center();
  auto r = diff(pos, offset);
  t.setCenter({});
  auto coord = t.nearestCoordTo(r) + offset;
  return diff(pos, coord);
}

}
