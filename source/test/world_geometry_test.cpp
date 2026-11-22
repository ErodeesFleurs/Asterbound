import std;
import star.poly;
import star.world_geometry;
import star.entity_rendering;

#include "gtest/gtest.h"

import star.vector;
import star.line;
import star.rect;
import star.math_common;
using namespace Star;

TEST(WorldGeometryTest, All) {
  WorldGeometry geom = WorldGeometry(Vec2U{8192, 3072});

  PolyF shape_a = {
      {19.9954f, 1603.62f}, {21.9954f, 1602.62f}, {21.9954f, 1607.62f}, {19.9954f, 1606.62f}, {19.9954f, 1605.12f}};
  Line2F line_a = {{20.9575f, 1605.13f}, {21.7853f, 1605.03f}};

  EXPECT_EQ(geom.line_intersects_poly(line_a, shape_a), true);

  PolyF shape_a2 = {{0.9954f, 1.62f}, {2.9954f, 0.62f}, {2.9954f, 5.62f}, {0.9954f, 4.62f}, {0.9954f, 3.12f}};
  Line2F line_a2 = {{1.9575f, 3.13f}, {2.7853f, 3.03f}};

  EXPECT_EQ(geom.line_intersects_poly(line_a2, shape_a2), true);

  PolyF shape_a3 = {{-10.0f, -10.0f}, {10, -10.0f}, {10.0f, 10.0f}, {-10.0f, 10.0f}};
  Line2F line_a3 = {{1.9575f, 3.13f}, {2.7853f, 3.03f}};

  EXPECT_EQ(geom.line_intersects_poly(line_a3, shape_a3), true);

  RectF shape_a3_rect = RectF::with_size({-10, -10}, {20, 20});

  EXPECT_EQ(geom.line_intersects_rect(line_a3, shape_a3_rect), true);

  PolyF shape_b = {
      {20.5608f, 1603.62f}, {22.5608f, 1602.62f}, {22.5608f, 1607.62f}, {20.5608f, 1606.62f}, {20.5608f, 1605.12f}};
  Line2F line_b = {{21.7893f, 1605.07f}, {22.6173f, 1604.98f}};

  EXPECT_EQ(geom.line_intersects_poly(line_b, shape_b), true);

  PolyF shape_c = {{-10.0f, -10.0f}, {10.0f, -10.0f}, {10.0f, 10.0f}, {-10.0f, 10.0f}};
  PolyF shape_c2 = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};

  EXPECT_EQ(geom.poly_intersects_poly(shape_c2, shape_c), true);
  EXPECT_EQ(geom.poly_intersects_poly(shape_c, shape_c2), true);
  EXPECT_EQ(geom.poly_contains(shape_c, Vec2F(0, 0)), true);
  auto overlap_area = geom.poly_overlap_area(shape_c, shape_c2);
  ASSERT_TRUE(overlap_area);
  EXPECT_TRUE(std::fabs(*overlap_area - 4) < 0.0001f);
}

TEST(WorldGeometryTest, WrappedCircleInteriorAndTangency) {
  WorldGeometry geometry(100, 32);
  RectF right(98, 0, 104, 10);
  RectF left(-4, 0, 2, 10);
  EXPECT_EQ(geometry.rect_intersects_circle(right, {1, 5}, 1), true);
  EXPECT_EQ(geometry.rect_intersects_circle(left, {99, 5}, 0), true);
  EXPECT_EQ(geometry.rect_intersects_circle(right, {5, 5}, 1), true);
  EXPECT_EQ(geometry.rect_intersects_circle(right, {6, 5}, 1), false);
  EXPECT_EQ(geometry.rect_intersects_circle(right, {1, 12}, 1), false);
}

TEST(WorldGeometryTest, FirstPolygonHitFollowsLineDirectionAcrossSeam) {
  WorldGeometry geometry(100, 32);
  PolyF poly{{-3, 1}, {3, 1}, {3, 9}, {-3, 9}};
  auto forward = geometry.line_intersects_poly_at({{92, 5}, {108, 5}}, poly);
  auto backward = geometry.line_intersects_poly_at({{108, 5}, {92, 5}}, poly);
  ASSERT_TRUE(forward && *forward);
  ASSERT_TRUE(backward && *backward);
  EXPECT_EQ(**forward, Vec2F(97, 5));
  EXPECT_EQ(**backward, Vec2F(3, 5));
}

TEST(WorldGeometryTest, InterpolatorRetainsWrapAndThresholdAfterGeometryExpires) {
  auto smooth = WorldGeometry(100, 32).x_lerp_function(4.0f);
  auto jump = WorldGeometry(100, 32).x_lerp_function(3.0f);
  EXPECT_FLOAT_EQ(smooth(0.5f, 98, 2), 100);
  EXPECT_FLOAT_EQ(jump(0.5f, 98, 2), 102);
  auto hold = WorldGeometry().lerp_function();
  EXPECT_EQ(hold(1, {7, 8}, {11, 12}), Vec2F(7, 8));
}

TEST(WorldGeometryTest, IntegerExtremesAndExactSeam) {
  constexpr int low = std::numeric_limits<int>::min();
  constexpr int high = std::numeric_limits<int>::max();
  WorldGeometry geometry(100, 32);
  EXPECT_EQ(geometry.pdiff(high, low), 95);
  EXPECT_EQ(geometry.diff(high, low), -5);
  EXPECT_EQ(geometry.diff(50, 0), 50);
  EXPECT_EQ(geometry.diff(0, 50), -50);
  EXPECT_EQ(WorldGeometry().nearest_to(high, low), low);
  EXPECT_EQ(geometry.nearest_to(high, low), high);
  EXPECT_EQ(geometry.diff(10, -40), -50);
  EXPECT_EQ(geometry.diff(Vec2I(10, high), Vec2I(9, low)), Vec2I(1, high));
  WorldGeometry wide(std::numeric_limits<unsigned>::max(), 32);
  EXPECT_EQ(wide.index(-1), std::numeric_limits<unsigned>::max() - 1);
  EXPECT_FALSE(wide.equal({-1, 0}, {-2, 0}));
  EXPECT_FALSE(wide.split_x_region(Vec2I(-2, -1)).has_value());
  auto near_limit = geometry.split_x_region(Vec2I(high - 1, high));
  ASSERT_TRUE(near_limit);
  ASSERT_EQ(near_limit->size(), 1u);
  EXPECT_EQ((*near_limit)[0], Vec2I(46, 47));
  auto exact = geometry.split_rect(RectI(-3, low, 0, high));
  ASSERT_TRUE(exact);
  ASSERT_EQ(exact->size(), 1u);
  EXPECT_EQ((*exact)[0], RectI(97, low, 100, high));
}

TEST(WorldGeometryTest, PresentationUsesShortestGeometryWithoutAdvancingState) {
  WorldGeometry geometry(Vec2U{100, 100});
  PresentationPosition position;
  position.reset({99, 5});
  position.advance({1, 9});
  EXPECT_EQ(position.sample(geometry, 0.0f), Vec2F(-1, 5));
  EXPECT_EQ(position.sample(geometry, 0.5f), Vec2F(0, 7));
  EXPECT_EQ(position.sample(geometry, 1.0f), Vec2F(1, 9));
  // Display count and ordering cannot advance either simulation endpoint.
  EXPECT_EQ(position.sample(geometry, 0.25f), Vec2F(-0.5f, 6));
  EXPECT_EQ(position.sample(geometry, 0.5f), Vec2F(0, 7));
  EXPECT_EQ(position.current, Vec2F(1, 9));
  EXPECT_EQ(position.previous, Vec2F(99, 5));
}

TEST(WorldGeometryTest, PresentationDiscontinuitiesCollapseBothEndpoints) {
  WorldGeometry geometry(Vec2U{100, 100});
  PresentationPosition position;
  position.reset({10, 5});
  position.advance({11, 6});
  position.reset({60, 40});
  EXPECT_EQ(position.sample(geometry, 0.0f), Vec2F(60, 40));
  EXPECT_EQ(position.sample(geometry, 0.5f), Vec2F(60, 40));
  EXPECT_EQ(position.sample(geometry, 1.0f), Vec2F(60, 40));
  position.advance({62, 42});
  EXPECT_EQ(position.sample(geometry, -1.0f), Vec2F(60, 40));
  EXPECT_EQ(position.sample(geometry, 2.0f), Vec2F(62, 42));
}

TEST(WorldGeometryTest, PresentationCorrectionPolicyPreservesContinuousMovement) {
  WorldGeometry geometry(Vec2U{1000, 100});
  PresentationPosition position;
  position.reset({99, 5});
  position.advance({99, 5});
  EXPECT_TRUE(position.finish({101, 5}, geometry, 8.0f));
  EXPECT_EQ(position.sample(geometry, 0.5f), Vec2F(100, 5));
  position.advance({101, 5});
  EXPECT_FALSE(position.finish({301, 5}, geometry, 8.0f));
  EXPECT_EQ(position.sample(geometry, 0.0f), Vec2F(301, 5));
  EXPECT_EQ(position.sample(geometry, 0.5f), Vec2F(301, 5));
  // The world supplies a larger speed-derived allowance to fast projectiles.
  position.advance({301, 5});
  EXPECT_TRUE(position.finish({321, 5}, geometry, 80.0f));
  EXPECT_EQ(position.sample(geometry, 0.5f), Vec2F(311, 5));
  position.reset({999, 5});
  position.advance({999, 5});
  EXPECT_TRUE(position.finish({1, 5}, geometry, 8.0f));
  EXPECT_EQ(position.sample(geometry, 0.5f), Vec2F(0, 5));
}
