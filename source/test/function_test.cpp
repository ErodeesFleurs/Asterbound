import std;
import star.json;
import star.list;
import star.interpolation;
import star.parametric_function;
import star.multi_array_interpolator;
import star.multi_table;

#include "gtest/gtest.h"
import star.stored_functions;

import star.math_common;
using namespace Star;

namespace {
template <typename T>
concept can_borrow_index = requires(T&& t) { std::forward<T>(t).index(0); };
template <typename T>
concept can_borrow_value = requires(T&& t) { std::forward<T>(t).value(0); };
template <typename T>
concept can_borrow_bin = requires(T&& t) { std::forward<T>(t).get(0); };
template <typename T>
concept can_invert = requires(T& t) { t.invert(); };

using Table = ParametricTable<int>;
static_assert(can_borrow_index<Table const&> && can_borrow_value<Table const&> && can_borrow_bin<Table const&>);
static_assert(!can_borrow_index<Table> && !can_borrow_value<Table> && !can_borrow_bin<Table>);
static_assert(!can_borrow_index<Table const> && !can_borrow_value<Table const> && !can_borrow_bin<Table const>);
static_assert(!can_invert<ParametricTable<int, long>>);
}

TEST(ParametricTableTest, StrictPredecessorBins) {
  Table table;
  EXPECT_FALSE(table.get(0));
  EXPECT_EQ(table.insert(20, 2), 0u);
  EXPECT_EQ(table.insert(10, 1), 0u);
  EXPECT_EQ(table.insert(20, 3), 2u);
  EXPECT_EQ(table.insert(30, 4), 3u);
  for (auto [query, expected] : {std::pair{9, 1}, {10, 1}, {19, 1}, {20, 1}, {21, 3}, {30, 3}, {31, 4}}) {
    auto bin = table.get(query);
    ASSERT_TRUE(bin);
    EXPECT_EQ(bin->get(), expected);
  }
  EXPECT_FALSE(table.value(table.size()));
  EXPECT_FALSE(table.index(table.size()));
  table.clear();
  EXPECT_TRUE(table.empty());
}

TEST(ParametricTableTest, BulkCreationAndMoveOnlyValues) {
  std::vector<std::pair<int, int>> points{{2, 20}, {1, 10}};
  auto table = Table::create(points);
  ASSERT_TRUE(table);
  EXPECT_EQ(points.front().first, 2);
  EXPECT_EQ(table->index(0)->get(), 1);
  points.emplace_back(1, 30);
  EXPECT_FALSE(Table::create(points));

  std::vector<std::pair<int, std::unique_ptr<int>>> owned;
  owned.emplace_back(2, std::make_unique<int>(20));
  owned.emplace_back(1, std::make_unique<int>(10));
  auto moved = ParametricTable<int, std::unique_ptr<int>>::create(std::move(owned));
  ASSERT_TRUE(moved);
  EXPECT_EQ(*moved->value(0)->get(), 10);
  EXPECT_EQ(*moved->value(1)->get(), 20);
}

TEST(ParametricTableTest, InvertOrLeaveUnchanged) {
  Table table;
  EXPECT_TRUE(table.invert());
  table.insert(1, 10);
  ASSERT_TRUE(table.invert());
  ASSERT_TRUE(table.invert());
  table.insert(2, 20);
  ASSERT_TRUE(table.is_invertible());
  ASSERT_TRUE(table.invert());
  EXPECT_EQ(table.index(0)->get(), 10);
  EXPECT_EQ(table.value(1)->get(), 2);
  ASSERT_TRUE(table.invert());
  table.insert(3, 5);
  EXPECT_FALSE(table.is_invertible());
  EXPECT_FALSE(table.invert());
  EXPECT_EQ(table.index(2)->get(), 3);
  EXPECT_EQ(table.value(2)->get(), 5);
  table.clear();
  table.insert(1, 10);
  table.insert(2, 10);
  EXPECT_FALSE(table.is_invertible());
  EXPECT_FALSE(table.invert());
  EXPECT_EQ(table.index(1)->get(), 2);
}

TEST(ParametricTableTest, CheckedConversionsPreserveModes) {
  ParametricTable<double> duplicates;
  duplicates.insert(1, 2);
  duplicates.insert(1, 3);
  EXPECT_FALSE(Table::convert(duplicates));

  ParametricFunction<double> function;
  function.set_interpolation_mode(InterpolationMode::Cubic);
  function.set_bound_mode(BoundMode::Extrapolate);
  function.insert(1, 2);
  function.insert(2, 4);
  auto converted = ParametricFunction<float>::convert(function);
  ASSERT_TRUE(converted);
  EXPECT_EQ(converted->interpolation_mode(), InterpolationMode::Cubic);
  EXPECT_EQ(converted->bound_mode(), BoundMode::Extrapolate);
  EXPECT_EQ(converted->index(0)->get(), 1);
  EXPECT_EQ(converted->value(1)->get(), 4);

  ParametricTable<double> source;
  source.insert(1.5, 1);
  EXPECT_FALSE(Table::convert(source));
  source.clear();
  source.insert(1, 1e100);
  EXPECT_FALSE(Table::convert(source));
}

TEST(StoredFunctionTest, All) {
  List<std::pair<double, double>> values = {{0.0f, 0.0f}, {1.0f, 1.0f}, {2.0f, 4.0f}, {3.0f, 9.0f}, {4.0f, 16.0f}};
  auto function = ParametricFunction<double, double>::create(values, InterpolationMode::Linear, BoundMode::Clamp);
  ASSERT_TRUE(function);
  auto levelingFunction = StoredFunction::create(*function);
  ASSERT_TRUE(levelingFunction);

  auto interpolated = function->interpolate(2.5f);
  ASSERT_TRUE(interpolated);
  EXPECT_LT(std::fabs(*interpolated - 6.5f), 0.001f);

  auto result = (*levelingFunction)->search(16.0f);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->found);
  EXPECT_LT(std::fabs(result->value - 16.0f), 0.001f);
  EXPECT_LT(std::fabs(result->solution - 4.0f), 0.001f);

  result = (*levelingFunction)->search(0.0f);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->found);
  EXPECT_LT(std::fabs(result->value - 0.0f), 0.001f);
  EXPECT_LT(std::fabs(result->solution - 0.0f), 0.001f);

  result = (*levelingFunction)->search(6.5f);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->found);
  EXPECT_LT(std::fabs(result->value - 6.5f), 0.001f);
  EXPECT_LT(std::fabs(result->solution - 2.5f), 0.001f);

  List<std::pair<double, double>> swordsValues = {{0.0f, 0.0f}, {1.0f, 10.0f}, {100.0f, 500.0f}, {9999.0f, 9999999.0f}};
  auto swordsFunction = ParametricFunction<double, double>::create(swordsValues, InterpolationMode::Linear, BoundMode::Clamp);
  ASSERT_TRUE(swordsFunction);
  auto swordsLevelingFunction = StoredFunction::create(*swordsFunction);
  ASSERT_TRUE(swordsLevelingFunction);

  result = (*swordsLevelingFunction)->search(0.0f);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->found);
  EXPECT_LT(std::fabs(result->value - 0.0f), 0.001f);
  EXPECT_LT(std::fabs(result->solution - 0.0f), 0.001f);

  result = (*swordsLevelingFunction)->search(10.0f);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->found);
  EXPECT_LT(std::fabs(result->value - 10.0f), 0.001f);
  EXPECT_LT(std::fabs(result->solution - 1.0f), 0.001f);

  result = (*swordsLevelingFunction)->search(500.0f);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->found);
  EXPECT_LT(std::fabs(result->value - 500.0f), 0.001f);
  EXPECT_LT(std::fabs(result->solution - 100.0f), 0.001f);

  result = (*swordsLevelingFunction)->search(501.0f);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->found);
  EXPECT_LT(std::fabs(result->value - 501.0f), 0.001f);
  EXPECT_LT(std::fabs(result->solution - 100.0f), 0.001f);

  result = (*swordsLevelingFunction)->search(500.01f);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->found);
  EXPECT_LT(std::fabs(result->value - 500.01f), 0.001f);
  EXPECT_LT(std::fabs(result->solution - 100.0f), 0.001f);
}
