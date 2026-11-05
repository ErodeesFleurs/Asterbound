import std;
import star.fixed_scheduler;

#include "gtest/gtest.h"

using namespace Star;

namespace {
constexpr auto schedulerTime(double seconds) {
  return std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>>(std::chrono::duration<double>(seconds));
}
}

TEST(FixedSchedulerTest, HighRefreshCanRenderWithoutAnyLogicTick) {
  FixedScheduler scheduler(64.0, 256.0);
  scheduler.reset(schedulerTime(10.0));
  EXPECT_EQ(scheduler.epoch(), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(10.0)), 1.0f);
  for (unsigned frame = 0; frame != 4; ++frame) {
    double now = 10.0 + frame / 256.0;
    EXPECT_EQ(scheduler.updatesDue(schedulerTime(now), 5), 0u);
    EXPECT_TRUE(scheduler.beginRender(schedulerTime(now)));
    EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(now)), 1.0f);
  }
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(10.0 + 1.0 / 64.0), 5), 1u);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(10.0 + 1.0 / 64.0)));
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(10.0 + 1.0 / 64.0)), 0.0f);
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(10.0 + 5.0 / 256.0), 5), 0u);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(10.0 + 5.0 / 256.0)));
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(10.0 + 5.0 / 256.0)), 0.25f);
  EXPECT_EQ(scheduler.epoch(), 1u);
}

TEST(FixedSchedulerTest, SlowBlockingRenderCatchesUpWithoutAnotherSleep) {
  FixedScheduler scheduler(64.0, 32.0);
  scheduler.reset(schedulerTime(0.0));
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(0.0)));

  // The initial present took three logic intervals. Neither logic nor render
  // gets a fresh relative sleep budget after that blocking present.
  double now = 3.0 / 64.0;
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(schedulerTime(now)).count(), 0.0);
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(now), 5), 3u);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(now)));
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(now)), 0.0f);
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(schedulerTime(now)).count(), 1.0 / 64.0);

  now = 5.0 / 64.0;
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(schedulerTime(now)).count(), 0.0);
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(now), 5), 2u);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(now)));
  EXPECT_EQ(scheduler.epoch(), 1u);
}

TEST(FixedSchedulerTest, LowRenderCapDoesNotReduceLogicCadence) {
  FixedScheduler scheduler(64.0, 16.0);
  scheduler.reset(schedulerTime(0.0));
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(0.0)));
  for (unsigned tick = 1; tick <= 8; ++tick) {
    double now = tick / 64.0;
    EXPECT_EQ(scheduler.updatesDue(schedulerTime(now), 5), 1u);
    EXPECT_EQ(scheduler.beginRender(schedulerTime(now)), tick % 4 == 0);
  }
  EXPECT_EQ(scheduler.epoch(), 1u);
}

TEST(FixedSchedulerTest, BoundedCatchupDropsOnlyWholeOverdueTicks) {
  FixedScheduler scheduler(64.0, 0.0);
  scheduler.reset(schedulerTime(0.0));
  auto updateEpoch = scheduler.updateEpoch();
  double now = 4.5 / 64.0;
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(now), 0), 1u);
  EXPECT_EQ(scheduler.epoch(), 2u);
  EXPECT_EQ(scheduler.updateEpoch(), updateEpoch);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(now)), 0.5f);
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(now), 0), 0u);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(now)));
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(5.0 / 64.0), 0), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(5.0 / 64.0)), 0.0f);

  // maxFrameSkip is the number of EXTRA updates, not the total batch size.
  scheduler.reset(schedulerTime(1.0));
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(1.0 + 6.0 / 64.0), 2), 3u);
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(1.0 + 6.0 / 64.0), 2), 0u);
  EXPECT_EQ(scheduler.epoch(), 4u);
}

TEST(FixedSchedulerTest, RenderCapSwitchPreservesLogicDeadlineAndPhase) {
  FixedScheduler scheduler(64.0, 128.0);
  scheduler.reset(schedulerTime(0.0));
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(0.0)));
  double now = 3.0 / 128.0;
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(now), 5), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(now)), 0.5f);
  auto updateEpoch = scheduler.updateEpoch();

  scheduler.setRenderRate(32.0, schedulerTime(now));
  EXPECT_EQ(scheduler.epoch(), 2u);
  EXPECT_EQ(scheduler.updateEpoch(), updateEpoch);
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(now), 5), 0u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(now)), 0.5f);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(now)));
  EXPECT_FALSE(scheduler.beginRender(schedulerTime(now)));
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(schedulerTime(now)).count(), 1.0 / 128.0);
  scheduler.setRenderRate(32.0, schedulerTime(now));
  EXPECT_EQ(scheduler.epoch(), 2u);

  scheduler.setRenderRate(0.0, schedulerTime(now));
  EXPECT_EQ(scheduler.epoch(), 3u);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(now)));
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(now)));
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(schedulerTime(now)).count(), 0.0);
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(4.0 / 128.0), 5), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(4.0 / 128.0)), 0.0f);
}

TEST(FixedSchedulerTest, LongClockGapAndClockReversalResetPresentation) {
  FixedScheduler scheduler(64.0, 128.0);
  scheduler.reset(schedulerTime(1.0));
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(1.0 + 1.0 / 64.0), 5), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(1.0 + 1.0 / 64.0)), 0.0f);

  EXPECT_EQ(scheduler.updatesDue(schedulerTime(2.0), 5), 1u);
  EXPECT_EQ(scheduler.epoch(), 2u);
  EXPECT_EQ(scheduler.updateEpoch(), 2u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(2.0)), 0.0f);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(2.0)));
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(schedulerTime(2.0)).count(), 1.0 / 128.0);
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(2.0 + 1.0 / 64.0), 5), 1u);

  EXPECT_EQ(scheduler.updatesDue(schedulerTime(1.5), 5), 1u);
  EXPECT_EQ(scheduler.epoch(), 3u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(1.5)), 0.0f);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(1.5)));
}

TEST(FixedSchedulerTest, LongPresentResetsInsteadOfAccumulatingDebt) {
  FixedScheduler scheduler(64.0, 128.0);
  scheduler.reset(schedulerTime(0.0));
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(0.0)));
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(schedulerTime(1.0)).count(), 0.0);
  EXPECT_EQ(scheduler.epoch(), 2u);
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(1.0), 5), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(1.0)), 0.0f);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(1.0)));
}

TEST(FixedSchedulerTest, RepeatedSlowPresentsCannotStarveSimulation) {
  FixedScheduler scheduler(60.0, 0.0);
  scheduler.reset(schedulerTime(0.0));
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(0.0)));
  for (unsigned frame = 1; frame <= 8; ++frame) {
    double now = frame * 0.375;
    EXPECT_DOUBLE_EQ(scheduler.sleepTime(schedulerTime(now)).count(), 0.0);
    EXPECT_EQ(scheduler.updatesDue(schedulerTime(now), 5), 1u);
    EXPECT_TRUE(scheduler.beginRender(schedulerTime(now)));
  }
}

TEST(FixedSchedulerTest, UpdateRateChangeRebasesWithoutAnImmediateExtraTick) {
  FixedScheduler scheduler(64.0, 128.0);
  scheduler.reset(schedulerTime(0.0));
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(1.0 / 64.0), 5), 1u);
  scheduler.setUpdateRate(64.0, schedulerTime(3.0 / 128.0));
  EXPECT_EQ(scheduler.epoch(), 1u);

  double now = 3.0 / 128.0;
  scheduler.setUpdateRate(128.0, schedulerTime(now));
  EXPECT_EQ(scheduler.epoch(), 2u);
  EXPECT_EQ(scheduler.updateEpoch(), 2u);
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(now), 5), 0u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(now)), 1.0f);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(now)));
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(4.0 / 128.0), 5), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(schedulerTime(4.0 / 128.0)), 0.0f);
}

TEST(FixedSchedulerTest, InvalidRateChangesLeaveTheScheduleUntouched) {
  FixedScheduler scheduler(64.0, 128.0);
  scheduler.reset(schedulerTime(0.0));
  for (double invalid : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    scheduler.setRenderRate(invalid, schedulerTime(0.0));
    scheduler.setUpdateRate(invalid, schedulerTime(0.0));
  }
  scheduler.setUpdateRate(0.0, schedulerTime(0.0));
  EXPECT_EQ(scheduler.epoch(), 1u);
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(1.0 / 64.0), 5), 1u);
}

TEST(FixedSchedulerTest, LowRateIdleWaitStillServicesTheOuterLoop) {
  FixedScheduler scheduler(1.0, 1.0);
  scheduler.reset(schedulerTime(0.0));
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(0.0)));
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(schedulerTime(0.0)).count(), FixedScheduler::MaximumSleep.count());
  for (unsigned poll = 1; poll < 8; ++poll) {
    double now = poll / 8.0;
    EXPECT_EQ(scheduler.updatesDue(schedulerTime(now), 5), 0u);
    EXPECT_FALSE(scheduler.beginRender(schedulerTime(now)));
    EXPECT_LE(scheduler.sleepTime(schedulerTime(now)), FixedScheduler::MaximumSleep);
  }
  EXPECT_EQ(scheduler.updatesDue(schedulerTime(1.0), 5), 1u);
  EXPECT_TRUE(scheduler.beginRender(schedulerTime(1.0)));
  EXPECT_EQ(scheduler.epoch(), 1u);
}

TEST(FixedSchedulerTest, QuantizedMicrosecondInputsKeepAbsoluteDeadlines) {
  FixedScheduler scheduler(64.0, 32.0);
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::microseconds> start(
      std::chrono::microseconds(9'876'543'210'987));
  scheduler.reset(start);
  EXPECT_TRUE(scheduler.beginRender(start));
  EXPECT_EQ(scheduler.updatesDue(start + std::chrono::microseconds(15'624), 5), 0u);
  EXPECT_EQ(scheduler.updatesDue(start + std::chrono::microseconds(15'625), 5), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(start + std::chrono::microseconds(15'625)), 0.0f);
  EXPECT_FALSE(scheduler.beginRender(start + std::chrono::microseconds(31'249)));
  EXPECT_TRUE(scheduler.beginRender(start + std::chrono::microseconds(31'250)));
  EXPECT_EQ(scheduler.updatesDue(start + std::chrono::microseconds(31'250), 5), 1u);
  EXPECT_EQ(scheduler.epoch(), 1u);
}
