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
  table.for_each([](Array2S const& index, int& val) {
      val = (index[0] + 1) * index[1];
    });

  EXPECT_EQ(table(3, 4), 16);
  EXPECT_EQ(table(5, 2), 12);
  EXPECT_EQ(table(0, 9), 9);
  EXPECT_EQ(table(8, 1), 9);
  EXPECT_EQ(table(0, 1), 1);
  EXPECT_EQ(table(8, 9), 81);

  MultiArray<int, 3> table3(5, 6, 7);
  table3.for_each([](Array3S const& index, int& val) {
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

  table3.for_each({3, 3, 3}, {2, 2, 2}, [](Array3S const&, int& val) {
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
          EXPECT_EQ((&view[x, y, z]), std::addressof(array.elements()[(x * 3 + y) * 4 + z]));
        }
    EXPECT_EQ((constView[1, 2, 3]), 123);
    auto checked = std::as_const(array).at(1, 2, 3);
    ASSERT_TRUE(checked);
    EXPECT_EQ(std::addressof(checked->get()), std::addressof(constView[1, 2, 3]));
  }

  EXPECT_FALSE(array.at(2, 0, 0));
  EXPECT_FALSE(std::as_const(array).at(0, 3, 0));
  auto copy = array;
  ASSERT_TRUE(array.resize(3, 4, 5));
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
  ASSERT_TRUE(table.set_ranges(ranges));
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
  ASSERT_TRUE(table.set_ranges({MultiTable2F::Range{0, 2, 4, 6, 8, 10}, MultiTable2F::Range{0, 5, 10}}));
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
  EXPECT_LT(std::fabs(table[{1, 1}] - 10.0f), 0.001f);

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
  std::vector<double> samples(array.extent(dim));
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
  ASSERT_TRUE(table.set_ranges(ranges));
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
  ASSERT_TRUE(table.set_ranges(ranges));
  for (std::size_t dim = 0; dim < Rank; ++dim) {
    ASSERT_TRUE(table.set_range(dim, {}));
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
    ASSERT_TRUE(table.set_range(dim, ranges[dim]));
    table.eval([](auto const&) { return 7.0; });
    for (auto mode : {InterpolationMode::Linear, InterpolationMode::Cubic, InterpolationMode::HalfStep}) {
      table.set_interpolation_mode(mode);
      auto result = table(point);
      ASSERT_TRUE(result);
      EXPECT_DOUBLE_EQ(*result, 7);
    }
  }
  table.clear();
  EXPECT_FALSE(table.interpolate(point));
}

TEST(MultiTableTest, EmptyAxesFailCheckedInterpolationAndRecoverAfterReconfiguration) {
  test_empty_axis_recovery<2>();
  test_empty_axis_recovery<3>();
  test_empty_axis_recovery<4>();
}

template <std::size_t Rank>
concept has_array_rank = requires { typename MultiArray<int, Rank>; };

template <std::size_t Rank>
concept has_table_rank = requires { typename MultiTable<int, double, Rank>; };

template <typename Array>
concept has_temporary_elements = requires(Array&& array) { std::move(array).elements(); };

template <typename Array>
concept has_checked_array_access = requires(Array& array, typename Array::IndexArray const& index) { array.at(index); };

template <typename Array>
concept has_temporary_checked_access =
  requires(Array&& array, typename Array::IndexArray const& index) { std::move(array).at(index); };

template <typename Array>
concept accepts_inexact_shape = requires(Array& array) { array.set_size(1, 2, 3); };

template <typename Array>
concept accepts_boolean_shape = requires(Array& array) { array.set_size(true, 2); };

TEST(MultiArrayTest, RankAndBorrowingConstraintsMatchStorage) {
  static_assert(!has_array_rank<0>);
  static_assert(!has_table_rank<0>);
  static_assert(has_array_rank<1>);
  static_assert(has_table_rank<1>);
  static_assert(!accepts_inexact_shape<MultiArray<int, 2>>);
  static_assert(!accepts_boolean_shape<MultiArray<int, 2>>);
  static_assert(has_checked_array_access<MultiArray<int, 2>>);
  static_assert(!has_checked_array_access<MultiArray<bool, 2>>);
  static_assert(!has_temporary_checked_access<MultiArray<int, 2>>);
  static_assert(!has_temporary_checked_access<MultiArray<int, 2> const>);
  static_assert(!has_temporary_elements<MultiArray<int, 2>>);
  static_assert(!has_temporary_elements<MultiArray<int, 2> const>);
  MultiArray<int, 1> array(3);
  ASSERT_TRUE(array.set(Array<std::size_t, 1>{2}, 17));
  EXPECT_EQ(array(2), 17);
  EXPECT_EQ(array[2], 17);
  EXPECT_EQ(array.size(), 3u);
  EXPECT_EQ(array.extent(0), 3u);
}

TEST(MultiArrayTest, InvalidShapesAndGrowthPreserveEveryCoordinate) {
  MultiArray<int, 2> array(2, 3);
  array.for_each([](Array2S const& index, int& value) { value = static_cast<int>(10 * index[0] + index[1]); });
  auto unchanged = [&] {
    EXPECT_EQ(array.extents(), (Array2S{2, 3}));
    EXPECT_EQ(array.size(), 6u);
    for (std::size_t x = 0; x < 2; ++x)
      for (std::size_t y = 0; y < 3; ++y)
        EXPECT_EQ(array(x, y), static_cast<int>(10 * x + y));
  };
  auto invalid = array.resize({array.max_size(), 2});
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<MultiArrayException>());
  unchanged();
  EXPECT_FALSE(array.set_size({2, array.max_size()}));
  unchanged();
  EXPECT_FALSE(array.set_size(-1, 3));
  unchanged();
  EXPECT_FALSE(array.resize(2, -1));
  unchanged();
  EXPECT_FALSE(array.set_size({std::numeric_limits<std::size_t>::max(), 0}));
  unchanged();
  EXPECT_FALSE(array.set_resize({std::numeric_limits<std::size_t>::max(), 0}, 99));
  unchanged();
  EXPECT_FALSE(array.at(-1, 0));
  EXPECT_FALSE(array.set({2, 0}, 99));
  unchanged();
  ASSERT_TRUE(array.set_resize({3, 4}, 99));
  EXPECT_EQ(array.extents(), (Array2S{4, 5}));
  EXPECT_EQ(array.size(), 20u);
  for (std::size_t x = 0; x < 4; ++x)
    for (std::size_t y = 0; y < 5; ++y)
      EXPECT_EQ(array(x, y), x == 3 && y == 4 ? 99 : x < 2 && y < 3 ? static_cast<int>(10 * x + y) : 0);
}

TEST(MultiArrayTest, ResizePreservesCoordinateIntersectionAndFillsOnlyNewCells) {
  MultiArray<int, 3> array(2, 3, 4);
  array.for_each([](Array3S const& index, int& value) {
    value = static_cast<int>(100 * index[0] + 10 * index[1] + index[2]);
  });
  ASSERT_TRUE(array.resize({3, 2, 5}, -7));
  EXPECT_EQ(array.size(), 30u);
  for (std::size_t x = 0; x < 3; ++x)
    for (std::size_t y = 0; y < 2; ++y)
      for (std::size_t z = 0; z < 5; ++z)
        EXPECT_EQ(array(x, y, z), x < 2 && z < 4 ? static_cast<int>(100 * x + 10 * y + z) : -7);
  ASSERT_TRUE(array.resize({3, 2, 5}, 999));
  EXPECT_EQ(array(2, 1, 4), -7);
  ASSERT_TRUE(array.resize({1, 2, 2}, 999));
  EXPECT_EQ(array.size(), 4u);
  EXPECT_EQ(array(0, 0, 0), 0);
  EXPECT_EQ(array(0, 0, 1), 1);
  EXPECT_EQ(array(0, 1, 0), 10);
  EXPECT_EQ(array(0, 1, 1), 11);
}

TEST(MultiArrayTest, LinearSetSizeRetainsPrefixAndZeroShapesRetainCapacity) {
  MultiArray<int, 3> array(2, 3, 4);
  for (std::size_t i = 0; i < array.size(); ++i)
    array[i] = static_cast<int>(i + 1);
  ASSERT_TRUE(array.set_size({3, 2, 4}, -1));
  for (std::size_t i = 0; i < array.size(); ++i)
    EXPECT_EQ(array[i], i + 1);
  ASSERT_TRUE(array.set_size({3, 3, 4}, -1));
  for (std::size_t i = 0; i < array.size(); ++i)
    EXPECT_EQ(array[i], i < 24 ? static_cast<int>(i + 1) : -1);
  auto capacity = array.capacity();
  for (auto shape : {Array3S{0, 3, 4}, Array3S{2, 0, 4}, Array3S{2, 3, 0}}) {
    ASSERT_TRUE(array.resize(shape, 77));
    EXPECT_EQ(array.extents(), shape);
    EXPECT_TRUE(array.empty());
    EXPECT_EQ(array.size(), 0u);
    EXPECT_EQ(array.capacity(), capacity);
    EXPECT_TRUE(array.elements().empty());
    std::size_t visits = 0;
    array.for_each([&](auto const&, auto&) { ++visits; });
    EXPECT_EQ(visits, 0u);
    MultiArray<int, 3> target(2, 3, 4);
    target.fill(17);
    ASSERT_TRUE(target.copy_from(array));
    ASSERT_TRUE(array.copy_from(target));
    for (auto value : target.elements())
      EXPECT_EQ(value, 17);
    ASSERT_TRUE(array.set_size({2, 3, 4}, 5));
    EXPECT_EQ(array.capacity(), capacity);
    for (auto value : array.elements())
      EXPECT_EQ(value, 5);
  }
  array.clear();
  EXPECT_EQ(array.extents(), Array3S::filled(0));
  EXPECT_TRUE(array.empty());
  EXPECT_EQ(array.capacity(), capacity);
  ASSERT_TRUE(array.set_size({2, 3, 4}));
  EXPECT_EQ(array.capacity(), capacity);
  for (auto value : array.elements())
    EXPECT_EQ(value, 0);
}

TEST(MultiArrayTest, CopyRegionsValidateBeforeWritingAndRespectOffsets) {
  MultiArray<int, 2> array(4, 6);
  array.for_each([](Array2S const& index, int& value) { value = static_cast<int>(10 * index[0] + index[1]); });
  auto original = array;
  EXPECT_FALSE(array.copy_region(array, {0, 0}, {2, 4}, {1, 1}));
  EXPECT_FALSE(array.copy_region(original, {2, 1}, {1, 3}, {0, 0}));
  EXPECT_FALSE(array.copy_region(original, {0, 0}, {5, 1}, {0, 0}));
  EXPECT_FALSE(array.copy_region(original, {0, 0}, {2, 4}, {3, 3}));
  EXPECT_FALSE(array.copy_region(original, {4, 6}, {4, 6}, {5, 6}));
  for (std::size_t i = 0; i < array.size(); ++i)
    EXPECT_EQ(array[i], original[i]);
  ASSERT_TRUE(array.copy_region(array, {0, 1}, {2, 3}, {0, 1}));
  ASSERT_TRUE(array.copy_region(array, {4, 6}, {4, 6}, {4, 6}));
  for (std::size_t i = 0; i < array.size(); ++i)
    EXPECT_EQ(array[i], original[i]);
  ASSERT_TRUE(array.copy_region(array, {0, 1}, {2, 3}, {2, 3}));
  for (std::size_t x = 0; x < 4; ++x)
    for (std::size_t y = 0; y < 6; ++y)
      EXPECT_EQ(array(x, y), x >= 2 && y >= 3 && y < 5 ? original(x - 2, y - 2) : original(x, y));
  MultiArray<int, 2> clipped(2, 3);
  ASSERT_TRUE(clipped.copy_from(original));
  for (std::size_t x = 0; x < 2; ++x)
    for (std::size_t y = 0; y < 3; ++y)
      EXPECT_EQ(clipped(x, y), original(x, y));
  MultiArray<int, 2> offset(4, 6);
  offset.fill(-1);
  ASSERT_TRUE(offset.copy_region(original, {1, 2}, {3, 5}, {0, 1}));
  for (std::size_t x = 0; x < 4; ++x)
    for (std::size_t y = 0; y < 6; ++y)
      EXPECT_EQ(offset(x, y), x < 2 && y >= 1 && y < 4 ? original(x + 1, y + 1) : -1);
}

TEST(MultiArrayTest, ProxyStorageSupportsIndexingTraversalAndRegionCopies) {
  MultiArray<bool, 2> bits(3, 4);
  bits(1, 2) = true;
  bits[3] = true;
  ASSERT_TRUE(bits.set({2, 1}, true));
  EXPECT_TRUE(std::as_const(bits).get({1, 2}));
  EXPECT_TRUE(std::as_const(bits)(0, 3));
  EXPECT_TRUE(std::as_const(bits)[9]);
  EXPECT_FALSE(std::as_const(bits).get({3, 0}));
  EXPECT_TRUE(std::as_const(bits).get({3, 0}, true));
  std::size_t visits = 0;
  bits.for_each([&](Array2S const& index, auto&& value) {
    EXPECT_EQ(index[0] * 4 + index[1], visits++);
    value = (index[0] + index[1]) % 2 != 0;
  });
  EXPECT_EQ(visits, 12u);
  std::size_t true_values = 0;
  std::as_const(bits).for_each([&](auto const&, auto value) { true_values += static_cast<bool>(value); });
  EXPECT_EQ(true_values, 6u);
  MultiArray<bool, 2> copied(3, 4);
  ASSERT_TRUE(copied.copy_region(bits, {1, 1}, {3, 4}, {0, 0}));
  for (std::size_t x = 0; x < 3; ++x)
    for (std::size_t y = 0; y < 4; ++y)
      EXPECT_EQ(static_cast<bool>(copied(x, y)), x < 2 && y < 3 ? static_cast<bool>(bits(x + 1, y + 1)) : false);
  ASSERT_TRUE(bits.resize({4, 5}, true));
  EXPECT_EQ(bits.size(), 20u);
  for (std::size_t x = 0; x < 4; ++x)
    for (std::size_t y = 0; y < 5; ++y)
      EXPECT_EQ(static_cast<bool>(bits(x, y)), x < 3 && y < 4 ? (x + y) % 2 != 0 : true);
}

TEST(MultiArrayTest, MoveOnlyOwnersAndBorrowedMutableVisitorsRemainUsable) {
  MultiArray<std::unique_ptr<int>, 2> owners;
  ASSERT_TRUE(owners.set_size(2, 3));
  owners(1, 2) = std::make_unique<int>(42);
  ASSERT_TRUE(owners.set_size({3, 2}));
  ASSERT_TRUE(owners[5]);
  EXPECT_EQ(*owners[5], 42);
  auto moved = std::move(owners);
  EXPECT_TRUE(owners.empty());
  EXPECT_EQ(owners.extents(), Array2S::filled(0));
  EXPECT_EQ(*moved[5], 42);
  owners = std::move(moved);
  EXPECT_TRUE(moved.empty());
  EXPECT_EQ(moved.extents(), Array2S::filled(0));
  auto capacity = owners.capacity();
  owners.clear();
  EXPECT_EQ(owners.capacity(), capacity);
  ASSERT_TRUE(owners.set_size(2, 3));
  EXPECT_FALSE(owners[5]);

  MultiArray<int, 2> array(2, 3);
  std::vector<Array2S> visited;
  auto visitor = [state = std::make_unique<int>(0), &visited](Array2S& index, int& value) mutable {
    value = ++*state;
    visited.push_back(index);
  };
  array.for_each(visitor);
  EXPECT_EQ(visited, (std::vector<Array2S>{{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}}));
  auto checked = array.at({1, 2});
  ASSERT_TRUE(checked);
  checked->get() = 99;
  EXPECT_EQ(array(1, 2), 99);
  int total = 0;
  std::as_const(array).for_each([&](Array2S const&, int const& value) { total += value; });
  EXPECT_EQ(total, 114);
}

template <typename Table>
concept has_temporary_table_array = requires(Table&& table) { std::move(table).array(); };

TEST(MultiTableTest, OwnerIsConstWhileBoundedBorrowsWriteThrough) {
  using Table = MultiTable2D;
  static_assert(std::same_as<decltype(std::declval<Table&>().array()), Table::MultiArray const&>);
  static_assert(!has_temporary_table_array<Table>);
  static_assert(!has_temporary_table_array<Table const>);
  static_assert(!has_temporary_array_view<Table>);
  static_assert(!has_temporary_elements<Table>);
  static_assert(!has_temporary_checked_access<Table>);
  static_assert(!has_mutable_array_view<MultiTable<bool, double, 2>>);
  static_assert(!has_checked_array_access<MultiTable<bool, double, 2>>);
  Table table;
  ASSERT_TRUE(table.set_ranges({Table::Range{0, 1}, Table::Range{0, 1, 2}}));
  table[{1, 2}] = 17;
  auto checked = table.at({1, 2});
  ASSERT_TRUE(checked);
  checked->get() = 23;
  EXPECT_EQ((std::as_const(table)[{1, 2}]), 23);
  auto const_checked = std::as_const(table).at({1, 2});
  ASSERT_TRUE(const_checked);
  static_assert(std::same_as<decltype(const_checked->get()), double const&>);
  EXPECT_FALSE(table.at({2, 0}));
  EXPECT_FALSE(table.set({0, 3}, 99));
  auto view = table.view();
  auto const_view = std::as_const(table).view();
  static_assert(std::same_as<decltype(view[0, 0]), double&>);
  static_assert(std::same_as<decltype(const_view[0, 0]), double const&>);
  view[0, 1] = 31;
  table.elements()[0] = 7;
  EXPECT_EQ((table[{0, 1}]), 31);
  EXPECT_EQ(std::as_const(table).elements()[0], 7);
  EXPECT_EQ(table.array().extents(), (Array2S{2, 3}));
}

TEST(MultiTableTest, InvalidConfigurationPreservesAxesDataAndInterpolation) {
  MultiTable2D table;
  ASSERT_TRUE(table.set_ranges({MultiTable2D::Range{0, 2}, MultiTable2D::Range{0, 4}}));
  table.eval([](Array2D const& point) { return point[0] + point[1] * 10; });
  auto unchanged = [&] {
    EXPECT_EQ(table.array().extents(), (Array2S{2, 2}));
    EXPECT_EQ((table[{0, 0}]), 0);
    EXPECT_EQ((table[{1, 0}]), 2);
    EXPECT_EQ((table[{0, 1}]), 40);
    EXPECT_EQ((table[{1, 1}]), 42);
    auto value = table.interpolate({1, 2});
    ASSERT_TRUE(value);
    EXPECT_DOUBLE_EQ(*value, 21);
  };
  auto invalid = table.set_range(2, {0, 1});
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<MathException>());
  unchanged();
  EXPECT_FALSE(table.set_range(0, {0, 2, 1}));
  unchanged();
  EXPECT_FALSE(table.set_ranges({MultiTable2D::Range{0, 1, 2}, MultiTable2D::Range{1, 0}}));
  unchanged();
  MultiArray2D wrong_shape(2, 3);
  EXPECT_FALSE(table.assign({MultiTable2D::Range{0, 2}, MultiTable2D::Range{0, 4}}, std::move(wrong_shape)));
  unchanged();
  MultiArray2D unsorted_data(2, 2);
  EXPECT_FALSE(table.assign({MultiTable2D::Range{2, 0}, MultiTable2D::Range{0, 4}}, std::move(unsorted_data)));
  unchanged();
  ASSERT_TRUE(table.set_range(0, {0, 2, 5}));
  EXPECT_EQ(table.array().extents(), (Array2S{3, 2}));
  EXPECT_EQ((table[{1, 1}]), 42);
  EXPECT_EQ((table[{2, 1}]), 0);
  table.set_interpolation_mode(static_cast<InterpolationMode>(-1));
  auto unsupported = table.interpolate({1, 2});
  ASSERT_FALSE(unsupported);
  EXPECT_TRUE(unsupported.error().is<MathException>());
  table.set_interpolation_mode(InterpolationMode::Linear);
  EXPECT_TRUE(table.interpolate({1, 2}));
}

TEST(MultiTableTest, AssignAdoptsStorageAndClearAndMovesPreserveShapeInvariant) {
  MultiArray2D data(2, 3);
  data.for_each([](Array2S const& index, double& value) { value = index[0] + index[1] * 10; });
  auto original_elements = data.elements();
  MultiTable2D table;
  ASSERT_TRUE(table.assign({MultiTable2D::Range{0, 1}, MultiTable2D::Range{0, 1, 2}}, std::move(data)));
  EXPECT_EQ(std::addressof(table.elements()[0]), std::addressof(original_elements[0]));
  EXPECT_TRUE(data.empty());
  EXPECT_EQ(data.extents(), Array2S::filled(0));
  auto value = table.interpolate({0.5, 1.5});
  ASSERT_TRUE(value);
  EXPECT_DOUBLE_EQ(*value, 15.5);
  auto moved = std::move(table);
  EXPECT_TRUE(table.array().empty());
  EXPECT_EQ(table.array().extents(), Array2S::filled(0));
  EXPECT_FALSE(table.interpolate({0, 0}));
  EXPECT_EQ((moved[{1, 2}]), 21);
  table = std::move(moved);
  EXPECT_TRUE(moved.array().empty());
  EXPECT_FALSE(moved.interpolate({0, 0}));
  auto capacity = table.array().capacity();
  table.clear();
  EXPECT_TRUE(table.array().empty());
  EXPECT_EQ(table.array().extents(), Array2S::filled(0));
  EXPECT_EQ(table.array().capacity(), capacity);
  EXPECT_FALSE(table.interpolate({0, 0}));
  ASSERT_TRUE(table.set_ranges({MultiTable2D::Range{0, 1}, MultiTable2D::Range{0, 1, 2}}));
  EXPECT_EQ(table.array().capacity(), capacity);
  table.eval([](Array2D&) { return 7; });
  auto recovered = table.interpolate({0.5, 0.5});
  ASSERT_TRUE(recovered);
  EXPECT_DOUBLE_EQ(*recovered, 7);

  using OwnedTable = MultiTable<std::unique_ptr<int>, double, 1>;
  OwnedTable::MultiArray owned_data(2);
  owned_data[1] = std::make_unique<int>(37);
  OwnedTable owners;
  ASSERT_TRUE(owners.assign(OwnedTable::RangeArray{OwnedTable::Range{0, 1}}, std::move(owned_data)));
  ASSERT_TRUE((owners[OwnedTable::IndexArray{1}]));
  EXPECT_EQ((*owners[OwnedTable::IndexArray{1}]), 37);
  owners.clear();
  EXPECT_TRUE(owners.array().empty());
}

TEST(MultiTableTest, BorrowedNoncopyableEvaluatorVisitsConfiguredCoordinates) {
  MultiTable2D table;
  ASSERT_TRUE(table.set_ranges({MultiTable2D::Range{1, 3}, MultiTable2D::Range{2, 4, 6}}));
  std::vector<Array2D> visited;
  auto evaluator = [state = std::make_unique<int>(0), &visited](Array2D& point) mutable {
    visited.push_back(point);
    return point[0] * 10 + point[1] + ++*state;
  };
  static_assert(multi_table_evaluator<decltype(evaluator), Array2D, double&>);
  static_assert(!multi_table_evaluator<int, Array2D, double&>);
  table.eval(evaluator);
  EXPECT_EQ(visited, (std::vector<Array2D>{{1, 2}, {1, 4}, {1, 6}, {3, 2}, {3, 4}, {3, 6}}));
  EXPECT_EQ((table[{0, 0}]), 13);
  EXPECT_EQ((table[{1, 2}]), 42);
  using BitTable = MultiTable<bool, double, 1>;
  BitTable bits;
  ASSERT_TRUE(bits.set_ranges(BitTable::RangeArray{BitTable::Range{0, 1, 2}}));
  bits.eval([](Array<double, 1>& point) { return point[0] != 1; });
  EXPECT_TRUE((bits[BitTable::IndexArray{0}]));
  EXPECT_FALSE((std::as_const(bits)[BitTable::IndexArray{1}]));
  EXPECT_TRUE((bits[BitTable::IndexArray{2}]));
}

TEST(MultiTableTest, DuplicateAxesKeepLowerInverseSelectionAndShortDomainBehavior) {
  using Table = MultiTable<double, double, 1>;
  Table table;
  Table::MultiArray data(4);
  data[0] = 10;
  data[1] = 20;
  data[2] = 80;
  data[3] = 100;
  ASSERT_TRUE(table.assign(Table::RangeArray{Table::Range{0, 1, 1, 2}}, std::move(data)));
  for (auto mode : {InterpolationMode::Linear, InterpolationMode::Cubic, InterpolationMode::HalfStep}) {
    table.set_interpolation_mode(mode);
    auto exact = table.interpolate(Table::PositionArray{1});
    ASSERT_TRUE(exact);
    EXPECT_DOUBLE_EQ(*exact, 20);
  }
  table.set_interpolation_mode(InterpolationMode::Linear);
  auto between = table.interpolate(Table::PositionArray{1.5});
  ASSERT_TRUE(between);
  EXPECT_DOUBLE_EQ(*between, 90);
  table.set_bound_mode(BoundMode::Extrapolate);
  auto below = table.interpolate(Table::PositionArray{-1});
  auto above = table.interpolate(Table::PositionArray{3});
  ASSERT_TRUE(below);
  ASSERT_TRUE(above);
  EXPECT_DOUBLE_EQ(*below, 0);
  EXPECT_DOUBLE_EQ(*above, 120);
  ASSERT_TRUE(table.set_ranges(Table::RangeArray{Table::Range{1, 1, 1, 1}}));
  // The legacy inverse chooses the penultimate interval above a flat domain.
  for (auto [point, expected] : {std::pair{-100.0, 10.0}, std::pair{1.0, 10.0}, std::pair{100.0, 80.0}}) {
    auto result = table.interpolate(Table::PositionArray{point});
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, expected);
  }
  ASSERT_TRUE(table.assign(Table::RangeArray{Table::Range{}}, Table::MultiArray{}));
  EXPECT_FALSE(table.interpolate(Table::PositionArray{1}));
  ASSERT_TRUE(table.set_range(0, {3}));
  ASSERT_TRUE(table.set(Table::IndexArray{0}, 17));
  for (auto mode : {InterpolationMode::Linear, InterpolationMode::Cubic, InterpolationMode::HalfStep}) {
    table.set_interpolation_mode(mode);
    for (auto bound : {BoundMode::Clamp, BoundMode::Wrap, BoundMode::Extrapolate}) {
      table.set_bound_mode(bound);
      auto singleton = table.interpolate(Table::PositionArray{100});
      ASSERT_TRUE(singleton);
      EXPECT_DOUBLE_EQ(*singleton, 17);
    }
  }
}

TEST(MultiArrayTest, RankOneResizeAndClippedCopyPreserveCoordinates) {
  MultiArray<int, 1> array(4);
  for (std::size_t i = 0; i < array.size(); ++i)
    array[i] = static_cast<int>(i + 1);
  ASSERT_TRUE(array.resize(Array<std::size_t, 1>{6}, -1));
  for (std::size_t i = 0; i < array.size(); ++i)
    EXPECT_EQ(array(i), i < 4 ? static_cast<int>(i + 1) : -1);
  ASSERT_TRUE(array.resize(3));
  MultiArray<int, 1> clipped(2);
  ASSERT_TRUE(clipped.copy_from(array));
  EXPECT_EQ(clipped[0], 1);
  EXPECT_EQ(clipped[1], 2);
  auto& same = array;
  array = std::move(same);
  EXPECT_EQ(array.extent(0), 3u);
  EXPECT_EQ(array.size(), 3u);
  EXPECT_EQ(array[2], 3);
}

TEST(MultiArrayTest, NonzeroSubregionVisitsExactLexicographicCoordinates) {
  MultiArray<int, 3> array(4, 5, 6);
  std::vector<Array3S> visited;
  auto visitor = [token = std::make_unique<int>(37), &visited](Array3S& index, int& value) mutable {
    visited.push_back(index);
    value = *token;
  };
  array.for_each({1, 2, 3}, {2, 2, 2}, visitor);
  EXPECT_EQ(visited, (std::vector<Array3S>{{1, 2, 3}, {1, 2, 4}, {1, 3, 3}, {1, 3, 4},
                                        {2, 2, 3}, {2, 2, 4}, {2, 3, 3}, {2, 3, 4}}));
  for (std::size_t x = 0; x < 4; ++x)
    for (std::size_t y = 0; y < 5; ++y)
      for (std::size_t z = 0; z < 6; ++z)
        EXPECT_EQ(array(x, y, z), x >= 1 && x < 3 && y >= 2 && y < 4 && z >= 3 && z < 5 ? 37 : 0);
  std::vector<Array3S> const_visited;
  std::as_const(array).for_each({1, 2, 3}, {2, 2, 2}, [&](Array3S const& index, int const& value) {
    const_visited.push_back(index);
    EXPECT_EQ(value, 37);
  });
  EXPECT_EQ(const_visited, visited);
}

struct NonDefaultArrayValue {
  NonDefaultArrayValue() = delete;
  explicit NonDefaultArrayValue(int value) : value(value) {}
  NonDefaultArrayValue(NonDefaultArrayValue const&) = default;
  NonDefaultArrayValue& operator=(NonDefaultArrayValue const&) = delete;
  int value;
};

TEST(MultiArrayTest, FillResizeDoesNotRequireDefaultConstructionOrAssignment) {
  MultiArray<NonDefaultArrayValue, 2> array(Array2S{2, 3}, NonDefaultArrayValue{7});
  array(1, 1).value = 42;
  ASSERT_TRUE(array.resize({3, 2}, NonDefaultArrayValue{99}));
  EXPECT_EQ(array.size(), 6u);
  for (std::size_t x = 0; x < 3; ++x)
    for (std::size_t y = 0; y < 2; ++y)
      EXPECT_EQ(array(x, y).value, x == 2 ? 99 : x == 1 && y == 1 ? 42 : 7);
}

struct AssignmentArrayValue {
  AssignmentArrayValue() = default;
  AssignmentArrayValue(AssignmentArrayValue const&) = delete;
  AssignmentArrayValue(AssignmentArrayValue&&) = default;
  AssignmentArrayValue& operator=(AssignmentArrayValue const&) = default;
  int value = 0;
};

TEST(MultiArrayTest, DefaultResizePreservesCopyAssignableNoncopyConstructibleValues) {
  MultiArray<AssignmentArrayValue, 2> array(2, 3);
  array.for_each([](Array2S const& index, AssignmentArrayValue& element) {
    element.value = static_cast<int>(10 * index[0] + index[1] + 1);
  });
  ASSERT_TRUE(array.resize(3, 2));
  EXPECT_EQ(array.size(), 6u);
  for (std::size_t x = 0; x < 3; ++x)
    for (std::size_t y = 0; y < 2; ++y)
      EXPECT_EQ(array(x, y).value, x < 2 ? static_cast<int>(10 * x + y + 1) : 0);
}

struct CopyOnlyArrayValue {
  CopyOnlyArrayValue() = default;
  explicit CopyOnlyArrayValue(int value) : value(value) {}
  explicit CopyOnlyArrayValue(CopyOnlyArrayValue const&) = default;
  CopyOnlyArrayValue& operator=(CopyOnlyArrayValue const&) = default;
  int value = 0;
};

TEST(MultiArrayTest, ConstOwningGetSupportsCopyOnlyValuesAndFallbacks) {
  MultiArray<CopyOnlyArrayValue, 1> array(Array<std::size_t, 1>{2}, CopyOnlyArrayValue{7});
  array(1).value = 17;
  auto first = std::as_const(array).get(Array<std::size_t, 1>{0});
  auto second = std::as_const(array).get(Array<std::size_t, 1>{1}, CopyOnlyArrayValue{99});
  auto defaulted = std::as_const(array).get(Array<std::size_t, 1>{2});
  auto explicit_fallback = std::as_const(array).get(Array<std::size_t, 1>{2}, CopyOnlyArrayValue{99});
  EXPECT_EQ(first.value, 7);
  EXPECT_EQ(second.value, 17);
  EXPECT_EQ(defaulted.value, 0);
  EXPECT_EQ(explicit_fallback.value, 99);
  first.value = 42;
  EXPECT_EQ(array(0).value, 7);
}

template <typename Table>
concept has_table_configuration = requires(Table& table, typename Table::RangeArray ranges) {
  table.set_range(0, std::move(ranges[0]));
  table.set_ranges(std::move(ranges));
};

template <typename Table>
concept has_table_assignment =
  requires(Table& table, typename Table::RangeArray ranges, typename Table::MultiArray data) {
    table.assign(std::move(ranges), std::move(data));
  };

template <typename Table>
concept has_table_element_set =
  requires(Table& table, typename Table::IndexArray const& index, typename Table::Element value) {
    table.set(index, std::move(value));
  };

struct NonDefaultTableValue {
  NonDefaultTableValue() = delete;
  explicit NonDefaultTableValue(int value) : value(value) {}
  NonDefaultTableValue(NonDefaultTableValue const&) = delete;
  NonDefaultTableValue(NonDefaultTableValue&&) = default;
  NonDefaultTableValue& operator=(NonDefaultTableValue&&) = default;
  int value;
};

struct NonAssignableTableValue {
  NonAssignableTableValue() = default;
  NonAssignableTableValue(NonAssignableTableValue const&) = delete;
  NonAssignableTableValue(NonAssignableTableValue&&) = default;
  NonAssignableTableValue& operator=(NonAssignableTableValue const&) = delete;
  NonAssignableTableValue& operator=(NonAssignableTableValue&&) = delete;
};

TEST(MultiTableTest, OperationConstraintsKeepMoveAdoptionIndependentOfElementCopying) {
  using OwnedTable = MultiTable<NonDefaultTableValue, double, 2>;
  static_assert(has_table_configuration<MultiTable2D>);
  static_assert(!has_table_configuration<OwnedTable>);
  static_assert(has_table_assignment<OwnedTable>);
  static_assert(has_mutable_array_view<OwnedTable>);
  static_assert(has_table_element_set<OwnedTable>);
  static_assert(!has_table_element_set<MultiTable<NonAssignableTableValue, double, 2>>);

  MultiTable2D populated;
  ASSERT_TRUE(populated.set_ranges({MultiTable2D::Range{0, 1}, MultiTable2D::Range{0, 1}}));
  populated.eval([](Array2D const& point) { return point[0] + 2 * point[1]; });
  auto& same = populated;
  populated = std::move(same);
  EXPECT_EQ(populated.array().extents(), (Array2S{2, 2}));
  auto sampled = populated.interpolate({0.5, 0.5});
  ASSERT_TRUE(sampled);
  EXPECT_DOUBLE_EQ(*sampled, 1.5);
}
