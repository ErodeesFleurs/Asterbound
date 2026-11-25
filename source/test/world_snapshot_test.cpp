#include <cstdio>
import star.json_extra;
import star.json;
import star.xx_hash;
import star.data_stream_extra;
import star.compression;
import star.either;
import star.ordered_map;
import star.rect;
import star.vector;
import star.game_types;
import star.message_types;
import star.entity_message_promise_queue;
import star.material_types;
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
import star.data_stream_devices;
import star.file;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.id_map;
import star.casting;
import star.image;
import star.string;
import star.strong_typedef;
import star.rpc_promise;
import star.set;
import star.worker_pool;

import std;
import star.byte_array;
import star.format;
import star.sector_array_2d;
import star.net_compatibility;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.btree_database;
import star.ordered_set;
import star.map;

import star.celestial_coordinate;
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
import star.lua_root;
import star.tile_sector_array;
import star.collision_block;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.tile_modification;
import star.world;
import star.world_impl;
import star.liquid_types;
import star.tile_damage;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.chat_types;
import star.uuid;
import star.warping;
import star.wiring;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.biome;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.cellular_light_array;
import star.cellular_lighting;
import star.cellular_liquid;
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
import star.projectile;
import star.projectile_database;
import star.material_database;
import star.world_structure;
import star.world_template;
import star.terrain_database;
import star.entity_factory;

#include "gtest/gtest.h"


import star.celestial_database;

import star.algorithm;
import star.game_test;
using namespace Star;

namespace {

const GameTestRegistration reentrant_replacement("WorldEntityMessageQueue", "ReentrantReplacementSurvivesNestedDrainAndGrowth",
[](GameTest& fixture) {
  EntityMessagePromiseQueue<Uuid> queue;
  auto const key = Uuid::nil();
  auto pending = RpcPromise<Json>::make_pair().first;
  std::vector<Json> replies;
  auto deliver = [&](Uuid const& uuid, std::expected<Json, String> reply) {
    fixture.check(uuid == key, "reply has the original UUID");
    if (fixture.check(reply.has_value(), "reply succeeds"))
      replies.push_back(std::move(*reply));
  };
  int calls = 0;
  auto original = RpcPromise<Json>::make_fulfilled(Json(1)).wrap([&](Json value) -> std::expected<Json, StarException> {
    ++calls;
    for (char byte = 1; byte <= 64; ++byte) {
      std::array<char, uuid_size> bytes{};
      bytes.front() = byte;
      auto another_key = Uuid::from_bytes(bytes);
      if (!another_key)
        return std::unexpected(std::move(another_key.error()));
      queue.insert_or_assign(*another_key, pending);
    }
    queue.insert_or_assign(key, RpcPromise<Json>::make_fulfilled(Json(2)));
    fixture.check(queue.drain(deliver).has_value(), "nested drain does not poll");
    return value;
  });
  if (!fixture.check(original.has_value(), "wrap original promise"))
    return;
  queue.insert_or_assign(key, std::move(*original));

  fixture.check(queue.drain(deliver).has_value(), "drain original snapshot");
  fixture.check(calls == 1, "observe original once");
  fixture.check(replies.empty(), "do not deliver old or new reply in the original snapshot");
  fixture.check(queue.drain(deliver).has_value(), "drain replacement");
  fixture.check(replies == std::vector<Json>{Json(2)}, "only the replacement reply was delivered");
  queue.clear();
  fixture.check(queue.empty(), "clear removes the pending registrations");
});

const GameTestRegistration disconnect_cleanup("WorldEntityMessageQueue", "DisconnectInvalidatesOldRegistrationButNotOtherClient",
[](GameTest& fixture) {
  using Key = std::pair<ConnectionId, Uuid>;
  EntityMessagePromiseQueue<Key> queue;
  auto const uuid = Uuid::nil();
  Key const disconnected{1, uuid};
  Key const other{2, uuid};
  auto [pending, keeper] = RpcPromise<Json>::make_pair();
  queue.insert_or_assign(other, std::move(pending));
  auto original = RpcPromise<Json>::make_fulfilled(Json(1)).wrap([&](Json value) -> std::expected<Json, StarException> {
    queue.discard_if([&](Key const& key) { return key.first == disconnected.first; });
    queue.insert_or_assign(disconnected, RpcPromise<Json>::make_fulfilled(Json(2)));
    return value;
  });
  if (!fixture.check(original.has_value(), "wrap original client's promise"))
    return;
  queue.insert_or_assign(disconnected, std::move(*original));

  std::optional<Json> disconnected_reply;
  std::optional<Json> other_reply;
  auto deliver = [&](Key const& key, std::expected<Json, String> reply) {
    if (!fixture.check(reply.has_value() && key.second == uuid, "reply succeeds with the shared UUID")
        || !fixture.check(key == disconnected || key == other, "reply targets one of the registered clients"))
      return;
    auto& slot = key == disconnected ? disconnected_reply : other_reply;
    fixture.check(!slot, "reply delivered only once");
    slot = std::move(*reply);
  };
  fixture.check(queue.drain(deliver).has_value(), "drain across disconnect");
  fixture.check(!disconnected_reply && !other_reply, "no stale or still pending reply delivered");
  fixture.check(keeper.fulfill(Json(3)).has_value(), "fulfill other client's reply");
  fixture.check(queue.drain(deliver).has_value(), "drain both completed registrations");
  fixture.check(disconnected_reply == Json(2), "reused connection receives only its own reply");
  fixture.check(other_reply == Json(3), "other client retains its registration for the same UUID");
  fixture.check(queue.empty(), "completed registrations are removed");
});

const GameTestRegistration diagnostics_retry("WorldEntityMessageQueue", "DiagnosticsRetryUnlessClearedDuringObservation",
[](GameTest& fixture) {
  EntityMessagePromiseQueue<Uuid> queue;
  auto const key = Uuid::nil();
  int calls = 0;
  auto retry = RpcPromise<Json>::make_fulfilled(Json()).wrap([&](Json value) -> std::expected<Json, StarException> {
    if (++calls == 1)
      return std::unexpected(StarException("transient diagnostic"));
    return value;
  });
  if (!fixture.check(retry.has_value(), "wrap retryable promise"))
    return;
  queue.insert_or_assign(key, std::move(*retry));
  int replies = 0;
  auto deliver = [&](Uuid const& uuid, std::expected<Json, String> reply) {
    fixture.check(uuid == key, "reply has the registered UUID");
    fixture.check(reply && *reply == Json(), "reply is a successful JSON null");
    ++replies;
  };
  auto diagnostic = queue.drain(deliver);
  fixture.check(!diagnostic, "current diagnostic is reported");
  fixture.check(!queue.empty() && calls == 1 && replies == 0, "diagnostic keeps registration for retry");
  fixture.check(queue.drain(deliver).has_value(), "next drain retries");
  fixture.check(calls == 2 && replies == 1, "retry delivers exactly one reply");

  auto stale = RpcPromise<Json>::make_fulfilled(Json()).wrap([&](Json) -> std::expected<Json, StarException> {
    queue.clear();
    queue.insert_or_assign(key, RpcPromise<Json>::make_fulfilled(Json()));
    return std::unexpected(StarException("obsolete diagnostic"));
  });
  if (!fixture.check(stale.has_value(), "wrap promise that clears the queue"))
    return;
  queue.insert_or_assign(key, std::move(*stale));
  fixture.check(queue.drain(deliver).has_value(), "diagnostic for cleared registration is discarded");
  fixture.check(replies == 1, "replacement survives until the next drain");
  fixture.check(queue.drain(deliver).has_value(), "next drain delivers replacement");
  fixture.check(replies == 2 && queue.empty(), "replacement is delivered once and removed");
});

}

TEST(PacketWire, PauseUsesStreamVersionThroughBaseInterface) {
  PausePacket original(true, 0.5f);
  Packet const& source = original;
  for (auto version : {legacy_version, open_protocol_version}) {
    DataStreamBuffer wire;
    wire.set_stream_compatibility_version(version);
    ASSERT_TRUE(source.write(wire));
    DataStreamBuffer expected;
    ASSERT_TRUE(expected.write(true));
    if (version != legacy_version)
      ASSERT_TRUE(expected.write(0.5f));
    ASSERT_EQ(wire.data(), expected.data());

    ASSERT_TRUE(wire.seek(0));
    PausePacket decoded(false, 0.0f);
    Packet& destination = decoded;
    ASSERT_TRUE(destination.read(wire));
    EXPECT_TRUE(decoded.pause);
    EXPECT_EQ(decoded.timescale, version == legacy_version ? 1.0f : 0.5f);
    auto ended = wire.at_end();
    ASSERT_TRUE(ended);
    EXPECT_TRUE(*ended);
  }
}

const GameTestRegistration regional_liquid_sampling_across_seam(
    "WorldSnapshot", "RegionalLiquidSamplingAcrossSeam", [](GameTest& fixture) {
  TileSectorArray<ClientTile, 4> tiles(Vec2U(8, 4));
  tiles.load_default_sector({0, 0});
  tiles.load_default_sector({1, 0});

  // Include fractional coverage on both sides of the seam and an empty row.
  RectF const region(7.5f, -0.25f, 8.5f, 0.5f);
  auto empty = WorldImpl::liquid_level(tiles, region);
  if (!fixture.check(bool(empty), "sample empty region"))
    return;
  fixture.check(empty->liquid == empty_liquid_id && empty->level == 0.0f, "empty region has no liquid");

  auto left = tiles.modify_tile({7, 0});
  auto right = tiles.modify_tile({0, 0});
  if (!fixture.check(left && right, "load tiles on both sides of the seam"))
    return;

  constexpr LiquidId high_id = std::numeric_limits<LiquidId>::max();
  left->get().liquid = {high_id, 2.0f};
  right->get().liquid = {high_id, 0.5f};
  auto single = WorldImpl::liquid_level(tiles, region);
  if (!fixture.check(bool(single), "sample single-liquid region"))
    return;
  fixture.check(single->liquid == high_id && single->level == 0.5f,
      "single liquid uses fractional coverage and includes the empty row");

  // Both liquids contribute, but the lower ID wins the tie despite being
  // visited second. The overfilled tile contributed at most one tile's volume.
  right->get().liquid = {1, 1.0f};
  auto mixed = WorldImpl::liquid_level(tiles, region);
  if (!fixture.check(bool(mixed), "sample mixed-liquid region"))
    return;
  fixture.check(mixed->liquid == LiquidId{1}, "lower liquid ID wins the volume tie");
  fixture.check(std::abs(mixed->level - 2.0f / 3.0f) <= std::numeric_limits<float>::epsilon(),
      "mixed level includes both liquids");

  constexpr float tiny = std::numeric_limits<float>::min();
  for (auto const& zero_region : {RectF(0.5f, 0, 0.5f, 1), RectF(0, 0, tiny, tiny)}) {
    auto zero = WorldImpl::liquid_level(tiles, zero_region);
    if (!fixture.check(bool(zero), "sample zero-area region"))
      return;
    fixture.check(zero->liquid == empty_liquid_id && zero->level == 0.0f, "zero area has no liquid");
  }
});

const GameTestRegistration regional_liquid_sampling_retains_ordered_sums(
    "WorldSnapshot", "RegionalLiquidSamplingRetainsOrderedSums", [](GameTest& fixture) {
  TileSectorArray<ClientTile, 4> tiles(Vec2U(12, 4));
  tiles.load_default_sector({0, 0});
  tiles.load_default_sector({1, 0});

  constexpr float half_ulp = 0x1p-24f;
  std::array<LiquidLevel, 6> const row{{
      {3, half_ulp / 2}, {3, half_ulp / 2}, {2, half_ulp},
      {1, 1.0f}, {1, half_ulp}, {1, half_ulp}}};
  auto populated = tiles.tile_eval(RectI(0, 0, 6, 1),
      [&](Vec2I const& position, ClientTile& tile) -> std::expected<void, StarException> {
    tile.liquid = row[static_cast<std::size_t>(position[0])];
    return {};
  });
  if (!fixture.check(bool(populated), "populate order-sensitive liquid tiles"))
    return;

  // The half-ULPs each round away at 1.0, both in the liquid 1 bucket
  // and when summing the three buckets by ID rather than by visit order.
  auto level = WorldImpl::liquid_level(tiles, RectF(0, 0, 8, 1));
  if (!fixture.check(bool(level), "sample order-sensitive liquid region"))
    return;
  fixture.check(level->liquid == LiquidId{1}, "liquid 1 has the greatest volume");
  fixture.check(level->level == 0.125f, "tile and liquid ID summation preserve ordered rounding");
});

namespace {
  // Hashes one tile together with its position.  The per sample hashes are
  // combined order independently, so that two worlds can be compared without
  // serialising all of them.
  [[nodiscard]] std::expected<std::uint64_t, StarException> tileHash(Vec2I position, ServerTile const& tile) {
    DataStreamBuffer buffer;
    auto positionWritten = buffer.write(position);
    if (!positionWritten)
      return std::unexpected(std::move(positionWritten.error()));
    auto tileWritten = tile.write(buffer);
    if (!tileWritten)
      return std::unexpected(std::move(tileWritten.error()));

    std::uint64_t hash = 14695981039346656037ull;
    for (auto byte : buffer.data())
      hash = (hash ^ (std::uint8_t)byte) * 1099511628211ull;
    return hash;
  }

  // Samples a fixed grid around the surface of a world.  Reading a tile
  // generates the sector it is in, so only the sampled sectors are generated.
  [[nodiscard]] std::expected<std::uint64_t, StarException> sampleWorld(WorldServer& world, float surfaceLevel) {
    std::uint64_t hash = 0;
    for (int x : {0, 128, 256, 384, 512, 640, 768, 896}) {
      for (int y = (int)surfaceLevel - 1280; y <= (int)surfaceLevel + 512; y += 128) {
        auto tile = world.getServerTile(Vec2I(x, y));
        if (!tile)
          return std::unexpected(std::move(tile.error()));
        auto sample = tileHash(Vec2I(x, y), tile->get());
        if (!sample)
          return std::unexpected(std::move(sample.error()));
        hash ^= *sample;
      }
    }
    return hash;
  }
}

const GameTestRegistration worldMessageBatchDrainsAfterFailure(
    "WorldSnapshot", "WorldMessageBatchDrainsAfterFailure", [](GameTest&) {
  for (bool handlerFailure : {true, false}) {
    List<WorldServerThread::Message> messages;
    List<RpcPromise<Json>> promises;
    for (unsigned i = 0; i != 5; ++i) {
      auto [promise, keeper] = RpcPromise<Json>::make_pair();
      promises.push_back(std::move(promise));
      messages.push_back({"batchRegression", {}, std::move(keeper)});
    }
    // Exercise an already-settled keeper both at the failure boundary and
    // during draining: neither error may leave the final request pending.
    if (!handlerFailure)
      ASSERT_TRUE(messages[1].promise.fulfill(Json(9)));
    ASSERT_TRUE(messages[3].promise.fail("Already settled"));
    unsigned handled = 0;
    auto settled = settleWorldMessageBatch(messages,
        [&](String const&, JsonArray const&)
            -> MessageResponseResult {
          ++handled;
          if (handlerFailure && handled == 2)
            return std::unexpected(StarException("Injected world handler failure"));
          return std::optional<ChainableJsonMessageResponse>(Json(17));
        });
    ASSERT_FALSE(settled);
    EXPECT_EQ(handled, 2u);
    if (handlerFailure)
      EXPECT_NE(std::string_view(settled.error().what()).find("Injected world handler failure"), std::string_view::npos);
    else
      EXPECT_TRUE(settled.error().is<RpcPromiseException>());
    for (auto const& promise : promises)
      EXPECT_TRUE(promise.finished());
    auto first = promises[0].result();
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, std::optional<Json>(Json(17)));
    auto failed = promises[1].error();
    ASSERT_TRUE(failed);
    EXPECT_EQ(*failed, handlerFailure ? std::optional<String>("World message handling failed") : std::nullopt);
    for (unsigned i : {2u, 4u}) {
      auto drained = promises[i].error();
      ASSERT_TRUE(drained);
      EXPECT_EQ(*drained, std::optional<String>("World message handling stopped"));
    }
    auto alreadySettled = promises[3].error();
    ASSERT_TRUE(alreadySettled);
    EXPECT_EQ(*alreadySettled, std::optional<String>("Already settled"));
  }
});

const GameTestRegistration terrainNamedSelector(
  "TerrainDatabaseTest", "UnknownNamesAndSeededNamedSelectorsPreserveSampling", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    ASSERT_TRUE(File::write_file(String(R"({
      "name":"seeded","type":"perlin","function":"perlin","octaves":3,"freq":0.03,"amp":5
    })"),
                                 File::relative_to(*directory, "seeded.terrain")));
    ASSERT_TRUE(File::write_file(String(R"({"name":"badType","type":"unknown-selector-type"})"),
                                 File::relative_to(*directory, "bad.terrain")));
    Assets::Settings settings{};
    settings.worker_pool_size = 1;
    settings.asset_time_to_live = 10;
    Logger logger;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets) << assets.error().what();
    auto database = TerrainDatabase::create(*assets, fixture.root().random());
    ASSERT_TRUE(database) << database.error().what();
    TerrainSelectorParameters parameters(fixture.root().random());
    parameters.seed = 0x123456789abcdef0ull;
    parameters.worldWidth = 2048;
    parameters.baseHeight = 128;
    auto unknown = (*database)->createNamedSelector("missing-selector", parameters);
    ASSERT_FALSE(unknown);
    EXPECT_TRUE(unknown.error().is<TerrainException>());
    auto badType = (*database)->createNamedSelector("badType", parameters);
    ASSERT_FALSE(badType);
    EXPECT_TRUE(badType.error().is<TerrainException>());
    auto first = (*database)->createNamedSelector("seeded", parameters);
    ASSERT_TRUE(first) << first.error().what();
    auto second = (*database)->createNamedSelector("seeded", parameters);
    ASSERT_TRUE(second) << second.error().what();
    auto config = (*assets)->json("/seeded.terrain");
    ASSERT_TRUE(config);
    auto direct = (*database)->createSelectorType("perlin", *config, parameters);
    ASSERT_TRUE(direct);
    EXPECT_EQ((*database)->storeSelector(*first), (*database)->storeSelector(*second));
    EXPECT_EQ((*first)->parameters.seed, parameters.seed);
    for (int x : {-128, 0, 37, 512, 2047}) {
      for (int y : {-64, 0, 128, 999}) {
        auto a = (*first)->get(x, y);
        auto b = (*second)->get(x, y);
        auto c = (*direct)->get(x, y);
        ASSERT_TRUE(a);
        ASSERT_TRUE(b);
        ASSERT_TRUE(c);
        EXPECT_EQ(*a, *b);
        EXPECT_EQ(*a, *c);
      }
    }
  });

const GameTestRegistration worldTimersPreserveOrderAndDeferReentrantActions(
  "WorldTimers", "PreserveOrderAndDeferReentrantActions", [](GameTest& fixture) {
    auto storage = File::ephemeral_file(fixture.root().random());
    ASSERT_TRUE(storage) << storage.error().what();
    auto created = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto closeWorld = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
    std::vector<int> fired;
    world->timer(0, [&](World& owner) -> std::expected<void, StarException> {
      fired.push_back(1);
      owner.timer(0, [&](World&) -> std::expected<void, StarException> {
        fired.push_back(4);
        return {};
      });
      return {};
    });
    world->timer(0.25f, [&](World&) -> std::expected<void, StarException> {
      fired.push_back(3);
      return {};
    });
    world->timer(0, [&](World&) -> std::expected<void, StarException> {
      fired.push_back(2);
      return {};
    });

    ASSERT_TRUE(world->update(0.125f));
    EXPECT_EQ(fired, (std::vector<int>{1, 2}));
    ASSERT_TRUE(world->update(0.125f));
    EXPECT_EQ(fired, (std::vector<int>{1, 2, 3, 4}));
    ASSERT_TRUE(world->update(0.125f));
    EXPECT_EQ(fired, (std::vector<int>{1, 2, 3, 4}));
  });

const GameTestRegistration celestial_chunk_response(
  "CelestialDatabase", "ChunkResponsePreservesWireBytesAndCachedSystems", [](GameTest& fixture) {
    auto& root = fixture.root();
    auto directory = File::temporary_directory(root.random());
    ASSERT_TRUE(directory) << directory.error().what();
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto path = File::relative_to(*directory, "universe.chunks");

    CelestialChunk seed;
    seed.chunk_index = {0, -1};
    seed.constellations = {{{{0, -1}, {1, -1}}, {{1, -1}, {2, -1}}}};
    auto parameters = [&](CelestialCoordinate coordinate) {
      return CelestialParameters::create(root, coordinate, 42, coordinate.id(), JsonObject{});
    };
    Vec3I const location{0, -1, 71};
    for (int x = 0; x < 12; ++x) {
      Vec3I system{x, -1, location[2] + x};
      auto star = parameters(CelestialCoordinate(system));
      ASSERT_TRUE(star) << star.error().what();
      seed.system_parameters[system] = std::move(*star);
      seed.system_objects[system] = {};
    }
    auto planet = parameters(CelestialCoordinate(location, 3));
    auto moon = parameters(CelestialCoordinate(location, 3, 1));
    ASSERT_TRUE(planet) << planet.error().what();
    ASSERT_TRUE(moon) << moon.error().what();
    auto& body = seed.system_objects[location][3];
    body.planet_parameters = std::move(*planet);
    body.satellite_parameters[1] = std::move(*moon);

    auto stored = seed.to_json();
    ASSERT_TRUE(stored) << stored.error().what();
    {
      auto versions = root.versioningDatabase();
      ASSERT_TRUE(versions) << versions.error().what();
      auto versioned = (*versions)->makeCurrentVersionedJson("CelestialChunk", *stored);
      ASSERT_TRUE(versioned) << versioned.error().what();
      DataStreamBuffer stream;
      ASSERT_TRUE(stream.write(*versioned));
      ASSERT_TRUE(VersionedJson::writeSubVersioning(stream, *versioned));
      auto compressed = compress(stream.data());
      ASSERT_TRUE(compressed) << compressed.error().what();
      auto key = DataStreamBuffer::serialize(seed.chunk_index);
      ASSERT_TRUE(key) << key.error().what();
      BTreeSha256Database storage(root.logger());
      ASSERT_TRUE(storage.set_content_identifier("Celestial2"));
      auto file = File::open(path, IOMode::ReadWrite);
      ASSERT_TRUE(file) << file.error().what();
      ASSERT_TRUE(storage.set_io_device(std::move(*file)));
      ASSERT_TRUE(storage.open());
      ASSERT_TRUE(storage.insert(*key, *compressed));
      ASSERT_TRUE(storage.close(true));
    }

    // Derive both legacy payloads from the same on-disk insertion order that
    // the master loads, not from the original map or the response under test.
    auto loaded = CelestialChunk::create(*stored);
    ASSERT_TRUE(loaded) << loaded.error().what();
    auto planets = loaded->system_objects.get(location);
    ASSERT_TRUE(planets) << planets.error().what();
    DataStreamBuffer system_wire;
    ASSERT_TRUE(system_wire.write(std::uint8_t{2}));
    ASSERT_TRUE(system_wire.write(CelestialSystemObjects{location, planets->get()}));
    auto legacy_chunk = *loaded;
    legacy_chunk.system_objects.clear();
    DataStreamBuffer chunk_wire;
    ASSERT_TRUE(chunk_wire.write(std::uint8_t{1}));
    ASSERT_TRUE(chunk_wire.write(legacy_chunk));

    auto database = CelestialMasterDatabase::create(root, path);
    ASSERT_TRUE(database) << database.error().what();
    auto close_database = make_scope_exit([&] { EXPECT_TRUE((*database)->close()); });
    for (int pass = 0; pass < 2; ++pass) {
      auto chunk = (*database)->respond_to_request(make_left(seed.chunk_index));
      ASSERT_TRUE(chunk) << chunk.error().what();
      ASSERT_TRUE(chunk->is_left());
      EXPECT_TRUE(chunk->left_ref().persistent); // Not serialized.
      auto summary = DataStreamBuffer::serialize(*chunk);
      ASSERT_TRUE(summary) << summary.error().what();
      EXPECT_EQ(*summary, chunk_wire.data());

      auto system = (*database)->respond_to_request(make_right(location));
      ASSERT_TRUE(system) << system.error().what();
      auto objects = DataStreamBuffer::serialize(*system);
      ASSERT_TRUE(objects) << objects.error().what();
      EXPECT_EQ(*objects, system_wire.data());
    }
  });

const GameTestRegistration compact_candidates_retain_names(
  "WorldTemplate", "CompactCandidatesRetainNamesAcrossSourceReplacement", [](GameTest& fixture) {
    auto& root = fixture.root();
    auto ocean_dungeon = [&](String name) {
      return BiomeItemDistribution::create(root, JsonObject{
          {"mode", "ocean"}, {"priority", 0.5}, {"type", "microdungeon"}, {"microdungeons", JsonArray{std::move(name)}},
          {"distribution", JsonObject{{"type", "random"}, {"blockProbability", 1}}}}, 42);
    };
    auto natural = ocean_dungeon("natural-anchor");
    auto queued = ocean_dungeon("queued-anchor");
    auto ordinary = BiomeItemDistribution::create(root, JsonObject{
        {"mode", "ocean"}, {"priority", 0.5}, {"type", "treasureBox"}, {"treasureBoxSets", JsonArray{"unused"}},
        {"distribution", JsonObject{{"type", "random"}, {"blockProbability", 1}}}}, 43);
    if (!fixture.check(natural && queued && ordinary, "create three guaranteed ocean samples")) return;

    Biome biome;
    biome.surface_placeables.item_distributions.push_back(std::move(*ordinary));
    biome.surface_placeables.item_distributions.push_back(std::move(*natural));
    auto stored_biome = biome.to_json();
    if (!fixture.check(stored_biome.has_value(), "store the natural placement distributions")) return;

    WorldRegion region;
    region.blockBiomeIndex = 1;
    region.regionLiquids.oceanLiquid = 1;
    region.regionLiquids.oceanLiquidLevel = 5;
    auto layout = WorldLayout::create(root, JsonObject{
        {"worldSize", JsonArray{16, 16}}, {"biomes", JsonArray{std::move(*stored_biome)}}, {"terrainSelectors", JsonArray{}},
        {"layers", JsonArray{JsonObject{{"yStart", 0}, {"boundaries", JsonArray{}}, {"cells", JsonArray{region.toJson()}}}}},
        {"regionBlending", 1}, {"playerStartSearchRegions", JsonArray{}}});
    auto world = WorldTemplate::create(root, Vec2U(16, 16));
    if (!fixture.check(layout && world, "build a world template with an ocean site")) return;
    (*world)->setWorldLayout(std::move(*layout));
    auto source = (*world)->biome(1);
    if (!fixture.check(source && *source, "read the original biome owner")) return;
    std::weak_ptr<Biome const> natural_owner = *source;
    auto observe_names = [](BiomeItemDistribution const& distribution, int x) -> std::weak_ptr<StringSet const> {
      auto sample = distribution.item_at(x, 5);
      if (!sample || !*sample) return {};
      auto const& item = (**sample).get();
      if (!std::holds_alternative<MicroDungeonNames>(item)) return {};
      return std::get<MicroDungeonNames>(item).names;
    };
    auto natural_names = observe_names((*source)->surface_placeables.item_distributions[1], 12);
    auto queued_names = observe_names(*queued, -4);
    if (!fixture.check(!natural_names.expired() && !queued_names.expired(), "observe both immutable dungeon-name sets")) return;
    source->reset();

    List<BiomeItemDistribution> distributions;
    distributions.push_back(std::move(*queued));
    WorldTemplate::PotentialMicroDungeonItems scratch;
    if (!fixture.check((*world)->potential_micro_dungeon_items_at(-4, 5, scratch).has_value(), "sample the natural biome across the seam")) return;
    auto validity = (*world)->biome_item_validity_at(-4, 5);
    if (!fixture.check(validity.has_value(), "evaluate terrain once for both sources")) return;
    std::vector<WorldTemplate::MicroDungeonItem> candidates;
    auto append = [&](WorldTemplate::MicroDungeonItem&& item) { candidates.push_back(std::move(item)); };
    scratch.append_valid(*validity, append);
    if (!fixture.check((*world)->add_potential_micro_dungeon_items(-4, 5, scratch, distributions).has_value(), "sample the queued distributions")) return;
    scratch.append_valid(*validity, append);
    if (!fixture.check(candidates.size() == 3, "emit exactly two natural samples and one queued sample")) return;

    (*world)->setWorldLayout({});
    distributions.clear();
    if (!fixture.check(natural_owner.expired() && !natural_names.expired() && !queued_names.expired(),
        "retained candidates keep their name sets, not the original distributions")) return;
    fixture.check(candidates[0].position == Vec2I(12, 5) && candidates[0].priority == 0.5f && !candidates[0].dungeon_names,
        "natural non-dungeon sample retains its priority and first append position");
    fixture.check(candidates[1].position == Vec2I(12, 5) && candidates[1].dungeon_names && candidates[1].dungeon_names->contains("natural-anchor"),
        "natural candidate uses wrapped coordinates and keeps its original names");
    fixture.check(candidates[2].position == Vec2I(-4, 5) && candidates[2].dungeon_names && candidates[2].dungeon_names->contains("queued-anchor"),
        "queued candidate keeps its raw sampling coordinates and original names");
    candidates.clear();
    fixture.check(natural_names.expired() && queued_names.expired(), "the last candidates release both name sets");
  });

// This configured asset/math-profile baseline catches unintended world changes.
// Independently constructed worlds must also agree. Update the fixed hash only
// for an intentional generation contract change, not to hide nondeterminism.
const GameTestRegistration fixedCoordinateGenerationIsDeterministic(
  "WorldSnapshot", "FixedCoordinateGenerationIsDeterministic", [](GameTest& fixture) {
    auto storagePath = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(storagePath) << storagePath.error().what();
    auto finallyGuard = make_scope_exit([&storagePath]() { EXPECT_TRUE(File::remove_all(*storagePath)); });
    auto celestial_database =
      CelestialMasterDatabase::create(fixture.root(), File::relative_to(*storagePath, "universe.chunks"));
    ASSERT_TRUE(celestial_database) << celestial_database.error().what();
    auto closeDatabase = make_scope_exit([&]() { EXPECT_TRUE((*celestial_database)->close()); });

    // Find a real planet: the celestial generator places systems sparsely, and a
    // hand picked coordinate is most likely empty.
    auto systems = (*celestial_database)->scan_systems(RectI::with_size(Vec2I(0, 0), Vec2I(32, 32)));
    ASSERT_TRUE(systems) << systems.error().what();
    ASSERT_FALSE(systems->empty());
    std::optional<CelestialCoordinate> coordinate;
    for (auto const& system : *systems) {
      auto children = (*celestial_database)->children(system);
      ASSERT_TRUE(children) << children.error().what();
      for (auto const& child : *children) {
        auto parameters = (*celestial_database)->parameters(child);
        ASSERT_TRUE(parameters) << parameters.error().what();
        if (child.is_planetary_body() && *parameters) {
          coordinate = child;
          break;
        }
      }
      if (coordinate)
        break;
    }
    ASSERT_TRUE(coordinate.has_value()) << "no planetary body found in the scanned region";
    // A separate template for the surface level: a template handed to a WorldServer
    // is modified by world generation.
    auto surfaceTemplate = WorldTemplate::create(fixture.root(), *coordinate, *celestial_database);
    ASSERT_TRUE(surfaceTemplate) << surfaceTemplate.error().what();
    auto surfaceLevel = (*surfaceTemplate)->surfaceLevel();

    auto makeWorld = [&]() -> std::expected<std::shared_ptr<WorldServer>, StarException> {
      auto worldTemplate = WorldTemplate::create(fixture.root(), *coordinate, *celestial_database);
      if (!worldTemplate)
        return std::unexpected(std::move(worldTemplate.error()));
      auto storage = File::ephemeral_file(fixture.root().random());
      if (!storage)
        return std::unexpected(std::move(storage.error()));
      return WorldServer::create(fixture.root(), *worldTemplate, *storage);
    };
    auto firstWorld = makeWorld();
    ASSERT_TRUE(firstWorld) << firstWorld.error().what();
    auto closeFirst = make_scope_exit([&]() { EXPECT_TRUE((*firstWorld)->close()); });
    auto secondWorld = makeWorld();
    ASSERT_TRUE(secondWorld) << secondWorld.error().what();
    auto closeSecond = make_scope_exit([&]() { EXPECT_TRUE((*secondWorld)->close()); });

    auto firstHash = sampleWorld(**firstWorld, surfaceLevel);
    ASSERT_TRUE(firstHash) << firstHash.error().what();
    auto secondHash = sampleWorld(**secondWorld, surfaceLevel);
    ASSERT_TRUE(secondHash) << secondHash.error().what();
    auto reported = Star::print(*stderr, "WorldSnapshot: {} hashes to 0x{:x}\n", coordinate->id(), *firstHash);
    ASSERT_TRUE(reported) << reported.error().what();

    EXPECT_EQ(*firstHash, *secondHash);
    // Bounded WeightedPool selection intentionally changes old dungeon choices.
    EXPECT_EQ(*firstHash, 0xc45d53a5d2740da5ull) << "update the expected hash (printed above)";
  });

const GameTestRegistration shipWorldIsDeterministic("WorldSnapshot", "ShipWorldIsDeterministic", [](GameTest& fixture) {
  // The ship world does not depend on celestial parameters, so it has its own
  // stable hash.
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto speciesShips = (*assets)->json("/universe_server.config:speciesShips");
  ASSERT_TRUE(speciesShips) << speciesShips.error().what();
  auto humanShips = speciesShips->get("human");
  ASSERT_TRUE(humanShips) << humanShips.error().what();
  auto shipPaths = json_to_string_list(*humanShips);
  ASSERT_TRUE(shipPaths) << shipPaths.error().what();
  ASSERT_FALSE(shipPaths->empty());
  auto shipStructure = WorldStructure::create(fixture.root(), (*shipPaths)[0]);
  ASSERT_TRUE(shipStructure) << shipStructure.error().what();
  Vec2U worldSize(2048, 2048);
  auto jWorldSize = shipStructure->configValue("worldSize");
  ASSERT_TRUE(jWorldSize) << jWorldSize.error().what();
  if (*jWorldSize) {
    auto size = json_to_vec2u(*jWorldSize);
    ASSERT_TRUE(size) << size.error().what();
    worldSize = *size;
  }

  auto makeWorld = [&]() -> std::expected<std::shared_ptr<WorldServer>, StarException> {
    auto storage = File::ephemeral_file(fixture.root().random());
    if (!storage)
      return std::unexpected(std::move(storage.error()));
    auto world = WorldServer::create(fixture.root(), worldSize, *storage);
    if (!world)
      return std::unexpected(std::move(world.error()));
    auto structureSet = (*world)->setCentralStructure(*shipStructure);
    if (!structureSet)
      return std::unexpected(std::move(structureSet.error()));
    return *world;
  };
  auto firstWorld = makeWorld();
  ASSERT_TRUE(firstWorld) << firstWorld.error().what();
  auto closeFirst = make_scope_exit([&]() { EXPECT_TRUE((*firstWorld)->close()); });
  auto secondWorld = makeWorld();
  ASSERT_TRUE(secondWorld) << secondWorld.error().what();
  auto closeSecond = make_scope_exit([&]() { EXPECT_TRUE((*secondWorld)->close()); });

  // The ship structure is placed around the middle of the world.
  Vec2I center((int)worldSize[0] / 2, (int)worldSize[1] / 2);
  std::uint64_t firstHash = 0;
  std::uint64_t secondHash = 0;
  unsigned nonEmptyTiles = 0;
  for (int x = -160; x <= 160; x += 16) {
    for (int y = -160; y <= 160; y += 16) {
      Vec2I position = center + Vec2I(x, y);
      auto firstTile = (*firstWorld)->getServerTile(position);
      ASSERT_TRUE(firstTile) << firstTile.error().what();
      auto firstSample = tileHash(position, firstTile->get());
      ASSERT_TRUE(firstSample) << firstSample.error().what();
      firstHash ^= *firstSample;
      auto secondTile = (*secondWorld)->getServerTile(position);
      ASSERT_TRUE(secondTile) << secondTile.error().what();
      auto secondSample = tileHash(position, secondTile->get());
      ASSERT_TRUE(secondSample) << secondSample.error().what();
      secondHash ^= *secondSample;
      if (firstTile->get().foreground != empty_material_id)
        ++nonEmptyTiles;
    }
  }
  auto reported = Star::print(*stderr, "WorldSnapshot: ship world hashes to 0x{:x}\n", firstHash);
  ASSERT_TRUE(reported) << reported.error().what();

  EXPECT_GT(nonEmptyTiles, 0u) << "the sampled region does not contain any ship structure";
  EXPECT_EQ(firstHash, secondHash);
  EXPECT_EQ(firstHash, 0x96a3df5c62832b0bull) << "update the expected hash (printed above)";
});

const GameTestRegistration stoppedWorldRetainsClientsAndExportsRecoverableChunks(
  "WorldSnapshot", "StoppedWorldRetainsClientsAndExportsRecoverableChunks", [](GameTest& fixture) {
    auto storageResult = File::temporary_file(fixture.root().random());
    ASSERT_TRUE(storageResult) << storageResult.error().what();
    auto storage = *storageResult;
    auto path = storage->file_name();
    auto cleanup = make_scope_exit([&]() {
      EXPECT_TRUE(storage->close());
      EXPECT_TRUE(File::remove(path));
    });
    auto worldResult = WorldServer::create(fixture.root(), Vec2U(2048, 2048), storage);
    ASSERT_TRUE(worldResult) << worldResult.error().what();
    auto world = *worldResult;
    auto closeWorld = make_scope_exit([&]() { EXPECT_TRUE(world->close()); });
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets) << assets.error().what();
    auto speciesShips = (*assets)->json("/universe_server.config:speciesShips");
    ASSERT_TRUE(speciesShips) << speciesShips.error().what();
    auto humanShips = speciesShips->get("human");
    ASSERT_TRUE(humanShips) << humanShips.error().what();
    auto shipPaths = json_to_string_list(*humanShips);
    ASSERT_TRUE(shipPaths) << shipPaths.error().what();
    ASSERT_FALSE(shipPaths->empty());
    auto structure = WorldStructure::create(fixture.root(), (*shipPaths)[0]);
    ASSERT_TRUE(structure) << structure.error().what();
    ASSERT_TRUE(world->setCentralStructure(*structure));
    ASSERT_TRUE(world->setProperty("stopSaveRegression", Json(42)));

    auto threadResult = WorldServerThread::create(world, ClientShipWorldId(Uuid(fixture.root().random())));
    ASSERT_TRUE(threadResult) << threadResult.error().what();
    auto& thread = **threadResult;
    auto added = thread.addClient(min_client_connection_id, SpawnTargetPosition(Vec2F(1024, 1024)), true, false,
                                  NetCompatibilityRules{}, main_client_world_id);
    ASSERT_TRUE(added) << added.error().what();
    ASSERT_TRUE(*added);
    ASSERT_TRUE(thread.start());
    ASSERT_TRUE(thread.stop());

    // UniverseServer still needs this list to migrate players after joining.
    ASSERT_TRUE(thread.hasClient(min_client_connection_id));
    EXPECT_EQ(thread.clients().size(), 1u);
    EXPECT_FALSE(thread.serverErrorOccurred());
    auto descriptor = ItemDescriptor::create("money", 7, Json());
    ASSERT_TRUE(descriptor) << descriptor.error().what();
    auto finalItem = std::make_shared<GiveItemPacket>(std::move(*descriptor));
    auto sent = world->sendPacket(min_client_connection_id, finalItem);
    ASSERT_TRUE(sent);

    // An asynchronous warp may wait for its destination.  Merely queueing it
    // must leave the stopped source alive, with its final outgoing queue intact.
    auto waitingChunks = thread.readChunks();
    ASSERT_TRUE(waitingChunks) << waitingChunks.error().what();
    ASSERT_FALSE(waitingChunks->empty());
    ASSERT_TRUE(thread.hasClient(min_client_connection_id));
    ASSERT_TRUE(storage->is_open());

    // Ship/custom-world snapshots are exported after stop, before final close.
    ASSERT_TRUE(thread.unloadAll(true));
    auto chunks = thread.readChunks();
    ASSERT_TRUE(chunks) << chunks.error().what();
    ASSERT_FALSE(chunks->empty());
    ASSERT_FALSE(thread.serverErrorOccurred());
    auto finalPackets = thread.removeClient(min_client_connection_id);
    ASSERT_TRUE(finalPackets) << finalPackets.error().what();
    ASSERT_FALSE(finalPackets->empty());
    auto transport = LocalPacketSocket::openPair();
    ASSERT_TRUE(transport.first->sendPackets(std::move(*finalPackets)));
    auto delivered = transport.second->receivePackets();
    ASSERT_TRUE(delivered) << delivered.error().what();
    EXPECT_TRUE(delivered->contains(finalItem));
    ASSERT_FALSE(delivered->empty());
    EXPECT_EQ(delivered->back()->type(), PacketType::WorldStop);
    EXPECT_TRUE(thread.noClients());
    ASSERT_TRUE(thread.close());
    ASSERT_TRUE(thread.close());
    EXPECT_FALSE(thread.serverErrorOccurred());

    auto fromChunks = WorldServer::create(fixture.root(), *chunks);
    ASSERT_TRUE(fromChunks) << fromChunks.error().what();
    EXPECT_EQ((*fromChunks)->getProperty("stopSaveRegression", Json()), Json(42));
    EXPECT_TRUE((*fromChunks)->close());

    auto reopened = File::open(path, IOMode::ReadWrite);
    ASSERT_TRUE(reopened) << reopened.error().what();
    auto fromFile = WorldServer::create(fixture.root(), *reopened);
    ASSERT_TRUE(fromFile) << fromFile.error().what();
    EXPECT_EQ((*fromFile)->getProperty("stopSaveRegression", Json()), Json(42));
    EXPECT_TRUE((*fromFile)->close());
  });

const GameTestRegistration delayedExplosionOutlivesProjectile("ProjectileActions", "DelayedExplosionOutlivesProjectile", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto worldResult = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(worldResult) << worldResult.error().what();
  auto world = *worldResult;
  auto closeWorld = make_scope_exit([&]() { EXPECT_TRUE(world->close()); });
  Vec2I tilePosition(1024, 1024);
  ASSERT_TRUE(world->generateRegion(RectI::with_size(tilePosition - Vec2I(4, 4), Vec2I(9, 9))));
  world->setTileProtectionEnabled(false);
  auto material_database = root.material_database();
  ASSERT_TRUE(material_database) << material_database.error().what();
  auto dirt = (*material_database)->material_id("dirt");
  ASSERT_TRUE(dirt) << dirt.error().what();
  auto modified = world->modifyServerTile(tilePosition, true);
  ASSERT_TRUE(modified) << modified.error().what();
  ASSERT_TRUE(*modified);
  auto& tile = (*modified)->get();
  tile.foreground = *dirt;

  auto projectileDatabase = root.projectileDatabase();
  ASSERT_TRUE(projectileDatabase) << projectileDatabase.error().what();
  auto config = Json::parse(R"({
    "actionOnReap": [{"action":"explosion","delaySteps":6,"foregroundRadius":2,
      "backgroundRadius":0,"explosiveDamageAmount":10000,"harvestLevel":99}],
    "actionOnHit": [], "actionOnTimeout": []
  })");
  ASSERT_TRUE(config) << config.error().what();
  auto projectileResult = (*projectileDatabase)->createProjectile("grenade", *config);
  ASSERT_TRUE(projectileResult) << projectileResult.error().what();
  auto projectile = *projectileResult;
  projectile->setInitialPosition(Vec2F(tilePosition) + Vec2F(0.5f, 0.5f));
  projectile->setInitialVelocity(Vec2F());
  projectile->setSourceEntity(4242, false);
  ASSERT_TRUE(world->addEntity(projectile));
  auto projectileId = projectile->entityId();
  std::weak_ptr<Projectile> retired = projectile;
  ASSERT_TRUE(world->removeEntity(projectileId, true));
  projectile.reset();
  projectileResult->reset();
  ASSERT_TRUE(retired.expired());
  EXPECT_FALSE(world->entity(projectileId));

  ASSERT_TRUE(world->update(0.025f));
  EXPECT_EQ(world->material(tilePosition, TileLayer::Foreground), *dirt);
  auto damagedTile = world->getServerTile(tilePosition);
  ASSERT_TRUE(damagedTile) << damagedTile.error().what();
  EXPECT_FALSE(damagedTile->get().foregroundDamage.dead());
  for (unsigned step = 0; step < 10 && world->material(tilePosition, TileLayer::Foreground) != empty_material_id; ++step)
    ASSERT_TRUE(world->update(0.2f));
  EXPECT_EQ(world->material(tilePosition, TileLayer::Foreground), empty_material_id);
});

const GameTestRegistration firstContactTriggersCollisionActionOnce("ProjectileActions", "FirstContactTriggersCollisionActionOnce", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto created = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(created) << created.error().what();
  auto world = *created;
  auto closeWorld = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
  world->setSpawningEnabled(false);
  world->setTileProtectionEnabled(false);
  Vec2I ground(1024, 1024);
  ASSERT_TRUE(world->generateRegion(RectI::with_size(ground - Vec2I(4, 4), Vec2I(9, 9))));
  auto materials = root.material_database();
  ASSERT_TRUE(materials) << materials.error().what();
  auto dirt = (*materials)->material_id("dirt");
  ASSERT_TRUE(dirt) << dirt.error().what();
  auto attachment = world->modifyServerTile(ground, true);
  ASSERT_TRUE(attachment) << attachment.error().what();
  ASSERT_TRUE(*attachment);
  ASSERT_EQ((*attachment)->get().foreground, empty_material_id);
  // As in the real material-isolation fixture, supply a background attachment:
  // forceModifyTile bypasses protection, not the normal adjacency requirement.
  (*attachment)->get().background = structure_material_id;
  // Production placement derives foreground collision from the material.
  auto placed = world->forceModifyTile(ground, PlaceMaterial{TileLayer::Foreground, *dirt, MaterialHue(0)}, true);
  ASSERT_TRUE(placed) << placed.error().what();
  ASSERT_TRUE(*placed);
  ASSERT_EQ(world->material(ground, TileLayer::Foreground), *dirt);
  auto solid = world->pointTileCollision(Vec2F(ground) + Vec2F(0.5f, 0.5f));
  ASSERT_TRUE(solid) << solid.error().what();
  ASSERT_TRUE(*solid);

  auto database = root.projectileDatabase();
  ASSERT_TRUE(database) << database.error().what();
  auto config = Json::parse(R"({
    "timeToLive":10, "bounces":-1, "power":0, "onlyHitTerrain":true,
    "scripts":[], "periodicActions":[],
    "actionOnReap":[], "actionOnHit":[], "actionOnTimeout":[],
    "movementSettings":{
      "mass":1, "collisionEnabled":true, "gravityEnabled":false,
      "frictionEnabled":false, "stickyCollision":true, "stickyForce":100,
      "maxMovementPerStep":1,
      "collisionPoly":[[-0.125,-0.125],[0.125,-0.125],[0.125,0.125],[-0.125,0.125]]
    },
    "actionOnCollide":[{
      "action":"projectile", "type":"grenade", "offset":[2,2], "direction":[1,0],
      "config":{
        "collisionActionMarker":true, "speed":0, "timeToLive":10,
        "power":0, "onlyHitTerrain":true, "scripts":[], "periodicActions":[],
        "actionOnCollide":[], "actionOnReap":[], "actionOnHit":[], "actionOnTimeout":[],
        "movementSettings":{"collisionEnabled":false,"gravityEnabled":false}
      }
    }]
  })");
  ASSERT_TRUE(config) << config.error().what();
  auto madeProjectile = (*database)->createProjectile("grenade", *config);
  ASSERT_TRUE(madeProjectile) << madeProjectile.error().what();
  auto projectile = *madeProjectile;
  projectile->setInitialPosition(Vec2F(ground) + Vec2F(0.5f, 1.5f));
  projectile->setInitialVelocity(Vec2F(0, -20));
  projectile->setSourceEntity(4242, false);
  ASSERT_TRUE(world->addEntity(projectile));
  auto projectileId = projectile->entityId();
  auto markerCount = [&]() -> std::expected<unsigned, StarException> {
    unsigned count = 0;
    for (auto entityId : world->entity_ids()) {
      if (auto entity = world->get<Projectile>(entityId)) {
        auto marker = entity->configValue("collisionActionMarker", false);
        if (!marker)
          return std::unexpected(std::move(marker.error()));
        if (*marker == Json(true))
          ++count;
      }
    }
    return count;
  };
  auto before = markerCount();
  ASSERT_TRUE(before) << before.error().what();
  EXPECT_EQ(*before, 0u);

  // This first movement crosses the tile top. Sticky contact keeps the parent
  // alive; no earlier non-colliding update initializes the collision-edge state.
  ASSERT_TRUE(world->update(0.025f));
  ASSERT_TRUE(world->entity(projectileId));
  EXPECT_GT(projectile->position()[1], 1025.0f);
  EXPECT_LT(projectile->position()[1], 1025.5f);
  auto firstStep = markerCount();
  ASSERT_TRUE(firstStep) << firstStep.error().what();
  EXPECT_EQ(*firstStep, 0u); // tickShared consumes the collision event next tick.

  ASSERT_TRUE(world->update(0.025f));
  auto firstAction = markerCount();
  ASSERT_TRUE(firstAction) << firstAction.error().what();
  ASSERT_EQ(*firstAction, 1u);
  auto contactPosition = projectile->position();
  for (unsigned tick = 0; tick < 5; ++tick) {
    ASSERT_TRUE(world->update(0.025f));
    ASSERT_TRUE(world->entity(projectileId));
    EXPECT_NEAR(projectile->position()[0], contactPosition[0], 0.001f);
    EXPECT_NEAR(projectile->position()[1], contactPosition[1], 0.001f);
    auto continuedContact = markerCount();
    ASSERT_TRUE(continuedContact) << continuedContact.error().what();
    EXPECT_EQ(*continuedContact, 1u) << "continuous contact must not retrigger actionOnCollide";
  }
  EXPECT_EQ(world->material(ground, TileLayer::Foreground), *dirt);
});

const GameTestRegistration clientSnapshotsRetainPresenceAcrossCameraTransitions(
  "WorldSnapshot", "ClientSnapshotsRetainPresenceAcrossCameraTransitions", [](GameTest& fixture) {
    auto& root = fixture.root();
    auto storage = File::ephemeral_file(root.random());
    ASSERT_TRUE(storage) << storage.error().what();
    auto created = WorldServer::create(root, Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto close = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
    world->setSpawningEnabled(false);
    ASSERT_TRUE(world->setFidelity(WorldServerFidelity::High));
    ASSERT_TRUE(world->generateRegion(RectI(896, 896, 1728, 1152)));
    auto database = root.projectileDatabase();
    ASSERT_TRUE(database) << database.error().what();
    auto parameters = Json::parse(R"({
    "timeToLive":3600,"power":0,"onlyHitTerrain":true,
    "scripts":[],"periodicActions":[],"actionOnCollide":[],
    "actionOnReap":[],"actionOnHit":[],"actionOnTimeout":[],
    "movementSettings":{"collisionEnabled":false,"gravityEnabled":false}
  })");
    ASSERT_TRUE(parameters) << parameters.error().what();
    auto anchor = (*database)->createProjectile("grenade", *parameters);
    ASSERT_TRUE(anchor) << anchor.error().what();
    (*anchor)->setInitialPosition(Vec2F(1000, 1000));
    (*anchor)->setInitialVelocity(Vec2F());
    ASSERT_TRUE(world->addEntity(*anchor));
    auto target = (*database)->createProjectile("grenade", *parameters);
    ASSERT_TRUE(target) << target.error().what();
    (*target)->setInitialPosition(Vec2F(1100, 1000));
    (*target)->setInitialVelocity(Vec2F());
    ASSERT_TRUE(world->addEntity(*target));
    auto factory = root.entityFactory();
    ASSERT_TRUE(factory) << factory.error().what();
    std::array<std::shared_ptr<WorldClientState>, 2> states;
    for (unsigned i = 0; i < states.size(); ++i) {
      auto state = WorldClientState::create(root);
      ASSERT_TRUE(state) << state.error().what();
      states[i] = *state;
      NetCompatibilityRules rules(i ? legacy_version : open_protocol_version);
      states[i]->setNetCompatibilityRules(rules);
      states[i]->setClientPresenceEntities({(*anchor)->entityId()});
      auto added =
        world->addClient(min_client_connection_id + i, SpawnTargetPosition(Vec2F(1024, 1024)), true, false, rules);
      ASSERT_TRUE(added) << added.error().what();
      ASSERT_TRUE(*added);
      ASSERT_TRUE(
        world->handleIncomingPackets(min_client_connection_id + i, {std::make_shared<WorldStartAcknowledgePacket>()}));
    }
    std::array<HashMap<EntityId, std::shared_ptr<Entity>>, 2> visible;
    std::array<HashMap<Vec2I, NetTile>, 2> clientTiles;
    struct RetainedSnapshot {
      std::shared_ptr<TileArrayUpdatePacket> packet;
      ByteArray wire;
      NetCompatibilityRules rules;
    };
    std::vector<RetainedSnapshot> retainedSnapshots;
    for (unsigned phase = 0; phase < 3; ++phase) {
      HashMap<Vec2I, std::shared_ptr<TileArrayUpdatePacket>> sharedSectors;
      for (unsigned i = 0; i < states.size(); ++i) {
        // The presence region remains near the anchor while the camera crosses
        // sectors. Re-entering must create the target again, not reuse a delta
        // version retained from the old visibility set.
        states[i]->setWindow(phase == 1 ? RectI(1472, 960, 1664, 1088) : RectI(960, 960, 1152, 1088));
        auto delta = states[i]->writeDelta();
        ASSERT_TRUE(delta) << delta.error().what();
        ASSERT_TRUE(world->handleIncomingPackets(min_client_connection_id + i,
                                                 {std::make_shared<WorldClientStateUpdatePacket>(*delta)}));
      }
      ASSERT_TRUE(world->setLiquid(Vec2I(1100, 1004), LiquidId(1), 0.5f + phase * 0.1f, 0.5f));
      ASSERT_TRUE(world->update(1.0f / 60.0f));
      for (unsigned i = 0; i < states.size(); ++i) {
        NetCompatibilityRules rules(i ? legacy_version : open_protocol_version);
        auto packets = world->getOutgoingPackets(min_client_connection_id + i);
        ASSERT_TRUE(packets) << packets.error().what();
        bool updateSetSeen = false;
        bool stepSeen = false;
        bool targetCreated = false;
        bool targetDestroyed = false;
        for (auto const& original : *packets) {
          DataStreamBuffer wire;
          wire.set_stream_compatibility_version(rules);
          ASSERT_TRUE(original->write(wire));
          auto decoded = createPacket(original->type());
          ASSERT_TRUE(decoded) << decoded.error().what();
          ASSERT_TRUE(wire.seek(0));
          ASSERT_TRUE((*decoded)->read(wire));
          auto atEnd = wire.at_end();
          ASSERT_TRUE(atEnd);
          EXPECT_TRUE(*atEnd);
          DataStreamBuffer encodedAgain;
          encodedAgain.set_stream_compatibility_version(rules);
          ASSERT_TRUE((*decoded)->write(encodedAgain));
          if (auto expected = as<WorldStartPacket>(original)) {
            auto actual = as<WorldStartPacket>(*decoded);
            ASSERT_TRUE(actual);
            EXPECT_EQ(std::tie(expected->templateData, expected->skyData, expected->weatherData, expected->playerStart,
                               expected->playerRespawn, expected->respawnInWorld, expected->worldProperties,
                               expected->dungeonIdGravity, expected->dungeonIdBreathable, expected->protectedDungeonIds,
                               expected->clientId, expected->local_interpolation_mode),
                      std::tie(actual->templateData, actual->skyData, actual->weatherData, actual->playerStart,
                               actual->playerRespawn, actual->respawnInWorld, actual->worldProperties,
                               actual->dungeonIdGravity, actual->dungeonIdBreathable, actual->protectedDungeonIds,
                               actual->clientId, actual->local_interpolation_mode));
          } else if (auto expected = as<CentralStructureUpdatePacket>(original)) {
            auto actual = as<CentralStructureUpdatePacket>(*decoded);
            ASSERT_TRUE(actual);
            EXPECT_EQ(expected->structureData, actual->structureData);
          } else if (auto expected = as<WorldLayoutUpdatePacket>(original)) {
            auto actual = as<WorldLayoutUpdatePacket>(*decoded);
            ASSERT_TRUE(actual);
            EXPECT_EQ(expected->layoutData, actual->layoutData);
          } else if (auto expected = as<EntityUpdateSetPacket>(original)) {
            auto actual = as<EntityUpdateSetPacket>(*decoded);
            ASSERT_TRUE(actual);
            EXPECT_EQ(expected->forConnection, actual->forConnection);
            EXPECT_EQ(expected->deltas, actual->deltas);
          } else {
            EXPECT_EQ(wire.data(), encodedAgain.data());
          }
          auto const& packet = *decoded;
          if (packet->type() == PacketType::StepUpdate)
            stepSeen = true;
          if (auto array = as<TileArrayUpdatePacket>(packet)) {
            auto source = as<TileArrayUpdatePacket>(original);
            ASSERT_TRUE(source);
            auto [shared, first] = sharedSectors.insert(source->min, source);
            if (!first)
              EXPECT_EQ(source, shared->second);
            retainedSnapshots.push_back({source, wire.data(), rules});
            for (std::size_t x = 0; x < array->array.extent(0); ++x) {
              for (std::size_t y = 0; y < array->array.extent(1); ++y) {
                auto pos = array->min + Vec2I(x, y);
                auto server = world->getServerTile(pos);
                ASSERT_TRUE(server) << server.error().what();
                auto const& tile = server->get();
                NetTile expected;
                expected.foreground = tile.foreground;
                expected.foregroundHueShift = tile.foregroundHueShift;
                expected.foregroundColorVariant = tile.foregroundColorVariant;
                expected.foregroundMod = tile.foregroundMod;
                expected.foregroundModHueShift = tile.foregroundModHueShift;
                expected.background = tile.background;
                expected.backgroundHueShift = tile.backgroundHueShift;
                expected.backgroundColorVariant = tile.backgroundColorVariant;
                expected.backgroundMod = tile.backgroundMod;
                expected.backgroundModHueShift = tile.backgroundModHueShift;
                expected.liquid = tile.liquid.net_update();
                expected.collision = tile.getCollision();
                expected.blockBiomeIndex = tile.blockBiomeIndex;
                expected.environmentBiomeIndex = tile.environmentBiomeIndex;
                expected.dungeonId = tile.dungeonId;
                DataStreamBuffer expectedWire;
                expectedWire.set_stream_compatibility_version(rules);
                ASSERT_TRUE(expectedWire.write(expected));
                DataStreamBuffer actualWire;
                actualWire.set_stream_compatibility_version(rules);
                ASSERT_TRUE(actualWire.write(array->array(x, y)));
                EXPECT_EQ(expectedWire.data(), actualWire.data());
                clientTiles[i].set(pos, array->array(x, y));
              }
            }
          } else if (auto tile = as<TileUpdatePacket>(packet)) {
            EXPECT_TRUE(clientTiles[i].contains(tile->position));
            clientTiles[i].set(tile->position, tile->tile);
          } else if (auto liquid = as<TileLiquidUpdatePacket>(packet)) {
            auto tile = clientTiles[i].get(liquid->position);
            ASSERT_TRUE(tile) << tile.error().what();
            tile->get().liquid = liquid->liquid_update;
          }
          if (auto entityCreate = as<EntityCreatePacket>(packet)) {
            EXPECT_TRUE(stepSeen);
            EXPECT_FALSE(updateSetSeen);
            EXPECT_FALSE(visible[i].contains(entityCreate->entityId));
            auto entity = (*factory)->net_load_entity(entityCreate->entityType, entityCreate->storeData, rules);
            ASSERT_TRUE(entity) << entity.error().what();
            ASSERT_TRUE((*entity)->read_net_state(entityCreate->firstNetState, 0.0f, rules));
            visible[i][entityCreate->entityId] = *entity;
            targetCreated |= entityCreate->entityId == (*target)->entityId();
          } else if (auto entityDestroy = as<EntityDestroyPacket>(packet)) {
            EXPECT_TRUE(visible[i].remove(entityDestroy->entityId));
            EXPECT_FALSE(entityDestroy->death);
            targetDestroyed |= entityDestroy->entityId == (*target)->entityId();
          } else if (auto update = as<EntityUpdateSetPacket>(packet)) {
            updateSetSeen = true;
            for (auto const& [id, delta] : update->deltas) {
              auto entity = visible[i].get(id);
              ASSERT_TRUE(entity) << entity.error().what();
              ASSERT_TRUE(entity->get()->read_net_state(delta, 0.0f, rules));
            }
          }
        }
        EXPECT_TRUE(stepSeen);
        EXPECT_TRUE(visible[i].contains((*anchor)->entityId()));
        EXPECT_EQ(visible[i].contains((*target)->entityId()), phase != 1);
        EXPECT_EQ(targetCreated, phase != 1);
        EXPECT_EQ(targetDestroyed, phase == 1);
        if (phase != 1) {
          auto installed = clientTiles[i].get(Vec2I(1100, 1004));
          ASSERT_TRUE(installed) << installed.error().what();
          auto live = world->getServerTile(Vec2I(1100, 1004));
          ASSERT_TRUE(live) << live.error().what();
          auto expected = live->get().liquid.net_update();
          EXPECT_EQ(installed->get().liquid.liquid, expected.liquid);
          EXPECT_EQ(installed->get().liquid.level, expected.level);
        }
      }
      // Later world changes and snapshots must not mutate packets already
      // handed to an asynchronous consumer, including the other wire version.
      for (auto const& retained : retainedSnapshots) {
        DataStreamBuffer wire;
        wire.set_stream_compatibility_version(retained.rules);
        ASSERT_TRUE(retained.packet->write(wire));
        EXPECT_EQ(wire.data(), retained.wire);
      }
    }
  });
