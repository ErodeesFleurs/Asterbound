import star.json;
import star.string;
import star.ordered_map;
import star.either;
import star.vector;
import star.game_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;

import star.celestial_coordinate;
import star.warping;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.lua;
import star.lua_root;

import star.test_universe;
#include "gtest/gtest.h"


import star.celestial_database;
import star.game_test;

using namespace Star;

[[nodiscard]] std::expected<TestUniverse::ReadyWorld, StarException>
validateWorld(Root& root, TestUniverse& testUniverse, std::int64_t timeoutMilliseconds = 60000) {
  auto ready = testUniverse.waitForWorldReady(timeoutMilliseconds);
  if (!ready)
    return std::unexpected(std::move(ready.error()));

  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  for (auto const& drawable : ready->drawables) {
    if (drawable.isImage()) {
      auto image = (*assets)->image(drawable.imagePart().image);
      if (!image)
        return std::unexpected(std::move(image.error()));
    }
  }
  ::testing::Test::RecordProperty("firstServerStep", std::to_string(ready->firstServerStep));
  ::testing::Test::RecordProperty("readyServerStep", std::to_string(ready->readyServerStep));
  ::testing::Test::RecordProperty("observedScriptedNpcs", std::to_string(ready->observedScriptedNpcs));
  ::testing::Test::RecordProperty("observedCoordinators", std::to_string(ready->observedCoordinators));
  return ready;
}

const GameTestRegistration randomCelestialWorld("SpawnTest", "RandomCelestialWorld", [](GameTest& fixture) {
  auto celestialDatabase = CelestialMasterDatabase::create(fixture.root());
  ASSERT_TRUE(celestialDatabase);
  auto celestialWorld = (*celestialDatabase)->findRandomWorld(10, 50,
      [&](CelestialCoordinate const& coord) -> std::expected<bool, StarException> {
        auto parameters = (*celestialDatabase)->parameters(coord);
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        return (*parameters)->isVisitable();
      });
  ASSERT_TRUE(celestialWorld);
  ASSERT_TRUE(*celestialWorld);

  auto testUniverse = TestUniverse::create(fixture.root(), Vec2U(100, 100));
  ASSERT_TRUE(testUniverse);
  WorldId worldId = CelestialWorldId(**celestialWorld);
  ASSERT_TRUE((*testUniverse)->warpPlayer(worldId));
  EXPECT_EQ((*testUniverse)->currentPlayerWorld(), worldId);
  auto ready = validateWorld(fixture.root(), **testUniverse);
  ASSERT_TRUE(ready) << ready.error().what();
  EXPECT_GT(ready->readyServerStep, ready->firstServerStep);
  EXPECT_FALSE(ready->drawables.empty());
  ASSERT_TRUE((*testUniverse)->close());
});

const GameTestRegistration celestialCoordinatorMigration("SpawnTest", "CelestialCoordinatorLua55Migration", [](GameTest& fixture) {
  // This exact world exposed vanilla npccombat.lua's reassigned loop control.
  auto worldId = parseWorldId("CelestialWorld:399373772:-84992261:-27065884:9");
  ASSERT_TRUE(worldId) << worldId.error().what();
  auto testUniverse = TestUniverse::create(fixture.root(), Vec2U(100, 100));
  ASSERT_TRUE(testUniverse);
  ASSERT_TRUE((*testUniverse)->warpPlayer(*worldId));
  EXPECT_EQ((*testUniverse)->currentPlayerWorld(), *worldId);
  auto ready = validateWorld(fixture.root(), **testUniverse);
  ASSERT_TRUE(ready) << ready.error().what();
  EXPECT_GT(ready->readyServerStep, ready->firstServerStep);
  EXPECT_GT(ready->observedScriptedNpcs, 0u);
  EXPECT_FALSE(ready->drawables.empty());
  ASSERT_TRUE((*testUniverse)->close());

  // Compile that same asset explicitly even if NPC scheduling changes.
  auto lua = LuaRoot::create(fixture.root());
  ASSERT_TRUE(lua) << lua.error().what();
  auto context = (*lua)->createContext("/stagehands/coordinator/npccombat.lua");
  ASSERT_TRUE(context) << context.error().what();
});

const GameTestRegistration randomInstanceWorld("SpawnTest", "RandomInstanceWorld", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto assets = root.assets();
  ASSERT_TRUE(assets);
  auto config = (*assets)->json("/instance_worlds.config");
  ASSERT_TRUE(config);
  auto worlds = config->toObject();
  ASSERT_TRUE(worlds);
  StringList instanceWorlds = worlds->keys();
  ASSERT_GT(instanceWorlds.size(), 0u);
  auto selectedWorld = root.random().randFrom(instanceWorlds);
  ASSERT_TRUE(selectedWorld);
  WorldId instanceWorld = InstanceWorldId(selectedWorld->get());

  auto testUniverse = TestUniverse::create(root, Vec2U(100, 100));
  ASSERT_TRUE(testUniverse);
  ASSERT_TRUE((*testUniverse)->warpPlayer(instanceWorld));
  EXPECT_EQ((*testUniverse)->currentPlayerWorld(), instanceWorld);
  // A bounded failure must leave the same real universe usable for a retry.
  auto timedOut = validateWorld(root, **testUniverse, 0);
  ASSERT_FALSE(timedOut);
  EXPECT_TRUE(timedOut.error().is<TestUniverseTimeoutException>());
  EXPECT_EQ((*testUniverse)->currentPlayerWorld(), instanceWorld);
  auto ready = validateWorld(root, **testUniverse);
  ASSERT_TRUE(ready) << ready.error().what();
  EXPECT_GT(ready->readyServerStep, ready->firstServerStep);
  EXPECT_FALSE(ready->drawables.empty());
  ASSERT_TRUE((*testUniverse)->close());
  auto closed = (*testUniverse)->waitForWorldReady();
  ASSERT_FALSE(closed);
  EXPECT_TRUE(closed.error().is<TestUniverseException>());
  ASSERT_TRUE((*testUniverse)->close());
});
