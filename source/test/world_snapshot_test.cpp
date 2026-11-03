import star.json_extra;
import star.json;
import star.xx_hash;
import star.data_stream_extra;
import star.ordered_map;
import star.rect;
import star.either;
import star.vector;
import star.game_types;
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
import star.version;
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
      auto [promise, keeper] = RpcPromise<Json>::createPair();
      promises.append(std::move(promise));
      messages.append({"batchRegression", {}, std::move(keeper)});
    }
    // Exercise an already-settled keeper both at the failure boundary and
    // during draining: neither error may leave the final request pending.
    if (!handlerFailure)
      ASSERT_TRUE(messages[1].promise.fulfill(Json(9)));
    ASSERT_TRUE(messages[3].promise.fail("Already settled"));
    unsigned handled = 0;
    auto settled = settleWorldMessageBatch(messages,
        [&](String const&, JsonArray const&)
            -> std::expected<std::optional<ChainableJsonMessageResponse>, StarException> {
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
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    ASSERT_TRUE(File::writeFile(String("[]"), File::relativeTo(*directory, "preload.config")));
    ASSERT_TRUE(File::writeFile(String(R"({
      "name":"seeded","type":"perlin","function":"perlin","octaves":3,"freq":0.03,"amp":5
    })"),
                                File::relativeTo(*directory, "seeded.terrain")));
    ASSERT_TRUE(File::writeFile(String(R"({"name":"badType","type":"unknown-selector-type"})"),
                                File::relativeTo(*directory, "bad.terrain")));
    Assets::Settings settings{};
    settings.workerPoolSize = 1;
    settings.assetTimeToLive = 10;
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
    auto storage = File::ephemeralFile(fixture.root().random());
    ASSERT_TRUE(storage) << storage.error().what();
    auto created = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto closeWorld = finally([&] { EXPECT_TRUE(world->close()); });
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

// World generation is deterministic: it is a pure function of the celestial
// parameters (the coordinate feeds the planet seed, the system/type fields come
// from fixed Perlin seeds in the assets), so these hashes are stable across
// runs and machines and can be compared to a constant.  When world generation
// changes on purpose, rerun with --gtest_filter='WorldSnapshot.*'; the new hash
// is printed to stderr.
const GameTestRegistration fixedCoordinateGenerationIsDeterministic("WorldSnapshot", "FixedCoordinateGenerationIsDeterministic", [](GameTest& fixture) {
  auto storagePath = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(storagePath) << storagePath.error().what();
  auto finallyGuard = finally([&storagePath]() { EXPECT_TRUE(File::removeDirectoryRecursive(*storagePath)); });
  auto celestialDatabase = CelestialMasterDatabase::create(fixture.root(), File::relativeTo(*storagePath, "universe.chunks"));
  ASSERT_TRUE(celestialDatabase) << celestialDatabase.error().what();
  auto closeDatabase = finally([&]() { EXPECT_TRUE((*celestialDatabase)->close()); });

  // Find a real planet: the celestial generator places systems sparsely, and a
  // hand picked coordinate is most likely empty.
  auto systems = (*celestialDatabase)->scanSystems(RectI::withSize(Vec2I(0, 0), Vec2I(32, 32)));
  ASSERT_TRUE(systems) << systems.error().what();
  ASSERT_FALSE(systems->empty());
  std::optional<CelestialCoordinate> coordinate;
  for (auto const& system : *systems) {
    auto children = (*celestialDatabase)->children(system);
    ASSERT_TRUE(children) << children.error().what();
    for (auto const& child : *children) {
      auto parameters = (*celestialDatabase)->parameters(child);
      ASSERT_TRUE(parameters) << parameters.error().what();
      if (child.isPlanetaryBody() && *parameters) {
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
  auto surfaceTemplate = WorldTemplate::create(fixture.root(), *coordinate, *celestialDatabase);
  ASSERT_TRUE(surfaceTemplate) << surfaceTemplate.error().what();
  auto surfaceLevel = (*surfaceTemplate)->surfaceLevel();

  auto makeWorld = [&]() -> std::expected<std::shared_ptr<WorldServer>, StarException> {
    auto worldTemplate = WorldTemplate::create(fixture.root(), *coordinate, *celestialDatabase);
    if (!worldTemplate)
      return std::unexpected(std::move(worldTemplate.error()));
    auto storage = File::ephemeralFile(fixture.root().random());
    if (!storage)
      return std::unexpected(std::move(storage.error()));
    return WorldServer::create(fixture.root(), *worldTemplate, *storage);
  };
  auto firstWorld = makeWorld();
  ASSERT_TRUE(firstWorld) << firstWorld.error().what();
  auto closeFirst = finally([&]() { EXPECT_TRUE((*firstWorld)->close()); });
  auto secondWorld = makeWorld();
  ASSERT_TRUE(secondWorld) << secondWorld.error().what();
  auto closeSecond = finally([&]() { EXPECT_TRUE((*secondWorld)->close()); });

  auto firstHash = sampleWorld(**firstWorld, surfaceLevel);
  ASSERT_TRUE(firstHash) << firstHash.error().what();
  auto secondHash = sampleWorld(**secondWorld, surfaceLevel);
  ASSERT_TRUE(secondHash) << secondHash.error().what();
  std::cerr << "WorldSnapshot: " << coordinate->id() << " hashes to 0x" << std::hex << *firstHash << std::dec << std::endl;

  EXPECT_EQ(*firstHash, *secondHash);
  EXPECT_EQ(*firstHash, 0x801745d507acfcaaull) << "update the expected hash (printed above)";
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
  auto shipPaths = jsonToStringList(*humanShips);
  ASSERT_TRUE(shipPaths) << shipPaths.error().what();
  ASSERT_FALSE(shipPaths->empty());
  auto shipStructure = WorldStructure::create(fixture.root(), (*shipPaths)[0]);
  ASSERT_TRUE(shipStructure) << shipStructure.error().what();
  Vec2U worldSize(2048, 2048);
  auto jWorldSize = shipStructure->configValue("worldSize");
  ASSERT_TRUE(jWorldSize) << jWorldSize.error().what();
  if (*jWorldSize) {
    auto size = jsonToVec2U(*jWorldSize);
    ASSERT_TRUE(size) << size.error().what();
    worldSize = *size;
  }

  auto makeWorld = [&]() -> std::expected<std::shared_ptr<WorldServer>, StarException> {
    auto storage = File::ephemeralFile(fixture.root().random());
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
  auto closeFirst = finally([&]() { EXPECT_TRUE((*firstWorld)->close()); });
  auto secondWorld = makeWorld();
  ASSERT_TRUE(secondWorld) << secondWorld.error().what();
  auto closeSecond = finally([&]() { EXPECT_TRUE((*secondWorld)->close()); });

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
      if (firstTile->get().foreground != EmptyMaterialId)
        ++nonEmptyTiles;
    }
  }
  std::cerr << "WorldSnapshot: ship world hashes to 0x" << std::hex << firstHash << std::dec << std::endl;

  EXPECT_GT(nonEmptyTiles, 0u) << "the sampled region does not contain any ship structure";
  EXPECT_EQ(firstHash, secondHash);
  EXPECT_EQ(firstHash, 0x96a3df5c62832b0bull) << "update the expected hash (printed above)";
});

const GameTestRegistration stoppedWorldRetainsClientsAndExportsRecoverableChunks("WorldSnapshot", "StoppedWorldRetainsClientsAndExportsRecoverableChunks", [](GameTest& fixture) {
  auto storageResult = File::temporaryFile(fixture.root().random());
  ASSERT_TRUE(storageResult) << storageResult.error().what();
  auto storage = *storageResult;
  auto path = storage->fileName();
  auto cleanup = finally([&]() {
    EXPECT_TRUE(storage->close());
    EXPECT_TRUE(File::remove(path));
  });
  auto worldResult = WorldServer::create(fixture.root(), Vec2U(2048, 2048), storage);
  ASSERT_TRUE(worldResult) << worldResult.error().what();
  auto world = *worldResult;
  auto closeWorld = finally([&]() { EXPECT_TRUE(world->close()); });
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto speciesShips = (*assets)->json("/universe_server.config:speciesShips");
  ASSERT_TRUE(speciesShips) << speciesShips.error().what();
  auto humanShips = speciesShips->get("human");
  ASSERT_TRUE(humanShips) << humanShips.error().what();
  auto shipPaths = jsonToStringList(*humanShips);
  ASSERT_TRUE(shipPaths) << shipPaths.error().what();
  ASSERT_FALSE(shipPaths->empty());
  auto structure = WorldStructure::create(fixture.root(), (*shipPaths)[0]);
  ASSERT_TRUE(structure) << structure.error().what();
  ASSERT_TRUE(world->setCentralStructure(*structure));
  ASSERT_TRUE(world->setProperty("stopSaveRegression", Json(42)));

  auto threadResult = WorldServerThread::create(world, ClientShipWorldId(Uuid(fixture.root().random())));
  ASSERT_TRUE(threadResult) << threadResult.error().what();
  auto& thread = **threadResult;
  auto added = thread.addClient(MinClientConnectionId, SpawnTargetPosition(Vec2F(1024, 1024)),
      true, false, NetCompatibilityRules{}, MainClientWorldId);
  ASSERT_TRUE(added) << added.error().what();
  ASSERT_TRUE(*added);
  ASSERT_TRUE(thread.start());
  ASSERT_TRUE(thread.stop());

  // UniverseServer still needs this list to migrate players after joining.
  ASSERT_TRUE(thread.hasClient(MinClientConnectionId));
  EXPECT_EQ(thread.clients().size(), 1u);
  EXPECT_FALSE(thread.serverErrorOccurred());
  auto descriptor = ItemDescriptor::create("money", 7, Json());
  ASSERT_TRUE(descriptor) << descriptor.error().what();
  auto finalItem = std::make_shared<GiveItemPacket>(std::move(*descriptor));
  auto sent = world->sendPacket(MinClientConnectionId, finalItem);
  ASSERT_TRUE(sent);

  // An asynchronous warp may wait for its destination.  Merely queueing it
  // must leave the stopped source alive, with its final outgoing queue intact.
  auto waitingChunks = thread.readChunks();
  ASSERT_TRUE(waitingChunks) << waitingChunks.error().what();
  ASSERT_FALSE(waitingChunks->empty());
  ASSERT_TRUE(thread.hasClient(MinClientConnectionId));
  ASSERT_TRUE(storage->isOpen());

  // Ship/custom-world snapshots are exported after stop, before final close.
  ASSERT_TRUE(thread.unloadAll(true));
  auto chunks = thread.readChunks();
  ASSERT_TRUE(chunks) << chunks.error().what();
  ASSERT_FALSE(chunks->empty());
  ASSERT_FALSE(thread.serverErrorOccurred());
  auto finalPackets = thread.removeClient(MinClientConnectionId);
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
  auto storage = File::ephemeralFile(root.random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto worldResult = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(worldResult) << worldResult.error().what();
  auto world = *worldResult;
  auto closeWorld = finally([&]() { EXPECT_TRUE(world->close()); });
  Vec2I tilePosition(1024, 1024);
  ASSERT_TRUE(world->generateRegion(RectI::withSize(tilePosition - Vec2I(4, 4), Vec2I(9, 9))));
  world->setTileProtectionEnabled(false);
  auto materialDatabase = root.materialDatabase();
  ASSERT_TRUE(materialDatabase) << materialDatabase.error().what();
  auto dirt = (*materialDatabase)->materialId("dirt");
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
  for (unsigned step = 0; step < 10 && world->material(tilePosition, TileLayer::Foreground) != EmptyMaterialId; ++step)
    ASSERT_TRUE(world->update(0.2f));
  EXPECT_EQ(world->material(tilePosition, TileLayer::Foreground), EmptyMaterialId);
});

const GameTestRegistration firstContactTriggersCollisionActionOnce("ProjectileActions", "FirstContactTriggersCollisionActionOnce", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto storage = File::ephemeralFile(root.random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto created = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(created) << created.error().what();
  auto world = *created;
  auto closeWorld = finally([&] { EXPECT_TRUE(world->close()); });
  world->setSpawningEnabled(false);
  world->setTileProtectionEnabled(false);
  Vec2I ground(1024, 1024);
  ASSERT_TRUE(world->generateRegion(RectI::withSize(ground - Vec2I(4, 4), Vec2I(9, 9))));
  auto materials = root.materialDatabase();
  ASSERT_TRUE(materials) << materials.error().what();
  auto dirt = (*materials)->materialId("dirt");
  ASSERT_TRUE(dirt) << dirt.error().what();
  auto attachment = world->modifyServerTile(ground, true);
  ASSERT_TRUE(attachment) << attachment.error().what();
  ASSERT_TRUE(*attachment);
  ASSERT_EQ((*attachment)->get().foreground, EmptyMaterialId);
  // As in the real material-isolation fixture, supply a background attachment:
  // forceModifyTile bypasses protection, not the normal adjacency requirement.
  (*attachment)->get().background = StructureMaterialId;
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
    for (auto entityId : world->entityIds()) {
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
    auto storage = File::ephemeralFile(root.random());
    ASSERT_TRUE(storage) << storage.error().what();
    auto created = WorldServer::create(root, Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto close = finally([&] { EXPECT_TRUE(world->close()); });
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
      NetCompatibilityRules rules(i ? LegacyVersion : OpenProtocolVersion);
      states[i]->setNetCompatibilityRules(rules);
      states[i]->setClientPresenceEntities({(*anchor)->entityId()});
      auto added =
        world->addClient(MinClientConnectionId + i, SpawnTargetPosition(Vec2F(1024, 1024)), true, false, rules);
      ASSERT_TRUE(added) << added.error().what();
      ASSERT_TRUE(*added);
      ASSERT_TRUE(
        world->handleIncomingPackets(MinClientConnectionId + i, {std::make_shared<WorldStartAcknowledgePacket>()}));
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
        ASSERT_TRUE(world->handleIncomingPackets(MinClientConnectionId + i,
                                                 {std::make_shared<WorldClientStateUpdatePacket>(*delta)}));
      }
      ASSERT_TRUE(world->setLiquid(Vec2I(1100, 1004), LiquidId(1), 0.5f + phase * 0.1f, 0.5f));
      ASSERT_TRUE(world->update(1.0f / 60.0f));
      for (unsigned i = 0; i < states.size(); ++i) {
        NetCompatibilityRules rules(i ? LegacyVersion : OpenProtocolVersion);
        auto packets = world->getOutgoingPackets(MinClientConnectionId + i);
        ASSERT_TRUE(packets) << packets.error().what();
        bool updateSetSeen = false;
        bool stepSeen = false;
        bool targetCreated = false;
        bool targetDestroyed = false;
        for (auto const& original : *packets) {
          DataStreamBuffer wire;
          wire.setStreamCompatibilityVersion(rules);
          ASSERT_TRUE(original->write(wire, rules));
          auto decoded = createPacket(original->type());
          ASSERT_TRUE(decoded) << decoded.error().what();
          ASSERT_TRUE(wire.seek(0));
          ASSERT_TRUE((*decoded)->read(wire, rules));
          auto atEnd = wire.atEnd();
          ASSERT_TRUE(atEnd);
          EXPECT_TRUE(*atEnd);
          DataStreamBuffer encodedAgain;
          encodedAgain.setStreamCompatibilityVersion(rules);
          ASSERT_TRUE((*decoded)->write(encodedAgain, rules));
          if (auto expected = as<WorldStartPacket>(original)) {
            auto actual = as<WorldStartPacket>(*decoded);
            ASSERT_TRUE(actual);
            EXPECT_EQ(std::tie(expected->templateData, expected->skyData, expected->weatherData, expected->playerStart,
                               expected->playerRespawn, expected->respawnInWorld, expected->worldProperties,
                               expected->dungeonIdGravity, expected->dungeonIdBreathable, expected->protectedDungeonIds,
                               expected->clientId, expected->localInterpolationMode),
                      std::tie(actual->templateData, actual->skyData, actual->weatherData, actual->playerStart,
                               actual->playerRespawn, actual->respawnInWorld, actual->worldProperties,
                               actual->dungeonIdGravity, actual->dungeonIdBreathable, actual->protectedDungeonIds,
                               actual->clientId, actual->localInterpolationMode));
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
            for (std::size_t x = 0; x < array->array.size(0); ++x) {
              for (std::size_t y = 0; y < array->array.size(1); ++y) {
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
                expected.liquid = tile.liquid.netUpdate();
                expected.collision = tile.getCollision();
                expected.blockBiomeIndex = tile.blockBiomeIndex;
                expected.environmentBiomeIndex = tile.environmentBiomeIndex;
                expected.dungeonId = tile.dungeonId;
                DataStreamBuffer expectedWire;
                expectedWire.setStreamCompatibilityVersion(rules);
                ASSERT_TRUE(expectedWire.write(expected));
                DataStreamBuffer actualWire;
                actualWire.setStreamCompatibilityVersion(rules);
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
            tile->get().liquid = liquid->liquidUpdate;
          }
          if (auto entityCreate = as<EntityCreatePacket>(packet)) {
            EXPECT_TRUE(stepSeen);
            EXPECT_FALSE(updateSetSeen);
            EXPECT_FALSE(visible[i].contains(entityCreate->entityId));
            auto entity = (*factory)->netLoadEntity(entityCreate->entityType, entityCreate->storeData, rules);
            ASSERT_TRUE(entity) << entity.error().what();
            ASSERT_TRUE((*entity)->readNetState(entityCreate->firstNetState, 0.0f, rules));
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
              ASSERT_TRUE(entity->get()->readNetState(delta, 0.0f, rules));
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
          auto expected = live->get().liquid.netUpdate();
          EXPECT_EQ(installed->get().liquid.liquid, expected.liquid);
          EXPECT_EQ(installed->get().liquid.level, expected.level);
        }
      }
      // Later world changes and snapshots must not mutate packets already
      // handed to an asynchronous consumer, including the other wire version.
      for (auto const& retained : retainedSnapshots) {
        DataStreamBuffer wire;
        wire.setStreamCompatibilityVersion(retained.rules);
        ASSERT_TRUE(static_cast<Packet const&>(*retained.packet).write(wire, retained.rules));
        EXPECT_EQ(wire.data(), retained.wire);
      }
    }
  });
