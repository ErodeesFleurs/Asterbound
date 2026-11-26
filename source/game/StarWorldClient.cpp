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
import star.message_types;
import star.material_types;
import star.poly;
import star.list;
import star.small_vector;
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

namespace {
const String secret_broadcast_public_key = "SecretBroadcastPublicKey";
constexpr std::string_view secret_broadcast_prefix("\0Broadcast\0", sizeof("\0Broadcast\0") - 1);
const String legacy_voice_public_key(std::string("\0SE_VOICE_SIGNING_KEY", sizeof("\0SE_VOICE_SIGNING_KEY") - 1));
constexpr std::string_view legacy_voice_prefix("data\0voice\0", sizeof("data\0voice\0") - 1);

std::optional<std::reference_wrapper<Directives const>> select_directive(
    std::shared_ptr<VisitableWorldParameters const> const& parameters, EntityId entity_id) {
  if (!parameters || !parameters->globalDirectives || parameters->globalDirectives->empty())
    return {};
  auto const& options = *parameters->globalDirectives;
  auto const& selected = options[static_cast<unsigned>(entity_id) % options.size()];
  if (selected.empty())
    return {};
  return std::cref(selected);
}
}

const float WorldClient::DropDist = 6.0f;
WorldClient::WorldClient(Root& root, UniverseClient& universe)
  : World(root), m_currentStep(0), m_current_time(0), m_pending_light_ready(false),
    m_subWorldId(main_client_world_id), m_worldDimLevel(0), m_latency(0),
    m_ambient_sounds(root), m_music_track(root), m_alt_music_track(root),
    m_broadcast_public_key(std::string(root.identity().public_key().begin(), root.identity().public_key().end())),
    m_universe(universe) {}

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
  m_expiry_timer = GameTimer(0);
  m_headless = false;
  m_subWorldId = main_client_world_id;
  
  m_currentStep = 0;
  m_current_time = 0;
  m_fullBright = false;
  m_asyncLighting = false;
  auto result3 = m_clientConfig.get_float("worldDimTime");
  if (!result3)
    return std::unexpected(std::move(result3.error()));
  m_world_dim_timer = GameTimer((std::move(*result3)));
  m_world_dim_timer.set_done();
  m_worldDimLevel = 0.0f;
  
  auto result4 = m_clientConfig.get_float("parallaxFadeTime");
  if (!result4)
    return std::unexpected(std::move(result4.error()));
  m_parallax_fade_timer = GameTimer((std::move(*result4)));
  m_parallax_fade_timer.set_done();
  
  m_collisionDebug = false;
  m_inWorld = false;

  m_luaRoot = std::move(luaRoot);
  auto result5 = LuaBindings::makeUniverseClientThreadCallbacks(this->root(), universe);
  if (!result5)
    return std::unexpected(std::move(result5.error()));
  m_luaThreadCallbacks["universe"] = (std::move(*result5));
  auto result6 = LuaBindings::make_celestial_callbacks(this->root(), universe.celestial_database());
  if (!result6)
    return std::unexpected(std::move(result6.error()));
  m_luaThreadCallbacks["celestial"] = (std::move(*result6));

  m_mainPlayer = std::move(mainPlayer);

  centerClientWindowOnPlayer(Vec2U(100, 100));
  
  m_collision_generator.init([&self = *this](int x, int y) -> std::expected<CollisionKind, StarException> {
    return self.effective_collision({x, y}, self.m_tileArray->tile({x, y}));
  });
  
  auto result7 = m_clientConfig.get_float("modifiedTilePredictionTimeout");
  if (!result7)
    return std::unexpected(std::move(result7.error()));
  m_modifiedTilePredictionTimeout =
    static_cast<int>(std::round((std::move(*result7)) / this->root().timing().time_step));

  m_latency = std::chrono::milliseconds::zero();
  
  auto result8 = m_clientConfig.get_object("blockDamageParticle");
  if (!result8)
    return std::unexpected(std::move(result8.error()));
  auto result9 = Particle::create(this->root(), (std::move(*result8)));
  if (!result9)
    return std::unexpected(std::move(result9.error()));
  m_blockDamageParticle = (std::move(*result9));
  auto result10 = m_clientConfig.get_object("blockDamageParticleVariance");
  if (!result10)
    return std::unexpected(std::move(result10.error()));
  auto result11 = Particle::create(this->root(), (std::move(*result10)));
  if (!result11)
    return std::unexpected(std::move(result11.error()));
  m_blockDamageParticleVariance = (std::move(*result11));
  auto result12 = m_clientConfig.get_float("blockDamageParticleProbability");
  if (!result12)
    return std::unexpected(std::move(result12.error()));
  m_blockDamageParticleProbability = (std::move(*result12));
  
  auto result13 = m_clientConfig.get_object("blockDingParticle");
  if (!result13)
    return std::unexpected(std::move(result13.error()));
  auto result14 = Particle::create(this->root(), (std::move(*result13)));
  if (!result14)
    return std::unexpected(std::move(result14.error()));
  m_blockDingParticle = (std::move(*result14));
  auto result15 = m_clientConfig.get_object("blockDingParticleVariance");
  if (!result15)
    return std::unexpected(std::move(result15.error()));
  auto result16 = Particle::create(this->root(), (std::move(*result15)));
  if (!result16)
    return std::unexpected(std::move(result16.error()));
  m_blockDingParticleVariance = (std::move(*result16));
  auto result17 = m_clientConfig.get_float("blockDingParticleProbability");
  if (!result17)
    return std::unexpected(std::move(result17.error()));
  m_blockDingParticleProbability = (std::move(*result17));
  
  auto result18 = m_clientConfig.get_float("damageNotificationBatchDuration");
  if (!result18)
    return std::unexpected(std::move(result18.error()));
  m_damageNotificationBatchDuration = std::chrono::duration<float>(std::move(*result18));
  
  auto result19 = assets->json("/interface.config:ambientTrackFadeInTime");
  if (!result19)
    return std::unexpected(std::move(result19.error()));
  auto result20 = (std::move(*result19)).to_float();
  if (!result20)
    return std::unexpected(std::move(result20.error()));
  m_ambient_sounds.set_track_fade_in_time(*result20);
  auto result21 = assets->json("/interface.config:ambientTrackSwitchGrace");
  if (!result21)
    return std::unexpected(std::move(result21.error()));
  auto result22 = (std::move(*result21)).to_float();
  if (!result22)
    return std::unexpected(std::move(result22.error()));
  m_ambient_sounds.set_track_switch_grace(*result22);
  
  auto result23 = assets->json("/interface.config:musicTrackSwitchGrace");
  if (!result23)
    return std::unexpected(std::move(result23.error()));
  auto result24 = (std::move(*result23)).to_float();
  if (!result24)
    return std::unexpected(std::move(result24.error()));
  m_music_track.set_track_switch_grace(*result24);
  auto result25 = assets->json("/interface.config:musicTrackFadeInTime");
  if (!result25)
    return std::unexpected(std::move(result25.error()));
  auto result26 = (std::move(*result25)).to_float();
  if (!result26)
    return std::unexpected(std::move(result26.error()));
  m_music_track.set_track_fade_in_time(*result26);
  m_alt_music_track.set_track_fade_in_time(*result26);
  // Alternate tracks historically use the fade-in interval as their switch grace.
  m_alt_music_track.set_track_switch_grace(*result26);
  
  m_pending_light_ready = false;
  
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
  auto result34 = m_clientConfig.get_float("idleSubWorldExpireTime");
  if (!result34)
    return std::unexpected(std::move(result34.error()));
  m_expiry_timer = GameTimer((std::move(*result34)));
  m_headless = true;
  
  m_currentStep = 0;
  m_current_time = 0;
  
  m_inWorld = false;
  m_subWorldId = subWorldId;
  
  auto result35 = LuaRoot::create(this->root());
  if (!result35)
    return std::unexpected(std::move(result35.error()));
  m_luaRoot = (std::move(*result35));
  auto result36 = m_clientConfig.get_float("luaGcPause");
  if (!result36)
    return std::unexpected(std::move(result36.error()));
  auto result37 = m_clientConfig.get_float("luaGcStepMultiplier");
  if (!result37)
    return std::unexpected(std::move(result37.error()));
  m_luaRoot->tune_auto_garbage_collection((std::move(*result36)), (std::move(*result37)));
  auto result38 = LuaBindings::makeUniverseClientThreadCallbacks(this->root(), universe);
  if (!result38)
    return std::unexpected(std::move(result38.error()));
  m_luaThreadCallbacks["universe"] = (std::move(*result38));
  auto result39 = LuaBindings::make_celestial_callbacks(this->root(), universe.celestial_database());
  if (!result39)
    return std::unexpected(std::move(result39.error()));
  m_luaThreadCallbacks["celestial"] = (std::move(*result39));
  m_luaRoot->add_callbacks("universe", m_luaThreadCallbacks["universe"]);
  m_luaRoot->add_callbacks("celestial", m_luaThreadCallbacks["celestial"]);
  
  m_collision_generator.init([&self = *this](int x, int y) -> std::expected<CollisionKind, StarException> {
    return self.effective_collision({x, y}, self.m_tileArray->tile({x, y}));
  });
  
  auto result42 = m_clientConfig.get_float("modifiedTilePredictionTimeout");
  if (!result42)
    return std::unexpected(std::move(result42.error()));
  m_modifiedTilePredictionTimeout =
    static_cast<int>(std::round((std::move(*result42)) / this->root().timing().time_step));

  m_latency = std::chrono::milliseconds::zero();
  
  auto result43 = m_clientConfig.get_float("damageNotificationBatchDuration");
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
    auto entityId = m_entityMap->reserve_entity_id();
    if (!entityId)
      return std::unexpected(std::move(entityId.error()));
    auto initialized = m_mainPlayer->init(*this, *entityId, EntityMode::Master);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto added = m_entityMap->add_entity(m_mainPlayer);
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
  auto state = entity->write_net_state(*fromVersion, netRules);
  if (!state)
    return std::unexpected(std::move(state.error()));
  m_outgoingPackets.push_back(
    std::make_shared<EntityDestroyPacket>(entity->entityId(), std::move(state->first), false));
  return notifyEntityCreate(entity);
}

std::expected<void, StarException> WorldClient::removeEntity(EntityId entityId, bool andDie) {
  m_broadcast_verifiers.erase(entityId);
  auto entity = m_entityMap->entity(entityId);
  if (!entity)
    return {};

  if (andDie) {
    ClientRenderCallback render_callback(std::nullopt);
    NullRenderCallback hidden_events;
    RenderCallback& events = m_startupHiddenEntities.contains(entityId)
        ? static_cast<RenderCallback&>(hidden_events)
        : static_cast<RenderCallback&>(render_callback);
    // Removal packets can arrive before the next tick's event pass. Drain
    // pending live events before destroy() contributes its final death effects.
    auto collected = entity->collect_render_events(events);
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

    if (auto selected = select_directive(directive_parameters(), entity->entityId()))
      for (auto& particle : render_callback.particles)
        particle.directives.append(selected->get());

    m_particles->addParticles(std::move(render_callback.particles));
    m_samples.append_range(std::move(render_callback.audios));
  }

  if (auto version = m_masterEntitiesNetVersion.try_extract_value(entity->entityId())) {
    auto netRules = m_clientState->netCompatibilityRules();
    auto state = entity->write_net_state(*version, netRules);
    if (!state)
      return std::unexpected(std::move(state.error()));
    ByteArray finalNetState = std::move(state->first);
    m_outgoingPackets.push_back(
      std::make_shared<EntityDestroyPacket>(entity->entityId(), std::move(finalNetState), andDie));
  }

  m_entityMap->remove_entity(entityId);
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
  
  return m_entityMap->closest_entity(center, radius, selector);
}

std::expected<void, StarException> WorldClient::forAllEntities(EntityCallback callback) const {
  return m_entityMap->for_all_entities(callback);
}

std::expected<void, StarException> WorldClient::forEachEntity(RectF const& boundBox, EntityCallback callback) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->for_each_entity(boundBox, callback);
}

std::expected<void, StarException> WorldClient::forEachEntityLine(Vec2F const& begin, Vec2F const& end, EntityCallback callback) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->for_each_entity_line(begin, end, callback);
}

std::expected<void, StarException> WorldClient::forEachEntityAtTile(Vec2I const& pos, EntityCallbackOf<TileEntity> callback) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->for_each_entity_at_tile(pos, callback);
}

std::expected<std::shared_ptr<Entity>, StarException> WorldClient::findEntity(RectF const& boundBox, EntityFilter entityFilter) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->find_entity(boundBox, entityFilter);
}

std::expected<std::shared_ptr<Entity>, StarException> WorldClient::findEntityLine(Vec2F const& begin, Vec2F const& end, EntityFilter entityFilter) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->find_entity_line(begin, end, entityFilter);
}

std::expected<std::shared_ptr<Entity>, StarException> WorldClient::findEntityAtTile(Vec2I const& pos, EntityFilterOf<TileEntity> entityFilter) const {
  if (!inWorld()) {
    return {};
  }
  return m_entityMap->find_entity_at_tile(pos, entityFilter);
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

CollisionKind WorldClient::effective_collision(Vec2I const& pos, ClientTile const& tile) const {
  if (!m_predicted_tiles.empty()) {
    auto prediction = m_predicted_tiles.find_ref(m_geometry.xwrap(pos));
    if (prediction && prediction->get().collision && (tile.collision != CollisionKind::Null || m_tileArray->tile_loaded(pos)))
      return *prediction->get().collision;
  }
  return tile.collision;
}

void WorldClient::invalidate_prediction_change(Vec2I const& pos, std::optional<CollisionKind> before, std::optional<CollisionKind> after) {
  if (before == after) return;
  if (auto tile = m_tileArray->modify_tile(pos)) {
    auto authoritative = tile->get().collision;
    if (before.value_or(authoritative) != after.value_or(authoritative))
      WorldImpl::invalidate_collision_cache(*m_tileArray, RectI::with_size(pos, {1, 1}));
  }
}

std::expected<bool, StarException> WorldClient::collision_blocks_satisfy(RectI const& region,
    std::function<std::expected<bool, StarException>(CollisionBlock const&)> const& predicate) const {
  if (!inWorld()) return false;
  return WorldImpl::collision_blocks_satisfy(*m_tileArray, region, m_collision_generator,
      [&self = *this](Vec2I const& pos, ClientTile const& tile) { return self.effective_collision(pos, tile); }, predicate);
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
  
  return WorldImpl::line_tile_collision(m_geometry, *m_tileArray, begin, end, collisionSet);
}

std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> WorldClient::lineTileCollisionPoint(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet) const {
  if (!inWorld()) {
    return {};
  }
  
  return WorldImpl::line_tile_collision_point(m_geometry, *m_tileArray, begin, end, collisionSet);
}

std::expected<List<Vec2I>, StarException> WorldClient::collidingTilesAlongLine(
    Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet, int maxSize, bool includeEdges) const {
  if (!inWorld()) {
    return {};
  }
  
  return WorldImpl::colliding_tiles_along_line(m_geometry, *m_tileArray, begin, end, collisionSet, maxSize, includeEdges);
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
  return WorldImpl::liquid_level(*m_tileArray, region);
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
          auto predicted = inform_tile_prediction(m_geometry.xwrap(pair.first), pair.second);
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
  if (netRules.version() <= 3)
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
  return WorldImpl::damage_would_destroy(this->root(), *m_tileArray, pos, layer, tileDamage);
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
  setClientWindow(RectI::with_center(Vec2I::floor(m_mainPlayer->position()), Vec2I(windowSize)));
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

std::shared_ptr<VisitableWorldParameters const> WorldClient::directive_parameters() const {
  return m_worldTemplate ? m_worldTemplate->worldParameters() : nullptr;
}

WorldClient::EntityRenderSample WorldClient::sample_entity(Entity const& entity, PresentationContext presentation) const {
  if (auto entry = m_entityPresentation.find_ref(entity.entityId())) {
    auto const& state = entry->get();
    if (state.continuous && state.instance.lock().get() == &entity && state.epoch == entity.presentation_epoch())
      return {presentation, m_geometry.diff(state.position.sample(m_geometry, presentation.interpolation()), entity.position())};
  }
  // A replacement, teleport, or explicit reset has no previous display pose.
  return {presentation.current(), {}};
}

void WorldClient::resetPresentation() {
  m_entityPresentation.clear();
  m_parallaxWindTime.resetPresentation();
  if (m_particles)
    m_particles->resetPresentation();
  if (m_sky)
    m_sky->resetPresentation();
}

std::expected<void, StarException> WorldClient::render(WorldRenderData& render_data, unsigned buffer_tiles, std::optional<std::reference_wrapper<TileDrawer>> tile_drawer, PresentationContext presentation) {
  if (m_headless) {
    return {};
  }
  if (m_lightingThreadStarted && m_lightingThread.is_finished()) {
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
  
  render_data.prepare_frame();
  float const interpolation = presentation.interpolation();
  if (!inWorld()) {
    render_data.entity_drawables.clear();
    render_data.particles.clear();
    return {};
  }
  
  // If we're dimming the world, then that takes priority
  float dim_ratio = m_world_dim_timer.remaining_fraction();
  
  // Spends 80% of the time at pitch black with 10% ramp up and down
  
  m_worldDimColor = {};
  // always reset this to prevent persistent dimming from other sources
  if (dim_ratio) {
    if (dim_ratio <= 0.1f) {
      m_worldDimLevel = dim_ratio / 0.1f;
    } else if (dim_ratio >= 0.9f) {
      m_worldDimLevel = (1 - dim_ratio) / (1 - 0.9f);
    } else {
      m_worldDimLevel = 1.0f;
    }
  }
  
  m_preview_tiles.clear();
  
  render_data.geometry = m_geometry;
  m_particles->sampleParticles(interpolation, render_data.particles);
  
  RectI window = m_clientState->window();
  RectI tileRange = window.padded(buffer_tiles);
  render_data.tile_min_position = tileRange.min();
  
  if (!m_fullBright) {
    ClientRenderCallback lighting_callback(tile_drawer, presentation, m_presentationTimestep);
    lighting_callback.light_sources = std::move(m_gather_lights);
    lighting_callback.light_sources.clear();
    auto lights = m_entityMap->for_all_entities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
      if (m_startupHiddenEntities.contains(entity->entityId()))
        return {};
      auto sample = sample_entity(*entity, presentation);
      lighting_callback.position_offset = sample.offset;
      lighting_callback.sample_context = sample.context;
      return entity->render_light_sources(lighting_callback);
    });
    if (!lights)
      return std::unexpected(std::move(lights.error()));
    {
      std::lock_guard prep_lock(m_light_map_prep_mutex);
      m_gather_lights = std::exchange(m_pending_lights, std::move(lighting_callback.light_sources));
      m_pending_particle_lights.clear();
      for (auto const& particle : render_data.particles)
        if (particle.light != Color::palette_clear)
          m_pending_particle_lights.push_back({particle.position, particle.light.to_rgb_f()});
      m_pending_light_range = window.padded(1);
      m_pending_light_ready = true;
    }
    //Kae: Padded by one to fix light spread issues at the edges of the frame.
    
    if (m_asyncLighting) {
      m_lightingCond.notify_one();
    } else {
      std::lock_guard lighting_locker(m_lightingMutex);
      auto result48 = calculate_lighting();
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
  auto result51 = (std::move(*result50)).to_float();
  if (!result51)
    return std::unexpected(std::move(result51.error()));
  float pulse_amount = (std::move(*result51));
  auto result52 = this->root().assets();
  if (!result52)
    return std::unexpected(std::move(result52.error()));
  auto result53 = (std::move(*result52))->json("/highlights.config:interactivePulseRate");
  if (!result53)
    return std::unexpected(std::move(result53.error()));
  auto result54 = (std::move(*result53)).to_float();
  if (!result54)
    return std::unexpected(std::move(result54.error()));
  float pulse_rate = (std::move(*result54));
  float pulse_level = 1 - pulse_amount * 0.5 * (std::sin(2 * std::numbers::pi * pulse_rate * std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)).time_since_epoch().count() / 1000.0) + 1);
  
  bool inspecting = m_mainPlayer->inspecting();
  
  EntityId player_aim_interactive = null_entity_id;
  {
    auto result59 = this->root().configuration();
    if (!result59)
      return std::unexpected(std::move(result59.error()));
    auto result60 = (std::move(*result59))->get("interactiveHighlight");
    if (!result60)
      return std::unexpected(std::move(result60.error()));
    auto result61 = (std::move(*result60)).to_bool();
    if (!result61)
      return std::unexpected(std::move(result61.error()));
    if ((std::move(*result61))) {
      {
        auto result62 = m_mainPlayer->bestInteractionEntity(false);
        if (!result62)
          return std::unexpected(std::move(result62.error()));
        if (auto entity = (std::move(*result62))) {
          player_aim_interactive = entity->entityId();
        }
      }
    }
  }
  
  auto directives = directive_parameters();
  std::shared_ptr<ImageMetadataDatabase const> image_metadata;
  std::size_t drawable_index = 0;
  auto result72 = m_entityMap->for_all_entities([&, &self = *this](std::shared_ptr<Entity> const& entity)  -> std::expected<void, StarException> {
      if (self.m_startupHiddenEntities.contains(entity->entityId())) {
        return {};
      }
      
      if (drawable_index == render_data.entity_drawables.size())
        render_data.entity_drawables.push_back({});
      auto& ed = render_data.entity_drawables[drawable_index++];
      auto sample = self.sample_entity(*entity, presentation);
      ClientRenderCallback render_callback(tile_drawer, sample.context, self.m_presentationTimestep, sample.offset);
      render_callback.drawable_target = ed.layers;
      
      auto rendered = entity->render(render_callback);
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
          auto result63 = EntityTypeNames->get_right(entity->entity_type());
          if (!result63)
            return std::unexpected(std::move(result63.error()));
          auto result64 = self.root().logger().error("WorldClient: Exception caught in {}::render ({}): {}", result63->get(), entity->entityId(), issue);
          if (!result64)
            return std::unexpected(std::move(result64.error()));
          auto tool_user = as<ToolUserEntity>(entity);
          String image = "/rendering/sprites/error.png";
          if (tool_user) {
            auto result65 = enum_name<direction_names>(tool_user->facing_direction());
            if (!result65)
              return std::unexpected(std::move(result65.error()));
            image = Star::format("/rendering/sprites/error_{}.png", *result65);
          }
          Color color = Color::rgb_f(
            0.8f + static_cast<float>(std::sin(self.m_current_time * std::numbers::pi * 2.0)) * 0.2f, 0.0f, 0.0f);
          auto result66 = self.root().image_metadata_database();
          if (!result66)
            return std::unexpected(std::move(result66.error()));
          auto result67 = Drawable::make_image(*(std::move(*result66)), image, 1.0f / tile_pixels, true, entity->position(), color);
          if (!result67)
            return std::unexpected(std::move(result67.error()));
          auto drawable = (std::move(*result67));
          drawable.fullbright = true;
          auto result68 = render_callback.add_drawable(std::move(drawable), render_layer_middle_particle);
          if (!result68)
            return std::unexpected(std::move(result68.error()));
        }
      }
      
      if (auto selected = select_directive(directives, entity->entityId())) {
        for (auto& layer : ed.layers) {
          for (auto& d : layer.second) {
            if (d.is_image()) {
              if (!image_metadata) {
                auto metadata = self.root().image_metadata_database();
                if (!metadata)
                  return std::unexpected(std::move(metadata.error()));
                image_metadata = std::move(*metadata);
              }
              auto result70 = d.image_part().add_directives(*image_metadata, selected->get(), true);
              if (!result70)
                return std::unexpected(std::move(result70.error()));
            }
          }
        }
      }
      
      if (self.m_interactiveHighlightMode || (!inspecting && entity->entityId() == player_aim_interactive)) {
        if (auto interactive = as<InteractiveEntity>(entity)) {
          if (interactive->is_interactive()) {
            ed.highlight_effect.type = EntityHighlightEffectType::Interactive;
            ed.highlight_effect.level = pulse_level;
          }
        }
      } else if (inspecting) {
        if (auto inspectable = as<InspectableEntity>(entity)) {
          auto result71 = self.m_mainPlayer->inspectionHighlight(inspectable);
          if (!result71)
            return std::unexpected(std::move(result71.error()));
          ed.highlight_effect = (std::move(*result71));
          ed.highlight_effect.level *= self.m_inspectionFlickerMultiplier;
        }
      }

      self.m_preview_tiles.append_range(std::move(render_callback.preview_tiles));
      render_data.overhead_bars.append_range(std::move(render_callback.overhead_bars));
      return {};
    }, [](std::shared_ptr<Entity> const& a, std::shared_ptr<Entity> const& b) {
      return a->entityId() < b->entityId();
    });
  if (!result72)
    return std::unexpected(std::move(result72.error()));
  render_data.entity_drawables.resize(drawable_index);
  
  auto result73 = this->root().workers();
  if (!result73)
    return std::unexpected(std::move(result73.error()));
  auto result74 = m_tileArray->tile_each_to(result73->get(), render_data.tiles, tileRange, [](RenderTile& render_tile, Vec2I const&, ClientTile const& client_tile)  -> std::expected<void, StarException> {
      render_tile.foreground = client_tile.foreground;
      render_tile.foregroundMod = client_tile.foregroundMod;
      
      render_tile.background = client_tile.background;
      render_tile.backgroundMod = client_tile.backgroundMod;
      
      render_tile.foregroundHueShift = client_tile.foregroundHueShift;
      render_tile.foregroundModHueShift = client_tile.foregroundModHueShift;
      render_tile.foregroundColorVariant = client_tile.foregroundColorVariant;
      render_tile.foregroundDamageType = client_tile.foregroundDamage.damage_type();
      render_tile.foregroundDamageLevel = float_to_byte(client_tile.foregroundDamage.damage_effect_percentage());
      
      render_tile.backgroundHueShift = client_tile.backgroundHueShift;
      render_tile.backgroundModHueShift = client_tile.backgroundModHueShift;
      render_tile.backgroundColorVariant = client_tile.backgroundColorVariant;
      render_tile.backgroundDamageType = client_tile.backgroundDamage.damage_type();
      render_tile.backgroundDamageLevel = float_to_byte(client_tile.backgroundDamage.damage_effect_percentage());
      
      render_tile.liquidId = client_tile.liquid.liquid;
      render_tile.liquidLevel = float_to_byte(client_tile.liquid.level);
      return {};
    });
  if (!result74)
    return std::unexpected(std::move(result74.error()));

  for (auto&& pair : m_predicted_tiles) {
    Vec2I tile_array_pos(m_geometry.pdiff(pair.first[0], render_data.tile_min_position[0]), pair.first[1] - render_data.tile_min_position[1]);
    if (tile_array_pos[0] >= 0 && tile_array_pos[0] < static_cast<int>(render_data.tiles.extent(0)) && tile_array_pos[1] >= 0
        && tile_array_pos[1] < static_cast<int>(render_data.tiles.extent(1))) {
      RenderTile& render_tile = render_data.tiles(tile_array_pos[0], tile_array_pos[1]);
      auto& prediction = pair.second;
      if (prediction.liquid_remainder || prediction.liquid_delta) {
        auto liquid = prediction.effective_liquid({render_tile.liquidId, byte_to_float(render_tile.liquidLevel)});
        render_tile.liquidId = liquid.liquid;
        render_tile.liquidLevel = float_to_byte(liquid.level, true);
      }
      
      prediction.apply(render_tile);
    }
  }

  for (auto const& preview_tile : m_preview_tiles) {
    Vec2I tile_array_pos(m_geometry.pdiff(preview_tile.position[0], render_data.tile_min_position[0]), preview_tile.position[1] - render_data.tile_min_position[1]);
    if (tile_array_pos[0] >= 0 && tile_array_pos[0] < static_cast<int>(render_data.tiles.extent(0)) && tile_array_pos[1] >= 0
        && tile_array_pos[1] < static_cast<int>(render_data.tiles.extent(1))) {
      RenderTile& render_tile = render_data.tiles(tile_array_pos[0], tile_array_pos[1]);
      
      auto material = preview_tile.mat_id;
      auto hue_shift = preview_tile.hue_shift;
      auto color_variant = preview_tile.color_variant;
      if (preview_tile.update_mat_id) {
        if (preview_tile.foreground) {
          render_tile.foreground = material;
          render_tile.foregroundHueShift = hue_shift;
          render_tile.foregroundColorVariant = color_variant;
        } else {
          render_tile.background = material;
          render_tile.backgroundHueShift = hue_shift;
          render_tile.backgroundColorVariant = color_variant;
        }
      }
      
      if (preview_tile.liq_id != empty_liquid_id) {
        render_tile.liquidId = preview_tile.liq_id;
        render_tile.liquidLevel = 255;
      }
    }
  }
  
  this->root().logMap().set("client_render_particle_count", render_data.particles.size());
  
  auto result75 = m_sky->renderData(interpolation);
  if (!result75)
    return std::unexpected(std::move(result75.error()));
  render_data.sky_render_data = (std::move(*result75));
  
  auto biome_result = mainEnvironmentBiome();
  if (!biome_result)
    return std::unexpected(std::move(biome_result.error()));
  auto const& environment_biome = *biome_result;

  if (environment_biome) {
    setParallax(environment_biome->parallax);
  }
  
  if (m_currentParallax) {
    if (m_parallax_fade_timer.ready()) {
      render_data.parallax_layers.append_range(m_currentParallax->layers());
    } else {
      for (auto layer : m_currentParallax->layers()) {
        layer.alpha = std::min(1.0f, m_parallax_fade_timer.remaining_fraction() * 2);
        render_data.parallax_layers.push_back(layer);
      }
    }
  }
  
  if (m_nextParallax) {
    for (auto layer : m_nextParallax->layers()) {
      layer.alpha = std::min(1.0f, (1.0f - m_parallax_fade_timer.remaining_fraction()) * 2);
      render_data.parallax_layers.push_back(layer);
    }
  }
  
  auto result77 = currentWeatherDomain();
  if (!result77)
    return std::unexpected(std::move(result77.error()));
  auto weather_parallax_asset = *result77 == m_weatherDomain ? m_weather->weatherParallax() : std::optional<String>();
  if (weather_parallax_asset != m_weatherParallaxAsset || environment_biome != m_weatherParallaxBiome) {
    m_weatherParallaxAsset = weather_parallax_asset;
    m_weatherParallaxBiome = environment_biome;
    m_weatherParallax.reset();
    
    {
      bool condition80 = static_cast<bool>(weather_parallax_asset);
      if (condition80) {
        auto result78 = this->root().assets();
        if (!result78)
          return std::unexpected(std::move(result78.error()));
        auto result79 = (std::move(*result78))->asset_exists(*weather_parallax_asset);
        if (!result79)
          return std::unexpected(std::move(result79.error()));
        condition80 = static_cast<bool>((std::move(*result79)));
      }
      if (condition80) {
        if (environment_biome && environment_biome->parallax) {
          auto result81 = environment_biome->parallax->createOverlay(*weather_parallax_asset);
          if (!result81)
            return std::unexpected(std::move(result81.error()));
          m_weatherParallax = (std::move(*result81));
        } else {
          float hue_shift = environment_biome ? environment_biome->hue_shift : 0.0f;
          std::optional<TreeVariant> tree_variant;
          if (environment_biome) {
            if (auto tree = environment_biome->surface_placeables.first_tree())
              tree_variant.emplace(tree->get()); // The parallax retains its own variant.
          }
          auto overlay = Parallax::create(this->root(), *weather_parallax_asset,
              m_worldTemplate->worldSeed(), m_worldTemplate->surfaceLevel(), hue_shift, std::move(tree_variant));
          if (!overlay)
            return std::unexpected(std::move(overlay.error()));
          m_weatherParallax = std::move(*overlay);
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
      render_data.parallax_layers.push_back(std::move(layer));
    }
  }
  
  
  auto result85 = this->root().functionDatabase();
  if (!result85)
    return std::unexpected(std::move(result85.error()));
  auto function_database = (std::move(*result85));
  float wind_interpolation = render_data.sky_render_data.epochTime == m_sky->epochTime() ? 1.0f : interpolation;
  auto wind_time_sample = m_parallaxWindTime.sample(wind_interpolation);
  for (auto& layer : render_data.parallax_layers) {
    if (layer.followsWind || layer.windSpeedMultiplier) {
      double wind_time;
      float horizontal_speed = layer.speed[0];
      if (layer.followsWind) {
        // Parallax texture offsets move the image opposite the offset sign,
        // so negate the magnitude to make positive wind move imagery right.
        horizontal_speed = -std::abs(horizontal_speed);
        wind_time = layer.windSpeedMultiplier
            ? wind_time_sample[2] * *layer.windSpeedMultiplier
            : wind_time_sample[0];
      } else {
        wind_time = wind_time_sample[1] * *layer.windSpeedMultiplier;
      }
      
      layer.parallaxOffset[0] += horizontal_speed * wind_time / m_sky->dayLength();
      layer.speed[0] = 0.0f;
    }
    
    if (!layer.timeOfDayCorrelation.empty()) {
      auto result86 = function_database->function(layer.timeOfDayCorrelation);
      if (!result86)
        return std::unexpected(std::move(result86.error()));
      auto result87 = (std::move(*result86))->evaluate(m_sky->timeOfDay() / m_sky->dayLength());
      if (!result87)
        return std::unexpected(std::move(result87.error()));
      layer.alpha *= std::clamp(static_cast<float>(std::move(*result87)), 0.0f, 1.0f);
    }
  }
  
  std::stable_sort(render_data.parallax_layers.begin(), render_data.parallax_layers.end(), [](ParallaxLayer const& a, ParallaxLayer const& b) {
    return std::tie(a.zLevel, a.verticalOrigin) > std::tie(b.zLevel, b.verticalOrigin);
  });
  
  auto overlay_to_drawable = [&self = *this](WorldStructure::Overlay const& overlay) -> std::expected<Drawable, StarException> {
    auto result88 = self.root().image_metadata_database();
    if (!result88)
      return std::unexpected(std::move(result88.error()));
    auto result89 = Drawable::make_image(*(std::move(*result88)), overlay.image, 1.0f / tile_pixels, false, overlay.min);
    if (!result89)
      return std::unexpected(std::move(result89.error()));
    Drawable drawable = (std::move(*result89));
    drawable.fullbright = overlay.fullbright;
    return drawable;
  };
  
  for (auto const& overlay : m_centralStructure.backgroundOverlays()) {
    auto drawable = overlay_to_drawable(overlay);
    if (!drawable) {
      return std::unexpected(std::move(drawable.error()));
    }
    render_data.background_overlays.push_back(std::move(*drawable));
  }
  for (auto const& overlay : m_centralStructure.foregroundOverlays()) {
    auto drawable = overlay_to_drawable(overlay);
    if (!drawable) {
      return std::unexpected(std::move(drawable.error()));
    }
    render_data.foreground_overlays.push_back(std::move(*drawable));
  }
  
  render_data.is_fullbright = m_fullBright;
  render_data.dim_level = m_worldDimLevel;
  render_data.dim_color = m_worldDimColor;
  return {};
}

List<std::shared_ptr<AudioInstance>> WorldClient::pullPendingAudio() { return std::exchange(m_samples, {}); }

List<std::shared_ptr<AudioInstance>> WorldClient::pull_pending_music() { return std::exchange(m_music, {}); }

void WorldClient::dimWorld() {
  m_world_dim_timer.reset();
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
    } else if (m_parallax_fade_timer.ready() && newParallax != m_currentParallax) {
      m_nextParallax = std::move(newParallax);
      m_parallax_fade_timer.reset();
    } else if (m_nextParallax && newParallax == m_currentParallax) {
      m_currentParallax = m_nextParallax;
      m_nextParallax = std::move(newParallax);
      m_parallax_fade_timer.invert();
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
  auto result90 = root.material_database();
  if (!result90)
    return std::unexpected(std::move(result90.error()));
  auto material_database = (std::move(*result90));
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
      auto type_name = packet_type_name(packet->type());
      if (!type_name)
        return std::unexpected(std::move(type_name.error()));
      auto result94 = this->root().logger().error("WorldClient received packet type {} while not in world", *type_name);
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
      if (m_entityMap->entity(entityCreate->entity_id)) {
        auto result98 = this->root().logger().error("WorldClient received entity create packet with duplicate entity id {}, deleting old entity.", entityCreate->entity_id);
        if (!result98)
          return std::unexpected(std::move(result98.error()));
        auto result99 = removeEntity(entityCreate->entity_id, false);
        if (!result99)
          return std::unexpected(std::move(result99.error()));
      }
      
      auto netRules = m_clientState->netCompatibilityRules();
      auto result100 = entityFactory->net_load_entity(entityCreate->entity_type, entityCreate->store_data, netRules);
      if (!result100)
        return std::unexpected(std::move(result100.error()));
      auto entity = (std::move(*result100));
      auto result101 = entity->read_net_state(entityCreate->first_net_state, 0.0f, netRules);
      if (!result101)
        return std::unexpected(std::move(result101.error()));
      auto result102 = entity->init(*this, entityCreate->entity_id, EntityMode::Slave);
      if (!result102)
        return std::unexpected(std::move(result102.error()));
      auto result103 = m_entityMap->add_entity(entity);
      if (!result103) {
        auto cleared = entity->uninit();
        if (!cleared)
          return std::unexpected(result103.error().with_context(Star::format("Error uninitializing rejected entity: {}", cleared.error().what())));
        return std::unexpected(std::move(result103.error()));
      }
      
      if (m_interpolation_tracker.interpolation_enabled()) {
        auto result104 = entity->enable_interpolation(m_interpolation_tracker.extrapolation_hint());
        if (!result104)
          return std::unexpected(std::move(result104.error()));
        
        // Delay appearance of new slaved entities to match with interplation
        // state.
        m_startupHiddenEntities.add(entityCreate->entity_id);
        timer(m_interpolation_tracker.interpolation_lead_time(), [&self = *this, entityId = entityCreate->entity_id](World&)  -> std::expected<void, StarException> {
          self.m_startupHiddenEntities.remove(entityId);
          return {};
        });
      }
    } else if (auto entity_update_set = as<EntityUpdateSetPacket>(packet)) {
      auto const interpolation_lead_time = m_interpolation_tracker.interpolation_lead_time();
      auto const rules = m_clientState->netCompatibilityRules();
      auto const& deltas = entity_update_set->deltas;
      ByteArray const empty_delta;
      auto applied = m_entityMap->for_all_entities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
        auto const entity_id = entity->entityId();
        if (connection_for_entity(entity_id) != entity_update_set->for_connection)
          return {};
        if constexpr (Star::debug_enabled)
          Star::star_assert_failed((entity->isSlave()));
        auto const delta = deltas.find_ref(entity_id);
        return entity->read_net_state(delta ? delta->get() : empty_delta, interpolation_lead_time, rules);
      });
      if (!applied)
        return std::unexpected(std::move(applied.error()));
    } else if (auto entityDestroy = as<EntityDestroyPacket>(packet)) {
      if (auto entity = m_entityMap->entity(entityDestroy->entity_id)) {
        auto result107 = entity->read_net_state(entityDestroy->final_net_state, m_interpolation_tracker.interpolation_lead_time(), m_clientState->netCompatibilityRules());
        if (!result107)
          return std::unexpected(std::move(result107.error()));
        
        // Before destroying the entity, we should make sure that the entity is
        // using the absolute latest data, so we disable interpolation.
        
        if (m_interpolation_tracker.interpolation_enabled() && entityDestroy->death) {
          // Delay death packets by the interpolation step to give time for
          // interpolation to catch up.
          timer(m_interpolation_tracker.interpolation_lead_time(), [&self = *this, entity, entity_id = entityDestroy->entity_id](World&) -> std::expected<void, StarException> {
            auto result108 = entity->disable_interpolation();
            if (!result108)
              return std::unexpected(std::move(result108.error()));
            auto result109 = self.removeEntity(entity_id, true);
            if (!result109)
              return std::unexpected(std::move(result109.error()));
            return {};
          });
        } else {
          auto result110 = entity->disable_interpolation();
          if (!result110)
            return std::unexpected(std::move(result110.error()));
          auto result111 = removeEntity(entityDestroy->entity_id, entityDestroy->death);
          if (!result111)
            return std::unexpected(std::move(result111.error()));
        }
      }
    } else if (auto structurePacket = as<CentralStructureUpdatePacket>(packet)) {
      auto result112 = WorldStructure::create(structurePacket->structure_data);
      if (!result112)
        return std::unexpected(std::move(result112.error()));
      m_centralStructure = (std::move(*result112));
    } else if (auto tileArrayUpdate = as<TileArrayUpdatePacket>(packet)) {
      RectI tileRegion = RectI::with_size(tileArrayUpdate->min, Vec2I(tileArrayUpdate->array.extents()));
      
      // NOTE: We're creating client side sectors on tileArrayUpdate here, and
      // at no other time, and this is sort of a big assumption that
      // tileArrayUpdate happens for all valid client side sectors first before
      // any other tile updates.
      {
        auto result113 = m_tileArray->valid_sectors_for(tileRegion);
        if (!result113)
          return std::unexpected(std::move(result113.error()));
        for (auto const& sector : (std::move(*result113))) {
          m_tileArray->load_default_sector(sector);
          WorldImpl::invalidate_collision_cache(*m_tileArray, m_tileArray->sector_region(sector));
        }
      }
      
      for (int x = tileRegion.x_min(); x < tileRegion.x_max(); ++x) {
        for (int y = tileRegion.y_min(); y < tileRegion.y_max(); ++y) {
          auto result114 = read_net_tile({x, y}, tileArrayUpdate->array(x - tileRegion.x_min(), y - tileRegion.y_min()), false);
          if (!result114)
            return std::unexpected(std::move(result114.error()));
        }
      }
    } else if (auto tileUpdate = as<TileUpdatePacket>(packet)) {
      auto result115 = read_net_tile(tileUpdate->position, tileUpdate->tile);
      if (!result115)
        return std::unexpected(std::move(result115.error()));
    } else if (auto tileDamageUpdate = as<TileDamageUpdatePacket>(packet)) {
      if (auto tile = m_tileArray->modify_tile(tileDamageUpdate->position)) {
        if (tileDamageUpdate->layer == TileLayer::Foreground) {
          tile->get().foregroundDamage = tileDamageUpdate->tile_damage;
        } else {
          tile->get().backgroundDamage = tileDamageUpdate->tile_damage;
        }
        
        m_damagedBlocks.add(tileDamageUpdate->position);
      }
    } else if (auto tileModificationFailure = as<TileModificationFailurePacket>(packet)) {
      // TODO: Right now we assume that every tile modification was caused by a
      // player, but this may not be true in the future.  In the future, there
      // may be context hints with tile modifications to figure out what to do
      // with failures.
      for (auto& modification : tileModificationFailure->modifications) {
        auto prediction = m_predicted_tiles.find(m_geometry.xwrap(modification.first));
        if (prediction != m_predicted_tiles.end()) {
          auto& p = prediction->second;
          if (std::holds_alternative<PlaceMaterial>(modification.second)) {
            auto const& placeMaterial = std::get<PlaceMaterial>(modification.second);
            if (placeMaterial.layer == TileLayer::Foreground) {
              p.foreground.reset();
              p.foregroundHueShift.reset();
              if (p.collision) {
                invalidate_prediction_change(prediction->first, p.collision, {});
                p.collision.reset();
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
            p.liquid_delta.reset();
          }
          
          if (!p) {
            m_predicted_tiles.erase(prediction);
          }
        }
        
        if (!m_headless) {
          if (std::holds_alternative<PlaceMaterial>(modification.second)) {
            auto result116 = material_database->material_item_drop(std::get<PlaceMaterial>(modification.second).material);
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
            auto result119 = material_database->mod_item_drop(std::get<PlaceMod>(modification.second).mod);
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
    } else if (auto liquid_update = as<TileLiquidUpdatePacket>(packet)) {
      auto prediction = m_predicted_tiles.find(m_geometry.xwrap(liquid_update->position));
      if (prediction != m_predicted_tiles.end()) {
        prediction->second.liquid_remainder.reset();
        prediction->second.liquid_delta.reset();
        if (!prediction->second)
          m_predicted_tiles.erase(prediction);
      }
      if (auto tile = m_tileArray->modify_tile(liquid_update->position)) {
        tile->get().liquid = liquid_update->liquid_update.liquid_level();
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
    } else if (auto step_update = as<StepUpdatePacket>(packet)) {
      m_interpolation_tracker.receive_time_update(step_update->remote_time);
    } else if (auto environmentUpdatePacket = as<EnvironmentUpdatePacket>(packet)) {
      auto result124 = m_sky->readUpdate(environmentUpdatePacket->sky_delta, m_clientState->netCompatibilityRules());
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
      auto result127 = m_weather->readUpdate(environmentUpdatePacket->weather_delta, m_clientState->netCompatibilityRules());
      if (!result127)
        return std::unexpected(std::move(result127.error()));
    } else if (auto hit = as<HitRequestPacket>(packet)) {
      auto result128 = m_damageManager->push_remote_hit_request(hit->remote_hit_request);
      if (!result128)
        return std::unexpected(std::move(result128.error()));
    } else if (auto damage = as<DamageRequestPacket>(packet)) {
      auto result129 = m_damageManager->push_remote_damage_request(damage->remote_damage_request);
      if (!result129)
        return std::unexpected(std::move(result129.error()));
    } else if (auto damage = as<DamageNotificationPacket>(packet)) {
      std::string_view view = damage->remote_damage_notification.damage_notification.target_material_kind.utf8();
      constexpr auto full_size = secret_broadcast_prefix.size() + Curve25519::signature_size;
      constexpr auto legacy_full_size = legacy_voice_prefix.size() + Curve25519::signature_size;

      if (view.size() >= full_size && view.starts_with(secret_broadcast_prefix)) {
        if (auto player = m_entityMap->get<Player>(damage->remote_damage_notification.source_entity_id)) {
          auto public_key = player->getSecretPropertyView(secret_broadcast_public_key);
          auto signature = view.substr(secret_broadcast_prefix.size(), Curve25519::signature_size);
          auto raw_broadcast = view.substr(full_size);
          if (verify_broadcast_signature(player, false,
                  public_key ? std::span<char const>(public_key->utf8()) : std::span<char const>{},
                  std::span<char const>(signature), std::span<char const>(raw_broadcast))) {
            auto handled = handleSecretBroadcast(player, raw_broadcast);
            if (!handled)
              return std::unexpected(std::move(handled.error()));
          }
        }
      } else if (view.size() > legacy_full_size && view.starts_with(legacy_voice_prefix)) {
        // Preserve StarExtensions' signed voice framing on vanilla servers.
        if (auto player = m_entityMap->get<Player>(damage->remote_damage_notification.source_entity_id)) {
          auto public_key = player->effectsAnimator()->global_tag(legacy_voice_public_key);
          auto signature = view.substr(legacy_voice_prefix.size(), Curve25519::signature_size);
          auto raw = view.substr(legacy_full_size);
          if (verify_broadcast_signature(player, true,
                  public_key ? std::span<char const>(public_key->get().utf8()) : std::span<char const>{},
                  std::span<char const>(signature), std::span<char const>(raw)) && m_broadcastCallback) {
            auto broadcast_data = std::string("Voice\0", sizeof("Voice\0") - 1);
            broadcast_data.append(raw);
            auto handled = m_broadcastCallback(player, broadcast_data);
            if (!handled)
              return std::unexpected(std::move(handled.error()));
          }
        }
      } else {
        auto result132 = m_damageManager->push_remote_damage_notification(damage->remote_damage_notification);
        if (!result132)
          return std::unexpected(std::move(result132.error()));
      }
    } else if (auto incoming = packet ? as_ref<EntityMessagePacket>(std::as_const(*packet)) : std::nullopt) {
      auto const& request = incoming->get();
      auto entity = std::holds_alternative<EntityId>(request.entity_id)
          ? m_entityMap->entity(std::get<EntityId>(request.entity_id))
          : m_entityMap->unique_entity(std::get<String>(request.entity_id));

      if (!entity) {
        m_outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(
          std::unexpected(String("Unknown entity")), request.uuid));
      } else if (!entity->isMaster()) {
        auto logged = root.logger().error("Server has sent a scripted entity response for a slave entity");
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        m_outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(
          std::unexpected(String("Entity delivery error")), request.uuid));
      } else {
        std::weak_ptr const request_world = m_entityMap;
        auto received = entity->receive_message(request.from_connection, request.message, request.args);
        if (!received)
          return std::unexpected(std::move(received.error()));
        // A handler can reenter and change worlds. Never answer an old world's request in the new one.
        if (request_world.owner_before(m_entityMap) || m_entityMap.owner_before(request_world))
          continue;
        if (!*received) {
          m_outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(
            std::unexpected(String("Message not handled by entity")), request.uuid));
        } else if (auto result = std::get_if<Json>(&**received)) {
          m_outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(std::move(*result), request.uuid));
        } else {
          m_entity_message_promises.insert_or_assign(request.uuid, std::get<RpcPromise<Json>>(std::move(**received)));
        }
      }
    } else if (auto incoming = packet ? as_ref<EntityMessageResponsePacket>(std::as_const(*packet)) : std::nullopt) {
      auto const& reply = incoming->get();
      if (auto response = m_entityMessageResponses.try_extract_value(reply.uuid)) {
        auto delivered = reply.response ? response->fulfill(*reply.response) : response->fail(reply.response.error());
        if (!delivered)
          return std::unexpected(std::move(delivered.error()));
      } else {
        auto logged = root.logger().warn("EntityMessageResponse received for unknown context [{}]!", reply.uuid.hex());
        if (!logged)
          return std::unexpected(std::move(logged.error()));
      }
    } else if (auto updateWorldProperties = as<UpdateWorldPropertiesPacket>(packet)) {
      // Kae: Properties set to null (nil from Lua) should be erased instead of lingering around
      for (auto&& pair : updateWorldProperties->updated_properties) {
        if (pair.second.is_null()) {
          m_worldProperties.erase(pair.first);
        } else {
          m_worldProperties[pair.first] = pair.second;
        }
      }
    } else if (auto updateTileProtection = as<UpdateTileProtectionPacket>(packet)) {
      setTileProtection(updateTileProtection->dungeon_id, updateTileProtection->is_protected);
    } else if (auto setDungeonGravity = as<SetDungeonGravityPacket>(packet)) {
      if (setDungeonGravity->gravity) {
        m_dungeonIdGravity[setDungeonGravity->dungeon_id] = *setDungeonGravity->gravity;
      } else {
        m_dungeonIdGravity.remove(setDungeonGravity->dungeon_id);
      }
    } else if (auto setDungeonBreathable = as<SetDungeonBreathablePacket>(packet)) {
      if (setDungeonBreathable->breathable.has_value()) {
        m_dungeonIdBreathable[setDungeonBreathable->dungeon_id] = *setDungeonBreathable->breathable;
      } else {
        m_dungeonIdBreathable.remove(setDungeonBreathable->dungeon_id);
      }
    } else if (auto incoming = as<EntityInteractPacket>(packet)) {
      auto promise = interact(incoming->interact_request);
      if (!promise)
        return std::unexpected(std::move(promise.error()));
      auto result = promise->result();
      if (!result)
        return std::unexpected(std::move(result.error()));
      m_outgoingPackets.push_back(std::make_shared<EntityInteractResultPacket>(
          std::move(*result).value_or(InteractAction{}), incoming->request_id, incoming->interact_request.source_id));
    } else if (auto incoming = as<EntityInteractResultPacket>(packet)) {
      if (auto response = m_entity_interaction_responses.try_extract_value(incoming->request_id)) {
        auto delivered = incoming->action ? response->fulfill(incoming->action) : response->fail("no interaction result");
        if (!delivered)
          return std::unexpected(std::move(delivered.error()));
      }
    } else if (auto setPlayerStart = as<SetPlayerStartPacket>(packet)) {
      m_playerStart = setPlayerStart->player_start;
      m_respawnInWorld = setPlayerStart->respawn_in_world;
    } else if (auto findUniqueEntityResponse = as<FindUniqueEntityResponsePacket>(packet)) {
      for (auto& promise : std::exchange(m_findUniqueEntityResponses[findUniqueEntityResponse->unique_entity_id], {})) {
        if (findUniqueEntityResponse->entity_position) {
          auto result145 = promise.fulfill(*findUniqueEntityResponse->entity_position);
          if (!result145)
            return std::unexpected(std::move(result145.error()));
        } else {
          auto result146 = promise.fail("Unknown entity");
          if (!result146)
            return std::unexpected(std::move(result146.error()));
        }
      }
    } else if (auto worldLayoutUpdate = as<WorldLayoutUpdatePacket>(packet)) {
      auto result147 = WorldLayout::create(this->root(), worldLayoutUpdate->layout_data);
      if (!result147)
        return std::unexpected(std::move(result147.error()));
      m_worldTemplate->setWorldLayout((std::move(*result147)));
    } else if (auto worldParametersUpdate = as<WorldParametersUpdatePacket>(packet)) {
      auto result148 = net_load_visitable_world_parameters(worldParametersUpdate->parameters_data);
      if (!result148)
        return std::unexpected(std::move(result148.error()));
      auto result149 = m_worldTemplate->setWorldParameters((std::move(*result148)));
      if (!result149)
        return std::unexpected(std::move(result149.error()));
    } else if (auto pongPacket = as<PongPacket>(packet)) {
      if (pongPacket->time) {
        m_latency = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) - std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>(std::chrono::milliseconds(pongPacket->time));
      } else if (m_pingTime) {
        auto result150 = take_optional(m_pingTime);
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

MessageResponseResult WorldClient::receive_message(ConnectionId from_connection, String const& message, JsonArray const& args) {
  m_expiry_timer.reset();
  if (!inWorld()) {
    // script contexts aren't active to handle this message
    return {};
  }
  for (auto const& entry : m_scriptContexts) {
    auto response = recover_lua_script_error(entry.second->handle_message(message, from_connection == server_connection_id, args));
    if (!response || *response)
      return response;
  }
  return std::nullopt;
}

std::shared_ptr<WorldClient::ScriptComponent> WorldClient::scriptContext(String const& contextName) {
  if (auto context = m_scriptContexts.find_ref(contextName))
    return context->get();
  else
    return nullptr;
}

std::expected<void, StarException> WorldClient::update(float dt) {
  m_expiry_timer.tick(dt);
  if (!inWorld()) {
    return {};
  }
  m_presentationTimestep = dt;
  m_world_dim_timer.tick(dt);
  m_parallax_fade_timer.tick(dt);
  if (m_parallax_fade_timer.ready() && m_nextParallax) {
    m_currentParallax = m_nextParallax;
    m_nextParallax.reset();
  }
  auto beginPresentation = m_entityMap->for_all_entities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    auto& state = m_entityPresentation[entity->entityId()];
    if (state.instance.lock() != entity || state.epoch != entity->presentation_epoch()) {
      state.instance = entity;
      state.epoch = entity->presentation_epoch();
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
  Star::erase_if(m_predicted_tiles, [&, &self = *this](auto&& pair) {
    float expiry = std::chrono::duration<float, std::milli>(float((now - pair.second.time).count())) / expireTime;
    auto center = Vec2F(pair.first) + Vec2F::filled(0.5f);
    auto size = Vec2F::filled(0.875f - expiry * 0.875f);
    auto poly = PolyF(RectF::with_center(center, size));
    self.root().spatialLogger().log_poly("world", poly, Color::palette_cyan.mix(Color::palette_red, expiry).to_rgba());
    if (expiry >= 1.0f) {
      self.invalidate_prediction_change(pair.first, pair.second.collision, {});
      return true;
    } else {
      return false;
    }
  });

  if (!m_headless) {
    // Compare actual tags, not a published-once flag: scripts may replace or
    // remove either tag, and the main player may be rebound on world entry.
    auto public_key = m_mainPlayer->getSecretPropertyView(secret_broadcast_public_key);
    if (!public_key || public_key->utf8() != m_broadcast_public_key.utf8()) {
      auto published = m_mainPlayer->setSecretProperty(secret_broadcast_public_key, m_broadcast_public_key);
      if (!published)
        return std::unexpected(std::move(published.error()));
    }
    auto legacy_key = m_mainPlayer->effectsAnimator()->global_tag(legacy_voice_public_key);
    if (!legacy_key || legacy_key->get() != m_broadcast_public_key)
      m_mainPlayer->effectsAnimator()->set_global_tag(legacy_voice_public_key, m_broadcast_public_key);
  }
  
  ++m_currentStep;
  m_current_time += dt;
  m_interpolation_tracker.update(m_current_time);
  
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
  auto result160 = m_entityMap->update_all_entities([&, &self = *this](std::shared_ptr<Entity> const& entity)  -> std::expected<void, StarException> {
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
          auto result156 = EntityTypeNames->get_right(entity->entity_type());
          if (!result156)
            return std::unexpected(std::move(result156.error()));
          auto result157 = self.root().logger().error("WorldClient: Exception caught in {}::update ({}): {}", result156->get(), entity->entityId(), issue);
          if (!result157)
            return std::unexpected(std::move(result157.error()));
        }
      }
      
      {
        auto result159 = entity->should_destroy();
        if (!result159)
          return std::unexpected(std::move(result159.error()));
        if ((std::move(*result159)) && entity->entityMode() == EntityMode::Master) {
          toRemove.push_back(entity->entityId());
        }
      }
      if (entity->isMaster() && entity->client_entity_mode() == ClientEntityMode::ClientPresenceMaster) {
        clientPresenceEntities.push_back(entity->entityId());
      }
      return {};
    }, [](std::shared_ptr<Entity> const& a, std::shared_ptr<Entity> const& b) {
      return a->entity_type() < b->entity_type();
    });
  if (!result160)
    return std::unexpected(std::move(result160.error()));
  if (m_headless) {
    if (m_entityMap->size() > 0) {
      m_expiry_timer.reset();
    } else if (m_expiry_timer.ready()) {
      // world has had no entities and isn't being used, destroy it
      requestDestroy();
    }
  }
  
  RectI particleRegion;
  if (!m_headless) {
    m_clientState->setPlayer(m_mainPlayer->entityId());
    m_sky->setAltitude(m_clientState->windowCenter()[1]);
    auto clientWindow = m_clientState->window();
    auto result161 = m_clientConfig.get_int("particleRegionPadding");
    if (!result161)
      return std::unexpected(std::move(result161.error()));
    particleRegion = clientWindow.padded((std::move(*result161)));
    // Weather generation is layer-scoped, but the particle manager also owns
    // particles from entities, materials, and effects and must retain the full
    // client region.
    RectI weatherParticleRegion = particleRegion;
    
    std::optional<String> weatherDomain = m_weatherDomain;
    if (clientWindow.is_empty()) {
      weatherParticleRegion = {};
    } else {
      auto result162 = m_worldTemplate->weatherLayerAt(Vec2I::floor(m_clientState->windowCenter()));
      if (!result162)
        return std::unexpected(std::move(result162.error()));
      if (auto layer = (std::move(*result162))) {
        weatherDomain = layer->get().domain;
        weatherParticleRegion = RectI(weatherParticleRegion.x_min(), std::max(weatherParticleRegion.y_min(), layer->get().minHeight),
            weatherParticleRegion.x_max(), std::min(weatherParticleRegion.y_max(), layer->get().maxHeight));
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
    auto result163 = recover_lua_script_error(pair.second->update(pair.second->update_dt(dt)));
    if (!result163)
      return std::unexpected(std::move(result163.error()));
  }

  auto result164 = m_damageManager->update(dt);
  if (!result164)
    return std::unexpected(std::move(result164.error()));
  auto result165 = handleDamageNotifications();
  if (!result165)
    return std::unexpected(std::move(result165.error()));
  
  if (!m_headless) {
    m_parallaxWindTime.beginUpdate(m_sky->epochTime());
    m_sky->beginUpdate();
  }
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
    auto flicker = flickerAmount->to_float();
    if (!flicker)
      return std::unexpected(std::move(flicker.error()));
    auto flickerSample = this->root().random().uniform_float(1 - *flicker, 1);
    if (!flickerSample)
      return std::unexpected(std::move(flickerSample.error()));
    m_inspectionFlickerMultiplier = *flickerSample;
    double parallaxEpoch = m_sky->epochTime();
    auto weather_domain = currentWeatherDomain();
    if (!weather_domain)
      return std::unexpected(std::move(weather_domain.error()));
    bool const weather_in_domain = *weather_domain == m_weatherDomain;
    float parallaxWind = weather_in_domain ? m_weather->wind() : 0.0f;
    m_parallaxWindTime.update(parallaxEpoch, parallaxWind);
    if (!m_mainPlayer->isDead()) {
      // Clear m_requestedDrops every so often in case of entity id reuse or
      // desyncs etc
      {
        auto result168 = m_clientConfig.get_int("itemRequestReset");
        if (!result168)
          return std::unexpected(std::move(result168.error()));
        if (m_currentStep % (std::move(*result168)) == 0) {
          m_requestedDrops.clear();
        }
      }
      
      auto const player_position = m_mainPlayer->position();
      auto drops = m_entityMap->query<ItemDrop>(RectF::with_center(player_position, Vec2F::filled(2.0f * DropDist)));
      if (!drops)
        return std::unexpected(std::move(drops.error()));
      for (auto const& drop : *drops) {
        auto const id = drop->entityId();
        if (m_requestedDrops.contains(id) || !drop->pickup_available()
            || !(m_geometry.diff(drop->position(), player_position).magnitude_squared() < square(DropDist)))
          continue;

        auto capacity = m_mainPlayer->itemsCanHold(drop->item());
        if (!capacity)
          return std::unexpected(std::move(capacity.error()));
        if (*capacity == 0) {
          m_requestedDrops.add(id);
          continue;
        }

        auto taken = drop->take_by(m_mainPlayer->entityId(), static_cast<float>(m_latency.count()) / 1000);
        if (!taken)
          return std::unexpected(std::move(taken.error()));
        if (!*taken)
          continue;
        m_requestedDrops.add(id);
        m_startupHiddenEntities.erase(id);
        m_outgoingPackets.push_back(std::make_shared<RequestDropPacket>(id));
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
    auto directives = directive_parameters();
    auto events = m_entityMap->for_all_entities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
      // The removal path performs the final live-event drain before destroy().
      // Collectors may synthesize effects, so a queued death must not run both.
      if (toRemove.contains(entity->entityId()))
        return {};
      // Offscreen entities still emit once per tick. Startup-hidden entities
      // consume their queues without exposing effects before their spawn.
      bool hidden = m_startupHiddenEntities.contains(entity->entityId());
      std::optional<std::reference_wrapper<List<std::shared_ptr<AudioInstance>>>> deferred_loops;
      if (hidden) {
        auto& pending = m_hiddenEntityAudio[entity->entityId()];
        if (pending.instance.lock() != entity) {
          pending.instance = entity;
          pending.loops.clear();
        }
        deferred_loops = pending.loops;
      }
      NullRenderCallback hidden_events(deferred_loops);
      m_event_render_callback.clear();
      auto collected = entity->collect_render_events(hidden
          ? static_cast<RenderCallback&>(hidden_events)
          : static_cast<RenderCallback&>(m_event_render_callback));
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
      if (auto selected = select_directive(directives, entity->entityId()))
        for (auto& particle : m_event_render_callback.particles)
          particle.directives.append(selected->get());
      m_particles->addParticles(std::move(m_event_render_callback.particles));
      m_samples.append_range(std::move(m_event_render_callback.audios));
      return {};
    });
    if (!events)
      return std::unexpected(std::move(events.error()));
    
    auto const track_position = environment_biome_track_position();
    bool const day_time = m_sky->isDayTime();
    {
      auto ambient = m_worldTemplate->ambient_noises(track_position[0], track_position[1]);
      if (!ambient)
        return std::unexpected(std::move(ambient.error()));
      auto sample = m_ambient_sounds.update_ambient(*ambient, day_time);
      if (!sample)
        return std::unexpected(std::move(sample.error()));
      if (*sample)
        m_samples.push_back(std::move(*sample));
    }
    {
      auto tracks = weather_in_domain ? m_weather->weather_track_options() : std::span<String const>{};
      auto sample = m_ambient_sounds.update_weather(tracks);
      if (!sample)
        return std::unexpected(std::move(sample.error()));
      if (*sample)
        m_samples.push_back(std::move(*sample));
    }
    
    if (inSpace()) {
      auto result182 = m_sky->pullSounds();
      if (!result182)
        return std::unexpected(std::move(result182.error()));
      m_samples.append_range((std::move(*result182)));

      if (m_space_sound && m_space_sound->finished()) {
        m_space_sound.reset();
        m_active_space_sound = "";
      }
      
      auto sky_ambient_noise = m_sky->ambientNoise();
      if (!sky_ambient_noise)
        return std::unexpected(std::move(sky_ambient_noise.error()));
      if (*sky_ambient_noise != m_active_space_sound) {
        if (m_space_sound) {
          m_space_sound->stop(sky_ambient_noise->empty() ? 3.0f : 0.0f);
        } else {
          m_active_space_sound = std::move(*sky_ambient_noise);
          if (!m_active_space_sound.empty()) {
            auto audio = assets->audio(m_active_space_sound);
            if (!audio)
              return std::unexpected(std::move(audio.error()));
            auto instance = AudioInstance::create(**audio);
            if (!instance)
              return std::unexpected(std::move(instance.error()));
            m_space_sound = std::move(*instance);
            m_samples.push_back(m_space_sound);
          }
        }
      }
    }
    
    if (auto request = m_mainPlayer->pull_pending_alt_music()) {
      if (request->tracks)
        play_alt_music(std::move(*request->tracks), request->fade_time, request->loops);
      else
        stop_alt_music(request->fade_time);
    }
    
    if (m_alt_music_description) {
      auto sample = m_alt_music_track.update_ambient(m_alt_music_description, true);
      if (!sample)
        return std::unexpected(std::move(sample.error()));
      if (*sample)
        m_music.push_back(std::move(*sample));
    }
    
    {
      auto music = m_worldTemplate->music_track(track_position[0], track_position[1]);
      if (!music)
        return std::unexpected(std::move(music.error()));
      auto sample = m_music_track.update_ambient(*music, day_time);
      if (!sample)
        return std::unexpected(std::move(sample.error()));
      if (*sample)
        m_music.push_back(std::move(*sample));
    }
  }
  
  for (EntityId entityId : toRemove) {
    auto result189 = removeEntity(entityId, true);
    if (!result189)
      return std::unexpected(std::move(result189.error()));
  }
  if (!m_entity_message_promises.empty()) {
    auto world = m_entityMap;
    auto drained = m_entity_message_promises.drain([&self = *this](Uuid const& uuid, std::expected<Json, String> reply) {
      self.m_outgoingPackets.push_back(std::make_shared<EntityMessageResponsePacket>(std::move(reply), uuid));
    });
    if (!drained)
      return std::unexpected(std::move(drained.error()));
    // Progress may have stopped the world or replaced it before returning.
    if (!m_inWorld || m_entityMap != world)
      return {};
  }
  
  auto result194 = queueUpdatePackets(m_entity_update_timer.wrap_tick(dt));
  if (!result194)
    return std::unexpected(std::move(result194.error()));
  
  if ((!m_clientState->netCompatibilityRules().is_legacy() && m_currentStep % 3 == 0) || !m_pingTime.has_value()) {
    m_pingTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
    m_outgoingPackets.push_back(std::make_shared<PingPacket>(m_pingTime->time_since_epoch().count()));
  }
  
  this->root().logMap().set(Star::format("client_{}_ping",m_subWorldId), m_latency.count());
  
  // Remove active sectors that are outside of the current monitoring region
  Set<ClientTileSectorArray::Sector> neededSectors;
  auto monitoredRegions = m_clientState->monitoringRegions([&self = *this](EntityId entityId) -> std::optional<RectI> {
    if (auto entity = self.entity(entityId)) {
      return RectI::integral(entity->meta_bound_box().translated(entity->position()));
    }
    return {};
  });
  for (auto const& monitoredRegion : monitoredRegions) {
    auto result195 = m_tileArray->valid_sectors_for(monitoredRegion.padded(world_sector_size));
    if (!result195)
      return std::unexpected(std::move(result195.error()));
    neededSectors.insert_range((std::move(*result195)));
  }

  auto loadedSectors = m_tileArray->loaded_sectors();
  for (auto sector : loadedSectors) {
    if (!neededSectors.contains(sector)) {
      m_tileArray->unload_sector(sector);
      WorldImpl::invalidate_collision_cache(*m_tileArray, m_tileArray->sector_region(sector));
    }
  }
  
  if (m_collisionDebug) {
    auto result196 = renderCollisionDebug();
    if (!result196)
      return std::unexpected(std::move(result196.error()));
  }
  
  this->root().logMap().set(Star::format("client_{}_entities",m_subWorldId), m_entityMap->size());
  this->root().logMap().set(Star::format("client_{}_sectors",m_subWorldId), Star::to_string(loadedSectors.size()));
  this->root().logMap().set(Star::format("client_{}_lua_mem",m_subWorldId), m_luaRoot->lua_memory_usage());
  auto endPresentation = m_entityMap->for_all_entities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    auto& state = m_entityPresentation[entity->entityId()];
    if (state.instance.lock() != entity || state.epoch != entity->presentation_epoch()) {
      state.instance = entity;
      state.epoch = entity->presentation_epoch();
      state.position.reset(entity->position());
      state.continuous = false;
    } else {
      // Script-authored setPosition/translate may be continuous movement.
      // Only unusually large unmarked corrections snap; high-speed entities
      // retain an allowance derived from their actual velocity.
      float continuityDistance = 8.0f;
      if (auto mobile = as<MobileEntity>(entity))
        continuityDistance = std::max(continuityDistance, 4.0f * dt * mobile->movement_controller().velocity().magnitude());
      if (!state.position.finish(entity->position(), m_geometry, continuityDistance)) {
        entity->resetPresentation();
        state.epoch = entity->presentation_epoch();
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
    return null_material_id;
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
    return no_mod_id;
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
  if (entity->client_entity_mode() != ClientEntityMode::ClientSlaveOnly) {
    auto reserved = m_entityMap->reserve_entity_id(entityId);
    if (!reserved)
      return std::unexpected(std::move(reserved.error()));
    auto initialized = entity->init(*this, *reserved, EntityMode::Master);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto added = m_entityMap->add_entity(entity);
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
  auto store = (*factory)->net_store_entity(entity, rules);
  if (!store)
    return std::unexpected(std::move(store.error()));
  auto state = entity->write_net_state(0, rules);
  if (!state)
    return std::unexpected(std::move(state.error()));
  m_outgoingPackets.push_back(
    std::make_shared<SpawnEntityPacket>(entity->entity_type(), std::move(*store), std::move(state->first)));
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
    return no_dungeon_id;

  return m_tileArray->tile(pos).dungeonId;
}

std::expected<void, StarException> WorldClient::collectLiquid(List<Vec2I> const& tile_positions, LiquidId liquid_id) {
  if (!inWorld())
    return {};

  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto bucket_size = load_liquid_bucket_size(**assets);
  if (!bucket_size)
    return std::unexpected(std::move(bucket_size.error()));

  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  auto set_remainder = [this, now](Vec2I const& pos, LiquidLevel level) {
    auto& prediction = m_predicted_tiles[pos];
    prediction.time = now;
    prediction.liquid_remainder = level;
    prediction.liquid_delta.reset();
  };
  float next_unit = *bucket_size;
  SmallVector<Vec2I, 8> pending_tiles;
  for (auto const& pos : tile_positions) {
    auto wrapped_pos = m_geometry.xwrap(pos);
    if (isTileProtected(wrapped_pos))
      continue;
    auto liquid = m_tileArray->tile(wrapped_pos).liquid;
    if (auto prediction = m_predicted_tiles.find_ref(wrapped_pos))
      liquid = prediction->get().effective_liquid(liquid);
    if (liquid.liquid != liquid_id)
      continue;

    if (liquid.level >= next_unit) {
      liquid.drain(next_unit);
      next_unit = *bucket_size;
      // A partial bucket does not drain any tile. Once full, clear the
      // contributors in input order, including the current tile if repeated.
      for (auto const& pending_pos : pending_tiles) {
        if (pending_pos == wrapped_pos)
          liquid = {};
        else
          set_remainder(pending_pos, {});
      }
      set_remainder(wrapped_pos, liquid);
      pending_tiles.clear();
    }

    if (liquid.level > 0) {
      next_unit -= liquid.level;
      pending_tiles.push_back(wrapped_pos);
    }
  }

  m_outgoingPackets.push_back(std::make_shared<CollectLiquidPacket>(tile_positions, liquid_id));
  return {};
}

std::expected<bool, StarException> WorldClient::waitForLighting(std::optional<std::reference_wrapper<WorldRenderData>> renderData) {
  std::lock_guard prep_lock(m_light_map_prep_mutex);
  std::lock_guard lightMapLocker(m_lightMapMutex);
  if (renderData && m_lightMap.ready) {
    for (auto& preview_tile : m_preview_tiles) {
      if (preview_tile.update_light) {
        Vec2I lightArrayPos(m_geometry.pdiff(preview_tile.position[0], m_lightMap.minPosition[0]), preview_tile.position[1] - m_lightMap.minPosition[1]);
        if (lightArrayPos[0] >= 0 && lightArrayPos[0] < static_cast<int>(m_lightMap.lightMap.width()) && lightArrayPos[1] >= 0
            && lightArrayPos[1] < static_cast<int>(m_lightMap.lightMap.height())) {
          auto result202 = m_lightMap.lightMap.set(lightArrayPos[0], lightArrayPos[1], Color::rgb_bytes_to_float(preview_tile.light));
          if (!result202)
            return std::unexpected(std::move(result202.error()));
        }
      }
    }
    return m_lightMap.take(renderData->get().light_map, renderData->get().light_min_position);
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

  m_outgoingPackets.push_back(std::make_shared<StepUpdatePacket>(m_current_time));

  {
    auto result205 = m_clientConfig.get_int("worldClientStateUpdateDelta");
    if (!result205)
      return std::unexpected(std::move(result205.error()));
    if (m_currentStep % (std::move(*result205)) == 0) {
      auto result206 = m_clientState->writeDelta();
      if (!result206)
        return std::unexpected(std::move(result206.error()));
      m_outgoingPackets.push_back(std::make_shared<WorldClientStateUpdatePacket>((std::move(*result206))));
    }
  }
  
  auto result208 = m_entityMap->for_all_entities([&self = *this](std::shared_ptr<Entity> const& entity)  -> std::expected<void, StarException> {
      auto result207 = self.notifyEntityCreate(entity);
      if (!result207)
        return std::unexpected(std::move(result207.error()));
      return {};
    });
  if (!result208)
    return std::unexpected(std::move(result208.error()));
  
  if (sendEntityUpdates) {
    auto entityUpdateSet = std::make_shared<EntityUpdateSetPacket>();
    entityUpdateSet->for_connection = *m_clientId;
    auto netRules = m_clientState->netCompatibilityRules();
    auto result210 = m_entityMap->for_all_entities(
      [&, &self = *this](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
        if (auto version = self.m_masterEntitiesNetVersion.find_ref(entity->entityId())) {
          auto result209 = entity->write_net_state(version->get(), netRules);
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
  
  for (auto& remoteHitRequest : m_damageManager->pull_remote_hit_requests()) {
    m_outgoingPackets.push_back(std::make_shared<HitRequestPacket>(std::move(remoteHitRequest)));
  }
  for (auto& remoteDamageRequest : m_damageManager->pull_remote_damage_requests()) {
    m_outgoingPackets.push_back(std::make_shared<DamageRequestPacket>(std::move(remoteDamageRequest)));
  }
  for (auto& remoteDamageNotification : m_damageManager->pull_remote_damage_notifications()) {
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
    m_damageManager->pull_pending_notifications();
    return {};
  }
  for (auto const& damageNotification : m_damageManager->pull_pending_notifications()) {
    auto result215 = this->root().damageDatabase();
    if (!result215)
      return std::unexpected(std::move(result215.error()));
    auto damageDatabase = (std::move(*result215));
    auto result216 = damageDatabase->damageKind(damageNotification.damage_source_kind);
    if (!result216)
      return std::unexpected(std::move(result216.error()));
    DamageKind const& damageKind = result216->get();
    auto result217 = damageDatabase->elementalType(damageKind.elementalType);
    if (!result217)
      return std::unexpected(std::move(result217.error()));
    ElementalType const& elementalType = result217->get();
    
    auto result218 = elementalType.damageNumberParticles.get(damageNotification.hit_type);
    if (!result218)
      return std::unexpected(std::move(result218.error()));
    auto damageNumberParticleKind = result218->get();
    auto damageNumberKey = DamageNumberKey{.damageNumberParticleKind = damageNumberParticleKind,
                                           .sourceEntityId = damageNotification.source_entity_id,
                                           .targetEntityId = damageNotification.target_entity_id};

    DamageNumber number;
    if (m_damageNumbers.contains(damageNumberKey)) {
      auto result219 = m_damageNumbers.extract_value(damageNumberKey);
      if (!result219)
        return std::unexpected(std::move(result219.error()));
      number = (std::move(*result219));
      
      if (damageNotification.hit_type == HitType::Kill) {
        auto result220 = renderParticle(damageNotification.position,
                    damageNotification.damage_dealt + number.amount,
                    damageNumberKey.damageNumberParticleKind);
        if (!result220)
          return std::unexpected(std::move(result220.error()));
      }
    } else {
      if (damageNotification.hit_type == HitType::Kill) {
        auto result221 = renderParticle(damageNotification.position, damageNotification.damage_dealt, damageNumberParticleKind);
        if (!result221)
          return std::unexpected(std::move(result221.error()));
      }
      number.amount = 0;
      number.timestamp = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
    }
    
    if (damageNotification.hit_type != HitType::Kill) {
      number.position = damageNotification.position;
      number.amount += damageNotification.damage_dealt;
      m_damageNumbers[damageNumberKey] = number;
    }
    
    String material = damageNotification.target_material_kind;
    if (!material.empty() && damageKind.effects.contains(material)) {
      // default to normal hit
      auto result222 = damageKind.effects.get(material);
      if (!result222)
        return std::unexpected(std::move(result222.error()));
      HitType effectHitType = result222->get().contains(damageNotification.hit_type) ? damageNotification.hit_type : HitType::Hit;
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
      if (auto selected = select_directive(directive_parameters(), damageNotification.target_entity_id))
        for (auto& particle : hitParticles)
          particle.directives.append(selected->get());
      m_particles->addParticles(std::move(hitParticles));
    }
  }
  return {};
}

std::expected<void, StarException> WorldClient::sparkDamagedBlocks() {
  if (!inWorld()) {
    return {};
  }
  
  auto result229 = this->root().material_database();
  if (!result229)
    return std::unexpected(std::move(result229.error()));
  auto material_database = (std::move(*result229));
  
  for (auto pos : m_damagedBlocks.values()) {
    if (auto tile = m_tileArray->modify_tile(pos)) {
      if (tile->get().backgroundDamage.healthy() && tile->get().foregroundDamage.healthy()) {
        m_damagedBlocks.remove(pos);
      }
      
      if (is_real_material(tile->get().foreground) && tile->get().foregroundDamage.damage_effect_percentage() - this->root().random().uniform_float() > 0.0f
                && (this->root().random().uniform_float() < m_blockDamageParticleProbability)) {
        auto particle = m_blockDamageParticle;
        auto result230 = material_database->material_particle_color(tile->get().foreground, tile->get().foregroundHueShift);
        if (!result230)
          return std::unexpected(std::move(result230.error()));
        particle.color = (std::move(*result230));
        
        if (isTileProtected(pos))
          particle = m_blockDingParticle;
        
        particle.position += center_of_tile(pos);
        particle.velocity = particle.velocity.magnitude()
            * vnorm(m_geometry.diff(tile->get().foregroundDamage.source_position(), particle.position));
        particle.applyVariance(this->root(), m_blockDamageParticleVariance);
        m_particles->add(particle);
      }
      
      if (is_real_material(tile->get().background) && tile->get().backgroundDamage.damage_effect_percentage() - this->root().random().uniform_float() > 0.0f
                && (this->root().random().uniform_float() < m_blockDamageParticleProbability)) {
        auto particle = m_blockDamageParticle;
        auto result232 = material_database->material_particle_color(tile->get().background, tile->get().backgroundHueShift);
        if (!result232)
          return std::unexpected(std::move(result232.error()));
        particle.color = (std::move(*result232));
        
        if (isTileProtected(pos))
          particle = m_blockDingParticle;
        
        particle.position += center_of_tile(pos);
        particle.velocity = particle.velocity.magnitude()
            * vnorm(m_geometry.diff(tile->get().backgroundDamage.source_position(), particle.position));
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
  return WorldImpl::can_reach_entity(m_geometry, *m_tileArray, *m_entityMap, position, radius, targetEntity, preferInteractive);
}

std::expected<RpcPromise<InteractAction>, StarException> WorldClient::interact(InteractRequest const& request) {
  if (!inWorld()) {
    return RpcPromise<InteractAction>::make_failed("not initialized in world");
  }
  
  if (auto target = m_entityMap->entity(request.target_id); target && target->isMaster()) {
    if (auto interactive = as<InteractiveEntity>(target)) {
      auto action = interactive->interact(request);
      if (!action)
        return std::unexpected(std::move(action.error()));
      return RpcPromise<InteractAction>::make_fulfilled(std::move(*action));
    }
    return RpcPromise<InteractAction>::make_fulfilled(InteractAction{});
  }
  
  auto [promise, keeper] = RpcPromise<InteractAction>::make_pair();
  Uuid request_id(root().random());
  m_entity_interaction_responses[request_id] = std::move(keeper);
  m_outgoingPackets.push_back(std::make_shared<EntityInteractPacket>(request, request_id));
  return promise;
}

std::expected<void, StarException> gatherClientLightingTiles(CellularLightingCalculator& calculator,
                                                             ClientTileSectorArray& tiles, WorkerPool& workers,
                                                             MaterialDatabase const& materials,
                                                             LiquidsDatabase const& liquids,
                                                             Vec3F const& environmentLight, float undergroundLevel) {
  // Each column is guaranteed to be no larger than the sector size.
  auto gatherColumn = [&](Vec2I const& pos, std::span<ClientTile const> column) -> std::expected<void, StarException> {
    std::size_t baseIndex = calculator.base_index_for(pos);
    for (std::size_t y = 0; y < column.size(); ++y) {
      auto& tile = column[y];
      Vec3F light;
      if (tile.foreground != empty_material_id || tile.foregroundMod != no_mod_id)
        light += materials.radiant_light(tile.foreground, tile.foregroundMod);
      if (tile.liquid.liquid != empty_liquid_id && tile.liquid.level != 0.0f)
        light += liquids.radiant_light(tile.liquid);
      if (tile.foregroundLightTransparent) {
        if (tile.background != empty_material_id || tile.backgroundMod != no_mod_id)
          light += materials.radiant_light(tile.background, tile.backgroundMod);
        if (tile.backgroundLightTransparent && pos[1] + y > undergroundLevel)
          light += environmentLight;
      }
      calculator.set_cell_index(baseIndex + y, light, !tile.foregroundLightTransparent);
    }
    return {};
  };

  RectI region = calculator.calculation_region();
  int worldWidth = static_cast<int>(tiles.size()[0]);
  int worldHeight = static_cast<int>(tiles.size()[1]);
  int gatheredRows = std::clamp(region.y_max(), 0, worldHeight) - std::clamp(region.y_min(), 0, worldHeight);
  // Real-asset gather measurements put small clipped regions below the useful
  // parallel grain. Keep the larger/interior cases on their existing workers.
  constexpr std::int64_t CallerGatherCellLimit = 12 * 1024;
  bool useCaller = static_cast<std::int64_t>(region.width()) * gatheredRows <= CallerGatherCellLimit;
  if (region.width() < worldWidth && region.x_min() >= -worldWidth
      && static_cast<std::int64_t>(region.x_max()) <= 2 * static_cast<std::int64_t>(worldWidth)) {
    if (useCaller)
      return tiles.tile_eval_columns(region, gatherColumn);
    return tiles.tile_eval_columns_parallel(workers, region, gatherColumn);
  }

  // Whole-world tile traversal deduplicates wrapped x coordinates. Lighting
  // instead needs every image in its unwrapped calculation region, including
  // the configured border around large viewports and small worlds.
  for (int destinationX = region.x_min(); destinationX < region.x_max();) {
    int sourceX = positive_mod<int>(destinationX, worldWidth);
    int stripWidth = std::min(region.x_max() - destinationX, worldWidth - sourceX);
    RectI strip(sourceX, region.y_min(), sourceX + stripWidth, region.y_max());
    auto gatherStrip = [&](Vec2I const& pos, std::span<ClientTile const> column) -> std::expected<void, StarException> {
      return gatherColumn({destinationX + (pos[0] - sourceX), pos[1]}, column);
    };
    auto gathered = useCaller ? tiles.tile_eval_columns(strip, gatherStrip)
                              : tiles.tile_eval_columns_parallel(workers, strip, gatherStrip);
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
  Vec3F environmentLight = m_sky->environmentLight().to_rgb_f();
  float undergroundLevel = m_worldTemplate->undergroundLevel();
  auto liquids_database = this->root().liquids_database();
  if (!liquids_database)
    return std::unexpected(std::move(liquids_database.error()));
  auto result235 = this->root().material_database();
  if (!result235)
    return std::unexpected(std::move(result235.error()));
  auto material_database = (std::move(*result235));

  auto result236 = this->root().workers();
  if (!result236)
    return std::unexpected(std::move(result236.error()));
  auto result237 = gatherClientLightingTiles(m_lighting_calculator, *m_tileArray, result236->get(), *material_database,
                                             **liquids_database, environmentLight, undergroundLevel);
  if (!result237)
    return std::unexpected(std::move(result237.error()));
  this->root().logMap().set("client_render_world_async_light_gather", Star::format("{:05d}\u00b5s", (std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500)) - start).count()));
  return {};
}

std::expected<void, StarException> WorldClient::calculate_lighting() {
  auto prep_wait_start = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
  std::unique_lock prep_lock(m_light_map_prep_mutex);
  auto prep_start = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
  if (!m_pending_light_ready.load()) {
    return {};
  }
  m_pending_light_ready = false;
  RectI light_range = m_pending_light_range;
  m_calculation_lights.swap(m_pending_lights);
  List<std::pair<Vec2F, Vec3F>> particle_lights = std::move(m_pending_particle_lights);
  auto& root = this->root();
  auto configuration = root.configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto mode = (*configuration)->get("newLighting").and_then([](Json const& value) { return value.opt_bool(); });
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  bool new_lighting = mode->value_or(true);
  auto monochrome = (*configuration)->get("monochromeLighting").and_then([](Json const& value) { return value.to_bool(); });
  if (!monochrome)
    return std::unexpected(std::move(monochrome.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto parameters = (*assets)->json("/lighting.config:lighting").and_then([new_lighting](Json const& value) {
    return value.set("pointAdditive", new_lighting);
  });
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (auto configured = m_lighting_calculator.set_parameters(*parameters); !configured)
    return std::unexpected(std::move(configured.error()));
  if (auto configured = m_lighting_calculator.set_monochrome(*monochrome); !configured)
    return std::unexpected(std::move(configured.error()));
  auto begun = m_lighting_calculator.begin(light_range);
  if (!begun) return std::unexpected(std::move(begun.error()));
  auto result248 = lightingTileGather();
  if (!result248)
    return std::unexpected(std::move(result248.error()));

  auto prep_end = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
  prep_lock.unlock();
  
  int region_min_x = m_lighting_calculator.calculation_region().min()[0];
  auto resolve_position = [this, region_min_x](Vec2F pos) {
    float x_floor = std::floor(pos[0]);
    float offset = static_cast<float>(m_geometry.pdiff(static_cast<int>(x_floor), region_min_x)) + (pos[0] - x_floor);
    return Vec2F(region_min_x + offset, pos[1]);
  };
  
  for (auto const& light : m_calculation_lights) {
    auto position = resolve_position(light.position);
    bool hybrid = light.type == LightType::PointAsSpread;
    if (light.type == LightType::Spread || (hybrid && !new_lighting)) {
      m_lighting_calculator.add_spread_light(position, light.color);
      continue;
    }
    // The calculator applies the hybrid point component's 15% weight.
    if (hybrid)
      m_lighting_calculator.add_spread_light(position, light.color * 0.85f);
    m_lighting_calculator.add_point_light(position, light.color, light.point_beam, light.beam_angle, light.beam_ambience, hybrid);
  }
  
  for (auto const& [position, color] : particle_lights)
    m_lighting_calculator.add_spread_light(resolve_position(position), color);
  
  auto result249 = m_lighting_calculator.calculate(m_pending_light_map);
  if (!result249)
    return std::unexpected(std::move(result249.error()));
  auto propagation_end = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::microseconds> publication_start;
  {
    std::lock_guard map_lock(m_lightMapMutex);
    publication_start = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
    m_lightMap.publish(m_pending_light_map, light_range.min());
  }
  auto publication_end = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
  root.logMap().set("client_render_world_async_light_prep_wait", Star::format("{:05d}\u00b5s", (prep_start - prep_wait_start).count()));
  root.logMap().set("client_render_world_async_light_prep", Star::format("{:05d}\u00b5s", (prep_end - prep_start).count()));
  root.logMap().set("client_render_world_async_light_propagation", Star::format("{:05d}\u00b5s", (propagation_end - prep_end).count()));
  root.logMap().set("client_render_world_async_light_publication_wait",
                    Star::format("{:05d}\u00b5s", (publication_start - propagation_end).count()));
  root.logMap().set("client_render_world_async_light_publication",
                    Star::format("{:05d}\u00b5s", (publication_end - publication_start).count()));
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
    auto result250 = calculate_lighting();
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
    auto settings = m_clientConfig.query(startPacket.local_interpolation_mode ? String("interpolationSettings.local") : String("interpolationSettings.normal"));
    if (!settings) return std::unexpected(std::move(settings.error()));
    auto tracker = InterpolationTracker::create(root(), *settings);
    if (!tracker) return std::unexpected(std::move(tracker.error()));
    m_interpolation_tracker = std::move(*tracker);
    m_entity_update_timer = GameTimer(m_interpolation_tracker.entity_update_delta());
    
    m_clientId = startPacket.client_id;
    auto result257 = connection_entity_space(startPacket.client_id);
    if (!result257)
      return std::unexpected(std::move(result257.error()));
    auto entitySpace = (std::move(*result257));
    auto result258 = WorldTemplate::create(this->root(), startPacket.template_data);
    if (!result258)
      return std::unexpected(std::move(result258.error()));
    m_worldTemplate = (std::move(*result258));
    m_entityMap = std::make_shared<EntityMap>(m_worldTemplate->size(), entitySpace.first, entitySpace.second);
    m_tileArray = std::make_shared<ClientTileSectorArray>(m_worldTemplate->size());
    m_tileGetterFunction = [&self = *this, tile = ClientTile()](Vec2I pos) mutable -> std::expected<std::reference_wrapper<ClientTile const>, StarException> {
      if (!self.m_predicted_tiles.empty()) {
        if (auto p = self.m_predicted_tiles.find_ref(self.m_geometry.xwrap(pos))) {
          p->get().apply(tile = self.m_tileArray->tile(pos));
          tile.liquid = p->get().effective_liquid(tile.liquid);
          return std::cref(tile);
        }
      }
      return std::cref(self.m_tileArray->tile(pos));
    };
    m_damageManager = std::make_shared<DamageManager>(*this, startPacket.client_id);
    m_playerStart = startPacket.player_respawn;
    m_respawnInWorld = startPacket.respawn_in_world;
    auto result259 = startPacket.world_properties.opt_object();
    if (!result259)
      return std::unexpected(std::move(result259.error()));
    m_worldProperties = (std::move(*result259)).value_or(JsonObject{});
    m_dungeonIdGravity = startPacket.dungeon_id_gravity;
    m_dungeonIdBreathable = startPacket.dungeon_id_breathable;
    m_protectedDungeonIds = startPacket.protected_dungeon_ids;
    
    m_geometry = WorldGeometry(m_worldTemplate->size());
    
    m_particles = std::make_shared<ParticleManager>(this->root(), m_geometry, m_tileArray);
    m_particles->setUndergroundLevel(m_worldTemplate->undergroundLevel());
    
    if (auto result = setup_force_regions(); !result)
      return std::unexpected(std::move(result.error()));
    
    auto result261 = Sky::create(this->root());
    if (!result261)
      return std::unexpected(std::move(result261.error()));
    m_sky = (std::move(*result261));
    auto result262 = m_sky->readUpdate(startPacket.sky_data, m_clientState->netCompatibilityRules());
    if (!result262)
      return std::unexpected(std::move(result262.error()));
    
    m_weather->setup(m_geometry, [&self = *this](Vec2I const& pos)  -> std::expected<bool, StarException> {
      auto const& tile = self.m_tileArray->tile(pos);
      return !is_real_material(tile.background) && !isSolidColliding(tile.getCollision());
    });
    {
      auto result263 = m_worldTemplate->weatherLayerAt(Vec2I::floor(startPacket.player_start));
      if (!result263)
        return std::unexpected(std::move(result263.error()));
      if (auto layer = (std::move(*result263))) {
        m_weatherDomain = layer->get().domain;
      }
    }
    auto result264 = m_weather->readUpdate(startPacket.weather_data, m_clientState->netCompatibilityRules());
    if (!result264)
      return std::unexpected(std::move(result264.error()));
    
    auto result265 = this->root().configuration();
    if (!result265)
      return std::unexpected(std::move(result265.error()));
    auto result266 = (std::move(*result265))->get("monochromeLighting");
    if (!result266)
      return std::unexpected(std::move(result266.error()));
    auto result267 = (std::move(*result266)).to_bool();
    if (!result267)
      return std::unexpected(std::move(result267.error()));
    auto result268 = m_lighting_calculator.set_monochrome((std::move(*result267)));
    if (!result268)
      return std::unexpected(std::move(result268.error()));
    auto result269 = assets->json("/lighting.config:lighting");
    if (!result269)
      return std::unexpected(std::move(result269.error()));
    auto result270 = m_lighting_calculator.set_parameters((std::move(*result269)));
    if (!result270)
      return std::unexpected(std::move(result270.error()));
    auto result271 = assets->json("/lighting.config:intensity");
    if (!result271)
      return std::unexpected(std::move(result271.error()));
    auto result272 = m_lightIntensityCalculator.set_parameters((std::move(*result271)));
    if (!result272)
      return std::unexpected(std::move(result272.error()));
    
    m_inWorld = true;
    if (!m_headless) {
      if (!m_mainPlayer->isDead()) {
        auto result273 = m_entityMap->reserve_entity_id();
        if (!result273)
          return std::unexpected(std::move(result273.error()));
        auto result274 = m_mainPlayer->init(*this, (std::move(*result273)), EntityMode::Master);
        if (!result274)
          return std::unexpected(std::move(result274.error()));
        auto result275 = m_entityMap->add_entity(m_mainPlayer);
        if (!result275) {
          auto cleared = m_mainPlayer->uninit();
          if (!cleared)
            return std::unexpected(result275.error().with_context(Star::format("Error uninitializing rejected entity: {}", cleared.error().what())));
          return std::unexpected(std::move(result275.error()));
        }
      }
      auto result276 = m_mainPlayer->moveTo(startPacket.player_start);
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
      auto result279 = m_clientConfig.get_object("worldScriptContexts");
      if (!result279)
        return std::unexpected(std::move(result279.error()));
      for (auto&& p : (std::move(*result279))) {
        auto scriptComponent = std::make_shared<ScriptComponent>(this->root());
        auto result280 = p.second.to_array();
        if (!result280)
          return std::unexpected(std::move(result280.error()));
        auto result281 = json_to_string_list((std::move(*result280)));
        if (!result281)
          return std::unexpected(std::move(result281.error()));
        auto result282 = scriptComponent->set_scripts((std::move(*result281)));
        if (!result282)
          return std::unexpected(std::move(result282.error()));
        
        m_scriptContexts.set(p.first, scriptComponent);
        auto result283 = recover_lua_script_error(scriptComponent->init(*this));
        if (!result283)
          return std::unexpected(std::move(result283.error()));
      }
    }
    {
      auto result284 = m_clientConfig.get_object(m_headless ? std::string_view("subWorldScriptContexts") : std::string_view("mainWorldScriptContexts"));
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
        auto result286 = p.second.to_array();
        if (!result286)
          return std::unexpected(std::move(result286.error()));
        auto result287 = json_to_string_list((std::move(*result286)));
        if (!result287)
          return std::unexpected(std::move(result287.error()));
        auto result288 = scriptComponent->set_scripts((std::move(*result287)));
        if (!result288)
          return std::unexpected(std::move(result288.error()));
        
        m_scriptContexts.set(p.first, scriptComponent);
        auto result289 = recover_lua_script_error(scriptComponent->init(*this));
        if (!result289)
          return std::unexpected(std::move(result289.error()));
      }
    }
    return {};
}

std::expected<void, StarException> WorldClient::initWorld(WorldStartPacket const& startPacket) {
  if (!m_headless) {
    auto const context = m_mainPlayer->client_context();
    if (!context || context->connection_id() != startPacket.client_id)
      return std::unexpected(WorldClientException("WorldStart client ID does not match ConnectSuccess"));
  }
  auto initialized = initializeWorld(startPacket);
  if (!initialized) {
    auto cleared = clearWorld();
    if (!cleared)
      return std::unexpected(initialized.error().with_context(Star::format("Error rolling back world initialization: {}", cleared.error().what())));
  }
  return initialized;
}

std::expected<void, StarException> WorldClient::clearWorld() {
  {
    // Drain propagation too: it runs after releasing the prep mutex and must
    // not publish an old world's map after this reset.
    std::lock_guard lightingLocker(m_lightingMutex);
    std::lock_guard prep_lock(m_light_map_prep_mutex);
    std::lock_guard map_lock(m_lightMapMutex);
    m_pending_light_ready = false;
    m_gather_lights = List<LightSource>{};
    m_pending_lights = List<LightSource>{};
    m_calculation_lights = List<LightSource>{};
    m_pending_particle_lights.clear();
    m_lightMap.reset();
  }
  resetPresentation();
  m_hiddenEntityAudio.clear();
  m_broadcast_verifiers.clear();
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
    remember(recover_lua_script_error(p.second->invoke("preUninit")));
  if (m_entityMap) {
    for (auto entityId : m_entityMap->entity_ids()) {
      auto entity = m_entityMap->entity(entityId);
      remember(removeEntity(entityId, false));
      // A failed final net snapshot must not leave an initialized entity attached.
      if (m_entityMap->entity(entityId)) {
        m_entityMap->remove_entity(entityId);
        remember(entity->uninit());
      }
    }
  }
  for (auto&& p : m_scriptContexts)
    remember(recover_lua_script_error(p.second->uninit()));
  m_scriptContexts.clear();

  m_currentStep = 0;
  m_current_time = 0;
  m_inWorld = false;
  m_clientId.reset();
  m_entity_message_promises.clear();

  m_interpolation_tracker = InterpolationTracker();

  m_masterEntitiesNetVersion.clear();
  m_outgoingPackets.clear();

  m_pingTime.reset();

  m_entityMap.reset();
  m_worldTemplate.reset();
  m_worldProperties.clear();

  m_tileArray.reset();
  m_predicted_tiles.clear();

  m_damageManager.reset();

  m_particles.reset();

  m_sky.reset();

  m_currentParallax.reset();
  m_nextParallax.reset();
  m_weatherParallaxAsset.reset();
  m_weatherParallaxBiome.reset();
  m_weatherParallax.reset();
  m_weatherDomain.reset();
  m_parallaxWindTime = {};
  if (m_weather)
    remember(m_weather->clear());
  m_parallax_fade_timer.set_done();

  if (m_clientState)
    m_clientState->reset();
  m_ambient_sounds.cancel_all();
  m_music_track.cancel_all();
  m_music_track.set_volume(1, 0, 0);
  m_alt_music_track.cancel_all();
  m_alt_music_description.reset();
  m_music.clear();

  if (m_space_sound) {
    m_space_sound->stop();
    m_space_sound.reset();
  }
  m_active_space_sound = "";

  for (auto&& response : std::exchange(m_entityMessageResponses, {}))
    remember(response.second.cancel("Client world closed"));
  for (auto&& responses : std::exchange(m_findUniqueEntityResponses, {})) {
    for (auto& response : responses.second)
      remember(response.cancel("Client world closed"));
  }
  for (auto&& response : std::exchange(m_entity_interaction_responses, {}))
    remember(response.second.cancel("Client world closed"));

  m_force_regions.clear();
  
  m_expiry_timer.reset();
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
      auto result291 = ItemDrop::create_randomized_drop(this->root(), spill->descriptor(), m_mainPlayer->position());
      if (!result291)
        return std::unexpected(std::move(result291.error()));
      auto result292 = addEntity((std::move(*result291)), null_entity_id);
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
    auto result293 = entity->write_net_state(0, netRules);
    if (!result293)
      return std::unexpected(std::move(result293.error()));
    auto firstNetState = (std::move(*result293));
    m_masterEntitiesNetVersion[entity->entityId()] = firstNetState.second;
    auto result294 = this->root().entityFactory();
    if (!result294)
      return std::unexpected(std::move(result294.error()));
    auto result295 = (std::move(*result294))->net_store_entity(entity, netRules);
    if (!result295)
      return std::unexpected(std::move(result295.error()));
    m_outgoingPackets.push_back(std::make_shared<EntityCreatePacket>(
      entity->entity_type(), (std::move(*result295)), std::move(firstNetState.first), entity->entityId()));
  }
  return {};
}

List<EntityId> WorldClient::entity_ids() const {
  return m_entityMap->entity_ids();
}

Vec2I WorldClient::environment_biome_track_position() const {
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
  if (window.is_empty()) {
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

void WorldClient::play_alt_music(StringList tracks, float fade_time, int loops) {
  if (!m_alt_music_description)
    m_music_track.set_volume(0.0f, 0.0f, fade_time);
  m_alt_music_description = std::make_shared<AmbientNoisesDescription const>(
      AmbientTrackGroup(std::move(tracks)), AmbientTrackGroup{}, loops);
}

void WorldClient::stop_alt_music(float fade_time) {
  if (!m_alt_music_description)
    return;
  m_alt_music_description.reset();
  m_alt_music_track.stop_ambient(fade_time);
  m_music_track.set_volume(1.0f, 0.0f, fade_time);
}

std::expected<std::shared_ptr<Biome const>, StarException> WorldClient::mainEnvironmentBiome() const {
  if (!inWorld()) {
    return {};
  }
  
  Vec2I pos = environment_biome_track_position();
  return m_worldTemplate->environmentBiome(pos[0], pos[1]);
}

std::expected<bool, StarException> WorldClient::read_net_tile(Vec2I const& pos, NetTile const& net_tile, bool invalidate) {
  auto tile = m_tileArray->modify_tile(pos);
  if (!tile) {
    return false;
  }
  
  bool collision_changed = invalidate && tile->get().collision != net_tile.collision;
  if (!m_predicted_tiles.empty()) {
    auto prediction = m_predicted_tiles.find(m_geometry.xwrap(pos));
    if (prediction != m_predicted_tiles.end()) {
      auto& p = prediction->second;
      
      // Whether confirmed or retained, an override keeps the same effective kind.
      collision_changed &= !p.collision;
      if (p.collision && *p.collision == net_tile.collision) {
        p.collision.reset();
      }
      if (p.foreground && (*p.foreground == structure_material_id || *p.foreground == net_tile.foreground)) {
        p.foreground.reset();
      }
      if (p.foregroundMod && *p.foregroundMod == net_tile.foregroundMod) {
        p.foregroundMod.reset();
      }
      if (p.foregroundHueShift && *p.foregroundHueShift == net_tile.foregroundHueShift) {
        p.foregroundHueShift.reset();
      }
      if (p.foregroundModHueShift && *p.foregroundModHueShift == net_tile.foregroundModHueShift) {
        p.foregroundModHueShift.reset();
      }
      
      if (p.background && *p.background == net_tile.background) {
        p.background.reset();
      }
      if (p.backgroundMod && *p.backgroundMod == net_tile.backgroundMod) {
        p.backgroundMod.reset();
      }
      if (p.backgroundHueShift && *p.backgroundHueShift == net_tile.backgroundHueShift) {
        p.backgroundHueShift.reset();
      }
      if (p.backgroundModHueShift && *p.backgroundModHueShift == net_tile.backgroundModHueShift) {
        p.backgroundModHueShift.reset();
      }
      p.liquid_remainder.reset();
      p.liquid_delta.reset();
      
      if (!p) {
        m_predicted_tiles.erase(prediction);
      }
    }
  }
  
  tile->get().background = net_tile.background;
  tile->get().backgroundHueShift = net_tile.backgroundHueShift;
  tile->get().backgroundColorVariant = net_tile.backgroundColorVariant;
  tile->get().backgroundMod = net_tile.backgroundMod;
  tile->get().backgroundModHueShift = net_tile.backgroundModHueShift;
  tile->get().foreground = net_tile.foreground;
  tile->get().foregroundHueShift = net_tile.foregroundHueShift;
  tile->get().foregroundColorVariant = net_tile.foregroundColorVariant;
  tile->get().foregroundMod = net_tile.foregroundMod;
  tile->get().foregroundModHueShift = net_tile.foregroundModHueShift;
  tile->get().collision = net_tile.collision;
  tile->get().blockBiomeIndex = net_tile.blockBiomeIndex;
  tile->get().environmentBiomeIndex = net_tile.environmentBiomeIndex;
  tile->get().liquid = net_tile.liquid.liquid_level();
  tile->get().dungeonId = net_tile.dungeonId;
  if (collision_changed)
    WorldImpl::invalidate_collision_cache(*m_tileArray, RectI::with_size(m_geometry.xwrap(pos), {1, 1}));
  
  auto result298 = this->root().material_database();
  if (!result298)
    return std::unexpected(std::move(result298.error()));
  auto material_database = (std::move(*result298));
  auto result299 = material_database->background_light_transparent(tile->get().background);
  if (!result299)
    return std::unexpected(std::move(result299.error()));
  tile->get().backgroundLightTransparent = (std::move(*result299));
  auto result301 = material_database->foreground_light_transparent(tile->get().foreground);
  if (!result301)
    return std::unexpected(std::move(result301.error()));
  tile->get().foregroundLightTransparent =
      (std::move(*result301)) && tile->get().collision != CollisionKind::Dynamic;
  
  return true;
}


std::expected<float, StarException> WorldClient::lightLevel(Vec2F const& pos) const {
  if (!inWorld()) {
    return 0.0f;
  }
  return WorldImpl::light_level(this->root(), m_tileArray, m_entityMap, m_geometry, m_worldTemplate, m_sky, m_lightIntensityCalculator, pos);
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
        && liquidLevel(Vec2I::floor(pos)).liquid == empty_liquid_id) {
    auto result303 = this->root().assets();
    if (!result303)
      return std::unexpected(std::move(result303.error()));
    auto assets = (std::move(*result303));
    auto result304 = assets->json("/weather.config:weatherRayCheckDistance");
    if (!result304)
      return std::unexpected(std::move(result304.error()));
    auto result305 = (std::move(*result304)).to_float();
    if (!result305)
      return std::unexpected(std::move(result305.error()));
    float weatherRayCheckDistance = (std::move(*result305));
    auto result306 = assets->json("/weather.config:weatherRayCheckWindInfluence");
    if (!result306)
      return std::unexpected(std::move(result306.error()));
    auto result307 = (std::move(*result306)).to_float();
    if (!result307)
      return std::unexpected(std::move(result307.error()));
    float weatherRayCheckWindInfluence = (std::move(*result307));
    auto offset = Vec2F(-m_weather->wind() * weatherRayCheckWindInfluence, weatherRayCheckDistance).normalized() * weatherRayCheckDistance;
    auto result308 = line_collision({pos, pos + offset});
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

std::span<PhysicsForceRegion const> WorldClient::force_regions() const {
  return m_force_regions;
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
  if (entityId == m_mainPlayer->entityId() || m_mainPlayer->is_admin())
    return true;
  auto radius = m_mainPlayer->interact_radius();
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
  if (!inWorld() || !m_mainPlayer || !m_mainPlayer->getSecretPropertyView(secret_broadcast_public_key))
    return false;

  auto damageNotification = std::make_shared<DamageNotificationPacket>();
  auto& remDmg = damageNotification->remote_damage_notification;
  auto& dmg = remDmg.damage_notification;

  dmg.target_entity_id = dmg.source_entity_id = remDmg.source_entity_id = m_mainPlayer->entityId();
  dmg.damage_dealt = dmg.health_lost = 0.0f;
  dmg.hit_type = HitType::Hit;
  dmg.damage_source_kind = "nodamage";
  if (raw) {
    dmg.target_material_kind = String(broadcast);
  } else {
    auto message = broadcast.utf8();
    auto signature = this->root().identity().sign(std::span<char const>(message));
    std::string wire(secret_broadcast_prefix);
    wire.reserve(secret_broadcast_prefix.size() + signature.size() + message.size());
    wire.append(signature.begin(), signature.end());
    wire.append(message);
    dmg.target_material_kind = String(std::move(wire));
  }
  dmg.position = m_mainPlayer->position();

  if (!compress)
    damageNotification->set_compression_mode(PacketCompressionMode::Disabled);

  m_outgoingPackets.emplace_back(std::move(damageNotification));
  return true;
}

bool WorldClient::verify_broadcast_signature(std::shared_ptr<Player> const& player, bool legacy,
    std::span<char const> public_key, std::span<char const> signature, std::span<char const> message) {
  auto& state = m_broadcast_verifiers[player->entityId()];
  if (state.player.lock() != player) {
    state = {};
    state.player = player;
  }
  auto& verifier = legacy ? state.legacy : state.modern;
  auto& cached_key = legacy ? state.legacy_key : state.modern_key;
  if (public_key.size() != Curve25519::public_key_size) {
    verifier.reset();
    return false;
  }

  if (!verifier || !std::ranges::equal(public_key, cached_key, {},
          [](char byte) { return static_cast<std::uint8_t>(byte); })) {
    // Invalidate first, including when a replacement is malformed.
    verifier.reset();
    Curve25519::PublicKey key;
    std::ranges::copy(public_key, key.begin());
    auto replacement = Curve25519::Verifier::create(key);
    if (!replacement)
      return false;
    cached_key = key;
    verifier.emplace(std::move(*replacement));
  }
  return verifier->verify(signature, message);
}

std::expected<bool, StarException> WorldClient::handleSecretBroadcast(std::shared_ptr<Player> player, StringView broadcast) {
  if (m_broadcastCallback) {
    return m_broadcastCallback(std::move(player), broadcast);
  } else {
    return false;
  }
}


void WorldClient::ClientRenderCallback::clear() {
  light_sources.clear();
  particles.clear();
  audios.clear();
  preview_tiles.clear();
  overhead_bars.clear();
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::add_drawable(Drawable drawable, EntityRenderLayer render_layer) {
  if (!drawable_target)
    return {};
  if (position_offset != Vec2F())
    drawable.translate(position_offset);
  drawable_target->get().append(render_layer, std::move(drawable));
  return {};
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::add_light_source(LightSource light_source) {
  if (position_offset != Vec2F())
    light_source.translate(position_offset);
  light_sources.push_back(std::move(light_source));
  return {};
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::add_particle(Particle&& particle) {
  particles.push_back(std::move(particle));
  return {};
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::add_audio(std::shared_ptr<AudioInstance> audio) {
  audios.push_back(std::move(audio));
  return {};
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::add_tile_preview(PreviewTile preview) {
  preview_tiles.push_back(std::move(preview));
  return {};
}

std::expected<void, StarException> WorldClient::ClientRenderCallback::add_overhead_bar(OverheadBar bar) {
  if (position_offset != Vec2F())
    bar.entity_position += position_offset;
  overhead_bars.push_back(std::move(bar));
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
  m_expiry_timer.reset();
  m_requestedDestroy = true;
}

std::expected<RpcPromise<Vec2F>, StarException> WorldClient::findUniqueEntity(String const& uniqueId) {
  if (!inWorld()) {
    return RpcPromise<Vec2F>::make_failed("Not currently in a world");
  }
  
  if (auto entity = m_entityMap->unique_entity(uniqueId)) {
    return RpcPromise<Vec2F>::make_fulfilled(entity->position());
  }
  
  auto pair = RpcPromise<Vec2F>::make_pair();
  auto& rpcPromises = m_findUniqueEntityResponses[uniqueId];
  if (rpcPromises.empty()) {
    m_outgoingPackets.push_back(std::make_shared<FindUniqueEntityPacket>(uniqueId));
  }
  rpcPromises.push_back(pair.second);

  return pair.first;
}

std::expected<RpcPromise<Json>, StarException> WorldClient::send_entity_message(std::variant<EntityId, String> const& target_id, String const& message, JsonArray const& args) {
  if (!inWorld()) {
    return RpcPromise<Json>::make_failed("Not currently in a world");
  }
  
  std::shared_ptr<Entity> entity;
  if (std::holds_alternative<EntityId>(target_id)) {
    auto id = std::get<EntityId>(target_id);
    auto owner = connection_for_entity(id);
    if (!owner)
      return RpcPromise<Json>::make_failed("Entity has no owning connection");
    entity = m_entityMap->entity(id);
    if (!entity && *owner == m_clientId)
      return RpcPromise<Json>::make_failed("Unknown entity");
  } else {
    entity = m_entityMap->unique_entity(std::get<String>(target_id));
  }

  if (entity && entity->isMaster()) {
    return to_message_promise(entity->receive_message(*m_clientId, message, args), "Message not handled by entity");
  } else {
    auto pair = RpcPromise<Json>::make_pair();
    Uuid uuid(this->root().random());
    m_entityMessageResponses[uuid] = pair.second;
    m_outgoingPackets.push_back(std::make_shared<EntityMessagePacket>(target_id, message, args, uuid));
    return pair.first;
  }
}

std::expected<List<ChatAction>, StarException> WorldClient::pull_pending_chat_actions() {
  List<ChatAction> result;
  if (m_entityMap) {
    {
      auto result310 = m_entityMap->all<ChattyEntity>();
      if (!result310)
        return std::unexpected(std::move(result310.error()));
      for (auto const& entity : (std::move(*result310))) {
        result.append_range(entity->pull_pending_chat_actions());
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
  if (clientWindow.is_empty()) {
    return {};
  }

  auto log_poly = [&self = *this](PolyF poly, Vec2F position, float r, float g, float b) {
    poly.translate(position);
    self.root().spatialLogger().log_poly("world", poly,
                                         {float_to_byte(r, true), float_to_byte(g, true), float_to_byte(b, true), 255});
  };

  auto result311 = for_each_collision_block(clientWindow, [&](auto const& block) -> std::expected<void, StarException> {
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
      auto forces = physics->force_regions();
      if (!forces)
        return std::unexpected(std::move(forces.error()));
      auto& logger = root().spatialLogger();
      for (auto const& force : *forces) {
        std::visit([&]<physics_force_region Region>(Region const& region) {
          if constexpr (std::same_as<Region, RadialForceRegion>) {
            logger.log_poly("world", PolyF(region.bounding_box()), {0, 255, 255, 255});
          } else {
            logger.log_poly("world", region.region, {255, 255, 0, 255});
            if constexpr (std::same_as<Region, GradientForceRegion>)
              logger.log_line("world", region.gradient, {255, 255, 0, 255});
          }
        }, force);
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

std::expected<void, StarException> WorldClient::inform_tile_prediction(Vec2I const& pos, TileModification const& modification) {
  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  if (std::holds_alternative<PlaceMaterial>(modification)
      && std::get<PlaceMaterial>(modification).layer == TileLayer::Foreground) {
    auto const& placement = std::get<PlaceMaterial>(modification);
    auto materials = root().material_database();
    if (!materials)
      return std::unexpected(std::move(materials.error()));
    auto behavior = (*materials)->falling_behavior(placement.material);
    if (!behavior)
      return std::unexpected(std::move(behavior.error()));
    CollisionKind collision;
    if (placement.collisionOverride == TileCollisionOverride::None) {
      auto kind = (*materials)->material_collision_kind(placement.material);
      if (!kind)
        return std::unexpected(std::move(kind.error()));
      collision = *kind;
    } else {
      collision = collisionKindFromOverride(placement.collisionOverride);
    }

    auto& prediction = m_predicted_tiles[pos];
    auto previous = prediction.collision;
    bool stationary = *behavior == FallingBehavior::Stationary;
    prediction.foreground = stationary ? placement.material : structure_material_id;
    prediction.foregroundHueShift = stationary ? placement.materialHueShift : std::nullopt;
    prediction.collision = collision;
    prediction.time = now;
    invalidate_prediction_change(pos, previous, collision);
    return {};
  }

  auto& p = m_predicted_tiles[pos];
  p.time = now;
  if (std::holds_alternative<PlaceMaterial>(modification)) {
    auto const& placement = std::get<PlaceMaterial>(modification);
    p.background = placement.material;
    p.backgroundHueShift = placement.materialHueShift;
  } else if (std::holds_alternative<PlaceMod>(modification)) {
    auto const& place_mod = std::get<PlaceMod>(modification);
    if (place_mod.layer == TileLayer::Foreground) {
      p.foregroundMod = place_mod.mod;
    } else {
      p.backgroundMod = place_mod.mod;
    }
  } else if (std::holds_alternative<PlaceMaterialColor>(modification)) {
    auto const& place_color = std::get<PlaceMaterialColor>(modification);
    if (place_color.layer == TileLayer::Foreground) {
      p.foregroundColorVariant = place_color.color;
    } else {
      p.backgroundColorVariant = place_color.color;
    }
  } else if (std::holds_alternative<PlaceLiquid>(modification)) {
    auto const& place_liquid = std::get<PlaceLiquid>(modification);
    if (!p.liquid_delta || p.liquid_delta->liquid != place_liquid.liquid) {
      p.liquid_delta = LiquidLevel(place_liquid.liquid, place_liquid.liquidLevel);
    } else {
      p.liquid_delta->level += place_liquid.liquidLevel;
    }
  }
  return {};
}

std::expected<void, StarException> WorldClient::setup_force_regions() {
  m_force_regions.clear();
  if (!m_worldTemplate) return {};
  auto parameters = m_worldTemplate->worldParameters();
  if (!parameters || parameters->world_edge_force_regions == WorldEdgeForceRegionType::None) return {};
  auto assets = root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/worldserver.config");
  if (!config) return std::unexpected(std::move(config.error()));
  return parameters->append_edge_force_regions(m_force_regions, *config, Vec2F(m_worldTemplate->size()),
      PhysicsCategoryFilter::whitelist({"player", "monster", "npc", "vehicle"}));
}

bool WorldClient::shouldExpire() {
  if (inWorld()) {
    return false;
  }

  return m_expiry_timer.ready();
}

}
