import star.xx_hash;
import star.json;
import star.id_map;
import star.game_types;
import star.image;
import star.list;
import star.bi_map;
import star.math_common;
import star.format;
import star.version;
import star.color;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.either;
import star.output_proxy;
import star.random;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
import star.audio;
import star.map;
import star.set;
import star.rpc_promise;
import star.ordered_map;
import star.net_compatibility;
import std;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.a_star;
import star.lua;
import star.periodic_function;
import star.io_device;
import star.ref_ptr;
import star.listener;
import star.logging;

import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.item_descriptor;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.tile_modification;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.warping;
import star.world_geometry;
import star.wiring;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.drawable;
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
import star.world;
import star.lua_components;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.game_timers;
import star.world_client;
import star.world_client_thread;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.actor_movement_controller;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.scripted_entity;
import star.chatty_entity;
import star.emote_entity;
import star.lounging_entities;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.npc_database;
import star.effect_emitter;
import star.npc;
import star.asset_source;
import star.assets;

import star.configuration;
import star.root;
import star.inventory_types;
import star.ai_types;
import star.radio_message_database;
import star.player;

import star.tick_rate_monitor;

import star.algorithm;
namespace Star {

std::expected<std::shared_ptr<WorldClientThread>, StarException> WorldClientThread::create(
    Root& root, ClientSubWorldId subWorldId, UniverseClient& universe) {
  auto world = WorldClient::create(root, subWorldId, universe);
  if (!world)
    return std::unexpected(std::move(world.error()));
  return std::shared_ptr<WorldClientThread>(new WorldClientThread(subWorldId, std::move(*world)));
}

WorldClientThread::WorldClientThread(ClientSubWorldId subWorldId, std::shared_ptr<WorldClient> worldClient)
  : Thread(Star::format("WorldClientThread: {}", subWorldId)),
    m_worldClient(std::move(worldClient)),
    m_subWorldId(subWorldId),
    m_stopSource(std::nostopstate),
    m_errorOccurred(false),
    m_shouldExpire(false) {}

WorldClientThread::~WorldClientThread() {
  if (auto stopped = stop(); !stopped)
    EmergencyLog::error("Error stopping client world thread during destruction: {}", stopped.error().what());
  if (!isJoined())
    Star::fatal_error("Cannot destroy WorldClientThread while its native thread still borrows world state", false);
  if (auto closed = m_worldClient->close(); !closed)
    EmergencyLog::error("Error closing client world during destruction: {}", closed.error().what());
}

ClientSubWorldId WorldClientThread::subWorldId() const {
  return m_subWorldId;
}

std::expected<void, StarException> WorldClientThread::start() {
  if (!isJoined())
    return {};
  m_stopSource = std::stop_source();
  m_errorOccurred = false;
  auto started = Thread::start();
  if (!started)
    return std::unexpected(std::move(started.error()));
  return {};
}

std::expected<void, StarException> WorldClientThread::stop() {
  m_stopSource.request_stop();
  auto joined = Thread::join();
  auto cleared = clearMessages();
  if (!joined) {
    if (!cleared)
      return std::unexpected(joined.error().withRelated(cleared.error(), "Client world message drain also failed"));
    return std::unexpected(std::move(joined.error()));
  }
  if (!cleared)
    return std::unexpected(std::move(cleared.error()));
  return {};
}

void WorldClientThread::setPause(std::shared_ptr<const std::atomic<bool>> pause) { m_pause = std::move(pause); }

bool WorldClientThread::errorOccurred() {
  return m_errorOccurred;
}

bool WorldClientThread::shouldExpire() {
  return m_shouldExpire;
}

void WorldClientThread::pushIncomingPackets(List<std::shared_ptr<Packet>> packets) {
  std::unique_lock queueLocker(m_queueMutex, std::defer_lock);
  lockWithDiagnostics(queueLocker);
  m_incomingPacketQueue.append_range(std::move(packets));
}

List<std::shared_ptr<Packet>> WorldClientThread::pullOutgoingPackets() {
  std::unique_lock queueLocker(m_queueMutex, std::defer_lock);
  lockWithDiagnostics(queueLocker);
  return std::exchange(m_outgoingPacketQueue, {});
}

std::expected<void, StarException> WorldClientThread::executeAction(WorldClientAction action) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (!action)
    return std::unexpected(StarException("Cannot execute an empty client world action"));
  return action(*this, *m_worldClient);
}

void WorldClientThread::setUpdateAction(WorldClientAction updateAction) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_updateAction = std::move(updateAction);
}

void WorldClientThread::passMessage(Message&& message) {
  std::unique_lock locker(m_messageMutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_messages.push_back(std::move(message));
}

std::expected<void, StarException> WorldClientThread::clearMessages() {
  List<Message> messages;
  {
    std::unique_lock locker(m_messageMutex, std::defer_lock);
    lockWithDiagnostics(locker);
    messages = std::move(m_messages);
  }
  std::optional<StarException> error;
  for (auto& message : messages) {
    auto failed = message.promise.fail("Messages discarded");
    if (!failed && !error)
      error = std::move(failed.error());
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> WorldClientThread::run() {
  auto result = runLoop(m_stopSource.get_token());
  if (!result) {
    auto logged = m_worldClient->root().logger().errorException("WorldClientThread error", ExceptionDiagnostic(result.error()));
    if (!logged)
      result = std::unexpected(result.error().withRelated(logged.error(), "World error logging also failed"));
    m_errorOccurred = true;
  }
  auto cleared = clearMessages();
  if (!cleared) {
    auto logged = m_worldClient->root().logger().errorException("WorldClientThread message drain failed", ExceptionDiagnostic(cleared.error()));
    if (!logged)
      cleared = std::unexpected(cleared.error().withRelated(logged.error(), "World message drain logging also failed"));
    m_errorOccurred = true;
    if (result)
      return std::unexpected(std::move(cleared.error()));
  }
  return result;
}

std::expected<void, StarException> WorldClientThread::runLoop(std::stop_token stopToken) {
  auto& root = m_worldClient->root();
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto measureConfig = (*assets)->json("/client.config:subWorldUpdateMeasureWindow");
  if (!measureConfig)
    return std::unexpected(std::move(measureConfig.error()));
  auto updateMeasureWindow = measureConfig->toDouble();
  if (!updateMeasureWindow)
    return std::unexpected(std::move(updateMeasureWindow.error()));

  TickRateApproacher tickApproacher(1.0f / root.timing().timestep, *updateMeasureWindow);
  while (!stopToken.stop_requested() && !m_errorOccurred) {
    root.logMap().set(Star::format("client_{}_update", m_subWorldId), Star::format("{:4.2f}Hz", tickApproacher.rate()));
    auto updated = update();
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    tickApproacher.setTargetTickRate(1.0f / root.timing().timestep);
    tickApproacher.tick();
    std::int64_t spareMilliseconds = std::floor(tickApproacher.spareTime() * 1000);
    if (spareMilliseconds > 0)
      Thread::sleepPrecise(spareMilliseconds);
  }
  return {};
}

std::expected<void, StarException> WorldClientThread::update() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  std::unique_lock queueLocker(m_queueMutex, std::defer_lock);
  lockWithDiagnostics(queueLocker);
  auto incomingPackets = std::exchange(m_incomingPacketQueue, {});
  queueLocker.unlock();
  auto incoming = m_worldClient->handleIncomingPackets(std::move(incomingPackets));
  if (!incoming) {
    auto logged = m_worldClient->root().logger().errorException("WorldClientThread error handling incoming packets", ExceptionDiagnostic(incoming.error()));
    lockWithDiagnostics(queueLocker);
    m_outgoingPacketQueue.push_back(std::make_shared<ClientSubWorldRequest>(m_subWorldId, WorldId()));
    queueLocker.unlock();
    m_errorOccurred = true;
    if (!logged)
      return std::unexpected(incoming.error().withRelated(logged.error(), "World packet error logging also failed"));
  }

  float dt = m_worldClient->root().timing().timestep * m_worldClient->root().timing().timescale;
  if (dt > 0.0f && (!m_pause || *m_pause == false)) {
    auto updated = m_worldClient->update(dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }

  if (m_worldClient->inWorld()) {
    List<Message> messages;
    {
      std::unique_lock messageLocker(m_messageMutex, std::defer_lock);
      lockWithDiagnostics(messageLocker);
      messages = std::move(m_messages);
    }
    auto settled = settleWorldMessageBatch(messages, [&](String const& message, JsonArray const& args) {
      return m_worldClient->receiveMessage(ServerConnectionId, message, args);
    });
    if (!settled)
      return std::unexpected(std::move(settled.error()));
  }

  auto outgoingPackets = m_worldClient->getOutgoingPackets();
  auto shouldDestroy = m_worldClient->pullRequestedDestroy();
  lockWithDiagnostics(queueLocker);
  m_outgoingPacketQueue.push_back(std::make_shared<ClientSubWorldPackets>(m_subWorldId, std::move(outgoingPackets)));
  if (shouldDestroy) {
    auto logged = m_worldClient->root().logger().info("WorldClientThread requesting destroy");
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    m_outgoingPacketQueue.push_back(std::make_shared<ClientSubWorldRequest>(m_subWorldId, WorldId()));
  }
  queueLocker.unlock();

  m_shouldExpire = m_worldClient->shouldExpire();
  if (m_updateAction)
    return m_updateAction(*this, *m_worldClient);
  return {};
}

}
