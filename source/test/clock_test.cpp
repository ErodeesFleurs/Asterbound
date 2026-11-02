import std;
import star.thread;
import star.time;

#include "gtest/gtest.h"

using namespace Star;

TEST(ClockTest, All) {
  Clock clock;

  Thread::sleepPrecise(20);

  // Only require progression; scheduler delays need not have an upper bound.
  EXPECT_GT(clock.time(), 0.0);

  clock.stop();
  double time = clock.time();
  Thread::sleepPrecise(20);
  EXPECT_EQ(clock.time(), time);

  clock.reset();
  EXPECT_EQ(clock.time(), 0.0);

  Timer nullTimer;
  EXPECT_TRUE(nullTimer.timeUp());
  EXPECT_FALSE(nullTimer.running());
}

TEST(ClockTest, StoppedClockPreservesSignedRoundingAndAdjustments) {
  Clock clock(false);
  EXPECT_FALSE(clock.running());

  clock.setTime(1.2345);
  EXPECT_DOUBLE_EQ(clock.time(), 1.2345);
  EXPECT_EQ(clock.milliseconds(), 1235);

  clock.setMilliseconds(-1500);
  EXPECT_DOUBLE_EQ(clock.time(), -1.5);
  // The numeric API adds half a millisecond before truncating, even for
  // negative elapsed times. Countdown timers depend on this same conversion.
  EXPECT_EQ(clock.milliseconds(), -1499);

  clock.adjustTime(2.0);
  EXPECT_DOUBLE_EQ(clock.time(), 0.5);
  clock.adjustMilliseconds(250);
  EXPECT_DOUBLE_EQ(clock.time(), 0.75);
  clock.adjustTime(-2.0);
  EXPECT_DOUBLE_EQ(clock.time(), 0.0);

  Clock copy(clock);
  copy.setTime(2.0);
  clock = copy;
  EXPECT_FALSE(clock.running());
  EXPECT_DOUBLE_EQ(clock.time(), 2.0);
  clock.reset();
  EXPECT_DOUBLE_EQ(clock.time(), 0.0);
}

TEST(ClockTest, StoppedCountdownPreservesClippingAndSignedRounding) {
  auto timer = Timer::withMilliseconds(1500, false);
  EXPECT_FALSE(timer.running());
  EXPECT_FALSE(timer.timeUp());
  EXPECT_DOUBLE_EQ(timer.timeLeft(), 1.5);
  EXPECT_EQ(timer.millisecondsLeft(), 1499);

  auto expired = Timer::withTime(-1.5, false);
  EXPECT_TRUE(expired.timeUp());
  EXPECT_DOUBLE_EQ(expired.timeLeft(), 0.0);
  EXPECT_DOUBLE_EQ(expired.timeLeft(true), -1.5);
  EXPECT_EQ(expired.millisecondsLeft(), 0);
  EXPECT_EQ(expired.millisecondsLeft(true), -1500);
}

TEST(ClockTest, NumericTickConversionsPreserveUnitsAndRounding) {
  constexpr std::int64_t frequency = 1'000'000'000;
  EXPECT_DOUBLE_EQ(Time::ticksToSeconds(1'500'000'000, frequency), 1.5);
  EXPECT_EQ(Time::ticksToMilliseconds(1'234'500'000, frequency), 1235);
  EXPECT_EQ(Time::ticksToMilliseconds(-1'234'500'000, frequency), -1234);
  EXPECT_EQ(Time::ticksToMicroseconds(1'234'500, frequency), 1235);
  EXPECT_EQ(Time::ticksToMicroseconds(-1'234'500, frequency), -1234);
  EXPECT_EQ(Time::secondsToTicks(1.25, frequency), 1'250'000'000);
  EXPECT_EQ(Time::secondsToTicks(-1.25, frequency), -1'250'000'000);
  EXPECT_EQ(Time::millisecondsToTicks(1250, frequency), 1'250'000'000);
  EXPECT_EQ(Time::microsecondsToTicks(-1250, frequency), -1'250'000);
}
