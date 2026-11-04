import star.json;
import star.xx_hash;
import star.id_map;
import star.string;
import star.lock_file;
import star.thread;
import star.game_types;
import star.either;
import star.data_stream;
import star.rpc_promise;
import star.io_device;
import star.byte_array;
import star.data_stream_devices;

import std;
typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.set;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.btree_database;
import star.casting;
import star.ordered_set;
import star.list;
import star.bi_map;
import star.version;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.image;
import star.map;
import star.ref_ptr;
import star.logging;
import star.listener;

import star.lua_root;
import star.universe;
import star.celestial_coordinate;
import star.host_address;
import star.uuid;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.tile_modification;
import star.world;
import star.celestial_types;
import star.chat_types;
import star.warping;
import star.wiring;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.cellular_light_array;
import star.cellular_lighting;
import star.cellular_liquid;
import star.world_structure;
import star.lua_components;
import star.drawable;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.world_render_data;
import star.world_client_state;
import star.interpolation_tracker;
import star.spawn_type_database;
import star.spawner;
import star.world_server;
import star.world_server_thread;
import star.world_template;
import star.system_world_server;
import star.system_world_server_thread;
import star.socket;
import star.tcp;
import star.p2p_networking_service;
import star.net_packet_socket;
import star.universe_connection;
import star.universe_settings;
import star.universe_server;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.game_test;
import star.file;
import star.algorithm;
import star.vector;
import star.json_extra;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

namespace Star {

struct WorldServerTestAccess {
  using Timing = WorldServer::UpdateTiming;
  using Limit = WorldServer::ProcessingLimit;

  static void setConfiguration(WorldServer& world, Json settings) {
    auto updated = world.m_serverConfig.set("fidelitySettings", std::move(settings));
    ASSERT_TRUE(updated) << updated.error().what();
    world.m_serverConfig = std::move(*updated);
  }

  static std::expected<std::optional<unsigned>, StarException> schedule(WorldServer& world, Timing timing, std::uint64_t step) {
    world.m_currentStep = step;
    return world.shouldRunThisStep(timing);
  }

  static std::expected<std::optional<std::uint64_t>, StarException> limit(WorldServer& world, Limit limit) {
    return world.processingLimit(limit);
  }
};

}

namespace {
std::expected<std::shared_ptr<WorldServer>, StarException> createSchedulingWorld(Root& root) {
  auto storage = File::ephemeralFile(root.random());
  if (!storage)
    return std::unexpected(std::move(storage.error()));
  return WorldServer::create(root, Vec2U(2048, 2048), *storage);
}
}// namespace

const GameTestRegistration serverRun("ServerTest", "Run", [](GameTest& fixture) {
  auto server = UniverseServer::create(fixture.root(), fixture.root().toStoragePath("universe"));
  ASSERT_TRUE(server) << server.error().what();
  auto started = (*server)->start();
  ASSERT_TRUE(started) << started.error().what();
  auto stopped = (*server)->stop();
  EXPECT_TRUE(stopped) << (stopped ? "" : stopped.error().what());
  auto joined = (*server)->join();
  ASSERT_TRUE(joined) << joined.error().what();
});

const GameTestRegistration
  fidelityScheduleIntervalsAndOffsets("ServerTest", "FidelityScheduleIntervalsAndOffsets", [](GameTest& fixture) {
    auto created = createSchedulingWorld(fixture.root());
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto close = finally([&] { EXPECT_TRUE(world->close()); });
    using Timing = WorldServerTestAccess::Timing;
    constexpr std::array<std::string_view, 7> names{"wiringUpdate",      "liquidUpdate",     "fallingBlocksUpdate",
                                                    "blockDamageUpdate", "worldStorageTick", "worldStorageGenerate",
                                                    "environmentUpdate"};
    JsonObject medium;
    for (std::size_t i = 0; i < names.size(); ++i)
      medium[names[i]] = JsonArray{Json(i + 2), Json(i + 1)};
    WorldServerTestAccess::setConfiguration(*world, JsonObject{{"medium", std::move(medium)}});

    // The production thread selects fidelity each tick, even while paused.
    // Each configured interval is returned unchanged for elapsed-time consumers.
    for (std::uint64_t step = 0; step < 32; ++step) {
      ASSERT_TRUE(world->setFidelity(WorldServerFidelity::Medium));
      for (std::size_t i = 0; i < names.size(); ++i) {
        auto scheduled = WorldServerTestAccess::schedule(*world, static_cast<Timing>(i), step);
        ASSERT_TRUE(scheduled) << scheduled.error().what();
        std::optional<unsigned> expected;
        if ((step + i + 1) % (i + 2) == 0)
          expected = static_cast<unsigned>(i + 2);
        EXPECT_EQ(*scheduled, expected) << names[i] << " step " << step;
      }
    }

    // Keep the original unsigned step/offset wraparound arithmetic.
    auto step = std::numeric_limits<std::uint64_t>::max();
    auto scheduled = WorldServerTestAccess::schedule(*world, Timing::Wiring, step);
    ASSERT_TRUE(scheduled) << scheduled.error().what();
    EXPECT_EQ(*scheduled, std::optional<unsigned>(2));
  });

const GameTestRegistration
  fidelityScheduleConfigurationChanges("ServerTest", "FidelityScheduleConfigurationChanges", [](GameTest& fixture) {
    auto created = createSchedulingWorld(fixture.root());
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto close = finally([&] { EXPECT_TRUE(world->close()); });
    using Timing = WorldServerTestAccess::Timing;
    using Limit = WorldServerTestAccess::Limit;
    auto settings = Json::parse(R"({
    "medium": {"blockDamageUpdate":[3,1], "worldStorageTick":[5,2],
      "liquidEngineBackgroundProcessingLimit":7},
    "high": {"blockDamageUpdate":[2,0], "worldStorageTick":[4,0],
      "liquidEngineBackgroundProcessingLimit":11, "worldStorageGenerationLevelLimit":13}
  })");
    ASSERT_TRUE(settings) << settings.error().what();
    WorldServerTestAccess::setConfiguration(*world, *settings);
    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::Medium));
    auto damage = WorldServerTestAccess::schedule(*world, Timing::BlockDamage, 2);
    auto storageTick = WorldServerTestAccess::schedule(*world, Timing::StorageTick, 3);
    ASSERT_TRUE(damage);
    ASSERT_TRUE(storageTick);
    EXPECT_EQ(*damage, std::optional<unsigned>(3));
    EXPECT_EQ(*storageTick, std::optional<unsigned>(5));
    auto liquid = WorldServerTestAccess::limit(*world, Limit::Liquid);
    auto generation = WorldServerTestAccess::limit(*world, Limit::StorageGenerate);
    ASSERT_TRUE(liquid);
    ASSERT_TRUE(generation);
    EXPECT_EQ(*liquid, std::optional<std::uint64_t>(7));
    EXPECT_FALSE(*generation);// Missing optional limits are successful cached values.

    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::High));
    damage = WorldServerTestAccess::schedule(*world, Timing::BlockDamage, 2);
    storageTick = WorldServerTestAccess::schedule(*world, Timing::StorageTick, 3);
    liquid = WorldServerTestAccess::limit(*world, Limit::Liquid);
    generation = WorldServerTestAccess::limit(*world, Limit::StorageGenerate);
    ASSERT_TRUE(damage);
    ASSERT_TRUE(storageTick);
    ASSERT_TRUE(liquid);
    ASSERT_TRUE(generation);
    EXPECT_EQ(*damage, std::optional<unsigned>(2));
    EXPECT_FALSE(*storageTick);
    EXPECT_EQ(*liquid, std::optional<std::uint64_t>(11));
    EXPECT_EQ(*generation, std::optional<std::uint64_t>(13));

    // Replacing configuration without changing fidelity must invalidate both
    // schedule and optional-limit values, including cached absence.
    WorldServerTestAccess::setConfiguration(*world,
                                            JsonObject{{"high",
                                                        JsonObject{{"blockDamageUpdate", JsonArray{5, 1}},
                                                                   {"worldStorageTick", JsonArray{3, 0}},
                                                                   {"worldStorageGenerationLevelLimit", 17}}}});
    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::High));
    damage = WorldServerTestAccess::schedule(*world, Timing::BlockDamage, 4);
    storageTick = WorldServerTestAccess::schedule(*world, Timing::StorageTick, 3);
    liquid = WorldServerTestAccess::limit(*world, Limit::Liquid);
    generation = WorldServerTestAccess::limit(*world, Limit::StorageGenerate);
    ASSERT_TRUE(damage);
    ASSERT_TRUE(storageTick);
    ASSERT_TRUE(liquid);
    ASSERT_TRUE(generation);
    EXPECT_EQ(*damage, std::optional<unsigned>(5));
    EXPECT_EQ(*storageTick, std::optional<unsigned>(3));
    EXPECT_FALSE(*liquid);
    EXPECT_EQ(*generation, std::optional<std::uint64_t>(17));
  });

const GameTestRegistration
  fidelityScheduleLazyErrors("ServerTest", "FidelityScheduleLazyErrors", [](GameTest& fixture) {
    auto created = createSchedulingWorld(fixture.root());
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto close = finally([&] { EXPECT_TRUE(world->close()); });
    using Timing = WorldServerTestAccess::Timing;
    using Limit = WorldServerTestAccess::Limit;
    auto settings = Json::parse(R"({
    "medium": {"wiringUpdate":[3,1], "liquidUpdate":[2,0],
      "environmentUpdate":"bad timing", "liquidEngineBackgroundProcessingLimit":false,
      "worldStorageGenerationLevelLimit":"bad limit"}
  })");
    ASSERT_TRUE(settings) << settings.error().what();
    WorldServerTestAccess::setConfiguration(*world, *settings);
    for (unsigned tick = 0; tick < 3; ++tick)
      ASSERT_TRUE(world->setFidelity(WorldServerFidelity::Medium));
    auto wiring = WorldServerTestAccess::schedule(*world, Timing::Wiring, 2);
    ASSERT_TRUE(wiring);
    EXPECT_EQ(*wiring, std::optional<unsigned>(3));
    auto liquidSkipped = WorldServerTestAccess::schedule(*world, Timing::Liquid, 1);
    ASSERT_TRUE(liquidSkipped);
    EXPECT_FALSE(*liquidSkipped);

    // Selecting fidelity and using valid fields must not validate unused ones.
    // Each invalid field fails on first actual use, and failures remain retryable.
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
      auto environment = WorldServerTestAccess::schedule(*world, Timing::Environment, attempt);
      auto liquid = WorldServerTestAccess::limit(*world, Limit::Liquid);
      auto generation = WorldServerTestAccess::limit(*world, Limit::StorageGenerate);
      ASSERT_FALSE(environment);
      ASSERT_FALSE(liquid);
      ASSERT_FALSE(generation);
      EXPECT_TRUE(environment.error().is<JsonException>());
      EXPECT_TRUE(liquid.error().is<JsonException>());
      EXPECT_TRUE(generation.error().is<JsonException>());
    }

    // A malformed selected configuration retains the old setter error timing:
    // selecting it succeeds; accessing its first field fails.
    WorldServerTestAccess::setConfiguration(*world, JsonObject{{"medium", false}});
    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::Medium));
    auto firstUse = WorldServerTestAccess::schedule(*world, Timing::Wiring, 0);
    ASSERT_FALSE(firstUse);
    EXPECT_TRUE(firstUse.error().is<JsonException>());

    WorldServerTestAccess::setConfiguration(*world,
                                            JsonObject{{"medium",
                                                        JsonObject{{"environmentUpdate", JsonArray{4, 1}},
                                                                   {"liquidEngineBackgroundProcessingLimit", 19},
                                                                   {"worldStorageGenerationLevelLimit", Json()}}}});
    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::Medium));
    auto repaired = WorldServerTestAccess::schedule(*world, Timing::Environment, 3);
    auto repairedLiquid = WorldServerTestAccess::limit(*world, Limit::Liquid);
    auto repairedGeneration = WorldServerTestAccess::limit(*world, Limit::StorageGenerate);
    ASSERT_TRUE(repaired);
    ASSERT_TRUE(repairedLiquid);
    ASSERT_TRUE(repairedGeneration);
    EXPECT_EQ(*repaired, std::optional<unsigned>(4));
    EXPECT_EQ(*repairedLiquid, std::optional<std::uint64_t>(19));
    EXPECT_FALSE(*repairedGeneration);
  });

const GameTestRegistration
  fidelityScheduleFirstErrorOrder("ServerTest", "FidelityScheduleFirstErrorOrder", [](GameTest& fixture) {
    auto created = createSchedulingWorld(fixture.root());
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto close = finally([&] { EXPECT_TRUE(world->close()); });
    world->setSpawningEnabled(false);
    unsigned triggered = 0;
    world->timer(0, [&](World&) -> std::expected<void, StarException> {
      ++triggered;
      return {};
    });

    JsonObject medium{{"liquidUpdate", "bad timing"}, {"liquidEngineBackgroundProcessingLimit", false}};
    WorldServerTestAccess::setConfiguration(*world, JsonObject{{"medium", medium}});
    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::Medium));
    auto missingWiring = world->update(0.025f);
    auto expectedWiring = Json(medium).get("wiringUpdate");
    ASSERT_FALSE(missingWiring);
    ASSERT_FALSE(expectedWiring);
    EXPECT_STREQ(missingWiring.error().what(), expectedWiring.error().what());
    EXPECT_EQ(world->currentStep(), 1u);
    EXPECT_EQ(triggered, 1u);// Earlier timer effects survive the scheduling failure.

    medium["wiringUpdate"] = JsonArray{1, 0};
    WorldServerTestAccess::setConfiguration(*world, JsonObject{{"medium", medium}});
    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::Medium));
    auto malformedLiquid = world->update(0.025f);
    auto expectedLiquid = jsonToVec2U(Json("bad timing"));
    ASSERT_FALSE(malformedLiquid);
    ASSERT_FALSE(expectedLiquid);
    EXPECT_STREQ(malformedLiquid.error().what(), expectedLiquid.error().what());
    EXPECT_EQ(world->currentStep(), 2u);
    EXPECT_EQ(triggered, 1u);

    medium["liquidUpdate"] = JsonArray{1, 0};
    WorldServerTestAccess::setConfiguration(*world, JsonObject{{"medium", medium}});
    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::Medium));
    auto malformedLimit = world->update(0.025f);
    auto expectedLimit = Json(false).optUInt();
    ASSERT_FALSE(malformedLimit);
    ASSERT_FALSE(expectedLimit);
    EXPECT_STREQ(malformedLimit.error().what(), expectedLimit.error().what());
    EXPECT_EQ(world->currentStep(), 3u);

    // On step 4 this liquid interval does not run: its invalid limit must not
    // mask the missing falling-block timing that is actually used next.
    medium["liquidUpdate"] = JsonArray{3, 0};
    WorldServerTestAccess::setConfiguration(*world, JsonObject{{"medium", medium}});
    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::Medium));
    auto missingFalling = world->update(0.025f);
    auto expectedFalling = Json(medium).get("fallingBlocksUpdate");
    ASSERT_FALSE(missingFalling);
    ASSERT_FALSE(expectedFalling);
    EXPECT_STREQ(missingFalling.error().what(), expectedFalling.error().what());
    EXPECT_EQ(world->currentStep(), 4u);
    EXPECT_EQ(triggered, 1u);
  });
