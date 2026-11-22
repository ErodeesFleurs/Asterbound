import std;
import star.tick_rate_monitor;
import star.periodic;
import star.periodic_function;
import star.random;

#include "gtest/gtest.h"

using namespace Star;

TEST(TickRateTest, QueriesDoNotCommitDecay) {
  using namespace std::chrono_literals;
  auto start = std::chrono::steady_clock::time_point{};
  TickRateMonitor monitor(1);
  EXPECT_EQ(monitor.tick(3, start), 3);
  EXPECT_EQ(monitor.tick(7, start), 10);
  EXPECT_EQ(monitor.rate(start + 250ms), 7.5);
  EXPECT_EQ(monitor.rate(start + 500ms), 5);
  EXPECT_EQ(monitor.rate(start + 2s), 0);
}

TEST(TickRateTest, FractionalResetAndColdStart) {
  TickRateApproacher monitor(0.015625, 32);
  EXPECT_EQ(monitor.rate(), 0);
  monitor.reset();
  EXPECT_GT(monitor.rate(), 0);
  EXPECT_DOUBLE_EQ(monitor.spare_time(0.03125), 32);
  volatile std::uint64_t nan_bits = 0x7ff8000000000001ULL;
  EXPECT_FALSE(valid_tick_interval(std::bit_cast<double>(std::uint64_t(nan_bits))));
  EXPECT_FALSE(valid_tick_interval(-0.0));
}

TEST(PeriodicTest, All) {
  Periodic periodic(2);

  EXPECT_TRUE(periodic.ready());
  EXPECT_TRUE(periodic.tick());
  EXPECT_FALSE(periodic.ready());
  EXPECT_FALSE(periodic.tick());
  EXPECT_TRUE(periodic.ready());
  EXPECT_TRUE(periodic.tick());

  periodic = Periodic(0);
  EXPECT_FALSE(periodic.tick());
  EXPECT_FALSE(periodic.tick());

  periodic = Periodic(3);
  EXPECT_TRUE(periodic.ready());
  EXPECT_TRUE(periodic.tick());
  EXPECT_FALSE(periodic.ready());
  EXPECT_FALSE(periodic.tick());
  EXPECT_FALSE(periodic.tick());
  EXPECT_TRUE(periodic.ready());
  EXPECT_TRUE(periodic.tick());
  EXPECT_FALSE(periodic.ready());
}

TEST(PeriodicTest, ChangingStepCount) {
  Periodic periodic = 5;
  EXPECT_TRUE(periodic.tick());
  periodic.set_step_count(3); // Clamp the four pending skips to two.
  EXPECT_FALSE(periodic.tick());
  periodic.set_step_count(6); // Keep the single remaining skip.
  EXPECT_EQ(periodic.step_count(), 6u);
  EXPECT_FALSE(periodic.tick());
  EXPECT_TRUE(periodic.tick());
  periodic.set_step_count(0);
  EXPECT_FALSE(periodic.ready());
  EXPECT_FALSE(periodic.tick());
  periodic.set_step_count(2);
  EXPECT_TRUE(periodic.ready());
  EXPECT_TRUE(periodic.tick());
  periodic.set_step_count(1);
  EXPECT_TRUE(periodic.tick());
  EXPECT_TRUE(periodic.tick());
}

namespace {
struct LvalueCallback {
  unsigned& calls;
  constexpr void operator()() & noexcept { ++calls; }
};

struct RvalueCallback {
  void operator()() && {}
};

template <typename Callback>
concept tick_callback = requires(Periodic periodic, Callback&& callback) {
  periodic.tick(std::forward<Callback>(callback));
};

static_assert(!tick_callback<RvalueCallback>);
static_assert(noexcept(std::declval<Periodic&>().tick(std::declval<LvalueCallback>())));
static_assert(!noexcept(std::declval<Periodic&>().tick([] {})));
}

TEST(PeriodicTest, CallbackRunsAsLvalueOnlyOnDueTicks) {
  constexpr auto count_calls = [] {
    unsigned calls = 0;
    Periodic periodic(2);
    periodic.tick(LvalueCallback{calls});
    periodic.tick(LvalueCallback{calls});
    LvalueCallback callback{calls};
    periodic.tick(callback);
    periodic.set_step_count(0);
    periodic.tick(callback);
    return calls;
  };
  static_assert(count_calls() == 2);
  EXPECT_EQ(count_calls(), 2u);
}

TEST(PeriodicFunctionTest, LargeDeltaAdvancesOneHalfCycle) {
  RandomService random(157), reference(157);
  auto wave = PeriodicFunction<double>::create(random, 2.0, 1.0, 4.0, 0.0, 0.25);
  ASSERT_TRUE(wave);
  auto weights = [owner = std::make_unique<int>(1)](double t) {
    return std::array{*owner - t, t};
  };
  auto offset = [&] { return (reference.uniform_float() * 2 - 1) * 0.25; };
  (void)offset(); // Initial source is the upper endpoint.
  EXPECT_DOUBLE_EQ(wave->value(weights), 1.0 + offset());
  wave->update(20.0);
  double upper = 4.0 + offset();
  (void)offset(); // Period draw is consumed even with zero period variance.
  EXPECT_DOUBLE_EQ(wave->value(weights), upper);
  wave->update(0.0);
  double lower = 1.0 + offset();
  (void)offset();
  EXPECT_DOUBLE_EQ(wave->value(weights), upper);
  wave->update(0.5);
  EXPECT_DOUBLE_EQ(wave->value(weights), (lower + upper) * 0.5);
  EXPECT_EQ(random.next_u32(), reference.next_u32());
}

TEST(PeriodicFunctionTest, ZeroLengthHalfCycleReachesTargetWithoutResampling) {
  // With this seed the fourth draw (first sampled duration) is exactly zero.
  RandomService random(1765881573), reference(1765881573);
  auto wave = PeriodicFunction<float>::create(random, 0.4f, 0.5f, 0.85f, 0.4f);
  ASSERT_TRUE(wave);
  for (int i = 0; i < 3; ++i)
    (void)reference.uniform_float();
  ASSERT_EQ(reference.uniform_float(), 0.0f);
  wave->update(0.0f);
  float phase = -1;
  auto weights = [&](float t) { phase = t; return std::array{1.0f - t, t}; };
  EXPECT_FLOAT_EQ(wave->value(weights), 0.85f);
  EXPECT_EQ(phase, 0.0f);
  EXPECT_EQ(random.next_u32(), reference.next_u32());
}

TEST(PeriodicFunctionTest, InvalidDurationDoesNotAdvanceRandomStream) {
  RandomService random(167), reference(167);
  using Wave = PeriodicFunction<float>;
  EXPECT_FALSE(Wave::create(random, 0.0f));
  EXPECT_FALSE(Wave::create(random, 1.0f, 0.0f, 1.0f, -0.25f));
  // Materialize the IEEE input at runtime; a fast-math call to infinity()
  // need not even store the value in the caller's argument slot.
  for (auto bits : {0x7f800000u, 0xff800000u, 0x7fc00001u}) {
    volatile std::uint32_t input = bits;
    auto period = std::bit_cast<float>(static_cast<std::uint32_t>(input));
    EXPECT_FALSE(Wave::create(random, period));
  }
  EXPECT_FALSE(Wave::create(random, std::numeric_limits<float>::denorm_min()));
  EXPECT_EQ(random.next_u32(), reference.next_u32());
}
