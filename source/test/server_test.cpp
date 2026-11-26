import star.json;
import star.xx_hash;
import star.id_map;
import star.string;
import star.thread;
import star.game_types;
import star.data_stream;
import star.rpc_promise;
import star.io_device;
import star.byte_array;
import star.data_stream_devices;
import star.data_stream_extra;

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
import star.buffer;
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
import star.entity_map;
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
import star.plant_drop;
import star.plant_database;
import star.biome;
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
import star.entity_rendering;
import star.entity_rendering_types;
import star.mixer;
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
import star.command_processor;
import star.message_types;
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
import star.falling_blocks_agent;
import star.material_types;
import star.material_database;
import star.world_generation;
import star.item;
import star.item_drop;
import star.rect;
import star.object;
import star.object_database;
import star.format;
import star.game_timers;

#include "gtest/gtest.h"

using namespace Star;

namespace Star {

struct WorldServerTestAccess {
  using Timing = WorldServer::UpdateTiming;
  using Limit = WorldServer::ProcessingLimit;

  static auto limit_entity_ids(WorldServer& world, EntityId first, EntityId last) {
    auto original = std::exchange(world.m_entityMap, std::make_shared<EntityMap>(world.geometry().size(), first, last));
    return make_scope_exit([&world, original = std::move(original)]() mutable {
      world.m_entityMap = std::move(original);
    });
  }

  static void start_break_clock(WorldServer& world) {
    world.m_current_time = 120;
    world.m_tile_entity_break_check_timer = GameTimer(0.125f);
  }

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

  static WorldStorage& world_storage(WorldServer& world) {
    return *world.m_worldStorage;
  }

  static std::expected<void, StarException> generate_queued_sectors(WorldServer& world) {
    return world.m_worldStorage->generateQueue({});
  }

  static std::expected<void, StarException> unload_sectors(WorldServer& world) {
    return world.m_worldStorage->unloadAll(true);
  }
};

}

namespace {
std::expected<std::shared_ptr<WorldServer>, StarException> createSchedulingWorld(Root& root) {
  auto storage = File::ephemeral_file(root.random());
  if (!storage)
    return std::unexpected(std::move(storage.error()));
  return WorldServer::create(root, Vec2U(2048, 2048), *storage);
}

class BreakCountingChair : public Object {
public:
  explicit BreakCountingChair(Root& root) : Entity(root), Object(root) {}
  using Object::initialize;

  std::expected<bool, StarException> check_broken() override {
    ++checks;
    return Object::check_broken();
  }

  unsigned checks = 0;
};
}// namespace

const GameTestRegistration server_break_clock("GameTimers", "GlobalBreakRequestsFollowElapsedSteps", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto world = createSchedulingWorld(root).value();
  auto close = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
  Vec2I const pos(1024, 1024);
  world->generateRegion(RectI::with_size(pos - Vec2I(40, 40), {80, 80})).value();
  for (int x = -3; x <= 3; ++x) {
    auto& floor = world->modifyServerTile(pos + Vec2I(x, -1)).value().value().get();
    floor.foreground = structure_material_id;
    floor.update_collision(CollisionKind::Block);
  }
  auto chair = std::make_shared<BreakCountingChair>(root);
  chair->initialize(root.object_database().value()->get_config("woodenchair").value(), JsonObject()).value();
  chair->set_tile_position(pos).value();
  world->addEntity(chair).value();
  WorldServerTestAccess::start_break_clock(*world);
  auto const before = chair->checks;
  for (unsigned step = 1; step <= 12; ++step) {
    // Idle for two steps: the periodic clock must still advance.
    if (step > 2) world->request_global_break_check();
    world->update(0.03125f).value();
    EXPECT_EQ(chair->checks - before, step / 4) << step;
  }
});

const GameTestRegistration plant_felling("PlantFelling", "WrappedTiesAndRetryAfterSecondPublicationFails", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directories(overlay));
  auto image = std::make_shared<Buffer>();
  ASSERT_TRUE(Image::filled({tile_pixels, tile_pixels}, {255, 255, 255, 255}).write_png(fixture.root().logger(), image));
  ASSERT_TRUE(File::write_file(image->take_data(), File::relative_to(overlay, "plant-felling-tile.png")));

  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  settings.mix_random_entropy = false;
  auto root = Root::create(std::move(settings));
  ASSERT_TRUE(root) << root.error().what();
  auto created = createSchedulingWorld(**root);
  ASSERT_TRUE(created) << created.error().what();
  auto& world = **created;
  auto close = make_scope_exit([&] { EXPECT_TRUE(world.close()); });
  ASSERT_TRUE(world.generateRegion(RectI::with_size({-4, 1018}, {16, 28})));

  // Use native ID exhaustion, not a stubbed addEntity: the standing tree and
  // two blockers occupy three of four slots, so the second drop is rejected.
  auto capacity = WorldServerTestAccess::limit_entity_ids(world, 500, 503);
  auto remove_probes = make_scope_exit([&] {
    for (auto id : world.entity_ids())
      EXPECT_TRUE(world.removeEntity(id, false));
  });
  auto piece = [](Vec2F const& offset, int segment, bool structural) {
    return JsonObject{{"image", "/plant-felling-tile.png"}, {"offset", json_from_vec2f(offset)},
        {"rotationType", "dontRotate"}, {"rotationOffset", 0}, {"structuralSegment", structural},
        {"segmentIdx", segment}, {"kind", structural ? Plant::Stem : Plant::Foliage}, {"flip", false}};
  };
  auto make_plant = [&](Vec2I const& origin, bool ceiling, JsonArray pieces) {
    return Plant::create(**root, JsonObject{{"tilePosition", json_from_vec2i(origin)}, {"ceiling", ceiling},
        {"descriptions", JsonObject{{"description", "Felling probe"}}}, {"ephemeral", false},
        {"stemDropConfig", JsonObject{}}, {"foliageDropConfig", JsonObject{}}, {"saplingDropConfig", Json()},
        {"tileDamageParameters", JsonObject{{"damageFactors", JsonObject{{"plantish", 1}}}, {"damageRecovery", 0}}},
        {"fallsWhenDead", true}, {"pieces", std::move(pieces)}});
  };
  auto standing_offsets = [](Plant const& plant) {
    List<Vec2F> offsets;
    for (auto const& piece : plant.pieces())
      offsets.push_back(piece.offset);
    return offsets;
  };
  auto expect_drop = [&](std::shared_ptr<PlantDrop> const& drop, Vec2F const& origin, bool first,
      List<Vec2F> const& offsets, RectF const& bounds, RectF const& stem_bounds) {
    ASSERT_TRUE(drop);
    EXPECT_EQ(world.geometry().diff(drop->position(), origin), Vec2F());
    auto wire = drop->net_store();
    ASSERT_TRUE(wire) << wire.error().what();
    DataStreamReader stream(wire->span());
    ASSERT_TRUE(stream.read<float>()); // Timeout.
    EXPECT_EQ(stream.read<bool>(), first);
    ASSERT_TRUE(stream.read<String>()); // Description.
    EXPECT_EQ(stream.read<RectF>(), bounds);
    EXPECT_EQ(stream.read<RectF>(), stem_bounds);
    ASSERT_TRUE(stream.read<float>()); // Rotation rate.
    List<Vec2F> actual;
    auto read_offsets = stream.read_container(actual, [](DataStream& stream, Vec2F& offset) -> std::expected<void, StarException> {
      if (auto image = stream.read<AssetPath>(); !image) return std::unexpected(std::move(image.error()));
      if (auto x = stream.read(offset[0]); !x) return x;
      if (auto y = stream.read(offset[1]); !y) return y;
      if (auto flip = stream.read<bool>(); !flip) return std::unexpected(std::move(flip.error()));
      if (auto kind = stream.read<Plant::PlantPieceKind>(); !kind) return std::unexpected(std::move(kind.error()));
      return {};
    });
    ASSERT_TRUE(read_offsets) << read_offsets.error().what();
    EXPECT_EQ(actual, offsets);
  };

  auto floor = make_plant({0, 1024}, false, {piece({0, 0}, 0, true), piece({0, 2}, 8, true), piece({2, 3}, 6, true),
      piece({-1, 2}, 3, true), piece({1, 3}, 8, false), piece({1, 1}, 0, false), piece({-2, 3}, 3, false),
      piece({3, 4}, 6, false)});
  auto blocker = make_plant({8, 1024}, false, {piece({0, 0}, 0, true)});
  auto second_blocker = make_plant({9, 1024}, false, {piece({0, 0}, 0, true)});
  ASSERT_TRUE(floor) << floor.error().what();
  ASSERT_TRUE(blocker) << blocker.error().what();
  ASSERT_TRUE(second_blocker) << second_blocker.error().what();
  for (auto const& position : {Vec2I(0, 1023), Vec2I(8, 1023), Vec2I(9, 1023), Vec2I(0, 1041)}) {
    auto support = world.modifyServerTile(position);
    ASSERT_TRUE(support && *support);
    (*support)->get().foreground = structure_material_id;
  }
  ASSERT_TRUE(world.addEntity(*floor, 500));
  ASSERT_TRUE(world.addEntity(*blocker, 502));
  ASSERT_TRUE(world.addEntity(*second_blocker, 503));

  constexpr std::uint64_t seed = 271828;
  (*root)->random().seed(seed);
  RandomService oracle(seed);
  ASSERT_TRUE(oracle.uniform_float(-0.3f, 0.3f));
  TileDamage const lethal(TileDamageType::Plantish, 2.0f);
  // Different canonical X positions at the same height, with the seam target
  // appearing before the other hit. Floor chooses the last piece-space match.
  auto failed = (*floor)->damage_tiles({{17, 1029}, {2047, 1026}, {0, 1026}, {-1, 1026}}, {2.0f, 1026.0f}, lethal);
  ASSERT_FALSE(failed);
  EXPECT_NE(std::string_view(failed.error().what()).find("No more entity id space"), std::string_view::npos);
  auto first = as<PlantDrop>(world.entity(501));
  expect_drop(first, {-0.5f, 1026.0f}, true, {{-0.5f, 0}, {-1.5f, 1}},
      RectF({-1, 0}, {1, 2}), RectF({0, 0}, {1, 1}));
  EXPECT_EQ(standing_offsets(**floor), (List<Vec2F>{{0, 0}, {0, 2}, {2, 3}, {1, 3}, {1, 1}, {3, 4}}));
  EXPECT_EQ((*floor)->spaces().contains({-1, 2}), false);
  EXPECT_EQ((*floor)->spaces().contains({0, 2}), true);
  EXPECT_EQ((*floor)->should_destroy(), false);
  EXPECT_FALSE((*floor)->check_broken()); // Still out of IDs; the same cut is pending.
  EXPECT_EQ(world.entity(501), first);

  ASSERT_TRUE(world.removeEntity(502, false));
  ASSERT_TRUE(world.removeEntity(503, false));
  ASSERT_TRUE((*floor)->update(0.0f, 1)); // No new hit or break request.
  EXPECT_EQ(world.entity(501), first);
  expect_drop(as<PlantDrop>(world.entity(502)), {-0.5f, 1026.0f}, false, {{2.5f, 1}, {3.5f, 2}},
      RectF({3, 1}, {5, 3}), RectF({3, 1}, {4, 2}));
  // The last group is constructed only after retry, using remapped indices.
  expect_drop(as<PlantDrop>(world.entity(503)), {-0.5f, 1026.0f}, false, {{0.5f, 0}, {1.5f, 1}},
      RectF({1, 0}, {3, 2}), RectF({1, 0}, {2, 1}));
  EXPECT_EQ(standing_offsets(**floor), (List<Vec2F>{{0, 0}, {1, 1}}));
  EXPECT_EQ((*floor)->roots(), (List<Vec2I>{{0, -1}}));
  EXPECT_EQ((*floor)->check_broken(), false);
  EXPECT_EQ((*root)->random().next_u64(), oracle.next_u64());

  // Root loss falls from the bottom of the remaining ground piece.
  ASSERT_TRUE(world.removeEntity(501, false));
  auto removed_support = world.modifyServerTile({0, 1023});
  ASSERT_TRUE(removed_support && *removed_support);
  (*removed_support)->get().foreground = empty_material_id;
  EXPECT_EQ((*floor)->check_broken(), false);
  EXPECT_EQ((*floor)->should_destroy(), true);
  expect_drop(as<PlantDrop>(world.entity(501)), {0.5f, 1024.0f}, true, {{-0.5f, 0}, {0.5f, 1}},
      RectF({-1, 0}, {1, 2}), RectF({-1, 0}, {0, 1}));
  ASSERT_TRUE(world.removeEntity(500, false));
  ASSERT_TRUE(world.removeEntity(501, false));
  ASSERT_TRUE(world.removeEntity(502, false));
  ASSERT_TRUE(world.removeEntity(503, false));

  auto ceiling = make_plant({0, 1040}, true, {piece({0, 0}, 0, true), piece({0, -2}, 8, true),
      piece({-1, -2}, 3, true), piece({1, -3}, 8, false)});
  ASSERT_TRUE(ceiling) << ceiling.error().what();
  ASSERT_TRUE(world.addEntity(*ceiling, 500));
  // The ceiling keeps the first piece-space at equal Y and breaks at its top;
  // the other equally-high structural group remains standing.
  EXPECT_EQ((*ceiling)->damage_tiles({{17, 1034}, {2047, 1038}, {0, 1038}}, {2.0f, 1038.0f}, lethal), true);
  List<std::shared_ptr<PlantDrop>> ceiling_drops;
  for (auto id : world.entity_ids()) {
    if (auto drop = as<PlantDrop>(world.entity(id)))
      ceiling_drops.push_back(std::move(drop));
  }
  ASSERT_EQ(ceiling_drops.size(), 1u);
  expect_drop(ceiling_drops.front(), {0.5f, 1039.0f}, true, {{-0.5f, -1}, {0.5f, -2}},
      RectF({-1, -2}, {1, 0}), RectF({-1, -1}, {0, 0}));
  EXPECT_EQ(standing_offsets(**ceiling), (List<Vec2F>{{0, 0}, {-1, -2}}));
  EXPECT_EQ((*ceiling)->roots(), (List<Vec2I>{{0, 1}}));
});

namespace {

class MicroDungeonFlushObserver : public LogSink {
public:
  std::expected<void, StarException> log(std::string_view message, LogLevel level) override {
    if (level == LogLevel::Debug && message == "Flushing dungeon into the worldgen.") {
      ++draws;
      if (draws == 3)
        return on_third_draw();
    }
    return {};
  }

  unsigned draws = 0;
  std::function<std::expected<void, StarException>()> on_third_draw;
};

template<typename Result>
concept placement_test_result = std::same_as<Result, std::expected<typename Result::value_type, StarException>>;

const GameTestRegistration micro_dungeon_retry("MicroDungeonPlacement", "DrawAndTileCursorsSurviveReentrantQueueGrowthAndCompaction",
    [](GameTest& fixture) {
  auto checked = [&](placement_test_result auto const& result, std::string_view label,
      std::source_location at = std::source_location::current()) {
    return result || fixture.check(false, std::format("{}: {}", label, result.error().what()), at);
  };
  auto directory = File::temporary_directory(fixture.root().random());
  if (!checked(directory, "create temporary test directory")) return;
  auto cleanup = make_scope_exit([&] { fixture.check(static_cast<bool>(File::remove_all(*directory)), "remove test directory"); });
  auto overlay = File::relative_to(*directory, "assets");
  if (!checked(File::create_directory(overlay), "create asset overlay")) return;
  auto write_asset = [&](String const& name, String const& text) { return File::write_file(text, File::relative_to(overlay, name)); };
  if (!checked(write_asset("micro-retry.dungeon", R"({
      "metadata":{"name":"asterboundTestMicroRetry","species":"generic","anchor":["anchor"]},
      "parts":[{"name":"anchor","rules":[],"def":["tmx","micro-retry.json"]}]
    })"), "write dungeon definition")) return;
  if (!checked(write_asset("micro-retry.json", R"({
      "width":3,"height":1,"tilewidth":8,"tileheight":8,"tilesets":[{"source":"/micro-retry-tiles.json","firstgid":1}],
      "layers":[{"type":"tilelayer","name":"front","width":3,"height":1,"data":[1,1,1]}]
    })"), "write dungeon map")) return;
  if (!checked(write_asset("micro-retry-tiles.json", R"({"tilecount":1,"tileproperties":{"0":{"surface":"0"}}})"),
      "write surface tile")) return;

  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto root_result = Root::create(std::move(settings));
  if (!checked(root_result, "create fixture root")) return;
  auto& root = **root_result;
  auto assets = root.assets();
  auto materials = root.material_database();
  if (!checked(assets, "load assets") || !checked(materials, "load materials")) return;
  auto dirt = (*materials)->material_id("dirt");
  if (!checked(dirt, "resolve dirt material")) return;

  auto distribution = [&](int x, int priority) -> std::expected<BiomeItemDistribution, StarException> {
    auto configured = BiomeItemDistribution::create(root, JsonObject{{"type", "microdungeon"},
      {"microdungeons", JsonArray{"asterboundTestMicroRetry"}}, {"priority", priority},
      {"distribution", JsonObject{{"type", "periodic"}, {"modulus", 128}}}}, 42);
    if (!configured) return std::unexpected(std::move(configured.error()));
    return configured->to_json()
        .and_then([&](Json const& stored) { return stored.set("modulusOffset", -x); })
        .and_then([&](Json const& stored) { return BiomeItemDistribution::create(**assets, stored); });
  };
  auto natural_first = distribution(40, -20);
  auto queued_first = distribution(48, -10);
  auto queued_retry = distribution(56, 0);
  auto queued_alternate = distribution(78, 0);
  auto natural_last = distribution(61, 10);
  auto queued_reentrant = distribution(85, 5);
  if (!checked(natural_first, "configure first natural trial") || !checked(queued_first, "configure first queue trial")
      || !checked(queued_retry, "configure failing queue trial") || !checked(queued_alternate, "configure alternate queue trial")
      || !checked(natural_last, "configure last natural trial") || !checked(queued_reentrant, "configure reentrant queue trial")) return;

  auto file = File::ephemeral_file(root.random());
  if (!checked(file, "create sector storage")) return;
  auto world_result = WorldServer::create(root, Vec2U{128, 128}, *file);
  if (!checked(world_result, "create blank world")) return;
  auto& world = **world_result;
  auto close = make_scope_exit([&] { checked(world.close(), "close world"); });
  auto& storage = WorldServerTestAccess::world_storage(world);
  WorldGenerator generator(world);
  auto world_template = world.worldTemplate();
  WorldRegion region;
  region.terrainSelectorIndex = 1;
  region.blockBiomeIndex = region.environmentBiomeIndex = 1;
  region.regionLiquids = {};
  auto layout = [&root, &region](Biome const& biome, bool with_terrain) -> std::expected<std::shared_ptr<WorldLayout>, StarException> {
    auto biome_json = biome.to_json();
    if (!biome_json) return std::unexpected(std::move(biome_json.error()));
    JsonObject terrain{{"type", "flatSurface"}, {"config", JsonObject{}}, {"parameters", JsonObject{
      {"worldWidth", 128}, {"baseHeight", 48}, {"seed", 42}, {"commonality", 1}}}};
    JsonObject layer{{"yStart", 0}, {"boundaries", JsonArray{}}, {"cells", JsonArray{region.toJson()}}};
    return WorldLayout::create(root, JsonObject{{"worldSize", JsonArray{128, 128}}, {"biomes", JsonArray{std::move(*biome_json)}},
      {"terrainSelectors", with_terrain ? JsonArray{std::move(terrain)} : JsonArray{}},
      {"layers", with_terrain ? JsonArray{std::move(layer)} : JsonArray{}},
      {"regionBlending", 1}, {"playerStartSearchRegions", JsonArray{}}});
  };
  Biome biome;
  biome.main_block = *dirt;
  biome.material_hue_shift = 37;
  biome.surface_placeables.item_distributions = {std::move(*natural_first), std::move(*natural_last)};
  auto starting_layout = layout(biome, true);
  if (!checked(starting_layout, "create flat terrain")) return;
  world_template->setWorldLayout(std::move(*starting_layout));
  biome.surface_placeables.item_distributions.clear();

  // Call the real generation stage directly. Storage-wide invalidation on its
  // public generation entry points intentionally rolls back and closes the DB.
  WorldStorage::Sector const original{1, 1}, other{2, 1}, empty{3, 1};
  for (auto sector : {original, other, empty}) {
    if (!checked(storage.loadSector(sector), "load base tiles")
        || !checked(generator.generateSectorLevel(storage, sector, SectorGenerationLevel::BaseTiles), "prepare base tiles")) return;
  }
  auto generate = [&](WorldStorage::Sector const& sector) {
    return generator.generateSectorLevel(storage, sector, SectorGenerationLevel::MicroDungeons);
  };
  auto tile_at = [&](Vec2I const& position) -> ServerTile& { return storage.tileArray()->modify_tile(position).value().get(); };
  auto painted = [&](Vec2I const& position, DungeonId id) {
    auto const& tile = tile_at(position);
    return tile.foreground == *dirt && tile.background == *dirt && tile.foregroundHueShift == 37 && tile.dungeonId == id;
  };
  auto non_dungeon = BiomeItemDistribution::create(root, JsonObject{{"type", "treasureBox"}, {"treasureBoxSets", JsonArray{"unused"}},
      {"distribution", JsonObject{{"type", "random"}, {"blockProbability", 1}}}}, 42);
  if (!checked(non_dungeon, "configure ordinary biome distribution")) return;
  fixture.check(!generator.enqueue_placement({}, 99), "reject an empty queue request");
  fixture.check(!generator.enqueue_placement({std::move(*non_dungeon)}, 99), "reject an ordinary-only queue request");
  auto first = generator.enqueue_placement({std::move(*queued_first)}, 101);
  auto retry = generator.enqueue_placement({std::move(*queued_retry), std::move(*queued_alternate)}, 102);
  if (!checked(first, "enqueue first placement") || !checked(retry, "enqueue retrying placement")) return;

  // Find the same progress boundary as the dungeon's modified-tile traversal:
  // one tile has been resolved, then the invalid biome index fails replacement.
  constexpr int painted_y = 47; // This one-row TMX anchor draws one tile below its selected position.
  Set<Vec2I> footprint;
  for (int x = 55; x <= 57; ++x)
    footprint.insert(Vec2I{x, painted_y});
  auto order = footprint.values();
  auto done = order[0], fail = order[1], remaining = order[2];
  tile_at(fail).blockBiomeIndex = 250;
  std::optional<RpcPromise<Vec2I>> appended;
  auto observer = std::make_shared<MicroDungeonFlushObserver>();
  observer->set_level(LogLevel::Debug);
  observer->on_third_draw = [&]() -> std::expected<void, StarException> {
    if (generate(original))
      return std::unexpected(StarException("same-sector reentry returned success"));
    auto added = generator.enqueue_placement({std::move(*queued_reentrant)}, 103);
    if (!added) return std::unexpected(std::move(added.error()));
    appended = std::move(*added);
    return generate(empty); // Compacts the earlier fulfilled queue entry while the draw is active.
  };
  root.logger().add_sink(observer);
  auto remove_observer = make_scope_exit([&] { root.logger().remove_sink(observer); });

  auto failed = generate(original);
  if (!fixture.check(!failed && observer->draws == 3 && appended.has_value(), "fail postprocessing after three native draws")
      || !fixture.check(painted(done, 102) && tile_at(fail).foreground == biome_material_id
          && tile_at(remaining).foreground == biome_material_id, "stop on the failing tile after completing its predecessor"))
    return;
  fixture.check(first->result().value_or(std::nullopt) == Vec2I{48, 48}, "finish the earlier queued draw once");
  fixture.check(retry->status() == RpcPromiseStatus::Pending, "promise remains pending until all tiles resolve");
  if (!checked(generate(other), "generate a second sector while the first is suspended")) return;
  fixture.check(observer->draws == 4 && appended->result().value_or(std::nullopt) == Vec2I{85, 48},
      "fulfill only the reentrant append in the other sector");
  fixture.check(retry->status() == RpcPromiseStatus::Pending && tile_at({78, painted_y}).dungeonId == no_dungeon_id,
      "do not draw the suspended request at an alternate anchor");

  tile_at(fail).blockBiomeIndex = 1;
  tile_at(done).blockBiomeIndex = 250; // Reprocessing an already committed tile would now fail.
  auto no_candidates = layout(biome, false);
  if (!checked(no_candidates, "remove all eligible terrain before retry")) return;
  world_template->setWorldLayout(*no_candidates);
  if (!checked(generate(original), "resume original stage without re-sampling")) return;
  fixture.check(observer->draws == 5 && painted({40, painted_y}, biome_micro_dungeon_id)
      && painted({61, painted_y}, biome_micro_dungeon_id), "do not redraw the earlier natural trial; draw the retained later trial");
  fixture.check(painted(fail, 102) && painted(remaining, 102) && retry->result().value_or(std::nullopt) == Vec2I{56, 48},
      "apply the original request ID to every remaining tile before fulfilling the original promise");
  if (!checked(generate(original), "reenter the completed stage with no candidates")) return;
  fixture.check(observer->draws == 5, "no painting or second fulfillment after completion");

  auto natural_retry = distribution(106, 0);
  if (!checked(natural_retry, "configure suspended natural trial")) return;
  biome.surface_placeables.item_distributions.push_back(std::move(*natural_retry));
  auto natural_layout = layout(biome, true);
  if (!checked(natural_layout, "restore terrain for natural trial")) return;
  world_template->setWorldLayout(std::move(*natural_layout));
  Vec2I const natural_failure{106, painted_y};
  tile_at(natural_failure).blockBiomeIndex = 250;
  auto natural_failed = generate(empty);
  if (!fixture.check(!natural_failed && observer->draws == 6 && tile_at(natural_failure).foreground == biome_material_id,
      "suspend a natural draw on the real biome replacement error")) return;
  tile_at(natural_failure).blockBiomeIndex = 1;
  world_template->setWorldLayout(std::move(*no_candidates));
  if (!checked(generate(empty), "resume suspended natural draw")) return;
  for (int x = 105; x <= 107; ++x)
    fixture.check(observer->draws == 6 && painted({x, painted_y}, biome_micro_dungeon_id),
        "resolve natural tiles exactly once with native dungeon IDs");
});

}

const GameTestRegistration falling_block_world_edges("FallingBlocksWorld", "UnloadedSectorEdgesAndWrappedMoves", [](GameTest& fixture) {
  auto created = createSchedulingWorld(fixture.root());
  ASSERT_TRUE(created) << created.error().what();
  auto& world = **created;
  auto close = make_scope_exit([&] { EXPECT_TRUE(world.close()); });
  auto facade = FallingBlocksWorld::create(world);
  ASSERT_TRUE(facade) << facade.error().what();
  auto& blocks = **facade;
  Vec2I source_pos(0, 1024), target_pos(-1, 1023);
  auto cell = [](Vec2I const& pos) { return RectI::with_size(pos, {1, 1}); };
  // The player's initial generation radius can include this seam. Start
  // with an empty activation queue and explicitly unload the startup sectors.
  ASSERT_TRUE(WorldServerTestAccess::generate_queued_sectors(world));
  ASSERT_TRUE(WorldServerTestAccess::unload_sectors(world));
  // No synchronous generation: the unloaded source is stationary, yet both
  // it and the distinct sector below it must become active on queue drain.
  ASSERT_EQ(world.regionActive(cell(source_pos)), false);
  ASSERT_EQ(world.regionActive(cell({0, 1023})), false);
  ASSERT_EQ(blocks.source_behavior(source_pos), FallingBehavior::Stationary);
  ASSERT_TRUE(WorldServerTestAccess::generate_queued_sectors(world));
  EXPECT_EQ(world.regionActive(cell(source_pos)), true);
  EXPECT_EQ(world.regionActive(cell({0, 1023})), true);

  ASSERT_TRUE(world.generateRegion(cell(target_pos)));
  auto source_tile = world.modifyServerTile(source_pos);
  auto target_tile = world.modifyServerTile({2047, 1023});
  ASSERT_TRUE(source_tile && *source_tile && target_tile && *target_tile);
  auto& source = (*source_tile)->get();
  auto& target = (*target_tile)->get();
  auto materials = fixture.root().material_database();
  ASSERT_TRUE(materials);
  auto sand = (*materials)->material_id("sand");
  ASSERT_TRUE(sand);
  source.foreground = *sand;
  source.foregroundHueShift = 29;
  source.foregroundColorVariant = 3;
  source.update_collision(CollisionKind::Block);
  target.foreground = empty_material_id;
  target.update_collision(CollisionKind::None);
  source.rootSource = source_pos;
  EXPECT_EQ(blocks.source_behavior(source_pos), FallingBehavior::Stationary);
  source.rootSource.reset();
  EXPECT_EQ(blocks.source_behavior(source_pos), FallingBehavior::Cascading);
  target.rootSource = source_pos;
  EXPECT_EQ(blocks.target_open(target_pos), false);
  target.rootSource.reset();
  EXPECT_EQ(blocks.target_open(target_pos), true);
  EXPECT_EQ(blocks.target_open({std::numeric_limits<int>::max(), 1023}), true);
  EXPECT_EQ(blocks.target_open({0, std::numeric_limits<int>::max()}), false);

  // An unavailable endpoint must not change the other end. A failed
  // collision lookup must not install the source material or its appearance.
  auto unavailable = blocks.move_block(source_pos, {0, -1});
  ASSERT_TRUE(unavailable);
  EXPECT_EQ(*unavailable, BlockMoveResult::Unchanged);
  EXPECT_EQ(source.foreground, *sand);
  MaterialId missing = 0;
  while (missing < first_meta_material_id && (*materials)->is_valid_material_id(missing)) ++missing;
  ASSERT_LT(missing, first_meta_material_id);
  source.foreground = missing;
  target.foregroundHueShift = 18;
  target.foregroundColorVariant = 1;
  auto failed = blocks.move_block(source_pos, target_pos);
  ASSERT_FALSE(failed);
  EXPECT_EQ(failed.error().result, BlockMoveResult::Unchanged);
  EXPECT_EQ(source.foreground, missing);
  EXPECT_EQ(target.foreground, empty_material_id);
  EXPECT_EQ(target.foregroundHueShift, 18);
  EXPECT_EQ(target.foregroundColorVariant, 1);
  EXPECT_EQ(target.getCollision(), CollisionKind::None);

  source.foreground = *sand;
  auto moved = blocks.move_block(source_pos, target_pos);
  ASSERT_TRUE(moved) << moved.error().cause.what();
  EXPECT_EQ(*moved, BlockMoveResult::Changed);
  EXPECT_EQ(source.foreground, empty_material_id);
  EXPECT_EQ(source.getCollision(), CollisionKind::None);
  EXPECT_EQ(target.foreground, *sand);
  EXPECT_EQ(target.foregroundHueShift, 29);
  EXPECT_EQ(target.foregroundColorVariant, 3);
  EXPECT_EQ(target.getCollision(), CollisionKind::Block);
  EXPECT_EQ(blocks.target_open(target_pos), false);
});

const GameTestRegistration falling_block_protection("FallingBlocksWorld", "ProtectedModOnlyAndMaterialDrops", [](GameTest& fixture) {
  auto created = createSchedulingWorld(fixture.root());
  ASSERT_TRUE(created) << created.error().what();
  auto& world = **created;
  auto close = make_scope_exit([&] { EXPECT_TRUE(world.close()); });
  auto facade = FallingBlocksWorld::create(world);
  ASSERT_TRUE(facade);
  auto& blocks = **facade;
  Vec2I source_pos(1024, 1025), target_pos(1024, 1024);
  ASSERT_TRUE(world.generateRegion(RectI::with_size(target_pos, {1, 2})));
  auto source_tile = world.modifyServerTile(source_pos);
  auto target_tile = world.modifyServerTile(target_pos);
  ASSERT_TRUE(source_tile && *source_tile && target_tile && *target_tile);
  auto& source = (*source_tile)->get();
  auto& target = (*target_tile)->get();
  target.dungeonId = 17;
  target.foreground = empty_material_id;
  world.setTileProtectionEnabled(true);
  ASSERT_TRUE(world.setTileProtection(17, true));
  auto materials = fixture.root().material_database();
  ASSERT_TRUE(materials);
  auto sand = (*materials)->material_id("sand");
  auto grass = (*materials)->mod_id("grass");
  auto copper = (*materials)->mod_id("copper");
  ASSERT_TRUE(sand && grass && copper);
  auto drops = [&]() {
    std::multiset<String> names;
    for (auto id : world.entity_ids()) {
      if (auto drop = as<ItemDrop>(world.entity(id)))
        names.insert(drop->item()->name());
    }
    return names;
  };
  auto initial = drops();
  source.foreground = *sand;
  source.update_collision(CollisionKind::Block);
  source.foregroundMod = *grass;
  auto stripped = blocks.move_block(source_pos, target_pos);
  ASSERT_TRUE(stripped) << stripped.error().cause.what();
  EXPECT_EQ(*stripped, BlockMoveResult::Changed);
  EXPECT_EQ(source.foreground, *sand);
  EXPECT_EQ(source.foregroundMod, no_mod_id);
  EXPECT_EQ(source.getCollision(), CollisionKind::Block);
  EXPECT_EQ(target.foreground, empty_material_id);
  EXPECT_EQ(drops(), initial);

  // A co-breaking mod and its material emit two distinct item entities.
  source.foregroundMod = *copper;
  auto broken = blocks.move_block(source_pos, target_pos);
  ASSERT_TRUE(broken) << broken.error().cause.what();
  EXPECT_EQ(*broken, BlockMoveResult::Changed);
  EXPECT_EQ(source.foreground, empty_material_id);
  EXPECT_EQ(source.foregroundMod, no_mod_id);
  EXPECT_EQ(source.getCollision(), CollisionKind::None);
  EXPECT_EQ(target.foreground, empty_material_id);
  EXPECT_EQ(target.dungeonId, 17);
  initial.insert("sand");
  initial.insert("copperore");
  EXPECT_EQ(drops(), initial);
  auto again = blocks.move_block(source_pos, target_pos);
  ASSERT_TRUE(again);
  EXPECT_EQ(*again, BlockMoveResult::Unchanged);
  EXPECT_EQ(drops(), initial);

  // A chair occupies empty foreground spaces, independently of rooted
  // material. Probe one of its actual footprint cells, not its bounding box.
  Vec2I chair_pos(1032, 1024);
  ASSERT_TRUE(world.generateRegion(RectI::with_size(chair_pos - Vec2I(3, 1), {7, 6})));
  for (int x = -3; x <= 3; ++x) {
    auto floor = world.modifyServerTile(chair_pos + Vec2I(x, -1));
    ASSERT_TRUE(floor && *floor);
    (*floor)->get().foreground = structure_material_id;
    (*floor)->get().update_collision(CollisionKind::Block);
  }
  auto objects = fixture.root().object_database();
  ASSERT_TRUE(objects);
  auto chair = (*objects)->create_for_placement(world, "woodenchair", chair_pos, Direction::Left);
  ASSERT_TRUE(chair && *chair);
  ASSERT_TRUE(world.addEntity(*chair));
  auto spaces = (*chair)->spaces();
  ASSERT_FALSE(spaces.empty());
  auto occupied_pos = chair_pos + spaces.front();
  auto occupied = world.getServerTile(occupied_pos);
  ASSERT_TRUE(occupied);
  ASSERT_EQ(occupied->get().foreground, empty_material_id);
  ASSERT_FALSE(occupied->get().rootSource);
  EXPECT_EQ(blocks.target_open(occupied_pos), false);
  EXPECT_EQ(blocks.source_behavior(occupied_pos), FallingBehavior::Stationary);
});

const GameTestRegistration falling_block_partial_drop("FallingBlocksWorld", "RetryAfterSecondProtectedDropFails", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  auto items = File::relative_to(overlay, "items/generic/crafting");
  ASSERT_TRUE(File::create_directories(items));
  // Only copper fails during ItemDrop::init. The first (sand) entity has
  // already been published when the second addEntity reports the error.
  ASSERT_TRUE(File::write_file(String(R"([{"op":"add","path":"/itemDrop","value":{"scripts":[false]}}])"),
      File::relative_to(items, "copperore.item.patch")));
  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto root = Root::create(std::move(settings));
  ASSERT_TRUE(root) << root.error().what();
  auto created = createSchedulingWorld(**root);
  ASSERT_TRUE(created) << created.error().what();
  auto& world = **created;
  auto close = make_scope_exit([&] { EXPECT_TRUE(world.close()); });
  auto facade = FallingBlocksWorld::create(world);
  ASSERT_TRUE(facade);
  auto agent = FallingBlocksAgent::create(*facade, 471, 1);
  ASSERT_TRUE(agent);
  Vec2I source_pos(1024, 1025), target_pos(1024, 1024);
  ASSERT_TRUE(world.generateRegion(RectI::with_size(target_pos, {1, 2})));
  auto source_tile = world.modifyServerTile(source_pos);
  auto target_tile = world.modifyServerTile(target_pos);
  ASSERT_TRUE(source_tile && *source_tile && target_tile && *target_tile);
  auto& source = (*source_tile)->get();
  auto& target = (*target_tile)->get();
  target.dungeonId = 17;
  target.foreground = empty_material_id;
  world.setTileProtectionEnabled(true);
  ASSERT_TRUE(world.setTileProtection(17, true));
  auto materials = (*root)->material_database();
  ASSERT_TRUE(materials);
  auto sand = (*materials)->material_id("sand");
  auto copper = (*materials)->mod_id("copper");
  ASSERT_TRUE(sand && copper);
  source.foreground = *sand;
  source.foregroundMod = *copper;
  source.update_collision(CollisionKind::Block);
  auto initial = world.entity_ids().size();
  (*agent)->visit_location(source_pos);
  auto failed = (*agent)->update();
  ASSERT_FALSE(failed);
  EXPECT_EQ(source.foreground, empty_material_id);
  EXPECT_EQ(source.foregroundMod, no_mod_id);
  EXPECT_EQ(target.foreground, empty_material_id);
  ASSERT_EQ(world.entity_ids().size(), initial + 1);
  unsigned sand_drops = 0;
  for (auto id : world.entity_ids()) {
    if (auto drop = as<ItemDrop>(world.entity(id)); drop && drop->item()->name() == "sand")
      ++sand_drops;
  }
  EXPECT_EQ(sand_drops, 1u);
  ASSERT_TRUE((*agent)->update());
  ASSERT_TRUE((*agent)->update());
  EXPECT_EQ(world.entity_ids().size(), initial + 1);
});

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

const GameTestRegistration command_dispatch("ServerTest", "CommandDispatchAndPatchedHelp", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directory(overlay));
  ASSERT_TRUE(File::write_file(String(R"([
    {"op":"replace","path":"/basicCommands","value":{"z":"last","topic":"first","help":"footer"}},
    {"op":"replace","path":"/openSbCommands","value":{"topic":"second","mod":"patched command"}},
    {"op":"replace","path":"/adminCommands","value":{"secret":"admin topic"}},
    {"op":"replace","path":"/basicHelpText","value":"Basic: <commands>"},
    {"op":"replace","path":"/openSbHelpText","value":"Mod: <commands>"},
    {"op":"remove","path":"/openSbDebugCommands"}
  ])"), File::relative_to(overlay, "help.config.patch")));
  ASSERT_TRUE(File::write_file(String(R"([
    {"op":"replace","path":"/commandProcessorScripts","value":["/command_probe.lua"]}
  ])"), File::relative_to(overlay, "universe_server.config.patch")));
  ASSERT_TRUE(File::write_file(String(R"(
    function command(name, id, args)
      return name .. ":" .. table.concat(args, "|")
    end
  )"), File::relative_to(overlay, "command_probe.lua")));
  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto root = Root::create(std::move(settings));
  ASSERT_TRUE(root) << root.error().what();
  auto config = (*root)->configuration();
  ASSERT_TRUE(config) << config.error().what();
  ASSERT_TRUE((*config)->set("allowAdminCommands", false));
  auto server = UniverseServer::create(**root, (*root)->toStoragePath("universe"));
  ASSERT_TRUE(server) << server.error().what();
  auto processor = CommandProcessor::create(**root, **server);
  ASSERT_TRUE(processor) << processor.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*processor)->close()); });
  auto expect_reply = [](std::expected<ServerCommandResult, StarException> result, std::string_view expected) {
    ASSERT_TRUE(result) << result.error().what();
    ASSERT_TRUE(std::holds_alternative<String>(*result));
    EXPECT_EQ(std::get<String>(*result).utf8(), expected);
  };
  auto user = [&](String const& name, String const& args) { return (*processor)->user_command(1, name, args); };
  auto admin = [&](String const& name, String const& args) { return (*server)->admin_command(name, args); };

  // Point queries consult even the admin categories, but stop on the first match.
  // The absent final category is not needed for a non-admin's summary either.
  expect_reply(user("HeLp", "topic"), "first");
  expect_reply(user("help", "mod"), "patched command");
  expect_reply(user("help", "secret"), "admin topic");
  expect_reply(user("help", ""), "Basic: /help, /topic, /z\nMod: /mod, /topic\nfooter");
  EXPECT_FALSE(admin("help", ""));
  expect_reply(user("SpAwNiTeM", ""), "Admin commands disabled on this server.");
  expect_reply(user("eval", ""), "The execute server script command can only be used locally.");
  expect_reply(user("entityeval", ""), "The execute server entity script command can only be used locally.");
  ASSERT_TRUE((*config)->set("allowAdminCommands", true));
  ASSERT_TRUE((*config)->set("allowAdminCommandsFromAnyone", false));
  expect_reply(user("spawnitem", ""), "Insufficient privileges to spawn items.");
  expect_reply(user("disablespawning", ""), "Insufficient privileges to disable world spawning.");
  expect_reply(user("whereami", ""), "Your current location is Nowhere");
  expect_reply(user("whereis", "@nobody"), "Your current location is Nowhere");
  expect_reply(admin("WHEREAMI", "@nobody"), "No user with specifier @nobody found.");
  expect_reply(admin("WhereIs", "@nobody"), "No user with specifier @nobody found.");
  expect_reply(admin("LuaProbe", R"('first two' '' three)"), "LuaProbe:first two||three");

  expect_reply(admin("TiMeWaRp", ""), "Not enough arguments to /timewarp");
  expect_reply(admin("timewarp", "later"), "Could not parse the argument later as a time adjustment");
  expect_reply(admin("timewarp", "-0"), "You suck at time travel.");
  expect_reply(admin("timewarp", "-1"), "Great Scott! We can't go back in time!");
  expect_reply(admin("timewarp", "-1 please"), "And then a step to the right...");
  expect_reply(admin("timewarp", "1"), "It's just a jump to the left...");
  expect_reply(admin("enablespawning", ""), "enabling monster spawning failed");
  expect_reply(admin("disablespawning", ""), "disabling monster spawning failed");
  expect_reply(admin("spawnitem", "probe 1 ["), "Could not parse item parameters");
  expect_reply(admin("spawnitem", "probe invalid"), "Could not load item 'probe'");
  expect_reply(admin("spawntreasure", "probe invalid"), "Could not load item 'probe'");
  expect_reply(admin("spawnvehicle", "probe []"), "Could not spawn vehicle");
  expect_reply(admin("spawnstagehand", "probe []"), "Could not spawn Stagehand of type 'probe []'");
});

const GameTestRegistration universe_directory_lock("ServerTest", "DirectoryLockPrecedesLuaInit", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directory(overlay));
  ASSERT_TRUE(File::write_file(String(R"([
    {"op":"add","path":"/scriptContexts/lockProbe","value":["/lock_probe.lua"]}
  ])"), File::relative_to(overlay, "universe_server.config.patch")));
  ASSERT_TRUE(File::write_file(String(R"(
    function init()
      root.setConfiguration("lockProbeCount", root.getConfiguration("lockProbeCount") + 1)
    end
  )"), File::relative_to(overlay, "lock_probe.lua")));
  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto root = Root::create(std::move(settings));
  ASSERT_TRUE(root) << root.error().what();
  auto config = (*root)->configuration();
  ASSERT_TRUE(config) << config.error().what();
  ASSERT_TRUE((*config)->set("lockProbeCount", 0));
  ASSERT_TRUE((*config)->set("clearUniverseFiles", true));
  auto universe = (*root)->toStoragePath("universe");
  {
    auto holder = UniverseServer::create(**root, universe);
    ASSERT_TRUE(holder) << holder.error().what();
    ASSERT_EQ((*config)->get("lockProbeCount"), Json(1));
    auto contender = UniverseServer::create(**root, universe);
    ASSERT_FALSE(contender);
    EXPECT_NE(std::string_view(contender.error().what()).find("Could not acquire lock"), std::string_view::npos);
    EXPECT_EQ((*config)->get("lockProbeCount"), Json(1));
  }

  // Fail after Lua init, while the directory belongs to the partially built server.
  ASSERT_TRUE((*config)->set("clearUniverseFiles", "not a boolean"));
  auto failed = UniverseServer::create(**root, universe);
  ASSERT_FALSE(failed);
  ASSERT_EQ((*config)->get("lockProbeCount"), Json(2));

  ASSERT_TRUE((*config)->set("clearUniverseFiles", false));
  auto successor = UniverseServer::create(**root, universe);
  ASSERT_TRUE(successor) << successor.error().what();
  EXPECT_EQ((*config)->get("lockProbeCount"), Json(3));
});

const GameTestRegistration
  fidelityScheduleIntervalsAndOffsets("ServerTest", "FidelityScheduleIntervalsAndOffsets", [](GameTest& fixture) {
    auto created = createSchedulingWorld(fixture.root());
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto close = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
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
    auto close = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
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
    auto close = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
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
    auto close = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
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
    EXPECT_EQ(missingWiring.error().what(), expectedWiring.error().what());
    EXPECT_EQ(world->currentStep(), 1u);
    EXPECT_EQ(triggered, 1u);// Earlier timer effects survive the scheduling failure.

    medium["wiringUpdate"] = JsonArray{1, 0};
    WorldServerTestAccess::setConfiguration(*world, JsonObject{{"medium", medium}});
    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::Medium));
    auto malformedLiquid = world->update(0.025f);
    auto expectedLiquid = json_to_vec2u(Json("bad timing"));
    ASSERT_FALSE(malformedLiquid);
    ASSERT_FALSE(expectedLiquid);
    EXPECT_EQ(malformedLiquid.error().what(), expectedLiquid.error().what());
    EXPECT_EQ(world->currentStep(), 2u);
    EXPECT_EQ(triggered, 1u);

    medium["liquidUpdate"] = JsonArray{1, 0};
    WorldServerTestAccess::setConfiguration(*world, JsonObject{{"medium", medium}});
    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::Medium));
    auto malformedLimit = world->update(0.025f);
    auto expectedLimit = Json(false).opt_uint();
    ASSERT_FALSE(malformedLimit);
    ASSERT_FALSE(expectedLimit);
    EXPECT_EQ(malformedLimit.error().what(), expectedLimit.error().what());
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
    EXPECT_EQ(missingFalling.error().what(), expectedFalling.error().what());
    EXPECT_EQ(world->currentStep(), 4u);
    EXPECT_EQ(triggered, 1u);
  });

namespace {

struct PlantEffectSink : NullRenderCallback {
  std::expected<void, StarException> add_particle(Particle&& particle) override {
    if (reject_at == accepted.size()) {
      reject_at.reset();
      rejected_particle = particle.position;
      return std::unexpected(StarException("Reject plant particle"));
    }
    accepted += 'p';
    particles.push_back(particle.position);
    return {};
  }

  std::expected<void, StarException> add_audio(std::shared_ptr<AudioInstance> audio) override {
    if (reject_at == accepted.size()) {
      reject_at.reset();
      rejected_audio = std::move(audio);
      return std::unexpected(StarException("Reject plant sound"));
    }
    accepted += 'a';
    sounds.push_back(std::move(audio));
    return {};
  }

  std::optional<std::size_t> reject_at;
  std::optional<Vec2F> rejected_particle;
  std::shared_ptr<AudioInstance> rejected_audio;
  std::string accepted;
  List<Vec2F> particles;
  List<std::shared_ptr<AudioInstance>> sounds;
};

std::expected<std::unique_ptr<Root>, StarException> plant_test_root(GameTest& fixture, String const& directory, String const& overlay = {}) {
  auto settings = fixture.rootSettings();
  if (!overlay.empty())
    settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  settings.mix_random_entropy = false;
  return Root::create(std::move(settings));
}

Plant::PlantPiece plant_drop_piece(String image = "/items/materials/dirt.png") {
  Plant::PlantPiece piece;
  piece.image = std::move(image);
  piece.kind = Plant::Stem;
  piece.structural_segment = true;
  piece.spaces.add({0, 0});
  return piece;
}

Json plant_test_sounds() {
  return JsonArray{JsonObject{{"file", "/sfx/interface/voice_on.ogg"}}};
}

Json plant_test_particles(float density) {
  return JsonObject{{"density", density}, {"options", JsonArray{JsonObject{{"type", "ember"}}}}};
}

const GameTestRegistration plant_item_delivery("PlantDrop", "RetriesSecondNativeItemPublicationAndPreservesSaplingAndRng", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto root = plant_test_root(fixture, *directory);
  ASSERT_TRUE(root) << root.error().what();
  ASSERT_TRUE((*root)->itemDatabase());
  ASSERT_TRUE((*root)->image_metadata_database());
  auto created = createSchedulingWorld(**root);
  ASSERT_TRUE(created) << created.error().what();
  auto& world = **created;
  auto close = make_scope_exit([&] { EXPECT_TRUE(world.close()); });
  Vec2F origin(1024, 1024);
  Json config = JsonObject{
      {"drops", JsonArray{JsonArray{JsonObject{{"item", "dirtmaterial"}, {"count", 7}}, JsonObject{{"item", "sapling"}, {"count", 2}}}}},
      {"sounds", JsonObject{{"breakTree", plant_test_sounds()}, {"hitGround", plant_test_sounds()}}}};
  // Vanilla's builder treats the empty string as a named foliage in Lua.
  // Use its stock stem/foliage pair, but distinguish both authored hue shifts.
  Json sapling_config = JsonObject{{"stemName", "pineytree"}, {"stemHueShift", 12}, {"foliageName", "pinefoliage"}, {"foliageHueShift", 31}};
  std::array<Vec2F, 2> baseline_positions;
  std::uint64_t baseline_rng = 0;
  for (bool reject_second : {false, true}) {
    auto ids = WorldServerTestAccess::limit_entity_ids(world, 500, 502);
    auto drop = PlantDrop::create(**root, {plant_drop_piece()}, origin, {}, "plant test", true, config, {}, sapling_config, true, 0);
    ASSERT_TRUE(drop) << drop.error().what();
    ASSERT_TRUE(world.addEntity(*drop, 500));
    if (reject_second) {
      auto filler = PlantDrop::create(**root, {plant_drop_piece()}, origin, {}, "filler", true, {}, {}, {}, false, 0);
      ASSERT_TRUE(filler);
      ASSERT_TRUE(world.addEntity(*filler, 502));
    }
    (*root)->random().seed(0x5a5011);
    auto landed = (*drop)->update(5001.0f, 1);
    if (reject_second) {
      ASSERT_FALSE(landed);
      EXPECT_EQ((*drop)->should_destroy(), false);
      auto rejected_store = (*drop)->net_store();
      auto rejected_state = (*drop)->snapshot_net_state();
      ASSERT_TRUE(rejected_store && rejected_state);
      auto observed = PlantDrop::create(**root, *rejected_store);
      ASSERT_TRUE(observed);
      ASSERT_TRUE((*observed)->read_net_state(rejected_state->first));
      EXPECT_EQ((*observed)->should_destroy(), false);
      // The first drop is present while the second still owns its retry slot.
      ASSERT_TRUE(as<ItemDrop>(world.entity(501)));
      ASSERT_TRUE(world.removeEntity(502, false));
      ASSERT_TRUE((*drop)->update(1.0f / 60, 2));
    } else {
      ASSERT_TRUE(landed) << landed.error().what();
    }
    EXPECT_EQ((*drop)->should_destroy(), true);
    ASSERT_TRUE((*drop)->update(1.0f / 60, 3));
    ASSERT_EQ(world.entity_ids().size(), 3u);
    auto dirt = as<ItemDrop>(world.entity(501));
    auto sapling = as<ItemDrop>(world.entity(502));
    ASSERT_TRUE(dirt && dirt->item() && sapling && sapling->item());
    auto dirt_item = dirt->item()->descriptor();
    auto sapling_item = sapling->item()->descriptor();
    EXPECT_EQ(dirt_item.name(), String("dirtmaterial"));
    EXPECT_EQ(dirt_item.count(), 7u);
    EXPECT_EQ(sapling_item.name(), String("sapling"));
    EXPECT_EQ(sapling_item.count(), 2u);
    EXPECT_EQ(sapling_item.parameters().get_string("stemName"), String("pineytree"));
    EXPECT_EQ(sapling_item.parameters().get_string("foliageName"), String("pinefoliage"));
    EXPECT_EQ(sapling_item.parameters().get_int("stemHueShift"), 12);
    EXPECT_EQ(sapling_item.parameters().get_int("foliageHueShift"), 31);
    std::array positions{dirt->position(), sapling->position()};
    auto next_rng = (*root)->random().next_u64();
    if (reject_second) {
      EXPECT_EQ(positions, baseline_positions);
      EXPECT_EQ(next_rng, baseline_rng);
    } else {
      baseline_positions = positions;
      baseline_rng = next_rng;
    }

    auto store = (*drop)->net_store();
    auto state = (*drop)->snapshot_net_state();
    ASSERT_TRUE(store && state);
    auto replica = PlantDrop::create(**root, *store);
    ASSERT_TRUE(replica);
    ASSERT_TRUE((*replica)->read_net_state(state->first));
    PlantEffectSink sink;
    ASSERT_TRUE((*replica)->collect_render_events(sink));
    EXPECT_TRUE(sink.accepted.empty()) << "late join replayed a completed event";
  }

  Json invalid = JsonObject{{"drops", JsonArray{JsonArray{JsonObject{{"item", "sapling"}, {"count", -1}}}}}};
  auto negative = PlantDrop::create(**root, {plant_drop_piece()}, origin, {}, "negative", true, invalid, {}, {}, true, 0);
  ASSERT_TRUE(negative);
  ASSERT_TRUE(world.addEntity(*negative));
  auto rejected = (*negative)->update(5001, 1);
  ASSERT_FALSE(rejected);
  EXPECT_NE(std::string_view(rejected.error().what()).find("negative"), std::string_view::npos);
  EXPECT_EQ((*negative)->should_destroy(), false);
  EXPECT_EQ(world.entity_ids().size(), 1u);
});

const GameTestRegistration plant_effect_delivery("PlantDrop", "FallingAndStandingEffectsRetryOnlyRejectedOutput", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto root = plant_test_root(fixture, *directory);
  ASSERT_TRUE(root) << root.error().what();
  ASSERT_TRUE((*root)->particle_database());
  ASSERT_TRUE((*root)->image_metadata_database());
  auto created = createSchedulingWorld(**root);
  ASSERT_TRUE(created) << created.error().what();
  auto& world = **created;
  auto close = make_scope_exit([&] { EXPECT_TRUE(world.close()); });

  Vec2I tile(1024, 1024);
  String image = "/items/materials/dirt.png";
  Json bursts = JsonObject{
      {"particles", JsonObject{{"breakTree", plant_test_particles(0.5f)}, {"damageTree", plant_test_particles(2.0f)}}},
      {"sounds", JsonObject{{"breakTree", plant_test_sounds()}, {"damageTree", plant_test_sounds()}}}};
  Json standing_store = JsonObject{
      {"tilePosition", json_from_vec2i(tile)}, {"ceiling", false}, {"ephemeral", false}, {"fallsWhenDead", false},
      {"descriptions", JsonObject{{"description", "standing test"}}},
      {"stemDropConfig", bursts}, {"foliageDropConfig", JsonObject{}}, {"saplingDropConfig", JsonObject{}},
      {"tileDamageParameters", JsonObject{{"damageFactors", JsonObject{}}, {"damageRecovery", 0}}},
      {"pieces", JsonArray{JsonObject{{"image", image}, {"offset", JsonArray{0, 0}}, {"rotationType", "dontRotate"},
          {"rotationOffset", 0}, {"structuralSegment", true}, {"kind", static_cast<int>(Plant::Stem)}, {"segmentIdx", 0}, {"flip", false}}}}};

  for (bool standing : {false, true}) {
    std::string baseline_trace;
    List<Vec2F> baseline_particles;
    std::uint64_t baseline_rng = 0;
    for (auto reject_at : std::array<std::optional<std::size_t>, 3>{std::nullopt,
        standing ? 1u : 2u, standing ? 2u : 1u}) {
      std::shared_ptr<Entity> entity;
      if (standing) {
        auto plant = Plant::create(**root, standing_store);
        ASSERT_TRUE(plant) << plant.error().what();
        ASSERT_TRUE((*plant)->init(world, 500, EntityMode::Master));
        ASSERT_TRUE((*plant)->damage_tiles({tile}, Vec2F(tile), TileDamage{TileDamageType::Blockish, 0.25f}));
        entity = std::move(*plant);
      } else {
        auto drop = PlantDrop::create(**root, {plant_drop_piece(image)}, Vec2F(tile), {}, "falling test", true, bursts, bursts, {}, true, 0);
        ASSERT_TRUE(drop);
        entity = std::move(*drop);
      }
      (*root)->random().seed(0x115011);
      PlantEffectSink sink;
      sink.reject_at = reject_at;
      auto collected = entity->collect_render_events(sink);
      if (reject_at) {
        ASSERT_FALSE(collected);
        ASSERT_EQ(sink.accepted.size(), *reject_at);
        auto particle_index = sink.particles.size();
        auto audio_index = sink.sounds.size();
        ASSERT_TRUE(entity->collect_render_events(sink));
        if (sink.rejected_particle) {
          ASSERT_LT(particle_index, sink.particles.size());
          EXPECT_EQ(sink.particles[particle_index], *sink.rejected_particle);
        } else {
          ASSERT_TRUE(sink.rejected_audio);
          ASSERT_LT(audio_index, sink.sounds.size());
          EXPECT_EQ(sink.sounds[audio_index], sink.rejected_audio);
        }
      } else {
        ASSERT_TRUE(collected) << collected.error().what();
      }
      auto next_rng = (*root)->random().next_u64();
      if (reject_at) {
        EXPECT_EQ(sink.accepted, baseline_trace);
        EXPECT_EQ(sink.particles, baseline_particles);
        EXPECT_EQ(next_rng, baseline_rng);
      } else {
        ASSERT_TRUE(sink.accepted.starts_with(standing ? "ppa" : "aapp"));
        baseline_trace = sink.accepted;
        baseline_particles = sink.particles;
        baseline_rng = next_rng;
      }
      ASSERT_TRUE(entity->collect_render_events(sink));
      EXPECT_EQ(sink.accepted, baseline_trace);
      if (standing)
        ASSERT_TRUE(entity->uninit());
    }
  }
});

const GameTestRegistration plant_particle_budget("PlantDrop", "SparseSamplesResumeAndTransparentOrInvalidInputsTerminate", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directory(overlay));
  constexpr unsigned width_in_tiles = 16;
  for (bool opaque_edge : {false, true}) {
    Image image(width_in_tiles * tile_pixels, tile_pixels, PixelFormat::RGBA32);
    for (unsigned y = 0; y < tile_pixels; ++y)
      for (unsigned x = 0; x < width_in_tiles * tile_pixels; ++x)
        ASSERT_TRUE(image.set(x, y, Vec4B(255, 255, 255, opaque_edge && x < tile_pixels ? 255 : 0)));
    auto png = std::make_shared<Buffer>();
    ASSERT_TRUE(image.write_png(fixture.root().logger(), png));
    auto name = opaque_edge ? String("plant-sparse.png") : String("plant-transparent.png");
    ASSERT_TRUE(File::write_file(png->take_data(), File::relative_to(overlay, name)));
  }
  auto root = plant_test_root(fixture, *directory, overlay);
  ASSERT_TRUE(root) << root.error().what();
  ASSERT_TRUE((*root)->particle_database());
  ASSERT_TRUE((*root)->image_metadata_database());

  auto create_drop = [&](String image, float density) {
    Json config = JsonObject{{"particles", JsonObject{{"breakTree", plant_test_particles(density)}}}};
    return PlantDrop::create(**root, {plant_drop_piece(std::move(image))}, {}, {}, "sparse", true, config, {}, {}, false, 0);
  };
  // Select a deterministic stream whose first hit cannot fit the initial tick.
  std::optional<std::uint64_t> sparse_seed;
  for (std::uint64_t seed = 0; seed < 64 && !sparse_seed; ++seed) {
    RandomSource sample(seed);
    for (std::size_t attempt = 1; attempt <= 1024; ++attempt) {
      auto x = sample.normal_float(width_in_tiles / 8.0f, 0);
      auto y = sample.normal_float(1.0f / 8.0f, 0);
      ASSERT_TRUE(x && y);
      float tile_x = width_in_tiles / 2.0f + *x;
      float tile_y = 0.5f + *y;
      if (tile_x >= 0 && tile_x < 1 && tile_y >= 0 && tile_y < 1) {
        if (attempt > 256)
          sparse_seed = seed;
        break;
      }
    }
  }
  ASSERT_TRUE(sparse_seed);
  PlantEffectSink sink;
  auto sparse = create_drop("/plant-sparse.png", width_in_tiles);
  ASSERT_TRUE(sparse);
  (*root)->random().seed(*sparse_seed);
  ASSERT_TRUE((*sparse)->collect_render_events(sink));
  EXPECT_TRUE(sink.accepted.empty());
  for (auto tick = 0; tick != 4; ++tick)
    ASSERT_TRUE((*sparse)->collect_render_events(sink));
  EXPECT_EQ(sink.accepted, "p");
  auto transparent = create_drop("/plant-transparent.png", width_in_tiles);
  ASSERT_TRUE(transparent);
  ASSERT_TRUE((*transparent)->collect_render_events(sink));
  EXPECT_EQ(sink.accepted, "p");

  for (float divisor : {0.0f, -1.0f, 1.0e-20f}) {
    auto invalid = create_drop("/plant-sparse.png", divisor);
    ASSERT_TRUE(invalid);
    EXPECT_FALSE((*invalid)->collect_render_events(sink));
  }
  EXPECT_EQ(sink.accepted, "p");
});

}
