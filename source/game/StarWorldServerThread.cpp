#include <stdio.h>

import star.json;
import star.xx_hash;
import star.id_map;
import star.list;
import star.bi_map;
import star.casting;
import star.either;
import star.image;
import star.color;
import star.vector;
import star.game_types;
import star.math_common;
import star.time;
import star.format;
import star.version;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.random;
import star.weighted_pool;
import star.strong_typedef;
import star.set;
import star.worker_pool;

import std;
import star.sector_array_2d;
import star.ordered_map;
import star.net_compatibility;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.map;
import star.rpc_promise;
import star.a_star;
import star.lua;
import star.periodic_function;
import star.audio;
import star.io_device;
import star.ref_ptr;
import star.listener;
import star.logging;
import star.output_proxy;

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
import star.force_regions;
import star.world;
import star.liquid_types;
import star.tile_damage;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
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
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.scripted_entity;
import star.chat_action;
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
import star.entity_rendering;
import star.radio_message_database;
import star.player;

import star.tick_rate_monitor;

import star.algorithm;
namespace Star {

std::expected<std::shared_ptr<WorldServerThread>, StarException> WorldServerThread::create(
    std::shared_ptr<WorldServer> server, WorldId worldId) {
  if (!server)
    return std::unexpected(StarException("Cannot create a world thread without a server"));
  auto worldName = Star::print_world_id(worldId);
  if (!worldName)
    return std::unexpected(std::move(worldName.error()));
  return std::shared_ptr<WorldServerThread>(new WorldServerThread(std::move(server), std::move(worldId), std::move(*worldName)));
}

WorldServerThread::WorldServerThread(std::shared_ptr<WorldServer> server, WorldId worldId, String worldName)
  : Thread("WorldServerThread: " + worldName),
    m_worldServer(std::move(server)),
    m_worldId(std::move(worldId)),
    m_worldName(std::move(worldName)),
    m_stopSource(std::nostopstate),
    m_errorOccurred(false),
    m_shouldExpire(true) {
  m_worldServer->setWorldId(m_worldName);
}

WorldServerThread::~WorldServerThread() {
  if (auto closed = close(); !closed)
    EmergencyLog::error("Error stopping world thread during destruction: {}", closed.error().what());
  if (!isJoined())
    Star::fatal_error("Cannot destroy WorldServerThread while its native thread still borrows world state", false);
}

std::expected<void, StarException> WorldServerThread::recordError(StarException const& error) const {
  m_errorOccurred = true;
  auto logged = m_worldServer->root().logger().errorException("WorldServerThread error", ExceptionDiagnostic(error));
  if (!logged)
    return std::unexpected(error.with_related(logged.error(), "World error logging also failed"));
  return {};
}

WorldId WorldServerThread::worldId() const {
  return m_worldId;
}

std::expected<void, StarException> WorldServerThread::start() {
  if (!isJoined())
    return {};
  m_stopSource = std::stop_source();
  m_errorOccurred = false;
  auto started = Thread::start();
  if (!started)
    return std::unexpected(std::move(started.error()));
  return {};
}

std::expected<void, StarException> WorldServerThread::stop() {
  m_stopSource.request_stop();
  auto joined = Thread::join();
  if (!joined)
    return std::unexpected(std::move(joined.error()));
  return {};
}

std::expected<void, StarException> WorldServerThread::close() {
  auto stopped = stop();
  if (!isJoined()) {
    auto error = stopped
        ? StarException("WorldServerThread native thread remains unjoined after stop")
        : std::move(stopped.error());
    auto drained = drainMessages();
    if (!drained)
      return std::unexpected(error.with_related(drained.error(), "World message drain also failed"));
    return std::unexpected(std::move(error));
  }
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  std::optional<StarException> error;
  if (!stopped)
    error = std::move(stopped.error());
  for (auto clientId : m_worldServer->clientIds()) {
    auto removed = removeClient(clientId);
    if (!removed && !error)
      error = std::move(removed.error());
  }
  auto drained = drainMessages();
  if (!drained && !error)
    error = std::move(drained.error());
  auto closed = m_worldServer->close();
  if (!closed) {
    auto logged = recordError(closed.error());
    if (!error)
      error = logged ? std::move(closed.error()) : std::move(logged.error());
    else if (!logged)
      *error = error->with_related(logged.error(), "World close logging also failed");
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

void WorldServerThread::setPause(std::shared_ptr<const std::atomic<bool>> pause) { m_pause = std::move(pause); }

bool WorldServerThread::serverErrorOccurred() {
  return m_errorOccurred;
}

bool WorldServerThread::shouldExpire() {
  return m_shouldExpire;
}

std::expected<bool, StarException> WorldServerThread::spawnTargetValid(SpawnTarget const& spawnTarget) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto valid = m_worldServer->spawnTargetValid(spawnTarget);
  if (!valid) {
    auto logged = recordError(valid.error());
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return valid;
}

std::expected<bool, StarException> WorldServerThread::addClient(ConnectionId clientId, SpawnTarget const& spawnTarget,
    bool isLocal, bool isAdmin, NetCompatibilityRules netRules, ClientSubWorldId subWorldId) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto added = m_worldServer->addClient(clientId, spawnTarget, isLocal, isAdmin, netRules, subWorldId);
  if (!added) {
    auto logged = recordError(added.error());
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    return std::unexpected(std::move(added.error()));
  }
  if (*added)
    m_clients.add(clientId);
  return *added;
}

std::expected<List<std::shared_ptr<Packet>>, StarException> WorldServerThread::removeClient(ConnectionId clientId) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  std::unique_lock queueLocker(m_queueMutex, std::defer_lock);
  lockWithDiagnostics(queueLocker);
  List<std::shared_ptr<Packet>> incomingPackets;
  if (auto queued = m_incomingPacketQueue.find_ref(clientId))
    incomingPackets = std::exchange(queued->get(), {});
  std::optional<StarException> error;
  if (m_worldServer->hasClient(clientId)) {
    m_clients.add(clientId);
    if (m_removingClients.add(clientId)) {
      auto handled = m_worldServer->handleIncomingPackets(clientId, std::move(incomingPackets));
      if (!handled) {
        auto logged = recordError(handled.error());
        error = logged ? std::move(handled.error()) : std::move(logged.error());
      }
    }
  }
  List<std::shared_ptr<Packet>> outgoingPackets;
  if (auto queued = m_outgoingPacketQueue.find_ref(clientId))
    outgoingPackets = std::exchange(queued->get(), {});
  if (m_worldServer->hasClient(clientId)) {
    auto removed = m_worldServer->removeClient(clientId);
    if (!removed) {
      auto logged = recordError(removed.error());
      if (!error)
        error = logged ? std::move(removed.error()) : std::move(logged.error());
      else if (!logged)
        *error = error->with_related(logged.error(), "World client removal logging also failed");
    } else {
      outgoingPackets.append_range(std::move(*removed));
    }
  }
  if (!m_worldServer->hasClient(clientId)) {
    m_clients.remove(clientId);
    m_removingClients.remove(clientId);
  }
  m_incomingPacketQueue.remove(clientId);
  if (error) {
    m_outgoingPacketQueue[clientId] = std::move(outgoingPackets);
    return std::unexpected(std::move(*error));
  }
  m_outgoingPacketQueue.remove(clientId);
  return outgoingPackets;
}

List<ConnectionId> WorldServerThread::clients() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_clients.values();
}

bool WorldServerThread::hasClient(ConnectionId clientId) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_clients.contains(clientId);
}

bool WorldServerThread::noClients() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_clients.empty();
}

ClientSubWorldId WorldServerThread::clientSubWorld(ConnectionId clientId) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_worldServer->clientSubWorld(clientId);
}

List<ConnectionId> WorldServerThread::erroredClients() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto unerroredClients = HashSet<ConnectionId>::from(m_worldServer->clientIds());
  return m_clients.difference(unerroredClients).values();
}

void WorldServerThread::pushIncomingPackets(ConnectionId clientId, List<std::shared_ptr<Packet>> packets) {
  std::unique_lock queueLocker(m_queueMutex, std::defer_lock);
  lockWithDiagnostics(queueLocker);
  m_incomingPacketQueue[clientId].append_range(std::move(packets));
}

List<std::shared_ptr<Packet>> WorldServerThread::pullOutgoingPackets(ConnectionId clientId) {
  std::unique_lock queueLocker(m_queueMutex, std::defer_lock);
  lockWithDiagnostics(queueLocker);
  return std::exchange(m_outgoingPacketQueue[clientId], {});
}

std::expected<std::optional<Vec2F>, StarException> WorldServerThread::playerRevivePosition(ConnectionId clientId) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (auto player = m_worldServer->clientPlayer(clientId)) {
    auto feetOffset = player->feetOffset();
    if (!feetOffset) {
      auto logged = recordError(feetOffset.error());
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      return std::unexpected(std::move(feetOffset.error()));
    }
    return std::optional<Vec2F>(player->position() + *feetOffset);
  }
  return std::optional<Vec2F>{};
}

std::expected<std::optional<std::pair<String, String>>, StarException> WorldServerThread::pullNewPlanetType() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_worldServer->pullNewPlanetType();
}

std::expected<void, StarException> WorldServerThread::executeAction(WorldServerAction action) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (!action)
    return std::unexpected(StarException("Cannot execute an empty server world action"));
  return action(*this, *m_worldServer);
}

void WorldServerThread::setUpdateAction(WorldServerAction updateAction) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_updateAction = std::move(updateAction);
}

void WorldServerThread::passMessages(List<Message>&& messages) {
  std::unique_lock locker(m_messageMutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_messages.append_range(std::move(messages));
}

std::expected<void, StarException> WorldServerThread::unloadAll(bool force) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto unloaded = m_worldServer->unloadAll(force);
  if (!unloaded) {
    auto logged = recordError(unloaded.error());
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return unloaded;
}

std::expected<WorldChunks, StarException> WorldServerThread::readChunks() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto chunks = m_worldServer->readChunks();
  if (!chunks) {
    auto logged = recordError(chunks.error());
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return chunks;
}

std::expected<void, StarException> WorldServerThread::drainMessages() {
  List<Message> messages;
  {
    std::unique_lock locker(m_messageMutex, std::defer_lock);
    lockWithDiagnostics(locker);
    messages = std::move(m_messages);
  }
  std::optional<StarException> error;
  for (auto& message : messages) {
    auto failed = message.promise.fail("World server stopped");
    if (!failed && !error)
      error = std::move(failed.error());
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> WorldServerThread::run() {
  auto result = runLoop(m_stopSource.get_token());
  if (!result) {
    auto logged = recordError(result.error());
    if (!logged)
      result = std::unexpected(std::move(logged.error()));
  }
  auto drained = drainMessages();
  if (!drained) {
    auto logged = recordError(drained.error());
    if (result) {
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      return std::unexpected(std::move(drained.error()));
    }
    if (!logged)
      result = std::unexpected(result.error().with_related(logged.error(), "World error logging also failed"));
  }
  return result;
}

std::expected<void, StarException> WorldServerThread::runLoop(std::stop_token stopToken) {
  auto& root = m_worldServer->root();
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/universe_server.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto updateMeasureWindow = config->getDouble("updateMeasureWindow");
  if (!updateMeasureWindow)
    return std::unexpected(std::move(updateMeasureWindow.error()));
  auto fidelityDecrementScore = config->getDouble("fidelityDecrementScore");
  if (!fidelityDecrementScore)
    return std::unexpected(std::move(fidelityDecrementScore.error()));
  auto fidelityIncrementScore = config->getDouble("fidelityIncrementScore");
  if (!fidelityIncrementScore)
    return std::unexpected(std::move(fidelityIncrementScore.error()));
  auto configuration = root.configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto fidelityConfig = (*configuration)->get("serverFidelity");
  if (!fidelityConfig)
    return std::unexpected(std::move(fidelityConfig.error()));
  auto serverFidelityMode = fidelityConfig->toString();
  if (!serverFidelityMode)
    return std::unexpected(std::move(serverFidelityMode.error()));
  auto automatic = serverFidelityMode->equals_ignore_case("automatic");
  if (!automatic)
    return std::unexpected(std::move(automatic.error()));
  if (!WorldServerFidelityNames)
    return std::unexpected(WorldServerFidelityNames.error());
  std::optional<WorldServerFidelity> lockedFidelity;
  if (!*automatic) {
    auto fidelity = WorldServerFidelityNames->get_left(*serverFidelityMode);
    if (!fidelity)
      return std::unexpected(std::move(fidelity.error()));
    lockedFidelity = fidelity->get();
  }
  auto storageIntervalConfig = config->getDouble("worldStorageInterval");
  if (!storageIntervalConfig)
    return std::unexpected(std::move(storageIntervalConfig.error()));
  double storageInterval = *storageIntervalConfig / 1000.0;
  Timer storageTimer = Timer::withTime(storageInterval);
  TickRateApproacher tickApproacher(1.0f / root.timing().serverTimestep, *updateMeasureWindow);
  double fidelityScore = 0.0;
  WorldServerFidelity automaticFidelity = WorldServerFidelity::Medium;
  while (!stopToken.stop_requested() && !m_errorOccurred) {
    auto fidelity = lockedFidelity.value_or(automaticFidelity);
    auto fidelityName = WorldServerFidelityNames->get_right(fidelity);
    if (!fidelityName)
      return std::unexpected(std::move(fidelityName.error()));
    root.logMap().set(Star::format("server_{}_fidelity", m_worldName), fidelityName->get());
    root.logMap().set(Star::format("server_{}_update", m_worldName), Star::format("{:4.2f}Hz", tickApproacher.rate()));
    auto updated = update(fidelity);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    tickApproacher.setTargetTickRate(1.0f / root.timing().serverTimestep);
    tickApproacher.tick();
    if (storageTimer.timeUp()) {
      auto synced = sync();
      if (!synced)
        return std::unexpected(std::move(synced.error()));
      storageTimer.restart(storageInterval);
    }
    double spareTime = tickApproacher.spareTime();
    fidelityScore += spareTime;
    if (fidelityScore <= *fidelityDecrementScore) {
      if (automaticFidelity > WorldServerFidelity::Minimum)
        automaticFidelity = static_cast<WorldServerFidelity>(static_cast<int>(automaticFidelity) - 1);
      fidelityScore = 0.0;
    }
    if (fidelityScore >= *fidelityIncrementScore) {
      if (automaticFidelity < WorldServerFidelity::High)
        automaticFidelity = static_cast<WorldServerFidelity>(static_cast<int>(automaticFidelity) + 1);
      fidelityScore = 0.0;
    }
    std::int64_t spareMilliseconds = std::floor(spareTime * 1000);
    if (spareMilliseconds > 0)
      Thread::sleepPrecise(spareMilliseconds);
  }
  return {};
}

std::expected<void, StarException> WorldServerThread::update(WorldServerFidelity fidelity) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto unerroredClientIds = m_worldServer->clientIds();
  for (auto clientId : m_worldServer->clientIds()) {
    std::unique_lock queueLocker(m_queueMutex, std::defer_lock);
    lockWithDiagnostics(queueLocker);
    auto incomingPackets = std::exchange(m_incomingPacketQueue[clientId], {});
    queueLocker.unlock();
    auto handled = m_worldServer->handleIncomingPackets(clientId, std::move(incomingPackets));
    if (!handled) {
      auto logged = m_worldServer->root().logger().errorException(
          Star::format("WorldServerThread error handling incoming packets for client {}", clientId), ExceptionDiagnostic(handled.error()));
      auto removed = m_worldServer->removeClient(clientId);
      if (!removed)
        return std::unexpected(std::move(removed.error()));
      std::unique_lock outgoingLocker(m_queueMutex, std::defer_lock);
      lockWithDiagnostics(outgoingLocker);
      m_outgoingPacketQueue[clientId].append_range(std::move(*removed));
      unerroredClientIds.remove(clientId);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
  }

  float dt = m_worldServer->root().timing().serverTimestep * m_worldServer->root().timing().timescale;
  if (auto set = m_worldServer->setFidelity(fidelity); !set)
    return std::unexpected(std::move(set.error()));
  if (dt > 0.0f && (!m_pause || *m_pause == false)) {
    auto updated = m_worldServer->update(dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }

  List<Message> messages;
  {
    std::unique_lock messageLocker(m_messageMutex, std::defer_lock);
    lockWithDiagnostics(messageLocker);
    messages = std::move(m_messages);
  }
  auto settled = settleWorldMessageBatch(messages, [&](String const& message, JsonArray const& args) {
    return m_worldServer->receiveMessage(ServerConnectionId, message, args);
  });
  if (!settled)
    return std::unexpected(std::move(settled.error()));
  for (auto clientId : unerroredClientIds) {
    auto outgoingPackets = m_worldServer->getOutgoingPackets(clientId);
    if (!outgoingPackets)
      return std::unexpected(std::move(outgoingPackets.error()));
    std::unique_lock queueLocker(m_queueMutex, std::defer_lock);
    lockWithDiagnostics(queueLocker);
    m_outgoingPacketQueue[clientId].append_range(std::move(*outgoingPackets));
  }
  m_shouldExpire = m_worldServer->shouldExpire();
  if (m_updateAction)
    return m_updateAction(*this, *m_worldServer);
  return {};
}

std::expected<void, StarException> WorldServerThread::sync() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto logged = m_worldServer->root().logger().debug("WorldServer: periodic sync to disk of world {}", m_worldName);
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  return m_worldServer->sync();
}

}
