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

  PolyF shapeA = {
      {19.9954f, 1603.62f}, {21.9954f, 1602.62f}, {21.9954f, 1607.62f}, {19.9954f, 1606.62f}, {19.9954f, 1605.12f}};
  Line2F lineA = {{20.9575f, 1605.13f}, {21.7853f, 1605.03f}};

  EXPECT_EQ(geom.lineIntersectsPoly(lineA, shapeA), true);

  PolyF shapeA2 = {{0.9954f, 1.62f}, {2.9954f, 0.62f}, {2.9954f, 5.62f}, {0.9954f, 4.62f}, {0.9954f, 3.12f}};
  Line2F lineA2 = {{1.9575f, 3.13f}, {2.7853f, 3.03f}};

  EXPECT_EQ(geom.lineIntersectsPoly(lineA2, shapeA2), true);

  PolyF shapeA3 = {{-10.0f, -10.0f}, {10, -10.0f}, {10.0f, 10.0f}, {-10.0f, 10.0f}}; //, {-10, 3.12};
  Line2F lineA3 = {{1.9575f, 3.13f}, {2.7853f, 3.03f}};

  EXPECT_EQ(geom.lineIntersectsPoly(lineA3, shapeA3), true);

  RectF shapeA3R = RectF::withSize({-10, -10}, {20, 20});

  EXPECT_EQ(geom.lineIntersectsRect(lineA3, shapeA3R), true);

  PolyF shapeB = {
      {20.5608f, 1603.62f}, {22.5608f, 1602.62f}, {22.5608f, 1607.62f}, {20.5608f, 1606.62f}, {20.5608f, 1605.12f}};
  Line2F lineB = {{21.7893f, 1605.07f}, {22.6173f, 1604.98f}};

  EXPECT_EQ(geom.lineIntersectsPoly(lineB, shapeB), true);

  PolyF shapeC = {{-10.0f, -10.0f}, {10.0f, -10.0f}, {10.0f, 10.0f}, {-10.0f, 10.0f}};
  PolyF shapeC2 = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};

  EXPECT_EQ(geom.polyIntersectsPoly(shapeC2, shapeC), true);
  EXPECT_EQ(geom.polyIntersectsPoly(shapeC, shapeC2), true);
  EXPECT_EQ(geom.polyContains(shapeC, Vec2F(0, 0)), true);
  auto overlapArea = geom.polyOverlapArea(shapeC, shapeC2);
  ASSERT_TRUE(overlapArea);
  EXPECT_TRUE(std::fabs(*overlapArea - 4) < 0.0001f);
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
