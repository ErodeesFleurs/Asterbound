import std;
import star.multi_array;
import star.interpolation;
import star.multi_array_interpolator;
import star.multi_table;

#include "gtest/gtest.h"

import star.array;
import star.math_common;
using namespace Star;

TEST(MultiArrayTest, All) {
  MultiArray<int, 2> table(10, 12);
  table.forEach([](Array2S const& index, int& val) {
      val = (index[0] + 1) * index[1];
    });

  EXPECT_EQ(table(3, 4), 16);
  EXPECT_EQ(table(5, 2), 12);
  EXPECT_EQ(table(0, 9), 9);
  EXPECT_EQ(table(8, 1), 9);
  EXPECT_EQ(table(0, 1), 1);
  EXPECT_EQ(table(8, 9), 81);

  MultiArray<int, 3> table3(5, 6, 7);
  table3.forEach([](Array3S const& index, int& val) {
      val = index[0] + index[1] + index[2];
    });

  EXPECT_EQ(table3(0, 0, 0), 0);
  EXPECT_EQ(table3(1, 1, 0), 2);
  EXPECT_EQ(table3(2, 0, 2), 4);
  EXPECT_EQ(table3(1, 1, 1), 3);
  EXPECT_EQ(table3(0, 1, 0), 1);
  EXPECT_EQ(table3(2, 2, 2), 6);
  EXPECT_EQ(table3(3, 3, 3), 9);
  EXPECT_EQ(table3(4, 4, 4), 12);

  table3.forEach({3, 3, 3}, {2, 2, 2}, [](Array3S const&, int& val) {
      val = 42;
    });

  EXPECT_EQ(table3(2, 2, 2), 6);
  EXPECT_EQ(table3(3, 3, 4), 42);
  EXPECT_EQ(table3(4, 4, 4), 42);
}

template <typename Array>
concept has_mutable_array_view = requires(Array& array) { array.view(); };

template <typename Array>
concept has_temporary_array_view = requires(Array&& array) { std::move(array).view(); };

TEST(MultiArrayTest, ViewsPreserveLayoutConstnessAndCheckedOwnerAccess) {
  static_assert(has_mutable_array_view<MultiArray<int, 3>>);
  static_assert(!has_mutable_array_view<MultiArray<bool, 3>>);
  static_assert(!has_temporary_array_view<MultiArray<int, 3>>);
  static_assert(!has_temporary_array_view<MultiArray<int, 3> const>);

  MultiArray<int, 3> array(2, 3, 4);
  {
    auto view = array.view();
    auto const constView = std::as_const(array).view();
    static_assert(std::same_as<decltype(view[0, 0, 0]), int&>);
    static_assert(std::same_as<decltype(constView[0, 0, 0]), int const&>);
    EXPECT_EQ(view.stride(0), 12u);
    EXPECT_EQ(view.stride(1), 4u);
    EXPECT_EQ(view.stride(2), 1u);
    for (std::size_t x = 0; x < view.extent(0); ++x)
      for (std::size_t y = 0; y < view.extent(1); ++y)
        for (std::size_t z = 0; z < view.extent(2); ++z) {
          view[x, y, z] = static_cast<int>(100 * x + 10 * y + z);
          EXPECT_EQ((&view[x, y, z]), array.data() + (x * 3 + y) * 4 + z);
        }
    EXPECT_EQ((constView[1, 2, 3]), 123);
    auto checked = std::as_const(array).at(1, 2, 3);
    ASSERT_TRUE(checked);
    EXPECT_EQ(std::addressof(checked->get()), std::addressof(constView[1, 2, 3]));
  }

  EXPECT_FALSE(array.at(2, 0, 0));
  EXPECT_FALSE(std::as_const(array).at(0, 3, 0));
  auto copy = array;
  array.resize(3, 4, 5);
  auto const resizedView = std::as_const(array).view();
  EXPECT_EQ(resizedView.extent(2), 5u);
  EXPECT_EQ((resizedView[1, 2, 3]), 123);
  EXPECT_EQ(copy(1, 2, 3), 123);

  MultiArray<int, 3> empty(2, 0, 4);
  EXPECT_EQ(empty.view().size(), 0u);
  MultiArray<bool, 2> bits(2, 3);
  ASSERT_TRUE(bits.set({1, 2}, true));
  EXPECT_TRUE(bits.get({1, 2}));
  EXPECT_FALSE(bits.set({2, 0}, true));
}

template <std::size_t Rank>
void testViewInterpolation() {
  using Table = MultiTable<double, double, Rank>;
  Table table;
  typename Table::RangeArray ranges;
  typename Table::PositionArray coordinate;
  double interpolatedExpected = 0;
  double steppedExpected = 0;
  double clampedExpected = 0;
  double factor = 1;
  for (std::size_t dim = 0; dim < Rank; ++dim) {
    for (std::size_t i = 0; i < dim + 4; ++i)
      ranges[dim].push_back(static_cast<double>(i));
    coordinate[dim] = static_cast<double>(dim) + 0.25;
    interpolatedExpected += coordinate[dim] * factor;
    steppedExpected += static_cast<double>(dim) * factor;
    clampedExpected += static_cast<double>(dim + 3) * factor;
    factor *= 10;
  }
  table.setRanges(ranges);
  table.eval([](typename Table::PositionArray const& point) {
    double value = 0;
    double factor = 1;
    for (std::size_t dim = 0; dim < Rank; ++dim) {
      value += point[dim] * factor;
      factor *= 10;
    }
    return value;
  });

  for (auto mode : {InterpolationMode::Linear, InterpolationMode::Cubic, InterpolationMode::HalfStep}) {
    table.setInterpolationMode(mode);
    auto interpolated = table.interpolate(coordinate);
    ASSERT_TRUE(interpolated);
    EXPECT_NEAR(*interpolated, mode == InterpolationMode::HalfStep ? steppedExpected : interpolatedExpected, 1e-9);
    auto clamped = table.interpolate(Table::PositionArray::filled(100));
    ASSERT_TRUE(clamped);
    EXPECT_NEAR(*clamped, clampedExpected, 1e-9);
  }
}

TEST(MultiTableTest, ViewInterpolationPreservesTwoThreeAndFourDimensionalResults) {
  testViewInterpolation<2>();
  testViewInterpolation<3>();
  testViewInterpolation<4>();
}

TEST(MultiTableTest, All) {
  MultiTable2F table;
  table.setRanges({MultiTable2F::Range{0, 2, 4, 6, 8, 10}, MultiTable2F::Range{0, 5, 10}});
  table.setInterpolationMode(InterpolationMode::Linear);
  table.setBoundMode(BoundMode::Clamp);
  table.eval([](Array2F const& index) {
      return index[0] * index[1];
    });

  auto interpolated1 = table.interpolate({1.0f, 1.0f});
  ASSERT_TRUE(interpolated1);
  EXPECT_LT(std::fabs(*interpolated1 - 1.0f), 0.001f);
  auto interpolated2 = table.interpolate({9.0f, 9.0f});
  ASSERT_TRUE(interpolated2);
  EXPECT_LT(std::fabs(*interpolated2 - 81.0f), 0.001f);
  auto interpolated3 = table.interpolate({6.0f, 10.0f});
  ASSERT_TRUE(interpolated3);
  EXPECT_LT(std::fabs(*interpolated3 - 60.0f), 0.001f);
  auto interpolated4 = table.interpolate({6.0f, 11.0f});
  ASSERT_TRUE(interpolated4);
  EXPECT_LT(std::fabs(*interpolated4 - 60.0f), 0.001f);
  EXPECT_LT(std::fabs(table.get({1, 1}) - 10.0f), 0.001f);

  table.setInterpolationMode(InterpolationMode::HalfStep);
  auto halfStep1 = table.interpolate({0.5f, 0.5f});
  ASSERT_TRUE(halfStep1);
  EXPECT_LT(std::fabs(*halfStep1 - 0.0f), 0.001f);
  auto halfStep2 = table.interpolate({4.0, 4.0});
  ASSERT_TRUE(halfStep2);
  EXPECT_LT(std::fabs(*halfStep2 - 20.0f), 0.001f);
}
