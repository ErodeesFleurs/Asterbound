import star.memory;
import star.json;
import star.xx_hash;
import star.json_extra;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.random;
import star.ttl_cache;
import star.casting;
import star.game_types;
import star.material_types;
import star.poly;
import star.list;
import star.rpc_promise;
import star.vector;
import star.rect;
import star.bi_map;
import star.a_star;
import star.data_stream;
import star.logging;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.math_common;
import star.parametric_function;
import star.multi_array_interpolator;
import star.multi_table;
import star.set;
import star.string;
import star.color;
import star.asset_path;
import star.image;
import star.format;
import star.version;
import star.audio;
import star.map;
import star.net_compatibility;
import std;
import star.worker_pool;
import star.optional;

import star.sector_array_2d;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.io_device;
import star.ref_ptr;
import star.listener;
import star.spatial_hash_2d;
import star.periodic_function;
import star.lua;
import star.periodic;
import star.byte_array;

using ZSTD_CCtx = struct ZSTD_CCtx_s;
using ZSTD_DCtx = struct ZSTD_DCtx_s;
using ZSTD_DStream = ZSTD_DCtx;
using ZSTD_CStream = ZSTD_CCtx;
import star.zstd_compression;

import star.lua_root;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.drawable;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;
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
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.warping;
import star.wiring;
import star.plant;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.chat_action;
import star.mixer;
import star.entity_rendering;
import star.lua_components;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;
import star.asset_source;
import star.assets;

import star.configuration;
import star.root;
import star.plant_database;
import star.biome_placement;
import star.spawn_type_database;
import star.biome;
import star.tile_damage;
import star.material_render_profile;
import star.liquid_types;
import star.blocks_along_line;
import star.world_template;
import star.liquids_database;
import star.material_database;
import star.entity_map;
import star.world_impl;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.ai_types;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.player_log;
import star.aggressive_entity;
import star.scripted_entity;
import star.item_drop;
import star.status_effect_entity;
import star.wire_entity;
import star.object;
import star.inspectable_entity;
import star.host_address;
import star.socket;
import star.tcp;
import star.p2p_networking_service;
import star.net_packet_socket;
import star.universe_connection;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;
import star.stored_functions;

import star.curve25519;
import star.universe_client_lua_bindings;
import star.celestial_lua_bindings;
import star.damage_database;
import star.effect_source_database;
import star.particle_database;
import star.object_database;


import star.client_context;
import star.particle_manager;
import star.entity_factory;

import star.sky;

import star.line;
import star.algorithm;
namespace Star {

const std::string SECRET_BROADCAST_PUBLIC_KEY = "SecretBroadcastPublicKey";
const std::string SECRET_BROADCAST_PREFIX = std::string("\0Broadcast\0", sizeof("\0Broadcast\0") - 1);

const float WorldClient::DropDist = 6.0f;
WorldClient::WorldClient(Root& root, UniverseClient& universe) : World(root), m_currentStep(0), m_currentTime(0), m_pendingLightReady(false), m_subWorldId(MainClientWorldId), m_worldDimLevel(0), m_latency(0), m_ambientSounds(root), m_musicTrack(root), m_altMusicTrack(root), m_universe(universe) {}

std::expected<std::shared_ptr<WorldClient>, StarException> WorldClient::create(Root& root, std::shared_ptr<Player> mainPlayer, std::shared_ptr<LuaRoot> luaRoot, UniverseClient& universe) {
  auto client = std::shared_ptr<WorldClient>(new WorldClient(root, universe));
  auto state = WorldClientState::create(root);
  if (!state)
    return std::unexpected(std::move(state.error()));
  client->m_clientState = std::move(*state);
  auto weather = ClientWeather::create(root);
  if (!weather)
    return std::unexpected(std::move(weather.error()));
  client->m_weather = std::move(*weather);
  auto initialized = client->initialize(std::move(mainPlayer), std::move(luaRoot), universe);
  if (!initialized) {
    auto cleared = client->close();
    if (!cleared)
      return std::unexpected(initialized.error().with_context(Star::format("Error rolling back client creation: {}", cleared.error().what())));
    return std::unexpected(std::move(initialized.error()));
  }
  return client;
}

std::expected<void, StarException> WorldClient::initialize(std::shared_ptr<Player> mainPlayer, std::shared_ptr<LuaRoot> luaRoot, UniverseClient& universe) {
  auto& root = this->root();
  // main client world, set up to render
  
  auto result1 = root.assets();
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto assets = (std::move(*result1));
  m_universe = universe;
  
  auto result2 = assets->json("/client.config");
  if (!result2)
    return std::unexpected(std::move(result2.error()));
  m_clientConfig = (std::move(*result2));
  m_expiryTimer = GameTimer(0);
  m_headless = false;
  m_subWorldId = MainClientWorldId;
  
  m_currentStep = 0;
  m_currentTime = 0;
  m_fullBright = false;
  m_asyncLighting = false;
  auto result3 = m_clientConfig.getFloat("worldDimTime");
  if (!result3)
    return std::unexpected(std::move(result3.error()));
  m_worldDimTimer = GameTimer((std::move(*result3)));
  m_worldDimTimer.setDone();
  m_worldDimLevel = 0.0f;
  
  auto result4 = m_clientConfig.getFloat("parallaxFadeTime");
  if (!result4)
    return std::unexpected(std::move(result4.error()));
  m_parallaxFadeTimer = GameTimer((std::move(*result4)));
  m_parallaxFadeTimer.setDone();
  
  m_collisionDebug = false;
  m_inWorld = false;

  m_luaRoot = std::move(luaRoot);
  auto result5 = LuaBindings::makeUniverseClientThreadCallbacks(this->root(), universe);
  if (!result5)
    return std::unexpected(std::move(result5.error()));
  m_luaThreadCallbacks["universe"] = (std::move(*result5));
  auto result6 = LuaBindings::makeCelestialCallbacks(this->root(), universe.celestialDatabase());
  if (!result6)
    return std::unexpected(std::move(result6.error()));
  m_luaThreadCallbacks["celestial"] = (std::move(*result6));

  m_mainPlayer = std::move(mainPlayer);

  centerClientWindowOnPlayer(Vec2U(100, 100));
  
  m_collisionGenerator.init([&self = *this](int x, int y)  -> std::expected<CollisionKind, StarException> {
    if (!self.m_predictedTiles.empty()) {
      if (auto p = self.m_predictedTiles.find_ref({x, y})) {
        if (p->get().collision) {
          return *p->get().collision;
        }
      }
    }
    return self.m_tileArray->tile({x, y}).collision;
  });
  
  auto result7 = m_clientConfig.getFloat("modifiedTilePredictionTimeout");
  if (!result7)
    return std::unexpected(std::move(result7.error()));
  m_modifiedTilePredictionTimeout =
    static_cast<int>(std::round((std::move(*result7)) / this->root().timing().timestep));

  m_latency = std::chrono::milliseconds::zero();
  
  auto result8 = m_clientConfig.getObject("blockDamageParticle");
  if (!result8)
    return std::unexpected(std::move(result8.error()));
  auto result9 = Particle::create(this->root(), (std::move(*result8)));
  if (!result9)
    return std::unexpected(std::move(result9.error()));
  m_blockDamageParticle = (std::move(*result9));
  auto result10 = m_clientConfig.getObject("blockDamageParticleVariance");
  if (!result10)
    return std::unexpected(std::move(result10.error()));
  auto result11 = Particle::create(this->root(), (std::move(*result10)));
  if (!result11)
    return std::unexpected(std::move(result11.error()));
  m_blockDamageParticleVariance = (std::move(*result11));
  auto result12 = m_clientConfig.getFloat("blockDamageParticleProbability");
  if (!result12)
    return std::unexpected(std::move(result12.error()));
  m_blockDamageParticleProbability = (std::move(*result12));
  
  auto result13 = m_clientConfig.getObject("blockDingParticle");
  if (!result13)
    return std::unexpected(std::move(result13.error()));
  auto result14 = Particle::create(this->root(), (std::move(*result13)));
  if (!result14)
    return std::unexpected(std::move(result14.error()));
  m_blockDingParticle = (std::move(*result14));
  auto result15 = m_clientConfig.getObject("blockDingParticleVariance");
  if (!result15)
    return std::unexpected(std::move(result15.error()));
  auto result16 = Particle::create(this->root(), (std::move(*result15)));
  if (!result16)
    return std::unexpected(std::move(result16.error()));
  m_blockDingParticleVariance = (std::move(*result16));
  auto result17 = m_clientConfig.getFloat("blockDingParticleProbability");
  if (!result17)
    return std::unexpected(std::move(result17.error()));
  m_blockDingParticleProbability = (std::move(*result17));
  
  auto result18 = m_clientConfig.getFloat("damageNotificationBatchDuration");
  if (!result18)
    return std::unexpected(std::move(result18.error()));
  m_damageNotificationBatchDuration = std::chrono::duration<float>(std::move(*result18));
  
  auto result19 = assets->json("/interface.config:ambientTrackFadeInTime");
  if (!result19)
    return std::unexpected(std::move(result19.error()));
  auto result20 = (std::move(*result19)).toFloat();
  if (!result20)
    return std::unexpected(std::move(result20.error()));
  m_ambientSounds.setTrackFadeInTime((std::move(*result20)));
  auto result21 = assets->json("/interface.config:ambientTrackSwitchGrace");
  if (!result21)
    return std::unexpected(std::move(result21.error()));
  auto result22 = (std::move(*result21)).toFloat();
  if (!result22)
    return std::unexpected(std::move(result22.error()));
  m_ambientSounds.setTrackSwitchGrace((std::move(*result22)));
  
  auto result23 = assets->json("/interface.config:musicTrackSwitchGrace");
  if (!result23)
    return std::unexpected(std::move(result23.error()));
  auto result24 = (std::move(*result23)).toFloat();
  if (!result24)
    return std::unexpected(std::move(result24.error()));
  m_musicTrack.setTrackSwitchGrace((std::move(*result24)));
  auto result25 = assets->json("/interface.config:musicTrackFadeInTime");
  if (!result25)
    return std::unexpected(std::move(result25.error()));
  auto result26 = (std::move(*result25)).toFloat();
  if (!result26)
    return std::unexpected(std::move(result26.error()));
  m_musicTrack.setTrackFadeInTime((std::move(*result26)));
  
  auto result27 = assets->json("/interface.config:musicTrackFadeInTime");
  if (!result27)
    return std::unexpected(std::move(result27.error()));
  auto result28 = (std::move(*result27)).toFloat();
  if (!result28)
    return std::unexpected(std::move(result28.error()));
  m_altMusicTrack.setTrackFadeInTime((std::move(*result28)));
  auto result29 = assets->json("/interface.config:musicTrackFadeInTime");
  if (!result29)
    return std::unexpected(std::move(result29.error()));
  auto result30 = (std::move(*result29)).toFloat();
  if (!result30)
    return std::unexpected(std::move(result30.error()));
  m_altMusicTrack.setTrackSwitchGrace((std::move(*result30)));
  m_altMusicTrack.setVolume(0, 0, 0);
  m_altMusicActive = false;
  
  m_pendingLightReady = false;
  
  auto result31 = clearWorld();
  if (!result31)
    return std::unexpected(std::move(result31.error()));
  return {};
}

std::expected<std::shared_ptr<WorldClient>, StarException> WorldClient::create(Root& root, ClientSubWorldId subWorldId, UniverseClient& universe) {
  auto client = std::shared_ptr<WorldClient>(new WorldClient(root, universe));
  auto state = WorldClientState::create(root);
  if (!state)
    return std::unexpected(std::move(state.error()));
  client->m_clientState = std::move(*state);
  auto weather = ClientWeather::create(root);
  if (!weather)
    return std::unexpected(std::move(weather.error()));
  client->m_weather = std::move(*weather);
  auto initialized = client->initialize(subWorldId, universe);
  if (!initialized) {
    auto cleared = client->close();
    if (!cleared)
      return std::unexpected(initialized.error().with_context(Star::format("Error rolling back client creation: {}", cleared.error().what())));
    return std::unexpected(std::move(initialized.error()));
  }
  return client;
}

std::expected<void, StarException> WorldClient::initialize(ClientSubWorldId subWorldId, UniverseClient& universe) {
  auto& root = this->root();
  // client subworld, doesn't render
  
  auto result32 = root.assets();
  if (!result32)
    return std::unexpected(std::move(result32.error()));
  auto assets = (std::move(*result32));
  m_universe = universe;
  
  auto result33 = assets->json("/client.config");
  if (!result33)
    return std::unexpected(std::move(result33.error()));
  m_clientConfig = (std::move(*result33));
  auto result34 = m_clientConfig.getFloat("idleSubWorldExpireTime");
  if (!result34)
    return std::unexpected(std::move(result34.error()));
  m_expiryTimer = GameTimer((std::move(*result34)));
  m_headless = true;
  
  m_currentStep = 0;
  m_currentTime = 0;
  
  m_inWorld = false;
  m_subWorldId = subWorldId;
  
  auto result35 = LuaRoot::create(this->root());
  if (!result35)
    return std::unexpected(std::move(result35.error()));
  m_luaRoot = (std::move(*result35));
  auto result36 = m_clientConfig.getFloat("luaGcPause");
  if (!result36)
    return std::unexpected(std::move(result36.error()));
  auto result37 = m_clientConfig.getFloat("luaGcStepMultiplier");
  if (!result37)
    return std::unexpected(std::move(result37.error()));
  m_luaRoot->tuneAutoGarbageCollection((std::move(*result36)), (std::move(*result37)));
  auto result38 = LuaBindings::makeUniverseClientThreadCallbacks(this->root(), universe);
  if (!result38)
    return std::unexpected(std::move(result38.error()));
  m_luaThreadCallbacks["universe"] = (std::move(*result38));
  auto result39 = LuaBindings::makeCelestialCallbacks(this->root(), universe.celestialDatabase());
  if (!result39)
    return std::unexpected(std::move(result39.error()));
  m_luaThreadCallbacks["celestial"] = (std::move(*result39));
  m_luaRoot->addCallbacks("universe", m_luaThreadCallbacks["universe"]);
  m_luaRoot->addCallbacks("celestial", m_luaThreadCallbacks["celestial"]);
  
  m_collisionGenerator.init([&self = *this](int x, int y)  -> std::expected<CollisionKind, StarException> {
    if (!self.m_predictedTiles.empty()) {
      if (auto p = self.m_predictedTiles.find_ref({x, y})) {
        if (p->get().collision) {
          return *p->get().collision;
        }
      }
    }
    return self.m_tileArray->tile({x, y}).collision;
  });
  
  auto result42 = m_clientConfig.getFloat("modifiedTilePredictionTimeout");
  if (!result42)
    return std::unexpected(std::move(result42.error()));
  m_modifiedTilePredictionTimeout =
    static_cast<int>(std::round((std::move(*result42)) / this->root().timing().timestep));

  m_latency = std::chrono::milliseconds::zero();
  
  auto result43 = m_clientConfig.getFloat("damageNotificationBatchDuration");
  if (!result43)
    return std::unexpected(std::move(result43.error()));
  m_damageNotificationBatchDuration = std::chrono::duration<float>(std::move(*result43));
  
  auto result44 = clearWorld();
  if (!result44)
    return std::unexpected(std::move(result44.error()));
  return {};
}

WorldClient::~WorldClient() {
  if (auto closed = close(); !closed)
    EmergencyLog::error("Error closing client world during destruction: {}", closed.error().what());
}

std::expected<void, StarException> WorldClient::close() {
  std::optional<StarException> error;
  if (m_lightingThreadStarted) {
    {
      std::lock_guard locker(m_lightingMutex);
      m_lightingStopSource.request_stop();
      m_lightingCond.notify_all();
    }
    auto stopped = m_lightingThread.finish();
    if (!stopped)
      error.emplace(std::move(stopped.error()));
    m_lightingThread = {};
    m_lightingThreadStarted = false;
  }
  auto cleared = clearWorld();
  if (!cleared && !error)
    error.emplace(std::move(cleared.error()));
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

bool WorldClient::inWorld() const {
  return m_inWorld;
}

bool WorldClient::isHeadless() const {
  return m_headless;
}

ClientSubWorldId WorldClient::subWorldId() const {
  return m_subWorldId;
}

bool WorldClient::inSpace() const {
  if (!m_sky)
    return false;
  return m_sky->inSpace();
}

bool WorldClient::flying() const {
  if (!m_sky)
    return false;
  return m_sky->flying();
}

bool WorldClient::mainPlayerDead() const {
  if (inWorld() && !m_headless)
    return !m_entityMap->get<Player>(m_mainPlayer->entityId());
  else
    return false;
}

std::expected<void, StarException> WorldClient::reviveMainPlayer() {
  if (inWorld() && !m_headless && mainPlayerDead()) {
    auto revived = m_mainPlayer->revive(m_playerStart);
    if (!revived)
      return std::unexpected(std::move(revived.error()));
    auto entityId = m_entityMap->reserveEntityId();
    if (!entityId)
      return std::unexpected(std::move(entityId.error()));
    auto initialized = m_mainPlayer->init(*this, *entityId, EntityMode::Master);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto added = m_entityMap->addEntity(m_mainPlayer);
    if (!added) {
      auto cleared = m_mainPlayer->uninit();
      if (!cleared)
        return std::unexpected(added.error().with_context(Star::format("Error uninitializing revived player: {}", cleared.error().what())));
    }
    return added;
  }
  return {};
}

bool WorldClient::respawnInWorld() const {
  return m_respawnInWorld;
}

void WorldClient::setRespawnInWorld(bool respawnInWorld) {
  m_respawnInWorld = respawnInWorld;
}

std::int64_t WorldClient::latency() const {
  return m_latency.count();
}

std::expected<void, StarException> WorldClient::resendEntity(EntityId entityId) {
  auto entity = m_entityMap->entity(entityId);
  if (!entity || !entity->isMaster() || !m_masterEntitiesNetVersion.contains(entity->entityId()))
    return {};
  auto fromVersion = m_masterEntitiesNetVersion.extract_value(entity->entityId());
  if (!fromVersion)
    return std::unexpected(std::move(fromVersion.error()));
  auto netRules = m_clientState->netCompatibilityRules();
  auto state = entity->writeNetState(*fromVersion, netRules);
  if (!state)
    return std::unexpected(std::move(state.error()));
  m_outgoingPackets.push_back(
    std::make_shared<EntityDestroyPacket>(entity->entityId(), std::move(state->first), false));
  return notifyEntityCreate(entity);
}

std::expected<void, StarException> WorldClient::removeEntity(EntityId entityId, bool andDie) {
  auto entity = m_entityMap->entity(entityId);
  if (!entity)
    return {};

  if (andDie) {
    ClientRenderCallback renderCallback(std::nullopt);
    NullRenderCallback hiddenEvents;
    RenderCallback& events = m_startupHiddenEntities.contains(entityId)
        ? static_cast<RenderCallback&>(hiddenEvents)
        : static_cast<RenderCallback&>(renderCallback);
    // Removal packets can arrive before the next tick's event pass. Drain
    // pending live events before destroy() contributes its final death effects.
    auto collected = entity->collectRenderEvents(events);
    if (!collected) {
      if (entity->isMaster())
        return std::unexpected(std::move(collected.error()));
      auto issue = Star::print_exception(collected.error(), true);
      if (!m_entityExceptionsLogged.insert(hash_of(issue)).second)
        issue = collected.error().what();
      auto logged = this->root().logger().error("WorldClient: Exception caught collecting entity {} presentation events during removal: {}", entityId, issue);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
    auto destroyed = entity->destroy(std::ref(events));
    if (!destroyed)
      return std::unexpected(std::move(destroyed.error()));

    std::optional<std::reference_wrapper<List<Directives> const>> directives;
    if (auto& worldTemplate = m_worldTemplate) {
      if (const auto& parameters = worldTemplate->worldParameters())
        if (auto& globalDirectives = m_worldTemplate->worldParameters()->globalDirectives)
          directives = std::cref(*globalDirectives);
    }
    if (directives) {
      int directiveIndex = static_cast<unsigned>(entity->entityId()) % directives->get().size();
      for (auto& p : renderCallback.particles)
        p.directives.append(directives->get()[directiveIndex]);
    }

    m_particles->addParticles(std::move(renderCallback.particles));
    m_samples.append_range(std::move(renderCallback.audios));
  }

  if (auto version = m_masterEntitiesNetVersion.try_extract_value(entity->entityId())) {
    auto netRules = m_clientState->netCompatibilityRules();
    auto state = entity->writeNetState(*version, netRules);
    if (!state)
      return std::unexpected(std::move(state.error()));
    ByteArray finalNetState = std::move(state->first);
    m_outgoingPackets.push_back(
      std::make_shared<EntityDestroyPacket>(entity->entityId(), std::move(finalNetState), andDie));
  }

  m_entityMap->removeEntity(entityId);
  m_entityPresentation.remove(entityId);
  m_hiddenEntityAudio.remove(entityId);
  return entity->uninit();
}

std::shared_ptr<WorldTemplate const> WorldClient::currentTemplate() const {
  return m_worldTemplate;
}

void WorldClient::setTemplate(Json newTemplate) {
  m_outgoingPackets.push_back(std::make_shared<UpdateWorldTemplatePacket>(std::move(newTemplate)));
}

std::shared_ptr<Sky const> WorldClient::currentSky() const {
  return m_sky;
}

void WorldClient::timer(float delay, WorldAction worldAction) {
  if (!inWorld())
    return;

  m_timers.push_back({delay, worldAction});
}

std::expected<std::shared_ptr<Entity>, StarException> WorldClient::closestEntity(Vec2F const& center, float radius, EntityFilter selector) const {
  if (!inWorld()) {
    return {};
  }
  
  return m_entityMap->closestEntity(center, radius, selector);
}

std::expected<void, StarException> WorldClient::forAllEntities(EntityCallback callback) const {
  return m_entityMap->forAllEntities(callback);
}

std::expected<void, StarException> WorldClient::forEachEntity(RectF const& boundBox, EntityCallback callback) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->forEachEntity(boundBox, callback);
}

std::expected<void, StarException> WorldClient::forEachEntityLine(Vec2F const& begin, Vec2F const& end, EntityCallback callback) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->forEachEntityLine(begin, end, callback);
}

std::expected<void, StarException> WorldClient::forEachEntityAtTile(Vec2I const& pos, EntityCallbackOf<TileEntity> callback) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->forEachEntityAtTile(pos, callback);
}

std::expected<std::shared_ptr<Entity>, StarException> WorldClient::findEntity(RectF const& boundBox, EntityFilter entityFilter) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->findEntity(boundBox, entityFilter);
}

std::expected<std::shared_ptr<Entity>, StarException> WorldClient::findEntityLine(Vec2F const& begin, Vec2F const& end, EntityFilter entityFilter) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->findEntityLine(begin, end, entityFilter);
}

std::expected<std::shared_ptr<Entity>, StarException> WorldClient::findEntityAtTile(Vec2I const& pos, EntityFilterOf<TileEntity> entityFilter) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->findEntityAtTile(pos, entityFilter);
}

std::expected<bool, StarException> WorldClient::tileIsOccupied(Vec2I const& pos, TileLayer layer, bool includeEphemeral, bool checkCollision) const {
  if (!inWorld()) {
    return false;
  }
  return WorldImpl::tileIsOccupied(m_tileArray, m_entityMap, pos, layer, includeEphemeral, checkCollision);
}

std::expected<CollisionKind, StarException> WorldClient::tileCollisionKind(Vec2I const& pos) const {
  if (!inWorld()) {
    return CollisionKind::Null;
  }
  return WorldImpl::tileCollisionKind(m_tileArray, m_entityMap, pos);
}

std::expected<void, StarException> WorldClient::forEachCollisionBlock(RectI const& region, std::function<std::expected<void, StarException>(CollisionBlock const&)> const& iterator) const {
  if (!inWorld())
    return {};
  auto freshened = const_cast<WorldClient&>(*this).freshenCollision(region);
  if (!freshened)
    return std::unexpected(std::move(freshened.error()));
  return m_tileArray->tileEach(region, [iterator](Vec2I const& pos, ClientTile const& tile) -> std::expected<void, StarException> {
    if (tile.getCollision() == CollisionKind::Null)
      return iterator(CollisionBlock::nullBlock(pos));
    if constexpr (Star::DebugEnabled)
      Star::star_assert_failed((!tile.collisionCacheDirty));
    for (auto const& block : tile.collisionCache) {
      auto visited = iterator(block);
      if (!visited)
        return std::unexpected(std::move(visited.error()));
    }
    return {};
  });
}

std::expected<bool, StarException> WorldClient::isTileConnectable(Vec2I const& pos, TileLayer layer, bool tilesOnly) const {
  if (!inWorld()) {
    return false;
  }
  
  return m_tileArray->tile(pos).isConnectable(layer, tilesOnly);
}

std::expected<bool, StarException> WorldClient::pointTileCollision(Vec2F const& point, CollisionSet const& collisionSet) const {
  if (!inWorld()) {
    return false;
  }
  
  return m_tileArray->tile(Vec2I(point.floor())).isColliding(collisionSet);
}

std::expected<bool, StarException> WorldClient::lineTileCollision(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet) const {
  if (!inWorld()) {
    return false;
  }
  
  return WorldImpl::lineTileCollision(m_geometry, m_tileArray, begin, end, collisionSet);
}

std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> WorldClient::lineTileCollisionPoint(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet) const {
  if (!inWorld()) {
    return {};
  }
  
  return WorldImpl::lineTileCollisionPoint(m_geometry, m_tileArray, begin, end, collisionSet);
}

std::expected<List<Vec2I>, StarException> WorldClient::collidingTilesAlongLine(
    Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet, int maxSize, bool includeEdges) const {
  if (!inWorld()) {
    return {};
  }
  
  return WorldImpl::collidingTilesAlongLine(m_geometry, m_tileArray, begin, end, collisionSet, maxSize, includeEdges);
}

std::expected<bool, StarException> WorldClient::rectTileCollision(RectI const& region, CollisionSet const& collisionSet) const {
  if (!inWorld()) {
    return false;
  }
  
  return WorldImpl::rectTileCollision(m_tileArray, region, collisionSet);
}

LiquidLevel WorldClient::liquidLevel(Vec2I const& pos) const {
  if (!inWorld())
    return {};
  return m_tileArray->tile(pos).liquid;
}

std::expected<LiquidLevel, StarException> WorldClient::liquidLevel(RectF const& region) const {
  if (!inWorld()) {
    return {};
  }
  return WorldImpl::liquidLevel(m_tileArray, region);
}

std::expected<TileModificationList, StarException> WorldClient::validTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap) const {
  if (!inWorld())
    return TileModificationList{};
  auto split = WorldImpl::splitTileModifications(this->root(), m_entityMap, modificationList, allowEntityOverlap, m_tileGetterFunction, [&self = *this](Vec2I pos, TileModification) -> std::expected<bool, StarException> {
    return !self.isTileProtected(pos);
  });
  if (!split)
    return std::unexpected(std::move(split.error()));
  return std::move(split->first);
}

std::expected<TileModificationList, StarException> WorldClient::applyTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap) {
  if (!inWorld()) {
    return TileModificationList{};
  }
  // thanks to new prediction: do each one by one so that previous modifications affect placeability
  TileModificationList success, failures, temp;
  std::reference_wrapper<TileModificationList const> list = modificationList;
  
  while (true) {
    bool yay = false;
    for (std::size_t i = 0; i != list.get().size(); ++i) {
      auto const& pair = list.get()[i];
      if (!isTileProtected(pair.first)) {
        auto result = WorldImpl::validateTileModification(this->root(), m_entityMap, pair.first, pair.second, allowEntityOverlap, m_tileGetterFunction);
        
        if (!result) {
          return std::unexpected(std::move(result.error()));
        }
        if (result->first) {
          auto predicted = informTilePrediction(pair.first, pair.second);
          if (!predicted) {
            return std::unexpected(std::move(predicted.error()));
          }
          success.push_back(pair);
          yay = true;
          continue;
        }
      }
      failures.push_back(pair);
    }
    if (yay) {
      list = std::cref(temp = std::move(failures));
      failures = {};
      continue;
    } else {
      break;
    }
  }
  
  if (!success.empty()) {
    m_outgoingPackets.push_back(std::make_shared<ModifyTileListPacket>(std::move(success), true));
  }
  
  return failures;
}

std::expected<TileModificationList, StarException> WorldClient::replaceTiles(TileModificationList const& modificationList, TileDamage const& tileDamage, bool applyDamage) {
  if (!inWorld())
    return TileModificationList{};
  
  // Tell client it can't send a replace packet
  auto netRules = m_clientState->netCompatibilityRules();
  if (netRules.isLegacy() || netRules.version() <= 3)
    return modificationList;
  
  TileModificationList success, failures;
  for (auto pair : modificationList) {
    bool valid = false;
    if (!isTileProtected(pair.first)) {
      auto replacement = WorldImpl::validateTileReplacement(this->root(), pair.second);
      if (!replacement)
        return std::unexpected(std::move(replacement.error()));
      valid = *replacement;
    }
    if (valid)
      success.push_back(pair);
    else
      failures.push_back(pair);
  }

  m_outgoingPackets.push_back(std::make_shared<ReplaceTileListPacket>(std::move(success), tileDamage, applyDamage));

  return failures;
}

std::expected<bool, StarException> WorldClient::damageWouldDestroy(Vec2I const& pos, TileLayer layer, TileDamage const& tileDamage) const {
  if (!inWorld()) {
    return false;
  }
  return WorldImpl::damageWouldDestroy(this->root(), m_tileArray, pos, layer, tileDamage);
}

std::expected<float, StarException> WorldClient::gravity(Vec2F const& pos) const {
  if (!inWorld()) {
    return 0.0f;
  }
  
  if (m_overrideGravity) {
    return *m_overrideGravity;
  }
  
  auto dungeonId = m_tileArray->tile(Vec2I::round(pos)).dungeonId;
  if (auto gravity = m_dungeonIdGravity.maybe(dungeonId)) {
    return *gravity;
  }
  return currentTemplate()->gravity();
}

std::expected<float, StarException> WorldClient::windLevel(Vec2F const& pos) const {
  if (!inWorld()) {
    return 0.0f;
  }
  
  auto layer = m_worldTemplate->weatherLayerAt(Vec2I::floor(pos));
  if (!layer) {
    return std::unexpected(std::move(layer.error()));
  }
  if (!*layer || (*layer)->get().domain != m_weatherDomain) {
    return WorldImpl::windLevel(m_tileArray, pos, 0.0f);
  }
  
  return WorldImpl::windLevel(m_tileArray, pos, m_weather->wind());
}

void WorldClient::setClientWindow(RectI window) {
  m_clientState->setWindow(window);
}

void WorldClient::centerClientWindowOnPlayer(Vec2U const& windowSize) {
  setClientWindow(RectI::withCenter(Vec2I::floor(m_mainPlayer->position()), Vec2I(windowSize)));
}

void WorldClient::centerClientWindowOnPlayer() {
  centerClientWindowOnPlayer(Vec2U(clientWindow().size()));
}

RectI WorldClient::clientWindow() const {
  return m_clientState->window();
}

WorldClientState& WorldClient::clientState() {
  return *m_clientState;
}

Vec2F WorldClient::renderPosition(EntityId entityId, PresentationContext presentation) const {
  auto entity = this->entity(entityId);
  if (!entity)
    return {};
  if (auto entry = m_entityPresentation.find_ref(entityId)) {
    auto const& state = entry->get();
    if (state.continuous && state.instance.lock() == entity && state.epoch == entity->presentationEpoch())
      return state.position.sample(m_geometry, presentation.interpolation());
  }
  return entity->position();
}

PresentationContext WorldClient::presentationFor(Entity const& entity, PresentationContext presentation) const {
  if (auto entry = m_entityPresentation.find_ref(entity.entityId())) {
    auto const& state = entry->get();
    if (state.continuous && state.instance.lock().get() == &entity && state.epoch == entity.presentationEpoch())
      return presentation;
  }
  // A replacement, teleport, or explicit reset has no previous display pose.
  return presentation.current();
}

void WorldClient::resetPresentation() {
  m_entityPresentation.clear();
  if (m_particles)
    m_particles->resetPresentation();
}

std::expected<void, StarException> WorldClient::render(WorldRenderData& renderData, unsigned bufferTiles, std::optional<std::reference_wrapper<TileDrawer>> tileDrawer, PresentationContext presentation) {
  if (m_headless) {
    return {};
  }
  if (m_lightingThreadStarted && m_lightingThread.isFinished()) {
    auto completed = m_lightingThread.finish();
    m_lightingThread = {};
    m_lightingThreadStarted = false;
    if (!completed) {
      return std::unexpected(std::move(completed.error()));
    }
  }
  if (!m_lightingThreadStarted && m_asyncLighting) {
    m_lightingStopSource = std::stop_source{};
    auto result45 = Thread::invoke("WorldClient::lightingMain", std::mem_fn(&WorldClient::lightingMain), std::ref(*this), m_lightingStopSource.get_token());
    if (!result45)
      return std::unexpected(std::move(result45.error()));
    m_lightingThread = (std::move(*result45));
    m_lightingThreadStarted = true;
  }
  
  renderData.prepareFrame();
  float const interpolation = presentation.interpolation();
  if (!inWorld()) {
    renderData.entityDrawables.clear();
    renderData.particles.clear();
    return {};
  }
  
  // If we're dimming the world, then that takes priority
  float dimRatio = m_worldDimTimer.percent();
  
  // Spends 80% of the time at pitch black with 10% ramp up and down
  
  m_worldDimColor = {};
  // always reset this to prevent persistent dimming from other sources
  if (dimRatio) {
    if (dimRatio <= 0.1f) {
      m_worldDimLevel = dimRatio / 0.1f;
    } else if (dimRatio >= 0.9f) {
      m_worldDimLevel = (1 - dimRatio) / (1 - 0.9f);
    } else {
      m_worldDimLevel = 1.0f;
    }
  }
  
  List<LightSource> renderLightSources;
  m_previewTiles.clear();
  
  renderData.geometry = m_geometry;
  m_particles->sampleParticles(interpolation, renderData.particles);
  
  ClientRenderCallback lightingRenderCallback(tileDrawer, presentation, m_presentationTimestep);
  auto result47 = m_entityMap->forAllEntities([&, &self = *this](std::shared_ptr<Entity> const& entity)  -> std::expected<void, StarException> {
      if (self.m_startupHiddenEntities.contains(entity->entityId())) {
        return {};
      }
      
      lightingRenderCallback.m_offset = self.m_geometry.diff(self.renderPosition(entity->entityId(), presentation), entity->position());
      lightingRenderCallback.m_presentation = self.presentationFor(*entity, presentation);
      auto result46 = entity->renderLightSources(lightingRenderCallback);
      if (!result46)
        return std::unexpected(std::move(result46.error()));
      return {};
    });
  if (!result47)
    return std::unexpected(std::move(result47.error()));
  
  renderLightSources = std::move(lightingRenderCallback.lightSources);
  
  RectI window = m_clientState->window();
  RectI tileRange = window.padded(bufferTiles);
  renderData.tileMinPosition = tileRange.min();
  
  if (!m_fullBright) {
    {
      std::lock_guard m_prepLocker(m_lightMapPrepMutex);
      m_pendingLights = std::move(renderLightSources);
      m_pendingParticleLights.clear();
      for (auto const& particle : renderData.particles)
        if (particle.light != Color::Clear)
          m_pendingParticleLights.push_back({particle.position, particle.light.toRgbF()});
      m_pendingLightRange = window.padded(1);
      m_pendingLightReady = true;
    }
    //Kae: Padded by one to fix light spread issues at the edges of the frame.
    
    if (m_asyncLighting) {
      m_lightingCond.notify_one();
    } else {
      auto result48 = lightingCalc();
      if (!result48)
        return std::unexpected(std::move(result48.error()));
    }
  }
  
  auto result49 = this->root().assets();
  if (!result49)
    return std::unexpected(std::move(result49.error()));
  auto result50 = (std::move(*result49))->json("/highlights.config:interactivePulseAmount");
  if (!result50)
    return std::unexpected(std::move(result50.error()));
  auto result51 = (std::move(*result50)).toFloat();
  if (!result51)
    return std::unexpected(std::move(result51.error()));
  float pulseAmount = (std::move(*result51));
  auto result52 = this->root().assets();
  if (!result52)
    return std::unexpected(std::move(result52.error()));
  auto result53 = (std::move(*result52))->json("/highlights.config:interactivePulseRate");
  if (!result53)
    return std::unexpected(std::move(result53.error()));
  auto result54 = (std::move(*result53)).toFloat();
  if (!result54)
    return std::unexpected(std::move(result54.error()));
  float pulseRate = (std::move(*result54));
  float pulseLevel = 1 - pulseAmount * 0.5 * (std::sin(2 * std::numbers::pi * pulseRate * std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)).time_since_epoch().count() / 1000.0) + 1);
  
  bool inspecting = m_mainPlayer->inspecting();
  
  EntityId playerAimInteractive = NullEntityId;
  {
    auto result59 = this->root().configuration();
    if (!result59)
      return std::unexpected(std::move(result59.error()));
    auto result60 = (std::move(*result59))->get("interactiveHighlight");
    if (!result60)
      return std::unexpected(std::move(result60.error()));
    auto result61 = (std::move(*result60)).toBool();
    if (!result61)
      return std::unexpected(std::move(result61.error()));
    if ((std::move(*result61))) {
      {
        auto result62 = m_mainPlayer->bestInteractionEntity(false);
        if (!result62)
          return std::unexpected(std::move(result62.error()));
        if (auto entity = (std::move(*result62))) {
          playerAimInteractive = entity->entityId();
        }
      }
    }
  }
  
  std::optional<std::reference_wrapper<List<Directives> const>> directives;
  if (auto& worldTemplate = m_worldTemplate) {
    if (const auto& parameters = worldTemplate->worldParameters()) {
      if (auto& globalDirectives = parameters->globalDirectives) {
        directives = std::cref(*globalDirectives);
      }
    }
  }
  std::size_t drawableIndex = 0;
  auto result72 = m_entityMap->forAllEntities([&, &self = *this](std::shared_ptr<Entity> const& entity)  -> std::expected<void, StarException> {
      if (self.m_startupHiddenEntities.contains(entity->entityId())) {
        return {};
      }
      
      if (drawableIndex == renderData.entityDrawables.size())
        renderData.entityDrawables.push_back({});
      auto& ed = renderData.entityDrawables[drawableIndex++];
      ClientRenderCallback renderCallback(tileDrawer, self.presentationFor(*entity, presentation), self.m_presentationTimestep,
          self.m_geometry.diff(self.renderPosition(entity->entityId(), presentation), entity->position()));
      renderCallback.drawableTarget = &ed.layers;
      
      auto rendered = entity->render(renderCallback);
      if (!rendered) {
        auto const& e = rendered.error();
        if (entity->isMaster()) {
          return std::unexpected(std::move(rendered.error()));
        } else {
          // this is THEIR problem!!
          auto issue = Star::print_exception(e, true);
          auto hash = hash_of(issue);
          if (!self.m_entityExceptionsLogged.contains(hash)) {
            self.m_entityExceptionsLogged.insert(hash);
          } else {
            issue = e.what();
          }
          
          if (!EntityTypeNames)
            return std::unexpected(EntityTypeNames.error());
          auto result63 = EntityTypeNames->get_right(entity->entityType());
          if (!result63)
            return std::unexpected(std::move(result63.error()));
          auto result64 = self.root().logger().error("WorldClient: Exception caught in {}::render ({}): {}", result63->get(), entity->entityId(), issue);
          if (!result64)
            return std::unexpected(std::move(result64.error()));
          auto toolUser = as<ToolUserEntity>(entity);
          String image = "/rendering/sprites/error.png";
          if (toolUser) {
            auto result65 = enumName<DirectionNames>(toolUser->facingDirection());
            if (!result65)
              return std::unexpected(std::move(result65.error()));
            image = Star::format("/rendering/sprites/error_{}.png", *result65);
          }
          Color color = Color::rgbf(
            0.8f + static_cast<float>(std::sin(self.m_currentTime * std::numbers::pi * 2.0)) * 0.2f, 0.0f, 0.0f);
          auto result66 = self.root().imageMetadataDatabase();
          if (!result66)
            return std::unexpected(std::move(result66.error()));
          auto result67 = Drawable::makeImage(*(std::move(*result66)), image, 1.0f / TilePixels, true, entity->position(), color);
          if (!result67)
            return std::unexpected(std::move(result67.error()));
          auto drawable = (std::move(*result67));
          drawable.fullbright = true;
          auto result68 = renderCallback.addDrawable(std::move(drawable), RenderLayerMiddleParticle);
          if (!result68)
            return std::unexpected(std::move(result68.error()));
        }
      }
      
      for (auto& p : ed.layers) {
        if (directives) {
          int directiveIndex = static_cast<unsigned>(entity->entityId()) % directives->get().size();
          for (auto& d : p.second) {
            if (d.isImage()) {
              auto result69 = self.root().imageMetadataDatabase();
              if (!result69)
                return std::unexpected(std::move(result69.error()));
              auto result70 = d.imagePart().addDirectives(*(std::move(*result69)), directives->get()[directiveIndex], true);
              if (!result70)
                return std::unexpected(std::move(result70.error()));
            }
          }
        }
      }
      
      if (self.m_interactiveHighlightMode || (!inspecting && entity->entityId() == playerAimInteractive)) {
        if (auto interactive = as<InteractiveEntity>(entity)) {
          if (interactive->isInteractive()) {
            ed.highlightEffect.type = EntityHighlightEffectType::Interactive;
            ed.highlightEffect.level = pulseLevel;
          }
        }
      } else if (inspecting) {
        if (auto inspectable = as<InspectableEntity>(entity)) {
          auto result71 = self.m_mainPlayer->inspectionHighlight(inspectable);
          if (!result71)
            return std::unexpected(std::move(result71.error()));
          ed.highlightEffect = (std::move(*result71));
          ed.highlightEffect.level *= self.m_inspectionFlickerMultiplier;
        }
      }

      self.m_previewTiles.append_range(std::move(renderCallback.previewTiles));
      renderData.overheadBars.append_range(std::move(renderCallback.overheadBars));
      return {};
    }, [](std::shared_ptr<Entity> const& a, std::shared_ptr<Entity> const& b) {
      return a->entityId() < b->entityId();
    });
  if (!result72)
    return std::unexpected(std::move(result72.error()));
  renderData.entityDrawables.resize(drawableIndex);
  
  auto result73 = this->root().workers();
  if (!result73)
    return std::unexpected(std::move(result73.error()));
  auto result74 = m_tileArray->tileEachTo(result73->get(), renderData.tiles, tileRange, [](RenderTile& renderTile, Vec2I const&, ClientTile const& clientTile)  -> std::expected<void, StarException> {
      renderTile.foreground = clientTile.foreground;
      renderTile.foregroundMod = clientTile.foregroundMod;
      
      renderTile.background = clientTile.background;
      renderTile.backgroundMod = clientTile.backgroundMod;
      
      renderTile.foregroundHueShift = clientTile.foregroundHueShift;
      renderTile.foregroundModHueShift = clientTile.foregroundModHueShift;
      renderTile.foregroundColorVariant = clientTile.foregroundColorVariant;
      renderTile.foregroundDamageType = clientTile.foregroundDamage.damageType();
      renderTile.foregroundDamageLevel = floatToByte(clientTile.foregroundDamage.damageEffectPercentage());
      
      renderTile.backgroundHueShift = clientTile.backgroundHueShift;
      renderTile.backgroundModHueShift = clientTile.backgroundModHueShift;
      renderTile.backgroundColorVariant = clientTile.backgroundColorVariant;
      renderTile.backgroundDamageType = clientTile.backgroundDamage.damageType();
      renderTile.backgroundDamageLevel = floatToByte(clientTile.backgroundDamage.damageEffectPercentage());
      
      renderTile.liquidId = clientTile.liquid.liquid;
      renderTile.liquidLevel = floatToByte(clientTile.liquid.level);
      return {};
    });
  if (!result74)
    return std::unexpected(std::move(result74.error()));

  for (auto&& pair : m_predictedTiles) {
    Vec2I tileArrayPos(m_geometry.pdiff(pair.first[0], renderData.tileMinPosition[0]), pair.first[1] - renderData.tileMinPosition[1]);
    if (tileArrayPos[0] >= 0 && tileArrayPos[0] < static_cast<int>(renderData.tiles.size(0)) && tileArrayPos[1] >= 0
        && tileArrayPos[1] < static_cast<int>(renderData.tiles.size(1))) {
      RenderTile& renderTile = renderData.tiles(tileArrayPos[0], tileArrayPos[1]);
      PredictedTile& p = pair.second;
      if (p.liquid) {
        auto& liquid = *p.liquid;
        if (liquid.liquid == renderTile.liquidId) {
          std::uint8_t added = floatToByte(liquid.level, true);
          renderTile.liquidLevel = (renderTile.liquidLevel > 255 - added) ? 255 : renderTile.liquidLevel + added;
        } else {
          renderTile.liquidId = liquid.liquid;
          renderTile.liquidLevel = floatToByte(liquid.level, true);
        }
      }
      
      pair.second.apply(renderTile);
    }
  }

  for (auto const& previewTile : m_previewTiles) {
    Vec2I tileArrayPos(m_geometry.pdiff(previewTile.position[0], renderData.tileMinPosition[0]), previewTile.position[1] - renderData.tileMinPosition[1]);
    if (tileArrayPos[0] >= 0 && tileArrayPos[0] < static_cast<int>(renderData.tiles.size(0)) && tileArrayPos[1] >= 0
        && tileArrayPos[1] < static_cast<int>(renderData.tiles.size(1))) {
      RenderTile& renderTile = renderData.tiles(tileArrayPos[0], tileArrayPos[1]);
      
      auto material = previewTile.matId;
      auto hueShift = previewTile.hueShift;
      auto colorVariant = previewTile.colorVariant;
      if (previewTile.updateMatId) {
        if (previewTile.foreground) {
          renderTile.foreground = material;
          renderTile.foregroundHueShift = hueShift;
          renderTile.foregroundColorVariant = colorVariant;
        } else {
          renderTile.background = material;
          renderTile.backgroundHueShift = hueShift;
          renderTile.backgroundColorVariant = colorVariant;
        }
      }
      
      if (previewTile.liqId != EmptyLiquidId) {
        renderTile.liquidId = previewTile.liqId;
        renderTile.liquidLevel = 255;
      }
    }
  }
  
  this->root().logMap().set("client_render_particle_count", renderData.particles.size());
  
  auto result75 = m_sky->renderData();
  if (!result75)
    return std::unexpected(std::move(result75.error()));
  renderData.skyRenderData = (std::move(*result75));
  
  auto result76 = mainEnvironmentBiome();
  if (!result76)
    return std::unexpected(std::move(result76.error()));
  auto environmentBiome = (std::move(*result76));
  
  
  if (environmentBiome) {
    setParallax(environmentBiome->parallax);
  }
  
  if (m_currentParallax) {
    if (m_parallaxFadeTimer.ready()) {
      renderData.parallaxLayers.append_range(m_currentParallax->layers());
    } else {
      for (auto layer : m_currentParallax->layers()) {
        layer.alpha = std::min(1.0f, m_parallaxFadeTimer.percent() * 2);
        renderData.parallaxLayers.push_back(layer);
      }
    }
  }
  
  if (m_nextParallax) {
    for (auto layer : m_nextParallax->layers()) {
      layer.alpha = std::min(1.0f, (1.0f - m_parallaxFadeTimer.percent()) * 2);
      renderData.parallaxLayers.push_back(layer);
    }
  }
  
  auto result77 = currentWeatherDomain();
  if (!result77)
    return std::unexpected(std::move(result77.error()));
  auto weatherParallaxAsset = (std::move(*result77)) == m_weatherDomain ? m_weather->weatherParallax() : std::optional<String>();
  if (weatherParallaxAsset != m_weatherParallaxAsset || environmentBiome != m_weatherParallaxBiome) {
    m_weatherParallaxAsset = weatherParallaxAsset;
    m_weatherParallaxBiome = environmentBiome;
    m_weatherParallax.reset();
    
    {
      bool condition80 = static_cast<bool>(weatherParallaxAsset);
      if (condition80) {
        auto result78 = this->root().assets();
        if (!result78)
          return std::unexpected(std::move(result78.error()));
        auto result79 = (std::move(*result78))->assetExists(*weatherParallaxAsset);
        if (!result79)
          return std::unexpected(std::move(result79.error()));
        condition80 = static_cast<bool>((std::move(*result79)));
      }
      if (condition80) {
        if (environmentBiome && environmentBiome->parallax) {
          auto result81 = environmentBiome->parallax->createOverlay(*weatherParallaxAsset);
          if (!result81)
            return std::unexpected(std::move(result81.error()));
          m_weatherParallax = (std::move(*result81));
        } else {
          float hueShift = environmentBiome ? environmentBiome->hueShift : 0.0f;
          std::optional<TreeVariant> treeVariant;
          if (environmentBiome) {
            treeVariant = environmentBiome->surfacePlaceables.firstTreeType();
          }
          
          auto result82 = Parallax::create(this->root(), *weatherParallaxAsset,
                      m_worldTemplate->worldSeed(), m_worldTemplate->surfaceLevel(), hueShift, std::move(treeVariant));
          if (!result82)
            return std::unexpected(std::move(result82.error()));
          m_weatherParallax = (std::move(*result82));
        }
        auto result83 = m_weatherParallax->fadeToSkyColor(m_sky->mainSkyColor());
        if (!result83)
          return std::unexpected(std::move(result83.error()));
      }
    }
  }
  
  if (m_weatherParallax) {
    for (auto layer : m_weatherParallax->layers()) {
      layer.alpha *= m_weather->weatherIntensity();
      renderData.parallaxLayers.push_back(std::move(layer));
    }
  }
  
  
  auto result85 = this->root().functionDatabase();
  if (!result85)
    return std::unexpected(std::move(result85.error()));
  auto functionDatabase = (std::move(*result85));
  for (auto& layer : renderData.parallaxLayers) {
    if (layer.followsWind || layer.windSpeedMultiplier) {
      double windTime;
      float horizontalSpeed = layer.speed[0];
      if (layer.followsWind) {
        // Parallax texture offsets move the image opposite the offset sign,
        // so negate the magnitude to make positive wind move imagery right.
        horizontalSpeed = -std::abs(horizontalSpeed);
        windTime = layer.windSpeedMultiplier
            ? m_parallaxSignedWindTime * *layer.windSpeedMultiplier
            : m_parallaxWindDirectionTime;
      } else {
        windTime = m_parallaxWindMagnitudeTime * *layer.windSpeedMultiplier;
      }
      
      layer.parallaxOffset[0] += horizontalSpeed * windTime / m_sky->dayLength();
      layer.speed[0] = 0.0f;
    }
    
    if (!layer.timeOfDayCorrelation.empty()) {
      auto result86 = functionDatabase->function(layer.timeOfDayCorrelation);
      if (!result86)
        return std::unexpected(std::move(result86.error()));
      auto result87 = (std::move(*result86))->evaluate(m_sky->timeOfDay() / m_sky->dayLength());
      if (!result87)
        return std::unexpected(std::move(result87.error()));
      layer.alpha *= std::clamp(static_cast<float>(std::move(*result87)), 0.0f, 1.0f);
    }
  }
  
  std::stable_sort(renderData.parallaxLayers.begin(), renderData.parallaxLayers.end(), [](ParallaxLayer const& a, ParallaxLayer const& b) {
    return std::tie(a.zLevel, a.verticalOrigin) > std::tie(b.zLevel, b.verticalOrigin);
  });
  
  auto overlayToDrawable = [&self = *this](WorldStructure::Overlay const& overlay) -> std::expected<Drawable, StarException> {
    auto result88 = self.root().imageMetadataDatabase();
    if (!result88)
      return std::unexpected(std::move(result88.error()));
    auto result89 = Drawable::makeImage(*(std::move(*result88)), overlay.image, 1.0f / TilePixels, false, overlay.min);
    if (!result89)
      return std::unexpected(std::move(result89.error()));
    Drawable drawable = (std::move(*result89));
    drawable.fullbright = overlay.fullbright;
    return drawable;
  };
  
  for (auto const& overlay : m_centralStructure.backgroundOverlays()) {
    auto drawable = overlayToDrawable(overlay);
    if (!drawable) {
      return std::unexpected(std::move(drawable.error()));
    }
    renderData.backgroundOverlays.push_back(std::move(*drawable));
  }
  for (auto const& overlay : m_centralStructure.foregroundOverlays()) {
    auto drawable = overlayToDrawable(overlay);
    if (!drawable) {
      return std::unexpected(std::move(drawable.error()));
    }
    renderData.foregroundOverlays.push_back(std::move(*drawable));
  }
  
  renderData.isFullbright = m_fullBright;
  renderData.dimLevel = m_worldDimLevel;
  renderData.dimColor = m_worldDimColor;
  return {};
}

List<std::shared_ptr<AudioInstance>> WorldClient::pullPendingAudio() { return std::exchange(m_samples, {}); }

List<std::shared_ptr<AudioInstance>> WorldClient::pullPendingMusic() { return std::exchange(m_music, {}); }

void WorldClient::dimWorld() {
  m_worldDimTimer.reset();
}

bool WorldClient::interactiveHighlightMode() const {
  return m_interactiveHighlightMode;
}

void WorldClient::setInteractiveHighlightMode(bool enabled) {
  m_interactiveHighlightMode = enabled;
}

void WorldClient::setParallax(std::shared_ptr<Parallax> newParallax) {
  if (newParallax) {
    if (!m_currentParallax) {
      m_currentParallax = std::move(newParallax);
    } else if (m_parallaxFadeTimer.ready() && newParallax != m_currentParallax) {
      m_nextParallax = std::move(newParallax);
      m_parallaxFadeTimer.reset();
    } else if (m_nextParallax && newParallax == m_currentParallax) {
      m_currentParallax = m_nextParallax;
      m_nextParallax = std::move(newParallax);
      m_parallaxFadeTimer.invert();
    }
  }
}

void WorldClient::overrideGravity(float gravity) {
  m_overrideGravity = gravity;
}

void WorldClient::resetGravity() {
  m_overrideGravity = {};
}

bool WorldClient::fullBright() const {
  return m_fullBright;
}

void WorldClient::setFullBright(bool fullBright) {
  m_fullBright = fullBright;
}

bool WorldClient::asyncLighting() const {
  return m_asyncLighting;
}

void WorldClient::setAsyncLighting(bool asyncLighting) {
  m_asyncLighting = asyncLighting;
}

bool WorldClient::collisionDebug() const {
  return m_collisionDebug;
}

void WorldClient::setCollisionDebug(bool collisionDebug) {
  m_collisionDebug = collisionDebug;
}

std::expected<void, StarException> WorldClient::handleIncomingPackets(List<std::shared_ptr<Packet>> const& packets) {
  auto& root = this->root();
  auto result90 = root.materialDatabase();
  if (!result90)
    return std::unexpected(std::move(result90.error()));
  auto materialDatabase = (std::move(*result90));
  auto result91 = root.itemDatabase();
  if (!result91)
    return std::unexpected(std::move(result91.error()));
  auto itemDatabase = (std::move(*result91));
  auto result92 = root.entityFactory();
  if (!result92)
    return std::unexpected(std::move(result92.error()));
  auto entityFactory = (std::move(*result92));
  
  for (auto const& packet : packets) {
    if (!inWorld() && !is<WorldStartPacket>(packet)) {
      if (!PacketTypeNames)
        return std::unexpected(PacketTypeNames.error());
      auto result93 = PacketTypeNames->get_right(packet->type());
      if (!result93)
        return std::unexpected(std::move(result93.error()));
      auto result94 = this->root().logger().error("WorldClient received packet type {} while not in world", result93->get());
      if (!result94)
        return std::unexpected(std::move(result94.error()));
    }
    
    if (auto worldStartPacket = as<WorldStartPacket>(packet)) {
      auto result95 = initWorld(*worldStartPacket);
      if (!result95)
        return std::unexpected(std::move(result95.error()));
    } else if (auto worldStopPacket = as<WorldStopPacket>(packet)) {
      auto result96 = this->root().logger().info("Client received world stop packet, leaving: {}", worldStopPacket->reason);
      if (!result96)
        return std::unexpected(std::move(result96.error()));
      auto result97 = clearWorld();
      if (!result97)
        return std::unexpected(std::move(result97.error()));
    } else if (auto entityCreate = as<EntityCreatePacket>(packet)) {
      if (m_entityMap->entity(entityCreate->entityId)) {
        auto result98 = this->root().logger().error("WorldClient received entity create packet with duplicate entity id {}, deleting old entity.", entityCreate->entityId);
        if (!result98)
          return std::unexpected(std::move(result98.error()));
        auto result99 = removeEntity(entityCreate->entityId, false);
        if (!result99)
          return std::unexpected(std::move(result99.error()));
      }
      
      auto netRules = m_clientState->netCompatibilityRules();
      auto result100 = entityFactory->netLoadEntity(entityCreate->entityType, entityCreate->storeData, netRules);
      if (!result100)
        return std::unexpected(std::move(result100.error()));
      auto entity = (std::move(*result100));
      auto result101 = entity->readNetState(entityCreate->firstNetState, 0.0f, netRules);
      if (!result101)
        return std::unexpected(std::move(result101.error()));
      auto result102 = entity->init(*this, entityCreate->entityId, EntityMode::Slave);
      if (!result102)
        return std::unexpected(std::move(result102.error()));
      auto result103 = m_entityMap->addEntity(entity);
      if (!result103) {
        auto cleared = entity->uninit();
        if (!cleared)
          return std::unexpected(result103.error().with_context(Star::format("Error uninitializing rejected entity: {}", cleared.error().what())));
        return std::unexpected(std::move(result103.error()));
      }
      
      if (m_interpolationTracker.interpolationEnabled()) {
        auto result104 = entity->enableInterpolation(m_interpolationTracker.extrapolationHint());
        if (!result104)
          return std::unexpected(std::move(result104.error()));
        
        // Delay appearance of new slaved entities to match with interplation
        // state.
        m_startupHiddenEntities.add(entityCreate->entityId);
        timer(m_interpolationTracker.interpolationLeadTime(), [&self = *this, entityId = entityCreate->entityId](World&)  -> std::expected<void, StarException> {
          self.m_startupHiddenEntities.remove(entityId);
          return {};
        });
      }
    } else if (auto entityUpdateSet = as<EntityUpdateSetPacket>(packet)) {
      float interpolationLeadTime = m_interpolationTracker.interpolationLeadTime();
      auto result106 = m_entityMap->forAllEntities([&, &self = *this](std::shared_ptr<Entity> const& entity)  -> std::expected<void, StarException> {
              EntityId entityId = entity->entityId();
              if (connectionForEntity(entityId) == entityUpdateSet->forConnection) {
                if (Star::DebugEnabled) {
                  Star::star_assert_failed((entity->isSlave()));
                }
                auto result105 = entity->readNetState(entityUpdateSet->deltas.value(entityId), interpolationLeadTime, self.m_clientState->netCompatibilityRules());
                if (!result105)
                  return std::unexpected(std::move(result105.error()));
              }
              return {};
            });
      if (!result106)
        return std::unexpected(std::move(result106.error()));
    } else if (auto entityDestroy = as<EntityDestroyPacket>(packet)) {
      if (auto entity = m_entityMap->entity(entityDestroy->entityId)) {
        auto result107 = entity->readNetState(entityDestroy->finalNetState, m_interpolationTracker.interpolationLeadTime(), m_clientState->netCompatibilityRules());
        if (!result107)
          return std::unexpected(std::move(result107.error()));
        
        // Before destroying the entity, we should make sure that the entity is
        // using the absolute latest data, so we disable interpolation.
        
        if (m_interpolationTracker.interpolationEnabled() && entityDestroy->death) {
          // Delay death packets by the interpolation step to give time for
          // interpolation to catch up.
          timer(m_interpolationTracker.interpolationLeadTime(), [&self = *this, entity, entityDestroy](World&)  -> std::expected<void, StarException> {
            auto result108 = entity->disableInterpolation();
            if (!result108)
              return std::unexpected(std::move(result108.error()));
            auto result109 = self.removeEntity(entityDestroy->entityId, entityDestroy->death);
            if (!result109)
              return std::unexpected(std::move(result109.error()));
            return {};
          });
        } else {
          auto result110 = entity->disableInterpolation();
          if (!result110)
            return std::unexpected(std::move(result110.error()));
          auto result111 = removeEntity(entityDestroy->entityId, entityDestroy->death);
          if (!result111)
            return std::unexpected(std::move(result111.error()));
        }
      }
    } else if (auto structurePacket = as<CentralStructureUpdatePacket>(packet)) {
      auto result112 = WorldStructure::create(structurePacket->structureData);
      if (!result112)
        return std::unexpected(std::move(result112.error()));
      m_centralStructure = (std::move(*result112));
    } else if (auto tileArrayUpdate = as<TileArrayUpdatePacket>(packet)) {
      RectI tileRegion = RectI::withSize(tileArrayUpdate->min, Vec2I(tileArrayUpdate->array.size()));
      
      // NOTE: We're creating client side sectors on tileArrayUpdate here, and
      // at no other time, and this is sort of a big assumption that
      // tileArrayUpdate happens for all valid client side sectors first before
      // any other tile updates.
      {
        auto result113 = m_tileArray->validSectorsFor(tileRegion);
        if (!result113)
          return std::unexpected(std::move(result113.error()));
        for (auto const& sector : (std::move(*result113))) {
          m_tileArray->loadDefaultSector(sector);
        }
      }
      
      for (int x = tileRegion.xMin(); x < tileRegion.xMax(); ++x) {
        for (int y = tileRegion.yMin(); y < tileRegion.yMax(); ++y) {
          auto result114 = readNetTile({x, y}, tileArrayUpdate->array(x - tileRegion.xMin(), y - tileRegion.yMin()), false);
          if (!result114)
            return std::unexpected(std::move(result114.error()));
        }
      }
      dirtyCollision(tileRegion);
    } else if (auto tileUpdate = as<TileUpdatePacket>(packet)) {
      auto result115 = readNetTile(tileUpdate->position, tileUpdate->tile);
      if (!result115)
        return std::unexpected(std::move(result115.error()));
    } else if (auto tileDamageUpdate = as<TileDamageUpdatePacket>(packet)) {
      if (auto tile = m_tileArray->modifyTile(tileDamageUpdate->position)) {
        if (tileDamageUpdate->layer == TileLayer::Foreground) {
          tile->get().foregroundDamage = tileDamageUpdate->tileDamage;
        } else {
          tile->get().backgroundDamage = tileDamageUpdate->tileDamage;
        }
        
        m_damagedBlocks.add(tileDamageUpdate->position);
      }
    } else if (auto tileModificationFailure = as<TileModificationFailurePacket>(packet)) {
      // TODO: Right now we assume that every tile modification was caused by a
      // player, but this may not be true in the future.  In the future, there
      // may be context hints with tile modifications to figure out what to do
      // with failures.
      for (auto& modification : tileModificationFailure->modifications) {
        auto findPrediction = m_predictedTiles.find(modification.first);
        if (findPrediction != m_predictedTiles.end()) {
          auto& p = findPrediction->second;
          if (std::holds_alternative<PlaceMaterial>(modification.second)) {
            auto const& placeMaterial = std::get<PlaceMaterial>(modification.second);
            if (placeMaterial.layer == TileLayer::Foreground) {
              p.foreground.reset();
              p.foregroundHueShift.reset();
              if (p.collision) {
                p.collision.reset();
                dirtyCollision(RectI::withSize(modification.first, { 1, 1 }));
              }
            } else {
              p.background.reset();
              p.backgroundHueShift.reset();
            }
          } else if (std::holds_alternative<PlaceMod>(modification.second)) {
            auto const& placeMod = std::get<PlaceMod>(modification.second);
            if (placeMod.layer == TileLayer::Foreground) {
              p.foregroundMod.reset();
              p.foregroundModHueShift.reset();
            } else {
              p.backgroundMod.reset();
              p.backgroundModHueShift.reset();
            }
          } else if (std::holds_alternative<PlaceMaterialColor>(modification.second)) {
            auto const& placeColor = std::get<PlaceMaterialColor>(modification.second);
            if (placeColor.layer == TileLayer::Foreground) {
              p.foregroundColorVariant.reset();
            } else {
              p.backgroundColorVariant.reset();
            }
          } else if (std::holds_alternative<PlaceLiquid>(modification.second)) {
            p.liquid.reset();
          }
          
          if (!p) {
            m_predictedTiles.erase(findPrediction);
          }
        }
        
        if (!m_headless) {
          if (std::holds_alternative<PlaceMaterial>(modification.second)) {
            auto result116 = materialDatabase->materialItemDrop(std::get<PlaceMaterial>(modification.second).material);
            if (!result116)
              return std::unexpected(std::move(result116.error()));
            auto stack = (std::move(*result116));
            auto result117 = itemDatabase->item(stack);
            if (!result117)
              return std::unexpected(std::move(result117.error()));
            auto result118 = tryGiveMainPlayerItem((std::move(*result117)), true);
            if (!result118)
              return std::unexpected(std::move(result118.error()));
          } else if (std::holds_alternative<PlaceMod>(modification.second)) {
            auto result119 = materialDatabase->modItemDrop(std::get<PlaceMod>(modification.second).mod);
            if (!result119)
              return std::unexpected(std::move(result119.error()));
            auto stack = (std::move(*result119));
            auto result120 = itemDatabase->item(stack);
            if (!result120)
              return std::unexpected(std::move(result120.error()));
            auto result121 = tryGiveMainPlayerItem((std::move(*result120)), true);
            if (!result121)
              return std::unexpected(std::move(result121.error()));
          }
        }
      }
    } else if (auto liquidUpdate = as<TileLiquidUpdatePacket>(packet)) {
      m_predictedTiles.remove(liquidUpdate->position);
      if (auto tile = m_tileArray->modifyTile(liquidUpdate->position)) {
        tile->get().liquid = liquidUpdate->liquidUpdate.liquidLevel();
      }
    } else if (auto giveItem = as<GiveItemPacket>(packet)) {
      if (m_headless) {
        // TODO: call something on world script
      } else {
        auto result122 = itemDatabase->item(giveItem->item);
        if (!result122)
          return std::unexpected(std::move(result122.error()));
        auto result123 = tryGiveMainPlayerItem((std::move(*result122)));
        if (!result123)
          return std::unexpected(std::move(result123.error()));
      }
    } else if (auto stepUpdate = as<StepUpdatePacket>(packet)) {
      m_interpolationTracker.receiveTimeUpdate(stepUpdate->remoteTime);
    } else if (auto environmentUpdatePacket = as<EnvironmentUpdatePacket>(packet)) {
      auto result124 = m_sky->readUpdate(environmentUpdatePacket->skyDelta, m_clientState->netCompatibilityRules());
      if (!result124)
        return std::unexpected(std::move(result124.error()));
      auto result125 = currentWeatherDomain();
      if (!result125)
        return std::unexpected(std::move(result125.error()));
      auto weatherDomain = (std::move(*result125));
      if (weatherDomain != m_weatherDomain) {
        auto result126 = m_weather->clear();
        if (!result126)
          return std::unexpected(std::move(result126.error()));
        m_weatherDomain = std::exchange(weatherDomain, {});
      }
      auto result127 = m_weather->readUpdate(environmentUpdatePacket->weatherDelta, m_clientState->netCompatibilityRules());
      if (!result127)
        return std::unexpected(std::move(result127.error()));
    } else if (auto hit = as<HitRequestPacket>(packet)) {
      auto result128 = m_damageManager->pushRemoteHitRequest(hit->remoteHitRequest);
      if (!result128)
        return std::unexpected(std::move(result128.error()));
    } else if (auto damage = as<DamageRequestPacket>(packet)) {
      auto result129 = m_damageManager->pushRemoteDamageRequest(damage->remoteDamageRequest);
      if (!result129)
        return std::unexpected(std::move(result129.error()));
    } else if (auto damage = as<DamageNotificationPacket>(packet)) {
      std::string_view view = damage->remoteDamageNotification.damageNotification.targetMaterialKind.utf8();
      static const std::size_t FULL_SIZE = SECRET_BROADCAST_PREFIX.size() + Curve25519::SignatureSize;
      static const std::string LEGACY_VOICE_PREFIX = std::string("data\0voice\0", sizeof("data\0voice\0") - 1);
      
      if (view.size() >= FULL_SIZE && view.rfind(SECRET_BROADCAST_PREFIX, 0) != NPos) {
        // this is actually a secret broadcast!!
        if (auto player = m_entityMap->get<Player>(damage->remoteDamageNotification.sourceEntityId)) {
          if (auto publicKey = player->getSecretPropertyView(SECRET_BROADCAST_PUBLIC_KEY)) {
            if (publicKey->utf8_size() == Curve25519::PublicKeySize) {
              auto signature = view.substr(SECRET_BROADCAST_PREFIX.size(), Curve25519::SignatureSize);
              
              auto rawBroadcast = view.substr(FULL_SIZE);
              if (Curve25519::verify(
                      std::span<std::uint8_t const, Curve25519::SignatureSize>(reinterpret_cast<std::uint8_t const*>(signature.data()), Curve25519::SignatureSize),
                      std::span<std::uint8_t const, Curve25519::PublicKeySize>(reinterpret_cast<std::uint8_t const*>(publicKey->utf8_data()), Curve25519::PublicKeySize),
                      std::span<char const>(rawBroadcast.data(), rawBroadcast.size()))) {
                auto result130 = handleSecretBroadcast(player, rawBroadcast);
                if (!result130)
                  return std::unexpected(std::move(result130.error()));
              }
            }
          }
        }
      } else if (view.size() > 75 && view.rfind(LEGACY_VOICE_PREFIX, 0) != NPos) {
        // this is a StarExtensions voice packet
        // (remove this and stop transmitting like this once most SE features are ported over)
        if (auto player = m_entityMap->get<Player>(damage->remoteDamageNotification.sourceEntityId)) {
          if (auto publicKey = player->effectsAnimator()->globalTag(std::string("\0SE_VOICE_SIGNING_KEY", sizeof("\0SE_VOICE_SIGNING_KEY") - 1))) {
            auto raw = view.substr(75);
            if (m_broadcastCallback && Curve25519::verify(
                    std::span<std::uint8_t const, Curve25519::SignatureSize>(reinterpret_cast<std::uint8_t const*>(view.data() + LEGACY_VOICE_PREFIX.size()), Curve25519::SignatureSize),
                    std::span<std::uint8_t const, Curve25519::PublicKeySize>(reinterpret_cast<std::uint8_t const*>(publicKey->get().utf8().data()), Curve25519::PublicKeySize),
                    std::span<char const>(raw.data(), raw.size()))) {
              auto broadcastData = std::string("Voice\0", sizeof("Voice\0") - 1);
              broadcastData.append(raw.data(), raw.size());
              auto result131 = m_broadcastCallback(player, broadcastData);
              if (!result131)
                return std::unexpected(std::move(result131.error()));
            }
          }
        }
      } else {
        auto result132 = m_damageManager->pushRemoteDamageNotification(damage->remoteDamageNotification);
        if (!result132)
          return std::unexpected(std::move(result132.error()));
      }
    } else if (auto entityMessagePacket = as<EntityMessagePacket>(packet)) {
      std::shared_ptr<Entity> entity;
      if (std::holds_alternative<EntityId>(entityMessagePacket->entityId)) {
        entity = m_entityMap->entity(std::get<EntityId>(entityMessagePacket->entityId));
      } else {
        entity = m_entityMap->uniqueEntity(std::get<String>(entityMessagePacket->entityId));
      }
      
      if (!entity) {
        m_outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(
          std::unexpected(String("Unknown entity")), entityMessagePacket->uuid));
      } else if (!entity->isMaster()) {
        auto result133 = this->root().logger().error("Server has sent a scripted entity response for a slave entity");
        if (!result133)
          return std::unexpected(std::move(result133.error()));
        m_outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(
          std::unexpected(String("Entity delivery error")), entityMessagePacket->uuid));
      } else {
        ConnectionId fromConnection = entityMessagePacket->fromConnection;
        if (fromConnection == *m_clientId) {
          fromConnection = ServerConnectionId;
        }
        
        {
          auto responseResult = entity->receiveMessage(entityMessagePacket->fromConnection,
                                                       entityMessagePacket->message, entityMessagePacket->args);
          if (!responseResult)
            return std::unexpected(std::move(responseResult.error()));
          if (auto response = std::move(*responseResult)) {
            struct QueueEntityResponse {
              WorldClient& client;
              Uuid const& uuid;

              void operator()(Json& result) const {
                client.m_outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(result, uuid));
              }

              void operator()(RpcPromise<Json>& promise) const {
                // Delay the response until this promise is done.
                client.m_entityMessagePromises[uuid] = promise;
              }
            };
            std::visit(QueueEntityResponse{.client = *this, .uuid = entityMessagePacket->uuid}, *response);
          } else {
            m_outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(
              std::unexpected(String("Message not handled by entity")), entityMessagePacket->uuid));
          }
        }
      }
    } else if (auto entityMessageResponsePacket = as<EntityMessageResponsePacket>(packet)) {
      if (!m_entityMessageResponses.contains(entityMessageResponsePacket->uuid)) {
        auto result135 = this->root().logger().warn("EntityMessageResponse received for unknown context [{}]!", entityMessageResponsePacket->uuid.hex());
        if (!result135)
          return std::unexpected(std::move(result135.error()));
      } else {
        auto result136 = m_entityMessageResponses.extract_value(entityMessageResponsePacket->uuid);
        if (!result136)
          return std::unexpected(std::move(result136.error()));
        auto response = (std::move(*result136));
        if (entityMessageResponsePacket->response.has_value()) {
          auto result138 = response.fulfill(*entityMessageResponsePacket->response);
          if (!result138)
            return std::unexpected(std::move(result138.error()));
        } else {
          auto result140 = response.fail(entityMessageResponsePacket->response.error());
          if (!result140)
            return std::unexpected(std::move(result140.error()));
        }
      }
    } else if (auto updateWorldProperties = as<UpdateWorldPropertiesPacket>(packet)) {
      // Kae: Properties set to null (nil from Lua) should be erased instead of lingering around
      for (auto&& pair : updateWorldProperties->updatedProperties) {
        if (pair.second.isNull()) {
          m_worldProperties.erase(pair.first);
        } else {
          m_worldProperties[pair.first] = pair.second;
        }
      }
    } else if (auto updateTileProtection = as<UpdateTileProtectionPacket>(packet)) {
      setTileProtection(updateTileProtection->dungeonId, updateTileProtection->isProtected);
    } else if (auto setDungeonGravity = as<SetDungeonGravityPacket>(packet)) {
      if (setDungeonGravity->gravity) {
        m_dungeonIdGravity[setDungeonGravity->dungeonId] = *setDungeonGravity->gravity;
      } else {
        m_dungeonIdGravity.remove(setDungeonGravity->dungeonId);
      }
    } else if (auto setDungeonBreathable = as<SetDungeonBreathablePacket>(packet)) {
      if (setDungeonBreathable->breathable.has_value()) {
        m_dungeonIdBreathable[setDungeonBreathable->dungeonId] = *setDungeonBreathable->breathable;
      } else {
        m_dungeonIdBreathable.remove(setDungeonBreathable->dungeonId);
      }
    } else if (auto entityInteract = as<EntityInteractPacket>(packet)) {
      auto result141 = interact(entityInteract->interactRequest);
      if (!result141)
        return std::unexpected(std::move(result141.error()));
      auto result142 = (std::move(*result141)).result();
      if (!result142)
        return std::unexpected(std::move(result142.error()));
      auto interactResult = (std::move(*result142));
      m_outgoingPackets.push_back(std::make_shared<EntityInteractResultPacket>(
        interactResult.value_or(InteractAction{}), entityInteract->requestId,
        entityInteract->interactRequest.sourceId));
    } else if (auto interactResult = as<EntityInteractResultPacket>(packet)) {
      if (auto response = m_entityInteractionResponses.try_extract_value(interactResult->requestId)) {
        if (interactResult->action) {
          auto result143 = response->fulfill(interactResult->action);
          if (!result143)
            return std::unexpected(std::move(result143.error()));
        } else {
          auto result144 = response->fail("no interaction result");
          if (!result144)
            return std::unexpected(std::move(result144.error()));
        }
      }
    } else if (auto setPlayerStart = as<SetPlayerStartPacket>(packet)) {
      m_playerStart = setPlayerStart->playerStart;
      m_respawnInWorld = setPlayerStart->respawnInWorld;
    } else if (auto findUniqueEntityResponse = as<FindUniqueEntityResponsePacket>(packet)) {
      for (auto& promise : std::exchange(m_findUniqueEntityResponses[findUniqueEntityResponse->uniqueEntityId], {})) {
        if (findUniqueEntityResponse->entityPosition) {
          auto result145 = promise.fulfill(*findUniqueEntityResponse->entityPosition);
          if (!result145)
            return std::unexpected(std::move(result145.error()));
        } else {
          auto result146 = promise.fail("Unknown entity");
          if (!result146)
            return std::unexpected(std::move(result146.error()));
        }
      }
    } else if (auto worldLayoutUpdate = as<WorldLayoutUpdatePacket>(packet)) {
      auto result147 = WorldLayout::create(this->root(), worldLayoutUpdate->layoutData);
      if (!result147)
        return std::unexpected(std::move(result147.error()));
      m_worldTemplate->setWorldLayout((std::move(*result147)));
    } else if (auto worldParametersUpdate = as<WorldParametersUpdatePacket>(packet)) {
      auto result148 = netLoadVisitableWorldParameters(worldParametersUpdate->parametersData);
      if (!result148)
        return std::unexpected(std::move(result148.error()));
      auto result149 = m_worldTemplate->setWorldParameters((std::move(*result148)));
      if (!result149)
        return std::unexpected(std::move(result149.error()));
    } else if (auto pongPacket = as<PongPacket>(packet)) {
      if (pongPacket->time) {
        m_latency = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) - std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>(std::chrono::milliseconds(pongPacket->time));
      } else if (m_pingTime) {
        auto result150 = takeOptional(m_pingTime);
        if (!result150)
          return std::unexpected(std::move(result150.error()));
        m_latency = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) - (std::move(*result150));
      }
    } else {
      auto result151 =
        this->root().logger().error("Improper packet type {} received by client", static_cast<int>(packet->type()));
      if (!result151)
        return std::unexpected(std::move(result151.error()));
    }
  }
  return {};
}

List<std::shared_ptr<Packet>> WorldClient::getOutgoingPackets() {
  return std::move(m_outgoingPackets);
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> WorldClient::receiveMessage(ConnectionId fromConnection, String const& message, JsonArray const& args) {
  m_expiryTimer.reset();
  if (!inWorld()) {
    // script contexts aren't active to handle this message
    return {};
  }
  std::optional<ChainableJsonMessageResponse> result;
  for (auto&& p : m_scriptContexts) {
    auto result152 = recoverLuaScriptError(p.second->handleMessage(message, fromConnection == ServerConnectionId, args));
    if (!result152)
      return std::unexpected(std::move(result152.error()));
    result = (std::move(*result152));
    if (result) {
      break;
    }
  }
  return result;
}

std::shared_ptr<WorldClient::ScriptComponent> WorldClient::scriptContext(String const& contextName) {
  if (auto context = m_scriptContexts.find_ref(contextName))
    return context->get();
  else
    return nullptr;
}

std::expected<void, StarException> WorldClient::update(float dt) {
  m_expiryTimer.tick(dt);
  if (!inWorld()) {
    return {};
  }
  m_presentationTimestep = dt;
  m_worldDimTimer.tick(dt);
  m_parallaxFadeTimer.tick(dt);
  if (m_parallaxFadeTimer.ready() && m_nextParallax) {
    m_currentParallax = m_nextParallax;
    m_nextParallax.reset();
  }
  auto beginPresentation = m_entityMap->forAllEntities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    auto& state = m_entityPresentation[entity->entityId()];
    if (state.instance.lock() != entity || state.epoch != entity->presentationEpoch()) {
      state.instance = entity;
      state.epoch = entity->presentationEpoch();
      state.position.reset(entity->position());
      state.continuous = false;
    } else {
      state.position.advance(entity->position());
      state.continuous = true;
    }
    return {};
  });
  if (!beginPresentation)
    return std::unexpected(std::move(beginPresentation.error()));
  
  auto result153 = this->root().assets();
  if (!result153)
    return std::unexpected(std::move(result153.error()));
  auto assets = (std::move(*result153));

  auto expireTime = std::min(
    std::chrono::duration<float, std::milli>(static_cast<float>((m_latency + std::chrono::milliseconds(800)).count())),
    std::chrono::duration<float, std::milli>(2000.f));
  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  eraseWhere(m_predictedTiles, [&, &self = *this](auto&& pair) {
    float expiry = std::chrono::duration<float, std::milli>(float((now - pair.second.time).count())) / expireTime;
    auto center = Vec2F(pair.first) + Vec2F::filled(0.5f);
    auto size = Vec2F::filled(0.875f - expiry * 0.875f);
    auto poly = PolyF(RectF::withCenter(center, size));
    self.root().spatialLogger().log_poly("world", poly, Color::Cyan.mix(Color::Red, expiry).toRgba());
    if (expiry >= 1.0f) {
      self.dirtyCollision(RectI::withSize(pair.first, { 1, 1 }));
      return true;
    } else {
      return false;
    }
  });

  if (!m_headless) {
    // Secret broadcasts are transmitted through DamageNotifications for vanilla server compatibility.
    // Because DamageNotification packets are spoofable, we have to sign the data so other clients can validate that it is legitimate.
    auto& publicKey = this->root().identity().publicKey();
    String publicKeyString(std::string_view(reinterpret_cast<char const*>(publicKey.data()), publicKey.size()));
    auto result154 = m_mainPlayer->setSecretProperty(SECRET_BROADCAST_PUBLIC_KEY, publicKeyString);
    if (!result154)
      return std::unexpected(std::move(result154.error()));
    // Temporary: Backwards compatibility with StarExtensions
    m_mainPlayer->effectsAnimator()->setGlobalTag(std::string("\0SE_VOICE_SIGNING_KEY", sizeof("\0SE_VOICE_SIGNING_KEY") - 1), publicKeyString);
  }
  
  ++m_currentStep;
  m_currentTime += dt;
  m_interpolationTracker.update(m_currentTime);
  
  List<WorldAction> triggeredActions;
  m_timers.filter([&triggeredActions, dt](std::pair<float, WorldAction>& timer) {
    if ((timer.first -= dt) <= 0) {
      triggeredActions.push_back(timer.second);
      return false;
    }
    return true;
  });

  for (auto const& action : triggeredActions) {
    auto result155 = action(*this);
    if (!result155)
      return std::unexpected(std::move(result155.error()));
  }
  
  List<EntityId> toRemove;
  List<EntityId> clientPresenceEntities;
  auto result160 = m_entityMap->updateAllEntities([&, &self = *this](std::shared_ptr<Entity> const& entity)  -> std::expected<void, StarException> {
      auto updated = entity->update(dt, self.m_currentStep);
      if (!updated) {
        auto const& e = updated.error();
        if (entity->isMaster()) {
          return std::unexpected(std::move(updated.error()));
        } else {
          // this is THEIR problem!!
          auto issue = Star::print_exception(e, true);
          auto hash = hash_of(issue);
          if (!self.m_entityExceptionsLogged.contains(hash)) {
            self.m_entityExceptionsLogged.insert(hash);
          } else {
            issue = e.what();
          }
          
          if (!EntityTypeNames)
            return std::unexpected(EntityTypeNames.error());
          auto result156 = EntityTypeNames->get_right(entity->entityType());
          if (!result156)
            return std::unexpected(std::move(result156.error()));
          auto result157 = self.root().logger().error("WorldClient: Exception caught in {}::update ({}): {}", result156->get(), entity->entityId(), issue);
          if (!result157)
            return std::unexpected(std::move(result157.error()));
        }
      }
      
      {
        auto result159 = entity->shouldDestroy();
        if (!result159)
          return std::unexpected(std::move(result159.error()));
        if ((std::move(*result159)) && entity->entityMode() == EntityMode::Master) {
          toRemove.push_back(entity->entityId());
        }
      }
      if (entity->isMaster() && entity->clientEntityMode() == ClientEntityMode::ClientPresenceMaster) {
        clientPresenceEntities.push_back(entity->entityId());
      }
      return {};
    }, [](std::shared_ptr<Entity> const& a, std::shared_ptr<Entity> const& b) {
      return a->entityType() < b->entityType();
    });
  if (!result160)
    return std::unexpected(std::move(result160.error()));
  if (m_headless) {
    if (m_entityMap->size() > 0) {
      m_expiryTimer.reset();
    } else if (m_expiryTimer.ready()) {
      // world has had no entities and isn't being used, destroy it
      requestDestroy();
    }
  }
  
  RectI particleRegion;
  if (!m_headless) {
    m_clientState->setPlayer(m_mainPlayer->entityId());
    m_sky->setAltitude(m_clientState->windowCenter()[1]);
    auto clientWindow = m_clientState->window();
    auto result161 = m_clientConfig.getInt("particleRegionPadding");
    if (!result161)
      return std::unexpected(std::move(result161.error()));
    particleRegion = clientWindow.padded((std::move(*result161)));
    // Weather generation is layer-scoped, but the particle manager also owns
    // particles from entities, materials, and effects and must retain the full
    // client region.
    RectI weatherParticleRegion = particleRegion;
    
    std::optional<String> weatherDomain = m_weatherDomain;
    if (clientWindow.isEmpty()) {
      weatherParticleRegion = {};
    } else {
      auto result162 = m_worldTemplate->weatherLayerAt(Vec2I::floor(m_clientState->windowCenter()));
      if (!result162)
        return std::unexpected(std::move(result162.error()));
      if (auto layer = (std::move(*result162))) {
        weatherDomain = layer->get().domain;
        weatherParticleRegion = RectI(weatherParticleRegion.xMin(), std::max(weatherParticleRegion.yMin(), layer->get().minHeight),
            weatherParticleRegion.xMax(), std::min(weatherParticleRegion.yMax(), layer->get().maxHeight));
      } else {
        weatherParticleRegion = {};
      }
    }
    
    if (weatherDomain != m_weatherDomain) {
      weatherParticleRegion = {};
    }
    
    m_weather->setVisibleRegion(weatherParticleRegion);
  }
  m_clientState->setClientPresenceEntities(std::move(clientPresenceEntities));

  for (auto&& pair : m_scriptContexts) {
    auto result163 = recoverLuaScriptError(pair.second->update(pair.second->updateDt(dt)));
    if (!result163)
      return std::unexpected(std::move(result163.error()));
  }

  auto result164 = m_damageManager->update(dt);
  if (!result164)
    return std::unexpected(std::move(result164.error()));
  auto result165 = handleDamageNotifications();
  if (!result165)
    return std::unexpected(std::move(result165.error()));
  
  auto result166 = m_sky->update(dt);
  if (!result166)
    return std::unexpected(std::move(result166.error()));
  
  auto result167 = m_weather->update(dt);
  if (!result167)
    return std::unexpected(std::move(result167.error()));
  
  if (!m_headless) {
    auto flickerAmount = assets->json("/highlights.config:inspectionFlickerAmount");
    if (!flickerAmount)
      return std::unexpected(std::move(flickerAmount.error()));
    auto flicker = flickerAmount->toFloat();
    if (!flicker)
      return std::unexpected(std::move(flicker.error()));
    auto flickerSample = this->root().random().randf(1 - *flicker, 1);
    if (!flickerSample)
      return std::unexpected(std::move(flickerSample.error()));
    m_inspectionFlickerMultiplier = *flickerSample;
    double parallaxEpoch = m_sky->epochTime();
    auto weatherDomain = currentWeatherDomain();
    if (!weatherDomain)
      return std::unexpected(std::move(weatherDomain.error()));
    float parallaxWind = *weatherDomain == m_weatherDomain ? m_weather->wind() : 0.0f;
    if (m_lastParallaxWindEpoch && parallaxEpoch >= *m_lastParallaxWindEpoch) {
      double elapsed = parallaxEpoch - *m_lastParallaxWindEpoch;
      m_parallaxWindDirectionTime += elapsed * (parallaxWind > 0.0f ? 1.0 : -1.0);
      m_parallaxWindMagnitudeTime += elapsed * std::abs(parallaxWind);
      m_parallaxSignedWindTime += elapsed * parallaxWind;
    }
    m_lastParallaxWindEpoch = parallaxEpoch;
    if (!m_mainPlayer->isDead()) {
      // Clear m_requestedDrops every so often in case of entity id reuse or
      // desyncs etc
      {
        auto result168 = m_clientConfig.getInt("itemRequestReset");
        if (!result168)
          return std::unexpected(std::move(result168.error()));
        if (m_currentStep % (std::move(*result168)) == 0) {
          m_requestedDrops.clear();
        }
      }
      
      Vec2F playerPos = m_mainPlayer->position();
      auto result169 = m_entityMap->query<ItemDrop>(RectF(playerPos - Vec2F::filled(DropDist / 2), playerPos + Vec2F::filled(DropDist / 2)));
      if (!result169)
        return std::unexpected(std::move(result169.error()));
      auto dropList = (std::move(*result169));
      for (auto const& itemDrop : dropList) {
        auto distSquared = m_geometry.diff(itemDrop->position(), playerPos).magnitude_squared();

        // If the drop is within DropDist and not owned, request it.
        {
          auto result173 = itemDrop->canTake();
          if (!result173)
            return std::unexpected(std::move(result173.error()));
          if ((std::move(*result173)) && !m_requestedDrops.contains(itemDrop->entityId()) && distSquared < square(DropDist)) {
            m_requestedDrops.add(itemDrop->entityId());
            {
              auto result174 = m_mainPlayer->itemsCanHold(itemDrop->item());
              if (!result174)
                return std::unexpected(std::move(result174.error()));
              if ((std::move(*result174)) != 0) {
                m_startupHiddenEntities.erase(itemDrop->entityId());
                auto result175 =
                  itemDrop->takeBy(m_mainPlayer->entityId(), static_cast<float>(m_latency.count()) / 1000);
                if (!result175)
                  return std::unexpected(std::move(result175.error()));
                m_outgoingPackets.push_back(std::make_shared<RequestDropPacket>(itemDrop->entityId()));
              }
            }
          }
        }
      }
    } else {
      m_requestedDrops.clear();
    }
    
    auto result176 = sparkDamagedBlocks();
    if (!result176)
      return std::unexpected(std::move(result176.error()));
    
    m_particles->addParticles(m_weather->pullNewParticles());
    auto result177 = m_particles->update(dt, RectF(particleRegion), m_weather->wind());
    if (!result177)
      return std::unexpected(std::move(result177.error()));
    // Entity events are born at this completed tick's state. Unlike weather,
    // they must not integrate/age for the timestep that produced their birth.
    auto events = m_entityMap->forAllEntities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
      // The removal path performs the final live-event drain before destroy().
      // Collectors may synthesize effects, so a queued death must not run both.
      if (toRemove.contains(entity->entityId()))
        return {};
      // Offscreen entities still emit once per tick. Startup-hidden entities
      // consume their queues without exposing effects before their spawn.
      bool hidden = m_startupHiddenEntities.contains(entity->entityId());
      List<std::shared_ptr<AudioInstance>>* deferredLoops = nullptr;
      if (hidden) {
        auto& pending = m_hiddenEntityAudio[entity->entityId()];
        if (pending.instance.lock() != entity) {
          pending.instance = entity;
          pending.loops.clear();
        }
        deferredLoops = &pending.loops;
      }
      NullRenderCallback hiddenEvents(deferredLoops);
      m_eventRenderCallback.clear();
      auto collected = entity->collectRenderEvents(hidden
          ? static_cast<RenderCallback&>(hiddenEvents)
          : static_cast<RenderCallback&>(m_eventRenderCallback));
      if (!collected) {
        if (entity->isMaster())
          return std::unexpected(std::move(collected.error()));
        auto issue = Star::print_exception(collected.error(), true);
        if (!m_entityExceptionsLogged.insert(hash_of(issue)).second)
          issue = collected.error().what();
        auto logged = this->root().logger().error("WorldClient: Exception caught collecting entity {} presentation events: {}", entity->entityId(), issue);
        if (!logged)
          return std::unexpected(std::move(logged.error()));
      }
      if (hidden)
        return {};
      if (auto pending = m_hiddenEntityAudio.try_extract_value(entity->entityId())) {
        if (pending->instance.lock() == entity)
          for (auto& audio : pending->loops)
            if (!audio->finished())
              m_samples.push_back(std::move(audio));
      }
      if (auto const& parameters = m_worldTemplate->worldParameters()) {
        if (auto const& directives = parameters->globalDirectives; directives && !directives->empty()) {
          auto index = static_cast<unsigned>(entity->entityId()) % directives->size();
          for (auto& particle : m_eventRenderCallback.particles)
            particle.directives.append((*directives)[index]);
        }
      }
      m_particles->addParticles(std::move(m_eventRenderCallback.particles));
      m_samples.append_range(std::move(m_eventRenderCallback.audios));
      return {};
    });
    if (!events)
      return std::unexpected(std::move(events.error()));
    
    {
      auto result178 = currentAmbientNoises();
      if (!result178)
        return std::unexpected(std::move(result178.error()));
      auto result179 = m_ambientSounds.updateAmbient((std::move(*result178)), m_sky->isDayTime());
      if (!result179)
        return std::unexpected(std::move(result179.error()));
      if (auto audioSample = (std::move(*result179))) {
        m_samples.push_back(audioSample);
      }
    }
    {
      auto result180 = currentWeatherNoises();
      if (!result180)
        return std::unexpected(std::move(result180.error()));
      auto result181 = m_ambientSounds.updateWeather((std::move(*result180)));
      if (!result181)
        return std::unexpected(std::move(result181.error()));
      if (auto audioSample = (std::move(*result181))) {
        m_samples.push_back(audioSample);
      }
    }
    
    if (inSpace()) {
      auto result182 = m_sky->pullSounds();
      if (!result182)
        return std::unexpected(std::move(result182.error()));
      m_samples.append_range((std::move(*result182)));

      if (m_spaceSound && m_spaceSound->finished()) {
        m_spaceSound = {};
        m_activeSpaceSound = "";
      }
      
      auto result183 = m_sky->ambientNoise();
      if (!result183)
        return std::unexpected(std::move(result183.error()));
      auto skyAmbientNoise = (std::move(*result183));
      if (skyAmbientNoise != m_activeSpaceSound) {
        if (m_spaceSound) {
          m_spaceSound->stop(skyAmbientNoise == "" ? 3.0 : 0.0);
        } else {
          m_activeSpaceSound = skyAmbientNoise;
          if (!m_activeSpaceSound.empty()) {
            auto result184 = assets->audio(m_activeSpaceSound);
            if (!result184)
              return std::unexpected(std::move(result184.error()));
            auto result185 = AudioInstance::create(*(std::move(*result184)));
            if (!result185)
              return std::unexpected(std::move(result185.error()));
            m_spaceSound = (std::move(*result185));
            m_samples.push_back(m_spaceSound);
          }
        }
      }
    }
    
    if (auto newAltMusic = m_mainPlayer->pullPendingAltMusic()) {
      if (newAltMusic->first) {
        playAltMusic(newAltMusic->first->first, newAltMusic->second, newAltMusic->first->second);
      } else {
        stopAltMusic(newAltMusic->second);
      }
    }
    
    {
      auto result186 = m_altMusicTrack.updateAmbient(currentAltMusicTrack(), true);
      if (!result186)
        return std::unexpected(std::move(result186.error()));
      if (auto audioSample = (std::move(*result186))) {
        m_music.push_back(audioSample);
      }
    }
    
    {
      auto result187 = currentMusicTrack();
      if (!result187)
        return std::unexpected(std::move(result187.error()));
      auto result188 = m_musicTrack.updateAmbient((std::move(*result187)), m_sky->isDayTime());
      if (!result188)
        return std::unexpected(std::move(result188.error()));
      if (auto audioSample = (std::move(*result188))) {
        m_music.push_back(audioSample);
      }
    }
  }
  
  for (EntityId entityId : toRemove) {
    auto result189 = removeEntity(entityId, true);
    if (!result189)
      return std::unexpected(std::move(result189.error()));
  }
  for (auto const& uuid : m_entityMessagePromises.keys()) {
    {
      auto result190 = m_entityMessagePromises[uuid].finished();
      if (!result190)
        return std::unexpected(std::move(result190.error()));
      if ((std::move(*result190))) {
        {
          auto result191 = m_entityMessagePromises[uuid].succeeded();
          if (!result191)
            return std::unexpected(std::move(result191.error()));
          if ((std::move(*result191))) {
            auto result192 = m_entityMessagePromises[uuid].result();
            if (!result192)
              return std::unexpected(std::move(result192.error()));
            m_outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(*(std::move(*result192)), uuid));
          } else {
            auto result193 = m_entityMessagePromises[uuid].error();
            if (!result193)
              return std::unexpected(std::move(result193.error()));
            m_outgoingPackets.push_back(
              std::make_shared<EntityMessageResponsePacket>(std::unexpected(*(std::move(*result193))), uuid));
          }
        }
        m_entityMessagePromises.remove(uuid);
      }
    }
  }
  
  auto result194 = queueUpdatePackets(m_entityUpdateTimer.wrapTick(dt));
  if (!result194)
    return std::unexpected(std::move(result194.error()));
  
  if ((!m_clientState->netCompatibilityRules().isLegacy() && m_currentStep % 3 == 0) || !m_pingTime.has_value()) {
    m_pingTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
    m_outgoingPackets.push_back(std::make_shared<PingPacket>(m_pingTime->time_since_epoch().count()));
  }
  
  this->root().logMap().set(Star::format("client_{}_ping",m_subWorldId), m_latency.count());
  
  // Remove active sectors that are outside of the current monitoring region
  Set<ClientTileSectorArray::Sector> neededSectors;
  auto monitoredRegions = m_clientState->monitoringRegions([&self = *this](EntityId entityId) -> std::optional<RectI> {
    if (auto entity = self.entity(entityId)) {
      return RectI::integral(entity->metaBoundBox().translated(entity->position()));
    }
    return {};
  });
  for (auto const& monitoredRegion : monitoredRegions) {
    auto result195 = m_tileArray->validSectorsFor(monitoredRegion.padded(WorldSectorSize));
    if (!result195)
      return std::unexpected(std::move(result195.error()));
    neededSectors.insert_range((std::move(*result195)));
  }

  auto loadedSectors = m_tileArray->loadedSectors();
  for (auto sector : loadedSectors) {
    if (!neededSectors.contains(sector)) {
      m_tileArray->unloadSector(sector);
    }
  }
  
  if (m_collisionDebug) {
    auto result196 = renderCollisionDebug();
    if (!result196)
      return std::unexpected(std::move(result196.error()));
  }
  
  this->root().logMap().set(Star::format("client_{}_entities",m_subWorldId), m_entityMap->size());
  this->root().logMap().set(Star::format("client_{}_sectors",m_subWorldId), Star::to_string(loadedSectors.size()));
  this->root().logMap().set(Star::format("client_{}_lua_mem",m_subWorldId), m_luaRoot->luaMemoryUsage());
  auto endPresentation = m_entityMap->forAllEntities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    auto& state = m_entityPresentation[entity->entityId()];
    if (state.instance.lock() != entity || state.epoch != entity->presentationEpoch()) {
      state.instance = entity;
      state.epoch = entity->presentationEpoch();
      state.position.reset(entity->position());
      state.continuous = false;
    } else {
      // Script-authored setPosition/translate may be continuous movement.
      // Only unusually large unmarked corrections snap; high-speed entities
      // retain an allowance derived from their actual velocity.
      float continuityDistance = 8.0f;
      if (auto mobile = as<MobileEntity>(entity))
        continuityDistance = std::max(continuityDistance, 4.0f * dt * mobile->movementController().velocity().magnitude());
      if (!state.position.finish(entity->position(), m_geometry, continuityDistance)) {
        entity->resetPresentation();
        state.epoch = entity->presentationEpoch();
        state.continuous = false;
      }
    }
    return {};
  });
  if (!endPresentation)
    return std::unexpected(std::move(endPresentation.error()));
  return {};
}

ConnectionId WorldClient::connection() const {
  // World methods requiring a connection are only used after world start.
  return *m_clientId;
}

WorldGeometry WorldClient::geometry() const {
  return m_geometry;
}

std::uint64_t WorldClient::currentStep() const {
  return m_currentStep;
}

MaterialId WorldClient::material(Vec2I const& pos, TileLayer layer) const {
  if (!inWorld())
    return NullMaterialId;
  return m_tileArray->tile(pos).material(layer);
}

MaterialHue WorldClient::materialHueShift(Vec2I const& position, TileLayer layer) const {
  if (!inWorld())
    return MaterialHue();
  auto const& tile = m_tileArray->tile(position);
  return layer == TileLayer::Foreground ? tile.foregroundHueShift : tile.backgroundHueShift;
}

ModId WorldClient::mod(Vec2I const& pos, TileLayer layer) const {
  if (!inWorld())
    return NoModId;
  return m_tileArray->tile(pos).mod(layer);
}

MaterialHue WorldClient::modHueShift(Vec2I const& position, TileLayer layer) const {
  if (!inWorld())
    return MaterialHue();
  auto const& tile = m_tileArray->tile(position);
  return layer == TileLayer::Foreground ? tile.foregroundModHueShift : tile.backgroundModHueShift;
}

MaterialColorVariant WorldClient::colorVariant(Vec2I const& position, TileLayer layer) const {
  if (!inWorld())
    return MaterialColorVariant();
  auto const& tile = m_tileArray->tile(position);
  return layer == TileLayer::Foreground ? tile.foregroundColorVariant : tile.backgroundColorVariant;
}

std::shared_ptr<Entity> WorldClient::entity(EntityId entityId) const {
  if (!inWorld())
    return {};

  return m_entityMap->entity(entityId);
}

std::expected<void, StarException> WorldClient::addEntity(std::shared_ptr<Entity> const& entity, EntityId entityId) {
  if (!entity || !inWorld())
    return {};
  if (entity->clientEntityMode() != ClientEntityMode::ClientSlaveOnly) {
    auto reserved = m_entityMap->reserveEntityId(entityId);
    if (!reserved)
      return std::unexpected(std::move(reserved.error()));
    auto initialized = entity->init(*this, *reserved, EntityMode::Master);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto added = m_entityMap->addEntity(entity);
    if (!added) {
      auto cleared = entity->uninit();
      if (!cleared)
        return std::unexpected(added.error().with_context(Star::format("Error uninitializing rejected entity: {}", cleared.error().what())));
      return std::unexpected(std::move(added.error()));
    }
    return notifyEntityCreate(entity);
  }
  auto factory = this->root().entityFactory();
  if (!factory)
    return std::unexpected(std::move(factory.error()));
  auto rules = m_clientState->netCompatibilityRules();
  auto store = (*factory)->netStoreEntity(entity, rules);
  if (!store)
    return std::unexpected(std::move(store.error()));
  auto state = entity->writeNetState(0, rules);
  if (!state)
    return std::unexpected(std::move(state.error()));
  m_outgoingPackets.push_back(
    std::make_shared<SpawnEntityPacket>(entity->entityType(), std::move(*store), std::move(state->first)));
  return {};
}

std::expected<TileDamageResult, StarException> WorldClient::damageTiles(List<Vec2I> const& pos, TileLayer layer, Vec2F const& sourcePosition, TileDamage const& tileDamage, std::optional<EntityId> sourceEntity) {
  if (!inWorld())
    return TileDamageResult::None;

  // Collect occupied tiles before separating protected tiles.
  List<Vec2I> occupied;
  for (auto const& position : pos) {
    auto isOccupied = tileIsOccupied(position, layer, true, false);
    if (!isOccupied)
      return std::unexpected(std::move(isOccupied.error()));
    if (*isOccupied)
      occupied.push_back(position);
  }
  List<Vec2I> toDing;
  List<Vec2I> toDamage;
  std::ranges::partition_copy(occupied, std::back_inserter(toDing), std::back_inserter(toDamage),
      [&self = *this](Vec2I position) { return self.isTileProtected(position); });

  if (toDamage.size() + toDing.size() == 0)
    return TileDamageResult::None;

  auto res = TileDamageResult::None;

  if (toDing.size()) {
    auto dingDamage = tileDamage;
    dingDamage.type = TileDamageType::Protected;
    m_outgoingPackets.push_back(std::make_shared<DamageTileGroupPacket>(std::move(toDing), layer, sourcePosition,
                                                                        dingDamage, std::optional<EntityId>()));
    res = TileDamageResult::Protected;
  }

  if (toDamage.size()) {
    m_outgoingPackets.push_back(
      std::make_shared<DamageTileGroupPacket>(std::move(toDamage), layer, sourcePosition, tileDamage, sourceEntity));
    res = TileDamageResult::Normal;
  }

  return res;
}

DungeonId WorldClient::dungeonId(Vec2I const& pos) const {
  if (!inWorld())
    return NoDungeonId;

  return m_tileArray->tile(pos).dungeonId;
}

std::expected<void, StarException> WorldClient::collectLiquid(List<Vec2I> const& tilePositions, LiquidId liquidId) {
  if (!inWorld()) {
    return {};
  }
  
  auto result197 = this->root().assets();
  if (!result197)
    return std::unexpected(std::move(result197.error()));
  auto result198 = (std::move(*result197))->json("/items/defaultParameters.config:liquidItems.bucketSize");
  if (!result198)
    return std::unexpected(std::move(result198.error()));
  auto result199 = (std::move(*result198)).toFloat();
  if (!result199)
    return std::unexpected(std::move(result199.error()));
  float bucketSize = (std::move(*result199));
  float nextUnit = bucketSize;
  List<Vec2I> maybeDrainTiles;
  
  for (auto& pos : tilePositions) {
    if (isTileProtected(pos))
      continue;
    auto& p = m_predictedTiles[pos];
    auto const& tile = m_tileArray->tile(pos);
    if ((p.liquid ? p.liquid->liquid : tile.liquid.liquid) == liquidId) {
      if (!p.liquid) {
        p.liquid.emplace(tile.liquid.liquid, tile.liquid.level);
      }
      auto& liquid = *p.liquid;
      if (liquid.level >= nextUnit) {
        liquid.take(nextUnit);
        nextUnit = bucketSize;
        
        for (std::size_t i = 0; i < maybeDrainTiles.size(); ++i) {
          m_predictedTiles[pos].liquid.emplace(EmptyLiquidId, 0.0f);
        }
        
        maybeDrainTiles.clear();
      }
      
      if (liquid.level > 0) {
        nextUnit -= liquid.level;
        maybeDrainTiles.push_back(pos);
      }
    }
  }

  m_outgoingPackets.push_back(std::make_shared<CollectLiquidPacket>(tilePositions, liquidId));
  return {};
}

std::expected<bool, StarException> WorldClient::waitForLighting(std::optional<std::reference_wrapper<WorldRenderData>> renderData) {
  std::lock_guard prepLocker(m_lightMapPrepMutex);
  std::lock_guard lightMapLocker(m_lightMapMutex);
  if (renderData && !m_lightMap.empty()) {
    for (auto& previewTile : m_previewTiles) {
      if (previewTile.updateLight) {
        Vec2I lightArrayPos(m_geometry.pdiff(previewTile.position[0], m_lightMinPosition[0]), previewTile.position[1] - m_lightMinPosition[1]);
        if (lightArrayPos[0] >= 0 && lightArrayPos[0] < static_cast<int>(m_lightMap.width()) && lightArrayPos[1] >= 0
            && lightArrayPos[1] < static_cast<int>(m_lightMap.height())) {
          auto result202 = m_lightMap.set(lightArrayPos[0], lightArrayPos[1], Color::v3bToFloat(previewTile.light));
          if (!result202)
            return std::unexpected(std::move(result202.error()));
        }
      }
    }
    renderData->get().lightMap = std::move(m_lightMap);
    renderData->get().lightMinPosition = m_lightMinPosition;
    return true;
  }
  return false;
}

WorldClient::BroadcastCallback& WorldClient::broadcastCallback() {
  return m_broadcastCallback;
}

bool WorldClient::isTileProtected(Vec2I const& pos) const {
  if (!inWorld()) {
    return true;
  }
  
  auto const& tile = m_tileArray->tile(pos);
  return m_protectedDungeonIds.contains(tile.dungeonId);
}

void WorldClient::setTileProtection(DungeonId dungeonId, bool isProtected) {
  if (isProtected) {
    m_protectedDungeonIds.add(dungeonId);
  } else {
    m_protectedDungeonIds.remove(dungeonId);
  }
}

std::expected<void, StarException> WorldClient::queueUpdatePackets(bool sendEntityUpdates) {
  auto& root = this->root();
  auto result203 = root.assets();
  if (!result203)
    return std::unexpected(std::move(result203.error()));
  auto assets = (std::move(*result203));
  auto result204 = root.entityFactory();
  if (!result204)
    return std::unexpected(std::move(result204.error()));
  auto entityFactory = (std::move(*result204));

  m_outgoingPackets.push_back(std::make_shared<StepUpdatePacket>(m_currentTime));

  {
    auto result205 = m_clientConfig.getInt("worldClientStateUpdateDelta");
    if (!result205)
      return std::unexpected(std::move(result205.error()));
    if (m_currentStep % (std::move(*result205)) == 0) {
      auto result206 = m_clientState->writeDelta();
      if (!result206)
        return std::unexpected(std::move(result206.error()));
      m_outgoingPackets.push_back(std::make_shared<WorldClientStateUpdatePacket>((std::move(*result206))));
    }
  }
  
  auto result208 = m_entityMap->forAllEntities([&self = *this](std::shared_ptr<Entity> const& entity)  -> std::expected<void, StarException> {
      auto result207 = self.notifyEntityCreate(entity);
      if (!result207)
        return std::unexpected(std::move(result207.error()));
      return {};
    });
  if (!result208)
    return std::unexpected(std::move(result208.error()));
  
  if (sendEntityUpdates) {
    auto entityUpdateSet = std::make_shared<EntityUpdateSetPacket>();
    entityUpdateSet->forConnection = *m_clientId;
    auto netRules = m_clientState->netCompatibilityRules();
    auto result210 = m_entityMap->forAllEntities(
      [&, &self = *this](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
        if (auto version = self.m_masterEntitiesNetVersion.find_ref(entity->entityId())) {
          auto result209 = entity->writeNetState(version->get(), netRules);
          if (!result209)
            return std::unexpected(std::move(result209.error()));
          auto updateAndVersion = (std::move(*result209));
          if (!updateAndVersion.first.empty()) {
            entityUpdateSet->deltas[entity->entityId()] = std::move(updateAndVersion.first);
          }
          version->get() = updateAndVersion.second;
        }
        return {};
      });
    if (!result210)
      return std::unexpected(std::move(result210.error()));
    m_outgoingPackets.push_back(std::move(entityUpdateSet));
  }
  
  for (auto& remoteHitRequest : m_damageManager->pullRemoteHitRequests()) {
    m_outgoingPackets.push_back(std::make_shared<HitRequestPacket>(std::move(remoteHitRequest)));
  }
  for (auto& remoteDamageRequest : m_damageManager->pullRemoteDamageRequests()) {
    m_outgoingPackets.push_back(std::make_shared<DamageRequestPacket>(std::move(remoteDamageRequest)));
  }
  for (auto& remoteDamageNotification : m_damageManager->pullRemoteDamageNotifications()) {
    m_outgoingPackets.push_back(std::make_shared<DamageNotificationPacket>(std::move(remoteDamageNotification)));
  }
  return {};
}

std::expected<void, StarException> WorldClient::handleDamageNotifications() {
  if (!inWorld()) {
    return {};
  }

  auto renderParticle = [&self = *this](Vec2F position, float amount,
                                        String const& damageNumberParticleKind) -> std::expected<void, StarException> {
    int displayValue = static_cast<int>(std::ceil(amount - 0.1f));
    if (displayValue <= 0) {
      return {};
    }
    auto result211 = self.root().particleDatabase();
    if (!result211)
      return std::unexpected(std::move(result211.error()));
    auto result212 = (std::move(*result211))->particle(damageNumberParticleKind);
    if (!result212)
      return std::unexpected(std::move(result212.error()));
    Particle particle = (std::move(*result212));
    particle.position += position;
    auto result213 = particle.string.replace("$dmg$", Star::to_string(displayValue));
    if (!result213)
      return std::unexpected(std::move(result213.error()));
    particle.string = (std::move(*result213));
    self.m_particles->add(particle);
    return {};
  };

  for (auto it = m_damageNumbers.begin(); it != m_damageNumbers.end();) {
    auto const& entry = *it;
    if (std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()) - entry.second.timestamp > m_damageNotificationBatchDuration) {
      auto result214 = renderParticle(entry.second.position, entry.second.amount, entry.first.damageNumberParticleKind);
      if (!result214)
        return std::unexpected(std::move(result214.error()));
      it = m_damageNumbers.erase(it);
    } else {
      ++it;
    }
  }
  
  if (m_headless) {
    m_damageManager->pullPendingNotifications();
    return {};
  }
  for (auto const& damageNotification : m_damageManager->pullPendingNotifications()) {
    auto result215 = this->root().damageDatabase();
    if (!result215)
      return std::unexpected(std::move(result215.error()));
    auto damageDatabase = (std::move(*result215));
    auto result216 = damageDatabase->damageKind(damageNotification.damageSourceKind);
    if (!result216)
      return std::unexpected(std::move(result216.error()));
    DamageKind const& damageKind = result216->get();
    auto result217 = damageDatabase->elementalType(damageKind.elementalType);
    if (!result217)
      return std::unexpected(std::move(result217.error()));
    ElementalType const& elementalType = result217->get();
    
    auto result218 = elementalType.damageNumberParticles.get(damageNotification.hitType);
    if (!result218)
      return std::unexpected(std::move(result218.error()));
    auto damageNumberParticleKind = result218->get();
    auto damageNumberKey = DamageNumberKey{.damageNumberParticleKind = damageNumberParticleKind,
                                           .sourceEntityId = damageNotification.sourceEntityId,
                                           .targetEntityId = damageNotification.targetEntityId};

    DamageNumber number;
    if (m_damageNumbers.contains(damageNumberKey)) {
      auto result219 = m_damageNumbers.extract_value(damageNumberKey);
      if (!result219)
        return std::unexpected(std::move(result219.error()));
      number = (std::move(*result219));
      
      if (damageNotification.hitType == HitType::Kill) {
        auto result220 = renderParticle(damageNotification.position,
                    damageNotification.damageDealt + number.amount,
                    damageNumberKey.damageNumberParticleKind);
        if (!result220)
          return std::unexpected(std::move(result220.error()));
      }
    } else {
      if (damageNotification.hitType == HitType::Kill) {
        auto result221 = renderParticle(damageNotification.position, damageNotification.damageDealt, damageNumberParticleKind);
        if (!result221)
          return std::unexpected(std::move(result221.error()));
      }
      number.amount = 0;
      number.timestamp = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
    }
    
    if (damageNotification.hitType != HitType::Kill) {
      number.position = damageNotification.position;
      number.amount += damageNotification.damageDealt;
      m_damageNumbers[damageNumberKey] = number;
    }
    
    String material = damageNotification.targetMaterialKind;
    if (!material.empty() && damageKind.effects.contains(material)) {
      // default to normal hit
      auto result222 = damageKind.effects.get(material);
      if (!result222)
        return std::unexpected(std::move(result222.error()));
      HitType effectHitType = result222->get().contains(damageNotification.hitType) ? damageNotification.hitType : HitType::Hit;
      auto result223 = damageKind.effects.get(material);
      if (!result223)
        return std::unexpected(std::move(result223.error()));
      auto result224 = result223->get().get(effectHitType);
      if (!result224)
        return std::unexpected(std::move(result224.error()));
      auto result225 = soundsFromDefinition(this->root(), result224->get().sounds, damageNotification.position);
      if (!result225)
        return std::unexpected(std::move(result225.error()));
      m_samples.append_range((std::move(*result225)));
      auto result226 = damageKind.effects.get(material);
      if (!result226)
        return std::unexpected(std::move(result226.error()));
      auto result227 = result226->get().get(effectHitType);
      if (!result227)
        return std::unexpected(std::move(result227.error()));
      auto result228 = particlesFromDefinition(this->root(), result227->get().particles, damageNotification.position);
      if (!result228)
        return std::unexpected(std::move(result228.error()));
      auto hitParticles = (std::move(*result228));
      std::optional<std::reference_wrapper<List<Directives> const>> directives;
      if (auto& worldTemplate = m_worldTemplate) {
        if (const auto& parameters = worldTemplate->worldParameters()) {
          if (auto& globalDirectives = parameters->globalDirectives) {
            directives = std::cref(*globalDirectives);
          }
        }
      }
      if (directives) {
        int directiveIndex = static_cast<unsigned>(damageNotification.targetEntityId) % directives->get().size();
        for (auto& p : hitParticles) {
          p.directives.append(directives->get()[directiveIndex]);
        }
      }
      m_particles->addParticles(std::move(hitParticles));
    }
  }
  return {};
}

std::expected<void, StarException> WorldClient::sparkDamagedBlocks() {
  if (!inWorld()) {
    return {};
  }
  
  auto result229 = this->root().materialDatabase();
  if (!result229)
    return std::unexpected(std::move(result229.error()));
  auto materialDatabase = (std::move(*result229));
  
  for (auto pos : m_damagedBlocks.values()) {
    if (auto tile = m_tileArray->modifyTile(pos)) {
      if (tile->get().backgroundDamage.healthy() && tile->get().foregroundDamage.healthy()) {
        m_damagedBlocks.remove(pos);
      }
      
      if (isRealMaterial(tile->get().foreground) && tile->get().foregroundDamage.damageEffectPercentage() - this->root().random().randf() > 0.0f
                && (this->root().random().randf() < m_blockDamageParticleProbability)) {
        auto particle = m_blockDamageParticle;
        auto result230 = materialDatabase->materialParticleColor(tile->get().foreground, tile->get().foregroundHueShift);
        if (!result230)
          return std::unexpected(std::move(result230.error()));
        particle.color = (std::move(*result230));
        
        if (isTileProtected(pos))
          particle = m_blockDingParticle;
        
        particle.position += centerOfTile(pos);
        particle.velocity = particle.velocity.magnitude()
            * vnorm(m_geometry.diff(tile->get().foregroundDamage.sourcePosition(), particle.position));
        particle.applyVariance(this->root(), m_blockDamageParticleVariance);
        m_particles->add(particle);
      }
      
      if (isRealMaterial(tile->get().background) && tile->get().backgroundDamage.damageEffectPercentage() - this->root().random().randf() > 0.0f
                && (this->root().random().randf() < m_blockDamageParticleProbability)) {
        auto particle = m_blockDamageParticle;
        auto result232 = materialDatabase->materialParticleColor(tile->get().background, tile->get().backgroundHueShift);
        if (!result232)
          return std::unexpected(std::move(result232.error()));
        particle.color = (std::move(*result232));
        
        if (isTileProtected(pos))
          particle = m_blockDingParticle;
        
        particle.position += centerOfTile(pos);
        particle.velocity = particle.velocity.magnitude()
            * vnorm(m_geometry.diff(tile->get().backgroundDamage.sourcePosition(), particle.position));
        particle.applyVariance(this->root(), m_blockDamageParticleVariance);
        m_particles->add(particle);
      }
    }
  }
  return {};
}

std::expected<std::shared_ptr<InteractiveEntity>, StarException> WorldClient::getInteractiveInRange(Vec2F const& targetPosition, Vec2F const& sourcePosition, float maxRange) const {
  if (!inWorld()) {
    return {};
  }
  return WorldImpl::getInteractiveInRange(m_geometry, m_entityMap, targetPosition, sourcePosition, maxRange);
}

std::expected<bool, StarException> WorldClient::canReachEntity(Vec2F const& position, float radius, EntityId targetEntity, bool preferInteractive) const {
  if (!inWorld()) {
    return false;
  }
  return WorldImpl::canReachEntity(m_geometry, m_tileArray, m_entityMap, position, radius, targetEntity, preferInteractive);
}

std::expected<RpcPromise<InteractAction>, StarException> WorldClient::interact(InteractRequest const& request) {
  if (!inWorld()) {
    return RpcPromise<InteractAction>::createFailed("not initialized in world");
  }
  
  if (auto targetEntity = m_entityMap->entity(request.targetId)) {
    if (targetEntity->isMaster()) {
      // client-side-master entities need to be handled here rather than over network
      if (auto interactiveTarget = as<InteractiveEntity>(targetEntity)) {
        auto action = interactiveTarget->interact(request);
        if (!action) {
          return std::unexpected(std::move(action.error()));
        }
        return RpcPromise<InteractAction>::createFulfilled(std::move(*action));
      } else {
        return RpcPromise<InteractAction>::createFulfilled(InteractAction());
      }
    }
  }
  
  auto pair = RpcPromise<InteractAction>::createPair();
  Uuid requestId(this->root().random());
  m_entityInteractionResponses[requestId] = pair.second;
  m_outgoingPackets.push_back(std::make_shared<EntityInteractPacket>(request, requestId));

  return pair.first;
}

std::expected<void, StarException> gatherClientLightingTiles(CellularLightingCalculator& calculator,
                                                             ClientTileSectorArray& tiles, WorkerPool& workers,
                                                             MaterialDatabase const& materials,
                                                             LiquidsDatabase const& liquids,
                                                             Vec3F const& environmentLight, float undergroundLevel) {
  // Each column is guaranteed to be no larger than the sector size.
  auto gatherColumn = [&](Vec2I const& pos, std::span<ClientTile const> column) -> std::expected<void, StarException> {
    std::size_t baseIndex = calculator.baseIndexFor(pos);
    for (std::size_t y = 0; y < column.size(); ++y) {
      auto& tile = column[y];
      Vec3F light;
      if (tile.foreground != EmptyMaterialId || tile.foregroundMod != NoModId)
        light += materials.radiantLight(tile.foreground, tile.foregroundMod);
      if (tile.liquid.liquid != EmptyLiquidId && tile.liquid.level != 0.0f)
        light += liquids.radiantLight(tile.liquid);
      if (tile.foregroundLightTransparent) {
        if (tile.background != EmptyMaterialId || tile.backgroundMod != NoModId)
          light += materials.radiantLight(tile.background, tile.backgroundMod);
        if (tile.backgroundLightTransparent && pos[1] + y > undergroundLevel)
          light += environmentLight;
      }
      calculator.setCellIndex(baseIndex + y, light, !tile.foregroundLightTransparent);
    }
    return {};
  };

  RectI region = calculator.calculationRegion();
  int worldWidth = static_cast<int>(tiles.size()[0]);
  int worldHeight = static_cast<int>(tiles.size()[1]);
  int gatheredRows = std::clamp(region.yMax(), 0, worldHeight) - std::clamp(region.yMin(), 0, worldHeight);
  // Real-asset gather measurements put small clipped regions below the useful
  // parallel grain. Keep the larger/interior cases on their existing workers.
  constexpr std::int64_t CallerGatherCellLimit = 12 * 1024;
  bool useCaller = static_cast<std::int64_t>(region.width()) * gatheredRows <= CallerGatherCellLimit;
  if (region.width() < worldWidth && region.xMin() >= -worldWidth
      && static_cast<std::int64_t>(region.xMax()) <= 2 * static_cast<std::int64_t>(worldWidth)) {
    if (useCaller)
      return tiles.tileEvalColumns(region, gatherColumn);
    return tiles.tileEvalColumnsParallel(workers, region, gatherColumn);
  }

  // Whole-world tile traversal deduplicates wrapped x coordinates. Lighting
  // instead needs every image in its unwrapped calculation region, including
  // the configured border around large viewports and small worlds.
  for (int destinationX = region.xMin(); destinationX < region.xMax();) {
    int sourceX = pmod<int>(destinationX, worldWidth);
    int stripWidth = std::min(region.xMax() - destinationX, worldWidth - sourceX);
    RectI strip(sourceX, region.yMin(), sourceX + stripWidth, region.yMax());
    auto gatherStrip = [&](Vec2I const& pos, std::span<ClientTile const> column) -> std::expected<void, StarException> {
      return gatherColumn({destinationX + (pos[0] - sourceX), pos[1]}, column);
    };
    auto gathered = useCaller ? tiles.tileEvalColumns(strip, gatherStrip)
                              : tiles.tileEvalColumnsParallel(workers, strip, gatherStrip);
    // The column API drains all borrowed jobs even on failure before either
    // the translation captures or the world snapshot can be released.
    if (!gathered)
      return std::unexpected(std::move(gathered.error()));
    destinationX += stripWidth;
  }
  return {};
}

std::expected<void, StarException> WorldClient::lightingTileGather() {
  auto start = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
  Vec3F environmentLight = m_sky->environmentLight().toRgbF();
  float undergroundLevel = m_worldTemplate->undergroundLevel();
  auto result234 = this->root().liquidsDatabase();
  if (!result234)
    return std::unexpected(std::move(result234.error()));
  auto liquidsDatabase = (std::move(*result234));
  auto result235 = this->root().materialDatabase();
  if (!result235)
    return std::unexpected(std::move(result235.error()));
  auto materialDatabase = (std::move(*result235));

  auto result236 = this->root().workers();
  if (!result236)
    return std::unexpected(std::move(result236.error()));
  auto result237 = gatherClientLightingTiles(m_lightingCalculator, *m_tileArray, result236->get(), *materialDatabase,
                                             *liquidsDatabase, environmentLight, undergroundLevel);
  if (!result237)
    return std::unexpected(std::move(result237.error()));
  this->root().logMap().set("client_render_world_async_light_gather", Star::format("{:05d}\u00b5s", (std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500)) - start).count()));
  return {};
}

std::expected<void, StarException> WorldClient::lightingCalc() {
  auto prepWaitStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
  std::unique_lock prepLocker(m_lightMapPrepMutex);
  auto prepStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
  if (!m_pendingLightReady.load()) {
    return {};
  }
  m_pendingLightReady = false;
  RectI lightRange = m_pendingLightRange;
  List<LightSource> lights = std::move(m_pendingLights);
  List<std::pair<Vec2F, Vec3F>> particleLights = std::move(m_pendingParticleLights);
  auto& root = this->root();
  auto result238 = root.configuration();
  if (!result238)
    return std::unexpected(std::move(result238.error()));
  auto configuration = (std::move(*result238));
  auto result239 = configuration->get("newLighting");
  if (!result239)
    return std::unexpected(std::move(result239.error()));
  auto result240 = (std::move(*result239)).optBool();
  if (!result240)
    return std::unexpected(std::move(result240.error()));
  bool newLighting = (std::move(*result240)).value_or(true);
  auto result241 = configuration->get("monochromeLighting");
  if (!result241)
    return std::unexpected(std::move(result241.error()));
  auto result242 = (std::move(*result241)).toBool();
  if (!result242)
    return std::unexpected(std::move(result242.error()));
  bool monochrome = (std::move(*result242));
  auto result243 = root.assets();
  if (!result243)
    return std::unexpected(std::move(result243.error()));
  auto result244 = (std::move(*result243))->json("/lighting.config:lighting");
  if (!result244)
    return std::unexpected(std::move(result244.error()));
  auto result245 = (std::move(*result244)).set("pointAdditive", newLighting);
  if (!result245)
    return std::unexpected(std::move(result245.error()));
  auto result246 = m_lightingCalculator.setParameters((std::move(*result245)));
  if (!result246)
    return std::unexpected(std::move(result246.error()));
  auto result247 = m_lightingCalculator.setMonochrome(monochrome);
  if (!result247)
    return std::unexpected(std::move(result247.error()));
  m_lightingCalculator.begin(lightRange);
  auto result248 = lightingTileGather();
  if (!result248)
    return std::unexpected(std::move(result248.error()));

  auto prepEnd = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
  prepLocker.unlock();
  
  Vec2I regionMin = m_lightingCalculator.calculationRegion().min();
  auto resolvePosition = [&, &self = *this](Vec2F const& pos) -> Vec2F {
    float xFloor = std::floor(pos[0]);
    float xOffset =
      static_cast<float>(self.m_geometry.pdiff(static_cast<int>(xFloor), regionMin[0])) + (pos[0] - xFloor);
    return Vec2F(regionMin[0] + xOffset, pos[1]);
  };
  
  for (auto const& light : lights) {
    Vec2F position = resolvePosition(light.position);
    if (light.type == LightType::Spread) {
      m_lightingCalculator.addSpreadLight(position, light.color);
    } else {
      if (light.type == LightType::PointAsSpread) {
        if (!newLighting) {
          m_lightingCalculator.addSpreadLight(position, light.color);
        } else {
          // hybrid (used for auto-converted object lights) - 85% spread, 15% point (* .15 is applied in the calculation code)
          m_lightingCalculator.addSpreadLight(position, light.color * 0.85f);
          m_lightingCalculator.addPointLight(position, light.color, light.pointBeam, light.beamAngle, light.beamAmbience, true);
        }
      } else {
        m_lightingCalculator.addPointLight(position, light.color, light.pointBeam, light.beamAngle, light.beamAmbience);
      }
    }
  }
  
  for (auto const& lightPair : particleLights) {
    Vec2F position = resolvePosition(lightPair.first);
    m_lightingCalculator.addSpreadLight(position, lightPair.second);
  }
  
  auto result249 = m_lightingCalculator.calculate(m_pendingLightMap);
  if (!result249)
    return std::unexpected(std::move(result249.error()));
  auto propagationEnd = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::microseconds> publicationStart;
  {
    std::lock_guard mapLocker(m_lightMapMutex);
    publicationStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
    m_lightMinPosition = lightRange.min();
    m_lightMap = std::move(m_pendingLightMap);
  }
  auto publicationEnd = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
  root.logMap().set("client_render_world_async_light_prep_wait", Star::format("{:05d}\u00b5s", (prepStart - prepWaitStart).count()));
  root.logMap().set("client_render_world_async_light_prep", Star::format("{:05d}\u00b5s", (prepEnd - prepStart).count()));
  root.logMap().set("client_render_world_async_light_propagation", Star::format("{:05d}\u00b5s", (propagationEnd - prepEnd).count()));
  root.logMap().set("client_render_world_async_light_publication_wait",
                    Star::format("{:05d}\u00b5s", (publicationStart - propagationEnd).count()));
  root.logMap().set("client_render_world_async_light_publication",
                    Star::format("{:05d}\u00b5s", (publicationEnd - publicationStart).count()));
  return {};
}

std::expected<void, StarException> WorldClient::lightingMain(std::stop_token stopToken) {
  std::unique_lock condLocker(m_lightingMutex);
  while (true) {
    if (stopToken.stop_requested())
      return {};
    m_lightingCond.wait(condLocker);
    if (stopToken.stop_requested()) {
      return {};
    }
    
    auto start = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
    auto result250 = lightingCalc();
    if (!result250)
      return std::unexpected(std::move(result250.error()));
    this->root().logMap().set("client_render_world_async_light_calc", Star::format("{:05d}\u00b5s", (std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500)) - start).count()));
  }
  return {};
}

std::expected<void, StarException> WorldClient::initializeWorld(WorldStartPacket const& startPacket) {
    auto result251 = clearWorld();
    if (!result251)
      return std::unexpected(std::move(result251.error()));
    m_outgoingPackets.push_back(std::make_shared<WorldStartAcknowledgePacket>());

    auto result252 = this->root().assets();
    if (!result252)
      return std::unexpected(std::move(result252.error()));
    auto assets = (std::move(*result252));
    if (startPacket.localInterpolationMode) {
      auto result253 = m_clientConfig.query("interpolationSettings.local");
      if (!result253)
        return std::unexpected(std::move(result253.error()));
      auto result254 = InterpolationTracker::create(this->root(), (std::move(*result253)));
      if (!result254)
        return std::unexpected(std::move(result254.error()));
      m_interpolationTracker = (std::move(*result254));
    } else {
      auto result255 = m_clientConfig.query("interpolationSettings.normal");
      if (!result255)
        return std::unexpected(std::move(result255.error()));
      auto result256 = InterpolationTracker::create(this->root(), (std::move(*result255)));
      if (!result256)
        return std::unexpected(std::move(result256.error()));
      m_interpolationTracker = (std::move(*result256));
    }
    
    m_entityUpdateTimer = GameTimer(m_interpolationTracker.entityUpdateDelta());
    
    m_clientId = startPacket.clientId;
    if (!m_headless) {
      m_mainPlayer->clientContext()->setConnectionId(startPacket.clientId);
    }
    auto result257 = connectionEntitySpace(startPacket.clientId);
    if (!result257)
      return std::unexpected(std::move(result257.error()));
    auto entitySpace = (std::move(*result257));
    auto result258 = WorldTemplate::create(this->root(), startPacket.templateData);
    if (!result258)
      return std::unexpected(std::move(result258.error()));
    m_worldTemplate = (std::move(*result258));
    m_entityMap = std::make_shared<EntityMap>(m_worldTemplate->size(), entitySpace.first, entitySpace.second);
    m_tileArray = std::make_shared<ClientTileSectorArray>(m_worldTemplate->size());
    m_tileGetterFunction = [&self = *this, tile = ClientTile()](Vec2I pos) mutable -> std::expected<std::reference_wrapper<ClientTile const>, StarException> {
      if (!self.m_predictedTiles.empty()) {
        if (auto p = self.m_predictedTiles.find_ref(pos)) {
          p->get().apply(tile = self.m_tileArray->tile(pos));
          if (p->get().liquid) {
            if (p->get().liquid->liquid == tile.liquid.liquid) {
              tile.liquid.level += p->get().liquid->level;
            } else {
              tile.liquid.liquid = p->get().liquid->liquid;
              tile.liquid.level = p->get().liquid->level;
            }
          }
          return std::cref(tile);
        }
      }
      return std::cref(self.m_tileArray->tile(pos));
    };
    m_damageManager = std::make_shared<DamageManager>(*this, startPacket.clientId);
    m_playerStart = startPacket.playerRespawn;
    m_respawnInWorld = startPacket.respawnInWorld;
    auto result259 = startPacket.worldProperties.optObject();
    if (!result259)
      return std::unexpected(std::move(result259.error()));
    m_worldProperties = (std::move(*result259)).value_or(JsonObject{});
    m_dungeonIdGravity = startPacket.dungeonIdGravity;
    m_dungeonIdBreathable = startPacket.dungeonIdBreathable;
    m_protectedDungeonIds = startPacket.protectedDungeonIds;
    
    m_geometry = WorldGeometry(m_worldTemplate->size());
    
    m_particles = std::make_shared<ParticleManager>(this->root(), m_geometry, m_tileArray);
    m_particles->setUndergroundLevel(m_worldTemplate->undergroundLevel());
    
    auto result260 = setupForceRegions();
    if (!result260)
      return std::unexpected(std::move(result260.error()));
    
    auto result261 = Sky::create(this->root());
    if (!result261)
      return std::unexpected(std::move(result261.error()));
    m_sky = (std::move(*result261));
    auto result262 = m_sky->readUpdate(startPacket.skyData, m_clientState->netCompatibilityRules());
    if (!result262)
      return std::unexpected(std::move(result262.error()));
    
    m_weather->setup(m_geometry, [&self = *this](Vec2I const& pos)  -> std::expected<bool, StarException> {
      auto const& tile = self.m_tileArray->tile(pos);
      return !isRealMaterial(tile.background) && !isSolidColliding(tile.getCollision());
    });
    {
      auto result263 = m_worldTemplate->weatherLayerAt(Vec2I::floor(startPacket.playerStart));
      if (!result263)
        return std::unexpected(std::move(result263.error()));
      if (auto layer = (std::move(*result263))) {
        m_weatherDomain = layer->get().domain;
      }
    }
    auto result264 = m_weather->readUpdate(startPacket.weatherData, m_clientState->netCompatibilityRules());
    if (!result264)
      return std::unexpected(std::move(result264.error()));
    
    auto result265 = this->root().configuration();
    if (!result265)
      return std::unexpected(std::move(result265.error()));
    auto result266 = (std::move(*result265))->get("monochromeLighting");
    if (!result266)
      return std::unexpected(std::move(result266.error()));
    auto result267 = (std::move(*result266)).toBool();
    if (!result267)
      return std::unexpected(std::move(result267.error()));
    auto result268 = m_lightingCalculator.setMonochrome((std::move(*result267)));
    if (!result268)
      return std::unexpected(std::move(result268.error()));
    auto result269 = assets->json("/lighting.config:lighting");
    if (!result269)
      return std::unexpected(std::move(result269.error()));
    auto result270 = m_lightingCalculator.setParameters((std::move(*result269)));
    if (!result270)
      return std::unexpected(std::move(result270.error()));
    auto result271 = assets->json("/lighting.config:intensity");
    if (!result271)
      return std::unexpected(std::move(result271.error()));
    auto result272 = m_lightIntensityCalculator.setParameters((std::move(*result271)));
    if (!result272)
      return std::unexpected(std::move(result272.error()));
    
    m_inWorld = true;
    if (!m_headless) {
      if (!m_mainPlayer->isDead()) {
        auto result273 = m_entityMap->reserveEntityId();
        if (!result273)
          return std::unexpected(std::move(result273.error()));
        auto result274 = m_mainPlayer->init(*this, (std::move(*result273)), EntityMode::Master);
        if (!result274)
          return std::unexpected(std::move(result274.error()));
        auto result275 = m_entityMap->addEntity(m_mainPlayer);
        if (!result275) {
          auto cleared = m_mainPlayer->uninit();
          if (!cleared)
            return std::unexpected(result275.error().with_context(Star::format("Error uninitializing rejected entity: {}", cleared.error().what())));
          return std::unexpected(std::move(result275.error()));
        }
      }
      auto result276 = m_mainPlayer->moveTo(startPacket.playerStart);
      if (!result276)
        return std::unexpected(std::move(result276.error()));
      if (const auto& parameters = m_worldTemplate->worldParameters()) {
        auto result277 = m_mainPlayer->overrideTech(parameters->overrideTech);
        if (!result277)
          return std::unexpected(std::move(result277.error()));
      } else {
        auto result278 = m_mainPlayer->overrideTech({});
        if (!result278)
          return std::unexpected(std::move(result278.error()));
      }
      
      // Auto reposition the client window on the player when the main player
      // changes position.
      centerClientWindowOnPlayer();
    }
    // script contexts for both kinds of worlds
    {
      auto result279 = m_clientConfig.getObject("worldScriptContexts");
      if (!result279)
        return std::unexpected(std::move(result279.error()));
      for (auto&& p : (std::move(*result279))) {
        auto scriptComponent = std::make_shared<ScriptComponent>(this->root());
        auto result280 = p.second.toArray();
        if (!result280)
          return std::unexpected(std::move(result280.error()));
        auto result281 = jsonToStringList((std::move(*result280)));
        if (!result281)
          return std::unexpected(std::move(result281.error()));
        auto result282 = scriptComponent->setScripts((std::move(*result281)));
        if (!result282)
          return std::unexpected(std::move(result282.error()));
        
        m_scriptContexts.set(p.first, scriptComponent);
        auto result283 = recoverLuaScriptError(scriptComponent->init(*this));
        if (!result283)
          return std::unexpected(std::move(result283.error()));
      }
    }
    {
      auto result284 = m_clientConfig.getObject(m_headless ? std::string_view("subWorldScriptContexts") : std::string_view("mainWorldScriptContexts"));
      if (!result284)
        return std::unexpected(std::move(result284.error()));
      for (auto&& p : (std::move(*result284))) {
        if (m_scriptContexts.contains(p.first)) {
          auto result285 = this->root().logger().error("World script context {} is already defined!", p.first);
          if (!result285)
            return std::unexpected(std::move(result285.error()));
          continue;
        }
        auto scriptComponent = std::make_shared<ScriptComponent>(this->root());
        auto result286 = p.second.toArray();
        if (!result286)
          return std::unexpected(std::move(result286.error()));
        auto result287 = jsonToStringList((std::move(*result286)));
        if (!result287)
          return std::unexpected(std::move(result287.error()));
        auto result288 = scriptComponent->setScripts((std::move(*result287)));
        if (!result288)
          return std::unexpected(std::move(result288.error()));
        
        m_scriptContexts.set(p.first, scriptComponent);
        auto result289 = recoverLuaScriptError(scriptComponent->init(*this));
        if (!result289)
          return std::unexpected(std::move(result289.error()));
      }
    }
    return {};
}

std::expected<void, StarException> WorldClient::initWorld(WorldStartPacket const& startPacket) {
  auto initialized = initializeWorld(startPacket);
  if (!initialized) {
    auto cleared = clearWorld();
    if (!cleared)
      return std::unexpected(initialized.error().with_context(Star::format("Error rolling back world initialization: {}", cleared.error().what())));
  }
  return initialized;
}

std::expected<void, StarException> WorldClient::clearWorld() {
  resetPresentation();
  m_hiddenEntityAudio.clear();
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (error)
        *error = error->with_context(Star::format("Additional client world cleanup failure: {}", result.error().what()));
      else
        error.emplace(std::move(result.error()));
    }
  };
  for (auto&& p : m_scriptContexts)
    remember(recoverLuaScriptError(p.second->invoke("preUninit")));
  if (m_entityMap) {
    for (auto entityId : m_entityMap->entityIds()) {
      auto entity = m_entityMap->entity(entityId);
      remember(removeEntity(entityId, false));
      // A failed final net snapshot must not leave an initialized entity attached.
      if (m_entityMap->entity(entityId)) {
        m_entityMap->removeEntity(entityId);
        remember(entity->uninit());
      }
    }
  }
  for (auto&& p : m_scriptContexts)
    remember(recoverLuaScriptError(p.second->uninit()));
  m_scriptContexts.clear();
  remember(waitForLighting(std::nullopt));

  m_currentStep = 0;
  m_currentTime = 0;
  m_inWorld = false;
  m_clientId.reset();

  m_interpolationTracker = InterpolationTracker();

  m_masterEntitiesNetVersion.clear();
  m_outgoingPackets.clear();

  m_pingTime.reset();

  m_entityMap.reset();
  m_worldTemplate.reset();
  m_worldProperties.clear();

  m_tileArray.reset();

  m_damageManager.reset();

  m_particles.reset();

  m_sky.reset();

  m_currentParallax.reset();
  m_nextParallax.reset();
  m_weatherParallaxAsset.reset();
  m_weatherParallaxBiome.reset();
  m_weatherParallax.reset();
  m_weatherDomain.reset();
  m_lastParallaxWindEpoch.reset();
  m_parallaxWindDirectionTime = 0.0;
  m_parallaxWindMagnitudeTime = 0.0;
  m_parallaxSignedWindTime = 0.0;
  if (m_weather)
    remember(m_weather->clear());
  m_parallaxFadeTimer.setDone();

  if (m_clientState)
    m_clientState->reset();
  m_ambientSounds.cancelAll();
  m_musicTrack.cancelAll();
  m_musicTrack.setVolume(1, 0, 0);
  m_altMusicTrack.cancelAll();
  m_altMusicTrack.setVolume(0, 0, 0);
  m_altMusicActive = false;

  if (m_spaceSound) {
    m_spaceSound->stop();
    m_spaceSound = {};
  }

  for (auto&& response : std::exchange(m_entityMessageResponses, {}))
    remember(response.second.cancel("Client world closed"));
  for (auto&& responses : std::exchange(m_findUniqueEntityResponses, {})) {
    for (auto& response : responses.second)
      remember(response.cancel("Client world closed"));
  }
  for (auto&& response : std::exchange(m_entityInteractionResponses, {}))
    remember(response.second.cancel("Client world closed"));
  m_entityMessagePromises = {};

  m_forceRegions.clear();
  
  m_expiryTimer.reset();
  m_requestedDestroy = false;
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> WorldClient::tryGiveMainPlayerItem(std::shared_ptr<Item> item, bool silent) {
  {
    auto result290 = m_mainPlayer->pickupItems(item, silent);
    if (!result290)
      return std::unexpected(std::move(result290.error()));
    if (auto spill = (std::move(*result290))) {
      auto result291 = ItemDrop::createRandomizedDrop(this->root(), spill->descriptor(), m_mainPlayer->position());
      if (!result291)
        return std::unexpected(std::move(result291.error()));
      auto result292 = addEntity((std::move(*result291)), NullEntityId);
      if (!result292)
        return std::unexpected(std::move(result292.error()));
    }
  }
  return {};
}

std::expected<void, StarException> WorldClient::notifyEntityCreate(std::shared_ptr<Entity> const& entity) {
  if (entity->isMaster() && !m_masterEntitiesNetVersion.contains(entity->entityId())) {
    // Server was unaware of this entity until now
    auto netRules = m_clientState->netCompatibilityRules();
    auto result293 = entity->writeNetState(0, netRules);
    if (!result293)
      return std::unexpected(std::move(result293.error()));
    auto firstNetState = (std::move(*result293));
    m_masterEntitiesNetVersion[entity->entityId()] = firstNetState.second;
    auto result294 = this->root().entityFactory();
    if (!result294)
      return std::unexpected(std::move(result294.error()));
    auto result295 = (std::move(*result294))->netStoreEntity(entity, netRules);
    if (!result295)
      return std::unexpected(std::move(result295.error()));
    m_outgoingPackets.push_back(std::make_shared<EntityCreatePacket>(
      entity->entityType(), (std::move(*result295)), std::move(firstNetState.first), entity->entityId()));
  }
  return {};
}

List<EntityId> WorldClient::entityIds() const {
  return m_entityMap->entityIds();
}

Vec2I WorldClient::environmentBiomeTrackPosition() const {
  if (!inWorld())
    return {};

  auto pos = Vec2I::floor(m_clientState->windowCenter());
  return {m_geometry.xwrap(pos[0]), pos[1]};
}

std::expected<std::optional<String>, StarException> WorldClient::currentWeatherDomain() const {
  if (!m_worldTemplate) {
    return {};
  }
  
  auto window = m_clientState->window();
  if (window.isEmpty()) {
    return m_weatherDomain;
  }
  {
    auto result296 = m_worldTemplate->weatherLayerAt(Vec2I::floor(m_clientState->windowCenter()));
    if (!result296)
      return std::unexpected(std::move(result296.error()));
    if (auto layer = (std::move(*result296))) {
      return layer->get().domain;
    }
  }
  return {};
}

std::expected<std::shared_ptr<AmbientNoisesDescription>, StarException> WorldClient::currentAmbientNoises() const {
  if (!inWorld()) {
    return {};
  }
  
  Vec2I pos = environmentBiomeTrackPosition();
  return m_worldTemplate->ambientNoises(pos[0], pos[1]);
}

std::expected<std::shared_ptr<WeatherNoisesDescription>, StarException> WorldClient::currentWeatherNoises() const {
  if (!inWorld()) {
    return {};
  }
  
  {
    auto result297 = currentWeatherDomain();
    if (!result297)
      return std::unexpected(std::move(result297.error()));
    if ((std::move(*result297)) != m_weatherDomain) {
      return {};
    }
  }
  
  auto trackOptions = m_weather->weatherTrackOptions();
  if (trackOptions.empty()) {
    return {};
  } else {
    return std::make_shared<WeatherNoisesDescription>(std::move(trackOptions));
  }
}

std::expected<std::shared_ptr<AmbientNoisesDescription>, StarException> WorldClient::currentMusicTrack() const {
  if (!inWorld()) {
    return {};
  }
  
  Vec2I pos = environmentBiomeTrackPosition();
  return m_worldTemplate->musicTrack(pos[0], pos[1]);
}

std::shared_ptr<AmbientNoisesDescription> WorldClient::currentAltMusicTrack() const {
  if (!inWorld())
    return {};

  return m_altMusicTrackDescription;
}

void WorldClient::playAltMusic(StringList const& newTracks, float fadeTime, int loops) {
  auto newTrackGroup = AmbientTrackGroup(newTracks);
  m_altMusicTrackDescription = std::make_shared<AmbientNoisesDescription>(AmbientTrackGroup(newTracks), AmbientTrackGroup(), loops);
  if (!m_altMusicActive) {
    m_musicTrack.setVolume(0.0, 0.0, fadeTime);
    m_altMusicTrack.setVolume(1.0, 0.0, fadeTime);
    m_altMusicActive = true;
  }
}

void WorldClient::stopAltMusic(float fadeTime) {
  if (m_altMusicActive) {
    m_musicTrack.setVolume(1.0, 0.0, fadeTime);
    m_altMusicTrack.setVolume(0.0, 0.0, fadeTime);
    m_altMusicActive = false;
  }
}

std::expected<std::shared_ptr<Biome const>, StarException> WorldClient::mainEnvironmentBiome() const {
  if (!inWorld()) {
    return {};
  }
  
  Vec2I pos = environmentBiomeTrackPosition();
  return m_worldTemplate->environmentBiome(pos[0], pos[1]);
}

std::expected<bool, StarException> WorldClient::readNetTile(Vec2I const& pos, NetTile const& netTile, bool updateCollision) {
  auto tile = m_tileArray->modifyTile(pos);
  if (!tile) {
    return false;
  }
  
  if (!m_predictedTiles.empty()) {
    auto findPrediction = m_predictedTiles.find(pos);
    if (findPrediction != m_predictedTiles.end()) {
      auto& p = findPrediction->second;
      
      if (p.collision && *p.collision == netTile.collision) {
        p.collision.reset();
      }
      if (p.foreground && (*p.foreground == StructureMaterialId || *p.foreground == netTile.foreground)) {
        p.foreground.reset();
      }
      if (p.foregroundMod && *p.foregroundMod == netTile.foregroundMod) {
        p.foregroundMod.reset();
      }
      if (p.foregroundHueShift && *p.foregroundHueShift == netTile.foregroundHueShift) {
        p.foregroundHueShift.reset();
      }
      if (p.foregroundModHueShift && *p.foregroundModHueShift == netTile.foregroundModHueShift) {
        p.foregroundModHueShift.reset();
      }
      
      if (p.background && *p.background == netTile.background) {
        p.background.reset();
      }
      if (p.backgroundMod && *p.backgroundMod == netTile.backgroundMod) {
        p.backgroundMod.reset();
      }
      if (p.backgroundHueShift && *p.backgroundHueShift == netTile.backgroundHueShift) {
        p.backgroundHueShift.reset();
      }
      if (p.backgroundModHueShift && *p.backgroundModHueShift == netTile.backgroundModHueShift) {
        p.backgroundModHueShift.reset();
      }
      
      if (!p) {
        m_predictedTiles.erase(findPrediction);
      }
    }
  }
  
  tile->get().background = netTile.background;
  tile->get().backgroundHueShift = netTile.backgroundHueShift;
  tile->get().backgroundColorVariant = netTile.backgroundColorVariant;
  tile->get().backgroundMod = netTile.backgroundMod;
  tile->get().backgroundModHueShift = netTile.backgroundModHueShift;
  tile->get().foreground = netTile.foreground;
  tile->get().foregroundHueShift = netTile.foregroundHueShift;
  tile->get().foregroundColorVariant = netTile.foregroundColorVariant;
  tile->get().foregroundMod = netTile.foregroundMod;
  tile->get().foregroundModHueShift = netTile.foregroundModHueShift;
  tile->get().collision = netTile.collision;
  tile->get().blockBiomeIndex = netTile.blockBiomeIndex;
  tile->get().environmentBiomeIndex = netTile.environmentBiomeIndex;
  tile->get().liquid = netTile.liquid.liquidLevel();
  tile->get().dungeonId = netTile.dungeonId;
  
  auto result298 = this->root().materialDatabase();
  if (!result298)
    return std::unexpected(std::move(result298.error()));
  auto materialDatabase = (std::move(*result298));
  auto result299 = materialDatabase->backgroundLightTransparent(tile->get().background);
  if (!result299)
    return std::unexpected(std::move(result299.error()));
  tile->get().backgroundLightTransparent = (std::move(*result299));
  auto result301 = materialDatabase->foregroundLightTransparent(tile->get().foreground);
  if (!result301)
    return std::unexpected(std::move(result301.error()));
  tile->get().foregroundLightTransparent =
      (std::move(*result301)) && tile->get().collision != CollisionKind::Dynamic;
  
  if (updateCollision) {
    dirtyCollision(RectI::withSize(pos, {1, 1}));
  }
  
  return true;
}

void WorldClient::dirtyCollision(RectI const& region) {
  if (!inWorld())
    return;

  auto dirtyRegion = region.padded(CollisionGenerator::BlockInfluenceRadius);
  for (int x = dirtyRegion.xMin(); x < dirtyRegion.xMax(); ++x) {
    for (int y = dirtyRegion.yMin(); y < dirtyRegion.yMax(); ++y) {
      if (auto tile = m_tileArray->modifyTile({x, y}))
        tile->get().collisionCacheDirty = true;
    }
  }
}

std::expected<void, StarException> WorldClient::freshenCollision(RectI const& region) {
  if (!inWorld()) {
    return {};
  }
  
  RectI freshenRegion = RectI::null();
  for (int x = region.xMin(); x < region.xMax(); ++x) {
    for (int y = region.yMin(); y < region.yMax(); ++y) {
      if (auto tile = m_tileArray->modifyTile({x, y})) {
        if (tile->get().collisionCacheDirty) {
          freshenRegion.combine(RectI(x, y, x + 1, y + 1));
        }
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
    
    auto blocks = m_collisionGenerator.getBlocks(freshenRegion);
    if (!blocks) {
      return std::unexpected(std::move(blocks.error()));
    }
    for (auto& collisionBlock : *blocks) {
      if (auto tile = m_tileArray->modifyTile(collisionBlock.space)) {
        auto appended = tile->get().collisionCache.push_back(std::move(collisionBlock));
        if (!appended)
          return std::unexpected(std::move(appended.error()));
      }
    }
  }
  return {};
}

std::expected<float, StarException> WorldClient::lightLevel(Vec2F const& pos) const {
  if (!inWorld()) {
    return 0.0f;
  }
  return WorldImpl::lightLevel(this->root(), m_tileArray, m_entityMap, m_geometry, m_worldTemplate, m_sky, m_lightIntensityCalculator, pos);
}

std::expected<bool, StarException> WorldClient::breathable(Vec2F const& pos) const {
  if (!inWorld()) {
    return true;
  }
  
  return WorldImpl::breathable(this->root(), *this, m_tileArray, m_dungeonIdBreathable, m_worldTemplate, pos);
}

std::expected<float, StarException> WorldClient::threatLevel() const {
  if (!inWorld()) {
    return 0.0f;
  }
  return m_worldTemplate->threatLevel();
}

std::expected<StringList, StarException> WorldClient::environmentStatusEffects(Vec2F const& pos) const {
  if (!inWorld()) {
    return {};
  }
  
  return m_worldTemplate->environmentStatusEffects(std::floor(pos[0]), std::floor(pos[1]));
}

std::expected<StringList, StarException> WorldClient::weatherStatusEffects(Vec2F const& pos) const {
  if (!inWorld()) {
    return {};
  }
  
  if (!m_weather->statusEffects().empty()) {
    {
      auto result302 = exposedToWeather(pos);
      if (!result302)
        return std::unexpected(std::move(result302.error()));
      if ((std::move(*result302))) {
        return m_weather->statusEffects();
      }
    }
  }
  
  return {};
}

std::expected<bool, StarException> WorldClient::exposedToWeather(Vec2F const& pos) const {
  if (!inWorld()) {
    return false;
  }
  auto layer = m_worldTemplate->weatherLayerAt(Vec2I::floor(pos));
  if (!layer) {
    return std::unexpected(std::move(layer.error()));
  }
  if (!*layer || !(*layer)->get().domain) {
    return false;
  }
  auto domain = m_worldTemplate->weatherDomain(*(*layer)->get().domain);
  if (!domain) {
    return std::unexpected(std::move(domain.error()));
  }
  if (*domain && (*layer)->get().domain == m_weatherDomain && pos[1] > (*domain)->get().effectsMinHeight
        && liquidLevel(Vec2I::floor(pos)).liquid == EmptyLiquidId) {
    auto result303 = this->root().assets();
    if (!result303)
      return std::unexpected(std::move(result303.error()));
    auto assets = (std::move(*result303));
    auto result304 = assets->json("/weather.config:weatherRayCheckDistance");
    if (!result304)
      return std::unexpected(std::move(result304.error()));
    auto result305 = (std::move(*result304)).toFloat();
    if (!result305)
      return std::unexpected(std::move(result305.error()));
    float weatherRayCheckDistance = (std::move(*result305));
    auto result306 = assets->json("/weather.config:weatherRayCheckWindInfluence");
    if (!result306)
      return std::unexpected(std::move(result306.error()));
    auto result307 = (std::move(*result306)).toFloat();
    if (!result307)
      return std::unexpected(std::move(result307.error()));
    float weatherRayCheckWindInfluence = (std::move(*result307));
    auto offset = Vec2F(-m_weather->wind() * weatherRayCheckWindInfluence, weatherRayCheckDistance).normalized() * weatherRayCheckDistance;
    auto result308 = lineCollision({pos, pos + offset});
    if (!result308)
      return std::unexpected(std::move(result308.error()));
    return !(std::move(*result308));
  }
  return false;
}

std::expected<std::optional<String>, StarException> WorldClient::activeWeather(Vec2F const&) const {
  if (!inWorld()) {
    return {};
  }
  return m_weather->activeWeather();
}

std::expected<bool, StarException> WorldClient::isUnderground(Vec2F const& pos) const {
  if (!inWorld()) {
    return true;
  }
  return m_worldTemplate->undergroundLevel() >= pos[1];
}

std::expected<bool, StarException> WorldClient::disableDeathDrops() const {
  if (const auto& parameters = m_worldTemplate->worldParameters()) {
    return parameters->disableDeathDrops;
  }
  return false;
}

std::expected<List<PhysicsForceRegion>, StarException> WorldClient::forceRegions() const {
  return m_forceRegions;
}

std::expected<Json, StarException> WorldClient::getProperty(String const& propertyName, Json const& def) const {
  if (!inWorld()) {
    return {};
  }
  
  return m_worldProperties.value(propertyName, def);
}

std::expected<void, StarException> WorldClient::setProperty(String const& propertyName, Json const& property) {
  if (!inWorld()) {
    return {};
  }
  
  if (m_worldProperties[propertyName] == property) {
    return {};
  }

  m_outgoingPackets.push_back(std::make_shared<UpdateWorldPropertiesPacket>(JsonObject{{propertyName, property}}));
  return {};
}

std::expected<bool, StarException> WorldClient::playerCanReachEntity(EntityId entityId, bool preferInteractive) const {
  if (entityId == m_mainPlayer->entityId() || m_mainPlayer->isAdmin())
    return true;
  auto radius = m_mainPlayer->interactRadius();
  if (!radius)
    return std::unexpected(std::move(radius.error()));
  return canReachEntity(m_mainPlayer->position(), *radius, entityId, preferInteractive);
}

void WorldClient::disconnectAllWires(Vec2I wireEntityPosition, WireNode const& node) {
  m_outgoingPackets.push_back(std::make_shared<DisconnectAllWiresPacket>(wireEntityPosition, node));
}

void WorldClient::wire(Vec2I const& outputPosition, std::size_t outputIndex, Vec2I const& inputPosition, std::size_t inputIndex) {
  WireConnection output = {.entityLocation = outputPosition, .nodeIndex = outputIndex};
  WireConnection input = {.entityLocation = inputPosition, .nodeIndex = inputIndex};
  connectWire(output, input);
}

void WorldClient::connectWire(WireConnection const& output, WireConnection const& input) {
  m_outgoingPackets.push_back(std::make_shared<ConnectWirePacket>(output, input));
}

bool WorldClient::sendSecretBroadcast(StringView broadcast, bool raw, bool compress) {
  if (!inWorld() || !m_mainPlayer || !m_mainPlayer->getSecretPropertyView(SECRET_BROADCAST_PUBLIC_KEY))
    return false;

  auto signature = this->root().identity().sign(std::span<char const>(broadcast.utf8()));

  auto damageNotification = std::make_shared<DamageNotificationPacket>();
  auto& remDmg = damageNotification->remoteDamageNotification;
  auto& dmg = remDmg.damageNotification;

  dmg.targetEntityId = dmg.sourceEntityId = remDmg.sourceEntityId = m_mainPlayer->entityId();
  dmg.damageDealt = dmg.healthLost = 0.0f;
  dmg.hitType = HitType::Hit;
  dmg.damageSourceKind = "nodamage";
  dmg.targetMaterialKind = raw ? String(broadcast) : String(Star::format("{}{}{}", SECRET_BROADCAST_PREFIX,
      StringView(std::string_view(reinterpret_cast<char const*>(signature.data()), signature.size())), broadcast));
  dmg.position = m_mainPlayer->position();

  if (!compress)
    damageNotification->setCompressionMode(PacketCompressionMode::Disabled);

  m_outgoingPackets.emplace_back(std::move(damageNotification));
  return true;
}

std::expected<bool, StarException> WorldClient::handleSecretBroadcast(std::shared_ptr<Player> player, StringView broadcast) {
  if (m_broadcastCallback) {
    return m_broadcastCallback(std::move(player), broadcast);
  } else {
    return false;
  }
}


void WorldClient::ClientRenderCallback::clear() {
  for (auto& layer : drawables)
    layer.second.clear();
  lightSources.clear();
  particles.clear();
  audios.clear();
  previewTiles.clear();
  overheadBars.clear();
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::addDrawable(Drawable drawable, EntityRenderLayer renderLayer) {
  drawable.translate(m_offset);
  (drawableTarget ? *drawableTarget : drawables)[renderLayer].push_back(std::move(drawable));
  return {};
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::addLightSource(LightSource lightSource) {
  lightSource.translate(m_offset);
  lightSources.push_back(std::move(lightSource));
  return {};
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::addParticle(Particle particle) {
  particles.push_back(std::move(particle));
  return {};
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::addAudio(std::shared_ptr<AudioInstance> audio) {
  audios.push_back(std::move(audio));
  return {};
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::addTilePreview(PreviewTile preview) {
  previewTiles.push_back(std::move(preview));
  return {};
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::addOverheadBar(OverheadBar bar) {
  bar.entityPosition += m_offset;
  overheadBars.push_back(std::move(bar));
  return {};
}

double WorldClient::epochTime() const {
  if (!inWorld())
    return 0;
  return m_sky->epochTime();
}

std::uint32_t WorldClient::day() const {
  if (!inWorld())
    return 0;
  return m_sky->day();
}

float WorldClient::dayLength() const {
  if (!inWorld())
    return 0;
  return m_sky->dayLength();
}

float WorldClient::timeOfDay() const {
  if (!inWorld())
    return 0;
  return m_sky->timeOfDay();
}

std::shared_ptr<LuaRoot> WorldClient::luaRoot() {
  return m_luaRoot;
}

std::expected<StringMap<LuaCallbacks>, StarException> WorldClient::luaThreadCallbacks() const {
  return m_luaThreadCallbacks;
}

bool WorldClient::pullRequestedDestroy() {
  auto out = m_requestedDestroy;
  m_requestedDestroy = false;
  return out;
}

void WorldClient::requestDestroy() {
  m_expiryTimer.reset();
  m_requestedDestroy = true;
}

std::expected<RpcPromise<Vec2F>, StarException> WorldClient::findUniqueEntity(String const& uniqueId) {
  if (!inWorld()) {
    return RpcPromise<Vec2F>::createFailed("Not currently in a world");
  }
  
  if (auto entity = m_entityMap->uniqueEntity(uniqueId)) {
    return RpcPromise<Vec2F>::createFulfilled(entity->position());
  }
  
  auto pair = RpcPromise<Vec2F>::createPair();
  auto& rpcPromises = m_findUniqueEntityResponses[uniqueId];
  if (rpcPromises.empty()) {
    m_outgoingPackets.push_back(std::make_shared<FindUniqueEntityPacket>(uniqueId));
  }
  rpcPromises.push_back(pair.second);

  return pair.first;
}

std::expected<RpcPromise<Json>, StarException> WorldClient::sendEntityMessage(std::variant<EntityId, String> const& entityId, String const& message, JsonArray const& args) {
  if (!inWorld()) {
    return RpcPromise<Json>::createFailed("Not currently in a world");
  }
  
  std::shared_ptr<Entity> entity;
  if (std::holds_alternative<EntityId>(entityId)) {
    entity = m_entityMap->entity(std::get<EntityId>(entityId));
  } else {
    entity = m_entityMap->uniqueEntity(std::get<String>(entityId));
  }
  
  // Only fail with "unknown entity" if we know this entity should exist on the
  // client, because it's entity id indicates it is master here.
  if (std::holds_alternative<EntityId>(entityId) && !entity && m_clientId == connectionForEntity(std::get<EntityId>(entityId))) {
    return RpcPromise<Json>::createFailed("Unknown entity");
  } else if (entity && entity->isMaster()) {
    auto responseResult = entity->receiveMessage(*m_clientId, message, args);
    if (!responseResult)
      return std::unexpected(std::move(responseResult.error()));
    auto response = std::move(*responseResult);
    if (!response)
      return RpcPromise<Json>::createFailed("Message not handled by entity");

    struct ResponseToPromise {
      std::expected<RpcPromise<Json>, StarException> operator()(Json& result) const {
        return RpcPromise<Json>::createFulfilled(result);
      }

      std::expected<RpcPromise<Json>, StarException> operator()(RpcPromise<Json>& promise) const { return promise; }
    };
    return std::visit(ResponseToPromise{}, *response);
  } else {
    auto pair = RpcPromise<Json>::createPair();
    Uuid uuid(this->root().random());
    m_entityMessageResponses[uuid] = pair.second;
    m_outgoingPackets.push_back(std::make_shared<EntityMessagePacket>(entityId, message, args, uuid));
    return pair.first;
  }
}

std::expected<List<ChatAction>, StarException> WorldClient::pullPendingChatActions() {
  List<ChatAction> result;
  if (m_entityMap) {
    {
      auto result310 = m_entityMap->all<ChattyEntity>();
      if (!result310)
        return std::unexpected(std::move(result310.error()));
      for (auto const& entity : (std::move(*result310))) {
        result.append_range(entity->pullPendingChatActions());
      }
    }
  }
  return result;
}

WorldStructure const& WorldClient::centralStructure() const {
  return m_centralStructure;
}

bool WorldClient::DamageNumberKey::operator<(DamageNumberKey const& other) const {
  return std::tie(sourceEntityId, targetEntityId, damageNumberParticleKind)
      < std::tie(other.sourceEntityId, other.targetEntityId, other.damageNumberParticleKind);
}

std::expected<void, StarException> WorldClient::renderCollisionDebug() {
  RectI clientWindow = m_clientState->window();
  if (clientWindow.isEmpty()) {
    return {};
  }

  auto log_poly = [&self = *this](PolyF poly, Vec2F position, float r, float g, float b) {
    poly.translate(position);
    self.root().spatialLogger().log_poly("world", poly,
                                         {floatToByte(r, true), floatToByte(g, true), floatToByte(b, true), 255});
  };

  auto result311 = forEachCollisionBlock(clientWindow, [&](auto const& block) -> std::expected<void, StarException> {
    log_poly(block.poly, Vec2F{}, 1.0f, 0.0f, 0.0f);
    return {};
  });
  if (!result311)
    return std::unexpected(std::move(result311.error()));
  
  auto objects = query<TileEntity>(RectF(clientWindow));
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  for (auto const& object : *objects) {
    for (auto const& space : object->spaces()) {
      log_poly(PolyF(RectF(Vec2F(space), Vec2F(space) + Vec2F(1, 1))), Vec2F(object->tilePosition()), 0., 1., 0.);
    }
  }
  
  auto physicsEntities = query<PhysicsEntity>(RectF(clientWindow));
  if (!physicsEntities)
    return std::unexpected(std::move(physicsEntities.error()));
  for (auto const& physics : *physicsEntities) {
    {
      auto result312 = physics->forceRegions();
      if (!result312)
        return std::unexpected(std::move(result312.error()));
      for (auto const& forceRegion : (std::move(*result312))) {
        if (std::holds_alternative<DirectionalForceRegion>(forceRegion)) {
          log_poly(std::get<DirectionalForceRegion>(forceRegion).region, {}, 1.0f, 1.0f, 0.0f);
        } else if (std::holds_alternative<RadialForceRegion>(forceRegion)) {
          log_poly(PolyF(std::get<RadialForceRegion>(forceRegion).boundBox()), {}, 0.0f, 1.0f, 1.0f);
        }
      }
    }
    
    for (std::size_t i = 0; i < physics->movingCollisionCount(); ++i) {
      {
        auto result313 = physics->movingCollision(i);
        if (!result313)
          return std::unexpected(std::move(result313.error()));
        if (auto pmc = (std::move(*result313))) {
          log_poly(pmc->collision, pmc->position, 1.0f, 1.0f, 1.0f);
        }
      }
    }
  }
  return {};
}

std::expected<void, StarException> WorldClient::informTilePrediction(Vec2I const& pos, TileModification const& modification) {
  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  auto& p = m_predictedTiles[pos];
  p.time = now;
  if (std::holds_alternative<PlaceMaterial>(modification)) {
    auto const& placeMaterial = std::get<PlaceMaterial>(modification);
    if (placeMaterial.layer == TileLayer::Foreground) {
      auto result314 = this->root().materialDatabase();
      if (!result314)
        return std::unexpected(std::move(result314.error()));
      auto materialDatabase = (std::move(*result314));
      {
        auto result316 = materialDatabase->isCascadingFallingMaterial(placeMaterial.material);
        if (!result316)
          return std::unexpected(std::move(result316.error()));
        if (!(std::move(*result316))
               && !materialDatabase->         isFallingMaterial(placeMaterial.material)) {
          p.foreground = placeMaterial.material;
          p.foregroundHueShift = placeMaterial.materialHueShift;
        } else {
          p.foreground = StructureMaterialId;
        }
      }
      if (placeMaterial.collisionOverride != TileCollisionOverride::None) {
        p.collision = collisionKindFromOverride(placeMaterial.collisionOverride);
      } else {
        auto result317 = materialDatabase->materialCollisionKind(placeMaterial.material);
        if (!result317)
          return std::unexpected(std::move(result317.error()));
        p.collision = (std::move(*result317));
      }
      dirtyCollision(RectI::withSize(pos, { 1, 1 }));
    } else {
      p.background = placeMaterial.material;
      p.backgroundHueShift = placeMaterial.materialHueShift;
    }
  } else if (std::holds_alternative<PlaceMod>(modification)) {
    auto const& placeMod = std::get<PlaceMod>(modification);
    if (placeMod.layer == TileLayer::Foreground) {
      p.foregroundMod = placeMod.mod;
    } else {
      p.backgroundMod = placeMod.mod;
    }
  } else if (std::holds_alternative<PlaceMaterialColor>(modification)) {
    auto const& placeColor = std::get<PlaceMaterialColor>(modification);
    if (placeColor.layer == TileLayer::Foreground) {
      p.foregroundColorVariant = placeColor.color;
    } else {
      p.backgroundColorVariant = placeColor.color;
    }
  } else if (std::holds_alternative<PlaceLiquid>(modification)) {
    auto const& placeLiquid = std::get<PlaceLiquid>(modification);
    if (!p.liquid || p.liquid->liquid != placeLiquid.liquid) {
      p.liquid = LiquidLevel(placeLiquid.liquid, placeLiquid.liquidLevel);
    } else {
      p.liquid->level += placeLiquid.liquidLevel;
    }
  }
  return {};
}

std::expected<void, StarException> WorldClient::setupForceRegions() {
  m_forceRegions.clear();
  
  if (!currentTemplate() || !currentTemplate()->worldParameters()) {
    return {};
  }
  
  auto forceRegionType = currentTemplate()->worldParameters()->worldEdgeForceRegions;
  
  if (forceRegionType == WorldEdgeForceRegionType::None) {
    return {};
  }
  
  bool addTopRegion = forceRegionType == WorldEdgeForceRegionType::Top || forceRegionType == WorldEdgeForceRegionType::TopAndBottom;
  bool addBottomRegion = forceRegionType == WorldEdgeForceRegionType::Bottom || forceRegionType == WorldEdgeForceRegionType::TopAndBottom;
  
  auto result318 = this->root().assets();
  if (!result318)
    return std::unexpected(std::move(result318.error()));
  auto result319 = (std::move(*result318))->json("/worldserver.config");
  if (!result319)
    return std::unexpected(std::move(result319.error()));
  auto worldServerConfig = (std::move(*result319));
  
  auto result320 = worldServerConfig.getFloat("worldEdgeForceRegionHeight");
  if (!result320)
    return std::unexpected(std::move(result320.error()));
  auto regionHeight = (std::move(*result320));
  auto result321 = worldServerConfig.getFloat("worldEdgeForceRegionForce");
  if (!result321)
    return std::unexpected(std::move(result321.error()));
  auto regionForce = (std::move(*result321));
  auto result322 = worldServerConfig.getFloat("worldEdgeForceRegionVelocity");
  if (!result322)
    return std::unexpected(std::move(result322.error()));
  auto regionVelocity = (std::move(*result322));
  auto regionCategoryFilter = PhysicsCategoryFilter::whitelist({"player", "monster", "npc", "vehicle"});
  auto worldSize = Vec2F(currentTemplate()->size());
  
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

bool WorldClient::shouldExpire() {
  if (inWorld()) {
    return false;
  }

  return m_expiryTimer.ready();
}

}
