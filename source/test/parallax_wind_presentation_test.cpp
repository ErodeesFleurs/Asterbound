import std;
import star.world_client;
import star.vector;

#include "gtest/gtest.h"

using namespace Star;

TEST(ParallaxWindPresentationTest, DirectionMagnitudeAndSignedTimeSampleWithoutIntegrating) {
  ParallaxWindTime wind;
  wind.beginUpdate(10.0);
  wind.update(10.25, 4.0f);
  for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f, 0.25f}) {
    EXPECT_EQ(wind.sample(alpha), Vec3D(0.25, 1.0, 1.0) * static_cast<double>(alpha));
    EXPECT_EQ(wind.current, Vec3D(0.25, 1.0, 1.0));
    EXPECT_EQ(wind.previous, Vec3D());
    EXPECT_EQ(wind.lastEpoch, 10.25);
  }
  wind.beginUpdate(10.25);
  wind.update(10.5, -2.0f);
  EXPECT_EQ(wind.sample(0), Vec3D(0.25, 1.0, 1.0));
  EXPECT_EQ(wind.sample(0.5f), Vec3D(0.125, 1.25, 0.75));
  EXPECT_EQ(wind.sample(1), Vec3D(0.0, 1.5, 0.5));
  EXPECT_EQ(wind.sample(-1), wind.previous);
  EXPECT_EQ(wind.sample(2), wind.current);
}

TEST(ParallaxWindPresentationTest, ClockCorrectionResetAndReplacementDoNotMixDomains) {
  ParallaxWindTime wind;
  wind.beginUpdate(59.75);
  wind.update(60.25, 2.0f);
  // Day boundaries do not wrap the absolute integration clock.
  EXPECT_EQ(wind.sample(0.5f), Vec3D(0.25, 0.5, 0.5));
  wind.resetPresentation();
  EXPECT_EQ(wind.sample(0), wind.sample(1));
  // A corrected clock becomes the next tick's baseline, not elapsed wind time.
  wind.beginUpdate(10000.0);
  wind.update(10000.25, -4.0f);
  EXPECT_EQ(wind.previous, Vec3D(0.5, 1.0, 1.0));
  EXPECT_EQ(wind.current, Vec3D(0.25, 2.0, 0.0));
  wind.beginUpdate(10000.25);
  wind.update(1.0, 100.0f);
  EXPECT_EQ(wind.sample(0), wind.sample(1));
  EXPECT_EQ(wind.current, Vec3D(0.25, 2.0, 0.0));
  wind.beginUpdate(1.0);
  wind.update(1.25, 0.0f);
  // Retain the existing zero-wind direction convention.
  EXPECT_EQ(wind.current, Vec3D(0.0, 2.0, 0.0));
  wind = {};
  for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f})
    EXPECT_EQ(wind.sample(alpha), Vec3D());
}
