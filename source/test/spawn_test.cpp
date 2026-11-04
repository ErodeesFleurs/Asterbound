import star.json;
import star.string;
import star.ordered_map;
import star.either;
import star.vector;
import star.game_types;
import star.entity_rendering_types;
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
import star.algorithm;
import star.file;
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
import star.entity;
import star.entity_rendering;
import star.rect;
import star.net_packets;
import star.world_client;
import star.world;
import star.byte_array;
import star.world_render_data;
import star.world_geometry;
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

namespace {

class PresentationLifecycleEntity : public Entity {
public:
  explicit PresentationLifecycleEntity(Root& root) : Entity(root) {}
  EntityType entityType() const override { return EntityType::Stagehand; }
  Vec2F position() const override { return {1024, 1024}; }
  RectF metaBoundBox() const override { return RectF::withSize({}, {1, 1}); }
  ClientEntityMode clientEntityMode() const override { return ClientEntityMode::ClientMasterAllowed; }
  std::expected<void, StarException> init(World& world, EntityId id, EntityMode) override {
    // Insert as a local slave so this behavioral fixture is not serialized to
    // the real server. Promotion below tests the client-master death path.
    return Entity::init(world, id, EntityMode::Slave);
  }
  std::expected<void, StarException> promote(World& world) {
    auto id = entityId();
    auto cleared = Entity::uninit();
    if (!cleared)
      return std::unexpected(std::move(cleared.error()));
    return Entity::init(world, id, EntityMode::Master);
  }
  std::expected<bool, StarException> shouldDestroy() const override { return dying; }
  std::expected<void, StarException> collectRenderEvents(RenderCallback& renderer) override {
    ++collections;
    if (failCollection)
      return std::unexpected(StarException("Deliberate presentation lifecycle test failure"));
    if (nextParticle) {
      auto emitted = renderer.addParticle(std::move(*nextParticle));
      nextParticle.reset();
      return emitted;
    }
    return {};
  }
  std::expected<void, StarException> destroy(std::optional<std::reference_wrapper<RenderCallback>>) override {
    ++destructions;
    return {};
  }
  std::expected<void, StarException> uninit() override {
    ++uninitializations;
    return Entity::uninit();
  }
  bool dying = false;
  bool failCollection = false;
  std::optional<Particle> nextParticle;
  unsigned collections = 0;
  unsigned destructions = 0;
  unsigned uninitializations = 0;
};

}

const GameTestRegistration presentationRemovalLifecycle(
  "SpawnTest", "PresentationRemovalCollectsOnceAndIsolatesSlaveFailures", [](GameTest& fixture) {
    // Failure isolation is the subject here; use a private Root rather than
    // the shared fixture's sink that fails on every expected Error record.
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto settings = fixture.rootSettings();
    settings.storageDirectory = File::relativeTo(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto createdRoot = Root::create(std::move(settings));
    ASSERT_TRUE(createdRoot) << createdRoot.error().what();
    auto& root = **createdRoot;
    auto universe = TestUniverse::create(root, Vec2U(100, 100));
    ASSERT_TRUE(universe) << universe.error().what();
    ASSERT_TRUE((*universe)->warpPlayer(InstanceWorldId("outpost")));
    auto ready = (*universe)->waitForWorldReady();
    ASSERT_TRUE(ready) << ready.error().what();
    auto world = (*universe)->worldClient();
    ASSERT_TRUE(world);
    constexpr float dt = 1.0f / 60.0f;

    auto dying = std::make_shared<PresentationLifecycleEntity>(root);
    ASSERT_TRUE(world->addEntity(dying));
    ASSERT_TRUE(dying->inWorld());
    ASSERT_TRUE(dying->promote(*world));
    auto dyingId = dying->entityId();
    dying->dying = true;
    ASSERT_TRUE(world->update(dt));
    EXPECT_EQ(dying->collections, 1u);
    EXPECT_EQ(dying->destructions, 1u);
    EXPECT_EQ(dying->uninitializations, 1u);
    EXPECT_FALSE(world->entity(dyingId));
    auto emitter = std::make_shared<PresentationLifecycleEntity>(root);
    ASSERT_TRUE(world->addEntity(emitter));
    Particle newborn;
    newborn.type = Particle::Type::Text;
    newborn.size = newborn.baseSize = 1;
    newborn.string = "presentation-birth-fixture";
    newborn.position = Vec2F(world->clientWindow().center());
    newborn.velocity = newborn.finalVelocity = {60, 0};
    newborn.approach = {};
    newborn.ignoreWind = true;
    newborn.collidesForeground = false;
    newborn.collidesLiquid = false;
    newborn.timeToLive = 2.0f;
    emitter->nextParticle = newborn;
    ASSERT_TRUE(world->update(dt));
    WorldRenderData display;
    auto findNewborn = [&]() -> Particle const* {
      for (auto const& particle : display.particles)
        if (particle.string == newborn.string)
          return &particle;
      return nullptr;
    };
    for (float alpha : {0.0f, 0.5f, 1.0f, 0.25f}) {
      ASSERT_TRUE(world->render(display, 0, std::nullopt, PresentationContext(alpha)));
      auto sampled = findNewborn();
      ASSERT_TRUE(sampled);
      EXPECT_EQ(sampled->position, newborn.position);
      EXPECT_FLOAT_EQ(sampled->timeToLive, 2.0f);
    }
    ASSERT_TRUE(world->update(dt));
    ASSERT_TRUE(world->render(display, 0, std::nullopt, PresentationContext()));
    auto advanced = findNewborn();
    ASSERT_TRUE(advanced);
    auto travel = world->geometry().diff(advanced->position, newborn.position);
    EXPECT_NEAR(travel[0], 1.0f, 1e-4f);
    EXPECT_NEAR(travel[1], 0.0f, 1e-4f);
    EXPECT_NEAR(advanced->timeToLive, 2.0f - dt, 1e-6f);
    ASSERT_TRUE(world->removeEntity(emitter->entityId(), false));

    // Exercise the real incoming death-packet path, including its optional
    // interpolation timer. Every intervening live tick and the final removal
    // may collect once, never both regular and teardown passes in one tick.
    for (bool failCollection : {false, true}) {
      auto slave = std::make_shared<PresentationLifecycleEntity>(root);
      slave->failCollection = failCollection;
      ASSERT_TRUE(world->addEntity(slave));
      ASSERT_TRUE(slave->inWorld());
      auto id = slave->entityId();
      auto packet = std::make_shared<EntityDestroyPacket>(id, ByteArray{}, true);
      ASSERT_TRUE(world->handleIncomingPackets({packet}));
      EXPECT_LE(slave->collections, 1u);
      for (unsigned tick = 0; tick != 120 && world->entity(id); ++tick) {
        auto before = slave->collections;
        ASSERT_TRUE(world->update(dt));
        EXPECT_LE(slave->collections - before, 1u);
      }
      EXPECT_FALSE(world->entity(id));
      EXPECT_FALSE(slave->inWorld());
      EXPECT_EQ(slave->destructions, 1u);
      EXPECT_EQ(slave->uninitializations, 1u);
      EXPECT_GT(slave->collections, 0u);
    }

    auto master = std::make_shared<PresentationLifecycleEntity>(root);
    master->failCollection = true;
    ASSERT_TRUE(world->addEntity(master));
    ASSERT_TRUE(master->promote(*world));
    auto masterId = master->entityId();
    EXPECT_FALSE(world->removeEntity(masterId, true));
    EXPECT_TRUE(world->entity(masterId));
    EXPECT_EQ(master->destructions, 0u);
    EXPECT_EQ(master->uninitializations, 0u);
    ASSERT_TRUE(world->removeEntity(masterId, false));
    ASSERT_TRUE((*universe)->close());
  });
