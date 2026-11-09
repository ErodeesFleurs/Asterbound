import std;
import star.line;

#include "gtest/gtest.h"

import star.vector;
import star.math_common;
import star.matrix3;
using namespace Star;

namespace {

template <typename T, std::size_t N>
concept valid_line = requires { typename Line<T, N>; };

template <typename L>
concept const_line_copies = requires(L const& line, typename L::VectorType const& offset) {
  { line.reversed() } -> std::same_as<L>;
  { line.translated(offset) } -> std::same_as<L>;
};

template <typename L>
concept has_2d_geometry = requires(L line, typename L::VectorType point, Mat3F matrix) {
  line.angle();
  line.intersection(line);
  line.intersects(line);
  line.line_projection(point);
  line.distance_to(point);
  line.rotate(0);
  line.transform(matrix);
  line.transformed(matrix);
  line.flip_horizontal(0);
  line.flip_vertical(0);
};

template <typename L>
concept has_no_2d_geometry = !requires(L line) { line.angle(); } && !requires(L line) { line.intersection(line); }
  && !requires(L line) { line.intersects(line); }
  && !requires(L line, typename L::VectorType point) { line.line_projection(point); }
  && !requires(L line, typename L::VectorType point) { line.distance_to(point); }
  && !requires(L line) { line.rotate(0); } && !requires(L line, Mat3F matrix) { line.transform(matrix); }
  && !requires(L line, Mat3F matrix) { line.transformed(matrix); }
  && !requires(L line, Mat3F matrix) { line.template transform<float>(matrix); }
  && !requires(L line, Mat3F matrix) { line.template transformed<float>(matrix); }
  && !requires(L line) { line.flip_horizontal(0); } && !requires(L line) { line.flip_vertical(0); };

static_assert(valid_line<int, 2> && valid_line<unsigned, 1>);
static_assert(valid_line<float, 3> && valid_line<double, 2> && valid_line<long double, 2>);
static_assert(!valid_line<bool, 2> && !valid_line<std::string, 2>);
static_assert(!valid_line<void*, 2> && !valid_line<float, 0>);
static_assert(const_line_copies<Line2F> && const_line_copies<Line<float, 3>>);
static_assert(has_2d_geometry<Line2F> && has_no_2d_geometry<Line<float, 3>>);
static_assert(requires(Line<float, 3> line) {
  { line.make_positive() } -> std::same_as<bool>;
});
static_assert(std::constructible_from<Line2F, Line2D const&>);
static_assert(std::constructible_from<Line2I, Line2F const&>);
static_assert(!std::constructible_from<Line2F, Line<float, 3> const&>);
static_assert(!std::constructible_from<Line2F, std::string const&>);
static_assert(!std::constructible_from<Line2F, Vec2F const&>);

}// namespace

TEST(LineTest, IntersectionEndpoint) {
  Line2F a({0, 0}, {10, 10});
  Line2F b({10, -10}, {0, 0});

  auto intersection1 = a.intersection(b);
  auto intersection2 = b.intersection(a);

  EXPECT_TRUE(intersection1.intersects);
  EXPECT_TRUE(intersection2.intersects);

  EXPECT_TRUE(vmag(intersection1.point - Vec2F(0, 0)) < 0.0001f);
  EXPECT_TRUE(vmag(intersection2.point - Vec2F(0, 0)) < 0.0001f);

  EXPECT_TRUE(intersection1.glances);
  EXPECT_TRUE(intersection2.glances);

  EXPECT_FALSE(intersection1.coincides);
  EXPECT_FALSE(intersection2.coincides);
}

TEST(LineTest, IntersectionMiddle) {
  Line2F a({-5, 0}, {5, 0});
  Line2F b({0, -2}, {0, 8});

  auto intersection1 = a.intersection(b);
  EXPECT_TRUE(intersection1.intersects);
  EXPECT_FALSE(intersection1.glances);
  EXPECT_FALSE(intersection1.coincides);
  EXPECT_TRUE(vmag(intersection1.point - Vec2F(0, 0)) < 0.0001f);
  EXPECT_TRUE(std::fabs(intersection1.t - 0.5f) < 0.0001f);
}

TEST(LineTest, IntersectionOneEndpoint) {
  Line2F a({0, 0}, {0, 5});
  Line2F b({-1, 5}, {1, 5});

  auto intersection1 = a.intersection(b);
  auto intersection2 = b.intersection(a);
  EXPECT_TRUE(intersection1.glances);
  EXPECT_TRUE(intersection2.glances);
  EXPECT_FALSE(intersection1.coincides);
  EXPECT_FALSE(intersection2.coincides);

  EXPECT_TRUE(std::fabs(intersection1.t - 1.0f) < 0.0001f);
  EXPECT_TRUE(std::fabs(intersection2.t - 0.5f) < 0.0001f);
}

TEST(LineTest, IntersectionOneVertical) {
  Line2F a({0, 3}, {8, 5});
  Line2F b({4, 0}, {4, 8});

  auto intersection1 = a.intersection(b);
  auto intersection2 = b.intersection(a);
  EXPECT_FALSE(intersection1.glances);
  EXPECT_FALSE(intersection2.glances);
  EXPECT_FALSE(intersection1.coincides);
  EXPECT_FALSE(intersection2.coincides);
  EXPECT_TRUE(intersection1.intersects);
  EXPECT_TRUE(intersection2.intersects);

  EXPECT_TRUE(std::fabs(intersection1.t - 0.5f) < 0.0001f);
  EXPECT_TRUE(std::fabs(intersection2.t - 0.5f) < 0.0001f);

  EXPECT_TRUE(vmag(intersection1.point - Vec2F(4, 4)) < 0.0001f);
  EXPECT_TRUE(vmag(intersection2.point - Vec2F(4, 4)) < 0.0001f);
}

TEST(LineTest, NoIntersection) {
  Line2F a({1, 1}, {2, 2});
  Line2F b({-1, 1}, {0, 0});

  auto intersection1 = a.intersection(b);
  auto intersection1inf = a.intersection(b, true);

  EXPECT_FALSE(intersection1.intersects);
  EXPECT_TRUE(intersection1inf.intersects);
}

TEST(LineTest, ParallelHorizontal) {
  Line2F a({9, 12}, {10, 12});
  Line2F b({10, 20}, {20, 20});

  auto intersection1 = a.intersection(b);
  auto intersection2 = b.intersection(a);

  EXPECT_FALSE(intersection1.intersects);
  EXPECT_FALSE(intersection2.intersects);

  EXPECT_FALSE(intersection1.glances);
  EXPECT_FALSE(intersection2.glances);

  EXPECT_FALSE(intersection1.coincides);
  EXPECT_FALSE(intersection2.coincides);
}

TEST(LineTest, ParallelVertical) {
  Line2F a({12, 12}, {12, 14});
  Line2F b({20, 10}, {20, 20});

  auto intersection1 = a.intersection(b);
  auto intersection2 = b.intersection(a);

  EXPECT_FALSE(intersection1.intersects);
  EXPECT_FALSE(intersection2.intersects);

  EXPECT_FALSE(intersection1.glances);
  EXPECT_FALSE(intersection2.glances);

  EXPECT_FALSE(intersection1.coincides);
  EXPECT_FALSE(intersection2.coincides);
}

TEST(LineTest, ParallelOther) {
  Line2F a({3, 3}, {4, 4});
  Line2F b({5, 6}, {7, 8});

  auto intersection1 = a.intersection(b);
  auto intersection2 = b.intersection(a);

  EXPECT_FALSE(intersection1.intersects);
  EXPECT_FALSE(intersection2.intersects);

  EXPECT_FALSE(intersection1.glances);
  EXPECT_FALSE(intersection2.glances);

  EXPECT_FALSE(intersection1.coincides);
  EXPECT_FALSE(intersection2.coincides);
}

TEST(LineTest, CoincidesVertical) {
  Line2F a({3, 3}, {3, 4});
  Line2F b({3, 5}, {3, 7});

  auto intersection1 = a.intersection(b);
  auto intersection2 = b.intersection(a);

  auto intersection1inf = a.intersection(b, true);

  EXPECT_FALSE(intersection1.intersects);
  EXPECT_FALSE(intersection2.intersects);
  EXPECT_TRUE(intersection1inf.intersects);

  EXPECT_FALSE(intersection1.glances);
  EXPECT_FALSE(intersection2.glances);
  EXPECT_TRUE(intersection1inf.glances);

  EXPECT_TRUE(intersection1.coincides);
  EXPECT_TRUE(intersection2.coincides);
  EXPECT_TRUE(intersection1inf.coincides);

  EXPECT_TRUE(std::fabs(intersection1.t - 2.0f) < 0.0001f);
  EXPECT_TRUE(std::fabs(intersection2.t + 0.5f) < 0.0001f);
}

TEST(LineTest, CoincidesHorizontal) {
  Line2F a({3, 3}, {4, 3});
  Line2F b({5, 3}, {7, 3});

  auto intersection1 = a.intersection(b);
  auto intersection2 = b.intersection(a);

  auto intersection1inf = a.intersection(b, true);

  EXPECT_FALSE(intersection1.intersects);
  EXPECT_FALSE(intersection2.intersects);
  EXPECT_TRUE(intersection1inf.intersects);

  EXPECT_FALSE(intersection1.glances);
  EXPECT_FALSE(intersection2.glances);
  EXPECT_TRUE(intersection1inf.glances);

  EXPECT_TRUE(intersection1.coincides);
  EXPECT_TRUE(intersection2.coincides);
  EXPECT_TRUE(intersection1inf.coincides);

  EXPECT_TRUE(std::fabs(intersection1.t - 2.0f) < 0.0001f);
  EXPECT_TRUE(std::fabs(intersection2.t + 0.5f) < 0.0001f);
}

TEST(LineTest, CoincidesOther) {
  Line2F a({3, 3}, {4, 4});
  Line2F b({5, 5}, {7, 7});

  auto intersection1 = a.intersection(b);
  auto intersection2 = b.intersection(a);

  auto intersection1inf = a.intersection(b, true);

  EXPECT_FALSE(intersection1.intersects);
  EXPECT_FALSE(intersection2.intersects);
  EXPECT_TRUE(intersection1inf.intersects);

  EXPECT_FALSE(intersection1.glances);
  EXPECT_FALSE(intersection2.glances);
  EXPECT_TRUE(intersection1inf.glances);

  EXPECT_TRUE(intersection1.coincides);
  EXPECT_TRUE(intersection2.coincides);
  EXPECT_TRUE(intersection1inf.coincides);

  EXPECT_TRUE(std::fabs(intersection1.t - 2.0f) < 0.0001f);
  EXPECT_TRUE(std::fabs(intersection2.t + 0.5f) < 0.0001f);
}

TEST(LineTest, IntersectCoincides) {
  Line2F a({3, 3}, {5, 5});
  Line2F b({4, 4}, {6, 6});

  auto intersection1 = a.intersection(b);
  auto intersection2 = b.intersection(a);

  EXPECT_TRUE(intersection1.intersects);
  EXPECT_TRUE(intersection2.intersects);

  EXPECT_TRUE(intersection1.glances);
  EXPECT_TRUE(intersection2.glances);

  EXPECT_TRUE(intersection1.coincides);
  EXPECT_TRUE(intersection2.coincides);

  EXPECT_TRUE(vmag(intersection1.point - Vec2F(4, 4)) < 0.0001f);
  EXPECT_TRUE(vmag(intersection2.point - Vec2F(4, 4)) < 0.0001f);

  EXPECT_TRUE(std::fabs(intersection1.t - 0.5f) < 0.0001f);
  EXPECT_TRUE(std::fabs(intersection2.t) < 0.0001f);
}

TEST(LineTest, Closest) {
  Line2F a({0, 0}, {10, 0});

  EXPECT_TRUE(std::fabs(a.distance_to(Vec2F(-1, 5), true) - 5.0f) < 0.0001f);
  EXPECT_TRUE(std::fabs(a.distance_to(Vec2F(-3, 4), false) - 5.0f) < 0.0001f);
}

TEST(LineTest, MakePositive) {
  Line2F a({0, 0}, {10, 0});
  Line2F aorig = a;
  Line2F b({10, 0}, {0, 0});
  Line2F borig = b;
  Line2F c({10, 0}, {10, 1});
  Line2F corig = c;
  Line2F d({10, 1}, {10, 0});
  Line2F dorig = d;
  Line<float, 3> e({10, 0, 0}, {10, 0, 1});
  Line<float, 3> eorig = e;
  Line<float, 3> f({10, 0, 1}, {10, 0, 0});
  Line<float, 3> forig = f;

  a.make_positive();
  EXPECT_TRUE(a == aorig);
  b.make_positive();
  EXPECT_TRUE(b == aorig);
  EXPECT_FALSE(b == borig);
  c.make_positive();
  EXPECT_TRUE(c == corig);
  d.make_positive();
  EXPECT_TRUE(d == corig);
  EXPECT_FALSE(d == dorig);
  e.make_positive();
  EXPECT_TRUE(e == eorig);
  f.make_positive();
  EXPECT_TRUE(f == eorig);
  EXPECT_FALSE(f == forig);
}

TEST(LineTest, OrientedEndpoints) {
  Line2F const line({8, -2}, {-4, 6});
  EXPECT_EQ(line.min(), Vec2F(8, -2));
  EXPECT_EQ(line.max(), Vec2F(-4, 6));
  EXPECT_EQ(line.center(), Vec2F(2, 2));
  EXPECT_EQ(line.reversed().min(), Vec2F(-4, 6));
  EXPECT_EQ(line.reversed().max(), Vec2F(8, -2));
  EXPECT_EQ(line.translated(Vec2F(1, 3)).min(), Vec2F(9, 1));
  EXPECT_EQ(line.translated(Vec2F(1, 3)).max(), Vec2F(-3, 9));
  EXPECT_EQ(line.min(), Vec2F(8, -2));
  EXPECT_EQ(line.max(), Vec2F(-4, 6));
}

TEST(LineTest, FiniteAndInfiniteHitFlags) {
  struct hit_case {
    Line2F first;
    Line2F second;
    bool finite;
    bool infinite;
  };
  hit_case const cases[] = {
    {Line2F({-5, 0}, {5, 0}), Line2F({0, -2}, {0, 8}), true, true},
    {Line2F({1, 1}, {2, 2}), Line2F({-1, 1}, {0, 0}), false, true},
    {Line2F({3, 3}, {4, 4}), Line2F({5, 6}, {7, 8}), false, false},
    {Line2F({3, 3}, {5, 5}), Line2F({4, 4}, {6, 6}), true, true},
    {Line2F({3, 3}, {4, 4}), Line2F({5, 5}, {7, 7}), false, true},
  };
  for (auto const& test_case : cases) {
    EXPECT_EQ(test_case.first.intersects(test_case.second), test_case.finite);
    EXPECT_EQ(test_case.second.intersects(test_case.first), test_case.finite);
    EXPECT_EQ(test_case.first.intersects(test_case.second, true), test_case.infinite);
    EXPECT_EQ(test_case.second.intersects(test_case.first, true), test_case.infinite);
    EXPECT_EQ(test_case.first.intersection(test_case.second).intersects, test_case.finite);
    EXPECT_EQ(test_case.first.intersection(test_case.second, true).intersects, test_case.infinite);
  }
}
