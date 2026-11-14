import std;
import star.list;
import star.weighted_pool;
import star.random;
import star.static_random;
import star.json;
import star.json_extra;
import star.memory;

#include "gtest/gtest.h"

using namespace Star;

namespace {
template <typename Pool>
concept temporary_item_borrow = requires(Pool&& pool) { std::move(pool).item(0); };
template <typename Pool>
concept temporary_entry_borrow = requires(Pool&& pool) { std::move(pool).at(0); };
template <typename Pool>
concept temporary_items_borrow = requires(Pool&& pool) { std::move(pool).items(); };
template <typename Pool>
concept owning_selection = requires(Pool const& pool) { pool.select(std::uint64_t{}); };
template <typename Pool>
concept unique_selection = requires(Pool const& pool) { pool.select_unique(1, std::uint64_t{}); };
template <typename Range>
concept pool_range = requires(Range&& range) { WeightedPool<int>::from_range(std::forward<Range>(range)); };

struct CopyOnly {
  int value = 0;
  CopyOnly() = default;
  explicit CopyOnly(int input) : value(input) {}
  explicit CopyOnly(CopyOnly const& other) : value(other.value) {}
  CopyOnly(CopyOnly&&) = delete;
  CopyOnly& operator=(CopyOnly const&) = delete;
};

struct CopyFallback {
  int value;
  explicit CopyFallback(int input) : value(input) {}
  explicit CopyFallback(CopyFallback const& other) : value(other.value) {}
  CopyFallback& operator=(CopyFallback const&) = delete;
};

static_assert(!temporary_item_borrow<WeightedPool<int>>);
static_assert(!temporary_item_borrow<WeightedPool<int> const>);
static_assert(!temporary_entry_borrow<WeightedPool<int>>);
static_assert(!temporary_entry_borrow<WeightedPool<int> const>);
static_assert(!temporary_items_borrow<WeightedPool<int>>);
static_assert(!temporary_items_borrow<WeightedPool<int> const>);
static_assert(!owning_selection<WeightedPool<std::unique_ptr<int>>>);
static_assert(owning_selection<WeightedPool<CopyOnly>>);
static_assert(!unique_selection<WeightedPool<CopyOnly>>);
static_assert(!owning_selection<WeightedPool<CopyFallback>>);
static_assert(unique_selection<WeightedPool<CopyFallback>>);
static_assert(!pool_range<std::vector<int>>);
static_assert(pool_range<std::vector<std::pair<double, int>>&>);
}

TEST(WeightedPoolTest, InvalidWeightsPreserveEntriesAndUnconsumedOwners) {
  WeightedPool<std::unique_ptr<int>> pool;
  ASSERT_TRUE(pool.insert(1.0, std::make_unique<int>(19)));
  for (auto bits : {0x7ff0000000000000ull, 0xfff0000000000000ull,
                    0x7ff8000000000001ull, 0xfff8000000000001ull, 0x7ff0000000000001ull}) {
    volatile std::uint64_t stored = bits;
    double weight = std::bit_cast<double>(static_cast<std::uint64_t>(stored));
    auto value = std::make_unique<int>(42);
    auto inserted = pool.insert(weight, std::move(value));
    ASSERT_FALSE(inserted);
    EXPECT_TRUE(inserted.error().is<WeightedPoolException>());
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 42);
    ASSERT_EQ(pool.size(), 1u);
    auto original = pool.item(0);
    ASSERT_TRUE(original);
    EXPECT_EQ(*original->get(), 19);
    auto original_weight = pool.weight(0);
    ASSERT_TRUE(original_weight);
    EXPECT_EQ(*original_weight, 1.0);
  }
  for (double weight : {0.0, -0.0, -2.0}) {
    auto ignored = std::make_unique<int>(53);
    ASSERT_TRUE(pool.insert(weight, std::move(ignored)));
    ASSERT_TRUE(ignored);
    EXPECT_EQ(*ignored, 53);
    EXPECT_EQ(pool.size(), 1u);
  }
}

TEST(WeightedPoolTest, OverflowRejectionLeavesThePoolUsable) {
  WeightedPool<int> pool;
  ASSERT_TRUE(pool.insert(std::numeric_limits<double>::max(), 7));
  auto rejected = pool.insert(std::numeric_limits<double>::max(), 9);
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<WeightedPoolException>());
  ASSERT_EQ(pool.size(), 1u);
  EXPECT_EQ(pool.select(std::uint64_t{81}), 7);
  pool.clear();
  ASSERT_TRUE(pool.insert(1.0, 11));
  ASSERT_TRUE(pool.insert(1.0, 13));
  RandomSource random(17), reference(17);
  for (unsigned i = 0; i < 128; ++i)
    EXPECT_EQ(pool.select(random), reference.uniform_double() <= 0.5 ? 11 : 13);
}

TEST(WeightedPoolTest, MoveConstructionAssignmentAndSelfMovePreserveReusableState) {
  WeightedPool<int> source;
  ASSERT_TRUE(source.insert(3.0, 21));
  ASSERT_TRUE(source.insert(5.0, 34));
  WeightedPool<int> moved(std::move(source));
  ASSERT_EQ(moved.size(), 2u);
  ASSERT_TRUE(source.insert(1.0, 11));
  ASSERT_TRUE(source.insert(1.0, 13));
  WeightedPool<int> assigned;
  ASSERT_TRUE(assigned.insert(70.0, -1));
  assigned = std::move(moved);
  ASSERT_TRUE(moved.insert(1.0, 11));
  ASSERT_TRUE(moved.insert(1.0, 13));
  auto& alias = assigned;
  assigned = std::move(alias);
  for (std::uint64_t seed = 0; seed < 128; ++seed) {
    EXPECT_EQ(source.select(seed), static_random_double(seed) <= 0.5 ? 11 : 13);
    EXPECT_EQ(moved.select(seed), static_random_double(seed) <= 0.5 ? 11 : 13);
    EXPECT_EQ(assigned.select(seed), static_random_double(seed) <= 0.375 ? 21 : 34);
  }
}

TEST(WeightedPoolTest, EmptyAndZeroCountOperationsPreserveRandomStreamContracts) {
  WeightedPool<int> empty;
  RandomSource source(71), expected(71);
  EXPECT_EQ(empty.select(source), 0);
  EXPECT_EQ(source.next_u64(), expected.next_u64());
  EXPECT_EQ(empty.select_index(source), NPos);
  (void)expected.uniform_double();
  EXPECT_EQ(source.next_u64(), expected.next_u64());
  RandomService service(81), service_expected(81);
  EXPECT_EQ(empty.select(service), 0);
  EXPECT_EQ(service.next_u64(), service_expected.next_u64());
  EXPECT_EQ(empty.select_index(service), NPos);
  (void)service_expected.uniform_double();
  EXPECT_EQ(service.next_u64(), service_expected.next_u64());
  EXPECT_TRUE(empty.select_unique(0, service).empty());
  (void)service_expected.next_u64();
  EXPECT_EQ(service.next_u64(), service_expected.next_u64());
  EXPECT_FALSE(empty.at(0));
  EXPECT_FALSE(empty.item(0));
  EXPECT_FALSE(empty.weight(0));
}

TEST(WeightedPoolTest, UniqueSelectionIsBoundedSortedAndRetainsDistinctIndices) {
  WeightedPool<int> skew;
  ASSERT_TRUE(skew.insert(1.0, 0));
  ASSERT_TRUE(skew.insert(1e-20, 1));
  ASSERT_TRUE(skew.insert(1e-20, 2));
  for (std::uint64_t seed = 0; seed < 64; ++seed) {
    auto partial = skew.select_unique(2, seed);
    ASSERT_EQ(partial.size(), 2u);
    EXPECT_EQ(partial[0], 0);
    EXPECT_TRUE(partial[1] == 1 || partial[1] == 2);
    EXPECT_EQ(partial, skew.select_unique(2, seed));
    EXPECT_EQ(skew.select_unique(20, seed), (List<int>{0, 1, 2}));
  }
  WeightedPool<int> duplicates;
  ASSERT_TRUE(duplicates.insert(1.0, 7));
  ASSERT_TRUE(duplicates.insert(3.0, 7));
  EXPECT_EQ(duplicates.select_unique(2, std::uint64_t{19}), (List<int>{7, 7}));
  WeightedPool<int> ordinary;
  for (int i = 0; i < 12; ++i)
    ASSERT_TRUE(ordinary.insert(i + 1.0, i));
  for (std::uint64_t seed = 0; seed < 64; ++seed) {
    auto values = ordinary.select_unique(6, seed);
    ASSERT_EQ(values.size(), 6u);
    for (std::size_t i = 0; i < values.size(); ++i) {
      EXPECT_GE(values[i], 0);
      EXPECT_LT(values[i], 12);
      if (i)
        EXPECT_LT(values[i - 1], values[i]);
    }
  }
}

TEST(WeightedPoolTest, UniqueSelectionHandlesExtremeWeightsAndUniformScaling) {
  WeightedPool<int> pool;
  ASSERT_TRUE(pool.insert(std::numeric_limits<double>::max(), 0));
  for (int i = 1; i <= 32; ++i)
    ASSERT_TRUE(pool.insert(1e291, i));
  for (std::uint64_t seed = 0; seed < 64; ++seed) {
    auto selected = pool.select_unique(3, seed);
    ASSERT_EQ(selected.size(), 3u);
    EXPECT_EQ(selected[0], 0);
    EXPECT_GT(selected[1], 0);
    EXPECT_LT(selected[1], selected[2]);
    EXPECT_LE(selected[2], 32);
  }
  WeightedPool<int> reference;
  ASSERT_TRUE(reference.insert(1.0, 0));
  ASSERT_TRUE(reference.insert(2.0, 1));
  ASSERT_TRUE(reference.insert(1.0, 2));
  // Uniform power-of-two scaling must retain the relative-weight selection.
  for (int exponent : {std::numeric_limits<double>::min_exponent - 1,
                       std::numeric_limits<double>::max_exponent - 3}) {
    double scale = std::scalbn(1.0, exponent);
    WeightedPool<int> scaled;
    ASSERT_TRUE(scaled.insert(scale, 0));
    ASSERT_TRUE(scaled.insert(2.0 * scale, 1));
    ASSERT_TRUE(scaled.insert(scale, 2));
    for (std::uint64_t seed = 0; seed < 64; ++seed)
      EXPECT_EQ(scaled.select_unique(2, seed), reference.select_unique(2, seed));
  }
}

TEST(WeightedPoolTest, RangeConstructionSupportsSinglePassAndStopsAtFirstError) {
  std::istringstream input("1 0 3");
  auto values = std::views::istream<int>(input)
    | std::views::transform([](int value) { return std::pair{static_cast<double>(value), value * 10}; });
  auto pool = WeightedPool<int>::from_range(values);
  ASSERT_TRUE(pool);
  ASSERT_EQ(pool->size(), 2u);
  auto first = pool->at(0), second = pool->at(1);
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_EQ(first->get(), (std::pair{1.0, 10}));
  EXPECT_EQ(second->get(), (std::pair{3.0, 30}));
  std::istringstream failing("1 2 3");
  // Keep the malformed input as runtime integer bits: finite-math compilation
  // can fold a conditional infinity literal to the finite branch before validation.
  volatile std::uint64_t stored_infinity = 0x7ff0000000000000ull;
  std::uint64_t infinity_bits = stored_infinity;
  auto bad_values = std::views::istream<int>(failing)
    | std::views::transform([infinity_bits](int value) {
        auto bits = value == 2 ? infinity_bits : std::uint64_t{0x3ff0000000000000ull};
        return std::pair{std::bit_cast<double>(bits), value};
      });
  auto rejected = WeightedPool<int>::from_range(bad_values);
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<WeightedPoolException>());
  int next = 0;
  ASSERT_TRUE(static_cast<bool>(failing >> next));
  EXPECT_EQ(next, 3);
}

TEST(WeightedPoolTest, RangeTransferAndExplicitCopyConstructionRetainCapabilities) {
  std::vector<std::pair<double, std::unique_ptr<int>>> values;
  values.emplace_back(1.0, std::make_unique<int>(17));
  values.emplace_back(0.0, std::make_unique<int>(99));
  values.emplace_back(2.0, std::make_unique<int>(23));
  auto pool = WeightedPool<std::unique_ptr<int>>::from_range(std::move(values));
  ASSERT_TRUE(pool);
  EXPECT_FALSE(values[0].second);
  ASSERT_TRUE(values[1].second);
  EXPECT_EQ(*values[1].second, 99);
  EXPECT_FALSE(values[2].second);
  auto first = pool->item(0), second = pool->item(1);
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_EQ(*first->get(), 17);
  EXPECT_EQ(*second->get(), 23);
  WeightedPool<CopyOnly> copied;
  CopyOnly source(31);
  ASSERT_TRUE(copied.insert(1.0, source));
  EXPECT_EQ(copied.select(std::uint64_t{1}).value, 31);
  auto independent = copied;
  WeightedPool<CopyOnly> assigned;
  assigned = copied;
  copied.clear();
  EXPECT_EQ(independent.select(std::uint64_t{1}).value, 31);
  EXPECT_EQ(assigned.select(std::uint64_t{1}).value, 31);
  WeightedPool<CopyFallback> unique;
  CopyFallback first_copy(37), second_copy(41);
  ASSERT_TRUE(unique.insert(1.0, first_copy));
  ASSERT_TRUE(unique.insert(2.0, second_copy));
  auto selected = unique.select_unique(2, std::uint64_t{1});
  ASSERT_EQ(selected.size(), 2u);
  EXPECT_EQ(selected[0].value, 37);
  EXPECT_EQ(selected[1].value, 41);
}

TEST(WeightedPoolTest, JsonRoundTripPreservesWeightsOrderAndFirstConversionError) {
  Json input(JsonArray{JsonArray{2.0, 17}, JsonObject{{"weight", 3.0}, {"item", 23}}});
  auto pool = json_to_weighted_pool<int>(input);
  ASSERT_TRUE(pool);
  auto encoded = json_from_weighted_pool(*pool);
  ASSERT_TRUE(encoded);
  EXPECT_EQ(*encoded, (Json(JsonArray{JsonObject{{"weight", 2.0}, {"item", 17}},
                                    JsonObject{{"weight", 3.0}, {"item", 23}}})));
  unsigned converted = 0;
  Json invalid(JsonArray{JsonArray{1.0, 17}, JsonArray{-1.0, "not an integer"}, JsonArray{1.0, 23}});
  auto rejected = json_to_weighted_pool<int>(invalid, [&converted](Json const& value) {
    ++converted;
    return value.toInt();
  });
  EXPECT_FALSE(rejected);
  EXPECT_EQ(converted, 2u);
}
