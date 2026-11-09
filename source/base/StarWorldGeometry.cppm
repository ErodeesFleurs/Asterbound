module;
import std;
import star.format;
import star.list;

import star.poly;

import star.vector;
import star.line;
import star.rect;
import star.math_common;
namespace Star {

// Utility class for dealing with the non-euclidean nature of the World.
// Handles the surprisingly complex job of deciding intersections and splitting
// geometry across the world wrap boundary.
class WorldGeometry {
public:
  // A null WorldGeometry will have diff / wrap methods etc be the normal
  // euclidean variety.
  WorldGeometry();
  WorldGeometry(unsigned width, unsigned height);
  WorldGeometry(Vec2U const& size);

  bool isNull() const;

  bool operator==(WorldGeometry const& other) const;
  bool operator!=(WorldGeometry const& other) const;

  unsigned width() const;
  unsigned height() const;
  Vec2U size() const;

  // Wrap given point back into world space by wrapping x
  int xwrap(int x) const;
  float xwrap(float x) const;
  // Only wraps x component.
  Vec2F xwrap(Vec2F const& pos) const;
  Vec2I xwrap(Vec2I const& pos) const;

  // y value is clamped to be in the range [0, height)
  float yclamp(float y) const;

  // Wraps and clamps position
  Vec2F limit(Vec2F const& pos) const;

  bool crossesWrap(float xMin, float xMax) const;

  // Do these two inexes point to the same location
  bool equal(Vec2I const& p1, Vec2I const& p2) const;

  // Same as wrap, returns unsigned type.
  unsigned index(int x) const;
  Vec2U index(Vec2I const& i) const;

  // returns right only distance from x2 to x1 (or x1 - x2).  Always positive.
  int pdiff(int x1, int x2) const;

  // Shortest difference between two given points.  Always returns diff on the
  // "side" that x1 is on.
  float diff(float x1, float x2) const;
  int diff(int x1, int x2) const;

  // Same but for 2d vectors
  Vec2F diff(Vec2F const& p1, Vec2F const& p2) const;
  Vec2I diff(Vec2I const& p1, Vec2I const& p2) const;

  // Midpoint of the shortest line connecting two points.
  Vec2F midpoint(Vec2F const& p1, Vec2F const& p2) const;

  std::function<float(float, float)> xDiffFunction() const;
  std::function<Vec2F(Vec2F, Vec2F)> diffFunction() const;
  std::function<float(float, float, float)> xLerpFunction(std::optional<float> discontinuityThreshold = {}) const;
  std::function<Vec2F(float, Vec2F, Vec2F)> lerpFunction(std::optional<float> discontinuityThreshold = {}) const;

  // Wrapping functions are not guaranteed to work for objects larger than
  // worldWidth / 2.  Bad things can happen.

  // Split the given Rect across world boundaries.
  [[nodiscard]] std::expected<StaticList<RectF, 2>, StarException> splitRect(RectF const& bbox) const;
  // Split the given Rect after translating it by position.
  [[nodiscard]] std::expected<StaticList<RectF, 2>, StarException> splitRect(RectF bbox, Vec2F const& position) const;

  [[nodiscard]] std::expected<StaticList<RectI, 2>, StarException> splitRect(RectI bbox) const;

  // Same but for Line
  [[nodiscard]] std::expected<StaticList<Line2F, 2>, StarException> splitLine(Line2F line, bool preserveDirection = false) const;
  [[nodiscard]] std::expected<StaticList<Line2F, 2>, StarException> splitLine(Line2F line, Vec2F const& position, bool preserveDirection = false) const;

  // Same but for Poly
  [[nodiscard]] std::expected<StaticList<PolyF, 2>, StarException> splitPoly(PolyF const& poly) const;
  [[nodiscard]] std::expected<StaticList<PolyF, 2>, StarException> splitPoly(PolyF poly, Vec2F const& position) const;

  // Split a horizontal region of the world across the world wrap point.
  [[nodiscard]] std::expected<StaticList<Vec2I, 2>, StarException> splitXRegion(Vec2I const& xRegion) const;
  [[nodiscard]] std::expected<StaticList<Vec2F, 2>, StarException> splitXRegion(Vec2F const& xRegion) const;

  [[nodiscard]] std::expected<bool, StarException> rectContains(RectF const& rect1, Vec2F const& pos) const;
  [[nodiscard]] std::expected<bool, StarException> rectIntersectsRect(RectF const& rect1, RectF const& rect2) const;
  RectF rectOverlap(RectF const& rect1, RectF const& rect2) const;
  [[nodiscard]] std::expected<bool, StarException> polyContains(PolyF const& poly, Vec2F const& pos) const;
  [[nodiscard]] std::expected<float, StarException> polyOverlapArea(PolyF const& poly1, PolyF const& poly2) const;

  [[nodiscard]] std::expected<bool, StarException> lineIntersectsRect(Line2F const& line, RectF const& rect) const;
  [[nodiscard]] std::expected<bool, StarException> lineIntersectsPoly(Line2F const& line, PolyF const& poly) const;
  [[nodiscard]] std::expected<bool, StarException> polyIntersectsPoly(PolyF const& poly1, PolyF const& poly2) const;

  [[nodiscard]] std::expected<bool, StarException> rectIntersectsCircle(RectF const& rect, Vec2F const& center, float radius) const;
  [[nodiscard]] std::expected<bool, StarException> lineIntersectsCircle(Line2F const& line, Vec2F const& center, float radius) const;

  [[nodiscard]] std::expected<std::optional<Vec2F>, StarException> lineIntersectsPolyAt(Line2F const& line, PolyF const& poly) const;

  // Returns the distance from a point to any part of the given poly
  float polyDistance(PolyF const& poly, Vec2F const& point) const;

  // Produces a point that is on the same "side" of the world as the source point.
  int nearestTo(int source, int target) const;
  float nearestTo(float source, float target) const;
  Vec2I nearestTo(Vec2I const& source, Vec2I const& target) const;
  Vec2F nearestTo(Vec2F const& source, Vec2F const& target) const;

  Vec2F nearestCoordInBox(RectF const& box, Vec2F const& pos) const;
  Vec2F diffToNearestCoordInBox(RectF const& box, Vec2F const& pos) const;

private:
  Vec2U m_size;
};

inline WorldGeometry::WorldGeometry()
  : m_size(Vec2U()) {}

inline WorldGeometry::WorldGeometry(unsigned width, unsigned height)
  : m_size(width, height) {}

inline WorldGeometry::WorldGeometry(Vec2U const& size)
  : m_size(size) {}

inline bool WorldGeometry::isNull() const {
  return m_size == Vec2U();
}

inline bool WorldGeometry::operator==(WorldGeometry const& other) const {
  return m_size == other.m_size;
}

inline bool WorldGeometry::operator!=(WorldGeometry const& other) const {
  return m_size != other.m_size;
}

inline unsigned WorldGeometry::width() const {
  return m_size[0];
}

inline unsigned WorldGeometry::height() const {
  return m_size[1];
}

inline Vec2U WorldGeometry::size() const {
  return m_size;
}

inline int WorldGeometry::xwrap(int x) const {
  if (m_size[0] == 0)
    return x;
  else
    return pmod<int>(x, m_size[0]);
}

inline float WorldGeometry::xwrap(float x) const {
  if (m_size[0] == 0)
    return x;
  else
    return pfmod<float>(x, m_size[0]);
}

inline Vec2F WorldGeometry::xwrap(Vec2F const& pos) const {
  return {xwrap(pos[0]), pos[1]};
}

inline Vec2I WorldGeometry::xwrap(Vec2I const& pos) const {
  return {xwrap(pos[0]), pos[1]};
}

inline float WorldGeometry::yclamp(float y) const {
  return std::clamp<float>(y, 0, std::nextafter(m_size[1], 0.0f));
}

inline Vec2F WorldGeometry::limit(Vec2F const& pos) const {
  return {xwrap(pos[0]), yclamp(pos[1])};
}

inline bool WorldGeometry::crossesWrap(float xMin, float xMax) const {
  return xwrap(xMax) < xwrap(xMin);
}

inline bool WorldGeometry::equal(Vec2I const& p1, Vec2I const& p2) const {
  return index(p1) == index(p2);
}

inline unsigned WorldGeometry::index(int x) const { return static_cast<unsigned>(xwrap(x)); }

inline Vec2U WorldGeometry::index(Vec2I const& i) const {
  return Vec2U(xwrap(i[0]), i[1]);
}

inline int WorldGeometry::pdiff(int x1, int x2) const {
  if (m_size[0] == 0)
    return x1 - x2;
  else
    return pmod<int>(x1 - x2, m_size[0]);
}

inline float WorldGeometry::diff(float x1, float x2) const {
  if (m_size[0] == 0)
    return x1 - x2;
  else
    return wrapDiffF<float>(x1, x2, m_size[0]);
}

inline int WorldGeometry::diff(int x1, int x2) const {
  if (m_size[0] == 0)
    return x1 - x2;
  else
    return wrapDiff<int>(x1, x2, m_size[0]);
}

inline Vec2F WorldGeometry::diff(Vec2F const& p1, Vec2F const& p2) const {
  float xdiff = diff(p1[0], p2[0]);
  return {xdiff, p1[1] - p2[1]};
}

inline Vec2I WorldGeometry::diff(Vec2I const& p1, Vec2I const& p2) const {
  int xdiff = diff(p1[0], p2[0]);
  return {xdiff, p1[1] - p2[1]};
}

inline Vec2F WorldGeometry::midpoint(Vec2F const& p1, Vec2F const& p2) const {
  return xwrap(diff(p1, p2) / 2 + p2);
}

inline int WorldGeometry::nearestTo(int source, int target) const {
  if (std::abs(target - source) < static_cast<int>((m_size[0] / 2)))
    return target;
  else
    return diff(target, source) + source;
}

inline float WorldGeometry::nearestTo(float source, float target) const {
  if (std::abs(target - source) < static_cast<float>((m_size[0] / 2)))
    return target;
  else
    return diff(target, source) + source;
}

inline Vec2I WorldGeometry::nearestTo(Vec2I const& source, Vec2I const& target) const {
  return Vec2I(nearestTo(source[0], target[0]), target[1]);
}

inline Vec2F WorldGeometry::nearestTo(Vec2F const& source, Vec2F const& target) const {
  return Vec2F(nearestTo(source[0], target[0]), target[1]);
}

}

export module star.world_geometry;

export namespace Star {
  using ::Star::WorldGeometry;
}
