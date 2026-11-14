import std;
import star.format;
import star.random;
import star.random_point;
import star.static_random;
import star.poly;
import star.rect;
import star.vector;

#include "gtest/gtest.h"

using namespace Star;

namespace {
  struct PointValue {
    explicit PointValue(int input) : value(input) {}
    int value;
  };

  struct ValueCallback {
    int operator()(RandomSource&) const;
  };
  struct CheckedCallback {
    std::expected<int, StarException> operator()(RandomSource&) const;
  };
  struct InvalidCallback {
    void operator()(RandomSource&) const;
  };
  struct EmptyCheckedCallback {
    std::expected<void, StarException> operator()(RandomSource&) const;
  };

  struct ImmobileCallback {
    ImmobileCallback() = default;
    ImmobileCallback(ImmobileCallback const&) = delete;
    int operator()(RandomSource&) { ++calls; return 13; }
    int calls = 0;
  };

  template <typename Float, typename Bits>
  [[gnu::noinline]] Float runtime_float(Bits bits) {
    volatile Bits runtimeBits = bits;
    return std::bit_cast<Float>(static_cast<Bits>(runtimeBits));
  }

  static_assert(point_callback<ValueCallback, PointValue>);
  static_assert(point_callback<CheckedCallback, PointValue>);
  static_assert(!point_callback<InvalidCallback, PointValue>);
  static_assert(!point_callback<EmptyCheckedCallback, PointValue>);
}

TEST(RandomPointTest, RejectsInvalidParametersBeforeDrawsOrCallbacks) {
  RandomService service(17);
  RandomService untouched(17);
  int calls = 0;
  auto callback = [&](RandomSource&) { ++calls; return 7; };
  PolyD area(RectD({0.25, 0.25}, {0.75, 0.75}));
  for (float cellSize : {0.0f, -1.0f, runtime_float<float>(0x7f800000u), runtime_float<float>(0x7fc00000u)}) {
    Random2dPointGenerator<int, double> generator(service, 29, cellSize, {1, 1});
    EXPECT_FALSE(generator.generate(area, callback));
  }
  for (Vec2I density : {Vec2I(-1, 1), Vec2I(2, 1), Vec2I(-2, -1)}) {
    Random2dPointGenerator<int, double> generator(service, 29, 1, density);
    EXPECT_FALSE(generator.generate(area, callback));
  }
  Random2dPointGenerator<int, double> generator(service, 29, 1, {1, 1});
  for (double coordinate : {runtime_float<double>(std::uint64_t{0x7ff0000000000000}), runtime_float<double>(std::uint64_t{0x7ff8000000000000}), 0x1p63, -0x1p64}) {
    for (auto vertex : {Vec2D(coordinate, 0.25), Vec2D(0.25, coordinate)}) {
      PolyD invalid = {vertex, {0.75, 0.25}, {0.75, 0.75}};
      EXPECT_FALSE(generator.generate(invalid, callback));
    }
  }
  // Each dimension fits int64, but the sector product cannot fit size_t.
  EXPECT_FALSE(generator.generate(PolyD(RectD({0, 0}, {0x1p32, 0x1p32})), callback));
  Random2dPointGenerator<int, double> dense(service, 29, 1, {0, std::numeric_limits<int>::max()});
  EXPECT_FALSE(dense.generate(PolyD(RectD({0, 0}, {1e9, 1e9})), callback));
  Random2dPointGenerator<int> overflowing(service, 29, std::numeric_limits<float>::max(), {1, 1});
  auto large = std::numeric_limits<float>::max() / 4;
  EXPECT_FALSE(overflowing.generate(PolyF(RectF({large, large}, {large * 2, large * 2})), callback));
  EXPECT_EQ(calls, 0);
  EXPECT_EQ(service.next_u64(), untouched.next_u64());

  auto empty = generator.generate(PolyD{}, callback);
  ASSERT_TRUE(empty);
  EXPECT_TRUE(empty->empty());
  auto recovered = generator.generate(area, callback);
  ASSERT_TRUE(recovered);
  ASSERT_EQ(recovered->size(), 1u);
  EXPECT_EQ(recovered->front().second, 7);
  EXPECT_EQ(calls, 1);
}

TEST(RandomPointTest, PreservesCompleteCellsSeedDrawsDataAndOrder) {
  RandomService service(17);
  constexpr std::uint64_t seed = 29;
  Random2dPointGenerator<std::uint32_t> generator(service, seed, 1, {2, 2});
  int calls = 0;
  auto callback = [&](RandomSource& random) { ++calls; return random.next_u32(); };
  PolyF area(RectF({0.25f, 0.25f}, {1.75f, 1.75f}));
  auto result = generator.generate(area, callback);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->size(), 8u);
  EXPECT_EQ(calls, 8);
  std::size_t index = 0;
  for (std::int64_t x = 0; x < 2; ++x) {
    for (std::int64_t y = 0; y < 2; ++y) {
      RandomSource random(static_random_u64(seed, x, y));
      auto count = random.uniform_int(2, 2);
      ASSERT_TRUE(count);
      ASSERT_EQ(*count, 2);
      for (int i = 0; i < 2; ++i) {
        Vec2F expected(x + static_cast<float>(random.uniform_double()), y + static_cast<float>(random.uniform_double()));
        EXPECT_EQ((*result)[index].first, expected);
        EXPECT_EQ((*result)[index].second, random.next_u32());
        ++index;
      }
    }
  }
  auto repeated = generator.generate(area, callback);
  ASSERT_TRUE(repeated);
  EXPECT_EQ(*repeated, *result);
  EXPECT_EQ(calls, 8);

  // A tiny subregion still returns both complete cached points of its cell.
  auto subset = generator.generate(PolyF(RectF({0.49f, 0.49f}, {0.51f, 0.51f})), callback);
  ASSERT_TRUE(subset);
  ASSERT_EQ(subset->size(), 2u);
  EXPECT_EQ((*subset)[0], (*result)[0]);
  EXPECT_EQ((*subset)[1], (*result)[1]);
  EXPECT_EQ(calls, 8);
}

TEST(RandomPointTest, DoubleSectorGeometryDoesNotNarrowThroughFloat) {
  RandomService service(17);
  constexpr std::uint64_t seed = 29;
  Random2dPointGenerator<std::uint64_t, double> generator(service, seed, 1, {1, 1});
  int calls = 0;
  auto callback = [&](RandomSource& random) { ++calls; return random.next_u64(); };
  for (std::int64_t x : {std::int64_t{16777216}, std::int64_t{16777217}}) {
    PolyD area(RectD({x + 0.25, 0.25}, {x + 0.75, 0.75}));
    auto result = generator.generate(area, callback);
    ASSERT_TRUE(result);
    ASSERT_EQ(result->size(), 1u);
    RandomSource random(static_random_u64(seed, x, std::int64_t{0}));
    ASSERT_TRUE(random.uniform_int(1, 1));
    Vec2D expected(x + random.uniform_double(), random.uniform_double());
    EXPECT_EQ(result->front().first, expected);
    EXPECT_EQ(result->front().second, random.next_u64());
    auto repeated = generator.generate(area, callback);
    ASSERT_TRUE(repeated);
    EXPECT_EQ(*repeated, *result);
  }
  EXPECT_EQ(calls, 2);
}

TEST(RandomPointTest, CheckedCallbackFailureDoesNotPoisonCell) {
  RandomService service(17);
  Random2dPointGenerator<PointValue> generator(service, 29, 1, {1, 1});
  PolyF area(RectF({0.25f, 0.25f}, {0.75f, 0.75f}));
  auto failed = generator.generate(area, [](RandomSource&) -> std::expected<int, StarException> {
    return std::unexpected(StarException("point callback failure"));
  });
  ASSERT_FALSE(failed);
  EXPECT_NE(std::string_view(failed.error().what()).find("point callback failure"), std::string_view::npos);
  auto recovered = generator.generate(area, [](RandomSource&) -> std::expected<int, StarException> { return 41; });
  ASSERT_TRUE(recovered);
  ASSERT_EQ(recovered->size(), 1u);
  EXPECT_EQ(recovered->front().second.value, 41);
  auto cached = generator.generate(area, [](RandomSource&) { return 99; });
  ASSERT_TRUE(cached);
  ASSERT_EQ(cached->size(), 1u);
  EXPECT_EQ(cached->front().second.value, 41);
}

TEST(RandomPointTest, AcceptsImmobileLvalueCallbackWithoutCopying) {
  RandomService service(17);
  Random2dPointGenerator<int> generator(service, 29, 1, {1, 1});
  ImmobileCallback callback;
  PolyF area(RectF({0.25f, 0.25f}, {0.75f, 0.75f}));
  auto result = generator.generate(area, callback);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->front().second, 13);
  EXPECT_EQ(callback.calls, 1);
  auto repeated = generator.generate(area, callback);
  ASSERT_TRUE(repeated);
  EXPECT_EQ(*repeated, *result);
  EXPECT_EQ(callback.calls, 1);
}

TEST(RandomPointTest, IntegerKeysSeparateCellsThatShareFloatCoordinates) {
  RandomService service(17);
  constexpr std::uint64_t seed = 29;
  Random2dPointGenerator<std::uint64_t> generator(service, seed, 1, {1, 1});
  int calls = 0;
  auto callback = [&](RandomSource& random) { ++calls; return random.next_u64(); };
  // -16777217 and -16777216 map to the same float, and both cells
  // intersect this area. Their callback data must still come from distinct
  // integer-sector seeds rather than sharing one float-keyed cache entry.
  PolyF area(RectF({-16777218.0f, 0.25f}, {-16777215.0f, 0.75f}));
  auto result = generator.generate(area, callback);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->size(), 3u);
  EXPECT_EQ(calls, 3);
  std::size_t index = 0;
  for (std::int64_t x = -16777218; x <= -16777216; ++x) {
    RandomSource random(static_random_u64(seed, x, std::int64_t{0}));
    ASSERT_TRUE(random.uniform_int(1, 1));
    Vec2F expected(x + static_cast<float>(random.uniform_double()), static_cast<float>(random.uniform_double()));
    EXPECT_EQ((*result)[index].first, expected);
    EXPECT_EQ((*result)[index].second, random.next_u64());
    ++index;
  }
  auto repeated = generator.generate(area, callback);
  ASSERT_TRUE(repeated);
  EXPECT_EQ(*repeated, *result);
  EXPECT_EQ(calls, 3);
}
