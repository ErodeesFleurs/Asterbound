import std;
import star.poly;
import star.random;

#include "gtest/gtest.h"

import star.vector;
import star.line;
import star.rect;
import star.math_common;
using namespace Star;

TEST(PolyTest, ConvexHull) {
  RandomService random(1947);
  PolyF::VertexList input_vertices;
  for (unsigned i = 0; i < 1000; ++i) {
    float angle = random.uniform_float() * 2 * std::numbers::pi;
    input_vertices.push_back({std::sin(angle), std::cos(angle)});
  }

  PolyF convex = PolyF::convex_hull(input_vertices);

  PolyF::VertexList test_vertices;
  for (unsigned i = 0; i < 1000; ++i) {
    float angle = random.uniform_float() * 2 * std::numbers::pi;
    test_vertices.push_back({std::sin(angle) * 0.75f, std::cos(angle) * 0.75f});
  }

  for (auto const& vertex : test_vertices)
    EXPECT_TRUE(convex.contains(vertex));
}

TEST(PolyTest, Distance) {
  PolyF square = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};

  EXPECT_TRUE(std::fabs(square.distance({-2.0f, -2.0f}) - std::numbers::sqrt2) < 0.00001f);
}

TEST(PolyTest, LineCollision) {
  PolyF square = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
  auto from_left = square.line_intersection(Line2F({-2, 0}, {2, 0}));
  ASSERT_TRUE(from_left);
  EXPECT_TRUE(vmag(Vec2F(-1, 0) - from_left->point) < 0.0001f);
  auto from_right = square.line_intersection(Line2F({2, 0}, {-2, 0}));
  ASSERT_TRUE(from_right);
  EXPECT_TRUE(vmag(Vec2F(1, 0) - from_right->point) < 0.0001f);
}

TEST(PolyTest, ConvexArea) {
  PolyF triangle = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {0.0f, 1.0f}};
  EXPECT_FLOAT_EQ(triangle.signed_area(), 2.0f);
}

TEST(PolyTest, Clipping) {
  PolyF triangle1 = {{-2.0f, -1.0f}, {2.0f, -1.0f}, {0.0f, 1.0f}};
  PolyF triangle2 = {{2.0f, 1.0f}, {-2.0f, 1.0f}, {0.0f, -1.0f}};
  PolyF overlap = PolyF::clip(triangle1, triangle2);
  overlap.deduplicate_vertices(0.0001f);
  EXPECT_EQ(overlap.sides(), 4u);
  EXPECT_NEAR(overlap.signed_area(), 2, 0.0001);
}

TEST(PolyTest, DegenerateHullAndAdjacentDuplicates) {
  PolyI point(Vec2I(3, 4));
  EXPECT_EQ(PolyI::convex_hull({{3, 4}}), point);
  EXPECT_EQ(PolyI::convex_hull({{3, 4}, {3, 4}, {3, 4}}), point);
  point.deduplicate_vertices(0);
  EXPECT_EQ(point.sides(), 1u);
  PolyF chain{{0, 0}, {0.75f, 0}, {1.5f, 0}, {0, 0}};
  chain.deduplicate_vertices(1);
  EXPECT_EQ(chain, (PolyF{{0, 0}, {1.5f, 0}}));
  EXPECT_EQ(PolyI::convex_hull({{4, 4}, {2, 2}, {0, 0}}), (PolyI{{0, 0}, {4, 4}}));
}

TEST(PolyTest, EmptyAndLowerDimensionalPenetration) {
  PolyF empty, point(Vec2F(0, 0));
  PolyF box{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  PolyF segment{{-1, 0}, {1, 0}};
  EXPECT_EQ(empty.center(), Vec2F());
  EXPECT_FALSE(empty.sat_intersection(empty).intersects);
  EXPECT_FALSE(box.intersects(empty));
  EXPECT_FALSE(point.intersects(point));
  EXPECT_FALSE(segment.intersects(PolyF{{0, 0}, {3, 0}}));
  EXPECT_TRUE(box.intersects(point));
  EXPECT_FALSE(box.intersects(PolyF(Vec2F(1, 0))));
  EXPECT_FALSE(box.directional_sat_intersection(point, Vec2F(), false).intersects);
  auto hit = box.line_intersection(Line2F({0, 0}, {2, 0}));
  ASSERT_TRUE(hit);
  EXPECT_FALSE(hit->intersected_side);
  EXPECT_DOUBLE_EQ(PolyI(Vec2I(3, 4)).distance(Vec2I()), 5.0);
  auto touching = PolyF{{1, -1}, {3, -1}, {3, 1}, {1, 1}};
  EXPECT_FALSE(box.intersects(touching));
  EXPECT_TRUE(PolyF::clip(box, touching).empty());
  auto shifted = box;
  shifted.translate({0, 1.75f});
  auto correction = shifted.directional_sat_intersection(box, Vec2F(0, 1), true);
  ASSERT_TRUE(correction.intersects);
  EXPECT_NEAR(correction.overlap[1], 0.25f, 1e-6f);
  EXPECT_EQ(correction.overlap[0], 0);
  auto shortest = shifted.sat_intersection(box);
  ASSERT_TRUE(shortest.intersects);
  EXPECT_NEAR(shortest.overlap[1], 0.25f, 1e-6f);
}

TEST(PolyTest, RectanglePreservesStrictSat) {
  RectF box(-1, -1, 1, 1);
  EXPECT_EQ(PolyF(box), (PolyF{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}));
  EXPECT_FALSE(PolyF{}.intersects_rect(box));
  EXPECT_TRUE(PolyF(Vec2F(0, 0)).intersects_rect(box));
  EXPECT_FALSE(PolyF(Vec2F(1, 0)).intersects_rect(box));
  PolyF square(box);
  EXPECT_FALSE(square.intersects_rect(RectF(1, -1, 2, 1))); // edge
  EXPECT_FALSE(square.intersects_rect(RectF(1, 1, 2, 2)));  // corner
  EXPECT_TRUE(square.intersects_rect(RectF(std::nextafter(1.f, 0.f), -1, 2, 1)));
  EXPECT_TRUE(square.intersects_rect(RectF(0, 0, 0, 0)));

  PolyF segment{{-1, 0}, {1, 0}};
  EXPECT_TRUE(segment.intersects_rect(RectF(0, -1, 0, 1)));
  EXPECT_FALSE(segment.intersects_rect(RectF(0, 0, 1, 0)));
  EXPECT_FALSE(segment.intersects_rect(RectF(0, 0, 0, 0)));
}

TEST(PolyTest, RectangleMatchesOwningPath) {
  // Include reversed rectangle axes and non-convex input: do not canonicalize
  // inputs or substitute a bounding-box overlap for the existing SAT.
  for (auto const& poly : {PolyD{}, PolyD(Vec2D(1, 1)), PolyD{{-3, 0}, {3, 0}},
           PolyD{{-2, -2}, {2, 2}, {3, 3}}, PolyD{{-3, 0}, {0, -3}, {3, 0}, {0, 3}},
           PolyD{{-2, -1}, {2, -1}, {2, -1}, {0, 0}, {-1, 2}}}) {
    for (auto const& rect : {RectD(-1, -1, 1, 1), RectD(1, 1, -1, -1), RectD(-1, 1, 1, -1),
             RectD(3, 0, 4, 1), RectD(2, 2, 4, 4), RectD(0, 0, 0, 0), RectD(0, -1, 0, 1),
             RectD(-1, 0, 1, 0), RectD(1.5 - 0x1p-50, 1.5, 2, 2)}) {
      EXPECT_EQ(poly.intersects_rect(rect), poly.intersects(PolyD(rect)))
          << std::format("polygon {} and rectangle {}", poly, rect);
    }
  }

  constexpr auto lo = std::numeric_limits<int>::min(), hi = std::numeric_limits<int>::max();
  RectI wide(lo, lo, hi, hi);
  EXPECT_TRUE(PolyI(Vec2I()).intersects_rect(wide));
  EXPECT_FALSE(PolyI(Vec2I(hi, hi)).intersects_rect(wide));
}

TEST(PolyTest, AxisBoundsAndWideOrientation) {
  PolyF triangle{{0, 4}, {2, 0}, {2, 8}};
  EXPECT_EQ(triangle.bottom_center(), Vec2F(4.0f / 3, 1));
  constexpr auto lo = std::numeric_limits<int>::min(), hi = std::numeric_limits<int>::max();
  PolyI rectangle{{lo, lo}, {hi, lo}, {hi, hi}, {lo, hi}};
  EXPECT_EQ(PolyI::convex_hull(rectangle.vertices()), rectangle);
  EXPECT_EQ(rectangle.winding_number(Vec2I()), 1);
  EXPECT_GT(rectangle.signed_area(), 1.8e19);
  PolyD thin{{0, 0}, {1, 0}, {1, 1.0 + 0x1p-40}, {0, 1.0 + 0x1p-40}};
  EXPECT_EQ(thin.signed_area(), 1.0 + 0x1p-40);
}

namespace {
template <typename P>
concept borrows_vertices = requires(P&& polygon) { std::forward<P>(polygon).vertices(); };
static_assert(borrows_vertices<PolyF&> && !borrows_vertices<PolyF>);
}
