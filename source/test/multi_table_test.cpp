import std;
import star.multi_array;
import star.interpolation;
import star.multi_array_interpolator;
import star.multi_table;

#include "gtest/gtest.h"

import star.array;
import star.math_common;
import star.vector;
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

template <std::size_t Rank> void test_view_interpolation() {
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
  table.set_ranges(ranges);
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
    table.set_interpolation_mode(mode);
    auto interpolated = table.interpolate(coordinate);
    ASSERT_TRUE(interpolated);
    EXPECT_NEAR(*interpolated, mode == InterpolationMode::HalfStep ? steppedExpected : interpolatedExpected, 1e-9);
    auto clamped = table.interpolate(Table::PositionArray::filled(100));
    ASSERT_TRUE(clamped);
    EXPECT_NEAR(*clamped, clampedExpected, 1e-9);
  }
}

TEST(MultiTableTest, ViewInterpolationPreservesTwoThreeAndFourDimensionalResults) {
  test_view_interpolation<2>();
  test_view_interpolation<3>();
  test_view_interpolation<4>();
}

TEST(MultiTableTest, All) {
  MultiTable2F table;
  table.set_ranges({MultiTable2F::Range{0, 2, 4, 6, 8, 10}, MultiTable2F::Range{0, 5, 10}});
  table.set_interpolation_mode(InterpolationMode::Linear);
  table.set_bound_mode(BoundMode::Clamp);
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

  table.set_interpolation_mode(InterpolationMode::HalfStep);
  auto halfStep1 = table.interpolate({0.5f, 0.5f});
  ASSERT_TRUE(halfStep1);
  EXPECT_LT(std::fabs(*halfStep1 - 0.0f), 0.001f);
  auto halfStep2 = table.interpolate({4.0, 4.0});
  ASSERT_TRUE(halfStep2);
  EXPECT_LT(std::fabs(*halfStep2 - 20.0f), 0.001f);
}

template <typename Interpolator>
concept has_tensor_interpolation =
  requires(Interpolator const& interpolator, typename Interpolator::MultiArray const& array,
           typename Interpolator::PositionList const& point) { interpolator.interpolate(array, point); };

struct InvalidTensorWeights {
  int operator()(double) const;
};

template <typename Weights>
concept has_linear_tensor_weights =
  requires { typename MultiArrayInterpolator2<MultiArray<double, 2>, double, Weights>; };

TEST(MultiArrayTest, InterpolationConstraintsRetainScalarAndVectorResults) {
  static_assert(has_tensor_interpolation<MultiArrayInterpolator2<MultiArray<int, 2>, double>>);
  static_assert(has_tensor_interpolation<MultiArrayInterpolator4<MultiArray<float, 3>, float>>);
  static_assert(has_tensor_interpolation<MultiArrayInterpolator2<MultiArray<Vec2F, 2>, float>>);
  static_assert(has_tensor_interpolation<MultiArrayInterpolator4<MultiArray<Vec4F, 4>, float>>);
  static_assert(!has_tensor_interpolation<MultiArrayInterpolator2<MultiArray<std::string, 2>, double>>);
  static_assert(!has_tensor_interpolation<MultiArrayInterpolator2<MultiArray<bool, 2>, double>>);
  static_assert(!has_linear_tensor_weights<InvalidTensorWeights>);
}

// An axis-by-axis scalar reference uses two taps on nonperiodic short axes and
// modular cubic polynomial samples on periodic short axes, independently of the
// tensor implementation's four-tap layout.
template <std::size_t Rank>
double tensor_reference(MultiArray<double, Rank> const& array, Array<double, Rank> const& point, BoundMode bound_mode,
                        InterpolationMode mode, Array<std::size_t, Rank> index = {}, std::size_t dim = 0) {
  if (dim == Rank)
    return array(index);
  std::vector<double> samples(array.size(dim));
  for (std::size_t i = 0; i < samples.size(); ++i) {
    index[dim] = i;
    samples[i] = tensor_reference(array, point, bound_mode, mode, index, dim + 1);
  }
  if (mode == InterpolationMode::Cubic && bound_mode == BoundMode::Wrap && samples.size() > 1 && samples.size() < 4) {
    double wrapped = std::fmod(point[dim], static_cast<double>(samples.size()));
    if (wrapped < 0)
      wrapped += samples.size();
    auto current = static_cast<std::size_t>(wrapped);
    auto extent = samples.size();
    return cubic4(wrapped - current, samples[(current + extent - 1) % extent], samples[current],
                  samples[(current + 1) % extent], samples[(current + 2) % extent]);
  }
  if (mode == InterpolationMode::Cubic && samples.size() >= 4)
    return list_interpolate4(samples, point[dim], Cubic4WeightOperator<double>(true), bound_mode);
  return list_interpolate2(samples, point[dim], LinearWeightOperator<double>{}, bound_mode);
}

template <std::size_t Rank> void test_tensor_reference(Array<std::size_t, Rank> const& shape) {
  using Table = MultiTable<double, double, Rank>;
  Table table;
  typename Table::RangeArray ranges;
  for (std::size_t dim = 0; dim < Rank; ++dim)
    for (std::size_t i = 0; i < shape[dim]; ++i)
      ranges[dim].push_back(static_cast<double>(i * i + i * (dim + 1)));
  table.set_ranges(ranges);
  table.eval([](typename Table::PositionArray const& point) {
    double value = 1;
    for (std::size_t dim = 0; dim < Rank; ++dim) {
      value += (dim + 1) * point[dim] * point[dim];
      if (dim != 0)
        value += point[dim - 1] * point[dim] * point[dim];
    }
    return value;
  });

  for (auto bound_mode : {BoundMode::Clamp, BoundMode::Wrap, BoundMode::Extrapolate}) {
    table.set_bound_mode(bound_mode);
    for (auto mode : {InterpolationMode::Linear, InterpolationMode::Cubic}) {
      table.set_interpolation_mode(mode);
      for (std::size_t sample = 0; sample < 4; ++sample) {
        typename Table::PositionArray index_point;
        typename Table::PositionArray point;
        for (std::size_t dim = 0; dim < Rank; ++dim) {
          double x = sample == 0 ? 0.35 + dim * 0.11
            : sample == 1        ? -0.35 - dim * 0.11
            : sample == 2        ? shape[dim] - 0.25
                                 : shape[dim] + 0.4;
          index_point[dim] = x;
          if (shape[dim] == 1) {
            point[dim] = ranges[dim][0] + x;
          } else {
            std::size_t lower = x < 0 ? 0 : std::min(static_cast<std::size_t>(x), shape[dim] - 2);
            point[dim] = ranges[dim][lower] + (x - lower) * (ranges[dim][lower + 1] - ranges[dim][lower]);
          }
        }
        double expected = tensor_reference(table.array(), index_point, bound_mode, mode);
        double tolerance = 1e-9 * std::max(1.0, std::abs(expected));
        auto actual = table.interpolate(point);
        ASSERT_TRUE(actual);
        EXPECT_NEAR(*actual, expected, tolerance);
        if (mode == InterpolationMode::Cubic) {
          MultiArrayInterpolator4<typename Table::MultiArray, double> interpolator(Cubic4WeightOperator<double>(true),
                                                                                   bound_mode);
          EXPECT_NEAR(interpolator.interpolate(table.array(), index_point), expected, tolerance);
        } else {
          MultiArrayInterpolator2<typename Table::MultiArray, double> interpolator(LinearWeightOperator<double>{},
                                                                                   bound_mode);
          EXPECT_NEAR(interpolator.interpolate(table.array(), index_point), expected, tolerance);
        }
      }
    }
  }
}

TEST(MultiTableTest, NonAffineReferencePreservesRegularAndMixedShortAxes) {
  test_tensor_reference<2>({4, 5});
  test_tensor_reference<2>({2, 4});
  test_tensor_reference<2>({3, 1});
  test_tensor_reference<3>({4, 5, 4});
  test_tensor_reference<3>({4, 2, 3});
  test_tensor_reference<3>({1, 4, 2});
  test_tensor_reference<4>({4, 4, 4, 5});
  test_tensor_reference<4>({4, 2, 3, 1});
  test_tensor_reference<4>({3, 4, 2, 4});
}

template <std::size_t Rank> void test_empty_axis_recovery() {
  using Table = MultiTable<double, double, Rank>;
  Table table;
  auto point = Table::PositionArray::filled(0.25);
  EXPECT_FALSE(table.interpolate(point));
  typename Table::RangeArray ranges;
  for (auto& range : ranges)
    range = {0, 1, 2, 3};
  table.set_ranges(ranges);
  for (std::size_t dim = 0; dim < Rank; ++dim) {
    table.set_range(dim, {});
    for (auto mode : {InterpolationMode::Linear, InterpolationMode::Cubic, InterpolationMode::HalfStep}) {
      table.set_interpolation_mode(mode);
      auto result = table.interpolate(point);
      ASSERT_FALSE(result);
      EXPECT_TRUE(result.error().template is<MathException>());
    }
    typename Table::Interpolator2 linear(LinearWeightOperator<double>{});
    typename Table::Interpolator4 cubic(Cubic4WeightOperator<double>{});
    typename Table::PiecewiseInterpolator piecewise(StepWeightOperator<double>{});
    EXPECT_DOUBLE_EQ(linear.interpolate(table.array(), point), 0);
    EXPECT_DOUBLE_EQ(cubic.interpolate(table.array(), point), 0);
    EXPECT_DOUBLE_EQ(piecewise.interpolate(table.array(), point), 0);
    table.set_range(dim, ranges[dim]);
    table.eval([](auto const&) { return 7.0; });
    for (auto mode : {InterpolationMode::Linear, InterpolationMode::Cubic, InterpolationMode::HalfStep}) {
      table.set_interpolation_mode(mode);
      auto result = table(point);
      ASSERT_TRUE(result);
      EXPECT_DOUBLE_EQ(*result, 7);
    }
  }
  table.array().clear();
  EXPECT_FALSE(table.interpolate(point));
}

TEST(MultiTableTest, EmptyAxesFailCheckedInterpolationAndRecoverAfterReconfiguration) {
  test_empty_axis_recovery<2>();
  test_empty_axis_recovery<3>();
  test_empty_axis_recovery<4>();
}
