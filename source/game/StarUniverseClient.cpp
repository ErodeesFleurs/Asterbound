import star.json;
import star.optional;
import star.xx_hash;
import star.json_extra;
import star.game_types;
import star.vector;
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
import star.strong_typedef;
import star.ordered_set;
import star.byte_array;
import star.data_stream_devices;

import std;
typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.file;
import star.format;
import star.output_proxy;
import star.id_map;
import star.math_common;
import star.ordered_map;
import star.casting;
import star.set;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.btree_database;
import star.rpc_promise;
import star.image;
import star.audio;
import star.map;
import star.lexical_cast;
import star.logging;
import star.version;
import star.ref_ptr;
import star.listener;
import star.a_star;
import star.periodic_function;
import star.time;
import star.io_device;
import star.string;
import star.encode;
import star.lua;

import star.universe_client;
import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.chat_types;
import star.uuid;
import star.warping;
import star.item_descriptor;
import star.quest_descriptor;
import star.ai_types;
import star.p2p_networking_service;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.tile_modification;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.world_geometry;
import star.wiring;
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
import star.net_packet_socket;
import star.universe_connection;
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
import star.lua_converters;
import star.lua_game_converters;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.game_timers;
import star.world_client;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.asset_source;
import star.assets;
import star.root_base;
import star.root;
import star.configuration;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.physics_entity;
import star.anchorable_entity;
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
import star.movement_controller;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.player_log;
import star.host_address;
import star.socket;
import star.tcp;
import star.system_world_client;
import star.player_universe_map;
import star.world_template;
import star.scripted_entity;
import star.status_effect_entity;
import star.effect_emitter;
import star.projectile;

import star.projectile_database;
import star.sha256;
import star.universe_client_lua_bindings;
import star.celestial_database;

import star.player_storage;
import star.client_context;
import star.statistics;
import star.team_client;

import star.sky;

import star.net_element;

import star.algorithm;
namespace Star {

UniverseClient::UniverseClient(Root& root) : m_root(root), m_storageTriggerDeadline(0) {}

std::expected<std::shared_ptr<UniverseClient>, StarException> UniverseClient::create(Root& root, std::shared_ptr<PlayerStorage> playerStorage, std::shared_ptr<Statistics> statistics, String const& customWorldStorageDir) {
  auto client = std::shared_ptr<UniverseClient>(new UniverseClient(root));
  client->m_playerStorage = std::move(playerStorage);
  client->m_statistics = std::move(statistics);
  client->m_customWorldStorageDirectory = customWorldStorageDir;
  client->m_pause = std::make_shared<std::atomic<bool>>(false);
  auto luaRootResult = LuaRoot::create(root);
  if (!luaRootResult)
    return std::unexpected(std::move(luaRootResult.error()));
  auto luaRoot = std::move(*luaRootResult);
  client->m_luaRoot = std::move(luaRoot);
  auto threadsResult = IdMap<ClientSubWorldId, std::shared_ptr<WorldClientThread>>::create(MinClientSubWorldId, MaxClientSubWorldId);
  if (!threadsResult)
    return std::unexpected(std::move(threadsResult.error()));
  auto threads = std::move(*threadsResult);
  client->m_subWorldThreads = std::move(threads);
  auto reset = client->reset();
  if (!reset)
    return std::unexpected(std::move(reset.error()));
  return client;
}

UniverseClient::~UniverseClient() {
  if (auto disconnected = disconnect(); !disconnected)
    EmergencyLog::error("Error disconnecting universe client during destruction: {}", disconnected.error().what());
  if (m_luaRoot) {
    if (auto stopped = m_luaRoot->shutdown(); !stopped)
      EmergencyLog::error("Error shutting down universe Lua during destruction: {}", stopped.error().what());
  }
}

Root& UniverseClient::root() const {
  return m_root;
}

std::expected<void, StarException> UniverseClient::setMainPlayer(std::shared_ptr<Player> player) {
  if (isConnected())
    return std::unexpected(StarException("Cannot call UniverseClient::setMainPlayer while connected"));
  if (m_mainPlayer) {
    auto saved = m_playerStorage->savePlayer(m_mainPlayer);
    if (!saved)
      return std::unexpected(std::move(saved.error()));
    m_mainPlayer->setClientContext({});
    m_mainPlayer->setStatistics({});
    m_mainPlayer->setUniverseClient(std::nullopt);
  }
  m_mainPlayer = std::move(player);
  if (m_mainPlayer) {
    m_mainPlayer->setClientContext(m_clientContext);
    m_mainPlayer->setStatistics(m_statistics);
    m_mainPlayer->setUniverseClient(std::ref(*this));
    auto uuidResult = m_mainPlayer->uuid();
    if (!uuidResult)
      return std::unexpected(std::move(uuidResult.error()));
    auto uuid = std::move(*uuidResult);
    auto backedUp = m_playerStorage->backupCycle(uuid);
    if (!backedUp)
      return std::unexpected(std::move(backedUp.error()));
    auto saved = m_playerStorage->savePlayer(m_mainPlayer);
    if (!saved)
      return std::unexpected(std::move(saved.error()));
    return m_playerStorage->moveToFront(uuid);
  }
  return {};
}

std::shared_ptr<Player> UniverseClient::mainPlayer() const {
  return m_mainPlayer;
}

std::expected<std::optional<String>, StarException> UniverseClient::connect(UniverseConnection connection, bool allowAssetsMismatch, String const& account, String const& password, bool const& forceLegacy) {
  auto joined = [&]() -> std::expected<std::optional<String>, StarException> {
    auto assetsResult = m_root.assets();
    if (!assetsResult)
      return std::unexpected(std::move(assetsResult.error()));
    auto assets = std::move(*assetsResult);
    auto operation1 = reset();
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
    m_disconnectReason.reset();
    if (!m_mainPlayer)
      return std::unexpected(StarException("Cannot call UniverseClient::connect with no main player"));
    auto timeoutJsonResult = assets->json("/client.config:serverConnectTimeout");
    if (!timeoutJsonResult)
      return std::unexpected(std::move(timeoutJsonResult.error()));
    auto timeoutJson = std::move(*timeoutJsonResult);
    auto timeoutResult = timeoutJson.toUInt();
    if (!timeoutResult)
      return std::unexpected(std::move(timeoutResult.error()));
    auto timeout = std::move(*timeoutResult);
    auto operation2 = m_root.logger().info("UniverseClient: Connecting to server, packet timeout is {}ms", timeout);
    if (!operation2)
      return std::unexpected(std::move(operation2.error()));
    auto protocolRequest = std::make_shared<ProtocolRequestPacket>(StarProtocolVersion);
    if (!forceLegacy)
      protocolRequest->setCompressionMode(PacketCompressionMode::Enabled);
    auto operation3 = connection.pushSingle(std::move(protocolRequest));
    if (!operation3)
      return std::unexpected(std::move(operation3.error()));
    auto operation4 = connection.sendAll(timeout);
    if (!operation4)
      return std::unexpected(std::move(operation4.error()));
    auto operation5 = connection.receiveAny(timeout);
    if (!operation5)
      return std::unexpected(std::move(operation5.error()));
    auto nextPacketResult = connection.pullSingle();
    if (!nextPacketResult)
      return std::unexpected(std::move(nextPacketResult.error()));
    auto nextPacket = std::move(*nextPacketResult);
    auto protocolResponse = as<ProtocolResponsePacket>(nextPacket);
    if (!nextPacket)
      return String("Join failed! Expected ProtocolResponse, but none received");
    if (!PacketTypeNames)
      return std::unexpected(PacketTypeNames.error());
    if (!protocolResponse) {
      auto typeResult = PacketTypeNames->getRight(nextPacket->type());
      if (!typeResult)
        return std::unexpected(std::move(typeResult.error()));
      auto type = std::move(*typeResult);
      return String(strf("Join failed! Expected ProtocolResponse, got {}", type.get()));
    }
    if (!protocolResponse->allowed)
      return String(strf("Join failed! Server does not support connections with protocol version {}", StarProtocolVersion));
    NetCompatibilityRules rules;
    rules.setVersion(LegacyVersion);
    bool legacyServer = forceLegacy || protocolResponse->compressionMode() != PacketCompressionMode::Enabled;
    auto socketResult = connection.packetSocket();
    if (!socketResult)
      return std::unexpected(std::move(socketResult.error()));
    auto& socket = socketResult->get();
    if (!legacyServer) {
      auto compressed = asRef<CompressedPacketSocket>(socket);
      if (protocolResponse->info) {
        auto versionResult = protocolResponse->info.getUInt("openProtocolVersion", 1);
        if (!versionResult)
          return std::unexpected(std::move(versionResult.error()));
        auto version = std::move(*versionResult);
        rules.setVersion(version);
        auto compressionNameResult = protocolResponse->info.getString("compression", "None");
        if (!compressionNameResult)
          return std::unexpected(std::move(compressionNameResult.error()));
        auto compressionName = std::move(*compressionNameResult);
        if (compressed) {
          if (!NetCompressionModeNames)
            return std::unexpected(NetCompressionModeNames.error());
          auto compressionResult = NetCompressionModeNames->maybeLeft(compressionName);
          if (!compressionResult)
            return std::unexpected(std::move(compressionResult.error()));
          auto compression = std::move(*compressionResult);
          if (!compression)
            return String(strf("Join failed! Unknown net stream connection type '{}'", compressionName));
          auto nameResult = NetCompressionModeNames->getRight(*compression);
          if (!nameResult)
            return std::unexpected(std::move(nameResult.error()));
          auto name = std::move(*nameResult);
          auto operation6 = m_root.logger().info("UniverseClient: Using '{}' network stream compression", name.get());
          if (!operation6)
            return std::unexpected(std::move(operation6.error()));
          compressed->get().setCompressionStreamEnabled(*compression == NetCompressionMode::Zstd);
        }
      } else {
        rules.setVersion(1);
        if (compressed) {
          auto operation7 = m_root.logger().info("UniverseClient: Defaulting to Zstd network stream compression (older server version)");
          if (!operation7)
            return std::unexpected(std::move(operation7.error()));
          compressed->get().setCompressionStreamEnabled(true);
        }
      }
    }
    socket.setNetRules(rules);
    auto uuidResult = m_mainPlayer->uuid();
    if (!uuidResult)
      return std::unexpected(std::move(uuidResult.error()));
    auto uuid = std::move(*uuidResult);
    auto nameResult = m_mainPlayer->name();
    if (!nameResult)
      return std::unexpected(std::move(nameResult.error()));
    auto name = std::move(*nameResult);
    auto shipDataResult = m_playerStorage->loadShipData(uuid);
    if (!shipDataResult)
      return std::unexpected(std::move(shipDataResult.error()));
    auto shipData = std::move(*shipDataResult);
    auto request = std::make_shared<ClientConnectPacket>(assets->digest(), allowAssetsMismatch, uuid, name,
        m_mainPlayer->shipSpecies(), std::move(shipData), m_mainPlayer->shipUpgrades(), m_mainPlayer->log()->introComplete(), account);
    request->info = JsonObject{{"brand", "OpenStarbound"}, {"openProtocolVersion", OpenProtocolVersion}};
    auto operation8 = connection.pushSingle(std::move(request));
    if (!operation8)
      return std::unexpected(std::move(operation8.error()));
    auto operation9 = connection.sendAll(timeout);
    if (!operation9)
      return std::unexpected(std::move(operation9.error()));
    auto operation10 = connection.receiveAny(timeout);
    if (!operation10)
      return std::unexpected(std::move(operation10.error()));
    auto packetResult = connection.pullSingle();
    if (!packetResult)
      return std::unexpected(std::move(packetResult.error()));
    auto packet = std::move(*packetResult);
    if (auto challenge = as<HandshakeChallengePacket>(packet)) {
      auto operation11 = m_root.logger().info("UniverseClient: Sending Handshake Response");
      if (!operation11)
        return std::unexpected(std::move(operation11.error()));
      ByteArray passAccountSalt = (password + account).utf8Bytes();
      passAccountSalt.append(challenge->passwordSalt);
      ByteArray passHash = Star::sha256(passAccountSalt);
      auto operation12 = connection.pushSingle(std::make_shared<HandshakeResponsePacket>(passHash));
      if (!operation12)
        return std::unexpected(std::move(operation12.error()));
      auto operation13 = connection.sendAll(timeout);
      if (!operation13)
        return std::unexpected(std::move(operation13.error()));
      auto operation14 = connection.receiveAny(timeout);
      if (!operation14)
        return std::unexpected(std::move(operation14.error()));
      auto responseResult = connection.pullSingle();
      if (!responseResult)
        return std::unexpected(std::move(responseResult.error()));
      auto response = std::move(*responseResult);
      packet = std::move(response);
    }
    if (auto success = as<ConnectSuccessPacket>(packet)) {
      m_universeClock = std::make_shared<Clock>();
      auto contextResult = ClientContext::create(m_root, success->serverUuid, uuid);
      if (!contextResult)
        return std::unexpected(std::move(contextResult.error()));
      auto context = std::move(*contextResult);
      m_clientContext = std::move(context);
      m_clientContext->setNetCompatibilityRules(rules);
      m_teamClient = std::make_shared<TeamClient>(m_root, m_mainPlayer, m_clientContext);
      m_mainPlayer->setClientContext(m_clientContext);
      m_mainPlayer->setStatistics(m_statistics);
      auto worldResult = WorldClient::create(m_root, m_mainPlayer, m_luaRoot, *this);
      if (!worldResult)
        return std::unexpected(std::move(worldResult.error()));
      auto world = std::move(*worldResult);
      m_worldClient = std::move(world);
      m_worldClient->clientState().setNetCompatibilityRules(rules);
      m_worldClient->setAsyncLighting(true);
      m_connection = std::move(connection);
      auto celestialResult = CelestialSlaveDatabase::create(assets, m_root.random(), std::move(success->celestialInformation));
      if (!celestialResult)
        return std::unexpected(std::move(celestialResult.error()));
      auto celestial = std::move(*celestialResult);
      m_celestialDatabase = std::move(celestial);
      auto systemResult = SystemWorldClient::create(m_root, m_universeClock, m_celestialDatabase, m_mainPlayer->universeMap());
      if (!systemResult)
        return std::unexpected(std::move(systemResult.error()));
      auto system = std::move(*systemResult);
      m_systemWorldClient = std::move(system);
      auto operation15 = m_root.logger().info("UniverseClient: Joined {} server as client {}", legacyServer ? "Starbound" : "OpenStarbound", success->clientId);
      if (!operation15)
        return std::unexpected(std::move(operation15.error()));
      return {};
    }
    if (auto failure = as<ConnectFailurePacket>(packet)) {
      auto operation16 = m_root.logger().error("UniverseClient: Join failed: {}", failure->reason);
      if (!operation16)
        return std::unexpected(std::move(operation16.error()));
      return failure->reason;
    }
    if (packet) {
      auto typeResult = PacketTypeNames->getRight(packet->type());
      if (!typeResult)
        return std::unexpected(std::move(typeResult.error()));
      auto type = std::move(*typeResult);
      return String(strf("Join failed! Expected ConnectSuccess/Failure, got {}", type.get()));
    }
    return String("Join failed! Expected ConnectSuccess/Failure, but none received");
  }();
  if (!joined && (m_connection || m_worldClient || m_clientContext)) {
    auto cleared = reset();
    if (!cleared)
      return std::unexpected(joined.error().withContext(strf("Error rolling back universe connection: {}", cleared.error().what())));
  }
  return joined;
}

bool UniverseClient::isConnected() const {
  return m_connection && m_connection->isOpen();
}

std::expected<void, StarException> UniverseClient::disconnect() {
  auto drained = [&]() -> std::expected<void, StarException> {
    auto assetsResult = m_root.assets();
    if (!assetsResult)
      return std::unexpected(std::move(assetsResult.error()));
    auto assets = std::move(*assetsResult);
    auto timeoutJsonResult = assets->json("/client.config:serverDisconnectTimeout");
    if (!timeoutJsonResult)
      return std::unexpected(std::move(timeoutJsonResult.error()));
    auto timeoutJson = std::move(*timeoutJsonResult);
    auto timeoutResult = timeoutJson.toInt();
    if (!timeoutResult)
      return std::unexpected(std::move(timeoutResult.error()));
    auto timeout = std::move(*timeoutResult);
    if (isConnected()) {
      auto operation17 = m_root.logger().info("UniverseClient: Client disconnecting...");
      if (!operation17)
        return std::unexpected(std::move(operation17.error()));
      auto operation18 = m_connection->pushSingle(std::make_shared<ClientDisconnectRequestPacket>());
      if (!operation18)
        return std::unexpected(std::move(operation18.error()));
    }
    while (m_connection) {
      auto operation19 = m_connection->sendAll(timeout);
      if (!operation19)
        return std::unexpected(std::move(operation19.error()));
      auto receivedResult = m_connection->receiveAny(timeout);
      if (!receivedResult)
        return std::unexpected(std::move(receivedResult.error()));
      auto received = std::move(*receivedResult);
      if (!received)
        break;
      auto packetsResult = m_connection->pull();
      if (!packetsResult)
        return std::unexpected(std::move(packetsResult.error()));
      auto packets = std::move(*packetsResult);
      auto operation20 = handlePackets(packets);
      if (!operation20)
        return std::unexpected(std::move(operation20.error()));
    }
    return {};
  }();
  m_root.timing().timescale = 1.0f;
  auto cleared = reset();
  if (m_mainPlayer)
    m_mainPlayer->setUniverseClient(std::nullopt);
  m_mainPlayer.reset();
  if (!drained) {
    if (!cleared)
      return std::unexpected(drained.error().withContext(strf("Error resetting disconnected universe: {}", cleared.error().what())));
    return std::unexpected(std::move(drained.error()));
  }
  return cleared;
}

std::optional<String> UniverseClient::disconnectReason() const {
  return m_disconnectReason;
}

std::expected<unsigned, StarException> UniverseClient::connectionVersion() {
  if (!m_connection)
    return std::unexpected(StarException("Universe client is not connected"));
  auto socketResult = m_connection->packetSocket();
  if (!socketResult)
    return std::unexpected(std::move(socketResult.error()));
  auto socket = std::move(*socketResult);
  return socket.get().netRules().version();
}

std::shared_ptr<WorldClient> UniverseClient::worldClient() const {
  return m_worldClient;
}

std::shared_ptr<SystemWorldClient> UniverseClient::systemWorldClient() const {
  return m_systemWorldClient;
}

std::expected<void, StarException> UniverseClient::update(float dt) {
  auto assetsResult = m_root.assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  if (!isConnected())
    return {};
  if (!m_warping && std::holds_alternative<std::monostate>(m_pendingWarp)) {
    if (auto request = m_mainPlayer->pullPendingWarp()) {
      auto actionResult = parseWarpAction(request->action);
      if (!actionResult)
        return std::unexpected(std::move(actionResult.error()));
      auto action = std::move(*actionResult);
      auto operation42 = warpPlayer(action, bool(request->animation), request->animation.value_or("default"), request->deploy);
      if (!operation42)
        return std::unexpected(std::move(operation42.error()));
    }
  }
  if (!std::holds_alternative<std::monostate>(m_pendingWarp)) {
    auto versionResult = connectionVersion();
    if (!versionResult)
      return std::unexpected(std::move(versionResult.error()));
    auto version = std::move(*versionResult);
    if (version < 15) {
      if (std::holds_alternative<WarpToWorld>(m_pendingWarp)) {
        auto const& target = std::get<WarpToWorld>(m_pendingWarp);
        if (std::holds_alternative<CustomWorldId>(target.world) || std::holds_alternative<ClientCustomWorldId>(target.world)) {
          auto operation43 = m_root.logger().warn("Attempting to warp to a custom world on an old server, cancelling.");
          if (!operation43)
            return std::unexpected(std::move(operation43.error()));
          m_pendingWarp = WarpAction();
          m_warping.reset();
          m_warpDelay.reset();
        }
      }
    }
  }
  if (!std::holds_alternative<std::monostate>(m_pendingWarp) &&
      ((m_warping && !m_mainPlayer->isTeleportingOut()) || (!m_warping && m_warpDelay.tick(dt)))) {
    auto operation44 = m_connection->pushSingle(std::make_shared<PlayerWarpPacket>(take(m_pendingWarp), m_mainPlayer->isDeploying()));
    if (!operation44)
      return std::unexpected(std::move(operation44.error()));
    m_warpDelay.reset();
    if (m_warping) {
      auto minimumJsonResult = assets->json("/client.config:playerWarpCinemaMinimumTime");
      if (!minimumJsonResult)
        return std::unexpected(std::move(minimumJsonResult.error()));
      auto minimumJson = std::move(*minimumJsonResult);
      auto minimumResult = minimumJson.toFloat();
      if (!minimumResult)
        return std::unexpected(std::move(minimumResult.error()));
      auto minimum = std::move(*minimumResult);
      m_warpCinemaCancelTimer = GameTimer(minimum);
      String path = m_mainPlayer->isDeploying() ? std::string_view("/client.config:deployCinematic") : std::string_view("/client.config:warpCinematic");
      auto cinematicPathJsonResult = assets->json(path);
      if (!cinematicPathJsonResult)
        return std::unexpected(std::move(cinematicPathJsonResult.error()));
      auto cinematicPathJson = std::move(*cinematicPathJsonResult);
      auto cinematicPathResult = cinematicPathJson.toString();
      if (!cinematicPathResult)
        return std::unexpected(std::move(cinematicPathResult.error()));
      auto cinematicPath = std::move(*cinematicPathResult);
      cinematicPath = cinematicPath.replaceTags(StringMap<String>{{"species", m_mainPlayer->species()}});
      auto baseResult = assets->json(path + "Base");
      if (!baseResult)
        return std::unexpected(std::move(baseResult.error()));
      auto base = std::move(*baseResult);
      auto cinematicResult = assets->json(cinematicPath);
      if (!cinematicResult)
        return std::unexpected(std::move(cinematicResult.error()));
      auto cinematic = std::move(*cinematicResult);
      auto merged = jsonMerge(base, cinematic);
      auto operation45 = m_mainPlayer->setPendingCinematic(merged);
      if (!operation45)
        return std::unexpected(std::move(operation45.error()));
    }
  }
  if (m_warpCinemaCancelTimer) {
    m_warpCinemaCancelTimer->tick(m_root.timing().timestep);
    if (m_warpCinemaCancelTimer->ready() && !m_warping) {
      m_warpCinemaCancelTimer.reset();
      auto operation46 = m_mainPlayer->setPendingCinematic(Json());
      if (!operation46)
        return std::unexpected(std::move(operation46.error()));
      auto operation47 = m_mainPlayer->teleportIn();
      if (!operation47)
        return std::unexpected(std::move(operation47.error()));
    }
  }
  auto operation48 = m_connection->receive();
  if (!operation48)
    return std::unexpected(std::move(operation48.error()));
  auto handled = [&]() -> std::expected<void, StarException> {
    auto packetsResult = m_connection->pull();
    if (!packetsResult)
      return std::unexpected(std::move(packetsResult.error()));
    auto packets = std::move(*packetsResult);
    return handlePackets(packets);
  }();
  if (!handled) {
    auto logged = m_root.logger().errorException("Exception caught handling incoming server packets", ExceptionDiagnostic(handled.error()));
    auto cleared = reset();
    if (!m_disconnectReason)
      m_disconnectReason = "Exception caught handling incoming server packets, check log";
    if (!logged) {
      auto error = logged.error().withContext(strf("Original server packet failure: {}", handled.error().what()));
      if (!cleared)
        error = error.withContext(strf("Error resetting failed universe: {}", cleared.error().what()));
      return std::unexpected(std::move(error));
    }
    if (!cleared)
      return std::unexpected(cleared.error().withContext(strf("Original server packet failure: {}", handled.error().what())));
  }
  if (!isConnected())
    return {};
  m_root.logMap().set("universe_time_client", m_universeClock->time());
  auto operation49 = m_statistics->update();
  if (!operation49)
    return std::unexpected(std::move(operation49.error()));
  if (!*m_pause) {
    auto operation50 = m_worldClient->update(dt);
    if (!operation50)
      return std::unexpected(std::move(operation50.error()));
    for (auto& p : m_scriptContexts) {
      auto operation51 = p.second->update();
      if (!operation51)
        return std::unexpected(std::move(operation51.error()));
    }
  }
  auto operation52 = m_connection->push(m_worldClient->getOutgoingPackets());
  if (!operation52)
    return std::unexpected(std::move(operation52.error()));
  auto versionResult = connectionVersion();
  if (!versionResult)
    return std::unexpected(std::move(versionResult.error()));
  auto version = std::move(*versionResult);
  if (version >= 16) {
    for (auto& p : m_subWorldThreads) {
      auto operation53 = m_connection->push(p.second->pullOutgoingPackets());
      if (!operation53)
        return std::unexpected(std::move(operation53.error()));
    }
  }
  for (auto const& uuid : m_universeMessagePromises.keys()) {
    auto& promise = m_universeMessagePromises[uuid];
    auto finishedResult = promise.finished();
    if (!finishedResult)
      return std::unexpected(std::move(finishedResult.error()));
    auto finished = std::move(*finishedResult);
    if (!finished)
      continue;
    auto succeededResult = promise.succeeded();
    if (!succeededResult)
      return std::unexpected(std::move(succeededResult.error()));
    auto succeeded = std::move(*succeededResult);
    if (succeeded) {
      auto resultResult = promise.result();
      if (!resultResult)
        return std::unexpected(std::move(resultResult.error()));
      auto result = std::move(*resultResult);
      if (!result)
        return std::unexpected(StarException("Completed universe message has no result"));
      auto operation54 = m_connection->pushSingle(std::make_shared<UniverseMessageResponse>(std::move(*result), uuid));
      if (!operation54)
        return std::unexpected(std::move(operation54.error()));
    } else {
      auto errorResult = promise.error();
      if (!errorResult)
        return std::unexpected(std::move(errorResult.error()));
      auto error = std::move(*errorResult);
      if (!error)
        return std::unexpected(StarException("Failed universe message has no error"));
      auto operation55 =
        m_connection->pushSingle(std::make_shared<UniverseMessageResponse>(std::unexpected(std::move(*error)), uuid));
      if (!operation55)
        return std::unexpected(std::move(operation55.error()));
    }
    m_universeMessagePromises.remove(uuid);
  }
  List<Message> messages;
  {
    std::unique_lock locker(m_messageMutex, std::defer_lock);
    lockWithDiagnostics(locker);
    messages = std::move(m_universeMessages);
  }
  for (std::size_t i = 0; i != messages.size(); ++i) {
    auto& message = messages[i];
    auto keeper = std::holds_alternative<RpcPromiseKeeper<Json>>(message.keeper)
        ? std::optional{std::ref(std::get<RpcPromiseKeeper<Json>>(message.keeper))} : std::nullopt;
    auto delivered = [&]() -> std::expected<void, StarException> {
      if (!keeper)
        return std::unexpected(StarException("Universe client message requires a local response keeper"));
      auto responseResult = receiveMessage(message.message, true, message.args);
      if (!responseResult)
        return std::unexpected(std::move(responseResult.error()));
      auto response = std::move(*responseResult);
      if (!response)
        return keeper->get().fail("Message not handled by universe");
      struct DeliverResponseToKeeper {
        RpcPromiseKeeper<Json>& keeper;

        std::expected<void, StarException> operator()(RpcPromise<Json>& promise) const {
          return keeper.chain(std::move(promise));
        }

        std::expected<void, StarException> operator()(Json& result) const { return keeper.fulfill(result); }
      };
      return std::visit(DeliverResponseToKeeper{keeper->get()}, *response);
    }();
    if (!delivered) {
      auto error = std::move(delivered.error());
      if (keeper) {
        auto failed = keeper->get().fail(String(std::string_view(error.what())));
        if (!failed)
          error = error.withContext(strf("Error rejecting failed universe message: {}", failed.error().what()));
      }
      std::unique_lock locker(m_messageMutex, std::defer_lock);
      lockWithDiagnostics(locker);
      messages.erase(messages.begin(), messages.begin() + i + 1);
      messages.appendAll(std::move(m_universeMessages));
      m_universeMessages = std::move(messages);
      return std::unexpected(std::move(error));
    }
  }
  if (!*m_pause) {
    auto operation59 = m_systemWorldClient->update(dt);
    if (!operation59)
      return std::unexpected(std::move(operation59.error()));
  }
  auto operation60 = m_connection->push(m_systemWorldClient->pullOutgoingPackets());
  if (!operation60)
    return std::unexpected(std::move(operation60.error()));
  auto operation61 = m_teamClient->update();
  if (!operation61)
    return std::unexpected(std::move(operation61.error()));
  for (auto& result : m_teamClient->pullInviteResults()) {
    String printout;
    if (std::holds_alternative<std::pair<String, bool>>(result)) {
      auto const& pair = std::get<std::pair<String, bool>>(result);
      printout = pair.second ? strf("Invited {}", pair.first) : strf("Couldn't find anyone to invite named {}", pair.first);
    } else if (std::holds_alternative<StringList>(result)) {
      auto const& invited = std::get<StringList>(result);
      for (std::size_t i = 0; i != invited.size(); ++i) {
        if (!printout.empty())
          printout += i == invited.size() - 1 ? std::string_view("^reset;, and ") : std::string_view("^reset;, ");
        printout += invited[i];
      }
      printout = "Invited " + printout;
    }
    m_pendingMessages.emplaceAppend(MessageContext::CommandResult, 0, "", printout);
  }
  auto contextUpdateResult = m_clientContext->writeUpdate(m_clientContext->netCompatibilityRules());
  if (!contextUpdateResult)
    return std::unexpected(std::move(contextUpdateResult.error()));
  auto contextUpdate = std::move(*contextUpdateResult);
  if (!contextUpdate.empty()) {
    auto operation62 = m_connection->pushSingle(std::make_shared<ClientContextUpdatePacket>(std::move(contextUpdate)));
    if (!operation62)
      return std::unexpected(std::move(operation62.error()));
  }
  auto requests = m_celestialDatabase->pullRequests();
  if (!requests.empty()) {
    auto operation63 = m_connection->pushSingle(std::make_shared<CelestialRequestPacket>(std::move(requests)));
    if (!operation63)
      return std::unexpected(std::move(operation63.error()));
  }
  auto operation64 = m_connection->send();
  if (!operation64)
    return std::unexpected(std::move(operation64.error()));
  if (Time::monotonicMilliseconds() >= m_storageTriggerDeadline) {
    if (m_mainPlayer) {
      auto operation65 = m_playerStorage->savePlayer(m_mainPlayer);
      if (!operation65)
        return std::unexpected(std::move(operation65.error()));
      auto originalResult = playerIsOriginal();
      if (!originalResult)
        return std::unexpected(std::move(originalResult.error()));
      auto original = std::move(*originalResult);
      if (original) {
        auto uuidResult = m_mainPlayer->uuid();
        if (!uuidResult)
          return std::unexpected(std::move(uuidResult.error()));
        auto uuid = std::move(*uuidResult);
        auto operation66 = m_playerStorage->moveToFront(uuid);
        if (!operation66)
          return std::unexpected(std::move(operation66.error()));
      }
    }
    auto intervalJsonResult = assets->json("/client.config:storageTriggerInterval");
    if (!intervalJsonResult)
      return std::unexpected(std::move(intervalJsonResult.error()));
    auto intervalJson = std::move(*intervalJsonResult);
    auto intervalResult = intervalJson.toUInt();
    if (!intervalResult)
      return std::unexpected(std::move(intervalResult.error()));
    auto interval = std::move(*intervalResult);
    m_storageTriggerDeadline = Time::monotonicMilliseconds() + interval;
    for (auto const& id : m_subWorldThreads.keys()) {
      auto entryResult = m_subWorldThreads.get(id);
      if (!entryResult)
        return std::unexpected(std::move(entryResult.error()));
      auto entry = std::move(*entryResult);
      auto thread = entry.get();
      if (thread->errorOccurred() || thread->shouldExpire()) {
        auto operation67 = m_root.logger().info("UniverseClient: Cleaning up subworld {}.", id);
        if (!operation67)
          return std::unexpected(std::move(operation67.error()));
        auto stopped = thread->stop();
        auto drained = thread->clearMessages();
        auto closed = thread->executeAction([](WorldClientThread&, WorldClient& world) -> std::expected<void, StarException> { return world.close(); });
        m_subWorldThreads.remove(id);
        if (!stopped)
          return std::unexpected(std::move(stopped.error()));
        if (!drained)
          return std::unexpected(std::move(drained.error()));
        if (!closed)
          return std::unexpected(std::move(closed.error()));
        if (m_subWorlds.hasLeftValue(id)) {
          m_subWorlds.removeLeft(id);
          auto operation68 = m_connection->pushSingle(std::make_shared<ClientSubWorldRequest>(id, WorldId()));
          if (!operation68)
            return std::unexpected(std::move(operation68.error()));
        }
      }
    }
  }
  if (!m_worldClient->mainPlayerDead() || m_mainPlayer->modeConfig().permadeath) {
    m_respawnWarped = m_respawning = false;
  } else if (m_respawning) {
    if (m_respawnTimer.tick(dt)) {
      bool ownShip = playerOnOwnShip();
      if (ownShip || m_worldClient->respawnInWorld()) {
        auto operation69 = m_worldClient->reviveMainPlayer();
        if (!operation69)
          return std::unexpected(std::move(operation69.error()));
        m_respawning = false;
      } else if (!ownShip && !m_respawnWarped) {
        if (m_respawnTimer.time > 0.f) {
          auto cinematicJsonResult = assets->json("/client.config:respawnCinematic");
          if (!cinematicJsonResult)
            return std::unexpected(std::move(cinematicJsonResult.error()));
          auto cinematicJson = std::move(*cinematicJsonResult);
          auto cinematicResult = cinematicJson.toString();
          if (!cinematicResult)
            return std::unexpected(std::move(cinematicResult.error()));
          auto cinematic = std::move(*cinematicResult);
          if (!PlayerModeNames)
            return std::unexpected(PlayerModeNames.error());
          auto modeResult = PlayerModeNames->getRight(m_mainPlayer->modeType());
          if (!modeResult)
            return std::unexpected(std::move(modeResult.error()));
          auto mode = std::move(*modeResult);
          cinematic = cinematic.replaceTags(StringMap<String>{{"species", m_mainPlayer->species()}, {"mode", mode.get()}});
          auto operation70 = m_mainPlayer->setPendingCinematic(Json(std::move(cinematic)));
          if (!operation70)
            return std::unexpected(std::move(operation70.error()));
        }
        m_respawnWarped = true;
        m_pendingWarp = WarpAlias::OwnShip;
        m_warpDelay.reset();
        m_respawning = false;
      }
    }
  } else if (m_respawnWarped) {
    if (m_respawnTimer.tick(dt) && playerOnOwnShip()) {
      auto operation71 = m_worldClient->reviveMainPlayer();
      if (!operation71)
        return std::unexpected(std::move(operation71.error()));
      m_respawnWarped = m_respawning = false;
    }
  } else {
    m_respawning = true;
    m_respawnTimer.reset();
  }
  m_celestialDatabase->cleanup();
  if (!PacketTypeNames)
    return std::unexpected(PacketTypeNames.error());
  if (auto stats = m_connection->incomingStats()) {
    m_root.logMap().set("net_total_incoming", strf("{:4.3f} kB/s", stats->bytesPerSecond / 1000.f));
    auto typeResult = PacketTypeNames->getRight(stats->worstPacketType);
    if (!typeResult)
      return std::unexpected(std::move(typeResult.error()));
    auto type = std::move(*typeResult);
    m_root.logMap().set("net_worst_incoming", strf("^cyan;{}^reset; ({:4.3f} kB/s)", type.get(), float(stats->worstPacketSize) / 1000.f));
  }
  if (auto stats = m_connection->outgoingStats()) {
    m_root.logMap().set("net_total_outgoing", strf("{:4.3f} kB/s", stats->bytesPerSecond / 1000.f));
    auto typeResult = PacketTypeNames->getRight(stats->worstPacketType);
    if (!typeResult)
      return std::unexpected(std::move(typeResult.error()));
    auto type = std::move(*typeResult);
    m_root.logMap().set("net_worst_outgoing", strf("^cyan;{}^reset; ({:4.3f} kB/s)", type.get(), float(stats->worstPacketSize) / 1000.f));
  }
  return {};
}

std::optional<BeamUpRule> UniverseClient::beamUpRule() const {
  if (auto worldTemplate = currentTemplate())
    if (auto parameters = worldTemplate->worldParameters())
      return parameters->beamUpRule;

  return {};
}

std::expected<bool, StarException> UniverseClient::canBeamUp() const {
  auto world = m_clientContext->playerWorldId();
  if (std::holds_alternative<std::monostate>(world) || std::holds_alternative<ClientShipWorldId>(world))
    return false;
  if (m_mainPlayer->isAdmin())
    return true;
  if (m_mainPlayer->isDead() || m_mainPlayer->isTeleporting())
    return false;
  auto rule = beamUpRule();
  if (rule == BeamUpRule::Anywhere || rule == BeamUpRule::AnywhereWithWarning)
    return true;
  if (rule == BeamUpRule::Surface) {
    if (m_mainPlayer->modeConfig().allowBeamUpUnderground)
      return true;
    return m_mainPlayer->isOutside();
  }
  return false;
}

std::expected<bool, StarException> UniverseClient::canBeamDown(bool deploy) const {
  auto orbit = m_clientContext->orbitWarpAction();
  if (!orbit || flying())
    return false;
  if (!deploy && orbit->second == WarpMode::DeployOnly)
    return false;
  if (deploy) {
    if (orbit->second == WarpMode::BeamOnly)
      return false;
    auto canDeployResult = m_mainPlayer->canDeploy();
    if (!canDeployResult)
      return std::unexpected(std::move(canDeployResult.error()));
    auto canDeploy = std::move(*canDeployResult);
    if (!canDeploy)
      return false;
  }
  if (m_mainPlayer->isAdmin())
    return true;
  return !m_mainPlayer->isDead() && !m_mainPlayer->isTeleporting() && m_clientContext->shipUpgrades().capabilities.contains("teleport");
}

std::expected<bool, StarException> UniverseClient::canBeamToTeamShip() const {
  auto world = m_clientContext->playerWorldId();
  if (std::holds_alternative<std::monostate>(world))
    return false;
  if (m_mainPlayer->isAdmin())
    return true;
  auto beamUpResult = canBeamUp();
  if (!beamUpResult)
    return std::unexpected(std::move(beamUpResult.error()));
  auto beamUp = std::move(*beamUpResult);
  if (beamUp)
    return true;
  return std::holds_alternative<ClientShipWorldId>(world) && m_clientContext->shipUpgrades().capabilities.contains("teleport");
}

std::expected<bool, StarException> UniverseClient::canTeleport() const {
  if (m_mainPlayer->isAdmin())
    return true;
  if (std::holds_alternative<ClientShipWorldId>(m_clientContext->playerWorldId()))
    return !flying() && m_clientContext->shipUpgrades().capabilities.contains("teleport");
  return m_mainPlayer->canUseTool();
}

std::expected<void, StarException> UniverseClient::warpPlayer(WarpAction const& warpAction, bool animate, String const& animationType, bool deploy) {
  if (m_warping || m_respawning)
    return {};
  auto operation21 = m_mainPlayer->stopLounging();
  if (!operation21)
    return std::unexpected(std::move(operation21.error()));
  if (std::holds_alternative<WarpToWorld>(warpAction)) {
    auto const& world = std::get<WarpToWorld>(warpAction);
    if (std::holds_alternative<std::monostate>(world.world) || world.world == playerWorld()) {
      if (std::holds_alternative<SpawnTargetPosition>(world.target))
        return m_mainPlayer->moveTo(std::get<SpawnTargetPosition>(world.target));
    }
  }
  if (animate) {
    auto operation22 = m_mainPlayer->teleportOut(animationType, deploy);
    if (!operation22)
      return std::unexpected(std::move(operation22.error()));
    m_warping = warpAction;
    m_warpDelay.reset();
  }
  m_pendingWarp = warpAction;
  return {};
}

std::expected<void, StarException> UniverseClient::flyShip(Vec3I const& system, SystemLocation const& destination, Json const& settings) {
  if (!m_connection)
    return std::unexpected(StarException("Universe client is not connected"));
  return m_connection->pushSingle(std::make_shared<FlyShipPacket>(system, destination, settings));
}

std::shared_ptr<CelestialDatabase> UniverseClient::celestialDatabase() {
  return m_celestialDatabase;
}

CelestialCoordinate UniverseClient::shipCoordinate() const {
  return m_clientContext->shipCoordinate();
}

bool UniverseClient::playerOnOwnShip() const {
  return std::holds_alternative<ClientShipWorldId>(playerWorld()) && std::get<ClientShipWorldId>(playerWorld()) == m_clientContext->playerUuid();
}

std::expected<bool, StarException> UniverseClient::playerIsOriginal() const {
  auto uuidResult = mainPlayer()->uuid();
  if (!uuidResult)
    return std::unexpected(std::move(uuidResult.error()));
  auto uuid = std::move(*uuidResult);
  return m_clientContext->playerUuid() == uuid;
}

WorldId UniverseClient::playerWorld() const {
  // Before the connect handshake has been answered there is no client context
  // yet, and a caller (UI, Lua, the warp path below) can ask this question
  // anyway, so report "no world" instead of dereferencing a null context.
  if (auto clientContext = m_clientContext)
    return clientContext->playerWorldId();

  return {};
}

bool UniverseClient::isAdmin() const {
  return m_mainPlayer->isAdmin();
}

Uuid UniverseClient::teamUuid() const {
  if (auto team = m_teamClient->currentTeam())
    return *team;
  return m_clientContext->playerUuid();
}

std::shared_ptr<WorldTemplate const> UniverseClient::currentTemplate() const {
  return m_worldClient->currentTemplate();
}

std::shared_ptr<Sky const> UniverseClient::currentSky() const {
  return m_worldClient->currentSky();
}

bool UniverseClient::flying() const {
  if (auto sky = currentSky())
    return sky->flying();
  return false;
}

std::expected<void, StarException> UniverseClient::sendChat(String const& text, ChatSendMode sendMode, std::optional<bool> speak, std::optional<JsonObject> data) {
  auto commandResult = text.beginsWith("/");
  if (!commandResult)
    return std::unexpected(std::move(commandResult.error()));
  auto command = std::move(*commandResult);
  if (speak.value_or(!command)) {
    auto operation23 = m_mainPlayer->addChatMessage(text);
    if (!operation23)
      return std::unexpected(std::move(operation23.error()));
  }
  auto packet = std::make_shared<ChatSendPacket>(text, sendMode);
  if (data)
    packet->data = std::move(*data);
  if (!m_connection)
    return std::unexpected(StarException("Universe client is not connected"));
  return m_connection->pushSingle(std::move(packet));
}

List<ChatReceivedMessage> UniverseClient::pullChatMessages() {
  return take(m_pendingMessages);
}

std::uint16_t UniverseClient::players() {
  return m_serverInfo.transform([](auto const& info) { return info.players; }).value_or(1);
}

std::uint16_t UniverseClient::maxPlayers() {
  return m_serverInfo.transform([](auto const& info) { return info.maxPlayers; }).value_or(1);
}

std::expected<void, StarException> UniverseClient::setLuaCallbacks(String const& groupName, LuaCallbacks const& callbacks) {
  m_luaRoot->addCallbacks(groupName, callbacks);
  return {};
}

std::expected<void, StarException> UniverseClient::restartLua() {
  auto operation24 = m_luaRoot->restart();
  if (!operation24)
    return std::unexpected(std::move(operation24.error()));
  auto assetsResult = m_root.assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  auto configResult = assets->json("/client.config");
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto config = std::move(*configResult);
  auto pauseResult = config.getFloat("luaGcPause");
  if (!pauseResult)
    return std::unexpected(std::move(pauseResult.error()));
  auto pause = std::move(*pauseResult);
  auto stepResult = config.getFloat("luaGcStepMultiplier");
  if (!stepResult)
    return std::unexpected(std::move(stepResult.error()));
  auto step = std::move(*stepResult);
  m_luaRoot->tuneAutoGarbageCollection(pause, step);
  auto configurationResult = m_root.configuration();
  if (!configurationResult)
    return std::unexpected(std::move(configurationResult.error()));
  auto configuration = std::move(*configurationResult);
  auto enableResult = configuration->getPath("safe.enableImGui");
  if (!enableResult)
    return std::unexpected(std::move(enableResult.error()));
  auto enable = std::move(*enableResult);
  if (enable) {
    auto enabledResult = enable.toBool();
    if (!enabledResult)
      return std::unexpected(std::move(enabledResult.error()));
    auto enabled = std::move(*enabledResult);
    if (enabled)
      return m_luaRoot->luaEngine().addImGui();
  }
  return {};
}

std::expected<void, StarException> UniverseClient::startLuaScripts() {
  auto started = [&]() -> std::expected<void, StarException> {
    auto assetsResult = m_root.assets();
    if (!assetsResult)
      return std::unexpected(std::move(assetsResult.error()));
    auto assets = std::move(*assetsResult);
    auto contextsJsonResult = assets->json("/client.config:universeScriptContexts");
    if (!contextsJsonResult)
      return std::unexpected(std::move(contextsJsonResult.error()));
    auto contextsJson = std::move(*contextsJsonResult);
    auto contextsResult = contextsJson.toObject();
    if (!contextsResult)
      return std::unexpected(std::move(contextsResult.error()));
    auto contexts = std::move(*contextsResult);
    for (auto& p : contexts) {
      auto script = std::make_shared<ScriptComponent>(m_root);
      auto operation25 = script->setLuaRoot(m_luaRoot);
      if (!operation25)
        return std::unexpected(std::move(operation25.error()));
      auto scriptsJsonResult = p.second.toArray();
      if (!scriptsJsonResult)
        return std::unexpected(std::move(scriptsJsonResult.error()));
      auto scriptsJson = std::move(*scriptsJsonResult);
      auto scriptsResult = jsonToStringList(scriptsJson);
      if (!scriptsResult)
        return std::unexpected(std::move(scriptsResult.error()));
      auto scripts = std::move(*scriptsResult);
      auto operation26 = script->setScripts(std::move(scripts));
      if (!operation26)
        return std::unexpected(std::move(operation26.error()));
      auto callbacksResult = LuaBindings::makeUniverseClientThreadCallbacks(m_root, *this);
      if (!callbacksResult)
        return std::unexpected(std::move(callbacksResult.error()));
      auto callbacks = std::move(*callbacksResult);
      auto operation27 = script->addThreadCallbacks("universe", std::move(callbacks));
      if (!operation27)
        return std::unexpected(std::move(operation27.error()));
      m_scriptContexts.set(p.first, script);
      auto operation28 = script->init();
      if (!operation28)
        return std::unexpected(std::move(operation28.error()));
    }
    return {};
  }();
  if (!started) {
    auto stopped = stopLua();
    if (!stopped)
      return std::unexpected(started.error().withContext(strf("Error rolling back universe scripts: {}", stopped.error().what())));
  }
  return started;
}

std::expected<void, StarException> UniverseClient::stopLua() {
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (error)
        *error = error->withContext(strf("Additional universe cleanup failure: {}", result.error().what()));
      else
        error.emplace(std::move(result.error()));
    }
  };
  for (auto& p : m_scriptContexts)
    remember(p.second->uninit());
  m_scriptContexts.clear();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::shared_ptr<LuaRoot> UniverseClient::luaRoot() {
  return m_luaRoot;
}

std::expected<bool, StarException> UniverseClient::reloadPlayer(Json const& data, Uuid const&, bool resetInterfaces, bool showIndicator) {
  auto player = mainPlayer();
  bool playerInWorld = player->inWorld();
  auto world = optionalRef(m_worldClient);
  if (playerInWorld) {
    auto playerWorldResult = player->world();
    if (!playerWorldResult)
      return std::unexpected(std::move(playerWorldResult.error()));
    auto& playerWorld = playerWorldResult->get();
    world = asRef<WorldClient>(playerWorld);
  }
  if (!world)
    return std::unexpected(StarException("Cannot reload player without a client world"));
  EntityId entityId = player->entityId();
  if (!playerInWorld && world->get().inWorld()) {
    auto spaceResult = connectionEntitySpace(world->get().connection());
    if (!spaceResult)
      return std::unexpected(std::move(spaceResult.error()));
    auto space = std::move(*spaceResult);
    entityId = space.first;
  }
  if (m_playerReloadPreCallback) {
    auto operation39 = m_playerReloadPreCallback(resetInterfaces);
    if (!operation39)
      return std::unexpected(std::move(operation39.error()));
  }
  std::shared_ptr<Projectile> indicator;
  if (playerInWorld && showIndicator) {
    auto databaseResult = m_root.projectileDatabase();
    if (!databaseResult)
      return std::unexpected(std::move(databaseResult.error()));
    auto database = std::move(*databaseResult);
    auto configResult = database->projectileConfig("opensb:playerloading");
    if (!configResult)
      return std::unexpected(std::move(configResult.error()));
    auto config = std::move(*configResult);
    auto createdResult = database->createProjectile("stationpartsound", config);
    if (!createdResult)
      return std::unexpected(std::move(createdResult.error()));
    auto created = std::move(*createdResult);
    indicator = std::move(created);
    indicator->setInitialPosition(player->position());
    indicator->setInitialDirection({1.0f, 0.0f});
    auto operation40 = world->get().addEntity(indicator);
    if (!operation40)
      return std::unexpected(std::move(operation40.error()));
  }
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (error)
        *error = error->withContext(strf("Additional universe cleanup failure: {}", result.error().what()));
      else
        error.emplace(std::move(result.error()));
    }
  };
  auto loaded = [&]() -> std::expected<void, StarException> {
    if (playerInWorld) {
      auto operation41 = world->get().removeEntity(player->entityId(), false);
      if (!operation41)
        return std::unexpected(std::move(operation41.error()));
    } else {
      m_respawning = false;
      m_respawnTimer.reset();
    }
    auto originalDataResult = m_playerStorage->savePlayer(player);
    if (!originalDataResult)
      return std::unexpected(std::move(originalDataResult.error()));
    auto originalData = std::move(*originalDataResult);
    auto changed = [&]() -> std::expected<void, StarException> {
      auto movementResult = originalData.get("movementController");
      if (!movementResult)
        return std::unexpected(std::move(movementResult.error()));
      auto movement = std::move(*movementResult);
      auto newDataResult = data.set("movementController", std::move(movement));
      if (!newDataResult)
        return std::unexpected(std::move(newDataResult.error()));
      auto newData = std::move(*newDataResult);
      return player->diskLoad(newData);
    }();
    if (!changed) {
      auto restored = player->diskLoad(originalData);
      if (!restored)
        return std::unexpected(changed.error().withContext(strf("Error restoring original player data: {}", restored.error().what())));
    }
    return changed;
  }();
  remember(std::move(loaded));
  if (!world->get().entity(entityId))
    remember(world->get().addEntity(player, entityId));
  if (indicator && indicator->inWorld())
    remember(world->get().removeEntity(indicator->entityId(), false));
  CelestialCoordinate coordinate(m_systemWorldClient->location());
  remember(player->universeMap()->addMappedCoordinate(coordinate));
  remember(player->universeMap()->filterMappedObjects(coordinate, m_systemWorldClient->objectKeys()));
  if (m_playerReloadCallback)
    remember(m_playerReloadCallback(resetInterfaces));
  if (error)
    return std::unexpected(std::move(*error));
  return true;
}

std::expected<bool, StarException> UniverseClient::switchPlayer(Uuid const& uuid) {
  auto currentResult = mainPlayer()->uuid();
  if (!currentResult)
    return std::unexpected(std::move(currentResult.error()));
  auto current = std::move(*currentResult);
  if (uuid == current)
    return false;
  if (auto data = m_playerStorage->maybeGetPlayerData(uuid)) {
    auto reloadedResult = reloadPlayer(*data, uuid, true, true);
    if (!reloadedResult)
      return std::unexpected(std::move(reloadedResult.error()));
    auto reloaded = std::move(*reloadedResult);
    if (reloaded) {
      auto assetsResult = m_root.assets();
      if (!assetsResult)
        return std::unexpected(std::move(assetsResult.error()));
      auto assets = std::move(*assetsResult);
      auto configResult = assets->json("/player.config");
      if (!configResult)
        return std::unexpected(std::move(configResult.error()));
      auto config = std::move(*configResult);
      auto danceResult = config.optString("swapDance");
      if (!danceResult)
        return std::unexpected(std::move(danceResult.error()));
      auto dance = std::move(*danceResult);
      if (dance) {
        auto operation29 = m_mainPlayer->humanoid()->setDance(*dance);
        if (!operation29)
          return std::unexpected(std::move(operation29.error()));
      }
      return true;
    }
  }
  return false;
}

std::expected<bool, StarException> UniverseClient::switchPlayer(std::size_t index) {
  if (auto uuid = m_playerStorage->playerUuidAt(index))
    return switchPlayer(*uuid);
  return false;
}

std::expected<bool, StarException> UniverseClient::switchPlayer(String const& name) {
  auto currentResult = mainPlayer()->uuid();
  if (!currentResult)
    return std::unexpected(std::move(currentResult.error()));
  auto current = std::move(*currentResult);
  auto uuidResult = m_playerStorage->playerUuidByName(name, current);
  if (!uuidResult)
    return std::unexpected(std::move(uuidResult.error()));
  auto uuid = std::move(*uuidResult);
  if (uuid)
    return switchPlayer(*uuid);
  if (name.utf8Size() == UuidSize * 2) {
    auto parsedResult = Uuid::create(name);
    if (!parsedResult)
      return std::unexpected(std::move(parsedResult.error()));
    auto parsed = std::move(*parsedResult);
    return switchPlayer(parsed);
  }
  return false;
}

UniverseClient::ReloadPlayerCallback& UniverseClient::playerReloadPreCallback() {
  return m_playerReloadPreCallback;
}

UniverseClient::ReloadPlayerCallback& UniverseClient::playerReloadCallback() {
  return m_playerReloadCallback;
}

std::shared_ptr<Clock const> UniverseClient::universeClock() const {
  return m_universeClock;
}

std::shared_ptr<JsonRpcInterface> UniverseClient::rpcInterface() const {
  return m_clientContext->rpcInterface();
}

std::shared_ptr<ClientContext> UniverseClient::clientContext() const {
  return m_clientContext;
}

std::shared_ptr<TeamClient> UniverseClient::teamClient() const {
  return m_teamClient;
}

std::shared_ptr<QuestManager> UniverseClient::questManager() const {
  return m_mainPlayer->questManager();
}

std::shared_ptr<PlayerStorage> UniverseClient::playerStorage() const {
  return m_playerStorage;
}

std::shared_ptr<Statistics> UniverseClient::statistics() const {
  return m_statistics;
}

std::shared_ptr<UniverseClient::ScriptComponent> UniverseClient::scriptContext(String const& contextName) {
  if (auto context = m_scriptContexts.maybeRef(contextName))
    return context->get();
  else
    return nullptr;
}

std::expected<void, StarException> UniverseClient::createCustomWorld(String const& name, Json templateData) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto directoryResult = File::isDirectory(m_customWorldStorageDirectory);
  if (!directoryResult)
    return std::unexpected(std::move(directoryResult.error()));
  auto directory = std::move(*directoryResult);
  if (!directory) {
    auto operation30 = m_root.logger().info("UniverseClient: Creating universe client storage directory");
    if (!operation30)
      return std::unexpected(std::move(operation30.error()));
    auto operation31 = File::makeDirectory(m_customWorldStorageDirectory);
    if (!operation31)
      return std::unexpected(std::move(operation31.error()));
  }
  String filename = File::relativeTo(m_customWorldStorageDirectory, strf("{}.world", name));
  auto existsResult = File::exists(filename);
  if (!existsResult)
    return std::unexpected(std::move(existsResult.error()));
  auto exists = std::move(*existsResult);
  if (!exists) {
    if (!m_connection)
      return std::unexpected(StarException("Universe client is not connected"));
    return m_connection->pushSingle(std::make_shared<ClientCustomWorldCreate>(name, std::move(templateData)));
  }
  return {};
}

std::expected<ClientSubWorldId, StarException> UniverseClient::createSubWorld() {
  auto idResult = m_subWorldThreads.nextId();
  if (!idResult)
    return std::unexpected(std::move(idResult.error()));
  auto id = std::move(*idResult);
  auto threadResult = WorldClientThread::create(m_root, id, *this);
  if (!threadResult)
    return std::unexpected(std::move(threadResult.error()));
  auto thread = std::move(*threadResult);
  thread->setPause(m_pause);
  auto operation32 = thread->start();
  if (!operation32)
    return std::unexpected(std::move(operation32.error()));
  auto added = m_subWorldThreads.add(id, thread);
  if (!added) {
    auto stopped = thread->stop();
    auto drained = thread->clearMessages();
    auto closed = thread->executeAction([](WorldClientThread&, WorldClient& world) -> std::expected<void, StarException> { return world.close(); });
    auto error = std::move(added.error());
    if (!stopped)
      error = error.withContext(strf("Error stopping rejected subworld: {}", stopped.error().what()));
    if (!drained)
      error = error.withContext(strf("Error draining rejected subworld: {}", drained.error().what()));
    if (!closed)
      error = error.withContext(strf("Error closing rejected subworld: {}", closed.error().what()));
    return std::unexpected(std::move(error));
  }
  return id;
}

std::expected<void, StarException> UniverseClient::setSubWorldWorld(ClientSubWorldId subWorldId, WorldId worldId) {
  auto versionResult = connectionVersion();
  if (!versionResult)
    return std::unexpected(std::move(versionResult.error()));
  auto version = std::move(*versionResult);
  if (version < 16)
    return m_root.logger().error("UniverseClient: Not setting subworld world, server is too old.");
  if (!std::holds_alternative<std::monostate>(worldId)) {
    auto nameResult = printWorldId(worldId);
    if (!nameResult)
      return std::unexpected(std::move(nameResult.error()));
    auto name = std::move(*nameResult);
    if (m_subWorlds.hasRightValue(worldId))
      return m_root.logger().error("UniverseClient: Not setting subworld world, one is already on world {}.", name);
    auto operation33 = m_root.logger().info("UniverseClient: Requesting subworld {} on world {}", subWorldId, name);
    if (!operation33)
      return std::unexpected(std::move(operation33.error()));
    auto operation34 = m_subWorlds.add(subWorldId, worldId);
    if (!operation34)
      return std::unexpected(std::move(operation34.error()));
  } else if (m_subWorlds.hasLeftValue(subWorldId)) {
    auto operation35 = m_root.logger().info("UniverseClient: Requesting destroy of subworld {}", subWorldId);
    if (!operation35)
      return std::unexpected(std::move(operation35.error()));
    m_subWorlds.removeLeft(subWorldId);
  }
  return m_connection->pushSingle(std::make_shared<ClientSubWorldRequest>(subWorldId, std::move(worldId)));
}

bool UniverseClient::subWorldExistsOnWorld(WorldId worldId) const {
  return m_subWorlds.hasRightValue(worldId);
}

std::expected<ClientSubWorldId, StarException> UniverseClient::getSubWorldOnWorld(WorldId worldId) {
  if (m_subWorlds.hasRightValue(worldId)) {
    auto idResult = m_subWorlds.getLeft(worldId);
    if (!idResult)
      return std::unexpected(std::move(idResult.error()));
    auto id = std::move(*idResult);
    return id.get();
  }
  ClientSubWorldId id = 0;
  for (auto const& p : m_subWorldThreads) {
    if (!p.second->errorOccurred() && !m_subWorlds.hasLeftValue(p.first)) {
      id = p.first;
      break;
    }
  }
  if (id == 0) {
    auto createdResult = createSubWorld();
    if (!createdResult)
      return std::unexpected(std::move(createdResult.error()));
    auto created = std::move(*createdResult);
    id = created;
  }
  if (!std::holds_alternative<std::monostate>(worldId)) {
    auto operation36 = setSubWorldWorld(id, std::move(worldId));
    if (!operation36)
      return std::unexpected(std::move(operation36.error()));
  }
  return id;
}

std::expected<void, StarException> UniverseClient::destroySubWorldOnWorld(WorldId worldId) {
  if (!m_subWorlds.hasRightValue(worldId))
    return {};
  auto idResult = m_subWorlds.getLeft(worldId);
  if (!idResult)
    return std::unexpected(std::move(idResult.error()));
  auto id = std::move(*idResult);
  return setSubWorldWorld(id.get(), WorldId());
}

std::expected<RpcPromise<Json>, StarException> UniverseClient::sendSubWorldOnWorldMessage(WorldId const& worldId, String const& message, JsonArray const& args) {
  auto versionResult = connectionVersion();
  if (!versionResult)
    return std::unexpected(std::move(versionResult.error()));
  auto version = std::move(*versionResult);
  if (version < 16)
    return RpcPromise<Json>::createFailed("Server too old");
  auto idResult = getSubWorldOnWorld(worldId);
  if (!idResult)
    return std::unexpected(std::move(idResult.error()));
  auto id = std::move(*idResult);
  auto threadResult = m_subWorldThreads.get(id);
  if (!threadResult)
    return std::unexpected(std::move(threadResult.error()));
  auto thread = std::move(*threadResult);
  auto pair = RpcPromise<Json>::createPair();
  thread.get()->passMessage({message, args, pair.second});
  return pair.first;
}

// though it resolves instantly, still use a promise for consistency
std::expected<RpcPromise<Json>, StarException> UniverseClient::sendMainWorldMessage(String const& message, JsonArray const& args) {
  if (!isConnected())
    return RpcPromise<Json>::createFailed("Not connected");
  auto responseResult = m_worldClient->receiveMessage(ServerConnectionId, message, args);
  if (!responseResult)
    return std::unexpected(std::move(responseResult.error()));
  auto response = std::move(*responseResult);
  if (!response)
    return RpcPromise<Json>::createFailed("Message not handled by world");
  struct ResponseToPromise {
    std::expected<RpcPromise<Json>, StarException> operator()(RpcPromise<Json>& promise) const {
      return std::move(promise);
    }

    std::expected<RpcPromise<Json>, StarException> operator()(Json& result) const {
      return RpcPromise<Json>::createFulfilled(result);
    }
  };
  return std::visit(ResponseToPromise{}, *response);
}

bool UniverseClient::paused() const {
  return *m_pause;
}

void UniverseClient::setPause(bool pause) {
  *m_pause = pause;

  if (pause)
    m_universeClock->stop();
  else
    m_universeClock->start();
}

std::expected<void, StarException> UniverseClient::handlePackets(List<std::shared_ptr<Packet>> const& packets) {
  if (!PacketTypeNames)
    return std::unexpected(PacketTypeNames.error());
  for (auto const& packet : packets) {
    auto typeResult = PacketTypeNames->getRight(packet->type());
    if (!typeResult)
      return std::unexpected(std::move(typeResult.error()));
    auto type = std::move(*typeResult);
    auto handled = [&]() -> std::expected<bool, StarException> {
      std::optional<Json> packetJson;
      auto functionName = strf("on{}Packet", type.get());
      for (auto& context : m_scriptContexts) {
        auto& optionalContext = context.second->context();
        if (!optionalContext)
          return std::unexpected(StarException("Universe script context is not initialized"));
        auto& lua = *optionalContext;
        auto methodResult = lua.get(functionName);
        if (!methodResult)
          return std::unexpected(std::move(methodResult.error()));
        auto method = std::move(*methodResult);
        if (!std::holds_alternative<LuaNilType>(method)) {
          if (!packetJson) {
            auto jsonResult = packet->writeJson();
            if (!jsonResult)
              return std::unexpected(std::move(jsonResult.error()));
            auto json = std::move(*jsonResult);
            packetJson = std::move(json);
          }
          auto functionResult = lua.luaTo<LuaFunction>(std::move(method));
          if (!functionResult)
            return std::unexpected(std::move(functionResult.error()));
          auto function = std::move(*functionResult);
          auto resultResult = function.invoke<LuaValue>(*packetJson);
          if (!resultResult)
            return std::unexpected(std::move(resultResult.error()));
          auto result = std::move(*resultResult);
          if (std::holds_alternative<LuaBoolean>(result) && std::get<LuaBoolean>(result))
            return false;
        }
      }
      if (auto update = as<ClientContextUpdatePacket>(packet)) {
        auto operation72 = m_clientContext->readUpdate(update->updateData, m_clientContext->netCompatibilityRules());
        if (!operation72)
          return std::unexpected(std::move(operation72.error()));
        auto operation73 = m_playerStorage->applyShipUpdates(m_clientContext->playerUuid(), m_clientContext->newShipUpdates());
        if (!operation73)
          return std::unexpected(std::move(operation73.error()));
        auto updates = m_clientContext->newCustomWorldUpdates();
        if (!updates.empty()) {
          std::unique_lock locker(m_mutex, std::defer_lock);
          lockWithDiagnostics(locker);
          for (auto& p : updates) {
            if (p.second.empty())
              continue;
            auto parentResult = p.first.contains("../");
            if (!parentResult)
              return std::unexpected(std::move(parentResult.error()));
            auto parent = std::move(*parentResult);
            if (!parent) {
              auto windowsParentResult = p.first.contains("..\\");
              if (!windowsParentResult)
                return std::unexpected(std::move(windowsParentResult.error()));
              auto windowsParent = std::move(*windowsParentResult);
              parent = windowsParent;
            }
            if (parent) {
              auto operation74 = m_root.logger().error("Not saving custom world named {}, path attempts to go up.", p.first);
              if (!operation74)
                return std::unexpected(std::move(operation74.error()));
            } else {
              String path = File::relativeTo(m_customWorldStorageDirectory, strf("{}.world", p.first));
              auto operation75 = WorldStorage::applyWorldChunksUpdateToFile(m_root.logger(), path, p.second);
              if (!operation75)
                return std::unexpected(std::move(operation75.error()));
            }
          }
        }
        auto originalResult = playerIsOriginal();
        if (!originalResult)
          return std::unexpected(std::move(originalResult.error()));
        auto original = std::move(*originalResult);
        if (original) {
          m_mainPlayer->setShipUpgrades(m_clientContext->shipUpgrades());
          if (playerOnOwnShip() && m_mainPlayer->inWorld()) {
            auto worldResult = m_mainPlayer->world();
            if (!worldResult)
              return std::unexpected(std::move(worldResult.error()));
            auto& world = worldResult->get();
            auto speciesJsonResult = world.getProperty("ship.species");
            if (!speciesJsonResult)
              return std::unexpected(std::move(speciesJsonResult.error()));
            auto speciesJson = std::move(*speciesJsonResult);
            if (speciesJson.isType(Json::Type::String)) {
              auto speciesResult = speciesJson.toString();
              if (!speciesResult)
                return std::unexpected(std::move(speciesResult.error()));
              auto species = std::move(*speciesResult);
              m_mainPlayer->setShipSpecies(std::move(species));
            }
          }
        }
        m_mainPlayer->setAdmin(m_clientContext->isAdmin());
        m_mainPlayer->setTeam(m_clientContext->team());
      } else if (auto chat = as<ChatReceivePacket>(packet)) {
        m_pendingMessages.append(chat->receivedMessage);
      } else if (auto time = as<UniverseTimeUpdatePacket>(packet)) {
        m_universeClock->setTime(time->universeTime);
      } else if (auto disconnect = as<ServerDisconnectPacket>(packet)) {
        auto operation76 = reset();
        if (!operation76)
          return std::unexpected(std::move(operation76.error()));
        m_disconnectReason = disconnect->reason;
        return true;
      } else if (auto celestial = as<CelestialResponsePacket>(packet)) {
        auto operation77 = m_celestialDatabase->pushResponses(std::move(celestial->responses));
        if (!operation77)
          return std::unexpected(std::move(operation77.error()));
      } else if (auto warp = as<PlayerWarpResultPacket>(packet)) {
        if (m_mainPlayer->isDeploying() && m_warping && std::holds_alternative<WarpToPlayer>(*m_warping)) {
          Uuid target = std::get<WarpToPlayer>(*m_warping);
          for (auto const& member : m_teamClient->members()) {
            if (member.uuid == target) {
              if (member.warpMode != WarpMode::DeployOnly && member.warpMode != WarpMode::BeamOrDeploy)
                m_mainPlayer->deployAbort();
              break;
            }
          }
        }
        m_warping.reset();
        if (!warp->success) {
          auto operation78 = m_mainPlayer->teleportAbort();
          if (!operation78)
            return std::unexpected(std::move(operation78.error()));
          if (warp->warpActionInvalid) {
            auto operation79 = m_mainPlayer->universeMap()->invalidateWarpAction(warp->warpAction);
            if (!operation79)
              return std::unexpected(std::move(operation79.error()));
          }
        }
      } else if (auto planet = as<PlanetTypeUpdatePacket>(packet)) {
        m_celestialDatabase->invalidateCacheFor(planet->coordinate);
      } else if (auto pause = as<PausePacket>(packet)) {
        setPause(pause->pause);
        m_root.timing().timescale = std::clamp(pause->timescale, 0.0f, 1024.f);
      } else if (auto info = as<ServerInfoPacket>(packet)) {
        m_serverInfo = ServerInfo{info->players, info->maxPlayers};
      } else if (auto request = as<ClientCustomWorldRequest>(packet)) {
        auto operation80 = m_root.logger().info("UniverseClient: Received request for client custom world {}.", request->name);
        if (!operation80)
          return std::unexpected(std::move(operation80.error()));
        auto parentResult = request->name.contains("../");
        if (!parentResult)
          return std::unexpected(std::move(parentResult.error()));
        auto parent = std::move(*parentResult);
        if (!parent) {
          auto windowsParentResult = request->name.contains("..\\");
          if (!windowsParentResult)
            return std::unexpected(std::move(windowsParentResult.error()));
          auto windowsParent = std::move(*windowsParentResult);
          parent = windowsParent;
        }
        if (parent) {
          auto operation81 = m_root.logger().error("Rejecting custom world name {}, path attempts to go up.", request->name);
          if (!operation81)
            return std::unexpected(std::move(operation81.error()));
          auto operation82 = m_connection->pushSingle(std::make_shared<ClientCustomWorldResponse>(request->name, WorldChunks()));
          if (!operation82)
            return std::unexpected(std::move(operation82.error()));
        } else {
          std::unique_lock locker(m_mutex, std::defer_lock);
          lockWithDiagnostics(locker);
          String filename = File::relativeTo(m_customWorldStorageDirectory, strf("{}.world", request->name));
          auto existsResult = File::exists(filename);
          if (!existsResult)
            return std::unexpected(std::move(existsResult.error()));
          auto exists = std::move(*existsResult);
          if (exists) {
            auto chunks = WorldStorage::getWorldChunksFromFile(m_root.logger(), filename);
            if (chunks) {
              auto operation83 = m_connection->pushSingle(std::make_shared<ClientCustomWorldResponse>(request->name, std::move(*chunks)));
              if (!operation83)
                return std::unexpected(std::move(operation83.error()));
            } else {
              auto operation84 = m_root.logger().errorException(strf("Failed to load custom world file {}", filename), ExceptionDiagnostic(chunks.error()));
              if (!operation84)
                return std::unexpected(std::move(operation84.error()));
              auto operation85 = m_connection->pushSingle(std::make_shared<ClientCustomWorldResponse>(request->name, WorldChunks()));
              if (!operation85)
                return std::unexpected(std::move(operation85.error()));
            }
          } else {
            auto operation86 = m_root.logger().error("Custom world file {} does not exist", filename);
            if (!operation86)
              return std::unexpected(std::move(operation86.error()));
            auto operation87 = m_connection->pushSingle(std::make_shared<ClientCustomWorldResponse>(request->name, WorldChunks()));
            if (!operation87)
              return std::unexpected(std::move(operation87.error()));
          }
        }
      } else if (auto loaded = as<NotifyWorldLoad>(packet)) {
        auto worldResult = printWorldId(loaded->worldId);
        if (!worldResult)
          return std::unexpected(std::move(worldResult.error()));
        auto world = std::move(*worldResult);
        if (std::holds_alternative<ClientCustomWorldId>(loaded->worldId)) {
          auto const& custom = std::get<ClientCustomWorldId>(loaded->worldId);
          if (custom.uuid == m_clientContext->playerUuid()) {
            for (auto& p : m_scriptContexts) {
              auto operation88 = p.second->invoke("clientCustomWorldLoaded", custom.name, world);
              if (!operation88)
                return std::unexpected(std::move(operation88.error()));
            }
          }
        } else if (std::holds_alternative<ClientShipWorldId>(loaded->worldId)) {
          if (std::get<ClientShipWorldId>(loaded->worldId) == m_clientContext->playerUuid()) {
            for (auto& p : m_scriptContexts) {
              auto operation89 = p.second->invoke("shipWorldLoaded", world);
              if (!operation89)
                return std::unexpected(std::move(operation89.error()));
            }
          }
        }
        for (auto& p : m_scriptContexts) {
          auto operation90 = p.second->invoke("serverWorldLoaded", world);
          if (!operation90)
            return std::unexpected(std::move(operation90.error()));
        }
      } else if (auto rejected = as<ClientSubWorldReject>(packet)) {
        auto operation91 = m_root.logger().warn("UniverseClient: Subworld {} rejected by server", rejected->subWorldId);
        if (!operation91)
          return std::unexpected(std::move(operation91.error()));
        if (m_subWorlds.hasLeftValue(rejected->subWorldId))
          m_subWorlds.removeLeft(rejected->subWorldId);
        if (m_subWorldThreads.contains(rejected->subWorldId)) {
          auto threadResult = m_subWorldThreads.get(rejected->subWorldId);
          if (!threadResult)
            return std::unexpected(std::move(threadResult.error()));
          auto thread = std::move(*threadResult);
          auto operation92 = thread.get()->clearMessages();
          if (!operation92)
            return std::unexpected(std::move(operation92.error()));
        }
        for (auto& p : m_scriptContexts) {
          auto operation93 = p.second->invoke("subWorldRejected", rejected->subWorldId);
          if (!operation93)
            return std::unexpected(std::move(operation93.error()));
        }
      } else if (auto subworld = as<ClientSubWorldPackets>(packet)) {
        if (m_subWorldThreads.contains(subworld->subWorldId)) {
          auto threadResult = m_subWorldThreads.get(subworld->subWorldId);
          if (!threadResult)
            return std::unexpected(std::move(threadResult.error()));
          auto thread = std::move(*threadResult);
          thread.get()->pushIncomingPackets(subworld->packets);
        } else {
          auto operation94 = m_root.logger().warn("UniverseClient: Received packets for non-existent subworld {}", subworld->subWorldId);
          if (!operation94)
            return std::unexpected(std::move(operation94.error()));
        }
      } else if (auto log = as<LogMapUpdate>(packet)) {
        for (auto& p : log->map)
          m_root.logMap().setValue(p.first, p.second);
      } else if (auto message = as<UniverseMessage>(packet)) {
        auto responseResult = receiveMessage(message->message, false, message->args);
        if (!responseResult)
          return std::unexpected(std::move(responseResult.error()));
        auto response = std::move(*responseResult);
        if (!response) {
          auto operation95 = m_connection->pushSingle(std::make_shared<UniverseMessageResponse>(
            std::unexpected(String("Message not handled by universe")), message->uuid));
          if (!operation95)
            return std::unexpected(std::move(operation95.error()));
        } else if (std::holds_alternative<Json>(*response)) {
          auto operation96 = m_connection->pushSingle(
            std::make_shared<UniverseMessageResponse>(std::get<Json>(*response), message->uuid));
          if (!operation96)
            return std::unexpected(std::move(operation96.error()));
        } else {
          m_universeMessagePromises[message->uuid] = std::get<RpcPromise<Json>>(*response);
        }
      } else if (auto message = as<UniverseMessageResponse>(packet)) {
        std::unique_lock locker(m_messageMutex, std::defer_lock);
        lockWithDiagnostics(locker);
        if (!m_universeMessageResponses.contains(message->uuid)) {
          auto operation97 = m_root.logger().warn("UniverseClient: UniverseMessageResponse received for unknown context [{}]!", message->uuid.hex());
          if (!operation97)
            return std::unexpected(std::move(operation97.error()));
        } else {
          auto keeperResult = m_universeMessageResponses.take(message->uuid);
          if (!keeperResult)
            return std::unexpected(std::move(keeperResult.error()));
          auto keeper = std::move(*keeperResult);
          if (message->response.has_value()) {
            auto operation98 = keeper.fulfill(*message->response);
            if (!operation98)
              return std::unexpected(std::move(operation98.error()));
          } else {
            auto operation99 = keeper.fail(message->response.error());
            if (!operation99)
              return std::unexpected(std::move(operation99.error()));
          }
        }
      } else {
        auto systemHandledResult = m_systemWorldClient->handleIncomingPacket(packet);
        if (!systemHandledResult)
          return std::unexpected(std::move(systemHandledResult.error()));
        auto systemHandled = std::move(*systemHandledResult);
        if (!systemHandled) {
          auto operation100 = m_worldClient->handleIncomingPackets({packet});
          if (!operation100)
            return std::unexpected(std::move(operation100.error()));
        }
      }
      return false;
    }();
    if (!handled) {
      return std::unexpected(handled.error().withContext(strf("Exception while handling {} packet", type.get())));
    }
    if (*handled)
      break;
  }
  return {};
}

std::expected<void, StarException> UniverseClient::reset() {
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (error)
        *error = error->withContext(strf("Additional universe cleanup failure: {}", result.error().what()));
      else
        error.emplace(std::move(result.error()));
    }
  };
  remember(stopLua());
  for (auto const& p : m_subWorldThreads) {
    remember(p.second->stop());
    remember(p.second->clearMessages());
    remember(p.second->executeAction([](WorldClientThread&, WorldClient& world) -> std::expected<void, StarException> { return world.close(); }));
  }
  m_subWorlds.clear();
  m_subWorldThreads.clear();
  if (m_worldClient)
    remember(m_worldClient->close());
  m_worldClient.reset();
  m_systemWorldClient.reset();
  m_universeClock.reset();
  m_celestialDatabase.reset();
  m_clientContext.reset();
  m_teamClient.reset();
  m_warping.reset();
  m_respawning = m_respawnWarped = false;
  auto timers = [&]() -> std::expected<void, StarException> {
    auto assetsResult = m_root.assets();
    if (!assetsResult)
      return std::unexpected(std::move(assetsResult.error()));
    auto assets = std::move(*assetsResult);
    auto delayJsonResult = assets->json("/client.config:playerWarpDelay");
    if (!delayJsonResult)
      return std::unexpected(std::move(delayJsonResult.error()));
    auto delayJson = std::move(*delayJsonResult);
    auto delayResult = delayJson.toFloat();
    if (!delayResult)
      return std::unexpected(std::move(delayResult.error()));
    auto delay = std::move(*delayResult);
    m_warpDelay = GameTimer(delay);
    auto reviveJsonResult = assets->json("/client.config:playerReviveTime");
    if (!reviveJsonResult)
      return std::unexpected(std::move(reviveJsonResult.error()));
    auto reviveJson = std::move(*reviveJsonResult);
    auto reviveResult = reviveJson.toFloat();
    if (!reviveResult)
      return std::unexpected(std::move(reviveResult.error()));
    auto revive = std::move(*reviveResult);
    m_respawnTimer = GameTimer(revive);
    return {};
  }();
  remember(std::move(timers));
  if (m_mainPlayer)
    remember(m_playerStorage->savePlayer(m_mainPlayer));
  if (m_connection)
    m_connection->close();
  m_connection.reset();
  std::unique_lock messageLocker(m_messageMutex, std::defer_lock);
  lockWithDiagnostics(messageLocker);
  for (auto& message : m_universeMessages) {
    if (std::holds_alternative<RpcPromiseKeeper<Json>>(message.keeper))
      remember(std::get<RpcPromiseKeeper<Json>>(message.keeper).fail("Universe disconnected"));
  }
  m_universeMessages.clear();
  for (auto& p : m_universeMessageResponses)
    remember(p.second.fail("Universe disconnected"));
  m_universeMessageResponses.clear();
  m_universeMessagePromises.clear();
  messageLocker.unlock();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> UniverseClient::passMessage(Universe::Message&& message) {
  if (!std::holds_alternative<RpcPromiseKeeper<Json>>(message.keeper))
    return std::unexpected(StarException("Universe client message requires a local response keeper"));
  std::unique_lock locker(m_messageMutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_universeMessages.append(std::move(message));
  return {};
}

std::expected<RpcPromise<Json>, StarException> UniverseClient::sendUniverseMessage(ConnectionId const& connectionId, String const& message, JsonArray const& args) {
  if (!m_clientContext)
    return std::unexpected(StarException("Universe client is not connected"));
  if (connectionId == m_clientContext->connectionId()) {
    auto pair = RpcPromise<Json>::createPair();
    auto operation37 = passMessage({message, args, pair.second});
    if (!operation37)
      return std::unexpected(std::move(operation37.error()));
    return pair.first;
  }
  auto versionResult = connectionVersion();
  if (!versionResult)
    return std::unexpected(std::move(versionResult.error()));
  auto version = std::move(*versionResult);
  if (version < 18)
    return RpcPromise<Json>::createFailed("Server is not new enough");
  auto pair = RpcPromise<Json>::createPair();
  Uuid uuid(m_root.random());
  std::unique_lock locker(m_messageMutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_universeMessageResponses[uuid] = pair.second;
  locker.unlock();
  auto operation38 = m_connection->pushSingle(std::make_shared<UniverseMessage>(connectionId, message, args, uuid));
  if (!operation38)
    return std::unexpected(std::move(operation38.error()));
  return pair.first;
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> UniverseClient::receiveMessage(String const& message, bool const& local, JsonArray const& args) {
  for (auto& p : m_scriptContexts) {
    auto responseResult = p.second->handleMessage(message, local, args);
    if (!responseResult)
      return std::unexpected(std::move(responseResult.error()));
    auto response = std::move(*responseResult);
    if (response)
      return response;
  }
  return {};
}

}
