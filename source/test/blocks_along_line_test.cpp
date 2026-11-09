import star.vector;
import star.blocks_along_line;
import star.format;
import std;
import star.list;

#include "gtest/gtest.h"

using namespace Star;

void testLine(Vec2D a, Vec2D b, Vec2I offset, List<Vec2I> comp) {
  List<Vec2I> res;
  auto traversed =
    forBlocksAlongLine(a + Vec2D(offset), b - a, [&res](int x, int y) -> std::expected<bool, StarException> {
      res.push_back({x, y});
      return true;
    });
  ASSERT_EQ(traversed, true);
  for (auto& c : comp)
    c += offset;
  EXPECT_TRUE(comp == res);
}

void testGroup(Vec2I offset) {
  testLine(Vec2D(0.5, 0.5), Vec2D(0.5, 0.5), offset, {{0, 0}});
  testLine(Vec2D(-0.5, -0.5), Vec2D(-0.5, -0.5), offset, {{-1, -1}});

  testLine(Vec2D(-0.5, -0.5), Vec2D(0.5, 0.5), offset, {{-1, -1}, {0, 0}});
  testLine(Vec2D(0.5, 0.5), Vec2D(-0.5, -0.5), offset, {{0, 0}, {-1, -1}});
  testLine(Vec2D(-0.5, 0.5), Vec2D(0.5, -0.5), offset, {{-1, 0}, {0, -1}});
  testLine(Vec2D(0.5, -0.5), Vec2D(-0.5, 0.5), offset, {{0, -1}, {-1, 0}});

  testLine(Vec2D(0.5, -0.5), Vec2D(0.5, 0.5), offset, {{0, -1}, {0, 0}});
  testLine(Vec2D(-0.5, 0.5), Vec2D(0.5, 0.5), offset, {{-1, 0}, {0, 0}});

  testLine(Vec2D(0.0, 0.5), Vec2D(0.0, -0.5), offset, {{0, 0}, {0, -1}});
  testLine(Vec2D(0.5, 0.0), Vec2D(-0.5, 0.0), offset, {{0, 0}, {-1, 0}});
}

TEST(BlocksAlongLine, All) {
  testGroup({0, 0});
  testGroup({50, 50});
  testGroup({-5, -50});
  testGroup({50, -5});
  testGroup({100, 10});
  testGroup({-10, -100});
  testGroup({-10, 10});
}
