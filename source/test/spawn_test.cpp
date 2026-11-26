import star.json;
import star.string;
import star.ordered_map;
import star.vector;
import star.game_types;
import star.entity_rendering_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.drawable;
import star.damage;
import star.damage_manager;
import star.world_template;
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
import star.light_source;
import star.listener;
import std;
import star.format;

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
import star.universe_client;
import star.player;
import star.player_log;
import star.entity;
import star.entity_rendering;
import star.rect;
import star.net_packets;
import star.world_client;
import star.plant;
import star.plant_drop;
import star.image;
import star.buffer;
import star.world;
import star.byte_array;
import star.world_render_data;
import star.world_geometry;
import star.cellular_lighting;
import star.collision_block;
import star.liquid_types;
import star.material_types;
import star.math_common;
import star.tile_modification;
import star.world_tiles;
#include "gtest/gtest.h"


import star.celestial_database;
import star.game_test;

using namespace Star;

TEST(EntityFrameLayers, StableAndChurningKeysReuseStorageAndKeepEntityOrder) {
  WorldRenderData frame;
  frame.entity_drawables.resize(2);
  FrameLayers<EntityDrawableGroup> routes;
  constexpr EntityRenderLayer shared_layer = 0;
  std::array<std::uintptr_t, 4> stable_storage{};
  std::array<std::array<std::uintptr_t, 4>, 2> vehicle_storage{};
  for (EntityRenderLayer i = 0; i < 1024; ++i) {
    // Stable first, then change one simulated vehicle layer every frame.
    auto const vehicle_layer = i < 16 ? 1u : i;
    frame.prepare_frame();
    auto& first = frame.entity_drawables[0];
    auto& second = frame.entity_drawables[1];
    first.highlight_effect.level = 0.25f;
    second.highlight_effect.level = 0.75f;
    first.layers.append(vehicle_layer, {});
    first.layers.append(shared_layer, {});
    second.layers.append(shared_layer, {});
    route_entity_drawables(routes, frame.entity_drawables);

    auto const bucket_count = i < 16 ? 2 : 3;
    ASSERT_EQ(std::ranges::distance(first.layers), bucket_count);
    ASSERT_EQ(std::ranges::distance(second.layers), 1);
    ASSERT_EQ(std::ranges::distance(routes), bucket_count);
    auto const& shared = *routes.begin();
    auto const& vehicle = *std::prev(routes.end());
    ASSERT_EQ(vehicle.first, vehicle_layer);
    ASSERT_EQ(shared.first, shared_layer);
    ASSERT_EQ(vehicle.second.size(), 1u);
    ASSERT_EQ(shared.second.size(), 2u);
    EXPECT_EQ(shared.second[0].highlight_effect.level, 0.25f);
    EXPECT_EQ(shared.second[1].highlight_effect.level, 0.75f);
    EXPECT_EQ(&shared.second[0].drawables.get(), &first.layers.begin()->second);
    EXPECT_EQ(&shared.second[1].drawables.get(), &second.layers.begin()->second);
    auto const& source = *std::prev(first.layers.end());
    ASSERT_EQ(source.first, vehicle_layer);
    EXPECT_EQ(&vehicle.second.front().drawables.get(), &source.second);
    // Nodes and list buffers survive, including the stable key encountered
    // *after* a new key. Churn needs only two alternating vehicle buckets.
    std::array const changing{
      reinterpret_cast<std::uintptr_t>(&source), reinterpret_cast<std::uintptr_t>(source.second.data()),
      reinterpret_cast<std::uintptr_t>(&vehicle), reinterpret_cast<std::uintptr_t>(vehicle.second.data())};
    std::array const stable{
      reinterpret_cast<std::uintptr_t>(&shared), reinterpret_cast<std::uintptr_t>(shared.second.data()),
      reinterpret_cast<std::uintptr_t>(shared.second[0].drawables.get().data()),
      reinterpret_cast<std::uintptr_t>(shared.second[1].drawables.get().data())};
    if (i == 0) {
      stable_storage = stable;
      vehicle_storage.fill(changing);
    }
    if (i == 16)
      vehicle_storage[0] = changing;
    EXPECT_EQ(stable, stable_storage);
    ASSERT_EQ(changing, vehicle_storage[i % 2]);
  }
  // When a layer disappears and stays absent, release its node and buffer.
  for (int i = 0; i < 3; ++i) {
    frame.prepare_frame();
    route_entity_drawables(routes, frame.entity_drawables);
  }
  EXPECT_TRUE(std::ranges::empty(routes));
  EXPECT_TRUE(std::ranges::empty(frame.entity_drawables[0].layers));
}

[[nodiscard]] std::expected<TestUniverse::ReadyWorld, StarException>
validateWorld(Root& root, TestUniverse& testUniverse, std::int64_t timeoutMilliseconds = 60000) {
  auto ready = testUniverse.waitForWorldReady(timeoutMilliseconds);
  if (!ready)
    return std::unexpected(std::move(ready.error()));

  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  for (auto const& drawable : ready->drawables) {
    if (drawable.is_image()) {
      auto image = (*assets)->image(drawable.image_part().image);
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
  auto celestial_database = CelestialMasterDatabase::create(fixture.root());
  ASSERT_TRUE(celestial_database);
  auto celestialWorld = (*celestial_database)->find_random_world(10, 50,
      [&](CelestialCoordinate const& coord) -> std::expected<bool, StarException> {
        auto parameters = (*celestial_database)->parameters(coord);
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        return (*parameters)->is_visitable();
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
  auto context = (*lua)->create_context("/stagehands/coordinator/npccombat.lua");
  ASSERT_TRUE(context) << context.error().what();
});

const GameTestRegistration randomInstanceWorld("SpawnTest", "RandomInstanceWorld", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto assets = root.assets();
  ASSERT_TRUE(assets);
  auto config = (*assets)->json("/instance_worlds.config");
  ASSERT_TRUE(config);
  auto worlds = config->to_object();
  ASSERT_TRUE(worlds);
  StringList instanceWorlds = worlds->keys();
  ASSERT_GT(instanceWorlds.size(), 0u);
  auto selectedWorld = root.random().sample_ref(instanceWorlds);
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

const GameTestRegistration client_liquid_prediction("SpawnTest", "LiquidCollectionPredictsAbsoluteRemainders", [](GameTest& fixture) {
  auto check_result = [&](auto const& result, std::string_view action, std::source_location at = std::source_location::current()) {
    return fixture.check(bool(result), result ? action : result.error().what(), at);
  };
  auto directory = File::temporary_directory(fixture.root().random());
  if (!check_result(directory, "create liquid prediction fixture directory")) return;
  auto cleanup = make_scope_exit([&] { check_result(File::remove_all(*directory), "remove liquid prediction fixture directory"); });
  auto overlay = File::relative_to(*directory, "assets");
  auto items = File::relative_to(overlay, "items");
  if (!check_result(File::create_directories(items), "create item patch directory")) return;
  if (!check_result(File::write_file(String(R"([
    {"op":"replace","path":"/liquidItems/bucketSize","value":1}
  ])"), File::relative_to(items, "defaultParameters.config.patch")), "write one-unit bucket patch")) return;
  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto root = Root::create(std::move(settings));
  if (!check_result(root, "create private liquid prediction root")) return;
  auto universe = TestUniverse::create(**root, {100, 100});
  if (!check_result(universe, "start isolated test universe")) return;
  auto close = make_scope_exit([&] { check_result((*universe)->close(), "close isolated test universe"); });
  (*universe)->client()->mainPlayer()->log()->setIntroComplete(true);
  if (!check_result((*universe)->warpPlayer(InstanceWorldId("outpost")), "warp to outpost")) return;
  auto world = (*universe)->worldClient();
  if (!fixture.check(bool(world), "get loaded client world")) return;

  // Inject authoritative levels into loaded, unprotected air. Do not tick
  // the server: every acknowledgement below is applied explicitly.
  auto patch = std::make_shared<TileArrayUpdatePacket>();
  NetTile air;
  air.foreground = empty_material_id;
  air.background = empty_material_id;
  air.collision = CollisionKind::None;
  air.dungeonId = no_dungeon_id;
  patch->min = {8, 8};
  if (!check_result(patch->array.set_size({7, 7}, air), "allocate authoritative tile patch")) return;
  constexpr LiquidId water = 1;
  Vec2I const first(10, 11), second(11, 11), tail(12, 11), repeated(11, 12), placement(12, 12);
  std::array const positions{first, second, tail, repeated, placement};
  std::array<std::uint8_t, positions.size()> const levels{96, 192, 64, 160, 128};
  for (std::size_t i = 0; i < positions.size(); ++i) {
    auto local = positions[i] - patch->min;
    patch->array(local[0], local[1]).liquid = {water, levels[i]};
  }
  if (!check_result(world->handleIncomingPackets({patch}), "seed authoritative liquid levels")) return;
  auto const wrap = Vec2I(static_cast<int>(world->geometry().width()), 0);
  world->setClientWindow(RectI::with_size(patch->min, {7, 7}));
  world->setFullBright(true);
  WorldRenderData frame;
  auto render_frame = [&](std::source_location at = std::source_location::current()) {
    return check_result(world->render(frame, 0, std::nullopt, PresentationContext()), "render liquid predictions", at);
  };
  auto expect_liquid = [&](Vec2I pos, LiquidId id, std::uint8_t level, std::source_location at = std::source_location::current()) {
    auto local = pos - frame.tile_min_position;
    if (!fixture.check(local[0] >= 0 && local[1] >= 0
        && static_cast<std::size_t>(local[0]) < frame.tiles.extent(0)
        && static_cast<std::size_t>(local[1]) < frame.tiles.extent(1), "tile is present in the rendered frame", at))
      return;
    auto const& tile = frame.tiles(local[0], local[1]);
    fixture.check(tile.liquidId == id && tile.liquidLevel == level,
        Star::format("tile {},{}: expected liquid {} at level {}, saw liquid {} at level {}",
            pos[0], pos[1], id, level, tile.liquidId, tile.liquidLevel), at);
  };

  if (!check_result(world->collectLiquid({first + wrap, tail}, water), "predict partial bucket") || !render_frame()) return;
  expect_liquid(first, water, levels[0]);
  expect_liquid(tail, water, levels[2]);

  // The first two visits count separately, then the wrapped spelling sees
  // the completed drain. A stale first tile would fill another bucket.
  if (!check_result(world->collectLiquid({first, first, second, first + wrap, tail}, water), "predict bucket with a wrapped duplicate") ||
      !check_result(world->collectLiquid({repeated, repeated + wrap}, water), "predict bucket filled by revisiting its sole tile")) return;
  float const second_remainder = byte_to_float(levels[1]) - (1.0f - byte_to_float(levels[0]) - byte_to_float(levels[0]));
  if (!render_frame()) return;
  expect_liquid(first, empty_liquid_id, 0);
  expect_liquid(second, water, float_to_byte(second_remainder));
  expect_liquid(tail, water, levels[2]);
  expect_liquid(repeated, empty_liquid_id, 0);
  fixture.check(world->liquidLevel(first).level == byte_to_float(levels[0]), "direct liquid query ignores predicted drain");
  fixture.check(world->liquidLevel(second).level == byte_to_float(levels[1]), "direct liquid query ignores predicted remainder");

  constexpr float added = 0.25f;
  TileModificationList const placements{{second, PlaceLiquid{water, added}}, {placement, PlaceLiquid{water, added}}};
  auto failed = world->applyTileModifications(placements, true);
  if (!check_result(failed, "predict placements") ||
      !fixture.check(failed->empty(), "both placements were accepted for prediction") || !render_frame()) return;
  expect_liquid(second, water, float_to_byte(second_remainder + added));
  expect_liquid(placement, water, float_to_byte(byte_to_float(levels[4]) + added));

  // A placement rejection must not discard the earlier collection.
  auto rejected = std::make_shared<TileModificationFailurePacket>(
      TileModificationList{{second + wrap, PlaceLiquid{water, added}}});
  if (!check_result(world->handleIncomingPackets({rejected}), "apply wrapped placement rejection") || !render_frame()) return;
  expect_liquid(second, water, float_to_byte(second_remainder));
  auto retried = world->applyTileModifications({{second, PlaceLiquid{water, added}}}, true);
  if (!check_result(retried, "retry placement") ||
      !fixture.check(retried->empty(), "retried placement was accepted for prediction")) return;

  // A new collection must use the predicted remainder plus the placement,
  // not the authoritative second tile or a delta applied to it twice.
  if (!check_result(world->collectLiquid({second + wrap, tail}, water), "collect from the predicted liquid level")) return;
  float const tail_remainder = byte_to_float(levels[2]) - (1.0f - (second_remainder + added));
  if (!render_frame()) return;
  expect_liquid(second, empty_liquid_id, 0);
  expect_liquid(tail, water, float_to_byte(tail_remainder));

  auto liquid_ack = std::make_shared<TileLiquidUpdatePacket>(second + wrap, LiquidNetUpdate{water, 77});
  auto full_ack = std::make_shared<TileUpdatePacket>();
  full_ack->position = placement;
  full_ack->tile = air;
  full_ack->tile.liquid = {water, 99};
  if (!check_result(world->handleIncomingPackets({liquid_ack, full_ack}), "apply authoritative liquid and full-tile updates") || !render_frame()) return;
  expect_liquid(second, water, 77);
  expect_liquid(placement, water, 99);
});

namespace {

class PresentationLifecycleEntity : public Entity {
public:
  explicit PresentationLifecycleEntity(Root& root) : Entity(root) {}
  EntityType entity_type() const override { return EntityType::Stagehand; }
  Vec2F position() const override { return current_position; }
  std::expected<void, StarException> render_light_sources(RenderCallback& renderer) override {
    ++light_collections;
    if (light)
      return renderer.add_light_source(*light);
    return {};
  }
  static constexpr EntityRenderLayer fixture_layer = render_layer_effect + 101;
  std::expected<void, StarException> render(RenderCallback& renderer) override {
    if (image)
      return renderer.add_drawable(*image, fixture_layer);
    return {};
  }
  RectF meta_bound_box() const override { return RectF::with_size({}, {1, 1}); }
  ClientEntityMode client_entity_mode() const override { return ClientEntityMode::ClientMasterAllowed; }
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
  std::expected<bool, StarException> should_destroy() const override { return dying; }
  std::expected<void, StarException> collect_render_events(RenderCallback& renderer) override {
    ++collections;
    if (next_position)
      current_position = *std::exchange(next_position, std::nullopt);
    if (fail_collection)
      return std::unexpected(StarException("Deliberate presentation lifecycle test failure"));
    if (next_particle) {
      auto emitted = renderer.add_particle(std::move(*next_particle));
      next_particle.reset();
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
  Vec2F current_position{1024, 1024};
  std::optional<Vec2F> next_position;
  std::optional<Drawable> image;
  std::optional<LightSource> light;
  unsigned light_collections = 0;
  bool dying = false;
  bool fail_collection = false;
  std::optional<Particle> next_particle;
  unsigned collections = 0;
  unsigned destructions = 0;
  unsigned uninitializations = 0;
};

}

const GameTestRegistration presentation_removal_lifecycle(
  "SpawnTest", "PresentationRemovalCollectsOnceAndIsolatesSlaveFailures", [](GameTest& fixture) {
    // Failure isolation is the subject here; use a private Root rather than
    // the shared fixture's sink that fails on every expected Error record.
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto settings = fixture.rootSettings();
    // Sampled text has no image processing payload; exercise the hit path
    // with a real textured damage particle rather than an animated stock hit.
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(File::create_directories(overlay));
    ASSERT_TRUE(File::write_file(String(R"({
      "kind":"presentation-directive-test",
      "effects":{"organic":{"hit":{"particles":[[{
        "type":"textured","image":"/rendering/sprites/error.png","timeToLive":2,
        "collidesForeground":false,"collidesLiquid":false
      }]]}}}
    })"), File::relative_to(overlay, "presentation-directive-test.damage")));
    settings.assetSources.push_back(overlay);
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto created_root = Root::create(std::move(settings));
    ASSERT_TRUE(created_root) << created_root.error().what();
    auto& root = **created_root;
    auto universe = TestUniverse::create(root, Vec2U(100, 100));
    ASSERT_TRUE(universe) << universe.error().what();
    // This fixture travels straight to the outpost; the unclothed test player
    // never enters the tutorial where the starter clothes would be assigned.
    (*universe)->client()->mainPlayer()->log()->setIntroComplete(true);
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
    auto dying_id = dying->entityId();
    dying->dying = true;
    ASSERT_TRUE(world->update(dt));
    EXPECT_EQ(dying->collections, 1u);
    EXPECT_EQ(dying->destructions, 1u);
    EXPECT_EQ(dying->uninitializations, 1u);
    EXPECT_FALSE(world->entity(dying_id));
    auto emitter = std::make_shared<PresentationLifecycleEntity>(root);
    emitter->current_position[0] = static_cast<float>(world->geometry().width()) - 1;
    ASSERT_TRUE(world->addEntity(emitter));
    Particle newborn;
    newborn.type = Particle::Type::Text;
    newborn.size = newborn.base_size = 1;
    newborn.string = "presentation-birth-fixture";
    newborn.position = Vec2F(world->clientWindow().center());
    newborn.velocity = newborn.final_velocity = {60, 0};
    newborn.approach = {};
    newborn.ignore_wind = true;
    newborn.collides_foreground = false;
    newborn.collides_liquid = false;
    newborn.time_to_live = 2.0f;
    emitter->next_particle = newborn;
    ASSERT_TRUE(world->update(dt));
    WorldRenderData display;
    auto find_newborn = [&]() -> std::optional<std::reference_wrapper<Particle const>> {
      for (auto const& particle : display.particles)
        if (particle.string == newborn.string)
          return std::cref(particle);
      return {};
    };
    world->setAsyncLighting(false);
    world->setFullBright(true);
    for (float alpha : {0.0f, 0.5f, 1.0f, 0.25f}) {
      ASSERT_TRUE(world->render(display, 0, std::nullopt, PresentationContext(alpha)));
      auto sampled = find_newborn();
      ASSERT_TRUE(sampled);
      EXPECT_EQ(sampled->get().position, newborn.position);
      EXPECT_FLOAT_EQ(sampled->get().time_to_live, 2.0f);
    }
    EXPECT_EQ(emitter->light_collections, 0u);
    world->setFullBright(false);
    emitter->next_position = Vec2F{1, emitter->current_position[1]};
    ASSERT_TRUE(world->update(dt));
    auto sample = world->sample_entity(*emitter, PresentationContext(0.25f));
    EXPECT_FLOAT_EQ(sample.context.interpolation(), 0.25f);
    EXPECT_FLOAT_EQ(sample.offset[0], -1.5f);
    EXPECT_FLOAT_EQ(sample.offset[1], 0.0f);
    PresentationLifecycleEntity replacement(root);
    ASSERT_TRUE(replacement.init(*world, emitter->entityId(), EntityMode::Slave));
    auto replaced = world->sample_entity(replacement, PresentationContext(0.25f));
    EXPECT_EQ(replaced.offset, Vec2F());
    EXPECT_FLOAT_EQ(replaced.context.interpolation(), 1.0f);
    ASSERT_TRUE(replacement.uninit());
    emitter->resetPresentation();
    auto reset = world->sample_entity(*emitter, PresentationContext(0.25f));
    EXPECT_EQ(reset.offset, Vec2F());
    EXPECT_FLOAT_EQ(reset.context.interpolation(), 1.0f);
    ASSERT_TRUE(world->render(display, 0, std::nullopt, PresentationContext()));
    EXPECT_EQ(emitter->light_collections, 1u);
    auto advanced = find_newborn();
    ASSERT_TRUE(advanced);
    auto travel = world->geometry().diff(advanced->get().position, newborn.position);
    EXPECT_NEAR(travel[0], 1.0f, 1e-4f);
    EXPECT_NEAR(travel[1], 0.0f, 1e-4f);
    EXPECT_NEAR(advanced->get().time_to_live, 2.0f - dt, 1e-6f);

    // Pick a dark cell in the real Outpost, not a fixed pixel whose ambient
    // color could mask the two sources after an asset change.
    auto published = world->waitForLighting(std::ref(display));
    ASSERT_TRUE(published && *published);
    auto light_at = [&](Vec2I cell) {
      auto index = cell - display.light_min_position;
      return display.light_map.get(index[0], index[1]);
    };
    auto interior = world->clientWindow().padded(-4);
    std::optional<Vec2I> probe_cell;
    Vec3F baseline = Vec3F::filled(1.0f);
    for (int x = interior.x_min(); x < interior.x_max(); ++x) {
      for (int y = interior.y_min(); y < interior.y_max(); ++y) {
        auto light = light_at({x, y});
        ASSERT_TRUE(light) << light.error().what();
        if (light->max() < baseline.max()) {
          probe_cell = Vec2I(x, y);
          baseline = *light;
        }
      }
    }
    ASSERT_TRUE(probe_cell);
    auto light_position = center_of_tile(*probe_cell);
    Particle flash = newborn;
    flash.string = "particle-light-fixture";
    flash.position = light_position;
    flash.light = Color::rgb(255, 0, 0);
    flash.fade = 2.0f / dt;
    emitter->light = LightSource{.position = light_position, .color = Vec3F(0, 0, 1)};
    emitter->next_particle = flash;
    ASSERT_TRUE(world->update(dt)); // Birth is collected after the tick.
    ASSERT_TRUE(world->update(dt)); // Move and fade the logical particle to clear.

    std::array<Vec3F, 2> colors;
    for (unsigned endpoint = 0; endpoint != colors.size(); ++endpoint) {
      ASSERT_TRUE(world->render(display, 0, std::nullopt, PresentationContext(static_cast<float>(endpoint))));
      auto particle = std::ranges::find(display.particles, flash.string, &Particle::string);
      ASSERT_NE(particle, display.particles.end());
      auto sampled_position = light_position + Vec2F(static_cast<float>(endpoint), 0);
      EXPECT_EQ(particle->position, sampled_position);
      EXPECT_EQ(particle->light, endpoint == 0 ? flash.light : Color::palette_clear);
      auto available = world->waitForLighting(std::ref(display));
      ASSERT_TRUE(available && *available);
      auto light = light_at(*probe_cell);
      ASSERT_TRUE(light) << light.error().what();
      colors[endpoint] = *light;
    }
    EXPECT_GT(colors[0][0], colors[1][0]); // Only the sampled red spread light faded.
    EXPECT_GT(colors[1][2], baseline[2]);  // The co-located blue entity light survived.
    EXPECT_FLOAT_EQ(colors[0][2], colors[1][2]);

    ASSERT_TRUE(world->removeEntity(emitter->entityId(), false));

    auto parameters_data = net_store_visitable_world_parameters(world->currentTemplate()->worldParameters());
    ASSERT_TRUE(parameters_data);
    auto parameters = net_load_visitable_world_parameters(std::move(*parameters_data));
    ASSERT_TRUE(parameters && *parameters);
    auto metadata = root.image_metadata_database();
    ASSERT_TRUE(metadata);
    auto image = Drawable::make_image(**metadata, "/rendering/sprites/error.png", 1.0f, false, newborn.position);
    ASSERT_TRUE(image);
    List<Directives> choices{Directives("?flipx"), Directives("?flipy"), Directives("?hueshift=27")};
    for (unsigned mode = 0; mode != 3; ++mode) {
      auto& options = (*parameters)->globalDirectives;
      options.reset();
      if (mode == 1)
        options.emplace();
      if (mode == 2)
        options = choices;
      auto encoded = net_store_visitable_world_parameters(*parameters);
      ASSERT_TRUE(encoded);
      ASSERT_TRUE(world->handleIncomingPackets({std::make_shared<WorldParametersUpdatePacket>(std::move(*encoded))}));
      auto probe = std::make_shared<PresentationLifecycleEntity>(root);
      probe->image = *image;
      ASSERT_TRUE(world->addEntity(probe));
      auto expected_directive = [&](EntityId id) {
        return mode == 2 ? choices[static_cast<unsigned>(id) % choices.size()].string() : String();
      };
      auto expected = expected_directive(probe->entityId());
      AssetPath tick_marker(std::string_view{Star::format("/rendering/sprites/error.png?hueshift={}", 10 + mode)});
      AssetPath removal_marker(std::string_view{Star::format("/rendering/sprites/error.png?hueshift={}", 20 + mode)});
      probe->next_particle = newborn;
      probe->next_particle->type = Particle::Type::Textured;
      probe->next_particle->image = tick_marker;
      auto damage = std::make_shared<DamageNotificationPacket>();
      auto& hit = damage->remote_damage_notification.damage_notification;
      hit.target_entity_id = -19;
      hit.position = newborn.position + Vec2F{static_cast<float>(mode) * 4, 0};
      hit.damage_source_kind = "presentation-directive-test";
      hit.target_material_kind = "organic";
      ASSERT_TRUE(world->handleIncomingPackets({damage}));
      ASSERT_TRUE(world->update(dt));
      ASSERT_TRUE(world->render(display, 0, std::nullopt, PresentationContext()));
      bool saw_image = false;
      for (auto const& entity : display.entity_drawables) {
        for (auto const& [layer, drawables] : entity.layers) {
          if (layer != probe->fixture_layer || drawables.empty())
            continue;
          ASSERT_EQ(drawables.size(), 1u);
          EXPECT_EQ(drawables.front().image_part().image.directives.to_string(), expected);
          saw_image = true;
        }
      }
      EXPECT_TRUE(saw_image);
      probe->next_particle = newborn;
      probe->next_particle->type = Particle::Type::Textured;
      probe->next_particle->image = removal_marker;
      ASSERT_TRUE(world->removeEntity(probe->entityId(), true));
      ASSERT_TRUE(world->render(display, 0, std::nullopt, PresentationContext()));
      bool saw_tick = false, saw_removal = false, saw_hit = false;
      for (auto const& particle : display.particles) {
        if (particle.type != Particle::Type::Textured)
          continue;
        if (particle.image == tick_marker || particle.image == removal_marker) {
          EXPECT_EQ(particle.directives.to_string(), expected);
          saw_tick |= particle.image == tick_marker;
          saw_removal |= particle.image == removal_marker;
        } else if (particle.image.base_path == "/rendering/sprites/error.png" && particle.image.directives.empty()
            && world->geometry().diff(particle.position, hit.position).magnitude() < 0.5f) {
          EXPECT_EQ(particle.directives.to_string(), expected_directive(hit.target_entity_id));
          saw_hit = true;
        }
      }
      EXPECT_TRUE(saw_tick);
      EXPECT_TRUE(saw_removal);
      EXPECT_TRUE(saw_hit);
    }

    // Exercise the real incoming death-packet path, including its optional
    // interpolation timer. Every intervening live tick and the final removal
    // may collect once, never both regular and teardown passes in one tick.
    for (bool fail_collection : {false, true}) {
      auto slave = std::make_shared<PresentationLifecycleEntity>(root);
      slave->fail_collection = fail_collection;
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
    master->fail_collection = true;
    ASSERT_TRUE(world->addEntity(master));
    ASSERT_TRUE(master->promote(*world));
    auto master_id = master->entityId();
    EXPECT_FALSE(world->removeEntity(master_id, true));
    EXPECT_TRUE(world->entity(master_id));
    EXPECT_EQ(master->destructions, 0u);
    EXPECT_EQ(master->uninitializations, 0u);
    ASSERT_TRUE(world->removeEntity(master_id, false));
    ASSERT_TRUE((*universe)->close());
  });

const GameTestRegistration detached_plant_impact("PlantDrop", "FinalSparseImpactContinuesAfterDeathPacketWithoutLateReplay", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directory(overlay));
  constexpr unsigned width_in_tiles = 8;
  Image sparse(width_in_tiles * tile_pixels, tile_pixels, PixelFormat::RGBA32);
  for (unsigned y = 0; y < tile_pixels; ++y)
    for (unsigned x = 0; x < width_in_tiles * tile_pixels; ++x)
      ASSERT_TRUE(sparse.set(x, y, Vec4B(255, 255, 255, x < tile_pixels ? 255 : 0)));
  auto png = std::make_shared<Buffer>();
  ASSERT_TRUE(sparse.write_png(fixture.root().logger(), png));
  constexpr auto image_path = "/plant-retired-sparse.png";
  ASSERT_TRUE(File::write_file(png->take_data(), File::relative_to(overlay, "plant-retired-sparse.png")));

  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  settings.mix_random_entropy = false;
  auto created_root = Root::create(std::move(settings));
  ASSERT_TRUE(created_root) << created_root.error().what();
  Root& root = **created_root;
  auto universe = TestUniverse::create(root, Vec2U(100, 100));
  ASSERT_TRUE(universe) << universe.error().what();
  (*universe)->client()->mainPlayer()->log()->setIntroComplete(true);
  ASSERT_TRUE((*universe)->warpPlayer(InstanceWorldId("outpost")));
  ASSERT_TRUE((*universe)->waitForWorldReady());
  auto world = (*universe)->worldClient();
  ASSERT_TRUE(world);
  world->setAsyncLighting(false);
  world->setFullBright(true);
  constexpr float dt = 1.0f / 60;
  constexpr EntityId live_id = 16319;
  constexpr EntityId late_id = 16320;
  Vec2F origin(world->clientWindow().center());

  Plant::PlantPiece piece;
  piece.image = String(std::string_view(image_path));
  piece.kind = Plant::Stem;
  piece.spaces.add({0, 0});
  piece.structural_segment = true;
  String marker = "plant-detached-impact-test";
  Json particle = JsonObject{{"type", "text"}, {"text", marker}, {"timeToLive", 10}, {"ignoreWind", true},
      {"collidesForeground", false}, {"collidesLiquid", false}};
  Json stem = JsonObject{{"particles", JsonObject{{"hitGround", JsonObject{{"options", JsonArray{particle}}, {"density", 2}}}}}};
  auto source = PlantDrop::create(root, {piece}, origin, {}, "retired tree", true, stem, {}, {}, false, 0);
  ASSERT_TRUE(source);
  ASSERT_TRUE((*source)->init(*world, live_id, EntityMode::Master));
  auto store = (*source)->net_store();
  auto baseline = (*source)->write_net_state();
  ASSERT_TRUE(store && baseline);
  ASSERT_TRUE(world->handleIncomingPackets({std::make_shared<EntityCreatePacket>(
      EntityType::PlantDrop, std::move(*store), std::move(baseline->first), live_id)}));
  auto live = world->entity(live_id);
  ASSERT_TRUE(live);
  for (unsigned tick = 0; tick < 30; ++tick)
    ASSERT_TRUE(world->update(dt));

  ASSERT_TRUE((*source)->update(5001, 1));
  auto final = (*source)->write_net_state(baseline->second);
  auto late_store = (*source)->net_store();
  auto late_state = (*source)->snapshot_net_state();
  ASSERT_TRUE(final && late_store && late_state);
  ASSERT_TRUE((*source)->uninit());
  root.random().seed(0x51a7710);
  ASSERT_TRUE(world->handleIncomingPackets({std::make_shared<EntityDestroyPacket>(live_id, std::move(final->first), true)}));
  for (unsigned tick = 0; tick < 120 && world->entity(live_id); ++tick)
    ASSERT_TRUE(world->update(dt));
  ASSERT_FALSE(world->entity(live_id));
  ASSERT_FALSE(live->inWorld());

  WorldRenderData display;
  auto particle_count = [&] { return std::ranges::count(display.particles, marker, &Particle::string); };
  ASSERT_TRUE(world->render(display, 0, std::nullopt, PresentationContext()));
  EXPECT_LT(particle_count(), 4);
  for (unsigned tick = 0; tick < 120 && particle_count() < 4; ++tick) {
    ASSERT_TRUE(world->update(dt));
    ASSERT_TRUE(world->render(display, 0, std::nullopt, PresentationContext()));
  }
  ASSERT_EQ(particle_count(), 4);

  ASSERT_TRUE(world->handleIncomingPackets({std::make_shared<EntityCreatePacket>(
      EntityType::PlantDrop, std::move(*late_store), std::move(late_state->first), late_id)}));
  ASSERT_TRUE(world->entity(late_id));
  for (unsigned tick = 0; tick < 30; ++tick)
    ASSERT_TRUE(world->update(dt));
  ASSERT_TRUE(world->handleIncomingPackets({std::make_shared<EntityDestroyPacket>(late_id, ByteArray{}, true)}));
  for (unsigned tick = 0; tick < 120; ++tick)
    ASSERT_TRUE(world->update(dt));
  ASSERT_FALSE(world->entity(late_id));
  ASSERT_TRUE(world->render(display, 0, std::nullopt, PresentationContext()));
  EXPECT_EQ(particle_count(), 4);
  ASSERT_TRUE((*universe)->close());
});
