#include <stdio.h>

import star.xx_hash;
import star.json;
import star.json_extra;
import star.id_map;
import star.vector;
import star.set;
import star.map;
import star.random;
import star.game_types;
import star.material_types;
import star.poly;
import star.list;
import star.bi_map;
import star.math_common;
import star.format;
import star.version;
import star.data_stream;
import star.logging;
import star.data_stream_extra;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import star.casting;
import star.rpc_promise;
import std;
import star.optional;
import star.rect;
import star.thread;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.string;
import star.image;
import star.ordered_map;
import star.net_compatibility;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.io_device;
import star.ref_ptr;
import star.listener;
import star.spatial_hash_2d;
import star.lua;
import star.periodic;
import star.periodic_function;
import star.audio;
import star.a_star;
import star.lock_file;
import star.byte_array;

using ZSTD_CCtx = struct ZSTD_CCtx_s;
using ZSTD_DCtx = struct ZSTD_DCtx_s;
using ZSTD_CStream = ZSTD_CCtx;
using ZSTD_DStream = ZSTD_DCtx;
import star.zstd_compression;

import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.sky_types;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.drawable;
import star.item;
import star.item_descriptor;
import star.item_recipe;
import star.lua_root;
import star.tile_modification;
import star.world;
import star.celestial_types;
import star.chat_types;
import star.inspectable_entity;
import star.plant;
import star.world_storage;
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
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.world_render_data;
import star.world_client_state;
import star.interpolation_tracker;
import star.spawner;
import star.world_server;
import star.asset_source;
import star.assets;

import star.configuration;
import star.root;
import star.plant_database;
import star.biome_placement;
import star.spawn_type_database;
import star.biome;
import star.world_geometry;
import star.wiring;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.wire_entity;
import star.blocks_along_line;
import star.world_template;
import star.liquids_database;
import star.material_database;
import star.entity_map;
import star.world_impl;
import star.dungeon_generator;
import star.movement_controller;
import star.mobile_entity;
import star.scripted_entity;
import star.game_timers;
import star.item_drop;
import star.lua_animation_component;
import star.status_effect_entity;
import star.chat_action;
import star.chatty_entity;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.entity_rendering;
import star.object;
import star.container_entity;
import star.force_regions;
import star.physics_entity;
import star.effect_emitter;
import star.projectile;
import star.humanoid;
import star.anchorable_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.ai_types;
import star.lua_actor_movement_component;
import star.player;
import star.uuid;
import star.warping;
import star.warp_target_entity;
import star.universe_settings;
import star.universe;
import star.host_address;
import star.world_server_thread;
import star.system_world_server;
import star.system_world_server_thread;
import star.socket;
import star.tcp;
import star.p2p_networking_service;
import star.net_packet_socket;
import star.universe_connection;
import star.universe_server;
import star.universe_server_lua_bindings;
import star.celestial_lua_bindings;
import star.world_generation;
import star.radio_message_database;
import star.versioning_database;
import star.object_database;
import star.item_database;
import star.biome_database;
import star.wire_processor;
import star.entity_factory;


import star.sky;
import star.weather;

import star.falling_blocks_agent;

import star.line;
import star.algorithm;
namespace Star {

extern std::expected<EnumMap<WorldServerFidelity>, StarException> const WorldServerFidelityNames = EnumMap<WorldServerFidelity>::create({
  {WorldServerFidelity::Minimum, "minimum"},
  {WorldServerFidelity::Low, "low"},
  {WorldServerFidelity::Medium, "medium"},
  {WorldServerFidelity::High, "high"}
});

WorldServer::WorldServer(Root& root) : World(root) {}

std::expected<std::shared_ptr<WorldServer>, StarException> WorldServer::create(Root& root, std::shared_ptr<WorldTemplate> const& worldTemplate, std::shared_ptr<IODevice> storage) {
  auto world = std::shared_ptr<WorldServer>(new WorldServer(root));
  auto spawner = Spawner::create(root);
  if (!spawner)
    return std::unexpected(std::move(spawner.error()));
  world->m_spawner = std::move(*spawner);
  world->m_worldTemplate = worldTemplate;
  auto worldStorage = WorldStorage::create(root, worldTemplate->size(), std::move(storage), std::make_shared<WorldGenerator>(*world));
  if (!worldStorage)
    return std::unexpected(std::move(worldStorage.error()));
  world->m_worldStorage = std::move(*worldStorage);
  world->m_adjustPlayerStart = true;
  auto settings = UniverseSettings::create(root);
  if (!settings)
    return std::unexpected(std::move(settings.error()));
  world->m_universeSettings = std::move(*settings);
  world->m_worldId = worldTemplate->worldName();
  world->m_expiryTimer = GameTimer(0.0f);
  if (auto result = world->init(true); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = world->writeMetadata(); !result)
    return std::unexpected(std::move(result.error()));
  world->m_initialized = true;
  return world;
}

std::expected<std::shared_ptr<WorldServer>, StarException> WorldServer::create(Root& root, Vec2U const& size, std::shared_ptr<IODevice> storage) {
  auto worldTemplate = WorldTemplate::create(root, size);
  if (!worldTemplate)
    return std::unexpected(std::move(worldTemplate.error()));
  return create(root, *worldTemplate, std::move(storage));
}

std::expected<std::shared_ptr<WorldServer>, StarException> WorldServer::create(Root& root, std::shared_ptr<IODevice> const& storage) {
  auto world = std::shared_ptr<WorldServer>(new WorldServer(root));
  auto spawner = Spawner::create(root);
  if (!spawner)
    return std::unexpected(std::move(spawner.error()));
  world->m_spawner = std::move(*spawner);
  auto worldStorage = WorldStorage::create(root, storage, std::make_shared<WorldGenerator>(*world));
  if (!worldStorage)
    return std::unexpected(std::move(worldStorage.error()));
  world->m_worldStorage = std::move(*worldStorage);
  auto settings = UniverseSettings::create(root);
  if (!settings)
    return std::unexpected(std::move(settings.error()));
  world->m_universeSettings = std::move(*settings);
  world->m_worldId = "Nowhere";
  if (auto result = world->readMetadata(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = world->init(false); !result)
    return std::unexpected(std::move(result.error()));
  world->m_initialized = true;
  return world;
}

std::expected<std::shared_ptr<WorldServer>, StarException> WorldServer::create(Root& root, WorldChunks const& chunks) {
  auto world = std::shared_ptr<WorldServer>(new WorldServer(root));
  auto spawner = Spawner::create(root);
  if (!spawner)
    return std::unexpected(std::move(spawner.error()));
  world->m_spawner = std::move(*spawner);
  auto worldStorage = WorldStorage::create(root, chunks, std::make_shared<WorldGenerator>(*world));
  if (!worldStorage)
    return std::unexpected(std::move(worldStorage.error()));
  world->m_worldStorage = std::move(*worldStorage);
  auto settings = UniverseSettings::create(root);
  if (!settings)
    return std::unexpected(std::move(settings.error()));
  world->m_universeSettings = std::move(*settings);
  world->m_worldId = "Nowhere";
  if (auto result = world->readMetadata(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = world->init(false); !result)
    return std::unexpected(std::move(result.error()));
  world->m_initialized = true;
  return world;
}

WorldServer::~WorldServer() {
  if (auto result = close(); !result) {
    if (auto written = Star::print(stderr, "Error closing world '{}' during destruction: {}\n", m_worldId.utf8_data(), result.error().what()); !written)
      std::_Exit(1);
  }
}

std::expected<void, StarException> WorldServer::close() {
  if (std::exchange(m_closeAttempted, true))
    return {};

  std::optional<StarException> error;
  auto recordError = [&](StarException next) {
    if (error)
      *error = error->with_context(Star::format("Additional error closing world: {}", next.what()));
    else
      error = std::move(next);
  };
  for (auto&& p : m_scriptContexts) {
    if (auto result = recoverLuaScriptError(p.second->uninit()); !result)
      recordError(std::move(result.error()));
  }
  m_scriptContexts.clear();
  if (m_spawner) {
    if (auto result = m_spawner->uninit(); !result)
      recordError(std::move(result.error()));
  }

  // Never commit partially initialized state or a save whose teardown/metadata
  // failed. Storage destruction retains its existing rollback-only semantics.
  if (m_initialized && !error) {
    if (auto result = writeMetadata(); !result)
      recordError(std::move(result.error()));
  }
  if (m_worldStorage && m_initialized && !error) {
    if (auto result = m_worldStorage->close(); !result)
      recordError(std::move(result.error()));
  } else {
    m_worldStorage.reset();
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

void WorldServer::setWorldId(String worldId) {
  m_worldId = std::move(worldId);
}

String const& WorldServer::worldId() const {
  return m_worldId;
}

void WorldServer::setUniverseSettings(std::shared_ptr<UniverseSettings> universeSettings) {
  m_universeSettings = std::move(universeSettings);
}

std::shared_ptr<UniverseSettings> WorldServer::universeSettings() const {
  return m_universeSettings;
}

void WorldServer::setReferenceClock(std::shared_ptr<Clock> clock) {
  for (auto const& pair : m_weatherDomains)
    pair.second->setReferenceClock(clock);
  if (m_emptyWeather)
    m_emptyWeather->setReferenceClock(clock);
  m_sky->setReferenceClock(clock);
  m_referenceClock = std::move(clock);
}

std::expected<void, StarException> WorldServer::setupWeatherDomains(bool preserveNonSurface) {
  auto previousDomains = std::exchange(m_weatherDomains, {});
  m_weatherDomains.clear();

  auto effectsActive = [&world = *this](Vec2I const& pos) -> std::expected<bool, StarException> {
  auto const& tile = world.m_tileArray->tile(pos);
  return !isRealMaterial(tile.background);
  };

  for (auto const& domain : m_worldTemplate->weatherDomains()) {
    if (preserveNonSurface && domain.name != "surface" && previousDomains.contains(domain.name)) {
      auto result1 = previousDomains.get(domain.name);
      if (!result1)
        return std::unexpected(std::move(result1.error()));
      m_weatherDomains.set(domain.name, result1->get());
      continue;
    }

    auto result2 = ServerWeather::create(this->root());
    if (!result2)
      return std::unexpected(std::move(result2.error()));
    auto weather = std::move(*result2);
    weather->setup(domain.pool, domain.effectsMinHeight, m_geometry, effectsActive);
    weather->setReferenceClock(m_referenceClock);
    m_weatherDomains.set(domain.name, std::exchange(weather, {}));
  }

  auto result3 = ServerWeather::create(this->root());
  if (!result3)
    return std::unexpected(std::move(result3.error()));
  m_emptyWeather = std::move(*result3);
  m_emptyWeather->setup({}, 0, m_geometry, effectsActive);
  m_emptyWeather->setReferenceClock(m_referenceClock);

  for (auto const& pair : m_clientInfo) {
    pair.second->weatherDomain.reset();
    pair.second->weatherNetVersion = 0;
  }
  return {};
}

std::shared_ptr<ServerWeather> WorldServer::weatherForDomain(std::optional<String> const& domain) const {
  if (domain && m_weatherDomains.contains(*domain))
    return m_weatherDomains.find(*domain)->second;
  return m_emptyWeather;
}

std::expected<std::shared_ptr<ServerWeather>, StarException> WorldServer::weatherAt(Vec2F const& position) const {
  auto result4 = m_worldTemplate->weatherLayerAt(Vec2I::floor(position));
  if (!result4)
    return std::unexpected(std::move(result4.error()));
  if (auto layer = std::move(*result4))
    return weatherForDomain(layer->get().domain);
  return m_emptyWeather;
}

std::shared_ptr<ServerWeather> WorldServer::surfaceWeather() const {
  return weatherForDomain(String("surface"));
}

void WorldServer::setPause(bool pause) {
  if (m_referenceClock && pause)
    m_referenceClock->stop();
  else if (m_referenceClock)
    m_referenceClock->start();
}

std::expected<void, StarException> WorldServer::initLua(UniverseServer& universe) {
  auto result5 = LuaBindings::makeUniverseServerCallbacks(this->root(), universe);
  if (!result5)
    return std::unexpected(std::move(result5.error()));
  m_luaThreadCallbacks["universe"] = std::move(*result5);
  auto result6 = LuaBindings::makeCelestialCallbacks(this->root(), universe);
  if (!result6)
    return std::unexpected(std::move(result6.error()));
  m_luaThreadCallbacks["celestial"] = std::move(*result6);
  m_luaRoot->addCallbacks("universe",m_luaThreadCallbacks["universe"]);
  m_luaRoot->addCallbacks("celestial",m_luaThreadCallbacks["celestial"]);
  auto result7 = this->root().assets();
  if (!result7)
    return std::unexpected(std::move(result7.error()));
  auto assets = std::move(*result7);
  auto result8 = assets->json("/worldserver.config:scriptContexts");
  if (!result8)
    return std::unexpected(std::move(result8.error()));
  auto result9 = std::move(*result8).toObject();
  if (!result9)
    return std::unexpected(std::move(result9.error()));
  for (auto&& p : std::move(*result9)) {
    auto scriptComponent = std::make_shared<ScriptComponent>(this->root());
    auto result10 = p.second.toArray();
    if (!result10)
      return std::unexpected(std::move(result10.error()));
    auto result11 = jsonToStringList(std::move(*result10));
    if (!result11)
      return std::unexpected(std::move(result11.error()));
    auto result12 = scriptComponent->setScripts(std::move(*result11));
    if (!result12)
      return std::unexpected(std::move(result12.error()));

    m_scriptContexts.set(p.first, scriptComponent);
    auto result13 = recoverLuaScriptError(scriptComponent->init(*this));
    if (!result13)
      return std::unexpected(std::move(result13.error()));
  }
  return {};
}

std::expected<WorldStructure, StarException> WorldServer::setCentralStructure(WorldStructure centralStructure) {
  auto result14 = removeCentralStructure();
  if (!result14)
    return std::unexpected(std::move(result14.error()));

  m_centralStructure = std::move(centralStructure);
  m_centralStructure.setAnchorPosition(Vec2I(m_geometry.size()) / 2);

  auto playerSpawnBlocks = m_centralStructure.flaggedBlocks("playerSpawn");
  auto result15 = playerSpawnBlocks.first();
  if (!result15)
    return std::unexpected(std::move(result15.error()));
  m_playerStart = Vec2F(result15->get()) + Vec2F(0, 1);
  m_adjustPlayerStart = false;

  auto result16 = this->root().materialDatabase();
  if (!result16)
    return std::unexpected(std::move(result16.error()));
  auto materialDatabase = std::move(*result16);
  for (auto const& foregroundBlock : m_centralStructure.foregroundBlocks()) {
    auto result17 = generateRegion(RectI::withSize(foregroundBlock.position, {1, 1}));
    if (!result17)
      return std::unexpected(std::move(result17.error()));
    if (auto tile = m_tileArray->modifyTile(foregroundBlock.position)) {
      if (tile->get().foreground == EmptyMaterialId) {
        tile->get().foreground = foregroundBlock.materialId;
        tile->get().foregroundColorVariant = foregroundBlock.materialColor;
        tile->get().foregroundHueShift = foregroundBlock.materialHue;
        tile->get().foregroundMod = foregroundBlock.materialMod;
        auto result18 = materialDatabase->materialCollisionKind(foregroundBlock.materialId);
        if (!result18)
          return std::unexpected(std::move(result18.error()));
        tile->get().updateCollision(std::move(*result18));
        auto result19 = queueTileUpdates(foregroundBlock.position);
        if (!result19)
          return std::unexpected(std::move(result19.error()));
        auto result20 = dirtyCollision(RectI::withSize(foregroundBlock.position, {1, 1}));
        if (!result20)
          return std::unexpected(std::move(result20.error()));
      }
    }
  }

  for (auto const& backgroundBlock : m_centralStructure.backgroundBlocks()) {
    auto result21 = generateRegion(RectI::withSize(backgroundBlock.position, {1, 1}));
    if (!result21)
      return std::unexpected(std::move(result21.error()));
    if (auto tile = m_tileArray->modifyTile(backgroundBlock.position)) {
      if (tile->get().background == EmptyMaterialId) {
        tile->get().background = backgroundBlock.materialId;
        tile->get().backgroundColorVariant = backgroundBlock.materialColor;
        tile->get().backgroundHueShift = backgroundBlock.materialHue;
        tile->get().backgroundMod = backgroundBlock.materialMod;
        auto result22 = queueTileUpdates(backgroundBlock.position);
        if (!result22)
          return std::unexpected(std::move(result22.error()));
      }
    }
  }

  auto result23 = this->root().objectDatabase();
  if (!result23)
    return std::unexpected(std::move(result23.error()));
  auto objectDatabase = std::move(*result23);
  for (auto structureObject : m_centralStructure.objects()) {
    auto result24 = generateRegion(RectI::withSize(structureObject.position, {1, 1}));
    if (!result24)
      return std::unexpected(std::move(result24.error()));
    auto result25 = objectDatabase->createForPlacement(*this, structureObject.name, structureObject.position, structureObject.direction, structureObject.parameters);
    if (!result25)
      return std::unexpected(std::move(result25.error()));
    if (auto object = std::move(*result25)) {
      auto result26 = addEntity(object);
      if (!result26)
        return std::unexpected(std::move(result26.error()));
    }
  }

  for (auto const& pair : m_clientInfo) {
    auto result27 = m_centralStructure.store();
    if (!result27)
      return std::unexpected(std::move(result27.error()));
    pair.second->outgoingPackets.push_back(std::make_shared<CentralStructureUpdatePacket>(std::move(*result27)));
  }

  return m_centralStructure;
}

WorldStructure const& WorldServer::centralStructure() const {
  return m_centralStructure;
}

std::expected<void, StarException> WorldServer::removeCentralStructure() {
  for (auto const& structureObject : m_centralStructure.objects()) {
    if (!structureObject.residual) {
      auto result28 = generateRegion(RectI::withSize(structureObject.position, {1, 1}));
      if (!result28)
        return std::unexpected(std::move(result28.error()));
      auto result29 = atTile<Object>(structureObject.position);
      if (!result29)
        return std::unexpected(std::move(result29.error()));
      for (auto const& objectEntity : std::move(*result29)) {
        bool condition31 = static_cast<bool>(objectEntity->tilePosition() == structureObject.position);
        if (condition31) {
          auto result30 = objectEntity->name();
          if (!result30)
            return std::unexpected(std::move(result30.error()));
          condition31 = static_cast<bool>(std::move(*result30) == structureObject.name);
        }
        if (condition31) {
          auto result32 = removeEntity(objectEntity->entityId(), false);
          if (!result32)
            return std::unexpected(std::move(result32.error()));
        }
      }
    }
  }

  for (auto const& backgroundBlock : m_centralStructure.backgroundBlocks()) {
    if (!backgroundBlock.residual) {
      auto result33 = generateRegion(RectI::withSize(backgroundBlock.position, {1, 1}));
      if (!result33)
        return std::unexpected(std::move(result33.error()));
      if (auto tile = m_tileArray->modifyTile(backgroundBlock.position)) {
        if (tile->get().background == backgroundBlock.materialId) {
          tile->get().background = EmptyMaterialId;
          tile->get().backgroundMod = NoModId;
          auto result34 = queueTileUpdates(backgroundBlock.position);
          if (!result34)
            return std::unexpected(std::move(result34.error()));
        }
      }
    }
  }

  for (auto const& foregroundBlock : m_centralStructure.foregroundBlocks()) {
    if (!foregroundBlock.residual) {
      auto result35 = generateRegion(RectI::withSize(foregroundBlock.position, {1, 1}));
      if (!result35)
        return std::unexpected(std::move(result35.error()));
      if (auto tile = m_tileArray->modifyTile(foregroundBlock.position)) {
        if (tile->get().foreground == foregroundBlock.materialId) {
          tile->get().foreground = EmptyMaterialId;
          tile->get().foregroundMod = NoModId;
          tile->get().updateCollision(CollisionKind::None);
          auto result36 = dirtyCollision(RectI::withSize(foregroundBlock.position, {1, 1}));
          if (!result36)
            return std::unexpected(std::move(result36.error()));
          auto result37 = queueTileUpdates(foregroundBlock.position);
          if (!result37)
            return std::unexpected(std::move(result37.error()));
        }
      }
    }
  }
  return {};
}

std::expected<bool, StarException> WorldServer::spawnTargetValid(SpawnTarget const& spawnTarget) const {
  if (std::holds_alternative<SpawnTargetUniqueEntity>(spawnTarget)) {
    auto result38 = m_worldStorage->loadUniqueEntity(std::get<SpawnTargetUniqueEntity>(spawnTarget));
    if (!result38)
      return std::unexpected(std::move(result38.error()));
    return static_cast<bool>(m_entityMap->get<WarpTargetEntity>(std::move(*result38)));
  }
  return true;
}

std::expected<bool, StarException> WorldServer::addClient(ConnectionId clientId, SpawnTarget const& spawnTarget, bool isLocal, bool isAdmin, NetCompatibilityRules netRules, ClientSubWorldId subWorldId) {
  // if on a subworld, assumes connection id is the subworld's connectionid
  if (m_clientInfo.contains(clientId))
    return false;

  Vec2F playerStart;
  if (std::holds_alternative<SpawnTargetPosition>(spawnTarget)) {
    playerStart = std::get<SpawnTargetPosition>(spawnTarget);
  } else if (std::holds_alternative<SpawnTargetX>(spawnTarget)) {
    auto targetX = std::get<SpawnTargetX>(spawnTarget);
    auto result39 = findPlayerSpaceStart(targetX);
    if (!result39)
      return std::unexpected(std::move(result39.error()));
    playerStart = std::move(*result39);
  } else if (std::holds_alternative<SpawnTargetUniqueEntity>(spawnTarget)) {
    auto result40 = m_worldStorage->loadUniqueEntity(std::get<SpawnTargetUniqueEntity>(spawnTarget));
    if (!result40)
      return std::unexpected(std::move(result40.error()));
    if (auto target = m_entityMap->get<WarpTargetEntity>(std::move(*result40))) {
      auto result41 = target->footPosition();
      if (!result41)
        return std::unexpected(std::move(result41.error()));
      playerStart = target->position() + std::move(*result41);
    } else {
      return false;
    }
  } else {
    playerStart = m_playerStart;
    if (m_adjustPlayerStart) {
      auto result42 = findPlayerStart(m_playerStart);
      if (!result42)
        return std::unexpected(std::move(result42.error()));
      m_playerStart = std::move(*result42);
      playerStart = m_playerStart;
    }
  }
  auto result43 = m_serverConfig.getInt("playerStartInitialGenRadius");
  if (!result43)
    return std::unexpected(std::move(result43.error()));
  RectF spawnRegion = RectF(playerStart, playerStart).padded(std::move(*result43));
  auto result44 = generateRegion(RectI::integral(spawnRegion));
  if (!result44)
    return std::unexpected(std::move(result44.error()));
  if (auto result = m_spawner->activateEmptyRegion(spawnRegion); !result)
    return std::unexpected(std::move(result.error()));

  InterpolationTracker tracker;
  if (isLocal) {
    auto result45 = m_serverConfig.query("interpolationSettings.local");
    if (!result45)
      return std::unexpected(std::move(result45.error()));
    auto result46 = InterpolationTracker::create(this->root(), std::move(*result45));
    if (!result46)
      return std::unexpected(std::move(result46.error()));
    tracker = std::move(*result46);
  } else {
    auto result47 = m_serverConfig.query("interpolationSettings.normal");
    if (!result47)
      return std::unexpected(std::move(result47.error()));
    auto result48 = InterpolationTracker::create(this->root(), std::move(*result47));
    if (!result48)
      return std::unexpected(std::move(result48.error()));
    tracker = std::move(*result48);
  }

  tracker.update(m_currentTime);

  auto result49 = ClientInfo::create(this->root(), clientId, tracker);
  if (!result49)
    return std::unexpected(std::move(result49.error()));
  auto result50 = m_clientInfo.add(clientId, std::move(*result49));
  if (!result50)
    return std::unexpected(std::move(result50.error()));
  auto& clientInfo = result50->get();
  clientInfo->subWorldId = subWorldId;
  clientInfo->local = isLocal;
  clientInfo->admin = isAdmin;
  clientInfo->clientState->setNetCompatibilityRules(netRules);

  auto result51 = m_worldTemplate->weatherLayerAt(Vec2I::floor(playerStart));
  if (!result51)
    return std::unexpected(std::move(result51.error()));
  if (auto layer = std::move(*result51))
    clientInfo->weatherDomain = layer->get().domain;

  auto worldStartPacket = std::make_shared<WorldStartPacket>();
  auto result52 = m_worldTemplate->store();
  if (!result52)
    return std::unexpected(std::move(result52.error()));
  auto& templateData = worldStartPacket->templateData = std::move(*result52);
  // this makes it possible to use custom InstanceWorlds without clients having the mod that adds their dungeon:
  auto result53 = templateData.optQueryString("worldParameters.primaryDungeon");
  if (!result53)
    return std::unexpected(std::move(result53.error()));
  bool condition57 = static_cast<bool>(std::move(*result53));
  if (condition57) {
    auto result54 = this->root().configuration();
    if (!result54)
      return std::unexpected(std::move(result54.error()));
    auto result55 = std::move(*result54)->getPath("compatibility.customDungeonWorld");
    if (!result55)
      return std::unexpected(std::move(result55.error()));
    auto result56 = std::move(*result55).optBool();
    if (!result56)
      return std::unexpected(std::move(result56.error()));
    condition57 = static_cast<bool>(std::move(*result56).value_or(false));
  }
  if (condition57) {
    auto result58 = worldStartPacket->templateData.setPath("worldParameters.primaryDungeon", "testarena");
    if (!result58)
      return std::unexpected(std::move(result58.error()));
    worldStartPacket->templateData = std::move(*result58);
  }

  auto result59 = m_sky->writeUpdate(0, netRules);
  if (!result59)
    return std::unexpected(std::move(result59.error()));
  std::tie(worldStartPacket->skyData, clientInfo->skyNetVersion) = std::move(*result59);
  auto result60 = weatherForDomain(clientInfo->weatherDomain)->writeUpdate(0, netRules);
  if (!result60)
    return std::unexpected(std::move(result60.error()));
  std::tie(worldStartPacket->weatherData, clientInfo->weatherNetVersion) = std::move(*result60);
  worldStartPacket->playerStart = playerStart;
  worldStartPacket->playerRespawn = m_playerStart;
  worldStartPacket->respawnInWorld = m_respawnInWorld;
  worldStartPacket->worldProperties = m_worldProperties;
  worldStartPacket->dungeonIdGravity = m_dungeonIdGravity;
  worldStartPacket->dungeonIdBreathable = m_dungeonIdBreathable;
  worldStartPacket->protectedDungeonIds = m_protectedDungeonIds;
  worldStartPacket->clientId = clientId;
  worldStartPacket->localInterpolationMode = isLocal;
  clientInfo->outgoingPackets.push_back(worldStartPacket);

  auto result61 = m_centralStructure.store();
  if (!result61)
    return std::unexpected(std::move(result61.error()));
  clientInfo->outgoingPackets.push_back(std::make_shared<CentralStructureUpdatePacket>(std::move(*result61)));

  for (auto&& p : m_scriptContexts) {
    auto result62 = recoverLuaScriptError(p.second->invoke("addClient", clientId, isLocal));
    if (!result62)
      return std::unexpected(std::move(result62.error()));
  }

  return true;
}

std::expected<List<std::shared_ptr<Packet>>, StarException> WorldServer::removeClient(ConnectionId clientId) {
  auto result63 = m_clientInfo.get(clientId);
  if (!result63)
    return std::unexpected(std::move(result63.error()));
  auto const& info = result63->get();

  for (auto&& p : m_scriptContexts) {
    auto result64 = recoverLuaScriptError(p.second->invoke("removeClient", clientId));
    if (!result64)
      return std::unexpected(std::move(result64.error()));
  }

  for (auto const& entityId : m_entityMap->entityIds()) {
    if (connectionForEntity(entityId) == clientId) {
      auto result65 = removeEntity(entityId, false);
      if (!result65)
        return std::unexpected(std::move(result65.error()));
    }
  }

  for (auto const& uuid : m_entityMessageResponses.keys()) {
    auto& entry = m_entityMessageResponses[uuid];
    if (entry.first == clientId) {
      auto response = entry.second;
      if (std::holds_alternative<ConnectionId>(response)) {
        if (auto clientInfo = m_clientInfo.value(std::get<ConnectionId>(response)))
          clientInfo->outgoingPackets.push_back(
            std::make_shared<EntityMessageResponsePacket>(std::unexpected(String("Client disconnected")), uuid));
      } else {
        if (auto failed = std::get<RpcPromiseKeeper<Json>>(response).fail("Client disconnected"); !failed)
          return std::unexpected(std::move(failed.error()));
      }
      m_entityMessageResponses.remove(uuid);
    } else if (std::holds_alternative<ConnectionId>(entry.second) && std::get<ConnectionId>(entry.second) == clientId) {
      m_entityMessageResponses.remove(uuid);
    }
  }

  auto packets = std::move(info->outgoingPackets);
  auto subWorldId = info->subWorldId;
  m_clientInfo.remove(clientId);

  packets.push_back(std::make_shared<WorldStopPacket>("Removed"));
  if (subWorldId == MainClientWorldId) {
    return packets;
  } else {
    return List<std::shared_ptr<Packet>>{std::make_shared<ClientSubWorldPackets>(subWorldId,std::move(packets))};
  }
}

List<ConnectionId> WorldServer::clientIds() const {
  return m_clientInfo.keys();
}

bool WorldServer::hasClient(ConnectionId clientId) const {
  return m_clientInfo.contains(clientId);
}

RectF WorldServer::clientWindow(ConnectionId clientId) const {
  auto i = m_clientInfo.find(clientId);
  if (i != m_clientInfo.end())
    return RectF(i->second->clientState->window());
  else
    return RectF::null();
}

std::shared_ptr<Player> WorldServer::clientPlayer(ConnectionId clientId) const {
  auto i = m_clientInfo.find(clientId);
  if (i != m_clientInfo.end())
    return get<Player>(i->second->clientState->playerId());
  else
    return {};
}

ClientSubWorldId WorldServer::clientSubWorld(ConnectionId clientId) const {
  auto i = m_clientInfo.find(clientId);
  if (i != m_clientInfo.end())
    return i->second->subWorldId;
  else
    return 0;
}

List<EntityId> WorldServer::players() const {
  List<EntityId> playerIds;
  for (auto const& pair : m_clientInfo)
    playerIds.push_back(pair.second->clientState->playerId());
  return playerIds;
}


std::expected<void, StarException> WorldServer::handleIncomingPackets(ConnectionId clientId, List<std::shared_ptr<Packet>> const& packets) {
  auto result66 = m_clientInfo.get(clientId);
  if (!result66)
    return std::unexpected(std::move(result66.error()));
  std::shared_ptr<ClientInfo> clientInfo = result66->get();
  auto& root = this->root();
  auto result67 = root.entityFactory();
  if (!result67)
    return std::unexpected(std::move(result67.error()));
  auto entityFactory = std::move(*result67);
  auto result68 = root.itemDatabase();
  if (!result68)
    return std::unexpected(std::move(result68.error()));
  auto itemDatabase = std::move(*result68);

  for (auto const& packet : packets) {
    if (auto worldStartAcknowledge = as<WorldStartAcknowledgePacket>(packet)) {
      clientInfo->started = true;
    } else if (!clientInfo->started) {
      // clients which have not sent a world start acknowledge are not sending packets intended for this world
      continue;
    } else if (auto heartbeat = as<StepUpdatePacket>(packet)) {
      clientInfo->interpolationTracker.receiveTimeUpdate(heartbeat->remoteTime);
    } else if (auto wcsPacket = as<WorldClientStateUpdatePacket>(packet)) {
      auto result69 = clientInfo->clientState->readDelta(wcsPacket->worldClientStateDelta);
      if (!result69)
        return std::unexpected(std::move(result69.error()));

      // Need to send all sectors that are now in the client window but were not
      // in the old
      HashSet<ServerTileSectorArray::Sector> oldSectors = std::exchange(clientInfo->activeSectors, {});

      auto result70 = clientInfo->monitoringRegions(m_entityMap);
      if (!result70)
        return std::unexpected(std::move(result70.error()));
      for (auto const& monitoredRegion : std::move(*result70)) {
        auto result71 = m_tileArray->validSectorsFor(monitoredRegion);
        if (!result71)
          return std::unexpected(std::move(result71.error()));
        clientInfo->activeSectors.insert_range(std::move(*result71));
      }

      clientInfo->pendingSectors.insert_range(clientInfo->activeSectors.difference(oldSectors));
    } else if (auto mtpacket = as<ModifyTileListPacket>(packet)) {
      auto result72 = this->root().liquidsDatabase();
      if (!result72)
        return std::unexpected(std::move(result72.error()));
      auto liquidsDb = std::move(*result72);
      auto result73 = this->root().materialDatabase();
      if (!result73)
        return std::unexpected(std::move(result73.error()));
      auto materialDb = std::move(*result73);
      TileModificationList sanitized;
      TileModificationList rejected;
      sanitized.reserve(mtpacket->modifications.size());
      for (auto const& pair : mtpacket->modifications) {
        bool ok = true;
        if (std::holds_alternative<PlaceLiquid>(pair.second))
          ok = liquidsDb->isValidLiquidId(std::get<PlaceLiquid>(pair.second).liquid); else if (std::holds_alternative<PlaceMaterial>(pair.second))
          ok = materialDb->isValidMaterialId(std::get<PlaceMaterial>(pair.second).material); else if (std::holds_alternative<PlaceMod>(pair.second))
          ok = materialDb->isValidModId(std::get<PlaceMod>(pair.second).mod);
        if (ok)
          sanitized.push_back(pair);
        else {
          rejected.push_back(pair);
          auto result74 = this->root().logger().warn("WorldServer: dropped tile mod with invalid id from client {}", clientId);
          if (!result74)
            return std::unexpected(std::move(result74.error()));
        }
      }
      auto result75 = applyTileModifications(sanitized, mtpacket->allowEntityOverlap);
      if (!result75)
        return std::unexpected(std::move(result75.error()));
      auto unappliedModifications = std::move(*result75);
      unappliedModifications.append_range(rejected);
      if (!unappliedModifications.empty())
        clientInfo->outgoingPackets.push_back(std::make_shared<TileModificationFailurePacket>(unappliedModifications));
    } else if (auto rtpacket = as<ReplaceTileListPacket>(packet)) {
      auto result76 = this->root().materialDatabase();
      if (!result76)
        return std::unexpected(std::move(result76.error()));
      auto materialDb = std::move(*result76);
      TileModificationList sanitized;
      TileModificationList rejected;
      for (auto const& pair : rtpacket->modifications) {
        bool ok = true;
        if (std::holds_alternative<PlaceMaterial>(pair.second))
          ok = materialDb->isValidMaterialId(std::get<PlaceMaterial>(pair.second).material);
        if (ok)
          sanitized.push_back(pair);
        else {
          rejected.push_back(pair);
        }
      }
      auto result77 = replaceTiles(sanitized, rtpacket->tileDamage, rtpacket->applyDamage);
      if (!result77)
        return std::unexpected(std::move(result77.error()));
      auto unappliedModifications = std::move(*result77);
      unappliedModifications.append_range(rejected);
      if (!unappliedModifications.empty())
        clientInfo->outgoingPackets.push_back(std::make_shared<TileModificationFailurePacket>(unappliedModifications));
    } else if (auto dtgpacket = as<DamageTileGroupPacket>(packet)) {
      auto result78 = damageTiles(dtgpacket->tilePositions, dtgpacket->layer, dtgpacket->sourcePosition, dtgpacket->tileDamage, dtgpacket->sourceEntity);
      if (!result78)
        return std::unexpected(std::move(result78.error()));
    } else if (auto clpacket = as<CollectLiquidPacket>(packet)) {
      auto result79 = this->root().liquidsDatabase();
      if (!result79)
        return std::unexpected(std::move(result79.error()));
      if (!std::move(*result79)->isValidLiquidId(clpacket->liquidId)) {
        auto result80 = this->root().logger().warn("WorldServer: dropped CollectLiquid with invalid liquid id {} from client {}", clpacket->liquidId, clientId);
        if (!result80)
          return std::unexpected(std::move(result80.error()));
        continue;
      }
      auto result81 = collectLiquid(clpacket->tilePositions, clpacket->liquidId);
      if (!result81)
        return std::unexpected(std::move(result81.error()));
      if (auto item = std::move(*result81))
        clientInfo->outgoingPackets.push_back(std::make_shared<GiveItemPacket>(item));
    } else if (auto sepacket = as<SpawnEntityPacket>(packet)) {
      auto acceptEntity = [&, &world = *this]() -> std::expected<void, StarException> {
      auto netRules = clientInfo->clientState->netCompatibilityRules();
      auto result82 = entityFactory->netLoadEntity(sepacket->entityType, std::move(sepacket->storeData), netRules);
      if (!result82)
      return std::unexpected(std::move(result82.error()));
      auto entity = std::move(*result82);
      auto result83 = entity->readNetState(std::move(sepacket->firstNetState), 0.0f, netRules);
      if (!result83)
      return std::unexpected(std::move(result83.error()));
      auto result84 = world.addEntity(std::move(entity));
      if (!result84)
      return std::unexpected(std::move(result84.error()));
      return {};
      };
      if (auto accepted = acceptEntity(); !accepted) {
        auto logged = this->root().logger().warnException(Star::format("WorldServer: rejected SpawnEntity from client {}", clientId), ExceptionDiagnostic(accepted.error()));
        if (!logged)
          return std::unexpected(std::move(logged.error()));
      }
    } else if (auto rdpacket = as<RequestDropPacket>(packet)) {
      auto drop = m_entityMap->get<ItemDrop>(rdpacket->dropEntityId);
      if (drop && drop->isMaster()) {
        auto canTake = drop->canTake();
        if (!canTake)
          return std::unexpected(std::move(canTake.error()));
        if (!*canTake)
          continue;
        auto result85 = drop->takeBy(clientInfo->clientState->playerId());
        if (!result85)
          return std::unexpected(std::move(result85.error()));
        if (auto taken = std::move(*result85))
          clientInfo->outgoingPackets.push_back(std::make_shared<GiveItemPacket>(taken->descriptor()));
      }
    } else if (auto hit = as<HitRequestPacket>(packet)) {
      if (hit->remoteHitRequest.destinationConnection() == ServerConnectionId) {
        auto result86 = m_damageManager->pushRemoteHitRequest(hit->remoteHitRequest);
        if (!result86)
          return std::unexpected(std::move(result86.error()));
      } else {
        auto destination = m_clientInfo.get(hit->remoteHitRequest.destinationConnection());
        if (!destination)
          return std::unexpected(std::move(destination.error()));
        destination->get()->outgoingPackets.push_back(std::make_shared<HitRequestPacket>(hit->remoteHitRequest));
      }
    } else if (auto damage = as<DamageRequestPacket>(packet)) {
      if (damage->remoteDamageRequest.destinationConnection() == ServerConnectionId) {
        auto result87 = m_damageManager->pushRemoteDamageRequest(damage->remoteDamageRequest);
        if (!result87)
          return std::unexpected(std::move(result87.error()));
      } else {
        auto destination = m_clientInfo.get(damage->remoteDamageRequest.destinationConnection());
        if (!destination)
          return std::unexpected(std::move(destination.error()));
        destination->get()->outgoingPackets.push_back(
          std::make_shared<DamageRequestPacket>(damage->remoteDamageRequest));
      }
    } else if (auto damage = as<DamageNotificationPacket>(packet)) {
      auto result88 = m_damageManager->pushRemoteDamageNotification(damage->remoteDamageNotification);
      if (!result88)
        return std::unexpected(std::move(result88.error()));
      for (auto const& pair : m_clientInfo) {
        if (pair.first != clientId && pair.second->needsDamageNotification(damage->remoteDamageNotification))
          pair.second->outgoingPackets.push_back(
            std::make_shared<DamageNotificationPacket>(damage->remoteDamageNotification));
      }
    } else if (auto entityInteract = as<EntityInteractPacket>(packet)) {
      auto targetEntityConnection = connectionForEntity(entityInteract->interactRequest.targetId);
      if (targetEntityConnection == ServerConnectionId) {
        auto result89 = interact(entityInteract->interactRequest);
        if (!result89)
          return std::unexpected(std::move(result89.error()));
        auto result90 = std::move(*result89).result();
        if (!result90)
          return std::unexpected(std::move(result90.error()));
        auto interactResult = std::move(*result90);
        auto result91 = takeOptional(interactResult);
        if (!result91)
          return std::unexpected(std::move(result91.error()));
        clientInfo->outgoingPackets.push_back(std::make_shared<EntityInteractResultPacket>(
          std::move(*result91), entityInteract->requestId, entityInteract->interactRequest.sourceId));
      } else {
        auto result92 = m_clientInfo.get(targetEntityConnection);
        if (!result92)
          return std::unexpected(std::move(result92.error()));
        auto const& forwardClientInfo = result92->get();
        forwardClientInfo->outgoingPackets.push_back(entityInteract);
      }
    } else if (auto interactResult = as<EntityInteractResultPacket>(packet)) {
      auto result93 = m_clientInfo.get(connectionForEntity(interactResult->sourceEntityId));
      if (!result93)
        return std::unexpected(std::move(result93.error()));
      auto const& forwardClientInfo = result93->get();
      forwardClientInfo->outgoingPackets.push_back(interactResult);
    } else if (auto entityCreate = as<EntityCreatePacket>(packet)) {
      auto legalEntityId = entityIdInSpace(entityCreate->entityId, clientInfo->clientId);
      if (!legalEntityId)
        return std::unexpected(std::move(legalEntityId.error()));
      if (!*legalEntityId) {
        return std::unexpected(WorldServerException::format("WorldServer received entity create packet with illegal entity id {}.", entityCreate->entityId));
      } else {
        auto acceptEntity = [&, &world = *this]() -> std::expected<void, StarException> {
          if (world.m_entityMap->entity(entityCreate->entityId)) {
            auto logged = world.root().logger().error("WorldServer received duplicate entity create packet from client, deleting old entity {}", entityCreate->entityId);
            if (!logged)
              return std::unexpected(std::move(logged.error()));
            if (auto result = world.removeEntity(entityCreate->entityId, false); !result)
              return std::unexpected(std::move(result.error()));
          }
          auto netRules = clientInfo->clientState->netCompatibilityRules();
          auto loaded = entityFactory->netLoadEntity(entityCreate->entityType, entityCreate->storeData, netRules);
          if (!loaded)
            return std::unexpected(std::move(loaded.error()));
          auto entity = std::move(*loaded);
          if (auto result = entity->readNetState(entityCreate->firstNetState, 0.0f, netRules); !result)
            return std::unexpected(std::move(result.error()));

          bool published = false;
          auto reject = [&](StarException error) -> std::expected<void, StarException> {
            if (published)
              world.m_entityMap->removeEntity(entity->entityId());
            if (auto result = entity->uninit(); !result)
              error = error.with_context(Star::format("Error uninitializing rejected slave entity: {}", result.error().what()));
            return std::unexpected(std::move(error));
          };
          if (auto result = entity->init(world, entityCreate->entityId, EntityMode::Slave); !result)
            return reject(std::move(result.error()));
          if (auto result = world.m_entityMap->addEntity(entity); !result)
            return reject(std::move(result.error()));
          published = true;
          if (clientInfo->interpolationTracker.interpolationEnabled()) {
            if (auto result = entity->enableInterpolation(clientInfo->interpolationTracker.extrapolationHint()); !result)
              return reject(std::move(result.error()));
          }
          return {};
        };
        if (auto accepted = acceptEntity(); !accepted) {
          auto logged = this->root().logger().warnException(Star::format("WorldServer: rejected EntityCreate from client {}", clientId), ExceptionDiagnostic(accepted.error()));
          if (!logged)
            return std::unexpected(std::move(logged.error()));
        }
      }
    } else if (auto entityUpdateSet = as<EntityUpdateSetPacket>(packet)) {
      float interpolationLeadTime = clientInfo->interpolationTracker.interpolationLeadTime();
      auto result102 = m_entityMap->forAllEntities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
        EntityId entityId = entity->entityId();
        if (connectionForEntity(entityId) == clientId) {
          if constexpr (Star::DebugEnabled) {
            Star::star_assert_failed((entity->isSlave()));
          }
          auto result101 = entity->readNetState(entityUpdateSet->deltas.value(entityId), interpolationLeadTime, clientInfo->clientState->netCompatibilityRules());
          if (!result101)
            return std::unexpected(std::move(result101.error()));
        }
        return {};
      });
      if (!result102)
        return std::unexpected(std::move(result102.error()));
      clientInfo->pendingForward = true;
    } else if (auto entityDestroy = as<EntityDestroyPacket>(packet)) {
      if (auto entity = m_entityMap->entity(entityDestroy->entityId)) {
        auto result103 = entity->readNetState(entityDestroy->finalNetState, clientInfo->interpolationTracker.interpolationLeadTime(), clientInfo->clientState->netCompatibilityRules());
        if (!result103)
          return std::unexpected(std::move(result103.error()));
        // Before destroying the entity, we should make sure that the entity is
        // using the absolute latest data, so we disable interpolation.
        auto result104 = entity->disableInterpolation();
        if (!result104)
          return std::unexpected(std::move(result104.error()));
        auto result105 = removeEntity(entityDestroy->entityId, entityDestroy->death);
        if (!result105)
          return std::unexpected(std::move(result105.error()));
      }
    } else if (auto disconnectWires = as<DisconnectAllWiresPacket>(packet)) {
      auto result106 = atTile<WireEntity>(disconnectWires->entityPosition);
      if (!result106)
        return std::unexpected(std::move(result106.error()));
      for (auto const& wireEntity : std::move(*result106)) {
        auto result107 = wireEntity->connectionsForNode(disconnectWires->wireNode);
        if (!result107)
          return std::unexpected(std::move(result107.error()));
        for (auto connection : std::move(*result107)) {
          auto result108 = wireEntity->removeNodeConnection(disconnectWires->wireNode, connection);
          if (!result108)
            return std::unexpected(std::move(result108.error()));
          auto result109 = atTile<WireEntity>(connection.entityLocation);
          if (!result109)
            return std::unexpected(std::move(result109.error()));
          for (auto const& connectedEntity : std::move(*result109)) {
            auto result110 = connectedEntity->removeNodeConnection(
              {.direction = otherWireDirection(disconnectWires->wireNode.direction), .nodeIndex = connection.nodeIndex},
              WireConnection{.entityLocation = disconnectWires->entityPosition,
                             .nodeIndex = disconnectWires->wireNode.nodeIndex});
            if (!result110)
              return std::unexpected(std::move(result110.error()));
          }
        }
      }
    } else if (auto connectWire = as<ConnectWirePacket>(packet)) {
      auto result111 = atTile<WireEntity>(connectWire->inputConnection.entityLocation);
      if (!result111)
        return std::unexpected(std::move(result111.error()));
      for (auto const& source : std::move(*result111)) {
        auto result112 = atTile<WireEntity>(connectWire->outputConnection.entityLocation);
        if (!result112)
          return std::unexpected(std::move(result112.error()));
        for (auto const& target : std::move(*result112)) {
          auto result113 = source->addNodeConnection(
            WireNode{.direction = WireDirection::Input, .nodeIndex = connectWire->inputConnection.nodeIndex},
            connectWire->outputConnection);
          if (!result113)
            return std::unexpected(std::move(result113.error()));
          auto result114 = target->addNodeConnection(
            WireNode{.direction = WireDirection::Output, .nodeIndex = connectWire->outputConnection.nodeIndex},
            connectWire->inputConnection);
          if (!result114)
            return std::unexpected(std::move(result114.error()));
        }
      }
    } else if (auto findUniqueEntity = as<FindUniqueEntityPacket>(packet)) {
      auto result115 = m_worldStorage->findUniqueEntity(findUniqueEntity->uniqueEntityId);
      if (!result115)
        return std::unexpected(std::move(result115.error()));
      clientInfo->outgoingPackets.push_back(
        std::make_shared<FindUniqueEntityResponsePacket>(findUniqueEntity->uniqueEntityId, std::move(*result115)));
    } else if (auto entityMessagePacket = as<EntityMessagePacket>(packet)) {
      auto rejectMessage = [&, &world = *this](String const& reason) -> std::expected<void, StarException> {
      auto result116 = world.root().logger().warn("WorldServer: rejecting EntityMessage '{}' from client {}: {}",
                       entityMessagePacket->message, clientId, reason);
      if (!result116)
      return std::unexpected(std::move(result116.error()));
      clientInfo->outgoingPackets.push_back(
        std::make_shared<EntityMessageResponsePacket>(std::unexpected(reason), entityMessagePacket->uuid));
      return {};
      };

      auto assetPathExists = [&world = *this](String const& path) -> std::expected<bool, StarException> {
        if (path.empty())
          return false;
        auto assets = world.root().assets();
        if (!assets)
          return std::unexpected(std::move(assets.error()));
        String checkPath = AssetPath::split(path).basePath;
        if (checkPath.empty())
          return false;
        return (*assets)->assetExists(checkPath);
      };

      bool messageRejected = false;

      if (entityMessagePacket->message == "playCinematic" && !entityMessagePacket->args.empty()) {
        Json const& cinematicArg = entityMessagePacket->args[0];
        bool valid = false;
        if (cinematicArg.isType(Json::Type::Object)) {
          // Inline cinematic definition - accept as-is.
          valid = true;
        } else if (cinematicArg.isType(Json::Type::String)) {
          auto result118 = cinematicArg.toString();
          if (!result118)
            return std::unexpected(std::move(result118.error()));
          auto exists = assetPathExists(*result118);
          if (!exists) {
            auto rejected = rejectMessage(Star::format("invalid cinematic asset path '{}': {}", *result118, exists.error().what()));
            if (!rejected)
              return std::unexpected(std::move(rejected.error()));
            continue;
          }
          valid = *exists;
          if (!valid) {
            auto result119 = cinematicArg.toString();
            if (!result119)
              return std::unexpected(std::move(result119.error()));
            auto result120 = rejectMessage(Star::format("invalid cinematic asset path '{}'", std::move(*result119)));
            if (!result120)
              return std::unexpected(std::move(result120.error()));
          }
        } else if (!cinematicArg.isNull()) {
          auto result121 = rejectMessage(
            Star::format("non-string/non-object cinematic argument of type {}", static_cast<int>(cinematicArg.type())));
          if (!result121)
            return std::unexpected(std::move(result121.error()));
        }
        if (!valid)
          messageRejected = true;
      } else if (entityMessagePacket->message == "playAltMusic" && !entityMessagePacket->args.empty()) {
        Json const& tracksArg = entityMessagePacket->args[0];
        bool valid = true;
        if (tracksArg.canConvert(Json::Type::Array)) {
          auto result123 = tracksArg.toArray();
          if (!result123)
            return std::unexpected(std::move(result123.error()));
          for (auto const& trackJson : std::move(*result123)) {
            bool condition125 = static_cast<bool>(!trackJson.isType(Json::Type::String));
            if (!condition125) {
              auto result124 = trackJson.toString();
              if (!result124)
                return std::unexpected(std::move(result124.error()));
              auto exists = assetPathExists(*result124);
              if (!exists) {
                auto rejected = rejectMessage(Star::format("invalid music asset path '{}': {}", *result124, exists.error().what()));
                if (!rejected)
                  return std::unexpected(std::move(rejected.error()));
                valid = false;
                break;
              }
              condition125 = !*exists;
            }
            if (condition125) {
              valid = false;
              auto result126 = trackJson.toString();
              if (!result126)
                return std::unexpected(std::move(result126.error()));
              auto result127 = rejectMessage(Star::format("invalid music asset path '{}'", std::move(*result126)));
              if (!result127)
                return std::unexpected(std::move(result127.error()));
              break;
            }
          }
        } else if (tracksArg.isType(Json::Type::String)) {
          auto result128 = tracksArg.toString();
          if (!result128)
            return std::unexpected(std::move(result128.error()));
          auto exists = assetPathExists(*result128);
          if (!exists) {
            auto rejected = rejectMessage(Star::format("invalid music asset path '{}': {}", *result128, exists.error().what()));
            if (!rejected)
              return std::unexpected(std::move(rejected.error()));
            continue;
          }
          if (!*exists) {
            valid = false;
            auto result129 = tracksArg.toString();
            if (!result129)
              return std::unexpected(std::move(result129.error()));
            auto result130 = rejectMessage(Star::format("invalid music asset path '{}'", std::move(*result129)));
            if (!result130)
              return std::unexpected(std::move(result130.error()));
          }
        } else {
          valid = false;
          auto result131 =
            rejectMessage(Star::format("invalid playAltMusic argument of type {}", static_cast<int>(tracksArg.type())));
          if (!result131)
            return std::unexpected(std::move(result131.error()));
        }
        if (!valid)
          messageRejected = true;
      } else if (entityMessagePacket->message == "warp" && !entityMessagePacket->args.empty()) {
        Json const& warpArg = entityMessagePacket->args[0];
        bool valid = false;
        if (warpArg.isType(Json::Type::String)) {
          auto warpString = warpArg.toString();
          if (!warpString)
            return std::unexpected(std::move(warpString.error()));
          auto warpAction = parseWarpAction(*warpString);
          if (warpAction)
            valid = true; else {
            auto result133 = rejectMessage(Star::format("invalid warp action '{}': {}", *warpString, warpAction.error().what()));
            if (!result133)
              return std::unexpected(std::move(result133.error()));
          }
        } else {
          auto result134 = rejectMessage(Star::format("non-string warp argument of type {}", static_cast<int>(warpArg.type())));
          if (!result134)
            return std::unexpected(std::move(result134.error()));
        }
        if (!valid)
          messageRejected = true;
      } else if (entityMessagePacket->message == "queueRadioMessage" && !entityMessagePacket->args.empty()) {
        Json const& configArg = entityMessagePacket->args[0];
        bool valid = false;
        auto radioDatabase = this->root().radioMessageDatabase();
        if (!radioDatabase) {
          auto rejected = rejectMessage(Star::format("invalid radio message: {}", radioDatabase.error().what()));
          if (!rejected)
            return std::unexpected(std::move(rejected.error()));
          continue;
        }
        auto radioMessage = (*radioDatabase)->createRadioMessage(configArg);
        if (radioMessage)
          valid = true; else {
          auto result136 = rejectMessage(Star::format("invalid radio message: {}", radioMessage.error().what()));
          if (!result136)
            return std::unexpected(std::move(result136.error()));
        }
        if (!valid)
          messageRejected = true;
      }

      if (messageRejected)
        continue;

      std::shared_ptr<Entity> entity;
      if (std::holds_alternative<EntityId>(entityMessagePacket->entityId))
        entity = m_entityMap->entity(std::get<EntityId>(entityMessagePacket->entityId)); else {
        auto result137 = loadUniqueEntity(std::get<String>(entityMessagePacket->entityId));
        if (!result137)
          return std::unexpected(std::move(result137.error()));
        entity = m_entityMap->entity(std::move(*result137));
      }

      if (!entity) {
        clientInfo->outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(
          std::unexpected(String("Unknown entity")), entityMessagePacket->uuid));
      } else {
        if (entity->isMaster()) {
          auto result138 = entity->receiveMessage(clientId, entityMessagePacket->message, entityMessagePacket->args);
          if (!result138)
            return std::unexpected(std::move(result138.error()));
          if (auto response = std::move(*result138)) {
            if (std::holds_alternative<Json>(*response))
              clientInfo->outgoingPackets.push_back(
                std::make_shared<EntityMessageResponsePacket>(std::get<Json>(*response), entityMessagePacket->uuid));
            else {
              // delay the response until this promise is done
              m_entityMessagePromises[entityMessagePacket->uuid] = std::make_pair(clientId, std::get<RpcPromise<Json>>(*response));
            }
          } else {
            clientInfo->outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(
              std::unexpected(String("Message not handled by entity")), entityMessagePacket->uuid));
          }
        } else if (auto const& clientInfo = m_clientInfo.value(connectionForEntity(entity->entityId()))) {
          m_entityMessageResponses[entityMessagePacket->uuid] = {clientInfo->clientId, clientId};
          entityMessagePacket->fromConnection = clientId;
          clientInfo->outgoingPackets.push_back(std::move(entityMessagePacket));
        }
      }
    } else if (auto entityMessageResponsePacket = as<EntityMessageResponsePacket>(packet)) {
      if (!m_entityMessageResponses.contains(entityMessageResponsePacket->uuid)) {
        auto result139 = this->root().logger().warn("EntityMessageResponse received for unknown context [{}]!", entityMessageResponsePacket->uuid.hex());
        if (!result139)
          return std::unexpected(std::move(result139.error()));
      } else {
        auto result140 = m_entityMessageResponses.extract_value(entityMessageResponsePacket->uuid);
        if (!result140)
          return std::unexpected(std::move(result140.error()));
        auto response = std::move(*result140).second;
        if (std::holds_alternative<ConnectionId>(response)) {
          if (auto clientInfo = m_clientInfo.value(std::get<ConnectionId>(response)))
            clientInfo->outgoingPackets.push_back(std::move(entityMessageResponsePacket));
        } else {
          if (entityMessageResponsePacket->response.has_value()) {
            if (auto fulfilled =
                  std::get<RpcPromiseKeeper<Json>>(response).fulfill(*entityMessageResponsePacket->response);
                !fulfilled)
              return std::unexpected(std::move(fulfilled.error()));
          } else {
            if (auto failed =
                  std::get<RpcPromiseKeeper<Json>>(response).fail(entityMessageResponsePacket->response.error());
                !failed)
              return std::unexpected(std::move(failed.error()));
          }
        }
      }
    } else if (auto pingPacket = as<PingPacket>(packet)) {
      clientInfo->outgoingPackets.push_back(std::make_shared<PongPacket>(pingPacket->time));
    } else if (auto updateWorldProperties = as<UpdateWorldPropertiesPacket>(packet)) {
      // Kae: Properties set to null (nil from Lua) should be erased instead of lingering around
      for (auto&& pair : updateWorldProperties->updatedProperties) {
        if (pair.second.isNull())
          m_worldProperties.erase(pair.first); else {
          m_worldProperties[pair.first] = pair.second;
        }
      }
      for (auto const& pair : m_clientInfo)
        pair.second->outgoingPackets.push_back(
          std::make_shared<UpdateWorldPropertiesPacket>(updateWorldProperties->updatedProperties));
    } else if (auto updateWorldTemplate = as<UpdateWorldTemplatePacket>(packet)) {
      if (!clientInfo->admin)
        continue;
      // nuh-uh!

      auto result143 = WorldTemplate::create(this->root(), updateWorldTemplate->templateData);
      if (!result143)
        return std::unexpected(std::move(result143.error()));
      auto newWorldTemplate = std::move(*result143);
      auto result144 = setTemplate(newWorldTemplate);
      if (!result144)
        return std::unexpected(std::move(result144.error()));
      // setTemplate re-adds all clients currently, update clientInfo
      auto result145 = m_clientInfo.get(clientId);
      if (!result145)
        return std::unexpected(std::move(result145.error()));
      clientInfo = result145->get();
    } else {
      if (!PacketTypeNames)
        return std::unexpected(PacketTypeNames.error());
      auto result146 = (*PacketTypeNames).get_right(packet->type());
      if (!result146)
        return std::unexpected(std::move(result146.error()));
      auto result147 = this->root().logger().warn("UniverseServer: Dropping unexpected {} packet from client",
                result146->get());
      if (!result147)
        return std::unexpected(std::move(result147.error()));
    }
  }
  return {};
}

std::expected<List<std::shared_ptr<Packet>>, StarException> WorldServer::getOutgoingPackets(ConnectionId clientId) {
  auto result148 = m_clientInfo.get(clientId);
  if (!result148)
    return std::unexpected(std::move(result148.error()));
  auto const& clientInfo = result148->get();
  if (clientInfo->subWorldId == MainClientWorldId) {
    return std::move(clientInfo->outgoingPackets);
  } else {
    return List<std::shared_ptr<Packet>>{std::make_shared<ClientSubWorldPackets>(clientInfo->subWorldId,std::move(clientInfo->outgoingPackets))};
  }
}

bool WorldServer::sendPacket(ConnectionId clientId, std::shared_ptr<Packet> const& packet) {
  if (auto clientInfo = m_clientInfo.find_ref(clientId)) {
    clientInfo->get()->outgoingPackets.push_back(packet);
    return true;
  }
  return false;
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> WorldServer::receiveMessage(ConnectionId fromConnection, String const& message, JsonArray const& args) {
  std::optional<ChainableJsonMessageResponse> result;
  for (auto&& p : m_scriptContexts) {
    auto result149 = recoverLuaScriptError(p.second->handleMessage(message, fromConnection == ServerConnectionId, args));
    if (!result149)
      return std::unexpected(std::move(result149.error()));
    result = std::move(*result149);
    if (result)
      break;
  }
  return result;
}

WorldServerFidelity WorldServer::fidelity() const {
  return m_fidelity;
}

std::expected<void, StarException> WorldServer::setFidelity(WorldServerFidelity fidelity) {
  if (m_fidelity != fidelity) {
    m_updateTimings = {};
    m_processingLimits = {};
  }
  m_fidelity = fidelity;
  auto result150 = m_serverConfig.get("fidelitySettings");
  if (!result150)
    return std::unexpected(std::move(result150.error()));
  if (!WorldServerFidelityNames)
    return std::unexpected(WorldServerFidelityNames.error());
  auto result151 = (*WorldServerFidelityNames).get_right(m_fidelity);
  if (!result151)
    return std::unexpected(std::move(result151.error()));
  auto result152 = std::move(*result150).get(result151->get());
  if (!result152)
    return std::unexpected(std::move(result152.error()));
  std::optional<std::reference_wrapper<JsonObject const>> identity;
  if (result152->type() == Json::Type::Object) {
    auto object = result152->objectPtr();
    if (!object)
      return std::unexpected(std::move(object.error()));
    identity = std::cref(**object);
  }
  if (!identity || !m_fidelityConfigIdentity || std::addressof(identity->get()) != std::addressof(m_fidelityConfigIdentity->get())) {
    m_updateTimings = {};
    m_processingLimits = {};
  }
  m_fidelityConfigIdentity = identity;
  m_fidelityConfig = std::move(*result152);
  return {};
}

bool WorldServer::shouldExpire() {
  if (!m_clientInfo.empty()) {
    m_expiryTimer.reset();
    return false;
  }

  return m_expiryTimer.ready();
}

void WorldServer::setExpiryTime(float expiryTime) {
  m_expiryTimer = GameTimer(expiryTime);
}

float WorldServer::expiryTime() {
  return m_expiryTimer.timer;
}

List<EntityId> WorldServer::entityIds() const {
  return m_entityMap->entityIds();
}

std::expected<void, StarException> WorldServer::update(float dt) {
  m_currentTime += dt;
  ++m_currentStep;
  for (auto const& pair : m_clientInfo)
    pair.second->interpolationTracker.update(m_currentTime);

  List<WorldAction> triggeredActions;
  m_timers.filter([&triggeredActions, dt](std::pair<float, WorldAction>& timer) {
    if ((timer.first -= dt) <= 0) {
      triggeredActions.push_back(timer.second);
      return false;
    }
    return true;
  });
  for (auto const& action : triggeredActions) {
    if (!action)
      return std::unexpected(WorldServerException("Empty world timer action"));
    auto result153 = action(*this);
    if (!result153)
      return std::unexpected(std::move(result153.error()));
  }

  auto result154 = m_spawner->update(dt);
  if (!result154)
    return std::unexpected(std::move(result154.error()));

  bool doBreakChecks = m_tileEntityBreakCheckTimer.wrapTick(m_currentTime) && m_needsGlobalBreakCheck;
  if (doBreakChecks)
    m_needsGlobalBreakCheck = false;

  List<EntityId> toRemove;
  auto result162 = m_entityMap->updateAllEntities([&, &world = *this](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    auto result155 = entity->update(dt, world.m_currentStep);
    if (!result155)
      return std::unexpected(std::move(result155.error()));

    if (auto tileEntity = as<TileEntity>(entity)) {
      // Only do break checks on objects if all sectors the object touches
      // *and surrounding sectors* are active.  Objects that this object
      // rests on can be up to an entire sector large in any direction.
      bool condition157 = static_cast<bool>(doBreakChecks);
      if (condition157) {
        auto result156 = world.regionActive(RectI::integral(tileEntity->metaBoundBox().translated(tileEntity->position())).padded(WorldSectorSize));
        if (!result156)
          return std::unexpected(std::move(result156.error()));
        condition157 = static_cast<bool>(std::move(*result156));
      }
      if (condition157) {
        auto result158 = tileEntity->checkBroken();
        if (!result158)
          return std::unexpected(std::move(result158.error()));
      }
      auto result159 = world.updateTileEntityTiles(tileEntity);
      if (!result159)
        return std::unexpected(std::move(result159.error()));
    }

    auto result161 = entity->shouldDestroy();
    if (!result161)
      return std::unexpected(std::move(result161.error()));
    if (std::move(*result161) && entity->entityMode() == EntityMode::Master)
      toRemove.push_back(entity->entityId());
    return {};
  }, [](std::shared_ptr<Entity> const& a, std::shared_ptr<Entity> const& b) {
        return a->entityType() < b->entityType();
      });
  if (!result162)
    return std::unexpected(std::move(result162.error()));

  for (auto&& pair : m_scriptContexts) {
    auto result163 = recoverLuaScriptError(pair.second->update(pair.second->updateDt(dt)));
    if (!result163)
      return std::unexpected(std::move(result163.error()));
  }

  auto result164 = updateDamage(dt);
  if (!result164)
    return std::unexpected(std::move(result164.error()));
  auto result165 = shouldRunThisStep(UpdateTiming::Wiring);
  if (!result165)
    return std::unexpected(std::move(result165.error()));
  if (std::move(*result165)) {
    auto result166 = m_wireProcessor->process();
    if (!result166)
      return std::unexpected(std::move(result166.error()));
  }

  auto result167 = m_sky->update(dt);
  if (!result167)
    return std::unexpected(std::move(result167.error()));

  StringMap<List<RectI>> weatherClientWindows;
  List<RectI> clientMonitoringRegions;
  for (auto const& pair : m_clientInfo) {
    auto window = pair.second->clientState->window();
    if (!window.isEmpty()) {
      auto result168 = m_worldTemplate->weatherLayerAt(Vec2I::floor(pair.second->clientState->windowCenter()));
      if (!result168)
        return std::unexpected(std::move(result168.error()));
      if (auto layer = std::move(*result168)) {
        if (layer->get().domain) {
          auto clippedWindow = RectI(window.xMin(), std::max(window.yMin(), layer->get().minHeight),
              window.xMax(), std::min(window.yMax(), layer->get().maxHeight));
          if (!clippedWindow.isEmpty())
            weatherClientWindows[*layer->get().domain].push_back(clippedWindow);
        }
      }
    }
    auto result169 = pair.second->monitoringRegions(m_entityMap);
    if (!result169)
      return std::unexpected(std::move(result169.error()));
    for (auto const& region : std::move(*result169)) {
      auto result170 = m_geometry.splitRect(region);
      if (!result170)
        return std::unexpected(std::move(result170.error()));
      clientMonitoringRegions.append_range(std::move(*result170));
    }
  }

  for (auto const& pair : m_weatherDomains) {
    pair.second->setClientVisibleRegions(weatherClientWindows.value(pair.first, {}));
    auto result171 = pair.second->update(dt);
    if (!result171)
      return std::unexpected(std::move(result171.error()));
    for (auto projectile : pair.second->pullNewProjectiles()) {
      auto result172 = addEntity(std::move(projectile));
      if (!result172)
        return std::unexpected(std::move(result172.error()));
    }
  }

  auto result173 = shouldRunThisStep(UpdateTiming::Liquid);
  if (!result173)
    return std::unexpected(std::move(result173.error()));
  if (std::move(*result173)) {
    auto result174 = processingLimit(ProcessingLimit::Liquid);
    if (!result174)
      return std::unexpected(std::move(result174.error()));
    m_liquidEngine->setProcessingLimit(std::move(*result174));
    m_liquidEngine->setNoProcessingLimitRegions(clientMonitoringRegions);
    auto result175 = m_liquidEngine->update();
    if (!result175)
      return std::unexpected(std::move(result175.error()));
  }

  auto result176 = shouldRunThisStep(UpdateTiming::FallingBlocks);
  if (!result176)
    return std::unexpected(std::move(result176.error()));
  if (std::move(*result176)) {
    auto result177 = m_fallingBlocksAgent->update();
    if (!result177)
      return std::unexpected(std::move(result177.error()));
  }

  auto result178 = shouldRunThisStep(UpdateTiming::BlockDamage);
  if (!result178)
    return std::unexpected(std::move(result178.error()));
  if (auto delta = std::move(*result178)) {
    auto result179 = updateDamagedBlocks(*delta * dt);
    if (!result179)
      return std::unexpected(std::move(result179.error()));
  }

  auto result180 = shouldRunThisStep(UpdateTiming::StorageTick);
  if (!result180)
    return std::unexpected(std::move(result180.error()));
  if (auto delta = std::move(*result180)) {
    auto result181 = m_worldStorage->tick(*delta * this->root().timing().timestep, std::cref(m_worldId));
    if (!result181)
      return std::unexpected(std::move(result181.error()));
  }

  auto result182 = shouldRunThisStep(UpdateTiming::StorageGenerate);
  if (!result182)
    return std::unexpected(std::move(result182.error()));
  if (auto delta = std::move(*result182)) {
    auto result183 = processingLimit(ProcessingLimit::StorageGenerate);
    if (!result183)
      return std::unexpected(std::move(result183.error()));
    auto result184 = m_worldStorage->generateQueue(std::move(*result183), [&world = *this](WorldStorage::Sector a, WorldStorage::Sector b) {
            auto distanceToClosestPlayer = [&world](WorldStorage::Sector sector) {
              Vec2F sectorCenter = RectF(*world.m_worldStorage->regionForSector(sector)).center();
              float distance = std::numeric_limits<float>::max();
              for (auto const& pair : world.m_clientInfo) {
                if (auto player = world.get<Player>(pair.second->clientState->playerId()))
                  distance = std::min(vmag(sectorCenter - player->position()), distance);
              }
              return distance;
            };

            return distanceToClosestPlayer(a) < distanceToClosestPlayer(b);
          });
    if (!result184)
      return std::unexpected(std::move(result184.error()));
  }

  for (EntityId entityId : toRemove) {
    auto result185 = removeEntity(entityId, true);
    if (!result185)
      return std::unexpected(std::move(result185.error()));
  }

  for (auto const& uuid : m_entityMessagePromises.keys()) {
    if (auto clientInfo = m_clientInfo.value(m_entityMessagePromises[uuid].first)) {
      auto result186 = m_entityMessagePromises[uuid].second.finished();
      if (!result186)
        return std::unexpected(std::move(result186.error()));
      if (std::move(*result186)) {
        auto result187 = m_entityMessagePromises[uuid].second.succeeded();
        if (!result187)
          return std::unexpected(std::move(result187.error()));
        if (std::move(*result187)) {
          auto result188 = m_entityMessagePromises[uuid].second.result();
          if (!result188)
            return std::unexpected(std::move(result188.error()));
          clientInfo->outgoingPackets.push_back(
            std::make_shared<EntityMessageResponsePacket>(*std::move(*result188), uuid));
        } else {
          auto result189 = m_entityMessagePromises[uuid].second.error();
          if (!result189)
            return std::unexpected(std::move(result189.error()));
          clientInfo->outgoingPackets.push_back(
            std::make_shared<EntityMessageResponsePacket>(std::unexpected(*std::move(*result189)), uuid));
        }
        m_entityMessagePromises.remove(uuid);
      }
    } else {
      m_entityMessagePromises.remove(uuid);
    }
  }

  bool sendRemoteUpdates = m_entityUpdateTimer.wrapTick(dt);
  std::optional<Vec2I> activeRegionPad;
  // These protocol-neutral packets are read-only after construction. Retain
  // them only through this final snapshot phase, never across simulation ticks.
  SectorSnapshotCache sectorSnapshots;
  for (auto const& pair : m_clientInfo) {
    auto result190 = pair.second->monitoringRegions(m_entityMap);
    if (!result190)
      return std::unexpected(std::move(result190.error()));
    // Simulation, sector generation and entity removal have finished. Keep
    // this client's regions through the final activation/snapshot phase only.
    for (auto const& monitoredRegion : *result190) {
      if (!activeRegionPad) {
        auto result191 = m_serverConfig.get("playerActiveRegionPad");
        if (!result191)
          return std::unexpected(std::move(result191.error()));
        auto result192 = jsonToVec2I(std::move(*result191));
        if (!result192)
          return std::unexpected(std::move(result192.error()));
        activeRegionPad = std::move(*result192);
      }
      auto result193 = signalRegion(monitoredRegion.padded(*activeRegionPad));
      if (!result193)
        return std::unexpected(std::move(result193.error()));
    }
    // Dungeon activation can synchronously load tiles; ordinary activation
    // only queues future generation and refreshes sector TTLs.
    if (m_generatingDungeon)
      sectorSnapshots.clear();
    auto result194 = queueUpdatePackets(pair.first, sendRemoteUpdates, *result190, sectorSnapshots);
    if (!result194)
      return std::unexpected(std::move(result194.error()));
  }
  m_netStateCache.clear();

  for (auto& pair : m_clientInfo)
    pair.second->pendingForward = false;

  m_expiryTimer.tick(dt);

  this->root().logMap().set(Star::format("server_{}_entities", m_worldId), Star::format("{} in {} sectors", m_entityMap->size(), m_tileArray->loadedSectorCount()));
  this->root().logMap().set(Star::format("server_{}_time", m_worldId), Star::format("age = {:4.2f}, day = {:4.2f}/{:4.2f}s", epochTime(), timeOfDay(), dayLength()));
  this->root().logMap().set(Star::format("server_{}_active_liquid", m_worldId), m_liquidEngine->activeCells());
  this->root().logMap().set(Star::format("server_{}_lua_mem", m_worldId), m_luaRoot->luaMemoryUsage());
  return {};
}

WorldGeometry WorldServer::geometry() const {
  return m_geometry;
}

std::uint64_t WorldServer::currentStep() const {
  return m_currentStep;
}

MaterialId WorldServer::material(Vec2I const& pos, TileLayer layer) const {
  return m_tileArray->tile(pos).material(layer);
}

MaterialHue WorldServer::materialHueShift(Vec2I const& position, TileLayer layer) const {
  auto const& tile = m_tileArray->tile(position);
  return layer == TileLayer::Foreground ? tile.foregroundHueShift : tile.backgroundHueShift;
}

ModId WorldServer::mod(Vec2I const& pos, TileLayer layer) const {
  return m_tileArray->tile(pos).mod(layer);
}

MaterialHue WorldServer::modHueShift(Vec2I const& position, TileLayer layer) const {
  auto const& tile = m_tileArray->tile(position);
  return layer == TileLayer::Foreground ? tile.foregroundModHueShift : tile.backgroundModHueShift;
}

MaterialColorVariant WorldServer::colorVariant(Vec2I const& position, TileLayer layer) const {
  auto const& tile = m_tileArray->tile(position);
  return layer == TileLayer::Foreground ? tile.foregroundColorVariant : tile.backgroundColorVariant;
}

std::shared_ptr<Entity> WorldServer::entity(EntityId entityId) const {
  return m_entityMap->entity(entityId);
}

std::expected<void, StarException> WorldServer::addEntity(std::shared_ptr<Entity> const& entity, EntityId entityId) {
  if (!entity)
    return {};

  auto reservedId = m_entityMap->reserveEntityId(entityId);
  if (!reservedId)
    return std::unexpected(std::move(reservedId.error()));

  auto reject = [&, &world = *this](StarException error, bool published) -> std::expected<void, StarException> {
    if (published) {
      if (auto tileEntity = as<TileEntity>(entity)) {
        if (auto result = world.updateTileEntityTiles(tileEntity, true, false); !result)
          error = error.with_context(Star::format("Error clearing rejected entity tiles: {}", result.error().what()));
      }
      world.m_entityMap->removeEntity(entity->entityId());
    }
    if (auto result = entity->uninit(); !result)
      error = error.with_context(Star::format("Error uninitializing rejected entity: {}", result.error().what()));
    return std::unexpected(std::move(error));
  };

  if (auto result = entity->init(*this, *reservedId, EntityMode::Master); !result)
    return reject(std::move(result.error()), false);
  if (auto result = m_entityMap->addEntity(entity); !result)
    return reject(std::move(result.error()), false);

  if (auto tileEntity = as<TileEntity>(entity)) {
    if (auto result = updateTileEntityTiles(tileEntity); !result)
      return reject(std::move(result.error()), true);
  }
  return {};
}

std::expected<std::shared_ptr<Entity>, StarException> WorldServer::closestEntity(Vec2F const& center, float radius, EntityFilter selector) const {
  auto result200 = m_entityMap->closestEntity(center, radius, selector);
  if (!result200)
    return std::unexpected(std::move(result200.error()));
  return std::move(*result200);
}

std::expected<void, StarException> WorldServer::forAllEntities(EntityCallback callback) const {
  auto result201 = m_entityMap->forAllEntities(callback);
  if (!result201)
    return std::unexpected(std::move(result201.error()));
  return {};
}

std::expected<void, StarException> WorldServer::forEachEntity(RectF const& boundBox, EntityCallback callback) const {
  auto result202 = m_entityMap->forEachEntity(boundBox, callback);
  if (!result202)
    return std::unexpected(std::move(result202.error()));
  return {};
}

std::expected<void, StarException> WorldServer::forEachEntityLine(Vec2F const& begin, Vec2F const& end, EntityCallback callback) const {
  auto result203 = m_entityMap->forEachEntityLine(begin, end, callback);
  if (!result203)
    return std::unexpected(std::move(result203.error()));
  return {};
}

std::expected<void, StarException> WorldServer::forEachEntityAtTile(Vec2I const& pos, EntityCallbackOf<TileEntity> callback) const {
  auto result204 = m_entityMap->forEachEntityAtTile(pos, callback);
  if (!result204)
    return std::unexpected(std::move(result204.error()));
  return {};
}

std::expected<std::shared_ptr<Entity>, StarException> WorldServer::findEntity(RectF const& boundBox, EntityFilter entityFilter) const {
  auto result205 = m_entityMap->findEntity(boundBox, entityFilter);
  if (!result205)
    return std::unexpected(std::move(result205.error()));
  return std::move(*result205);
}

std::expected<std::shared_ptr<Entity>, StarException> WorldServer::findEntityLine(Vec2F const& begin, Vec2F const& end, EntityFilter entityFilter) const {
  auto result206 = m_entityMap->findEntityLine(begin, end, entityFilter);
  if (!result206)
    return std::unexpected(std::move(result206.error()));
  return std::move(*result206);
}

std::expected<std::shared_ptr<Entity>, StarException> WorldServer::findEntityAtTile(Vec2I const& pos, EntityFilterOf<TileEntity> entityFilter) const {
  auto result207 = m_entityMap->findEntityAtTile(pos, entityFilter);
  if (!result207)
    return std::unexpected(std::move(result207.error()));
  return std::move(*result207);
}

std::expected<bool, StarException> WorldServer::tileIsOccupied(Vec2I const& pos, TileLayer layer, bool includeEphemeral, bool checkCollision) const {
  auto result208 = WorldImpl::tileIsOccupied(m_tileArray, m_entityMap, pos, layer, includeEphemeral, checkCollision);
  if (!result208)
    return std::unexpected(std::move(result208.error()));
  return std::move(*result208);
}

std::expected<CollisionKind, StarException> WorldServer::tileCollisionKind(Vec2I const& pos) const {
  return WorldImpl::tileCollisionKind(m_tileArray, m_entityMap, pos);
}


std::expected<void, StarException> WorldServer::forEachCollisionBlock(RectI const& region, std::function<std::expected<void, StarException>(CollisionBlock const&)> const& iterator) const {
  if (auto result = const_cast<WorldServer&>(*this).freshenCollision(region); !result)
    return std::unexpected(std::move(result.error()));
  auto result211 = m_tileArray->tileEach(region, [iterator](Vec2I const& pos, ServerTile const& tile) -> std::expected<void, StarException> {
    if (!iterator)
      return std::unexpected(WorldServerException("Empty collision block iterator"));
    if (tile.getCollision() == CollisionKind::Null) {
      auto result209 = iterator(CollisionBlock::nullBlock(pos));
      if (!result209)
        return std::unexpected(std::move(result209.error()));
    } else {
      if constexpr (Star::DebugEnabled) {
        Star::star_assert_failed((!tile.collisionCacheDirty));
      }
      for (auto const& block : tile.collisionCache) {
        auto result210 = iterator(block);
        if (!result210)
          return std::unexpected(std::move(result210.error()));
      }
    }
    return {};
  });
  if (!result211)
    return std::unexpected(std::move(result211.error()));
  return {};
}

std::expected<bool, StarException> WorldServer::isTileConnectable(Vec2I const& pos, TileLayer layer, bool tilesOnly) const {
  return m_tileArray->tile(pos).isConnectable(layer, tilesOnly);
}

std::expected<bool, StarException> WorldServer::pointTileCollision(Vec2F const& point, CollisionSet const& collisionSet) const {
  return m_tileArray->tile(Vec2I(point.floor())).isColliding(collisionSet);
}

std::expected<bool, StarException> WorldServer::lineTileCollision(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet) const {
  auto result212 = WorldImpl::lineTileCollision(m_geometry, m_tileArray, begin, end, collisionSet);
  if (!result212)
    return std::unexpected(std::move(result212.error()));
  return std::move(*result212);
}

std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> WorldServer::lineTileCollisionPoint(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet) const {
  auto result213 = WorldImpl::lineTileCollisionPoint(m_geometry, m_tileArray, begin, end, collisionSet);
  if (!result213)
    return std::unexpected(std::move(result213.error()));
  return std::move(*result213);
}

std::expected<List<Vec2I>, StarException> WorldServer::collidingTilesAlongLine(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet, int maxSize, bool includeEdges) const {
  auto result214 = WorldImpl::collidingTilesAlongLine(m_geometry, m_tileArray, begin, end, collisionSet, maxSize, includeEdges);
  if (!result214)
    return std::unexpected(std::move(result214.error()));
  return std::move(*result214);
}

std::expected<bool, StarException> WorldServer::rectTileCollision(RectI const& region, CollisionSet const& collisionSet) const {
  auto result215 = WorldImpl::rectTileCollision(m_tileArray, region, collisionSet);
  if (!result215)
    return std::unexpected(std::move(result215.error()));
  return std::move(*result215);
}

LiquidLevel WorldServer::liquidLevel(Vec2I const& pos) const {
  return m_tileArray->tile(pos).liquid;
}

std::expected<LiquidLevel, StarException> WorldServer::liquidLevel(RectF const& region) const {
  auto result216 = WorldImpl::liquidLevel(m_tileArray, region);
  if (!result216)
    return std::unexpected(std::move(result216.error()));
  return std::move(*result216);
}

std::expected<void, StarException> WorldServer::activateLiquidRegion(RectI const& region) {
  m_liquidEngine->visitRegion(region);
  return {};
}

std::expected<void, StarException> WorldServer::activateLiquidLocation(Vec2I const& location) {
  m_liquidEngine->visitLocation(location);
  return {};
}

void WorldServer::requestGlobalBreakCheck() {
  m_needsGlobalBreakCheck = true;
}

void WorldServer::setSpawningEnabled(bool spawningEnabled) {
  m_spawner->setActive(spawningEnabled);
}

void WorldServer::setPropertyListener(String const& propertyName, WorldPropertyListener listener) {
  m_worldPropertyListeners[propertyName] = std::move(listener);
}

std::expected<TileModificationList, StarException> WorldServer::validTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap) const {
  auto result218 = WorldImpl::splitTileModifications(this->root(), m_entityMap, modificationList, allowEntityOverlap, m_tileGetterFunction, [&world = *this](Vec2I pos, TileModification) -> std::expected<bool, StarException> {
    return !world.isTileProtected(pos);
  });
  if (!result218)
    return std::unexpected(std::move(result218.error()));
  return std::move(*result218).first;
}

std::expected<TileModificationList, StarException> WorldServer::applyTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap) {
  auto result219 = doApplyTileModifications(modificationList, allowEntityOverlap);
  if (!result219)
    return std::unexpected(std::move(result219.error()));
  return std::move(*result219);
}

std::expected<bool, StarException> WorldServer::forceModifyTile(Vec2I const& pos, TileModification const& modification, bool allowEntityOverlap) {
  auto result220 = forceApplyTileModifications({{pos, modification}}, allowEntityOverlap);
  if (!result220)
    return std::unexpected(std::move(result220.error()));
  return std::move(*result220).empty();
}

std::expected<TileModificationList, StarException> WorldServer::forceApplyTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap) {
  auto result221 = doApplyTileModifications(modificationList, allowEntityOverlap, true);
  if (!result221)
    return std::unexpected(std::move(result221.error()));
  return std::move(*result221);
}

std::expected<bool, StarException> WorldServer::replaceTile(Vec2I const& pos, TileModification const& modification, TileDamage const& tileDamage) {
  if (isTileProtected(pos))
    return false;

  auto result223 = WorldImpl::validateTileReplacement(this->root(), modification);
  if (!result223)
    return std::unexpected(std::move(result223.error()));
  if (!std::move(*result223))
    return false;

  if (std::holds_alternative<PlaceMaterial>(modification)) {
    auto const& placeMaterial = std::get<PlaceMaterial>(modification);
    auto result224 = isTileConnectable(pos, placeMaterial.layer, true);
    if (!result224)
      return std::unexpected(std::move(result224.error()));
    if (!std::move(*result224))
      return false;

    if (auto tile = m_tileArray->modifyTile(pos)) {
      auto result225 = WorldImpl::tileDamageParameters(this->root(), tile->get(), placeMaterial.layer, tileDamage);
      if (!result225)
        return std::unexpected(std::move(result225.error()));
      auto damageParameters = std::move(*result225);
      bool harvested = tileDamage.amount >= 0 && tileDamage.harvestLevel >= damageParameters.requiredHarvestLevel();
      auto damage = placeMaterial.layer == TileLayer::Foreground ? tile->get().foregroundDamage : tile->get().backgroundDamage;
      Vec2F dropPosition = centerOfTile(pos);

      auto result226 = destroyBlock(placeMaterial.layer, pos, harvested, !tileDamageIsPenetrating(damage.damageType()), false);
      if (!result226)
        return std::unexpected(std::move(result226.error()));
      for (auto const& drop : std::move(*result226)) {
        auto result227 = ItemDrop::createRandomizedDrop(this->root(), drop, dropPosition);
        if (!result227)
          return std::unexpected(std::move(result227.error()));
        auto result228 = addEntity(std::move(*result227), NullEntityId);
        if (!result228)
          return std::unexpected(std::move(result228.error()));
      }

      return true;
    }
  }

  return false;
}

std::expected<TileModificationList, StarException> WorldServer::replaceTiles(TileModificationList const& modificationList, TileDamage const& tileDamage, bool applyDamage) {
  TileModificationList success, failures;

  if (applyDamage) {
    List<Vec2I> toDamage;
    TileLayer layer;

    for (auto pair : modificationList) {
      if (std::holds_alternative<PlaceMaterial>(pair.second)) {
        auto const& placeMaterial = std::get<PlaceMaterial>(pair.second);
        layer = placeMaterial.layer;

        if (placeMaterial.material == material(pair.first, layer)) {
          failures.push_back(pair);
          continue;
        }

        auto result229 = damageWouldDestroy(pair.first, layer, tileDamage);
        if (!result229)
          return std::unexpected(std::move(result229.error()));
        if (std::move(*result229)) {
          auto result230 = replaceTile(pair.first, pair.second, tileDamage);
          if (!result230)
            return std::unexpected(std::move(result230.error()));
          if (std::move(*result230))
            success.push_back(pair);
          else {
            failures.push_back(pair);
          }
          continue;
        }

        toDamage.push_back(pair.first);
        success.push_back(pair);
        continue;
      }

      failures.push_back(pair);
    }

    if (!toDamage.empty()) {
      auto result231 = damageTiles(toDamage, layer, Vec2F(), tileDamage, std::optional<EntityId>());
      if (!result231)
        return std::unexpected(std::move(result231.error()));
    }
  } else {
    for (auto pair : modificationList) {
      auto result232 = replaceTile(pair.first, pair.second, tileDamage);
      if (!result232)
        return std::unexpected(std::move(result232.error()));
      if (std::move(*result232))
        success.push_back(pair);
      else {
        failures.push_back(pair);
      }
    }
  }

  auto result233 = doApplyTileModifications(success, true, false, false);
  if (!result233)
    return std::unexpected(std::move(result233.error()));
  failures.append_range(std::move(*result233));

  for (auto pair : success) {
    auto result234 = checkEntityBreaks(RectF::withSize(Vec2F(pair.first), Vec2F(1, 1)));
    if (!result234)
      return std::unexpected(std::move(result234.error()));
    m_liquidEngine->visitLocation(pair.first);
    m_fallingBlocksAgent->visitLocation(pair.first);
  }

  return failures;
}

std::expected<bool, StarException> WorldServer::damageWouldDestroy(Vec2I const& pos, TileLayer layer, TileDamage const& tileDamage) const {
  auto result235 = WorldImpl::damageWouldDestroy(this->root(), m_tileArray, pos, layer, tileDamage);
  if (!result235)
    return std::unexpected(std::move(result235.error()));
  return std::move(*result235);
}

std::expected<TileDamageResult, StarException> WorldServer::damageTiles(List<Vec2I> const& positions, TileLayer layer, Vec2F const& sourcePosition, TileDamage const& damage, std::optional<EntityId> sourceEntity) {
  Set<Vec2I> positionSet;
  for (auto const& pos : positions)
    positionSet.add(m_geometry.xwrap(pos));

  Set<std::shared_ptr<Entity>> damagedEntities;
  auto res = TileDamageResult::None;

  for (auto const& pos : positionSet) {
    if (auto tile = m_tileArray->modifyTile(pos)) {
      auto tileDamage = damage;
      if (isTileProtected(pos))
        tileDamage.type = TileDamageType::Protected;

      auto tileRes = TileDamageResult::None;
      if (layer == TileLayer::Foreground) {
        Vec2I entityDamagePos = pos;
        Set<Vec2I> damagePositionSet = Set<Vec2I>(positionSet);
        if (tile->get().rootSource) {
          entityDamagePos = *tile->get().rootSource;
          damagePositionSet.add(entityDamagePos);
        }

        auto result237 = m_entityMap->entitiesAtTile(entityDamagePos);
        if (!result237)
          return std::unexpected(std::move(result237.error()));
        for (auto const& entity : *result237) {
          if (!damagedEntities.contains(entity)) {
            Set<Vec2I> entitySpacesSet;
            for (auto const& space : entity->spaces())
              entitySpacesSet.add(m_geometry.xwrap(entity->tilePosition() + space));

            List<Vec2I> entityDamagePositions;
            entityDamagePositions.reserve(std::min(entitySpacesSet.size(), damagePositionSet.size()));
            for (auto const& space : entitySpacesSet) {
              if (damagePositionSet.contains(space))
                entityDamagePositions.push_back(space);
            }
            auto result238 = entity->damageTiles(entityDamagePositions, sourcePosition, tileDamage);
            if (!result238)
              return std::unexpected(std::move(result238.error()));
            bool broken = std::move(*result238);
            if (sourceEntity.has_value() && broken) {
              std::optional<String> name;
              if (auto object = as<Object>(entity)) {
                auto result239 = object->name();
                if (!result239)
                  return std::unexpected(std::move(result239.error()));
                name = std::move(*result239);
              }
              if (!EntityTypeNames)
                return std::unexpected(EntityTypeNames.error());
              auto result240 = (*EntityTypeNames).get_right(entity->entityType());
              if (!result240)
                return std::unexpected(std::move(result240.error()));
              auto result241 = sendEntityMessage(*sourceEntity, "tileEntityBroken", {
                                jsonFromVec2I(pos),
                                result240->get(),
                                jsonFromOptional(name),
                              });
              if (!result241)
                return std::unexpected(std::move(result241.error()));
            }

            if (tileDamage.type == TileDamageType::Protected)
              tileRes = TileDamageResult::Protected; else {
              bool condition243 = static_cast<bool>(broken);
              if (!condition243) {
                auto result242 = entity->canBeDamaged();
                if (!result242)
                  return std::unexpected(std::move(result242.error()));
                condition243 = static_cast<bool>(std::move(*result242));
              }
              if (condition243) {
                tileRes = TileDamageResult::Normal;
                damagedEntities.add(entity);
              }
            }
          }
        }
      }

      // Penetrating damage should carry through to the blocks behind this
      // entity.
      if (tileRes == TileDamageResult::None || tileDamageIsPenetrating(tileDamage.type)) {
        auto result244 = WorldImpl::tileDamageParameters(this->root(), tile->get(), layer, tileDamage);
        if (!result244)
          return std::unexpected(std::move(result244.error()));
        auto damageParameters = std::move(*result244);

        if (layer == TileLayer::Foreground && isRealMaterial(tile->get().foreground)) {
          if (!tile->get().rootSource || damagedEntities.empty()) {
            auto result245 = tile->get().foregroundDamage.damage(damageParameters, sourcePosition, tileDamage);
            if (!result245)
              return std::unexpected(std::move(result245.error()));

            // if the tile is broken, send a message back to the source entity with position, layer, dungeonId, and whether the tile was harvested
            if (sourceEntity.has_value() && tile->get().foregroundDamage.dead()) {
              auto result246 = enumName<TileLayerNames>(TileLayer::Foreground);
              if (!result246)
                return std::unexpected(std::move(result246.error()));
              auto result247 = sendEntityMessage(*sourceEntity, "tileBroken",
                                                 {
                                                   jsonFromVec2I(pos),
                                                   String(*result246),
                                                   tile->get().foreground,
                                                   tile->get().dungeonId,
                                                   tile->get().foregroundDamage.harvested(),
                                                 });
              if (!result247)
                return std::unexpected(std::move(result247.error()));
            }

            auto result248 = queueTileDamageUpdates(pos, TileLayer::Foreground);
            if (!result248)
              return std::unexpected(std::move(result248.error()));
            m_damagedBlocks.add(pos);

            if (tileDamage.type == TileDamageType::Protected)
              tileRes = TileDamageResult::Protected; else {
              tileRes = TileDamageResult::Normal;
            }
          }
        } else if (layer == TileLayer::Background && isRealMaterial(tile->get().background)) {
          auto result249 = tile->get().backgroundDamage.damage(damageParameters, sourcePosition, tileDamage);
          if (!result249)
            return std::unexpected(std::move(result249.error()));

          // if the tile is broken, send a message back to the source entity with position and whether the tile was harvested
          if (sourceEntity.has_value() && tile->get().backgroundDamage.dead()) {
            auto result250 = enumName<TileLayerNames>(TileLayer::Background);
            if (!result250)
              return std::unexpected(std::move(result250.error()));
            auto result251 = sendEntityMessage(*sourceEntity, "tileBroken",
                                               {
                                                 jsonFromVec2I(pos),
                                                 String(*result250),
                                                 tile->get().background,
                                                 tile->get().dungeonId,
                                                 tile->get().backgroundDamage.harvested(),
                                               });
            if (!result251)
              return std::unexpected(std::move(result251.error()));
          }

          auto result252 = queueTileDamageUpdates(pos, TileLayer::Background);
          if (!result252)
            return std::unexpected(std::move(result252.error()));
          m_damagedBlocks.add(pos);

          if (tileDamage.type == TileDamageType::Protected)
            tileRes = TileDamageResult::Protected; else {
            tileRes = TileDamageResult::Normal;
          }
        }
      }

      res = std::max<TileDamageResult>(res, tileRes);
    }
  }

  return res;
}

DungeonId WorldServer::dungeonId(Vec2I const& pos) const {
  return m_tileArray->tile(pos).dungeonId;
}

std::expected<bool, StarException> WorldServer::isPlayerModified(RectI const& region) const {
  auto result253 = m_tileArray->tileSatisfies(region, [](Vec2I const&, ServerTile const& tile) -> std::expected<bool, StarException> {
    return tile.dungeonId == ConstructionDungeonId || tile.dungeonId == DestroyedBlockDungeonId;
  });
  if (!result253)
    return std::unexpected(std::move(result253.error()));
  return std::move(*result253);
}

std::expected<ItemDescriptor, StarException> WorldServer::collectLiquid(List<Vec2I> const& tilePositions, LiquidId liquidId) {
  auto result254 = this->root().assets();
  if (!result254)
    return std::unexpected(std::move(result254.error()));
  auto result255 = std::move(*result254)->json("/items/defaultParameters.config:liquidItems.bucketSize");
  if (!result255)
    return std::unexpected(std::move(result255.error()));
  auto result256 = std::move(*result255).toFloat();
  if (!result256)
    return std::unexpected(std::move(result256.error()));
  float bucketSize = std::move(*result256);
  unsigned drainedUnits = 0;
  float nextUnit = bucketSize;
  List<std::reference_wrapper<ServerTile>> maybeDrainTiles;

  for (auto pos : tilePositions) {
    auto tile = m_tileArray->modifyTile(pos);
    if (tile->get().liquid.liquid == liquidId && !isTileProtected(pos)) {
      if (tile->get().liquid.level >= nextUnit) {
        tile->get().liquid.take(nextUnit);
        nextUnit = bucketSize;
        drainedUnits++;

        for (auto previousTile : maybeDrainTiles)
          previousTile.get().liquid.take(previousTile.get().liquid.level);

        maybeDrainTiles.clear();
      }

      if (tile->get().liquid.level > 0) {
        nextUnit -= tile->get().liquid.level;
        maybeDrainTiles.push_back(*tile);
      }

      for (auto const& pair : m_clientInfo) {
        if (pair.second->activeSectors.contains(m_tileArray->sectorFor(pos)))
          pair.second->pendingLiquidUpdates.add(pos);
      }
      m_liquidEngine->visitLocation(pos);
    }
  }

  if (drainedUnits > 0) {
    auto result259 = this->root().liquidsDatabase();
    if (!result259)
      return std::unexpected(std::move(result259.error()));
    auto liquidConfig = std::move(*result259)->liquidSettings(liquidId);
    if (liquidConfig && liquidConfig->itemDrop)
      return liquidConfig->itemDrop.multiply(drainedUnits);
  }

  return ItemDescriptor();
}

std::expected<bool, StarException> WorldServer::placeDungeon(String const& dungeonName, Vec2I const& position, std::optional<DungeonId> dungeonId, bool forcePlacement) {
  m_generatingDungeon = true;
  m_tileProtectionEnabled = false;
  auto generationScope = finally([&world = *this] {
    world.m_tileProtectionEnabled = true;
    world.m_generatingDungeon = false;
  });

  auto seed = worldTemplate()->seedFor(position[0], position[1]);
  auto facade = std::make_shared<DungeonGeneratorWorld>(*this, true);
  bool placed = false;
  auto result261 = DungeonGenerator::create(this->root(), dungeonName, seed, m_worldTemplate->threatLevel(), dungeonId);
  if (!result261)
    return std::unexpected(std::move(result261.error()));
  auto dungeonGenerator = std::move(*result261);
  auto result262 = dungeonGenerator->generate(facade, position, false, forcePlacement);
  if (!result262)
    return std::unexpected(std::move(result262.error()));
  if (auto generateResult = std::move(*result262)) {
    auto worldGenerator = std::make_shared<WorldGenerator>(*this);
    for (auto position : generateResult->second) {
      auto result263 = modifyServerTile(position);
      if (!result263)
        return std::unexpected(std::move(result263.error()));
      if (auto tile = std::move(*result263)) {
        auto result264 = worldGenerator->replaceBiomeBlocks(tile->get());
        if (!result264)
          return std::unexpected(std::move(result264.error()));
      }
    }
    placed = true;
  }

  return placed;
}

std::expected<void, StarException> WorldServer::addBiomeRegion(Vec2I const& position, String const& biomeName, String const& subBlockSelector, int width) {
  width = std::min(width, static_cast<int>(m_worldTemplate->size()[0]));

  auto result265 = m_worldTemplate->previewAddBiomeRegion(position, width);
  if (!result265)
    return std::unexpected(std::move(result265.error()));
  auto regions = std::move(*result265);

  if (regions.empty()) {
    auto result266 = this->root().logger().info("Aborting addBiomeRegion as it would have no result!");
    if (!result266)
      return std::unexpected(std::move(result266.error()));
    return {};
  }

  for (auto const& region : regions) {
    auto result267 = m_worldStorage->sectorsForRegion(region);
    if (!result267)
      return std::unexpected(std::move(result267.error()));
    for (auto sector : std::move(*result267)) {
      auto result268 = m_worldStorage->triggerTerraformSector(sector);
      if (!result268)
        return std::unexpected(std::move(result268.error()));
    }
  }

  auto result269 = m_worldTemplate->addBiomeRegion(position, biomeName, subBlockSelector, width);
  if (!result269)
    return std::unexpected(std::move(result269.error()));
  return {};
}

std::expected<void, StarException> WorldServer::expandBiomeRegion(Vec2I const& position, int newWidth) {
  newWidth = std::min(newWidth, static_cast<int>(m_worldTemplate->size()[0]));

  auto result270 = m_worldTemplate->previewExpandBiomeRegion(position, newWidth);
  if (!result270)
    return std::unexpected(std::move(result270.error()));
  auto regions = std::move(*result270);

  if (regions.empty()) {
    auto result271 = this->root().logger().info("Aborting expandBiomeRegion as it would have no result!");
    if (!result271)
      return std::unexpected(std::move(result271.error()));
    return {};
  }

  for (auto const& region : regions) {
    auto result272 = m_worldStorage->sectorsForRegion(region);
    if (!result272)
      return std::unexpected(std::move(result272.error()));
    for (auto sector : std::move(*result272)) {
      auto result273 = m_worldStorage->triggerTerraformSector(sector);
      if (!result273)
        return std::unexpected(std::move(result273.error()));
    }
  }

  auto result274 = m_worldTemplate->expandBiomeRegion(position, newWidth);
  if (!result274)
    return std::unexpected(std::move(result274.error()));
  return {};
}

std::expected<bool, StarException> WorldServer::pregenerateAddBiome(Vec2I const& position, int width) {
  auto result275 = m_worldTemplate->previewAddBiomeRegion(position, width);
  if (!result275)
    return std::unexpected(std::move(result275.error()));
  auto regions = std::move(*result275);

  bool generationComplete = true;
  for (auto const& region : regions) {
    bool condition277 = static_cast<bool>(generationComplete);
    if (condition277) {
      auto result276 = signalRegion(region);
      if (!result276)
        return std::unexpected(std::move(result276.error()));
      condition277 = static_cast<bool>(std::move(*result276));
    }
    generationComplete = condition277;
  }

  return generationComplete;
}

std::expected<bool, StarException> WorldServer::pregenerateExpandBiome(Vec2I const& position, int newWidth) {
  auto result278 = m_worldTemplate->previewExpandBiomeRegion(position, newWidth);
  if (!result278)
    return std::unexpected(std::move(result278.error()));
  auto regions = std::move(*result278);

  bool generationComplete = true;
  for (auto const& region : regions) {
    bool condition280 = static_cast<bool>(generationComplete);
    if (condition280) {
      auto result279 = signalRegion(region);
      if (!result279)
        return std::unexpected(std::move(result279.error()));
      condition280 = static_cast<bool>(std::move(*result279));
    }
    generationComplete = condition280;
  }

  return generationComplete;
}

std::expected<void, StarException> WorldServer::setLayerEnvironmentBiome(Vec2I const& position) {
  auto result281 = m_worldTemplate->worldLayout()->setLayerEnvironmentBiome(position);
  if (!result281)
    return std::unexpected(std::move(result281.error()));
  auto biomeName = std::move(*result281);

  auto result282 = m_worldTemplate->worldLayout()->toJson();
  if (!result282)
    return std::unexpected(std::move(result282.error()));
  auto layoutJson = std::move(*result282);
  for (auto const& pair : m_clientInfo)
    pair.second->outgoingPackets.push_back(std::make_shared<WorldLayoutUpdatePacket>(layoutJson));
  return {};
}

std::expected<void, StarException> WorldServer::setPlanetType(String const& planetType, String const& primaryBiomeName) {
  if (auto terrestrialParameters = as<TerrestrialWorldParameters>(m_worldTemplate->worldParameters())) {
    if (terrestrialParameters->typeName != planetType) {
      auto newTerrestrialParameters = std::make_shared<TerrestrialWorldParameters>(*terrestrialParameters);

      newTerrestrialParameters->typeName = planetType;
      newTerrestrialParameters->primaryBiome = primaryBiomeName;

      auto result283 = this->root().biomeDatabase();
      if (!result283)
        return std::unexpected(std::move(result283.error()));
      auto biomeDatabase = std::move(*result283);
      auto result284 = biomeDatabase->biomeWeathers(primaryBiomeName, m_worldTemplate->worldSeed(), m_worldTemplate->threatLevel());
      if (!result284)
        return std::unexpected(std::move(result284.error()));
      auto newWeatherPool = std::move(*result284);
      newTerrestrialParameters->weatherPool = newWeatherPool;

      auto result285 = biomeDatabase->biomeSkyColoring(primaryBiomeName, m_worldTemplate->worldSeed());
      if (!result285)
        return std::unexpected(std::move(result285.error()));
      auto newSkyColors = std::move(*result285);
      newTerrestrialParameters->skyColoring = newSkyColors;

      auto result286 = biomeDatabase->biomeIsAirless(primaryBiomeName);
      if (!result286)
        return std::unexpected(std::move(result286.error()));
      newTerrestrialParameters->airless = std::move(*result286);
      newTerrestrialParameters->environmentStatusEffects = {};

      newTerrestrialParameters->terraformed = true;

      auto result287 = m_worldTemplate->setWorldParameters(newTerrestrialParameters);
      if (!result287)
        return std::unexpected(std::move(result287.error()));

      for (auto const& pair : m_clientInfo) {
        auto result288 = netStoreVisitableWorldParameters(newTerrestrialParameters);
        if (!result288)
          return std::unexpected(std::move(result288.error()));
        pair.second->outgoingPackets.push_back(std::make_shared<WorldParametersUpdatePacket>(std::move(*result288)));
      }

      auto newSkyParameters = SkyParameters(m_worldTemplate->skyParameters(), newTerrestrialParameters);
      m_worldTemplate->setSkyParameters(newSkyParameters);

      auto referenceClock = m_sky->referenceClock();
      auto result290 = Sky::create(this->root(), m_worldTemplate->skyParameters(), false);
      if (!result290)
        return std::unexpected(std::move(result290.error()));
      m_sky = std::move(*result290);
      m_sky->setReferenceClock(referenceClock);

      auto result291 = setupWeatherDomains(true);
      if (!result291)
        return std::unexpected(std::move(result291.error()));

      m_newPlanetType = std::pair<String, String>{planetType, primaryBiomeName};
    }
  }
  return {};
}


std::expected<void, StarException> WorldServer::setWeatherIndex(std::size_t weatherIndex, bool force) {
  auto result292 = surfaceWeather()->setWeatherIndex(weatherIndex, force);
  if (!result292)
    return std::unexpected(std::move(result292.error()));
  return {};
}

std::expected<void, StarException> WorldServer::setWeather(String const& weatherName, bool force) {
  auto result293 = surfaceWeather()->setWeather(weatherName, force);
  if (!result293)
    return std::unexpected(std::move(result293.error()));
  return {};
}

std::expected<void, StarException> WorldServer::setWeather(Vec2F const& position, String const& weatherName, bool force) {
  auto result294 = weatherAt(position);
  if (!result294)
    return std::unexpected(std::move(result294.error()));
  auto result295 = std::move(*result294)->setWeather(weatherName, force);
  if (!result295)
    return std::unexpected(std::move(result295.error()));
  return {};
}

std::expected<StringList, StarException> WorldServer::weatherList() const {
  return surfaceWeather()->weatherList();
}

std::expected<StringList, StarException> WorldServer::weatherList(Vec2F const& position) const {
  auto result296 = weatherAt(position);
  if (!result296)
    return std::unexpected(std::move(result296.error()));
  return std::move(*result296)->weatherList();
}

std::optional<std::pair<String, String>> WorldServer::pullNewPlanetType() {
  return std::exchange(m_newPlanetType, {});
}

bool WorldServer::isTileProtected(Vec2I const& pos) const {
  if (!m_tileProtectionEnabled)
    return false;

  auto const& tile = m_tileArray->tile(pos);
  return m_protectedDungeonIds.contains(tile.dungeonId);
}

bool WorldServer::getTileProtection(DungeonId dungeonId) const {
  return m_protectedDungeonIds.contains(dungeonId);
}

std::expected<void, StarException> WorldServer::setTileProtection(DungeonId dungeonId, bool isProtected) {
  bool updated = false;
  if (isProtected) {
    updated = m_protectedDungeonIds.add(dungeonId);
  } else {
    updated = m_protectedDungeonIds.remove(dungeonId);
  }

  if (updated) {
    for (auto const& pair : m_clientInfo)
      pair.second->outgoingPackets.push_back(std::make_shared<UpdateTileProtectionPacket>(dungeonId, isProtected));

    auto result297 = this->root().logger().info("Protected dungeonIds for world set to {}", m_protectedDungeonIds);
    if (!result297)
      return std::unexpected(std::move(result297.error()));
  }
  return {};
}

std::expected<std::size_t, StarException> WorldServer::setTileProtection(List<DungeonId> const& dungeonIds, bool isProtected) {
  List<std::shared_ptr<Packet>> updates;
  updates.reserve(dungeonIds.size());
  for (auto const& dungeonId : dungeonIds)
    if (isProtected ? m_protectedDungeonIds.add(dungeonId) : m_protectedDungeonIds.remove(dungeonId))
      updates.push_back(std::make_shared<UpdateTileProtectionPacket>(dungeonId, isProtected));

  if (updates.empty())
    return 0;

  for (auto const& pair : m_clientInfo)
    pair.second->outgoingPackets.append_range(updates);

  auto newDungeonIds = m_protectedDungeonIds.values();
  std::sort(newDungeonIds.begin(), newDungeonIds.end(), std::less<DungeonId>());
  auto result298 = this->root().logger().info("Protected dungeonIds for world set to {}", newDungeonIds);
  if (!result298)
    return std::unexpected(std::move(result298.error()));
  return updates.size();
}

void WorldServer::setTileProtectionEnabled(bool enabled) {
  m_tileProtectionEnabled = enabled;
}

std::expected<void, StarException> WorldServer::setDungeonId(RectI const& tileArea, DungeonId dungeonId) {
  for (int x = tileArea.xMin(); x < tileArea.xMax(); ++x) {
    for (int y = tileArea.yMin(); y < tileArea.yMax(); ++y) {
      auto pos = Vec2I{x, y};
      if (auto tile = m_tileArray->modifyTile(pos)) {
        tile->get().dungeonId = dungeonId;
        auto result299 = queueTileUpdates(pos);
        if (!result299)
          return std::unexpected(std::move(result299.error()));
      }
    }
  }
  return {};
}

std::expected<void, StarException> WorldServer::setDungeonGravity(DungeonId dungeonId, std::optional<float> gravity) {
  std::optional<float> current = m_dungeonIdGravity.maybe(dungeonId);
  if (gravity != current) {
    if (gravity)
      m_dungeonIdGravity[dungeonId] = *gravity; else {
      m_dungeonIdGravity.remove(dungeonId);
    }

    for (auto const& p : m_clientInfo)
      p.second->outgoingPackets.push_back(std::make_shared<SetDungeonGravityPacket>(dungeonId, gravity));
  }
  return {};
}

std::expected<float, StarException> WorldServer::gravity(Vec2F const& pos) const {
  auto result300 = gravityFromTile(m_tileArray->tile(Vec2I::round(pos)));
  if (!result300)
    return std::unexpected(std::move(result300.error()));
  return std::move(*result300);
}

std::expected<float, StarException> WorldServer::gravityFromTile(ServerTile const& tile) const {
  auto result301 = m_worldTemplate->gravity();
  if (!result301)
    return std::unexpected(std::move(result301.error()));
  return m_dungeonIdGravity.maybe(tile.dungeonId).value_or(std::move(*result301));
}

bool WorldServer::isFloatingDungeonWorld() const {
  return m_worldTemplate && m_worldTemplate->worldParameters()
      && m_worldTemplate->worldParameters()->type() == WorldParametersType::FloatingDungeonWorldParameters;
}

std::expected<void, StarException> WorldServer::init(bool firstTime) {
  auto& root = this->root();
  auto result302 = root.assets();
  if (!result302)
    return std::unexpected(std::move(result302.error()));
  auto assets = std::move(*result302);
  auto result303 = root.liquidsDatabase();
  if (!result303)
    return std::unexpected(std::move(result303.error()));
  auto liquidsDatabase = std::move(*result303);

  auto result304 = assets->json("/worldserver.config");
  if (!result304)
    return std::unexpected(std::move(result304.error()));
  m_serverConfig = std::move(*result304);
  auto result305 = setFidelity(WorldServerFidelity::Medium);
  if (!result305)
    return std::unexpected(std::move(result305.error()));

  m_worldStorage->setFloatingDungeonWorld(isFloatingDungeonWorld());

  m_currentTime = 0;
  m_currentStep = 0;
  m_generatingDungeon = false;
  m_geometry = WorldGeometry(m_worldTemplate->size());
  m_entityMap = m_worldStorage->entityMap();
  m_tileArray = m_worldStorage->tileArray();
  m_tileGetterFunction = [&world = *this](Vec2I pos) -> std::expected<std::reference_wrapper<ServerTile const>, StarException> {
  return std::cref(world.m_tileArray->tile(pos));
  };
  m_damageManager = std::make_shared<DamageManager>(*this, ServerConnectionId);
  m_wireProcessor = std::make_shared<WireProcessor>(m_worldStorage);
  auto result306 = LuaRoot::create(this->root());
  if (!result306)
    return std::unexpected(std::move(result306.error()));
  m_luaRoot = std::move(*result306);
  auto result307 = m_serverConfig.getFloat("luaGcPause");
  if (!result307)
    return std::unexpected(std::move(result307.error()));
  auto result308 = m_serverConfig.getFloat("luaGcStepMultiplier");
  if (!result308)
    return std::unexpected(std::move(result308.error()));
  m_luaRoot->tuneAutoGarbageCollection(std::move(*result307), std::move(*result308));

  auto result309 = Sky::create(this->root(), m_worldTemplate->skyParameters(), false);
  if (!result309)
    return std::unexpected(std::move(result309.error()));
  m_sky = std::move(*result309);

  auto result310 = assets->json("/lighting.config:intensity");
  if (!result310)
    return std::unexpected(std::move(result310.error()));
  auto result311 = m_lightIntensityCalculator.setParameters(std::move(*result310));
  if (!result311)
    return std::unexpected(std::move(result311.error()));

  m_entityMessageResponses = {};
  m_entityMessagePromises = {};

  m_collisionGenerator.init([&world = *this](int x, int y) -> std::expected<CollisionKind, StarException> {
  return world.m_tileArray->tile({x, y}).getCollision();
  });

  auto result312 = m_serverConfig.query("interpolationSettings.normal");
  if (!result312)
    return std::unexpected(std::move(result312.error()));
  auto result313 = std::move(*result312).getFloat("entityUpdateDelta");
  if (!result313)
    return std::unexpected(std::move(result313.error()));
  m_entityUpdateTimer = GameTimer(std::move(*result313) / 60.f);
  auto result314 = m_serverConfig.getFloat("tileEntityBreakCheckInterval");
  if (!result314)
    return std::unexpected(std::move(result314.error()));
  m_tileEntityBreakCheckTimer = GameTimer(std::move(*result314));

  auto liquidSeed = root.random().randu64();
  auto liquidWorld = LiquidWorld::create(*this);
  if (!liquidWorld)
    return std::unexpected(std::move(liquidWorld.error()));
  m_liquidEngine = std::make_shared<LiquidCellEngine<LiquidId>>(liquidSeed, liquidsDatabase->liquidEngineParameters(), std::move(*liquidWorld));
  for (auto const& liquidSettings : liquidsDatabase->allLiquidSettings())
    m_liquidEngine->setLiquidTickDelta(liquidSettings->id, liquidSettings->tickDelta);

  auto fallingBlocksWorld = FallingBlocksWorld::create(*this);
  if (!fallingBlocksWorld)
    return std::unexpected(std::move(fallingBlocksWorld.error()));
  auto result315 = FallingBlocksAgent::create(this->root(), std::move(*fallingBlocksWorld));
  if (!result315)
    return std::unexpected(std::move(result315.error()));
  m_fallingBlocksAgent = std::move(*result315);

  // Dungeon and spawned entity scripts may query wind or weather while the
  // initial world regions are being generated, so domains must exist first.
  auto result316 = setupWeatherDomains(false);
  if (!result316)
    return std::unexpected(std::move(result316.error()));

  auto result317 = setupForceRegions();
  if (!result317)
    return std::unexpected(std::move(result317.error()));

  auto result318 = setTileProtection(ProtectedZeroGDungeonId, true);
  if (!result318)
    return std::unexpected(std::move(result318.error()));

  auto initialize = [&, &world = *this]() -> std::expected<void, StarException> {
  world.m_spawner->init(std::make_shared<SpawnerWorld>(world));

  auto rnd = RandomSource(world.m_worldTemplate->worldSeed());

  if (firstTime) {
    world.m_generatingDungeon = true;
    DungeonId currentDungeonId = 0;

    auto result320 = world.m_worldTemplate->dungeons();
    if (!result320)
      return std::unexpected(std::move(result320.error()));
    for (auto const& dungeon : std::move(*result320)) {
      auto result321 = world.root().logger().info("Placing dungeon {}", dungeon.dungeon);
      if (!result321)
        return std::unexpected(std::move(result321.error()));
      auto result322 = world.m_serverConfig.getInt("spawnDungeonRetries");
      if (!result322)
        return std::unexpected(std::move(result322.error()));
      int retryCounter = std::move(*result322);
      while (retryCounter > 0) {
        --retryCounter;
        auto dungeonFacade = std::make_shared<DungeonGeneratorWorld>(world, true);
        auto result323 = rnd.randInt(0, dungeon.xVariance);
        if (!result323)
          return std::unexpected(std::move(result323.error()));
        Vec2I position = Vec2I((dungeon.baseX + std::move(*result323)) % world.m_geometry.width(), dungeon.baseHeight);
        auto result324 = DungeonGenerator::create(world.root(), dungeon.dungeon, world.m_worldTemplate->worldSeed(), world.m_worldTemplate->threatLevel(), currentDungeonId);
        if (!result324)
          return std::unexpected(std::move(result324.error()));
        auto dungeonGenerator = std::move(*result324);
        auto result325 = dungeonGenerator->generate(dungeonFacade, position, dungeon.blendWithTerrain, dungeon.force);
        if (!result325)
          return std::unexpected(std::move(result325.error()));
        if (auto generateResult = std::move(*result325)) {
          if (dungeonGenerator->definition()->isProtected()) {
            auto result326 = world.setTileProtection(currentDungeonId, true);
            if (!result326)
              return std::unexpected(std::move(result326.error()));
          }

          if (auto gravity = dungeonGenerator->definition()->gravity())
            world.m_dungeonIdGravity[currentDungeonId] = *gravity;

          if (auto breathable = dungeonGenerator->definition()->breathable())
            world.m_dungeonIdBreathable[currentDungeonId] = *breathable;

          currentDungeonId++;

          // floating dungeon worlds should force immediate generation (since there won't be terrain) to avoid
          // bottlenecking "generation" of empty generation levels during loading
          if (world.isFloatingDungeonWorld()) {
            for (auto const& region : generateResult->first) {
              auto result327 = world.generateRegion(region);
              if (!result327)
                return std::unexpected(std::move(result327.error()));
            }
          }

          break;
        }
      }
    }

    world.m_dungeonIdGravity[ZeroGDungeonId] = 0.0;
    world.m_dungeonIdGravity[ProtectedZeroGDungeonId] = 0.0;

    world.m_generatingDungeon = false;
  }

  if (world.m_adjustPlayerStart) {
    auto result328 = world.findPlayerStart(firstTime ? std::optional<Vec2F>() : world.m_playerStart);
    if (!result328)
      return std::unexpected(std::move(result328.error()));
    world.m_playerStart = std::move(*result328);
  }

  auto result329 = world.m_serverConfig.getInt("playerStartInitialGenRadius");
  if (!result329)
    return std::unexpected(std::move(result329.error()));
  auto result330 = world.generateRegion(RectI::integral(RectF(world.m_playerStart, world.m_playerStart)).padded(std::move(*result329)));
  if (!result330)
    return std::unexpected(std::move(result330.error()));
  return {};
  };
  auto initialized = initialize();
  if (!initialized) {
    auto unloaded = m_worldStorage->unloadAll(true);
    if (!unloaded)
      return std::unexpected(std::move(unloaded.error()));
    return std::unexpected(WorldServerException("Exception encountered initializing world", initialized.error()));
  }
  return {};
}

std::expected<std::optional<unsigned>, StarException> WorldServer::shouldRunThisStep(UpdateTiming timingConfiguration) {
  auto index = static_cast<std::size_t>(timingConfiguration);
  auto& cached = m_updateTimings[index];
  if (!cached) {
    static constexpr std::array<std::string_view, static_cast<std::size_t>(UpdateTiming::Count)> names{
        "wiringUpdate", "liquidUpdate", "fallingBlocksUpdate", "blockDamageUpdate",
        "worldStorageTick", "worldStorageGenerate", "environmentUpdate"};
    auto configured = m_fidelityConfig.get(names[index]);
    if (!configured)
      return std::unexpected(std::move(configured.error()));
    auto timing = jsonToVec2U(*configured);
    if (!timing)
      return std::unexpected(std::move(timing.error()));
    cached = std::move(*timing);
  }
  if ((m_currentStep + (*cached)[1]) % (*cached)[0] == 0)
    return (*cached)[0];
  return {};
}

std::expected<std::optional<std::uint64_t>, StarException> WorldServer::processingLimit(ProcessingLimit limitConfiguration) {
  auto index = static_cast<std::size_t>(limitConfiguration);
  auto& cached = m_processingLimits[index];
  if (!cached) {
    static constexpr std::array<std::string_view, static_cast<std::size_t>(ProcessingLimit::Count)> names{
        "liquidEngineBackgroundProcessingLimit", "worldStorageGenerationLevelLimit"};
    auto configured = m_fidelityConfig.optUInt(names[index]);
    if (!configured)
      return std::unexpected(std::move(configured.error()));
    cached.emplace(std::move(*configured));
  }
  return *cached;
}

std::expected<TileModificationList, StarException> WorldServer::doApplyTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap, bool ignoreTileProtection, bool updateNeighbors) {
  auto result333 = this->root().materialDatabase();
  if (!result333)
    return std::unexpected(std::move(result333.error()));
  auto materialDatabase = std::move(*result333);

  TileModificationList unapplied = modificationList;
  std::size_t unappliedSize = unapplied.size();
  auto it = unapplied.begin();
  while (it != unapplied.end()) {
    auto current = it++;
    Vec2I pos;
    TileModification modification;
    std::tie(pos, modification) = *current;

    if (!ignoreTileProtection && isTileProtected(pos))
      continue;

    if (std::holds_alternative<PlaceMaterial>(modification)) {
      auto const& placeMaterial = std::get<PlaceMaterial>(modification);
      bool allowTileOverlap = placeMaterial.collisionOverride != TileCollisionOverride::None && collisionKindFromOverride(placeMaterial.collisionOverride) < CollisionKind::Dynamic;
      auto result336 = WorldImpl::canPlaceMaterial(this->root(), m_entityMap, pos, placeMaterial.layer, placeMaterial.material, allowEntityOverlap, allowTileOverlap, m_tileGetterFunction);
      if (!result336)
        return std::unexpected(std::move(result336.error()));
      if (!std::move(*result336))
        continue;

      auto tile = m_tileArray->modifyTile(pos);
      if (!tile)
        continue;

      if (placeMaterial.layer == TileLayer::Background) {
        tile->get().background = placeMaterial.material;
        if (placeMaterial.materialHueShift)
          tile->get().backgroundHueShift = *placeMaterial.materialHueShift; else {
          auto result337 = m_worldTemplate->biomeMaterialHueShift(tile->get().blockBiomeIndex, placeMaterial.material);
          if (!result337)
            return std::unexpected(std::move(result337.error()));
          tile->get().backgroundHueShift = std::move(*result337);
        }

        tile->get().backgroundColorVariant = DefaultMaterialColorVariant;
        if (tile->get().background == EmptyMaterialId) {
          // Remove the background mod if removing the background.
          tile->get().backgroundMod = NoModId;
        } else if (tile->get().liquid.source) {
          tile->get().liquid.pressure = 1.0f;
          tile->get().liquid.source = false;
        }
      } else {
        tile->get().foreground = placeMaterial.material;
        if (placeMaterial.materialHueShift)
          tile->get().foregroundHueShift = *placeMaterial.materialHueShift; else {
          auto result338 = m_worldTemplate->biomeMaterialHueShift(tile->get().blockBiomeIndex, placeMaterial.material);
          if (!result338)
            return std::unexpected(std::move(result338.error()));
          tile->get().foregroundHueShift = std::move(*result338);
        }

        tile->get().foregroundColorVariant = DefaultMaterialColorVariant;
        if (placeMaterial.collisionOverride != TileCollisionOverride::None)
          tile->get().updateCollision(collisionKindFromOverride(placeMaterial.collisionOverride)); else {
          auto result339 = materialDatabase->materialCollisionKind(tile->get().foreground);
          if (!result339)
            return std::unexpected(std::move(result339.error()));
          tile->get().updateCollision(std::move(*result339));
        }
        if (tile->get().foreground == EmptyMaterialId) {
          // Remove the foreground mod if removing the foreground.
          tile->get().foregroundMod = NoModId;
        }
        auto result340 = materialDatabase->blocksLiquidFlow(tile->get().foreground);
        if (!result340)
          return std::unexpected(std::move(result340.error()));
        if (std::move(*result340))
          tile->get().liquid = LiquidStore();
      }

      tile->get().dungeonId = ConstructionDungeonId;

      if (updateNeighbors) {
        auto result341 = checkEntityBreaks(RectF::withSize(Vec2F(pos), Vec2F(1, 1)));
        if (!result341)
          return std::unexpected(std::move(result341.error()));
        m_liquidEngine->visitLocation(pos);
        m_fallingBlocksAgent->visitLocation(pos);
      }

      if (placeMaterial.layer == TileLayer::Foreground) {
        auto result342 = dirtyCollision(RectI::withSize(pos, {1, 1}));
        if (!result342)
          return std::unexpected(std::move(result342.error()));
      }
      auto result343 = queueTileUpdates(pos);
      if (!result343)
        return std::unexpected(std::move(result343.error()));
    } else if (std::holds_alternative<PlaceMod>(modification)) {
      auto const& placeMod = std::get<PlaceMod>(modification);
      auto result344 = WorldImpl::canPlaceMod(this->root(), pos, placeMod.layer, placeMod.mod, m_tileGetterFunction);
      if (!result344)
        return std::unexpected(std::move(result344.error()));
      if (!std::move(*result344))
        continue;

      auto tile = m_tileArray->modifyTile(pos);
      if (!tile)
        continue;

      if (placeMod.layer == TileLayer::Background) {
        tile->get().backgroundMod = placeMod.mod;
        if (placeMod.modHueShift)
          tile->get().backgroundModHueShift = *placeMod.modHueShift; else {
          auto result345 = m_worldTemplate->biomeModHueShift(tile->get().blockBiomeIndex, placeMod.mod);
          if (!result345)
            return std::unexpected(std::move(result345.error()));
          tile->get().backgroundModHueShift = std::move(*result345);
        }
      } else {
        tile->get().foregroundMod = placeMod.mod;
        if (placeMod.modHueShift)
          tile->get().foregroundModHueShift = *placeMod.modHueShift; else {
          auto result346 = m_worldTemplate->biomeModHueShift(tile->get().blockBiomeIndex, placeMod.mod);
          if (!result346)
            return std::unexpected(std::move(result346.error()));
          tile->get().foregroundModHueShift = std::move(*result346);
        }
      }

      m_liquidEngine->visitLocation(pos);
      auto result347 = queueTileUpdates(pos);
      if (!result347)
        return std::unexpected(std::move(result347.error()));
    } else if (std::holds_alternative<PlaceMaterialColor>(modification)) {
      auto const& placeMaterialColor = std::get<PlaceMaterialColor>(modification);
      auto result348 = WorldImpl::canPlaceMaterialColorVariant(this->root(), pos, placeMaterialColor.layer, placeMaterialColor.color, m_tileGetterFunction);
      if (!result348)
        return std::unexpected(std::move(result348.error()));
      if (!std::move(*result348))
        continue;

      auto tile = m_tileArray->modifyTile(pos);
      if (!tile)
        continue;

      if (placeMaterialColor.layer == TileLayer::Background) {
        tile->get().backgroundHueShift = 0;
        auto result349 = materialDatabase->isMultiColor(tile->get().background);
        if (!result349)
          return std::unexpected(std::move(result349.error()));
        if (!std::move(*result349))
          continue;
        tile->get().backgroundColorVariant = placeMaterialColor.color;
      } else {
        tile->get().foregroundHueShift = 0;
        auto result350 = materialDatabase->isMultiColor(tile->get().foreground);
        if (!result350)
          return std::unexpected(std::move(result350.error()));
        if (!std::move(*result350))
          continue;
        tile->get().foregroundColorVariant = placeMaterialColor.color;
      }

      auto result351 = queueTileUpdates(pos);
      if (!result351)
        return std::unexpected(std::move(result351.error()));
    } else if (std::holds_alternative<PlaceLiquid>(modification)) {
      auto const& plpacket = std::get<PlaceLiquid>(modification);
      auto result352 = this->root().liquidsDatabase();
      if (!result352)
        return std::unexpected(std::move(result352.error()));
      if (!std::move(*result352)->isValidLiquidId(plpacket.liquid)) {
        auto result353 =
          this->root().logger().warn("WorldServer: rejected PlaceLiquid with invalid liquid id {} at ({}, {})",
                                     static_cast<int>(plpacket.liquid), pos[0], pos[1]);
        if (!result353)
          return std::unexpected(std::move(result353.error()));
        continue;
        // leave in `unapplied` so caller sees a failure
      }
      auto result354 = modifyLiquid(pos, plpacket.liquid, plpacket.liquidLevel, true);
      if (!result354)
        return std::unexpected(std::move(result354.error()));
      m_liquidEngine->visitLocation(pos);
      m_fallingBlocksAgent->visitLocation(pos);
    }

    it = unapplied.erase(current);

    if (it == unapplied.end()) {
      // If we are at the end, but have made progress by applying at least one
      // modification, then start over at the beginning and keep trying more
      // modifications until we can't make any more progress.
      if (unapplied.size() != unappliedSize) {
        unappliedSize = unapplied.size();
        it = unapplied.begin();
      }
    }
  }

  return unapplied;
}

std::expected<void, StarException> WorldServer::updateTileEntityTiles(std::shared_ptr<TileEntity> const& entity, bool removing, bool checkBreaks) {
  // This method of updating tile entity collision only works if each tile
  // entity's collision spaces are a subset of their normal spaces, and thus no
  // two tile entities can have collision spaces that overlap.
  // NOTE: Some entities may violate this; it's an odd thing to rely on policy
  // for and maybe we shouldn't allow tile entity configurations to specify
  // material spaces outside of their spaces

  auto& spaces = m_tileEntitySpaces[entity->entityId()];

  List<MaterialSpace> newMaterialSpaces = removing ? List<MaterialSpace>() : entity->materialSpaces();
  List<Vec2I> newRoots = removing || entity->ephemeral() ? List<Vec2I>() : entity->roots();

  if (!removing && spaces.materials == newMaterialSpaces && spaces.roots == newRoots) {
    return {};
  }

  auto result355 = this->root().materialDatabase();
  if (!result355)
    return std::unexpected(std::move(result355.error()));
  auto materialDatabase = std::move(*result355);

  // remove all old roots
  for (auto const& rootPos : spaces.roots) {
    if (auto tile = m_tileArray->modifyTile(rootPos + entity->tilePosition()))
      tile->get().rootSource = {};
  }

  // remove all old material spaces
  for (auto const& materialSpace : spaces.materials) {
    Vec2I pos = materialSpace.space + entity->tilePosition();

    auto tile = m_tileArray->modifyTile(pos);
    if (tile) {
      tile->get().rootSource = {};
      bool updatedTile = false;
      bool updatedCollision = false;
      if (isBiomeMaterial(materialSpace.material) || tile->get().foreground == materialSpace.material) {
        // if the world is old, the materialSpace's collision may still be in the tile
        tile->get().foreground = EmptyMaterialId;
        tile->get().foregroundMod = NoModId;
        updatedTile = true;
        updatedCollision = tile->get().updateCollision(CollisionKind::None);
      }
      if (tile->get().updateObjectCollision(CollisionKind::None))
        updatedTile = updatedCollision = true;
      if (updatedCollision) {
        m_liquidEngine->visitLocation(pos);
        m_fallingBlocksAgent->visitLocation(pos);
        auto result356 = dirtyCollision(RectI::withSize(pos, { 1, 1 }));
        if (!result356)
          return std::unexpected(std::move(result356.error()));
      }
      if (updatedTile) {
        auto result357 = queueTileUpdates(pos);
        if (!result357)
          return std::unexpected(std::move(result357.error()));
      }
    }
  }

  if (removing) {
    m_tileEntitySpaces.remove(entity->entityId());
  } else {
    // add new material spaces and update the known material spaces entry
    List<MaterialSpace> passedSpaces;
    for (auto const& materialSpace : newMaterialSpaces) {
      Vec2I pos = materialSpace.space + entity->tilePosition();

      bool updatedTile = false;
      bool updatedCollision = false;
      auto tile = m_tileArray->modifyTile(pos);
      if (tile) {
        if (tile->get().foreground == EmptyMaterialId) {
          tile->get().foreground = materialSpace.material;
          tile->get().foregroundMod = NoModId;
          updatedTile = true;
        }
        if (isRealMaterial(materialSpace.material))
          tile->get().rootSource = entity->tilePosition();
        passedSpaces.emplace_back(materialSpace);
        auto result358 = materialDatabase->materialCollisionKind(materialSpace.material);
        if (!result358)
          return std::unexpected(std::move(result358.error()));
        updatedTile |= updatedCollision = tile->get().updateObjectCollision(std::move(*result358));
      }
      if (updatedCollision) {
        m_liquidEngine->visitLocation(pos);
        m_fallingBlocksAgent->visitLocation(pos);
        auto result359 = dirtyCollision(RectI::withSize(pos, { 1, 1 }));
        if (!result359)
          return std::unexpected(std::move(result359.error()));
      }
      if (updatedTile) {
        auto result360 = queueTileUpdates(pos);
        if (!result360)
          return std::unexpected(std::move(result360.error()));
      }
    }
    spaces.materials = std::move(passedSpaces);

    // add new roots and update known roots entry
    for (auto const& rootPos : newRoots) {
      if (auto tile = m_tileArray->modifyTile(rootPos + entity->tilePosition()))
        tile->get().rootSource = entity->tilePosition();
    }
    spaces.roots = std::move(newRoots);
  }

  // check whether we've broken any other nearby entities
  if (checkBreaks) {
    auto result361 = checkEntityBreaks(entity->metaBoundBox().translated(entity->position()));
    if (!result361)
      return std::unexpected(std::move(result361.error()));
  }
  return {};
}

ConnectionId WorldServer::connection() const {
  return ServerConnectionId;
}

std::expected<bool, StarException> WorldServer::signalRegion(RectI const& region) {
  auto result362 = m_worldStorage->sectorsForRegion(region);
  if (!result362)
    return std::unexpected(std::move(result362.error()));
  auto sectors = std::move(*result362);
  if (m_generatingDungeon) {
    // When generating a dungeon, all sector activations should immediately
    // load whatever is available and make the sector active for writing, but
    // should trigger no generation (for world generation speed).
    for (auto const& sector : sectors) {
      auto result363 = m_worldStorage->loadSector(sector);
      if (!result363)
        return std::unexpected(std::move(result363.error()));
    }
  } else {
    for (auto const& sector : sectors) {
      auto result364 = m_worldStorage->queueSectorActivation(sector);
      if (!result364)
        return std::unexpected(std::move(result364.error()));
    }
  }
  for (auto const& sector : sectors) {
    if (!m_worldStorage->sectorActive(sector))
      return false;
  }
  return true;
}

std::expected<void, StarException> WorldServer::generateRegion(RectI const& region) {
  auto result365 = m_worldStorage->sectorsForRegion(region);
  if (!result365)
    return std::unexpected(std::move(result365.error()));
  for (auto sector : std::move(*result365)) {
    auto result366 = m_worldStorage->activateSector(sector);
    if (!result366)
      return std::unexpected(std::move(result366.error()));
  }
  return {};
}

std::expected<bool, StarException> WorldServer::regionActive(RectI const& region) {
  auto result367 = m_worldStorage->sectorsForRegion(region);
  if (!result367)
    return std::unexpected(std::move(result367.error()));
  for (auto const& sector : std::move(*result367)) {
    if (!m_worldStorage->sectorActive(sector))
      return false;
  }
  return true;
}

std::shared_ptr<WorldServer::ScriptComponent> WorldServer::scriptContext(String const& contextName) {
  if (auto context = m_scriptContexts.find_ref(contextName))
    return context->get();
  else
    return {};
}

std::expected<RpcPromise<Vec2I>, StarException> WorldServer::enqueuePlacement(List<BiomeItemDistribution> distributions, std::optional<DungeonId> id) {
  return m_worldStorage->enqueuePlacement(std::move(distributions), id);
}

std::expected<std::reference_wrapper<ServerTile const>, StarException> WorldServer::getServerTile(Vec2I const& position, bool withSignal) {
  if (withSignal) {
    auto result368 = signalRegion(RectI::withSize(position, {1, 1}));
    if (!result368)
      return std::unexpected(std::move(result368.error()));
  }

  return std::cref(m_tileArray->tile(position));
}

std::expected<std::optional<std::reference_wrapper<ServerTile>>, StarException> WorldServer::modifyServerTile(Vec2I const& position, bool withSignal) {
  if (withSignal) {
    auto result369 = signalRegion(RectI::withSize(position, {1, 1}));
    if (!result369)
      return std::unexpected(std::move(result369.error()));
  }

  auto tile = m_tileArray->modifyTile(position);
  if (tile) {
    auto result370 = dirtyCollision(RectI::withSize(position, {1, 1}));
    if (!result370)
      return std::unexpected(std::move(result370.error()));
    m_liquidEngine->visitLocation(position);
    auto result371 = queueTileUpdates(position);
    if (!result371)
      return std::unexpected(std::move(result371.error()));
  }
  return tile;
}

std::expected<EntityId, StarException> WorldServer::loadUniqueEntity(String const& uniqueId) {
  auto result372 = m_worldStorage->loadUniqueEntity(uniqueId);
  if (!result372)
    return std::unexpected(std::move(result372.error()));
  return std::move(*result372);
}

std::shared_ptr<WorldTemplate> WorldServer::worldTemplate() const {
  return m_worldTemplate;
}

std::shared_ptr<Sky> WorldServer::sky() const {
  return m_sky;
}

std::expected<void, StarException> WorldServer::modifyLiquid(Vec2I const& pos, LiquidId liquid, float quantity, bool additive) {
  if (liquid == EmptyLiquidId)
    quantity = 0;

  if (auto tile = m_tileArray->modifyTile(pos)) {
    auto result373 = this->root().materialDatabase();
    if (!result373)
      return std::unexpected(std::move(result373.error()));
    auto materialDatabase = std::move(*result373);
    bool condition375 = static_cast<bool>(tile->get().foreground == EmptyMaterialId);
    if (!condition375) {
      auto result374 = materialDatabase->materialCollisionKind(tile->get().foreground);
      if (!result374)
        return std::unexpected(std::move(result374.error()));
      condition375 = static_cast<bool>(!isSolidColliding(std::move(*result374)));
    }
    if (condition375) {
      if (additive && liquid == tile->get().liquid.liquid)
        quantity += tile->get().liquid.level;

      auto result376 = setLiquid(pos, liquid, quantity, tile->get().liquid.pressure);
      if (!result376)
        return std::unexpected(std::move(result376.error()));
      m_liquidEngine->visitLocation(pos);
    }
  }
  return {};
}

std::expected<void, StarException> WorldServer::setLiquid(Vec2I const& pos, LiquidId liquid, float level, float pressure) {
  if (auto tile = m_tileArray->modifyTile(pos)) {
    if (liquid == EmptyLiquidId)
      level = 0;

    if (auto netUpdate = tile->get().liquid.update(liquid, level, pressure)) {
      for (auto const& pair : m_clientInfo) {
        if (pair.second->activeSectors.contains(m_tileArray->sectorFor(pos)))
          pair.second->pendingLiquidUpdates.add(pos);
      }
    }
  }
  return {};
}

std::expected<List<ItemDescriptor>, StarException> WorldServer::destroyBlock(TileLayer layer, Vec2I const& pos, bool genItems, bool destroyModFirst, bool updateNeighbors) {
  auto result378 = this->root().materialDatabase();
  if (!result378)
    return std::unexpected(std::move(result378.error()));
  auto materialDatabase = std::move(*result378);

  auto tile = m_tileArray->modifyTile(pos);
  if (!tile)
    return {};

  List<ItemDescriptor> drops;

  if (layer == TileLayer::Background) {
    bool condition380 = static_cast<bool>(isRealMod(tile->get().backgroundMod) && destroyModFirst);
    if (condition380) {
      auto result379 = materialDatabase->modBreaksWithTile(tile->get().backgroundMod);
      if (!result379)
        return std::unexpected(std::move(result379.error()));
      condition380 = static_cast<bool>(!std::move(*result379));
    }
    if (condition380) {
      if (genItems) {
        auto result381 = materialDatabase->modItemDrop(tile->get().backgroundMod);
        if (!result381)
          return std::unexpected(std::move(result381.error()));
        if (auto drop = std::move(*result381))
          drops.push_back(drop);
      }
      tile->get().backgroundMod = NoModId;
    } else {
      if (genItems) {
        auto result382 = materialDatabase->materialItemDrop(tile->get().background);
        if (!result382)
          return std::unexpected(std::move(result382.error()));
        if (auto drop = std::move(*result382))
          drops.push_back(drop);
        auto result383 = materialDatabase->modItemDrop(tile->get().backgroundMod);
        if (!result383)
          return std::unexpected(std::move(result383.error()));
        if (auto drop = std::move(*result383))
          drops.push_back(drop);
      }
      tile->get().background = EmptyMaterialId;
      tile->get().backgroundColorVariant = DefaultMaterialColorVariant;
      tile->get().backgroundHueShift = 0;
      tile->get().backgroundMod = NoModId;
    }

    tile->get().backgroundDamage.reset();
  } else {
    bool condition385 = static_cast<bool>(isRealMod(tile->get().foregroundMod) && destroyModFirst);
    if (condition385) {
      auto result384 = materialDatabase->modBreaksWithTile(tile->get().foregroundMod);
      if (!result384)
        return std::unexpected(std::move(result384.error()));
      condition385 = static_cast<bool>(!std::move(*result384));
    }
    if (condition385) {
      if (genItems) {
        auto result386 = materialDatabase->modItemDrop(tile->get().foregroundMod);
        if (!result386)
          return std::unexpected(std::move(result386.error()));
        if (auto drop = std::move(*result386))
          drops.push_back(drop);
      }
      tile->get().foregroundMod = NoModId;
    } else {
      if (genItems) {
        auto result387 = materialDatabase->materialItemDrop(tile->get().foreground);
        if (!result387)
          return std::unexpected(std::move(result387.error()));
        if (auto drop = std::move(*result387))
          drops.push_back(drop);
        auto result388 = materialDatabase->modItemDrop(tile->get().foregroundMod);
        if (!result388)
          return std::unexpected(std::move(result388.error()));
        if (auto drop = std::move(*result388))
          drops.push_back(drop);
      }
      tile->get().foreground = EmptyMaterialId;
      tile->get().foregroundColorVariant = DefaultMaterialColorVariant;
      tile->get().foregroundHueShift = 0;
      tile->get().foregroundMod = NoModId;
      tile->get().updateCollision(CollisionKind::None);
      auto result389 = dirtyCollision(RectI::withSize(pos, {1, 1}));
      if (!result389)
        return std::unexpected(std::move(result389.error()));
    }

    tile->get().foregroundDamage.reset();
  }

  if (tile->get().background == EmptyMaterialId && tile->get().foreground == EmptyMaterialId) {
    auto result390 = m_worldTemplate->blockInfo(pos[0], pos[1]);
    if (!result390)
      return std::unexpected(std::move(result390.error()));
    auto blockInfo = std::move(*result390);
    if (blockInfo.oceanLiquid != EmptyLiquidId && !blockInfo.encloseLiquids && pos[1] < blockInfo.oceanLiquidLevel)
      tile->get().liquid = LiquidStore::endless(blockInfo.oceanLiquid, blockInfo.oceanLiquidLevel - pos[1]);
  }

  tile->get().dungeonId = DestroyedBlockDungeonId;

  if (updateNeighbors) {
    auto result391 = checkEntityBreaks(RectF::withSize(Vec2F(pos), Vec2F(1, 1)));
    if (!result391)
      return std::unexpected(std::move(result391.error()));
    m_liquidEngine->visitLocation(pos);
    m_fallingBlocksAgent->visitLocation(pos);
  }
  auto result392 = queueTileUpdates(pos);
  if (!result392)
    return std::unexpected(std::move(result392.error()));
  auto result393 = queueTileDamageUpdates(pos, layer);
  if (!result393)
    return std::unexpected(std::move(result393.error()));

  return drops;
}

std::expected<void, StarException> WorldServer::queueUpdatePackets(ConnectionId clientId, bool sendRemoteUpdates,
    List<RectI> const& monitoringRegions, SectorSnapshotCache& sectorSnapshots) {
  auto result394 = m_clientInfo.get(clientId);
  if (!result394)
    return std::unexpected(std::move(result394.error()));
  auto const& clientInfo = result394->get();
  clientInfo->outgoingPackets.push_back(std::make_shared<StepUpdatePacket>(m_currentTime));

  std::optional<String> weatherDomain;
  auto clientWindow = clientInfo->clientState->window();
  if (clientWindow.isEmpty()) {
    weatherDomain = clientInfo->weatherDomain;
  } else {
    auto result395 = m_worldTemplate->weatherLayerAt(Vec2I::floor(clientInfo->clientState->windowCenter()));
    if (!result395)
      return std::unexpected(std::move(result395.error()));
    if (auto layer = std::move(*result395)) {
      weatherDomain = layer->get().domain;
    }
  }
  bool weatherDomainChanged = weatherDomain != clientInfo->weatherDomain;
  if (weatherDomainChanged) {
    clientInfo->weatherDomain = weatherDomain;
    clientInfo->weatherNetVersion = 0;
  }

  auto result397 = shouldRunThisStep(UpdateTiming::Environment);
  if (!result397)
    return std::unexpected(std::move(result397.error()));
  if (std::move(*result397) || weatherDomainChanged) {
    ByteArray skyDelta;
    auto result398 = m_sky->writeUpdate(clientInfo->skyNetVersion, clientInfo->clientState->netCompatibilityRules());
    if (!result398)
      return std::unexpected(std::move(result398.error()));
    std::tie(skyDelta, clientInfo->skyNetVersion) = std::move(*result398);

    ByteArray weatherDelta;
    auto result399 = weatherForDomain(clientInfo->weatherDomain)->writeUpdate(clientInfo->weatherNetVersion, clientInfo->clientState->netCompatibilityRules());
    if (!result399)
      return std::unexpected(std::move(result399.error()));
    std::tie(weatherDelta, clientInfo->weatherNetVersion) = std::move(*result399);

    if (!skyDelta.empty() || !weatherDelta.empty())
      clientInfo->outgoingPackets.push_back(
        std::make_shared<EnvironmentUpdatePacket>(std::move(skyDelta), std::move(weatherDelta)));
  }

  for (auto sector : clientInfo->pendingSectors.values()) {
    if (!m_worldStorage->sectorActive(sector))
      continue;

    auto snapshot = sectorSnapshots.find(sector);
    if (snapshot == sectorSnapshots.end()) {
      auto tileArrayUpdate = std::make_shared<TileArrayUpdatePacket>();
      auto sectorTiles = m_tileArray->sectorRegion(sector);
      tileArrayUpdate->min = sectorTiles.min();
      tileArrayUpdate->array.resize(Vec2S(sectorTiles.width(), sectorTiles.height()));
      for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
        for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
          auto result400 = writeNetTile({x, y}, tileArrayUpdate->array(x - sectorTiles.xMin(), y - sectorTiles.yMin()));
          if (!result400)
            return std::unexpected(std::move(result400.error()));
        }
      }
      snapshot = sectorSnapshots.insert(sector, std::move(tileArrayUpdate)).first;
    }

    clientInfo->outgoingPackets.push_back(snapshot->second);
    clientInfo->pendingSectors.remove(sector);
  }

  for (auto pos : clientInfo->pendingTileUpdates) {
    auto tileUpdate = std::make_shared<TileUpdatePacket>();
    tileUpdate->position = pos;
    auto result401 = writeNetTile(pos, tileUpdate->tile);
    if (!result401)
      return std::unexpected(std::move(result401.error()));

    clientInfo->outgoingPackets.push_back(tileUpdate);
  }
  clientInfo->pendingTileUpdates.clear();

  for (auto pair : clientInfo->pendingTileDamageUpdates) {
    auto tile = m_tileArray->tile(pair.first);
    if (pair.second == TileLayer::Foreground)
      clientInfo->outgoingPackets.push_back(
        std::make_shared<TileDamageUpdatePacket>(pair.first, TileLayer::Foreground, tile.foregroundDamage));
    else {
      clientInfo->outgoingPackets.push_back(
        std::make_shared<TileDamageUpdatePacket>(pair.first, TileLayer::Background, tile.backgroundDamage));
    }
  }
  clientInfo->pendingTileDamageUpdates.clear();

  for (auto pos : clientInfo->pendingLiquidUpdates) {
    auto tile = m_tileArray->tile(pos);
    clientInfo->outgoingPackets.push_back(std::make_shared<TileLiquidUpdatePacket>(pos, tile.liquid.netUpdate()));
  }
  clientInfo->pendingLiquidUpdates.clear();

  HashSet<std::shared_ptr<Entity>> monitoredEntities;
  for (auto const& monitoredRegion : monitoringRegions) {
    auto collected = m_entityMap->forEachEntity(RectF(monitoredRegion),
        [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
          monitoredEntities.add(entity);
          return {};
        });
    if (!collected)
      return std::unexpected(std::move(collected.error()));
  }

  auto result404 = this->root().entityFactory();
  if (!result404)
    return std::unexpected(std::move(result404.error()));
  auto entityFactory = std::move(*result404);
  auto outOfMonitoredRegionsEntities = HashSet<EntityId>::from(clientInfo->clientSlavesNetVersion.keys());
  for (auto const& monitoredEntity : monitoredEntities)
    outOfMonitoredRegionsEntities.remove(monitoredEntity->entityId());
  for (auto entityId : outOfMonitoredRegionsEntities) {
    clientInfo->outgoingPackets.push_back(std::make_shared<EntityDestroyPacket>(entityId, ByteArray(), false));
    clientInfo->clientSlavesNetVersion.remove(entityId);
  }

  HashMap<ConnectionId, std::shared_ptr<EntityUpdateSetPacket>> updateSetPackets;
  if (sendRemoteUpdates || clientInfo->local) {
    auto result405 = updateSetPackets.add(ServerConnectionId, std::make_shared<EntityUpdateSetPacket>(ServerConnectionId));
    if (!result405)
      return std::unexpected(std::move(result405.error()));
  }
  for (auto const& p : m_clientInfo) {
    if (p.first != clientId && p.second->pendingForward) {
      auto result406 = updateSetPackets.add(p.first, std::make_shared<EntityUpdateSetPacket>(p.first));
      if (!result406)
        return std::unexpected(std::move(result406.error()));
    }
  }

  auto netRules = clientInfo->clientState->netCompatibilityRules();
  for (auto const& monitoredEntity : monitoredEntities) {
    EntityId entityId = monitoredEntity->entityId();
    ConnectionId connectionId = connectionForEntity(entityId);
    if (connectionId != clientId) {
      if (auto version = clientInfo->clientSlavesNetVersion.find_ref(entityId)) {
        if (auto updateSetPacket = updateSetPackets.value(connectionId)) {
          auto pair = std::make_pair(entityId, version->get());
          auto& cache = m_netStateCache[netRules];
          auto i = cache.find(pair);
          if (i == cache.end()) {
            auto result407 = monitoredEntity->writeNetState(version->get(), netRules);
            if (!result407)
              return std::unexpected(std::move(result407.error()));
            i = cache.insert(pair, std::move(*result407)).first;
          }
          const auto& netState = i->second;
          if (!netState.first.empty())
            updateSetPacket->deltas[entityId] = netState.first;
          version->get() = netState.second;
        }
      } else if (!monitoredEntity->masterOnly()) {
        // Client was unaware of this entity until now
        auto result408 = monitoredEntity->writeNetState(0, netRules);
        if (!result408)
          return std::unexpected(std::move(result408.error()));
        auto firstUpdate = std::move(*result408);
        auto result409 = clientInfo->clientSlavesNetVersion.add(entityId, firstUpdate.second);
        if (!result409)
          return std::unexpected(std::move(result409.error()));
        auto result410 = entityFactory->netStoreEntity(monitoredEntity, netRules);
        if (!result410)
          return std::unexpected(std::move(result410.error()));
        clientInfo->outgoingPackets.push_back(std::make_shared<EntityCreatePacket>(
          monitoredEntity->entityType(), std::move(*result410), std::move(firstUpdate.first), entityId));
      }
    }
  }

  for (auto&& p : updateSetPackets)
    clientInfo->outgoingPackets.push_back(std::move(p.second));
  return {};
}

std::expected<void, StarException> WorldServer::updateDamage(float dt) {
  auto result411 = m_damageManager->update(dt);
  if (!result411)
    return std::unexpected(std::move(result411.error()));

  // Do nothing with damage notifications at the moment.
  m_damageManager->pullPendingNotifications();

  for (auto const& remoteHitRequest : m_damageManager->pullRemoteHitRequests()) {
    auto destination = m_clientInfo.get(remoteHitRequest.destinationConnection());
    if (!destination)
      return std::unexpected(std::move(destination.error()));
    destination->get()->outgoingPackets.push_back(std::make_shared<HitRequestPacket>(remoteHitRequest));
  }

  for (auto const& remoteDamageRequest : m_damageManager->pullRemoteDamageRequests()) {
    auto destination = m_clientInfo.get(remoteDamageRequest.destinationConnection());
    if (!destination)
      return std::unexpected(std::move(destination.error()));
    destination->get()->outgoingPackets.push_back(std::make_shared<DamageRequestPacket>(remoteDamageRequest));
  }

  for (auto const& remoteDamageNotification : m_damageManager->pullRemoteDamageNotifications()) {
    for (auto const& pair : m_clientInfo) {
      if (pair.second->needsDamageNotification(remoteDamageNotification))
        pair.second->outgoingPackets.push_back(std::make_shared<DamageNotificationPacket>(remoteDamageNotification));
    }
  }
  return {};
}

std::expected<void, StarException> WorldServer::sync() {
  auto result412 = writeMetadata();
  if (!result412)
    return std::unexpected(std::move(result412.error()));
  auto result413 = m_worldStorage->sync();
  if (!result413)
    return std::unexpected(std::move(result413.error()));
  return {};
}

std::expected<void, StarException> WorldServer::unloadAll(bool force) {
  auto result414 = m_worldStorage->unloadAll(force);
  if (!result414)
    return std::unexpected(std::move(result414.error()));
  return {};
}

std::expected<WorldChunks, StarException> WorldServer::readChunks() {
  auto result415 = writeMetadata();
  if (!result415)
    return std::unexpected(std::move(result415.error()));
  return m_worldStorage->readChunks();
}

std::expected<void, StarException> WorldServer::updateDamagedBlocks(float dt) {
  auto result416 = this->root().materialDatabase();
  if (!result416)
    return std::unexpected(std::move(result416.error()));
  auto materialDatabase = std::move(*result416);

  for (auto pos : m_damagedBlocks.values()) {
    auto tile = m_tileArray->modifyTile(pos);
    if (!tile) {
      m_damagedBlocks.remove(pos);
      continue;
    }

    Vec2F dropPosition = centerOfTile(pos);
    if (tile->get().foregroundDamage.dead()) {
      bool harvested = tile->get().foregroundDamage.harvested();
      auto result417 = destroyBlock(TileLayer::Foreground, pos, harvested, !tileDamageIsPenetrating(tile->get().foregroundDamage.damageType()), true);
      if (!result417)
        return std::unexpected(std::move(result417.error()));
      for (auto const& drop : std::move(*result417)) {
        auto result418 = ItemDrop::createRandomizedDrop(this->root(), drop, dropPosition);
        if (!result418)
          return std::unexpected(std::move(result418.error()));
        auto result419 = addEntity(std::move(*result418), NullEntityId);
        if (!result419)
          return std::unexpected(std::move(result419.error()));
      }
    } else if (tile->get().foregroundDamage.damaged()) {
      if (isRealMaterial(tile->get().foreground)) {
        if (isRealMod(tile->get().foregroundMod)) {
          if (tileDamageIsPenetrating(tile->get().foregroundDamage.damageType())) {
            auto result420 = materialDatabase->materialDamageParameters(tile->get().foreground);
            if (!result420)
              return std::unexpected(std::move(result420.error()));
            tile->get().foregroundDamage.recover(std::move(*result420), dt);
          } else {
            auto result421 = materialDatabase->modBreaksWithTile(tile->get().foregroundMod);
            if (!result421)
              return std::unexpected(std::move(result421.error()));
            if (std::move(*result421)) {
              auto result422 = materialDatabase->modDamageParameters(tile->get().foregroundMod);
              if (!result422)
                return std::unexpected(std::move(result422.error()));
              auto result423 = materialDatabase->materialDamageParameters(tile->get().foreground);
              if (!result423)
                return std::unexpected(std::move(result423.error()));
              tile->get().foregroundDamage.recover(std::move(*result422).sum(std::move(*result423)), dt);
            } else {
              auto result424 = materialDatabase->modDamageParameters(tile->get().foregroundMod);
              if (!result424)
                return std::unexpected(std::move(result424.error()));
              tile->get().foregroundDamage.recover(std::move(*result424), dt);
            }
          }
        } else {
          auto result425 = materialDatabase->materialDamageParameters(tile->get().foreground);
          if (!result425)
            return std::unexpected(std::move(result425.error()));
          tile->get().foregroundDamage.recover(std::move(*result425), dt);
        }
      } else {
        tile->get().foregroundDamage.reset();
      }

      auto result426 = queueTileDamageUpdates(pos, TileLayer::Foreground);
      if (!result426)
        return std::unexpected(std::move(result426.error()));
    }

    if (tile->get().backgroundDamage.dead()) {
      bool harvested = tile->get().backgroundDamage.harvested();
      auto result427 = destroyBlock(TileLayer::Background, pos, harvested, !tileDamageIsPenetrating(tile->get().backgroundDamage.damageType()), true);
      if (!result427)
        return std::unexpected(std::move(result427.error()));
      for (auto const& drop : std::move(*result427)) {
        auto result428 = ItemDrop::createRandomizedDrop(this->root(), drop, dropPosition);
        if (!result428)
          return std::unexpected(std::move(result428.error()));
        auto result429 = addEntity(std::move(*result428), NullEntityId);
        if (!result429)
          return std::unexpected(std::move(result429.error()));
      }
    } else if (tile->get().backgroundDamage.damaged()) {
      if (isRealMaterial(tile->get().background)) {
        if (isRealMod(tile->get().backgroundMod)) {
          if (tileDamageIsPenetrating(tile->get().backgroundDamage.damageType())) {
            auto result430 = materialDatabase->materialDamageParameters(tile->get().background);
            if (!result430)
              return std::unexpected(std::move(result430.error()));
            tile->get().backgroundDamage.recover(std::move(*result430), dt);
          } else {
            auto result431 = materialDatabase->modBreaksWithTile(tile->get().backgroundMod);
            if (!result431)
              return std::unexpected(std::move(result431.error()));
            if (std::move(*result431)) {
              auto result432 = materialDatabase->modDamageParameters(tile->get().backgroundMod);
              if (!result432)
                return std::unexpected(std::move(result432.error()));
              auto result433 = materialDatabase->materialDamageParameters(tile->get().background);
              if (!result433)
                return std::unexpected(std::move(result433.error()));
              tile->get().backgroundDamage.recover(std::move(*result432).sum(std::move(*result433)), dt);
            } else {
              auto result434 = materialDatabase->modDamageParameters(tile->get().backgroundMod);
              if (!result434)
                return std::unexpected(std::move(result434.error()));
              tile->get().backgroundDamage.recover(std::move(*result434), dt);
            }
          }
        } else {
          auto result435 = materialDatabase->materialDamageParameters(tile->get().background);
          if (!result435)
            return std::unexpected(std::move(result435.error()));
          tile->get().backgroundDamage.recover(std::move(*result435), dt);
        }
      } else {
        tile->get().backgroundDamage.reset();
      }

      auto result436 = queueTileDamageUpdates(pos, TileLayer::Background);
      if (!result436)
        return std::unexpected(std::move(result436.error()));
    }

    if (tile->get().backgroundDamage.healthy() && tile->get().foregroundDamage.healthy())
      m_damagedBlocks.remove(pos);
  }
  return {};
}

std::expected<void, StarException> WorldServer::checkEntityBreaks(RectF const& rect) {
  auto result437 = m_entityMap->query<TileEntity>(rect);
  if (!result437)
    return std::unexpected(std::move(result437.error()));
  for (auto const& tileEntity : std::move(*result437)) {
    auto result438 = tileEntity->checkBroken();
    if (!result438)
      return std::unexpected(std::move(result438.error()));
  }
  return {};
}

std::expected<void, StarException> WorldServer::queueTileUpdates(Vec2I const& pos) {
  for (auto const& pair : m_clientInfo) {
    if (pair.second->activeSectors.contains(m_tileArray->sectorFor(pos)))
      pair.second->pendingTileUpdates.add(pos);
  }
  return {};
}

std::expected<void, StarException> WorldServer::queueTileDamageUpdates(Vec2I const& pos, TileLayer layer) {
  for (auto const& pair : m_clientInfo) {
    if (pair.second->activeSectors.contains(m_tileArray->sectorFor(pos)))
      pair.second->pendingTileDamageUpdates.add({pos, layer});
  }
  return {};
}

std::expected<void, StarException> WorldServer::writeNetTile(Vec2I const& pos, NetTile& netTile) const {
  auto const& tile = m_tileArray->tile(pos);
  netTile.foreground = tile.foreground;
  netTile.foregroundHueShift = tile.foregroundHueShift;
  netTile.foregroundColorVariant = tile.foregroundColorVariant;
  netTile.foregroundMod = tile.foregroundMod;
  netTile.foregroundModHueShift = tile.foregroundModHueShift;
  netTile.background = tile.background;
  netTile.backgroundHueShift = tile.backgroundHueShift;
  netTile.backgroundColorVariant = tile.backgroundColorVariant;
  netTile.backgroundMod = tile.backgroundMod;
  netTile.backgroundModHueShift = tile.backgroundModHueShift;
  netTile.liquid = tile.liquid.netUpdate();
  netTile.collision = tile.getCollision();
  netTile.blockBiomeIndex = tile.blockBiomeIndex;
  netTile.environmentBiomeIndex = tile.environmentBiomeIndex;
  netTile.dungeonId = tile.dungeonId;
  return {};
}

std::expected<void, StarException> WorldServer::dirtyCollision(RectI const& region) {
  auto dirtyRegion = region.padded(CollisionGenerator::BlockInfluenceRadius);
  for (int x = dirtyRegion.xMin(); x < dirtyRegion.xMax(); ++x) {
    for (int y = dirtyRegion.yMin(); y < dirtyRegion.yMax(); ++y) {
      if (auto tile = m_tileArray->modifyTile({x, y}))
        tile->get().collisionCacheDirty = true;
    }
  }
  return {};
}

std::expected<void, StarException> WorldServer::freshenCollision(RectI const& region) {
  RectI freshenRegion = RectI::null();
  for (int x = region.xMin(); x < region.xMax(); ++x) {
    for (int y = region.yMin(); y < region.yMax(); ++y) {
      if (auto tile = m_tileArray->modifyTile({x, y})) {
        if (tile->get().collisionCacheDirty)
          freshenRegion.combine(RectI(x, y, x + 1, y + 1));
      }
    }
  }

  if (!freshenRegion.isNull()) {
    for (int x = freshenRegion.xMin(); x < freshenRegion.xMax(); ++x) {
      for (int y = freshenRegion.yMin(); y < freshenRegion.yMax(); ++y) {
        if (auto tile = m_tileArray->modifyTile({x, y})) {
          tile->get().collisionCacheDirty = false;
          tile->get().collisionCache.clear();
        }
      }
    }

    auto result439 = m_collisionGenerator.getBlocks(freshenRegion);
    if (!result439)
      return std::unexpected(std::move(result439.error()));
    for (auto collisionBlock : std::move(*result439)) {
      if (auto tile = m_tileArray->modifyTile(collisionBlock.space)) {
        if (auto appended = tile->get().collisionCache.push_back(std::move(collisionBlock)); !appended)
          return std::unexpected(std::move(appended.error()));
      }
    }
  }
  return {};
}

std::expected<void, StarException> WorldServer::removeEntity(EntityId entityId, bool andDie) {
  auto entity = m_entityMap->entity(entityId);
  if (!entity) {
    return {};
  }

  if (auto tileEntity = as<TileEntity>(entity)) {
    auto result440 = updateTileEntityTiles(tileEntity, true, true);
    if (!result440)
      return std::unexpected(std::move(result440.error()));
  }

  if (andDie) {
    auto result441 = entity->destroy(std::nullopt);
    if (!result441)
      return std::unexpected(std::move(result441.error()));
  }

  for (auto const& pair : m_clientInfo) {
    auto& clientInfo = pair.second;
    if (auto version = clientInfo->clientSlavesNetVersion.try_extract_value(entity->entityId())) {
      auto netRules = clientInfo->clientState->netCompatibilityRules();
      auto result442 = entity->writeNetState(*version, netRules);
      if (!result442)
        return std::unexpected(std::move(result442.error()));
      ByteArray finalDelta = std::move(*result442).first;
      clientInfo->outgoingPackets.push_back(
        std::make_shared<EntityDestroyPacket>(entity->entityId(), std::move(finalDelta), andDie));
    }
  }

  m_entityMap->removeEntity(entityId);
  auto result443 = entity->uninit();
  if (!result443)
    return std::unexpected(std::move(result443.error()));
  return {};
}

std::expected<float, StarException> WorldServer::windLevel(Vec2F const& pos) const {
  auto result444 = weatherAt(pos);
  if (!result444)
    return std::unexpected(std::move(result444.error()));
  return WorldImpl::windLevel(m_tileArray, pos, std::move(*result444)->wind());
}

std::expected<float, StarException> WorldServer::lightLevel(Vec2F const& pos) const {
  auto result445 = WorldImpl::lightLevel(this->root(), m_tileArray, m_entityMap, m_geometry, m_worldTemplate, m_sky, m_lightIntensityCalculator, pos);
  if (!result445)
    return std::unexpected(std::move(result445.error()));
  return std::move(*result445);
}

std::expected<void, StarException> WorldServer::setDungeonBreathable(DungeonId dungeonId, std::optional<bool> breathable) {
  std::optional<bool> current = m_dungeonIdBreathable.maybe(dungeonId);
  if (breathable != current) {
    if (breathable)
      m_dungeonIdBreathable[dungeonId] = *breathable; else {
      m_dungeonIdBreathable.remove(dungeonId);
    }

    for (auto const& p : m_clientInfo)
      p.second->outgoingPackets.push_back(std::make_shared<SetDungeonBreathablePacket>(dungeonId, breathable));
  }
  return {};
}



std::expected<bool, StarException> WorldServer::breathable(Vec2F const& pos) const {
  auto result446 = WorldImpl::breathable(this->root(), *this, m_tileArray, m_dungeonIdBreathable, m_worldTemplate, pos);
  if (!result446)
    return std::unexpected(std::move(result446.error()));
  return std::move(*result446);
}

std::expected<float, StarException> WorldServer::threatLevel() const {
  return m_worldTemplate->threatLevel();
}

std::expected<StringList, StarException> WorldServer::environmentStatusEffects(Vec2F const& pos) const {
  return m_worldTemplate->environmentStatusEffects(std::floor(pos[0]), std::floor(pos[1]));
}

std::expected<StringList, StarException> WorldServer::weatherStatusEffects(Vec2F const& pos) const {
  auto result447 = weatherAt(pos);
  if (!result447)
    return std::unexpected(std::move(result447.error()));
  auto weather = std::move(*result447);
  if (!weather->statusEffects().empty()) {
    auto result448 = exposedToWeather(pos);
    if (!result448)
      return std::unexpected(std::move(result448.error()));
    if (std::move(*result448))
      return weather->statusEffects();
  }

  return {};
}

std::expected<bool, StarException> WorldServer::exposedToWeather(Vec2F const& pos) const {
  auto result449 = m_worldTemplate->weatherLayerAt(Vec2I::floor(pos));
  if (!result449)
    return std::unexpected(std::move(result449.error()));
  auto layer = std::move(*result449);
  std::optional<std::reference_wrapper<WorldTemplate::WeatherDomain const>> domain;
  if (layer && layer->get().domain) {
    auto result450 = m_worldTemplate->weatherDomain(*layer->get().domain);
    if (!result450)
      return std::unexpected(std::move(result450.error()));
    domain = std::move(*result450);
  }
  if (domain && pos[1] > domain->get().effectsMinHeight && liquidLevel(Vec2I::floor(pos)).liquid == EmptyLiquidId) {
    auto result451 = this->root().assets();
    if (!result451)
      return std::unexpected(std::move(result451.error()));
    auto assets = std::move(*result451);
    auto result452 = assets->json("/weather.config:weatherRayCheckDistance");
    if (!result452)
      return std::unexpected(std::move(result452.error()));
    auto result453 = std::move(*result452).toFloat();
    if (!result453)
      return std::unexpected(std::move(result453.error()));
    float weatherRayCheckDistance = std::move(*result453);
    auto result454 = assets->json("/weather.config:weatherRayCheckWindInfluence");
    if (!result454)
      return std::unexpected(std::move(result454.error()));
    auto result455 = std::move(*result454).toFloat();
    if (!result455)
      return std::unexpected(std::move(result455.error()));
    float weatherRayCheckWindInfluence = std::move(*result455);

    auto result456 = weatherAt(pos);
    if (!result456)
      return std::unexpected(std::move(result456.error()));
    auto offset = Vec2F(-std::move(*result456)->wind() * weatherRayCheckWindInfluence, weatherRayCheckDistance).normalized() * weatherRayCheckDistance;

    return !lineCollision({pos, pos + offset});
  }

  return false;
}

std::expected<std::optional<String>, StarException> WorldServer::activeWeather(Vec2F const& pos) const {
  auto result457 = weatherAt(pos);
  if (!result457)
    return std::unexpected(std::move(result457.error()));
  auto weather = std::move(*result457);
  return weather->activeWeather();
}

std::expected<bool, StarException> WorldServer::isUnderground(Vec2F const& pos) const {
  return m_worldTemplate->undergroundLevel() >= pos[1];
}

std::expected<bool, StarException> WorldServer::disableDeathDrops() const {
  if (m_worldTemplate->worldParameters())
    return m_worldTemplate->worldParameters()->disableDeathDrops;
  return false;
}

std::expected<List<PhysicsForceRegion>, StarException> WorldServer::forceRegions() const {
  return m_forceRegions;
}

std::expected<Json, StarException> WorldServer::getProperty(String const& propertyName, Json const& def) const {
  return m_worldProperties.value(propertyName, def);
}

std::expected<void, StarException> WorldServer::setProperty(String const& propertyName, Json const& property) {
  // Kae: Properties set to null (nil from Lua) should be erased instead of lingering around
  auto entry = m_worldProperties.find(propertyName);
  bool missing = entry == m_worldProperties.end();
  if (missing ? !property.isNull() : property != entry->second) {
    if (missing)
      m_worldProperties.emplace(propertyName, property); else if (property.isNull())
      m_worldProperties.erase(entry); else {
      entry->second = property;
    }
    for (auto const& pair : m_clientInfo)
      pair.second->outgoingPackets.push_back(
        std::make_shared<UpdateWorldPropertiesPacket>(JsonObject{{propertyName, property}}));
  }
  auto listener = m_worldPropertyListeners.find(propertyName);
  if (listener != m_worldPropertyListeners.end()) {
    if (!listener->second)
      return std::unexpected(WorldServerException("Empty world property listener"));
    auto result458 = listener->second(property);
    if (!result458)
      return std::unexpected(std::move(result458.error()));
  }
  return {};
}

void WorldServer::timer(float delay, WorldAction worldAction) { m_timers.push_back({delay, worldAction}); }

std::expected<void, StarException> WorldServer::startFlyingSky(bool enterHyperspace, bool startInWarp, Json settings) {
  auto result459 = m_sky->startFlying(enterHyperspace, startInWarp, std::move(settings));
  if (!result459)
    return std::unexpected(std::move(result459.error()));
  return {};
}

std::expected<void, StarException> WorldServer::stopFlyingSkyAt(SkyParameters const& destination) {
  auto result460 = m_sky->stopFlyingAt(destination);
  if (!result460)
    return std::unexpected(std::move(result460.error()));
  m_sky->setType(SkyType::Orbital);
  return {};
}

std::expected<void, StarException> WorldServer::setOrbitalSky(SkyParameters const& destination) {
  auto result461 = m_sky->jumpTo(destination);
  if (!result461)
    return std::unexpected(std::move(result461.error()));
  m_sky->setType(SkyType::Orbital);
  return {};
}

double WorldServer::epochTime() const {
  return m_sky->epochTime();
}

std::uint32_t WorldServer::day() const {
  return m_sky->day();
}

float WorldServer::dayLength() const {
  return m_sky->dayLength();
}

float WorldServer::timeOfDay() const {
  return m_sky->timeOfDay();
}

std::shared_ptr<LuaRoot> WorldServer::luaRoot() {
  return m_luaRoot;
}

std::expected<StringMap<LuaCallbacks>, StarException> WorldServer::luaThreadCallbacks() const {
  return m_luaThreadCallbacks;
}

std::expected<RpcPromise<Vec2F>, StarException> WorldServer::findUniqueEntity(String const& uniqueId) {
  auto result462 = m_worldStorage->findUniqueEntity(uniqueId);
  if (!result462)
    return std::unexpected(std::move(result462.error()));
  if (auto pos = std::move(*result462))
    return RpcPromise<Vec2F>::createFulfilled(*pos); else {
    return RpcPromise<Vec2F>::createFailed("Unknown entity");
  }
}

std::expected<RpcPromise<Json>, StarException> WorldServer::sendEntityMessage(std::variant<EntityId, String> const& entityId, String const& message, JsonArray const& args) {
  std::shared_ptr<Entity> entity;
  if (std::holds_alternative<EntityId>(entityId))
    entity = m_entityMap->entity(std::get<EntityId>(entityId)); else {
    auto result463 = loadUniqueEntity(std::get<String>(entityId));
    if (!result463)
      return std::unexpected(std::move(result463.error()));
    entity = m_entityMap->entity(std::move(*result463));
  }

  if (!entity) {
    return RpcPromise<Json>::createFailed("Unknown entity");
  } else if (entity->isMaster()) {
    auto result464 = entity->receiveMessage(ServerConnectionId, message, args);
    if (!result464)
      return std::unexpected(std::move(result464.error()));
    if (auto resp = std::move(*result464)) {
      if (std::holds_alternative<Json>(*resp))
        return RpcPromise<Json>::createFulfilled(std::get<Json>(*resp)); else {
        return std::get<RpcPromise<Json>>(*resp);
      }
    } else {
      return RpcPromise<Json>::createFailed("Message not handled by entity");
    }
  } else {
    auto pair = RpcPromise<Json>::createPair();
    auto destination = m_clientInfo.get(connectionForEntity(entity->entityId()));
    if (!destination)
      return std::unexpected(std::move(destination.error()));
    auto const& clientInfo = destination->get();
    Uuid uuid(this->root().random());
    m_entityMessageResponses[uuid] = {clientInfo->clientId, pair.second};
    clientInfo->outgoingPackets.push_back(
      std::make_shared<EntityMessagePacket>(entity->entityId(), message, args, uuid));
    return pair.first;
  }
}

void WorldServer::setPlayerStart(Vec2F const& startPosition, bool respawnInWorld) {
  m_playerStart = startPosition;
  m_respawnInWorld = respawnInWorld;
  m_adjustPlayerStart = false;
  for (auto const& pair : m_clientInfo)
    pair.second->outgoingPackets.push_back(std::make_shared<SetPlayerStartPacket>(m_playerStart, m_respawnInWorld));
}

std::expected<Vec2F, StarException> WorldServer::findPlayerStart(std::optional<Vec2F> firstTry) {
  auto result465 = m_serverConfig.get("playerStartRegionSize");
  if (!result465)
    return std::unexpected(std::move(result465.error()));
  auto result466 = jsonToVec2F(std::move(*result465));
  if (!result466)
    return std::unexpected(std::move(result466.error()));
  Vec2F spawnRectSize = std::move(*result466);
  auto result467 = m_serverConfig.getInt("playerStartRegionMaximumVerticalSearch");
  if (!result467)
    return std::unexpected(std::move(result467.error()));
  auto maximumVerticalSearch = std::move(*result467);
  auto result468 = m_serverConfig.getInt("playerStartRegionMaximumTries");
  if (!result468)
    return std::unexpected(std::move(result468.error()));
  auto maximumTries = std::move(*result468);

  static const Set<DungeonId> allowedSpawnDungeonIds = {NoDungeonId, SpawnDungeonId, ConstructionDungeonId, DestroyedBlockDungeonId};

  Vec2F pos;
  if (firstTry)
    pos = *firstTry; else {
    auto result469 = m_worldTemplate->findSensiblePlayerStart();
    if (!result469)
      return std::unexpected(std::move(result469.error()));
    pos = Vec2F(std::move(*result469).value_or(Vec2I(0, m_worldTemplate->surfaceLevel())));
  }

  CollisionSet collideWithAnything{CollisionKind::Null, CollisionKind::Block, CollisionKind::Dynamic, CollisionKind::Platform, CollisionKind::Slippery};
  for (int t = 0; t < maximumTries; ++t) {
    bool foundGround = false;
    // First go downward until we collide with terrain
    for (int i = 0; i < maximumVerticalSearch; ++i) {
      RectF spawnRect = RectF(pos[0] - spawnRectSize[0] / 2, pos[1], pos[0] + spawnRectSize[0] / 2, pos[1] + spawnRectSize[1]);
      auto result470 = generateRegion(RectI::integral(spawnRect));
      if (!result470)
        return std::unexpected(std::move(result470.error()));
      auto result471 = rectTileCollision(RectI::integral(spawnRect), collideWithAnything);
      if (!result471)
        return std::unexpected(std::move(result471.error()));
      if (std::move(*result471)) {
        foundGround = true;
        break;
      }
      --pos[1];
    }

    if (foundGround) {
      // Then go up until our spawn region is no longer in the terrain, but bail
      // out and try again if we can't signal the region or we are stuck in a
      // dungeon.
      for (int i = 0; i < maximumVerticalSearch; ++i) {
        if (m_tileArray->tile(Vec2I::floor(pos)).liquid.liquid != EmptyLiquidId)
          break;

        RectF spawnRect = RectF(pos[0] - spawnRectSize[0] / 2, pos[1], pos[0] + spawnRectSize[0] / 2, pos[1] + spawnRectSize[1]);

        auto result472 = generateRegion(RectI::integral(spawnRect));
        if (!result472)
          return std::unexpected(std::move(result472.error()));

        auto result473 = getServerTile(Vec2I::floor(pos), false);
        if (!result473)
          return std::unexpected(std::move(result473.error()));
        auto tileDungeonId = result473->get().dungeonId;

        if (!allowedSpawnDungeonIds.contains(tileDungeonId))
          break;

        auto result475 = rectTileCollision(RectI::integral(spawnRect), collideWithAnything);
        if (!result475)
          return std::unexpected(std::move(result475.error()));
        if (!std::move(*result475) && spawnRect.yMax() < m_geometry.height())
          return pos;

        ++pos[1];
      }
    }

    auto result476 = m_worldTemplate->findSensiblePlayerStart();
    if (!result476)
      return std::unexpected(std::move(result476.error()));
    pos = Vec2F(std::move(*result476).value_or(Vec2I(0, m_worldTemplate->surfaceLevel())));
  }

  return pos;
}

std::expected<Vec2F, StarException> WorldServer::findPlayerSpaceStart(float targetX) {
  auto result477 = m_serverConfig.get("playerSpaceStartRegionSize");
  if (!result477)
    return std::unexpected(std::move(result477.error()));
  auto result478 = jsonToVec2F(std::move(*result477));
  if (!result478)
    return std::unexpected(std::move(result478.error()));
  Vec2F testRectSize = std::move(*result478);
  auto result479 = m_serverConfig.getFloat("playerSpaceStartDistanceIncrement");
  if (!result479)
    return std::unexpected(std::move(result479.error()));
  auto distanceIncrement = std::move(*result479);
  auto result480 = m_serverConfig.getInt("playerSpaceStartMaximumTries");
  if (!result480)
    return std::unexpected(std::move(result480.error()));
  auto maximumTries = std::move(*result480);

  Vec2F basePos = Vec2F(targetX, m_geometry.height() * 0.5);

  CollisionSet collideWithAnything{CollisionKind::Null, CollisionKind::Block, CollisionKind::Dynamic, CollisionKind::Platform, CollisionKind::Slippery};
  for (int t = 0; t < maximumTries; ++t) {
    Vec2F testPos = m_geometry.limit(
      basePos + Vec2F::with_angle(this->root().random().randf() * 2 * std::numbers::pi, t * distanceIncrement));
    RectF testRect = RectF::withCenter(testPos, testRectSize);
    auto result481 = generateRegion(RectI::integral(testRect));
    if (!result481)
      return std::unexpected(std::move(result481.error()));
    auto result482 = rectTileCollision(RectI::integral(testRect), collideWithAnything);
    if (!result482)
      return std::unexpected(std::move(result482.error()));
    if (!std::move(*result482))
      return testPos;
  }

  return basePos;
}

std::expected<void, StarException> WorldServer::readMetadata() {
  auto result483 = this->root().dungeonDefinitions();
  if (!result483)
    return std::unexpected(std::move(result483.error()));
  auto dungeonDefinitions = std::move(*result483);
  auto result484 = this->root().versioningDatabase();
  if (!result484)
    return std::unexpected(std::move(result484.error()));
  auto versioningDatabase = std::move(*result484);

  auto result485 = m_worldStorage->worldMetadata();
  if (!result485)
    return std::unexpected(std::move(result485.error()));
  auto result486 = versioningDatabase->loadVersionedJson(std::move(*result485), "WorldMetadata");
  if (!result486)
    return std::unexpected(std::move(result486.error()));
  auto metadata = std::move(*result486);

  auto result487 = metadata.get("playerStart");
  if (!result487)
    return std::unexpected(std::move(result487.error()));
  auto result488 = jsonToVec2F(std::move(*result487));
  if (!result488)
    return std::unexpected(std::move(result488.error()));
  m_playerStart = std::move(*result488);
  auto result489 = metadata.getBool("respawnInWorld");
  if (!result489)
    return std::unexpected(std::move(result489.error()));
  m_respawnInWorld = std::move(*result489);
  auto result490 = metadata.getBool("adjustPlayerStart");
  if (!result490)
    return std::unexpected(std::move(result490.error()));
  m_adjustPlayerStart = std::move(*result490);
  auto result491 = metadata.get("worldTemplate");
  if (!result491)
    return std::unexpected(std::move(result491.error()));
  auto worldTemplate = WorldTemplate::create(this->root(), std::move(*result491));
  if (!worldTemplate)
    return std::unexpected(std::move(worldTemplate.error()));
  m_worldTemplate = std::move(*worldTemplate);
  auto result492 = metadata.get("centralStructure");
  if (!result492)
    return std::unexpected(std::move(result492.error()));
  auto result493 = WorldStructure::create(std::move(*result492));
  if (!result493)
    return std::unexpected(std::move(result493.error()));
  m_centralStructure = std::move(*result493);
  auto result494 = metadata.get("protectedDungeonIds");
  if (!result494)
    return std::unexpected(std::move(result494.error()));
  auto result495 = jsonToSet<StableHashSet<DungeonId>>(std::move(*result494), std::mem_fn(&Json::toUInt));
  if (!result495)
    return std::unexpected(std::move(result495.error()));
  m_protectedDungeonIds = std::move(*result495);
  auto result496 = metadata.getObject("worldProperties");
  if (!result496)
    return std::unexpected(std::move(result496.error()));
  m_worldProperties = std::move(*result496);
  auto result497 = metadata.getBool("spawningEnabled");
  if (!result497)
    return std::unexpected(std::move(result497.error()));
  m_spawner->setActive(std::move(*result497));

  m_dungeonIdGravity.clear();
  auto result498 = metadata.getArray("dungeonIdGravity");
  if (!result498)
    return std::unexpected(std::move(result498.error()));
  for (auto const& p : std::move(*result498)) {
    auto result499 = p.getInt(0);
    if (!result499)
      return std::unexpected(std::move(result499.error()));
    auto result500 = p.getFloat(1);
    if (!result500)
      return std::unexpected(std::move(result500.error()));
    m_dungeonIdGravity.set(std::move(*result499), std::move(*result500));
  }

  m_dungeonIdBreathable.clear();
  auto result501 = metadata.getArray("dungeonIdBreathable");
  if (!result501)
    return std::unexpected(std::move(result501.error()));
  for (auto const& p : std::move(*result501)) {
    auto result502 = p.getInt(0);
    if (!result502)
      return std::unexpected(std::move(result502.error()));
    auto result503 = p.getBool(1);
    if (!result503)
      return std::unexpected(std::move(result503.error()));
    m_dungeonIdBreathable.set(std::move(*result502), std::move(*result503));
  }
  return {};
}

std::expected<void, StarException> WorldServer::writeMetadata() {
  auto result504 = this->root().versioningDatabase();
  if (!result504)
    return std::unexpected(std::move(result504.error()));
  auto versioningDatabase = std::move(*result504);

  auto result505 = m_worldTemplate->store();
  if (!result505)
    return std::unexpected(std::move(result505.error()));
  auto result506 = m_centralStructure.store();
  if (!result506)
    return std::unexpected(std::move(result506.error()));
  auto result507 = jsonFromSet(m_protectedDungeonIds);
  if (!result507)
    return std::unexpected(std::move(result507.error()));
  Json metadata = JsonObject{
    {"playerStart", jsonFromVec2F(m_playerStart)},
    {"respawnInWorld", m_respawnInWorld},
    {"adjustPlayerStart", m_adjustPlayerStart},
    {"worldTemplate", std::move(*result505)},
    {"centralStructure", std::move(*result506)},
    {"protectedDungeonIds", std::move(*result507)},
    {"worldProperties", m_worldProperties},
    {"spawningEnabled", m_spawner->active()},
    {"dungeonIdGravity", [&gravityByDungeon = m_dungeonIdGravity]() {
        JsonArray gravity;
        gravity.reserve(gravityByDungeon.size());
        for (auto const& p : gravityByDungeon)
          gravity.push_back(JsonArray{p.first, p.second});
        return gravity;
      }()},
    {"dungeonIdBreathable", [&breathableByDungeon = m_dungeonIdBreathable]() {
        JsonArray breathable;
        breathable.reserve(breathableByDungeon.size());
        for (auto const& p : breathableByDungeon)
          breathable.push_back(JsonArray{p.first, p.second});
        return breathable;
      }()}
  };

  auto result508 = versioningDatabase->makeCurrentVersionedJson("WorldMetadata", metadata);
  if (!result508)
    return std::unexpected(std::move(result508.error()));
  auto result509 = m_worldStorage->setWorldMetadata(std::move(*result508));
  if (!result509)
    return std::unexpected(std::move(result509.error()));
  return {};
}

std::expected<bool, StarException> WorldServer::isVisibleToPlayer(RectF const& region) const {
  for (auto const& p : m_clientInfo) {
    auto result510 = p.second->monitoringRegions(m_entityMap);
    if (!result510)
      return std::unexpected(std::move(result510.error()));
    for (auto const& playerRegion : std::move(*result510)) {
      auto result511 = m_geometry.rectIntersectsRect(RectF(playerRegion), region);
      if (!result511)
        return std::unexpected(std::move(result511.error()));
      if (std::move(*result511))
        return true;
    }
  }
  return false;
}

WorldServer::ClientInfo::ClientInfo(ConnectionId clientId, InterpolationTracker const trackerInit)
  : clientId(clientId), subWorldId(MainClientWorldId), skyNetVersion(0), weatherNetVersion(0), pendingForward(false), started(false), local(false), admin(false), interpolationTracker(trackerInit) {}

std::expected<std::shared_ptr<WorldServer::ClientInfo>, StarException> WorldServer::ClientInfo::create(Root& root, ConnectionId clientId, InterpolationTracker const trackerInit) {
  auto clientState = WorldClientState::create(root);
  if (!clientState)
    return std::unexpected(std::move(clientState.error()));
  auto info = std::shared_ptr<ClientInfo>(new ClientInfo(clientId, trackerInit));
  info->clientState = std::move(*clientState);
  return info;
}

std::expected<List<RectI>, StarException> WorldServer::ClientInfo::monitoringRegions(std::shared_ptr<EntityMap> const& entityMap) const {
  return clientState->monitoringRegions([entityMap](EntityId entityId) -> std::optional<RectI> {
    if (auto entity = entityMap->entity(entityId))
      return RectI::integral(entity->metaBoundBox().translated(entity->position()));
    return {};
  });
}

bool WorldServer::ClientInfo::needsDamageNotification(RemoteDamageNotification const& rdn) const {
  if (clientId == connectionForEntity(rdn.sourceEntityId) || clientId == connectionForEntity(rdn.damageNotification.targetEntityId))
    return true;

  if (clientSlavesNetVersion.contains(rdn.damageNotification.targetEntityId))
    return true;

  if (clientState->window().contains(Vec2I::floor(rdn.damageNotification.position)))
    return true;

  return false;
}

std::expected<std::shared_ptr<InteractiveEntity>, StarException> WorldServer::getInteractiveInRange(Vec2F const& targetPosition, Vec2F const& sourcePosition, float maxRange) const {
  auto result512 = WorldImpl::getInteractiveInRange(m_geometry, m_entityMap, targetPosition, sourcePosition, maxRange);
  if (!result512)
    return std::unexpected(std::move(result512.error()));
  return std::move(*result512);
}

std::expected<bool, StarException> WorldServer::canReachEntity(Vec2F const& position, float radius, EntityId targetEntity, bool preferInteractive) const {
  auto result513 = WorldImpl::canReachEntity(m_geometry, m_tileArray, m_entityMap, position, radius, targetEntity, preferInteractive);
  if (!result513)
    return std::unexpected(std::move(result513.error()));
  return std::move(*result513);
}

std::expected<RpcPromise<InteractAction>, StarException> WorldServer::interact(InteractRequest const& request) {
  if (auto entity = as<InteractiveEntity>(m_entityMap->entity(request.targetId))) {
    auto action = entity->interact(request);
    if (!action)
      return std::unexpected(std::move(action.error()));
    return RpcPromise<InteractAction>::createFulfilled(std::move(*action));
  }
  return RpcPromise<InteractAction>::createFulfilled(InteractAction());
}

std::expected<void, StarException> WorldServer::setupForceRegions() {
  m_forceRegions.clear();

  if (!worldTemplate() || !worldTemplate()->worldParameters()) {
    return {};
  }

  auto forceRegionType = worldTemplate()->worldParameters()->worldEdgeForceRegions;

  if (forceRegionType == WorldEdgeForceRegionType::None) {
    return {};
  }

  bool addTopRegion = forceRegionType == WorldEdgeForceRegionType::Top || forceRegionType == WorldEdgeForceRegionType::TopAndBottom;
  bool addBottomRegion = forceRegionType == WorldEdgeForceRegionType::Bottom || forceRegionType == WorldEdgeForceRegionType::TopAndBottom;

  auto result514 = m_serverConfig.getFloat("worldEdgeForceRegionHeight");
  if (!result514)
    return std::unexpected(std::move(result514.error()));
  auto regionHeight = std::move(*result514);
  auto result515 = m_serverConfig.getFloat("worldEdgeForceRegionForce");
  if (!result515)
    return std::unexpected(std::move(result515.error()));
  auto regionForce = std::move(*result515);
  auto result516 = m_serverConfig.getFloat("worldEdgeForceRegionVelocity");
  if (!result516)
    return std::unexpected(std::move(result516.error()));
  auto regionVelocity = std::move(*result516);
  auto regionCategoryFilter = PhysicsCategoryFilter::whitelist({"player", "monster", "npc", "vehicle", "itemdrop"});
  auto worldSize = Vec2F(worldTemplate()->size());

  if (addTopRegion) {
    auto topForceRegion = GradientForceRegion();
    topForceRegion.region = PolyF({
        {0, worldSize[1] - regionHeight},
        {worldSize[0], worldSize[1] - regionHeight},
        (worldSize),
        {0, worldSize[1]}});
    topForceRegion.gradient = Line2F({0, worldSize[1]}, {0, worldSize[1] - regionHeight});
    topForceRegion.baseTargetVelocity = regionVelocity;
    topForceRegion.baseControlForce = regionForce;
    topForceRegion.categoryFilter = regionCategoryFilter;
    m_forceRegions.push_back(topForceRegion);
  }

  if (addBottomRegion) {
    auto bottomForceRegion = GradientForceRegion();
    bottomForceRegion.region = PolyF({
        {0, 0},
        {worldSize[0], 0},
        {worldSize[0], regionHeight},
        {0, regionHeight}});
    bottomForceRegion.gradient = Line2F({0, 0}, {0, regionHeight});
    bottomForceRegion.baseTargetVelocity = regionVelocity;
    bottomForceRegion.baseControlForce = regionForce;
    bottomForceRegion.categoryFilter = regionCategoryFilter;
    m_forceRegions.push_back(bottomForceRegion);
  }
  return {};
}

std::expected<void, StarException> WorldServer::setTemplate(std::shared_ptr<WorldTemplate> newTemplate) {
  m_worldTemplate = std::move(newTemplate);
  for (auto& client : clientIds()) {
    auto result517 = m_clientInfo.get(client);
    if (!result517)
      return std::unexpected(std::move(result517.error()));
    auto& info = result517->get();
    bool local = info->local;
    bool isAdmin = info->admin;
    auto netRules = info->clientState->netCompatibilityRules();
    auto subWorldId = info->subWorldId;
    SpawnTarget spawnTarget;
    if (auto player = clientPlayer(client)) {
      auto feetOffset = player->feetOffset();
      if (!feetOffset)
        return std::unexpected(std::move(feetOffset.error()));
      spawnTarget = SpawnTargetPosition(player->position() + *feetOffset);
    }
    auto result518 = removeClient(client);
    if (!result518)
      return std::unexpected(std::move(result518.error()));
    auto result519 = addClient(client, spawnTarget, local, isAdmin, netRules, subWorldId);
    if (!result519)
      return std::unexpected(std::move(result519.error()));
  }
  return {};
}

std::expected<void, StarException> WorldServer::wire(Vec2I const& outputPosition, std::size_t outputIndex, Vec2I const& inputPosition, std::size_t inputIndex) {
  WireConnection output = {.entityLocation = outputPosition, .nodeIndex = outputIndex};
  WireConnection input = {.entityLocation = inputPosition, .nodeIndex = inputIndex};
  auto result520 = atTile<WireEntity>(input.entityLocation);
  if (!result520)
    return std::unexpected(std::move(result520.error()));
  for (auto const& source : std::move(*result520)) {
    auto result521 = atTile<WireEntity>(output.entityLocation);
    if (!result521)
      return std::unexpected(std::move(result521.error()));
    for (auto const& target : std::move(*result521)) {
      auto result522 =
        source->addNodeConnection(WireNode{.direction = WireDirection::Input, .nodeIndex = input.nodeIndex}, output);
      if (!result522)
        return std::unexpected(std::move(result522.error()));
      auto result523 =
        target->addNodeConnection(WireNode{.direction = WireDirection::Output, .nodeIndex = output.nodeIndex}, input);
      if (!result523)
        return std::unexpected(std::move(result523.error()));
    }
  }
  return {};
}

}
