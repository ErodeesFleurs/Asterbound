import std;
import star.string;
import star.thread;
import star.time;

#include "gtest/gtest.h"

using namespace Star;

TEST(ClockTest, StopIncludesUnreadInterval) {
  Clock clock(false);
  clock.start();
  clock.start();
  auto before = std::chrono::steady_clock::now();
  Thread::sleep_precise(2);
  auto lower_bound = std::chrono::steady_clock::now() - before;
  clock.stop();
  EXPECT_GE(clock.time(), std::chrono::duration<double>(lower_bound).count());
  auto frozen = clock.time();
  clock.stop();
  EXPECT_EQ(clock.time(), frozen);
}

TEST(ClockTest, CrossAssignmentAndRunningReset) {
  Clock left(false), right(false);
  left.set_milliseconds(75);
  right = left;
  {
    std::jthread assign_left([&] { for (int i = 0; i < 2000; ++i) left = right; });
    for (int i = 0; i < 2000; ++i) right = left;
  }
  EXPECT_EQ(left.milliseconds(), 75);
  EXPECT_EQ(right.milliseconds(), 75);
  right.start();
  left = right;
  right.reset();
  EXPECT_TRUE(right.running());
  auto reset_time = right.time();
  EXPECT_GE(left.time() - reset_time, 0.075);
}

TEST(ClockTest, FormatCarriesAcrossUnits) {
  EXPECT_EQ(Time::print_duration(1.9996), String("2 seconds, 0 milliseconds"));
  EXPECT_EQ(Time::print_duration(-3599.9999), String("-1 hour, 0 minutes, 0 seconds, 0 milliseconds"));
  EXPECT_EQ(Time::print_duration(-0.0001), String("0 milliseconds"));
  EXPECT_EQ(Time::print_duration(86400), String("24 hours, 0 minutes, 0 seconds, 0 milliseconds"));
  volatile std::uint64_t nan_bits = 0x7ff8000000000001ULL;
  EXPECT_EQ(Time::print_duration(std::bit_cast<double>(std::uint64_t(nan_bits))), String("duration out of range"));
}

TEST(ClockTest, All) {
  Clock clock;

  Thread::sleep_precise(20);

  // Only require progression; scheduler delays need not have an upper bound.
  EXPECT_GT(clock.time(), 0.0);

  clock.stop();
  double time = clock.time();
  Thread::sleep_precise(20);
  EXPECT_EQ(clock.time(), time);

  clock.reset();
  EXPECT_EQ(clock.time(), 0.0);

  Timer nullTimer;
  EXPECT_TRUE(nullTimer.time_up());
  EXPECT_FALSE(nullTimer.running());
}

TEST(ClockTest, StoppedClockPreservesSignedRoundingAndAdjustments) {
  Clock clock(false);
  EXPECT_FALSE(clock.running());

  clock.set_time(1.2345);
  EXPECT_DOUBLE_EQ(clock.time(), 1.2345);
  EXPECT_EQ(clock.milliseconds(), 1235);

  clock.set_milliseconds(-1500);
  EXPECT_DOUBLE_EQ(clock.time(), -1.5);
  // The numeric API adds half a millisecond before truncating, even for
  // negative elapsed times. Countdown timers depend on this same conversion.
  EXPECT_EQ(clock.milliseconds(), -1499);

  clock.adjust_time(2.0);
  EXPECT_DOUBLE_EQ(clock.time(), 0.5);
  clock.adjust_milliseconds(250);
  EXPECT_DOUBLE_EQ(clock.time(), 0.75);
  clock.adjust_time(-2.0);
  EXPECT_DOUBLE_EQ(clock.time(), 0.0);

  Clock copy(clock);
  copy.set_time(2.0);
  clock = copy;
  EXPECT_FALSE(clock.running());
  EXPECT_DOUBLE_EQ(clock.time(), 2.0);
  clock.reset();
  EXPECT_DOUBLE_EQ(clock.time(), 0.0);
}

TEST(ClockTest, StoppedCountdownPreservesClippingAndSignedRounding) {
  auto timer = Timer::with_milliseconds(1500, false);
  EXPECT_FALSE(timer.running());
  EXPECT_FALSE(timer.time_up());
  EXPECT_DOUBLE_EQ(timer.time_left(), 1.5);
  EXPECT_EQ(timer.milliseconds_left(), 1499);

  auto expired = Timer::with_time(-1.5, false);
  EXPECT_TRUE(expired.time_up());
  EXPECT_DOUBLE_EQ(expired.time_left(), 0.0);
  EXPECT_DOUBLE_EQ(expired.time_left(true), -1.5);
  EXPECT_EQ(expired.milliseconds_left(), 0);
  EXPECT_EQ(expired.milliseconds_left(true), -1500);
}

TEST(ClockTest, LocalCalendarPreservesSubsecondsAcrossSecondBoundary) {
  auto base = std::chrono::sys_days(std::chrono::year(2024) / 1 / 15) + std::chrono::hours(12);
  auto whole = Time::local_time(base);
  ASSERT_TRUE(whole) << whole.error().what();
  auto before = Time::local_time(base + std::chrono::milliseconds(999));
  ASSERT_TRUE(before) << before.error().what();
  auto after = Time::local_time(base + std::chrono::milliseconds(1001));
  ASSERT_TRUE(after) << after.error().what();
  EXPECT_EQ(*before - *whole, std::chrono::milliseconds(999));
  EXPECT_EQ(*after - *before, std::chrono::milliseconds(2));
}
