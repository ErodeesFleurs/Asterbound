import std;
import star.celestial_coordinate;
import star.celestial_database;
import star.celestial_graphics;
import star.celestial_parameters;
import star.celestial_types;
import star.either;
import star.list;
import star.sky;
import star.sky_parameters;
import star.sky_types;
import star.game_test;
import star.root;
import star.json;
import star.vector;
import star.math_common;

#include "gtest/gtest.h"

using namespace Star;

namespace {
auto advanceSky(Sky& sky, double dt) {
  sky.beginUpdate();
  return sky.update(dt);
}
}

const GameTestRegistration twinkle_frames("CelestialGraphics", "TwinkleWrapsAtPeriodAndRejectsInvalidPhase", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto master = CelestialMasterDatabase::create(root);
  ASSERT_TRUE(master);
  auto assets = root.assets();
  ASSERT_TRUE(assets);
  auto slave = CelestialSlaveDatabase::create(*assets, root.random(), (*master)->base_information());
  ASSERT_TRUE(slave);
  CelestialCoordinate const system(Vec3I(0, 0, 1));
  for (int period : {2, 0}) {
    Json params = JsonObject{{"twinkleTime", JsonArray{period, period}}, {"twinkleFrames", "frames"}, {"twinkleBackground", "background"}};
    auto star = CelestialParameters::create(root, system, 5, "Star", std::move(params));
    ASSERT_TRUE(star);
    CelestialChunk chunk;
    chunk.chunk_index = {0, 0};
    chunk.system_parameters[system.location()] = std::move(*star);
    List<CelestialResponse> responses;
    responses.push_back(make_left(std::move(chunk)));
    ASSERT_TRUE((*slave)->push_responses(std::move(responses)));
    auto layers = CelestialGraphics::draw_system_twinkle(root, **slave, system, 2.0);
    if (period == 0) {
      EXPECT_FALSE(layers);
    } else {
      ASSERT_TRUE(layers);
      ASSERT_EQ(layers->size(), 2u);
      EXPECT_EQ((*layers)[0].first, "background");
      EXPECT_EQ((*layers)[1].first, "frames:0");
      volatile std::uint64_t infinity_bits = 0x7ff0000000000000ull;
      auto infinity = std::bit_cast<double>(static_cast<std::uint64_t>(infinity_bits));
      EXPECT_FALSE(CelestialGraphics::draw_system_twinkle(root, **slave, system, infinity));
    }
  }
});

const GameTestRegistration skyPresentationTime(
    "SkyPresentationTest", "SamplesWithoutAdvancingTheClock", [](GameTest& fixture) {
  auto sky = Sky::create(fixture.root());
  ASSERT_TRUE(sky);
  ASSERT_TRUE(advanceSky(**sky, 0.04));
  for (float alpha : {0.25f, 0.75f, 0.25f}) {
    auto sample = (*sky)->renderData(alpha);
    ASSERT_TRUE(sample);
    EXPECT_NEAR(sample->epochTime, 0.04 * alpha, 1e-9);
    EXPECT_DOUBLE_EQ((*sky)->epochTime(), 0.04);
  }
  ASSERT_TRUE(advanceSky(**sky, 0.04));
  auto previous = (*sky)->renderData(-1.0f);
  auto paused = (*sky)->renderData(1.0f);
  auto beyond = (*sky)->renderData(2.0f);
  ASSERT_TRUE(previous);
  ASSERT_TRUE(paused);
  ASSERT_TRUE(beyond);
  EXPECT_DOUBLE_EQ(previous->epochTime, 0.04);
  EXPECT_DOUBLE_EQ(paused->epochTime, 0.08);
  EXPECT_DOUBLE_EQ(beyond->epochTime, 0.08);
});

const GameTestRegistration skyPresentationWrap(
    "SkyPresentationTest", "DayAndRotationWrapDoNotSweepBackwards", [](GameTest& fixture) {
  SkyParameters parameters;
  parameters.dayLength = 60.0f;
  auto sky = Sky::create(fixture.root(), parameters, true);
  ASSERT_TRUE(sky);
  ASSERT_TRUE(advanceSky(**sky, 59.99));
  ASSERT_TRUE(advanceSky(**sky, 0.02));
  auto middle = (*sky)->renderData(0.5f);
  ASSERT_TRUE(middle);
  EXPECT_NEAR(middle->epochTime, 60.0, 1e-9);
  EXPECT_NEAR(std::remainder(middle->timeOfDay, 60.0f), 0.0f, 1e-5f);
  EXPECT_NEAR(std::sin(middle->orbitAngle), 0.0f, 1e-5f);
  EXPECT_NEAR(std::cos(middle->orbitAngle), 1.0f, 1e-5f);
  EXPECT_NEAR(std::sin(middle->starRotation), 0.0f, 1e-5f);
  EXPECT_NEAR(std::cos(middle->starRotation), 1.0f, 1e-5f);
});

const GameTestRegistration skyPresentationFlight(
    "SkyPresentationTest", "FlightMotionSamplesButTransitionsReset", [](GameTest& fixture) {
  auto sky = Sky::create(fixture.root());
  ASSERT_TRUE(sky);
  ASSERT_TRUE((*sky)->startFlying(true, true,
      JsonObject{{"flyMaxVelocity", 1000.0f}, {"starVelocityFactor", 0.01f}, {"flyingTimer", 0.0f}}));
  ASSERT_TRUE((*sky)->stateUpdate());
  ASSERT_TRUE(advanceSky(**sky, 0.04));
  auto middle = (*sky)->renderData(0.5f);
  auto current = (*sky)->renderData(1.0f);
  ASSERT_TRUE(middle);
  ASSERT_TRUE(current);
  EXPECT_NEAR(middle->starOffset[0], 0.2f, 1e-5f);
  EXPECT_NEAR(current->starOffset[0], 0.4f, 1e-5f);

  SkyParameters destination;
  destination.seed = 1234;
  ASSERT_TRUE((*sky)->stopFlyingAt(destination));
  ASSERT_TRUE(advanceSky(**sky, 0.04));
  auto reset = (*sky)->renderData(0.0f);
  current = (*sky)->renderData(1.0f);
  ASSERT_TRUE(reset);
  ASSERT_TRUE(current);
  EXPECT_EQ(reset->type, SkyType::Orbital);
  EXPECT_EQ(reset->skyParameters.seed, destination.seed);
  EXPECT_DOUBLE_EQ(reset->epochTime, current->epochTime);
  EXPECT_EQ(reset->starOffset, current->starOffset);
  EXPECT_FLOAT_EQ(reset->starRotation, current->starRotation);
  EXPECT_EQ(reset->worldOffset, current->worldOffset);
});

const GameTestRegistration skyPresentationReset(
    "SkyPresentationTest", "ClockCorrectionsAndExplicitResetsDiscardHistory", [](GameTest& fixture) {
  auto server = Sky::create(fixture.root());
  auto client = Sky::create(fixture.root());
  ASSERT_TRUE(server);
  ASSERT_TRUE(client);
  ASSERT_TRUE((*server)->update(2.0));
  auto initial = (*server)->writeUpdate();
  ASSERT_TRUE(initial);
  ASSERT_TRUE((*client)->readUpdate(initial->first));
  ASSERT_TRUE(advanceSky(**client, 0.04));
  ASSERT_TRUE((*server)->update(2.0));
  auto delta = (*server)->writeUpdate(initial->second);
  ASSERT_TRUE(delta);
  ASSERT_TRUE((*client)->readUpdate(delta->first));
  auto corrected = (*client)->renderData(0.0f);
  ASSERT_TRUE(corrected);
  EXPECT_DOUBLE_EQ(corrected->epochTime, 4.0);

  ASSERT_TRUE(advanceSky(**client, 0.04));
  auto sample = (*client)->renderData(0.25f);
  ASSERT_TRUE(sample);
  EXPECT_NEAR(sample->epochTime, 4.01, 1e-9);
  (*client)->resetPresentation();
  sample = (*client)->renderData(0.0f);
  ASSERT_TRUE(sample);
  EXPECT_DOUBLE_EQ(sample->epochTime, 4.04);

  (*client)->setEpochTime(100.0);
  sample = (*client)->renderData(0.0f);
  ASSERT_TRUE(sample);
  EXPECT_DOUBLE_EQ(sample->epochTime, 100.0);
});
