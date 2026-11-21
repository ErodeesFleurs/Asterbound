import std;
import star.rect;

#include "gtest/gtest.h"

import star.vector;
import star.line;
import star.math_common;
using namespace Star;

TEST(RectTest, SubtractPartitionsWeatherSpawnZonesInOrder) {
  RectI viewport(0, 0, 10, 10);
  auto check = [&](RectI previous, std::initializer_list<RectI> expected) {
    auto zones = viewport.subtract(previous);
    ASSERT_EQ(zones.size(), expected.size());
    int area = 0;
    for (std::size_t i = 0; i < zones.size(); ++i) {
      EXPECT_EQ(zones[i], expected.begin()[i]);
      EXPECT_TRUE(viewport.contains(zones[i]));
      EXPECT_FALSE(zones[i].intersects(previous, false));
      for (std::size_t j = 0; j < i; ++j)
        EXPECT_FALSE(zones[i].intersects(zones[j], false));
      area += zones[i].volume();
    }
    auto cut = viewport.overlap(previous);
    EXPECT_EQ(area, viewport.volume() - (cut.is_empty() ? 0 : cut.volume()));
  };
  check({2, 3, 8, 7}, {{0, 0, 2, 10}, {8, 0, 10, 10}, {2, 0, 8, 3}, {2, 7, 8, 10}});
  check({-5, 3, 15, 7}, {{0, 0, 10, 3}, {0, 7, 10, 10}});
  check({-5, 3, 8, 7}, {{8, 0, 10, 10}, {0, 0, 8, 3}, {0, 7, 8, 10}});
  check({10, 0, 20, 10}, {viewport});
  check({-1, -1, 11, 11}, {});
}

TEST(RectTest, CircleTouchesCornersAndCollapsedOrReversedEdges) {
  RectF box(0, 0, 8, 8);
  EXPECT_TRUE(box.intersects_circle({4, 4}, -1));
  EXPECT_FALSE(box.intersects_circle({4, 9}, -1));
  EXPECT_TRUE(box.intersects_circle({11, 12}, 5));
  EXPECT_FALSE(box.intersects_circle({11, 12}, 4));
  EXPECT_TRUE(RectF(2, 3, 2, 3).intersects_circle({5, 7}, 5));
  EXPECT_TRUE(RectF(2, 0, 2, 8).intersects_circle({5, 4}, 3));
  RectI reversed(8, 8, 0, 0);
  EXPECT_FALSE(reversed.intersects_circle({4, 4}, 3));
  EXPECT_TRUE(reversed.intersects_circle({4, 4}, 4));
  EXPECT_TRUE(reversed.intersects_circle({8, 4}, 0));
  EXPECT_FALSE(reversed.intersects_circle({8, 4}, -1));
  constexpr int low = std::numeric_limits<int>::lowest();
  constexpr int high = std::numeric_limits<int>::max();
  EXPECT_FALSE(RectI(low, 0, low, 0).intersects_circle({high, 0}, high));
}

TEST(RectTest, EdgeContactsKeepDirectedTimeAndMetadata) {
  RectF box(0, 0, 8, 8);
  auto check = [&](Line2F line, Vec2F point, float t, bool coincides, bool glances) {
    EXPECT_TRUE(box.intersects(line));
    auto hit = box.edge_intersection(line);
    ASSERT_TRUE(hit.intersects);
    EXPECT_EQ(hit.point, point);
    EXPECT_EQ(hit.t, t);
    EXPECT_EQ(hit.coincides, coincides);
    EXPECT_EQ(hit.glances, glances);
  };
  check({{12, 4}, {-4, 4}}, {8, 4}, 0.25f, false, false);
  check({{-1, 1}, {1, -1}}, {0, 0}, 0.5f, false, true);
  check({{-4, 0}, {12, 0}}, {0, 0}, 0.25f, true, true);
  check({{12, 12}, {-4, -4}}, {8, 8}, 0.25f, false, false);
  Line2F inside({1, 2}, {3, 4});
  EXPECT_TRUE(box.intersects(inside));
  EXPECT_FALSE(box.edge_intersection(inside).intersects);
}

namespace {
template <typename B>
concept has_rect_edges = requires(B const& box) { box.edges(); };

template <typename Input>
concept rect_bound_input = requires(Input const& input) { RectI::bound_box_of(input); };

template <typename Range>
concept rect_point_range = requires(Range const& range) { RectI::bound_box_of_points(range); };

static_assert(has_rect_edges<RectI> && !has_rect_edges<Box<int, 3>>);
static_assert(std::constructible_from<RectI, int, int, int, int>);
static_assert(!std::constructible_from<Box<int, 3>, int, int, int, int>);
static_assert(rect_bound_input<Vec2I> && rect_bound_input<RectI> && !rect_bound_input<int>);
static_assert(rect_point_range<std::array<Vec2I, 2>> && !rect_point_range<int>);
static_assert(std::same_as<decltype(std::declval<RectI&>().min()), Vec2I&>);
static_assert(std::same_as<decltype(std::declval<RectI const&>().max()), Vec2I const&>);
static_assert(std::same_as<decltype(std::declval<RectI&&>().max()), Vec2I>);
static_assert(std::same_as<decltype(std::declval<RectI const&&>().min()), Vec2I>);
static_assert(RectI(1, 2, 3, 4).min()[1] == 2);
}

TEST(RectTest, TranslateToInclude) {
  RectF rect;
  rect = {0, 0, 10, 10};
  rect.translate_to_include(20, 20, 2, 2);
  EXPECT_TRUE(rect.x_max() < 22.01 && rect.x_max() > 21.99);
  EXPECT_TRUE(rect.y_max() < 22.01 && rect.y_max() > 21.99);

  rect = {0, 0, 10, 10};
  rect.translate_to_include(-20, -20, 2, 2);
  EXPECT_TRUE(rect.x_min() > -22.01 && rect.x_min() < -21.99);
  EXPECT_TRUE(rect.y_min() > -22.01 && rect.y_min() < -21.99);

  rect = {0, 0, 10, 10};
  rect.translate_to_include(5, 5, 3, 3);
  EXPECT_TRUE(rect.x_min() < 0.01 && rect.x_min() > -0.01);
  EXPECT_TRUE(rect.y_min() < 0.01 && rect.y_min() > -0.01);
}

TEST(RectTest, BoxGlanceCorner) {
  RectF rect1 = {0, 0, 10, 10};
  RectF rect2 = {-10, -10, 0, 0};
  auto res = rect1.intersection(rect2);

  EXPECT_TRUE(!res.intersects);
  EXPECT_TRUE(res.glances);
  EXPECT_TRUE(vmag(res.overlap) < 0.0001f);
}

TEST(RectTest, BoxGlanceEdge) {
  RectF rect1 = {0, 0, 10, 10};
  RectF rect2 = {-10, 0, 0, 10};
  auto res = rect1.intersection(rect2);

  EXPECT_TRUE(!res.intersects);
  EXPECT_TRUE(res.glances);
  EXPECT_TRUE(vmag(res.overlap) < 0.0001f);
}

TEST(RectTest, BoxIntersectionNone) {
  RectF rect1 = {0, 0, 10, 10};
  RectF rect2 = {-10, 0, -1, 10};
  auto res = rect1.intersection(rect2);

  EXPECT_TRUE(!res.intersects);
  EXPECT_TRUE(!res.glances);
}

TEST(RectTest, BoxIntersectionOverlapX) {
  RectF rect1 = {0, 0, 10, 10};
  RectF rect2 = {7, 6, 10, 10};
  auto res = rect1.intersection(rect2);

  EXPECT_TRUE(res.intersects);
  EXPECT_TRUE(!res.glances);
  EXPECT_TRUE(std::fabs(res.overlap[0] + 3) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.overlap[1]) < 0.0001f);
}

TEST(RectTest, BoxIntersectionOverlapY) {
  RectF rect1 = {0, 0, 10, 10};
  RectF rect2 = {5, 6, 10, 10};
  auto res = rect1.intersection(rect2);

  EXPECT_TRUE(res.intersects);
  EXPECT_TRUE(!res.glances);
  EXPECT_TRUE(std::fabs(res.overlap[0]) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.overlap[1] + 4) < 0.0001f);
}

TEST(RectTest, ContainsPoint) {
  Box<float, 4> rect1 = {{0, 0, 0, 0}, {10, 10, 10, 10}};
  Vec4F point1 = {5, 5, 5, 10};
  Vec4F point2 = {-10, 0, 0, 0};
  Vec4F point3 = {5, 4, 3, 2};
  Vec4F point4 = {5, 4, 3, -2};

  EXPECT_TRUE(rect1.contains(point1));
  EXPECT_FALSE(rect1.contains(point1, false));
  EXPECT_FALSE(rect1.contains(point2));
  EXPECT_FALSE(rect1.contains(point2, false));
  EXPECT_TRUE(rect1.contains(point3));
  EXPECT_TRUE(rect1.contains(point3, false));
  EXPECT_FALSE(rect1.contains(point4));
  EXPECT_FALSE(rect1.contains(point4, false));
}

TEST(RectTest, EdgeIntersection) {
  RectF rect1 = {10, 10, 20, 20};

  Line2F line0_1 = {{3, 3}, {4, 4}};
  auto res = rect1.edge_intersection(line0_1);
  EXPECT_FALSE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);

  Line2F line1_1 = {{9, 12}, {10, 12}};
  res = rect1.edge_intersection(line1_1);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_TRUE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 12) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.t - 1.0f) < 0.0001f);

  Line2F line1_2 = {{9, 12}, {11, 12}};
  res = rect1.edge_intersection(line1_2);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 12) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.t - 0.5f) < 0.0001f);

  Line2F line1_3 = {{10, 12}, {11, 12}};
  res = rect1.edge_intersection(line1_3);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 12) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.t) < 0.0001f);

  Line2F line1_4 = {{10, 12}, {10, 13}};
  res = rect1.edge_intersection(line1_4);
  EXPECT_TRUE(res.intersects);
  EXPECT_TRUE(res.coincides);
  EXPECT_TRUE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 12) < 0.0001f);

  Line2F line2_1 = {{10, 10}, {11, 10}};
  res = rect1.edge_intersection(line2_1);
  EXPECT_TRUE(res.intersects);
  EXPECT_TRUE(res.coincides);
  EXPECT_TRUE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line2_2 = {{15, 9}, {20, 15}};
  res = rect1.edge_intersection(line2_2);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line2_3 = {{15, 9}, {21, 15}};
  res = rect1.edge_intersection(line2_3);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line2_4 = {{15, 10}, {21, 15}};
  res = rect1.edge_intersection(line2_4);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line2_5 = {{15, 11}, {20, 15}};
  res = rect1.edge_intersection(line2_5);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 20) < 0.0001f);

  Line2F line2_6 = {{9, 9}, {11, 11}};
  res = rect1.edge_intersection(line2_6);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line2_7 = {{9, 11}, {11, 9}};
  res = rect1.edge_intersection(line2_7);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_TRUE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line2_8 = {{10, 10.5}, {10, 10}};
  res = rect1.edge_intersection(line2_8);
  EXPECT_TRUE(res.intersects);
  EXPECT_TRUE(res.coincides);
  EXPECT_TRUE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 10.5) < 0.0001f);

  Line2F line2_9 = {{10, 10}, {10, 10.5}};
  res = rect1.edge_intersection(line2_9);
  EXPECT_TRUE(res.intersects);
  EXPECT_TRUE(res.coincides);
  EXPECT_TRUE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line3_1 = {{10, 10}, {20, 10}};
  res = rect1.edge_intersection(line3_1);
  EXPECT_TRUE(res.intersects);
  EXPECT_TRUE(res.coincides);
  EXPECT_TRUE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line3_2 = {{9, 10}, {21, 10}};
  res = rect1.edge_intersection(line3_2);
  EXPECT_TRUE(res.intersects);
  EXPECT_TRUE(res.coincides);
  EXPECT_TRUE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line3_3 = {{9, 8}, {15, 20}};
  res = rect1.edge_intersection(line3_3);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line3_4 = {{9, 8}, {16, 22}};
  res = rect1.edge_intersection(line3_4);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line4_1 = {{9, 9}, {21, 21}};
  res = rect1.edge_intersection(line4_1);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 10) < 0.0001f);

  Line2F line4_2 = {{9, 21}, {21, 9}};
  res = rect1.edge_intersection(line4_2);
  EXPECT_TRUE(res.intersects);
  EXPECT_FALSE(res.coincides);
  EXPECT_FALSE(res.glances);
  EXPECT_TRUE(std::fabs(res.point[0] - 10) < 0.0001f);
  EXPECT_TRUE(std::fabs(res.point[1] - 20) < 0.0001f);
}

TEST(RectTest, Center) {
  RectU a(0, 0, 10, 10);

  a.set_center(Vec2U(5, 5));
  EXPECT_EQ(a, RectU(0, 0, 10, 10));

  a.set_center(Vec2U(10, 10));
  EXPECT_EQ(a, RectU(5, 5, 15, 15));

  a.set_center(Vec2U(5, 5));
  EXPECT_EQ(a, RectU(0, 0, 10, 10));
}

TEST(RectTest, ThreeDimensionalAxisRangesAndExpansion) {
  using Box3 = Box<float, 3>;
  Box3 original{{1, -2, 3}, {5, 4, 11}};
  EXPECT_EQ(original.range(2), Vec2F(3, 11));

  auto axes = original;
  axes.set_range(0, {6, 6});
  axes.set_range(2, {12, 11});
  axes.range_set_if_empty({{-1, 20, -3}, {7, 30, 13}});
  EXPECT_EQ(axes, Box3({-1, -2, -3}, {7, 4, 13}));
  axes.scale(Vec3F(2, 3, 4));
  EXPECT_EQ(axes, Box3({-2, -6, -12}, {14, 12, 52}));

  EXPECT_EQ(original.expanded(2), Box3({-1, -5, -1}, {7, 7, 15}));
  original.expand(Vec3F(0.5f, 3, 1));
  EXPECT_EQ(original, Box3({2, -8, 3}, {4, 10, 11}));
}

TEST(RectTest, ThreeDimensionalAspectAndCube) {
  using Box3 = Box<float, 3>;
  Box3 original{{0, 0, 0}, {8, 4, 6}};
  Vec3F aspect{2, 1, 0.5f};
  auto grow = original;
  ASSERT_TRUE(grow.set_aspect(aspect));
  EXPECT_EQ(grow, Box3({-8, -4, 0}, {16, 8, 6}));
  auto shrink = original;
  ASSERT_TRUE(shrink.set_aspect(aspect, true));
  EXPECT_EQ(shrink, Box3({0, 0, 2}, {8, 4, 4}));
  ASSERT_TRUE(original.make_cube());
  EXPECT_EQ(original, Box3({0, -2, -1}, {8, 6, 7}));
}

TEST(RectTest, IntegralCentersTruncateTowardZeroWithoutOverflow) {
  constexpr int lo = std::numeric_limits<int>::lowest(), hi = std::numeric_limits<int>::max();
  EXPECT_EQ(RectI(lo, hi - 1, lo + 1, hi).center(), Vec2I(lo + 1, hi - 1));
  EXPECT_EQ(RectI(-2, hi, 1, lo).center(), Vec2I(0, 0));
  constexpr unsigned uhi = std::numeric_limits<unsigned>::max();
  EXPECT_EQ(RectU(uhi, 0, uhi - 1, uhi).center(), Vec2U(uhi - 1, uhi / 2));
  EXPECT_EQ(RectI::with_center({-3, 4}, {3, 5}), RectI(-4, 2, -2, 6));
  RectI point(lo, hi, lo, hi);
  point.set_center({hi, lo});
  EXPECT_EQ(point, RectI(hi, lo, hi, lo));
}

TEST(RectTest, IntegralAspectHandlesFullWidthAndRejectsInvalidResultsAtomically) {
  constexpr auto lo = std::numeric_limits<int>::lowest(), hi = std::numeric_limits<int>::max();
  auto full = RectI::inf();
  EXPECT_FALSE(full.set_aspect({1, 0}));
  EXPECT_FALSE(full.set_aspect({-1, 1}));
  EXPECT_FALSE(full.set_aspect({1, 2}));
  EXPECT_EQ(full, RectI::inf());
  ASSERT_TRUE(full.make_cube());
  EXPECT_EQ(full, RectI(-hi, -hi, hi, hi));

  RectI shifted{lo, hi, hi, hi};
  EXPECT_FALSE(shifted.make_cube());
  EXPECT_EQ(shifted, RectI(lo, hi, hi, hi));
  RectU near_origin{0, 0, 8, 2};
  EXPECT_FALSE(near_origin.make_cube());
  EXPECT_EQ(near_origin, RectU(0, 0, 8, 2));
  RectI reversed{4, 0, 2, 5};
  EXPECT_FALSE(reversed.set_aspect({1, 1}));
  EXPECT_EQ(reversed, RectI(4, 0, 2, 5));
}

template <typename B>
concept has_directed_box_intersection = requires(B const& b) { b.intersection(b); };

static_assert(!has_directed_box_intersection<Rect<std::uintmax_t>>);
static_assert(!has_directed_box_intersection<Rect<std::intmax_t>>);
static_assert(has_directed_box_intersection<RectU>);

TEST(RectTest, DirectedIntegerIntersectionWidensBeforeSubtracting) {
  RectI full = RectI::inf();
  constexpr auto span = static_cast<std::intmax_t>(std::numeric_limits<int>::max())
      - static_cast<std::intmax_t>(std::numeric_limits<int>::lowest());
  auto tied = full.intersection(full);
  ASSERT_TRUE(tied.intersects);
  EXPECT_FALSE(tied.glances);
  EXPECT_EQ(tied.overlap[0], -span);
  EXPECT_EQ(tied.overlap[1], 0);

  auto separated = RectU(2, 0, 3, 4).intersection(RectU(7, 0, 8, 4));
  EXPECT_FALSE(separated.intersects);
  EXPECT_FALSE(separated.glances);
  EXPECT_EQ(separated.overlap[0], -4);
  auto penetrating = RectU(0, 0, 4, 4).intersection(RectU(3, 0, 5, 4));
  EXPECT_TRUE(penetrating.intersects);
  EXPECT_EQ(penetrating.overlap[0], -1);
  auto touching = RectU(4, 0, 6, 4).intersection(RectU(6, 1, 9, 2));
  EXPECT_FALSE(touching.intersects);
  EXPECT_TRUE(touching.glances);
  EXPECT_EQ(touching.overlap[0], 0);

  using Wide = Rect<std::uintmax_t>;
  auto last = std::numeric_limits<std::uintmax_t>::max();
  EXPECT_FALSE(Wide(0, 0, 1, 1).intersects(Wide(last, 0, last, 1)));
}
