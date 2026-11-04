import std;
import star.fixed_scheduler;

#include "gtest/gtest.h"

using namespace Star;

TEST(FixedSchedulerTest, HighRefreshCanRenderWithoutAnyLogicTick) {
  FixedScheduler scheduler(64.0, 256.0);
  scheduler.reset(10.0);
  EXPECT_EQ(scheduler.epoch(), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(10.0), 1.0f);
  for (unsigned frame = 0; frame != 4; ++frame) {
    double now = 10.0 + frame / 256.0;
    EXPECT_EQ(scheduler.updatesDue(now, 5), 0u);
    EXPECT_TRUE(scheduler.beginRender(now));
    EXPECT_FLOAT_EQ(scheduler.interpolation(now), 1.0f);
  }
  EXPECT_EQ(scheduler.updatesDue(10.0 + 1.0 / 64.0, 5), 1u);
  EXPECT_TRUE(scheduler.beginRender(10.0 + 1.0 / 64.0));
  EXPECT_FLOAT_EQ(scheduler.interpolation(10.0 + 1.0 / 64.0), 0.0f);
  EXPECT_EQ(scheduler.updatesDue(10.0 + 5.0 / 256.0, 5), 0u);
  EXPECT_TRUE(scheduler.beginRender(10.0 + 5.0 / 256.0));
  EXPECT_FLOAT_EQ(scheduler.interpolation(10.0 + 5.0 / 256.0), 0.25f);
  EXPECT_EQ(scheduler.epoch(), 1u);
}

TEST(FixedSchedulerTest, SlowBlockingRenderCatchesUpWithoutAnotherSleep) {
  FixedScheduler scheduler(64.0, 32.0);
  scheduler.reset(0.0);
  EXPECT_TRUE(scheduler.beginRender(0.0));

  // The initial present took three logic intervals. Neither logic nor render
  // gets a fresh relative sleep budget after that blocking present.
  double now = 3.0 / 64.0;
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(now), 0.0);
  EXPECT_EQ(scheduler.updatesDue(now, 5), 3u);
  EXPECT_TRUE(scheduler.beginRender(now));
  EXPECT_FLOAT_EQ(scheduler.interpolation(now), 0.0f);
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(now), 1.0 / 64.0);

  now = 5.0 / 64.0;
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(now), 0.0);
  EXPECT_EQ(scheduler.updatesDue(now, 5), 2u);
  EXPECT_TRUE(scheduler.beginRender(now));
  EXPECT_EQ(scheduler.epoch(), 1u);
}

TEST(FixedSchedulerTest, LowRenderCapDoesNotReduceLogicCadence) {
  FixedScheduler scheduler(64.0, 16.0);
  scheduler.reset(0.0);
  EXPECT_TRUE(scheduler.beginRender(0.0));
  for (unsigned tick = 1; tick <= 8; ++tick) {
    double now = tick / 64.0;
    EXPECT_EQ(scheduler.updatesDue(now, 5), 1u);
    EXPECT_EQ(scheduler.beginRender(now), tick % 4 == 0);
  }
  EXPECT_EQ(scheduler.epoch(), 1u);
}

TEST(FixedSchedulerTest, BoundedCatchupDropsOnlyWholeOverdueTicks) {
  FixedScheduler scheduler(64.0, 0.0);
  scheduler.reset(0.0);
  auto updateEpoch = scheduler.updateEpoch();
  double now = 4.5 / 64.0;
  EXPECT_EQ(scheduler.updatesDue(now, 0), 1u);
  EXPECT_EQ(scheduler.epoch(), 2u);
  EXPECT_EQ(scheduler.updateEpoch(), updateEpoch);
  EXPECT_FLOAT_EQ(scheduler.interpolation(now), 0.5f);
  EXPECT_EQ(scheduler.updatesDue(now, 0), 0u);
  EXPECT_TRUE(scheduler.beginRender(now));
  EXPECT_EQ(scheduler.updatesDue(5.0 / 64.0, 0), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(5.0 / 64.0), 0.0f);

  // maxFrameSkip is the number of EXTRA updates, not the total batch size.
  scheduler.reset(1.0);
  EXPECT_EQ(scheduler.updatesDue(1.0 + 6.0 / 64.0, 2), 3u);
  EXPECT_EQ(scheduler.updatesDue(1.0 + 6.0 / 64.0, 2), 0u);
  EXPECT_EQ(scheduler.epoch(), 4u);
}

TEST(FixedSchedulerTest, RenderCapSwitchPreservesLogicDeadlineAndPhase) {
  FixedScheduler scheduler(64.0, 128.0);
  scheduler.reset(0.0);
  EXPECT_TRUE(scheduler.beginRender(0.0));
  double now = 3.0 / 128.0;
  EXPECT_EQ(scheduler.updatesDue(now, 5), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(now), 0.5f);
  auto updateEpoch = scheduler.updateEpoch();

  scheduler.setRenderRate(32.0, now);
  EXPECT_EQ(scheduler.epoch(), 2u);
  EXPECT_EQ(scheduler.updateEpoch(), updateEpoch);
  EXPECT_EQ(scheduler.updatesDue(now, 5), 0u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(now), 0.5f);
  EXPECT_TRUE(scheduler.beginRender(now));
  EXPECT_FALSE(scheduler.beginRender(now));
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(now), 1.0 / 128.0);
  scheduler.setRenderRate(32.0, now);
  EXPECT_EQ(scheduler.epoch(), 2u);

  scheduler.setRenderRate(0.0, now);
  EXPECT_EQ(scheduler.epoch(), 3u);
  EXPECT_TRUE(scheduler.beginRender(now));
  EXPECT_TRUE(scheduler.beginRender(now));
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(now), 0.0);
  EXPECT_EQ(scheduler.updatesDue(4.0 / 128.0, 5), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(4.0 / 128.0), 0.0f);
}

TEST(FixedSchedulerTest, LongClockGapAndClockReversalResetPresentation) {
  FixedScheduler scheduler(64.0, 128.0);
  scheduler.reset(1.0);
  EXPECT_EQ(scheduler.updatesDue(1.0 + 1.0 / 64.0, 5), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(1.0 + 1.0 / 64.0), 0.0f);

  EXPECT_EQ(scheduler.updatesDue(2.0, 5), 1u);
  EXPECT_EQ(scheduler.epoch(), 2u);
  EXPECT_EQ(scheduler.updateEpoch(), 2u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(2.0), 0.0f);
  EXPECT_TRUE(scheduler.beginRender(2.0));
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(2.0), 1.0 / 128.0);
  EXPECT_EQ(scheduler.updatesDue(2.0 + 1.0 / 64.0, 5), 1u);

  EXPECT_EQ(scheduler.updatesDue(1.5, 5), 1u);
  EXPECT_EQ(scheduler.epoch(), 3u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(1.5), 0.0f);
  EXPECT_TRUE(scheduler.beginRender(1.5));
}

TEST(FixedSchedulerTest, LongPresentResetsInsteadOfAccumulatingDebt) {
  FixedScheduler scheduler(64.0, 128.0);
  scheduler.reset(0.0);
  EXPECT_TRUE(scheduler.beginRender(0.0));
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(1.0), 0.0);
  EXPECT_EQ(scheduler.epoch(), 2u);
  EXPECT_EQ(scheduler.updatesDue(1.0, 5), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(1.0), 0.0f);
  EXPECT_TRUE(scheduler.beginRender(1.0));
}

TEST(FixedSchedulerTest, RepeatedSlowPresentsCannotStarveSimulation) {
  FixedScheduler scheduler(60.0, 0.0);
  scheduler.reset(0.0);
  EXPECT_TRUE(scheduler.beginRender(0.0));
  for (unsigned frame = 1; frame <= 8; ++frame) {
    double now = frame * 0.375;
    EXPECT_DOUBLE_EQ(scheduler.sleepTime(now), 0.0);
    EXPECT_EQ(scheduler.updatesDue(now, 5), 1u);
    EXPECT_TRUE(scheduler.beginRender(now));
  }
}

TEST(FixedSchedulerTest, UpdateRateChangeRebasesWithoutAnImmediateExtraTick) {
  FixedScheduler scheduler(64.0, 128.0);
  scheduler.reset(0.0);
  EXPECT_EQ(scheduler.updatesDue(1.0 / 64.0, 5), 1u);
  scheduler.setUpdateRate(64.0, 3.0 / 128.0);
  EXPECT_EQ(scheduler.epoch(), 1u);

  double now = 3.0 / 128.0;
  scheduler.setUpdateRate(128.0, now);
  EXPECT_EQ(scheduler.epoch(), 2u);
  EXPECT_EQ(scheduler.updateEpoch(), 2u);
  EXPECT_EQ(scheduler.updatesDue(now, 5), 0u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(now), 1.0f);
  EXPECT_TRUE(scheduler.beginRender(now));
  EXPECT_EQ(scheduler.updatesDue(4.0 / 128.0, 5), 1u);
  EXPECT_FLOAT_EQ(scheduler.interpolation(4.0 / 128.0), 0.0f);
}

TEST(FixedSchedulerTest, InvalidRateChangesLeaveTheScheduleUntouched) {
  FixedScheduler scheduler(64.0, 128.0);
  scheduler.reset(0.0);
  for (double invalid : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    scheduler.setRenderRate(invalid, 0.0);
    scheduler.setUpdateRate(invalid, 0.0);
  }
  scheduler.setUpdateRate(0.0, 0.0);
  EXPECT_EQ(scheduler.epoch(), 1u);
  EXPECT_EQ(scheduler.updatesDue(1.0 / 64.0, 5), 1u);
}

TEST(FixedSchedulerTest, LowRateIdleWaitStillServicesTheOuterLoop) {
  FixedScheduler scheduler(1.0, 1.0);
  scheduler.reset(0.0);
  EXPECT_TRUE(scheduler.beginRender(0.0));
  EXPECT_DOUBLE_EQ(scheduler.sleepTime(0.0), FixedScheduler::MaximumSleep);
  for (unsigned poll = 1; poll < 8; ++poll) {
    double now = poll / 8.0;
    EXPECT_EQ(scheduler.updatesDue(now, 5), 0u);
    EXPECT_FALSE(scheduler.beginRender(now));
    EXPECT_LE(scheduler.sleepTime(now), FixedScheduler::MaximumSleep);
  }
  EXPECT_EQ(scheduler.updatesDue(1.0, 5), 1u);
  EXPECT_TRUE(scheduler.beginRender(1.0));
  EXPECT_EQ(scheduler.epoch(), 1u);
}
