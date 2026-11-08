import std;
import star.poly;
import star.random;

#include "gtest/gtest.h"

import star.vector;
import star.line;
import star.math_common;
using namespace Star;

TEST(PolyTest, ConvexHull) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  PolyF::VertexList inputVertexes;
  for (unsigned i = 0; i < 1000; ++i) {
    float angle = random.randf() * 2 * std::numbers::pi;
    inputVertexes.push_back({std::sin(angle), std::cos(angle)});
  }

  PolyF convex = PolyF::convexHull(inputVertexes);

  PolyF::VertexList testVertexes;
  for (unsigned i = 0; i < 1000; ++i) {
    float angle = random.randf() * 2 * std::numbers::pi;
    testVertexes.push_back({std::sin(angle) * 0.75f, std::cos(angle) * 0.75f});
  }

  for (auto const& vertex : testVertexes)
    EXPECT_TRUE(convex.contains(vertex * 0.75f));
}

TEST(PolyTest, Distance) {
  PolyF square = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};

  EXPECT_TRUE(std::fabs(square.distance({-2.0f, -2.0f}) - std::numbers::sqrt2) < 0.00001f);
}

TEST(PolyTest, LineCollision) {
  PolyF square = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
  auto fromLeft = square.lineIntersection(Line2F({-2, 0}, {2, 0}));
  ASSERT_TRUE(fromLeft);
  EXPECT_TRUE(vmag(Vec2F(-1, 0) - fromLeft->point) < 0.0001f);
  auto fromRight = square.lineIntersection(Line2F({2, 0}, {-2, 0}));
  ASSERT_TRUE(fromRight);
  EXPECT_TRUE(vmag(Vec2F(1, 0) - fromRight->point) < 0.0001f);
}

TEST(PolyTest, ConvexArea) {
  PolyF triangle = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {0.0f, 1.0f}};
  EXPECT_TRUE(std::fabs(triangle.convexArea() - 2.0f) < 0.0001f);
}

TEST(PolyTest, Clipping) {
  PolyF triangle1 = {{-2.0f, -1.0f}, {2.0f, -1.0f}, {0.0f, 1.0f}};
  PolyF triangle2 = {{2.0f, 1.0f}, {-2.0f, 1.0f}, {0.0f, -1.0f}};
  PolyF overlap = PolyF::clip(triangle1, triangle2);
  overlap.deduplicateVertexes(0.0001f);
  EXPECT_TRUE(overlap.sides() == 4);
  EXPECT_TRUE(overlap.convexArea() - 2 < 0.0001f);
}
