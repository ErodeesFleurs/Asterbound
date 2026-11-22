import star.vector;
import star.blocks_along_line;
import star.format;
import std;
import star.list;
import star.math_common;

#include "gtest/gtest.h"

using namespace Star;

namespace {

using CheckedVisitor = std::function<std::expected<bool, StarException>(int, int)>;
static_assert(block_visitor<CheckedVisitor>);
static_assert(!block_visitor<std::function<bool(int, int)>>);
static_assert(!block_visitor<std::function<std::expected<void, StarException>(int, int)>>);
static_assert(!block_visitor<std::function<std::expected<bool, StarException>(int&, int&)>>);

template <typename Scalar>
concept supports_block_traversal = requires(Vector<Scalar, 2> point, CheckedVisitor& visitor) {
  for_each_block_along_line(point, point, visitor);
};
static_assert(supports_block_traversal<float> && supports_block_traversal<double>);
static_assert(!supports_block_traversal<int> && !supports_block_traversal<long double>);

template <finite_classifiable Scalar>
void expect_path(Vector<Scalar, 2> origin, Vector<Scalar, 2> delta, List<Vec2I> const& expected) {
  List<Vec2I> visited;
  auto result = for_each_block_along_line(origin, delta, [&](int x, int y) -> std::expected<bool, StarException> {
    visited.push_back({x, y});
    return visited.size() <= expected.size();
  });
  ASSERT_EQ(result, true);
  EXPECT_EQ(visited, expected);
}

void test_line(Vec2D a, Vec2D b, Vec2I offset, List<Vec2I> expected) {
  for (auto& cell : expected)
    cell += offset;
  expect_path(a + Vec2D(offset), b - a, expected);
}

void test_group(Vec2I offset) {
  test_line(Vec2D(0.5, 0.5), Vec2D(0.5, 0.5), offset, {{0, 0}});
  test_line(Vec2D(-0.5, -0.5), Vec2D(-0.5, -0.5), offset, {{-1, -1}});

  test_line(Vec2D(-0.5, -0.5), Vec2D(0.5, 0.5), offset, {{-1, -1}, {0, 0}});
  test_line(Vec2D(0.5, 0.5), Vec2D(-0.5, -0.5), offset, {{0, 0}, {-1, -1}});
  test_line(Vec2D(-0.5, 0.5), Vec2D(0.5, -0.5), offset, {{-1, 0}, {0, -1}});
  test_line(Vec2D(0.5, -0.5), Vec2D(-0.5, 0.5), offset, {{0, -1}, {-1, 0}});

  test_line(Vec2D(0.5, -0.5), Vec2D(0.5, 0.5), offset, {{0, -1}, {0, 0}});
  test_line(Vec2D(-0.5, 0.5), Vec2D(0.5, 0.5), offset, {{-1, 0}, {0, 0}});

  test_line(Vec2D(0.0, 0.5), Vec2D(0.0, -0.5), offset, {{0, 0}, {0, -1}});
  test_line(Vec2D(0.5, 0.0), Vec2D(-0.5, 0.0), offset, {{0, 0}, {-1, 0}});
}

TEST(BlocksAlongLine, All) {
  test_group({0, 0});
  test_group({50, 50});
  test_group({-5, -50});
  test_group({50, -5});
  test_group({100, 10});
  test_group({-10, -100});
  test_group({-10, 10});
}

TEST(BlocksAlongLine, FlooredEndpointsAtMixedDirectionCorners) {
  expect_path(Vec2D(0.5, 1.5), Vec2D(0.5, -0.5), {{0, 1}, {1, 1}});
  expect_path(Vec2D(1, 1), Vec2D(-0.5, 0.5), {{1, 1}, {0, 1}});
  expect_path(Vec2D(1.5, 0.5), Vec2D(-0.5, 0.5), {{1, 0}, {1, 1}});
  expect_path(Vec2D(1, 1), Vec2D(0.5, -0.5), {{1, 1}, {1, 0}});
  expect_path(Vec2D(-0.5, 2.5), Vec2D(1.5, -1.5), {{-1, 2}, {0, 1}, {1, 1}});
  expect_path(Vec2D(1, 1), Vec2D(-1.5, 1.5), {{1, 1}, {0, 1}, {-1, 2}});
}

TEST(BlocksAlongLine, ExactAndAdjacentInteriorCorners) {
  Vec2D const start(0.5, 0.5);
  expect_path(start, Vec2D(1, 1), {{0, 0}, {1, 1}});
  expect_path(start, Vec2D(1, std::nextafter(1.0, 0.0)), {{0, 0}, {1, 0}, {1, 1}});
  expect_path(start, Vec2D(1, std::nextafter(1.0, 2.0)), {{0, 0}, {0, 1}, {1, 1}});
  expect_path(Vec2D(0.25, 0.25), Vec2D(8, 2),
      {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, {7, 2}, {8, 2}});
}

TEST(BlocksAlongLine, EndpointRoundsInInputPrecision) {
  float const just_below_half = std::nextafter(0.5f, 0.0f);
  expect_path(Vec2F(0.5f, 0), Vec2F(just_below_half, 0), {{0, 0}, {1, 0}});
  expect_path(Vec2D(0.5, 0), Vec2D(just_below_half, 0), {{0, 0}});
}

TEST(BlocksAlongLine, IntegerLimitsAndFirstCellStop) {
  constexpr int low = std::numeric_limits<int>::min();
  constexpr int high = std::numeric_limits<int>::max();
  expect_path(Vec2D(high - 1, 0), Vec2D(1.5, 0), {{high - 1, 0}, {high, 0}});
  expect_path(Vec2D(0, low + 1), Vec2D(0, -1), {{0, low + 1}, {0, low}});
  expect_path(Vec2D(high - 1, high - 1), Vec2D(1, 1), {{high - 1, high - 1}, {high, high}});
  expect_path(Vec2D(low + 1, low + 1), Vec2D(-1, -1), {{low + 1, low + 1}, {low, low}});

  int calls = 0;
  double const span = double(high) - low;
  auto stopped = for_each_block_along_line(Vec2D(low, high), Vec2D(span, -span),
      [&](int x, int y) -> std::expected<bool, StarException> {
        ++calls;
        EXPECT_EQ(x, low);
        EXPECT_EQ(y, high);
        return false;
      });
  EXPECT_EQ(stopped, false);
  EXPECT_EQ(calls, 1);
}

TEST(BlocksAlongLine, BorrowsVisitorAndReturnsItsFirstError) {
  struct Visitor {
    Visitor() = default;
    Visitor(Visitor const&) = delete;
    explicit operator bool() const { return false; }
    std::expected<bool, StarException> operator()(int&& x, int&& y) & {
      visited.push_back({x, y});
      x = 42;
      if (visited.size() == 3)
        return std::unexpected(IOException("tile read failed"));
      return true;
    }
    List<Vec2I> visited;
  } visitor;

  auto failed = for_each_block_along_line(Vec2D(0.5, 0.5), Vec2D(4, 0), std::move(visitor));
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<IOException>());
  EXPECT_EQ(visitor.visited, (List<Vec2I>{{0, 0}, {1, 0}, {2, 0}}));
  EXPECT_FALSE(for_each_block_along_line(Vec2D(), Vec2D(), CheckedVisitor{}).has_value());
}

TEST(BlocksAlongLine, RejectsInvalidCoordinatesBeforeVisiting) {
  constexpr double below = double(std::numeric_limits<int>::min()) - 0.25;
  constexpr double above = double(std::numeric_limits<int>::max()) + 1;
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  constexpr double inf = std::numeric_limits<double>::infinity();
  int calls = 0;
  auto reject = [&]<finite_classifiable Scalar>(Vector<Scalar, 2> const& origin, Vector<Scalar, 2> const& delta,
      std::source_location input = std::source_location::current()) {
    SCOPED_TRACE(input.line());
    auto result = for_each_block_along_line(origin, delta, [&](int, int) -> std::expected<bool, StarException> {
      ++calls;
      return false;
    });
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().template is<OutOfRangeException>());
  };
  reject(Vec2D(below, 0), Vec2D(1, 0));
  reject(Vec2D(0, above), Vec2D(0, -1));
  reject(Vec2D(0, 0), Vec2D(0, below));
  reject(Vec2D(0, 0), Vec2D(above, 0));
  reject(Vec2D(nan, 0), Vec2D(1, 1));
  reject(Vec2D(0, -inf), Vec2D(0, 0));
  reject(Vec2D(0, 0), Vec2D(inf, 0));
  reject(Vec2F(0, 0), Vec2F(0, std::numeric_limits<float>::quiet_NaN()));
  float const largest = std::numeric_limits<float>::max();
  reject(Vec2F(largest, 0), Vec2F(largest, 0));
  EXPECT_EQ(calls, 0);
}

}
