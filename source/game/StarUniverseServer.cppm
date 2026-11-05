module;
import star.type_identity;
import star.xx_hash;
import star.json;
import star.id_map;
import star.format;
import star.string;
import star.lock_file;
import star.thread;
import star.game_types;
import star.vector;
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
import star.lua_root;
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

namespace Star {

class Clock;
class File;
class Player;
class ChatProcessor;
class CommandProcessor;
class TeamManager;
class WorldTemplate;
class CelestialMasterDatabase;
class WorldServer;
class UniverseSettings;
class ServerClientContext;
class Root;

struct UniverseServerExceptionTag {
  static constexpr std::string_view name() { return "UniverseServerException"; }
};
using UniverseServerException = StarError<UniverseServerExceptionTag, StarException>;

// Manages all running worlds, listens for new client connections and marshalls
// between all the different worlds and all the different client connections
// and routes packets between them.
class UniverseServer : public Universe, public Thread {
public:
  void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<UniverseServer, Universe, Thread>(this, key); }
  std::string_view dynamicTypeName() const noexcept override { return Star::typeName<UniverseServer>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<UniverseServer>, StarException> create(Root& root, String const& storageDir, bool const& isLocal = false);
  [[nodiscard]] std::expected<void, StarException> close();
  ~UniverseServer() override;

  Root& root() const;

  // If enabled, will listen on the configured server port for incoming
  // connections.
  void setListeningTcp(bool listenTcp);

  // Connects an arbitrary UniverseConnection to this server
  [[nodiscard]] std::expected<void, StarException> addClient(UniverseConnection remoteConnection);
  // Constructs an in-process connection to a UniverseServer for a
  // UniverseClient, and returns the other side of the connection.
  [[nodiscard]] std::expected<UniverseConnection, StarException> addLocalClient();

  // Signals the UniverseServer to stop and then joins the thread.
  [[nodiscard]] std::expected<void, StarException> stop();

  [[nodiscard]] std::expected<void, StarException> setPause(bool pause);
  [[nodiscard]] std::expected<void, StarException> setTimescale(float timescale);
  void setTickRate(float tickRate);

  [[nodiscard]] std::expected<List<WorldId>, StarException> activeWorlds() const;
  bool isWorldActive(WorldId const& worldId) const;

  List<ConnectionId> clientIds() const;
  [[nodiscard]] std::expected<List<std::pair<ConnectionId, std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>>>, StarException> clientIdsAndCreationTime() const;
  std::size_t numberOfClients() const;
  std::uint32_t maxClients() const;
  bool isConnectedClient(ConnectionId clientId) const;

  [[nodiscard]] std::expected<String, StarException> clientDescriptor(ConnectionId clientId) const;
  [[nodiscard]] std::expected<unsigned, StarException> clientConnectionVersion(ConnectionId clientId) const;

  [[nodiscard]] std::expected<String, StarException> clientNick(ConnectionId clientId) const;
  std::optional<ConnectionId> findNick(String const& nick) const;

  [[nodiscard]] std::expected<String, StarException> clientAccount(ConnectionId clientId) const;

  std::optional<Uuid> uuidForClient(ConnectionId clientId) const;
  std::optional<ConnectionId> clientForUuid(Uuid const& uuid) const;

  [[nodiscard]] std::expected<void, StarException> adminBroadcast(String const& text);
  [[nodiscard]] std::expected<void, StarException> adminWhisper(ConnectionId clientId, String const& text);
  [[nodiscard]] std::expected<ServerCommandResult, StarException> adminCommand(String text);

  [[nodiscard]] std::expected<bool, StarException> isAdmin(ConnectionId clientId) const;
  [[nodiscard]] std::expected<bool, StarException> canBecomeAdmin(ConnectionId clientId) const;
  [[nodiscard]] std::expected<void, StarException> setAdmin(ConnectionId clientId, bool admin);

  [[nodiscard]] std::expected<bool, StarException> serverDebug(ConnectionId clientId) const;
  [[nodiscard]] std::expected<void, StarException> setServerDebug(ConnectionId clientId, bool serverDebug);

  [[nodiscard]] std::expected<bool, StarException> isLocal(ConnectionId clientId) const;

  [[nodiscard]] std::expected<bool, StarException> isPvp(ConnectionId clientId) const;
  [[nodiscard]] std::expected<void, StarException> setPvp(ConnectionId clientId, bool pvp);

  [[nodiscard]] std::expected<RpcPromise<Json>, StarException> sendWorldMessage(WorldId const& worldId, String const& message, JsonArray const& args = {});

  [[nodiscard]] std::expected<void, StarException> clientWarpPlayer(ConnectionId clientId, WarpAction action, bool deploy = false);
  [[nodiscard]] std::expected<void, StarException> clientFlyShip(ConnectionId clientId, Vec3I const& system, SystemLocation const& location, Json const& settings = {});
  [[nodiscard]] std::expected<WorldId, StarException> clientWorld(ConnectionId clientId) const;
  [[nodiscard]] std::expected<CelestialCoordinate, StarException> clientShipCoordinate(ConnectionId clientId) const;

  std::shared_ptr<Clock> universeClock() const;
  std::shared_ptr<UniverseSettings> universeSettings() const;

  std::shared_ptr<CelestialDatabase> celestialDatabase() override;

  // If the client exists and is in a valid connection state, executes the
  // given function on the client world and player object in a thread safe way.
  // Returns true if function was called, false if client was not found or in
  // an invalid connection state.
  [[nodiscard]] std::expected<bool, StarException> executeForClient(ConnectionId clientId, std::function<std::expected<void, StarException>(WorldServer&, std::shared_ptr<Player>)> action);
  [[nodiscard]] std::expected<void, StarException> disconnectClient(ConnectionId clientId, String const& reason);
  [[nodiscard]] std::expected<void, StarException> banUser(ConnectionId clientId, String const& reason, std::pair<bool, bool> banType, std::optional<int> timeout);
  [[nodiscard]] std::expected<bool, StarException> unbanIp(String const& addressString);
  [[nodiscard]] std::expected<bool, StarException> unbanUuid(String const& uuidString);

  [[nodiscard]] std::expected<bool, StarException> updatePlanetType(CelestialCoordinate const& coordinate, String const& newType, String const& weatherBiome);

  [[nodiscard]] std::expected<bool, StarException> setWeather(CelestialCoordinate const& coordinate, String const& weatherName, bool force = false);

  [[nodiscard]] std::expected<StringList, StarException> weatherList(CelestialCoordinate const& coordinate);
  
  [[nodiscard]] std::expected<void, StarException> createCustomWorld(CustomWorldId const& customWorld, std::shared_ptr<WorldTemplate> worldTemplate);

  [[nodiscard]] std::expected<bool, StarException> sendPacket(ConnectionId clientId, std::shared_ptr<Packet> packet);
  
  [[nodiscard]] std::expected<void, StarException> passMessage(Universe::Message&& message) override;
  
  [[nodiscard]] std::expected<RpcPromise<Json>, StarException> sendUniverseMessage(ConnectionId const& connectionId, String const& message, JsonArray const& args = {}) override;
  
  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(String const& message, bool const& local, JsonArray const& args) override;

protected:
  [[nodiscard]] virtual std::expected<void, StarException> run() override;

private:
  explicit UniverseServer(Root& root);
  [[nodiscard]] std::expected<void, StarException> initialize(String const& storageDir, bool const& isLocal);
  [[nodiscard]] std::expected<void, StarException> runLoop(std::stop_token stopToken);
  [[nodiscard]] std::expected<void, StarException> shutdown();
  bool m_closed = false;
  bool m_initialized = false;
  bool shutdownThreadsJoined() const;

  struct TimeoutBan {
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> banExpiry;
    String reason;
    std::optional<HostAddress> ip;
    std::optional<Uuid> uuid;
  };
  
  struct WorldServerPromise {
    std::variant<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>, RpcPromise<WorldChunks>> currentPromise;
    std::optional<std::function<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>(WorldChunks)>> producer;
    WorldServerPromise(std::function<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>(WorldChunks)> producer, RpcPromise<WorldChunks> promise);
    WorldServerPromise(WorkerPoolPromise<std::shared_ptr<WorldServerThread>> promise);
    
    [[nodiscard]] std::expected<bool, StarException> done() const;
    [[nodiscard]] std::expected<bool, StarException> poll();
    
    [[nodiscard]] std::expected<std::shared_ptr<WorldServerThread>, StarException> get();
  };

  enum class TcpState : std::uint8_t { No, Yes, Fuck };

  [[nodiscard]] std::expected<void, StarException> processUniverseFlags();
  [[nodiscard]] std::expected<void, StarException> sendPendingChat();
  [[nodiscard]] std::expected<void, StarException> updateTeams();
  [[nodiscard]] std::expected<void, StarException> updateShips();
  [[nodiscard]] std::expected<void, StarException> sendClockUpdates();
  [[nodiscard]] std::expected<void, StarException> sendClientContextUpdate(std::shared_ptr<ServerClientContext> clientContext);
  [[nodiscard]] std::expected<void, StarException> sendClientContextUpdates();
  [[nodiscard]] std::expected<void, StarException> kickErroredPlayers();
  [[nodiscard]] std::expected<void, StarException> reapConnections();
  [[nodiscard]] std::expected<void, StarException> processPlanetTypeChanges();
  [[nodiscard]] std::expected<void, StarException> warpPlayers();
  [[nodiscard]] std::expected<void, StarException> flyShips();
  [[nodiscard]] std::expected<void, StarException> arriveShips();
  [[nodiscard]] std::expected<void, StarException> respondToCelestialRequests();
  [[nodiscard]] std::expected<void, StarException> processChat();
  [[nodiscard]] std::expected<void, StarException> clearBrokenWorlds();
  [[nodiscard]] std::expected<void, StarException> handleWorldMessages();
  [[nodiscard]] std::expected<void, StarException> handleUniverseMessages();
  [[nodiscard]] std::expected<void, StarException> shutdownInactiveWorlds();
  [[nodiscard]] std::expected<void, StarException> doTriggeredStorage();

  [[nodiscard]] std::expected<void, StarException> saveSettings();
  [[nodiscard]] std::expected<void, StarException> loadSettings();

  [[nodiscard]] std::expected<void, StarException> startLuaScripts();
  [[nodiscard]] std::expected<void, StarException> updateLua();
  [[nodiscard]] std::expected<void, StarException> stopLua();

  // Either returns the default configured starter world, or a new randomized
  // starter world, or if a randomized world is not yet available, starts a job
  // to find a randomized starter world and returns nothing until it is ready.
  [[nodiscard]] std::expected<std::optional<CelestialCoordinate>, StarException> nextStarterWorld();

  [[nodiscard]] std::expected<void, StarException> loadTempWorldIndex();
  [[nodiscard]] std::expected<void, StarException> saveTempWorldIndex();
  String tempWorldFile(InstanceWorldId const& worldId) const;

  [[nodiscard]] std::expected<std::optional<String>, StarException> isBannedUser(std::optional<HostAddress> hostAddress, Uuid playerUuid) const;
  [[nodiscard]] std::expected<void, StarException> doTempBan(ConnectionId clientId, String const& reason, std::pair<bool, bool> banType, int timeout);
  [[nodiscard]] std::expected<void, StarException> doPermBan(ConnectionId clientId, String const& reason, std::pair<bool, bool> banType);
  [[nodiscard]] std::expected<void, StarException> removeTimedBan();

  [[nodiscard]] std::expected<void, StarException> addCelestialRequests(ConnectionId clientId, List<CelestialRequest> requests);

  [[nodiscard]] std::expected<void, StarException> worldUpdated(WorldServerThread& worldServer);
  [[nodiscard]] std::expected<void, StarException> systemWorldUpdated(SystemWorldServerThread& systemWorldServer);
  [[nodiscard]] std::expected<void, StarException> packetsReceived(UniverseConnectionServer& connectionServer, ConnectionId clientId, List<std::shared_ptr<Packet>> packets);

  [[nodiscard]] std::expected<void, StarException> acceptConnection(UniverseConnection connection, std::optional<HostAddress> remoteAddress);

  // Main lock and clients read lock must be held when calling
  [[nodiscard]] std::expected<WarpToWorld, StarException> resolveWarpAction(WarpAction warpAction, ConnectionId clientId, bool deploy) const;
  [[nodiscard]] std::expected<bool, StarException> canWarpToShip(ConnectionId clientId, Uuid const& targetShipUuid) const;

  [[nodiscard]] std::expected<void, StarException> doDisconnection(ConnectionId clientId, String const& reason);

  // Clients read lock must be held when calling
  std::optional<ConnectionId> getClientForUuid(Uuid const& uuid) const;

  // Get the world only if it is already loaded, Main lock must be held when
  // calling.
  [[nodiscard]] std::expected<std::shared_ptr<WorldServerThread>, StarException> getWorld(WorldId const& worldId);

  // If the world is not created, block and load it, otherwise just return the
  // loaded world.  Main lock and Clients read lock must be held when calling.
  [[nodiscard]] std::expected<std::shared_ptr<WorldServerThread>, StarException> createWorld(WorldId const& worldId);

  // Trigger off-thread world creation, returns a value when the creation is
  // finished, either successfully or with an error.  Main lock and Clients
  // read lock must be held when calling.
  [[nodiscard]] std::expected<std::optional<std::shared_ptr<WorldServerThread>>, StarException> triggerWorldCreation(WorldId const& worldId);

  [[nodiscard]] std::expected<void, StarException> retireFailedWorldCreation(
      WorldId const& worldId, std::optional<WorldServerPromise>& promise, StarException const& error);

  // Main lock and clients read lock must be held when calling world promise
  // generators
  [[nodiscard]] std::expected<std::optional<WorldServerPromise>, StarException> makeWorldPromise(WorldId const& worldId);
  [[nodiscard]] std::expected<std::optional<WorldServerPromise>, StarException> shipWorldPromise(ClientShipWorldId const& uuid);
  [[nodiscard]] std::expected<std::optional<WorldServerPromise>, StarException> celestialWorldPromise(CelestialWorldId const& coordinate);
  [[nodiscard]] std::expected<std::optional<WorldServerPromise>, StarException> instanceWorldPromise(InstanceWorldId const& instanceWorld);
  [[nodiscard]] std::expected<std::optional<WorldServerPromise>, StarException> customWorldPromise(CustomWorldId const& customWorld, std::optional<std::shared_ptr<WorldTemplate>> worldTemplate = {});
  [[nodiscard]] std::expected<std::optional<WorldServerPromise>, StarException> clientCustomWorldPromise(ClientCustomWorldId const& clientCustomWorld, std::optional<std::shared_ptr<WorldTemplate>> worldTemplate = {});
  
  [[nodiscard]] std::expected<std::shared_ptr<WorldServerThread>, StarException> startWorldThread(std::shared_ptr<WorldServerThread> thread, WorldId const& worldId);

  [[nodiscard]] std::expected<void, StarException> notifyWorldCreated(WorldId const& worldId);

  // If the system world is not created, initialize it, otherwise return the
  // already initialized one
  [[nodiscard]] std::expected<std::shared_ptr<SystemWorldServerThread>, StarException> createSystemWorld(Vec3I const& location);

  [[nodiscard]] std::expected<bool, StarException> instanceWorldStoredOrActive(InstanceWorldId const& worldId) const;

  // Signal that a world either failed to load, or died due to an exception,
  // kicks clients if that world is a ship world.  Main lock and clients read
  // lock must be held when calling.
  [[nodiscard]] std::expected<void, StarException> worldDiedWithError(WorldId world);

  // Get SkyParameters if the coordinate is a valid world, and empty
  // SkyParameters otherwise.
  [[nodiscard]] std::expected<SkyParameters, StarException> celestialSkyParameters(CelestialCoordinate const& coordinate) const;

  Root& m_root;

  mutable std::recursive_mutex m_mainLock;
  
  bool m_isLocal;

  String m_storageDirectory;
  ByteArray m_assetsDigest;
  std::optional<LockFile> m_storageDirectoryLock;
  StringMap<StringList> m_speciesShips;
  std::shared_ptr<CelestialMasterDatabase> m_celestialDatabase;
  std::shared_ptr<Clock> m_universeClock;
  std::shared_ptr<UniverseSettings> m_universeSettings;
  WorkerPool m_workerPool;

  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_storageTriggerDeadline;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_clearBrokenWorldsDeadline;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_lastClockUpdateSent;
  // Acceptance can start before the native universe thread; shutdown is terminal.
  std::stop_source m_stopSource;
  std::atomic<TcpState> m_tcpState;

  mutable ReadersWriterMutex m_clientsLock;
  unsigned m_maxPlayers;
  IdMap<ConnectionId, std::shared_ptr<ServerClientContext>> m_clients;
  // Retain context/save/removal ownership after a disconnect error without
  // re-running disconnect scripts or treating it as an active service client.
  Set<ConnectionId> m_disconnectingClients;

  std::shared_ptr<std::atomic<bool>> m_pause;
  bool m_secureWarps;
  Map<WorldId, std::optional<WorldServerPromise>> m_worlds;
  // Stopped instances awaiting the existing warp/disconnect paths to deliver
  // their clients' final packets.  They must not block same-ID recreation.
  List<std::shared_ptr<WorldServerThread>> m_retiringWorlds;
  Map<InstanceWorldId, std::pair<std::int64_t, std::int64_t>> m_tempWorldIndex;
  Map<Vec3I, std::shared_ptr<SystemWorldServerThread>> m_systemWorlds;
  std::shared_ptr<UniverseConnectionServer> m_connectionServer;
  std::unique_ptr<TcpServer> m_tcpServer;

  std::recursive_mutex m_connectionAcceptThreadsMutex;
  List<ThreadFunction<void>> m_connectionAcceptThreads;
  LinkedList<std::pair<UniverseConnection, std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>>> m_deadConnections;

  std::shared_ptr<ChatProcessor> m_chatProcessor;
  std::shared_ptr<CommandProcessor> m_commandProcessor;
  std::shared_ptr<TeamManager> m_teamManager;

  HashMap<ConnectionId, std::pair<WarpAction, bool>> m_pendingPlayerWarps;
  HashMap<ConnectionId, HashMap<ClientSubWorldId, WorldId>> m_pendingSubWorlds;
  HashMap<ConnectionId, std::pair<std::tuple<Vec3I, SystemLocation, Json>, std::optional<double>>> m_queuedFlights;
  HashMap<ConnectionId, std::tuple<Vec3I, SystemLocation, Json>> m_pendingFlights;
  HashMap<ConnectionId, CelestialCoordinate> m_pendingArrivals;
  HashMap<ConnectionId, String> m_pendingDisconnections;
  HashMap<ConnectionId, List<WorkerPoolPromise<CelestialResponse>>> m_pendingCelestialRequests;
  List<std::pair<WorldId, UniverseFlagAction>> m_pendingFlagActions;
  HashMap<ConnectionId, List<std::tuple<String, ChatSendMode, JsonObject>>> m_pendingChat;
  std::optional<WorkerPoolPromise<CelestialCoordinate>> m_nextRandomizedStarterWorld;
  Map<WorldId, List<WorldServerThread::Message>> m_pendingWorldMessages;

  List<TimeoutBan> m_tempBans;

  std::shared_ptr<LuaRoot> m_luaRoot;

  typedef LuaMessageScript ScriptComponent;
  StringMap<std::shared_ptr<UniverseServer::ScriptComponent>> m_scriptContexts;

  mutable std::recursive_mutex m_messageLock;
  List<Universe::Message> m_universeMessages;
  
  HashMap<Uuid, std::pair<ConnectionId, std::variant<std::monostate, ConnectionId, RpcPromiseKeeper<Json>>>> m_universeMessageResponses;
  HashMap<Uuid, std::pair<ConnectionId, RpcPromise<Json>>> m_universeMessagePromises;
};

}


export module star.universe_server;

export namespace Star {
  using ::Star::Clock;
  using ::Star::File;
  using ::Star::Player;
  using ::Star::ChatProcessor;
  using ::Star::CommandProcessor;
  using ::Star::TeamManager;
  using ::Star::UniverseServer;
  using ::Star::WorldTemplate;
  using ::Star::CelestialMasterDatabase;
  using ::Star::WorldServer;
  using ::Star::UniverseSettings;
  using ::Star::ServerClientContext;
  using ::Star::UniverseServerExceptionTag;
  using ::Star::UniverseServerException;
}

