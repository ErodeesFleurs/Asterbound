module;
import star.xx_hash;
import star.json;
import star.id_map;
import star.data_stream;
import star.list;
import star.bi_map;
import star.multi_array;
import star.game_types;
import star.vector;
import star.string;
import star.map;
import star.format;
import star.version;
import star.ordered_map;
import star.color;
import star.random;
import star.weighted_pool;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.strong_typedef;
import std;
import star.type_identity;
import star.set;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.rpc_promise;

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

import star.byte_array;
namespace Star {

struct StarPacketExceptionTag {
  static constexpr std::string_view name() { return "StarPacketException"; }
};
using StarPacketException = StarError<StarPacketExceptionTag, IOException>;

extern VersionNumber const StarProtocolVersion;

// Packet types sent between the client and server over a NetSocket.  Does not
// correspond to actual packets, simply logical portions of NetSocket data.
enum class PacketType : std::uint8_t {
  // Packets used as part of the initial handshake
  ProtocolRequest,
  ProtocolResponse,

  // Packets sent universe server -> universe client
  ServerDisconnect,
  ConnectSuccess,
  ConnectFailure,
  HandshakeChallenge,
  ChatReceive,
  UniverseTimeUpdate,
  CelestialResponse,
  PlayerWarpResult,
  PlanetTypeUpdate,
  Pause,
  ServerInfo,

  // Packets sent universe client -> universe server
  ClientConnect,
  ClientDisconnectRequest,
  HandshakeResponse,
  PlayerWarp,
  FlyShip,
  ChatSend,
  CelestialRequest,

  // Packets sent bidirectionally between the universe client and the universe
  // server
  ClientContextUpdate,

  // Packets sent world server -> world client
  WorldStart,
  WorldStop,
  WorldLayoutUpdate,
  WorldParametersUpdate,
  CentralStructureUpdate,
  TileArrayUpdate,
  TileUpdate,
  TileLiquidUpdate,
  TileDamageUpdate,
  TileModificationFailure,
  GiveItem,
  EnvironmentUpdate,
  UpdateTileProtection,
  SetDungeonGravity,
  SetDungeonBreathable,
  SetPlayerStart,
  FindUniqueEntityResponse,
  Pong,

  // Packets sent world client -> world server
  ModifyTileList,
  DamageTileGroup,
  CollectLiquid,
  RequestDrop,
  SpawnEntity,
  ConnectWire,
  DisconnectAllWires,
  WorldClientStateUpdate,
  FindUniqueEntity,
  WorldStartAcknowledge,
  Ping,

  // Packets sent bidirectionally between world client and world server
  EntityCreate,
  EntityUpdateSet,
  EntityDestroy,
  EntityInteract,
  EntityInteractResult,
  HitRequest,
  DamageRequest,
  DamageNotification,
  EntityMessage,
  EntityMessageResponse,
  UpdateWorldProperties,
  StepUpdate,

  // Packets sent system server -> system client
  SystemWorldStart,
  SystemWorldUpdate,
  SystemObjectCreate,
  SystemObjectDestroy,
  SystemShipCreate,
  SystemShipDestroy,

  // Packets sent system client -> system server
  SystemObjectSpawn,

  // OpenStarbound packets
  ReplaceTileList,
  UpdateWorldTemplate,
  
  ClientCustomWorldRequest,
  ClientCustomWorldResponse,
  ClientCustomWorldCreate,
  
  ClientSubWorldPackets,
  ClientSubWorldRequest,
  ClientSubWorldReject,
  NotifyWorldLoad,
  
  LogMapUpdate,
  
  UniverseMessage,
  UniverseMessageResponse
};
[[nodiscard]] std::expected<std::string_view, StarException> packet_type_name(PacketType type);
[[nodiscard]] std::expected<PacketType, StarException> packet_type_from_name(StringView name);

enum class NetCompressionMode : std::uint8_t {
  None,
  Zstd
};
extern std::expected<EnumMap<NetCompressionMode>, StarException> const NetCompressionModeNames;

enum class PacketCompressionMode : std::uint8_t {
  Disabled,
  Automatic,
  Enabled
};

struct Packet {
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<Packet>(*this, query); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<Packet>(); }

  virtual ~Packet();

  virtual PacketType type() const = 0;

  [[nodiscard]] virtual std::expected<void, StarException> read(DataStream& ds);
  [[nodiscard]] virtual std::expected<void, StarException> write(DataStream& ds) const;

  [[nodiscard]] virtual std::expected<void, StarException> read_json(Json const& json);
  [[nodiscard]] virtual std::expected<Json, StarException> write_json() const;

  PacketCompressionMode compressionMode() const;
  void setCompressionMode(PacketCompressionMode compressionMode);

  PacketCompressionMode m_compressionMode = PacketCompressionMode::Automatic;
};

// Stages a packet for wire decoding; identity fields are initially unfilled.
[[nodiscard]] std::expected<std::shared_ptr<Packet>, StarException> createPacket(PacketType type);
// Constructs a raw outgoing packet, generating identity fields before JSON load.
[[nodiscard]] std::expected<std::shared_ptr<Packet>, StarException> createPacket(RandomService& random, PacketType type, std::optional<Json> const& args);

template <PacketType PacketT>
struct PacketBase : public Packet {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PacketBase<PacketT>, Packet>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PacketBase<PacketT>>(); }

  static PacketType const Type = PacketT;

  PacketType type() const override { return Type; }
};

struct ProtocolRequestPacket : PacketBase<PacketType::ProtocolRequest> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ProtocolRequestPacket, PacketBase<PacketType::ProtocolRequest>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ProtocolRequestPacket>(); }

  ProtocolRequestPacket();
  ProtocolRequestPacket(VersionNumber requestProtocolVersion);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  VersionNumber requestProtocolVersion;
};

struct ProtocolResponsePacket : PacketBase<PacketType::ProtocolResponse> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ProtocolResponsePacket, PacketBase<PacketType::ProtocolResponse>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ProtocolResponsePacket>(); }

  ProtocolResponsePacket();
  ProtocolResponsePacket(bool allowed, Json info = {});

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  bool allowed;
  Json info;
};

struct ServerDisconnectPacket : PacketBase<PacketType::ServerDisconnect> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ServerDisconnectPacket, PacketBase<PacketType::ServerDisconnect>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ServerDisconnectPacket>(); }

  ServerDisconnectPacket();
  ServerDisconnectPacket(String reason);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String reason;
};

struct ConnectSuccessPacket : PacketBase<PacketType::ConnectSuccess> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ConnectSuccessPacket, PacketBase<PacketType::ConnectSuccess>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ConnectSuccessPacket>(); }

  ConnectSuccessPacket();
  ConnectSuccessPacket(ConnectionId clientId, Uuid serverUuid, CelestialBaseInformation celestialInformation);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ConnectionId clientId;
  Uuid serverUuid;
  CelestialBaseInformation celestialInformation;
};

struct ConnectFailurePacket : PacketBase<PacketType::ConnectFailure> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ConnectFailurePacket, PacketBase<PacketType::ConnectFailure>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ConnectFailurePacket>(); }

  ConnectFailurePacket();
  ConnectFailurePacket(String reason);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String reason;
};

struct HandshakeChallengePacket : PacketBase<PacketType::HandshakeChallenge> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<HandshakeChallengePacket, PacketBase<PacketType::HandshakeChallenge>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<HandshakeChallengePacket>(); }

  HandshakeChallengePacket();
  HandshakeChallengePacket(ByteArray const& passwordSalt);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray passwordSalt;
};

struct ChatReceivePacket : PacketBase<PacketType::ChatReceive> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ChatReceivePacket, PacketBase<PacketType::ChatReceive>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ChatReceivePacket>(); }

  ChatReceivePacket();
  ChatReceivePacket(ChatReceivedMessage received_message);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  ChatReceivedMessage received_message;
};

struct UniverseTimeUpdatePacket : PacketBase<PacketType::UniverseTimeUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<UniverseTimeUpdatePacket, PacketBase<PacketType::UniverseTimeUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<UniverseTimeUpdatePacket>(); }

  UniverseTimeUpdatePacket();
  UniverseTimeUpdatePacket(double universeTime);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  double universeTime;
  float timescale;
};

struct CelestialResponsePacket : PacketBase<PacketType::CelestialResponse> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<CelestialResponsePacket, PacketBase<PacketType::CelestialResponse>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CelestialResponsePacket>(); }

  CelestialResponsePacket();
  CelestialResponsePacket(List<CelestialResponse> responses);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  List<CelestialResponse> responses;
};

struct PlayerWarpResultPacket : PacketBase<PacketType::PlayerWarpResult> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PlayerWarpResultPacket, PacketBase<PacketType::PlayerWarpResult>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PlayerWarpResultPacket>(); }

  PlayerWarpResultPacket();
  PlayerWarpResultPacket(bool success, WarpAction warpAction, bool warpActionInvalid);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  bool success;
  WarpAction warpAction;
  bool warpActionInvalid;
};

struct PlanetTypeUpdatePacket : PacketBase<PacketType::PlanetTypeUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PlanetTypeUpdatePacket, PacketBase<PacketType::PlanetTypeUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PlanetTypeUpdatePacket>(); }

  PlanetTypeUpdatePacket();
  PlanetTypeUpdatePacket(CelestialCoordinate coordinate);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  CelestialCoordinate coordinate;
};

struct PausePacket : PacketBase<PacketType::Pause> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PausePacket, PacketBase<PacketType::Pause>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PausePacket>(); }

  PausePacket();
  PausePacket(bool pause, float timescale = 1.0f);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  bool pause = false;
  float timescale = 1.0f;
};

struct ServerInfoPacket : PacketBase<PacketType::ServerInfo> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ServerInfoPacket, PacketBase<PacketType::ServerInfo>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ServerInfoPacket>(); }

  ServerInfoPacket();
  ServerInfoPacket(std::uint16_t players, std::uint16_t maxPlayers);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  std::uint16_t players;
  std::uint16_t maxPlayers;
};

struct ClientConnectPacket : PacketBase<PacketType::ClientConnect> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ClientConnectPacket, PacketBase<PacketType::ClientConnect>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ClientConnectPacket>(); }

  ClientConnectPacket();
  ClientConnectPacket(ByteArray assetsDigest, bool allowAssetsMismatch, Uuid playerUuid, String playerName,
      String shipSpecies, WorldChunks shipChunks, ShipUpgrades ship_upgrades, bool introComplete,
      String account, Json info = {});

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray assetsDigest;
  bool allowAssetsMismatch;
  Uuid playerUuid;
  String playerName;
  String shipSpecies;
  WorldChunks shipChunks;
  ShipUpgrades ship_upgrades;
  bool introComplete;
  String account;
  Json info;
};

struct ClientDisconnectRequestPacket : PacketBase<PacketType::ClientDisconnectRequest> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ClientDisconnectRequestPacket, PacketBase<PacketType::ClientDisconnectRequest>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override {
    return Star::type_name<ClientDisconnectRequestPacket>();
  }

  ClientDisconnectRequestPacket();

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;
};

struct HandshakeResponsePacket : PacketBase<PacketType::HandshakeResponse> {
  [[nodiscard]] static ByteArray password_digest(String const& password, String const& account, ByteArray const& salt);
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<HandshakeResponsePacket, PacketBase<PacketType::HandshakeResponse>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<HandshakeResponsePacket>(); }

  HandshakeResponsePacket();
  HandshakeResponsePacket(ByteArray const& passHash);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray passHash;
};

struct PlayerWarpPacket : PacketBase<PacketType::PlayerWarp> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PlayerWarpPacket, PacketBase<PacketType::PlayerWarp>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PlayerWarpPacket>(); }

  PlayerWarpPacket();
  PlayerWarpPacket(WarpAction action, bool deploy);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  WarpAction action;
  bool deploy;
};

struct FlyShipPacket : PacketBase<PacketType::FlyShip> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<FlyShipPacket, PacketBase<PacketType::FlyShip>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<FlyShipPacket>(); }

  FlyShipPacket();
  FlyShipPacket(Vec3I system, SystemLocation location, Json settings = {});

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec3I system;
  SystemLocation location;
  Json settings;
};

struct ChatSendPacket : PacketBase<PacketType::ChatSend> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ChatSendPacket, PacketBase<PacketType::ChatSend>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ChatSendPacket>(); }

  ChatSendPacket();
  ChatSendPacket(String text, ChatSendMode send_mode);
  ChatSendPacket(String text, ChatSendMode send_mode, JsonObject data);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String text;
  ChatSendMode send_mode;
  JsonObject data;
};

struct CelestialRequestPacket : PacketBase<PacketType::CelestialRequest> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<CelestialRequestPacket, PacketBase<PacketType::CelestialRequest>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CelestialRequestPacket>(); }

  CelestialRequestPacket();
  CelestialRequestPacket(List<CelestialRequest> requests);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  List<CelestialRequest> requests;
};

struct ClientContextUpdatePacket : PacketBase<PacketType::ClientContextUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ClientContextUpdatePacket, PacketBase<PacketType::ClientContextUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ClientContextUpdatePacket>(); }

  ClientContextUpdatePacket();
  ClientContextUpdatePacket(ByteArray update_data);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray update_data;
};

// Sent when a client should initialize themselves on a new world
struct WorldStartPacket : PacketBase<PacketType::WorldStart> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<WorldStartPacket, PacketBase<PacketType::WorldStart>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<WorldStartPacket>(); }

  WorldStartPacket();

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Json templateData;
  ByteArray skyData;
  ByteArray weatherData;
  Vec2F playerStart;
  Vec2F playerRespawn;
  bool respawnInWorld;
  HashMap<DungeonId, float> dungeonIdGravity;
  HashMap<DungeonId, bool> dungeonIdBreathable;
  StableHashSet<DungeonId> protectedDungeonIds;
  Json worldProperties;
  ConnectionId clientId;
  bool local_interpolation_mode;
};

// Sent when a client is leaving a world
struct WorldStopPacket : PacketBase<PacketType::WorldStop> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<WorldStopPacket, PacketBase<PacketType::WorldStop>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<WorldStopPacket>(); }

  WorldStopPacket();
  WorldStopPacket(String const& reason);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String reason;
};

// Sent when the region data for the client's current world changes
struct WorldLayoutUpdatePacket : PacketBase<PacketType::WorldLayoutUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<WorldLayoutUpdatePacket, PacketBase<PacketType::WorldLayoutUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<WorldLayoutUpdatePacket>(); }

  WorldLayoutUpdatePacket();
  WorldLayoutUpdatePacket(Json const& layoutData);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Json layoutData;
};

// Sent when the environment status effect list for the client's current world changes
struct WorldParametersUpdatePacket : PacketBase<PacketType::WorldParametersUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<WorldParametersUpdatePacket, PacketBase<PacketType::WorldParametersUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override {
    return Star::type_name<WorldParametersUpdatePacket>();
  }

  WorldParametersUpdatePacket();
  WorldParametersUpdatePacket(ByteArray parametersData);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray parametersData;
};

struct CentralStructureUpdatePacket : PacketBase<PacketType::CentralStructureUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<CentralStructureUpdatePacket, PacketBase<PacketType::CentralStructureUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override {
    return Star::type_name<CentralStructureUpdatePacket>();
  }

  CentralStructureUpdatePacket();
  CentralStructureUpdatePacket(Json structureData);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Json structureData;
};

struct TileArrayUpdatePacket : PacketBase<PacketType::TileArrayUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<TileArrayUpdatePacket, PacketBase<PacketType::TileArrayUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<TileArrayUpdatePacket>(); }

  using TileArray = MultiArray<NetTile, 2>;

  TileArrayUpdatePacket();

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec2I min;
  TileArray array;
};

struct TileUpdatePacket : PacketBase<PacketType::TileUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<TileUpdatePacket, PacketBase<PacketType::TileUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<TileUpdatePacket>(); }

  TileUpdatePacket() {}
  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec2I position;
  NetTile tile;
};

struct TileLiquidUpdatePacket : PacketBase<PacketType::TileLiquidUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<TileLiquidUpdatePacket, PacketBase<PacketType::TileLiquidUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<TileLiquidUpdatePacket>(); }

  TileLiquidUpdatePacket();
  TileLiquidUpdatePacket(Vec2I const& position, LiquidNetUpdate liquidUpdate);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec2I position;
  LiquidNetUpdate liquidUpdate;
};

struct TileDamageUpdatePacket : PacketBase<PacketType::TileDamageUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<TileDamageUpdatePacket, PacketBase<PacketType::TileDamageUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<TileDamageUpdatePacket>(); }

  TileDamageUpdatePacket();
  TileDamageUpdatePacket(Vec2I const& position, TileLayer layer, TileDamageStatus const& tileDamage);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec2I position;
  TileLayer layer;

  TileDamageStatus tileDamage;
};

struct TileModificationFailurePacket : PacketBase<PacketType::TileModificationFailure> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<TileModificationFailurePacket, PacketBase<PacketType::TileModificationFailure>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override {
    return Star::type_name<TileModificationFailurePacket>();
  }

  TileModificationFailurePacket();
  TileModificationFailurePacket(TileModificationList modifications);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  TileModificationList modifications;
};

struct GiveItemPacket : PacketBase<PacketType::GiveItem> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<GiveItemPacket, PacketBase<PacketType::GiveItem>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<GiveItemPacket>(); }

  GiveItemPacket();
  GiveItemPacket(ItemDescriptor const& item);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  ItemDescriptor item;
};

struct EnvironmentUpdatePacket : PacketBase<PacketType::EnvironmentUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<EnvironmentUpdatePacket, PacketBase<PacketType::EnvironmentUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<EnvironmentUpdatePacket>(); }

  EnvironmentUpdatePacket();
  EnvironmentUpdatePacket(ByteArray skyDelta, ByteArray weatherDelta);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray skyDelta;
  ByteArray weatherDelta;
};

struct UpdateTileProtectionPacket : PacketBase<PacketType::UpdateTileProtection> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<UpdateTileProtectionPacket, PacketBase<PacketType::UpdateTileProtection>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<UpdateTileProtectionPacket>(); }

  UpdateTileProtectionPacket();
  UpdateTileProtectionPacket(DungeonId dungeonId, bool isProtected);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  DungeonId dungeonId;
  bool isProtected;
};

struct SetDungeonGravityPacket : PacketBase<PacketType::SetDungeonGravity> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<SetDungeonGravityPacket, PacketBase<PacketType::SetDungeonGravity>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SetDungeonGravityPacket>(); }

  SetDungeonGravityPacket();
  SetDungeonGravityPacket(DungeonId dungeonId, std::optional<float> gravity);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  DungeonId dungeonId;
  std::optional<float> gravity;
};

struct SetDungeonBreathablePacket : PacketBase<PacketType::SetDungeonBreathable> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<SetDungeonBreathablePacket, PacketBase<PacketType::SetDungeonBreathable>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SetDungeonBreathablePacket>(); }

  SetDungeonBreathablePacket();
  SetDungeonBreathablePacket(DungeonId dungeonId, std::optional<bool> breathable);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  DungeonId dungeonId;
  std::optional<bool> breathable;
};

struct SetPlayerStartPacket : PacketBase<PacketType::SetPlayerStart> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<SetPlayerStartPacket, PacketBase<PacketType::SetPlayerStart>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SetPlayerStartPacket>(); }

  SetPlayerStartPacket();
  SetPlayerStartPacket(Vec2F playerStart, bool respawnInWorld);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  Vec2F playerStart;
  bool respawnInWorld;
};

struct FindUniqueEntityResponsePacket : PacketBase<PacketType::FindUniqueEntityResponse> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<FindUniqueEntityResponsePacket, PacketBase<PacketType::FindUniqueEntityResponse>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override {
    return Star::type_name<FindUniqueEntityResponsePacket>();
  }

  FindUniqueEntityResponsePacket();
  FindUniqueEntityResponsePacket(String uniqueEntityId, std::optional<Vec2F> entityPosition);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String uniqueEntityId;
  std::optional<Vec2F> entityPosition;
};

struct PongPacket : PacketBase<PacketType::Pong> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PongPacket, PacketBase<PacketType::Pong>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PongPacket>(); }

  PongPacket();
  PongPacket(std::int64_t time);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  std::int64_t time = 0;
};

struct ModifyTileListPacket : PacketBase<PacketType::ModifyTileList> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ModifyTileListPacket, PacketBase<PacketType::ModifyTileList>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ModifyTileListPacket>(); }

  ModifyTileListPacket();
  ModifyTileListPacket(TileModificationList modifications, bool allowEntityOverlap);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  TileModificationList modifications;
  bool allowEntityOverlap;
};

struct ReplaceTileListPacket : PacketBase<PacketType::ReplaceTileList> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ReplaceTileListPacket, PacketBase<PacketType::ReplaceTileList>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ReplaceTileListPacket>(); }

  ReplaceTileListPacket();
  ReplaceTileListPacket(TileModificationList modifications, TileDamage tileDamage, bool applyDamage);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  TileModificationList modifications;
  TileDamage tileDamage;
  bool applyDamage;
};

struct DamageTileGroupPacket : PacketBase<PacketType::DamageTileGroup> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<DamageTileGroupPacket, PacketBase<PacketType::DamageTileGroup>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<DamageTileGroupPacket>(); }

  DamageTileGroupPacket();
  DamageTileGroupPacket(List<Vec2I> tilePositions, TileLayer layer, Vec2F sourcePosition, TileDamage tileDamage, std::optional<EntityId> sourceEntity);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  List<Vec2I> tilePositions;
  TileLayer layer;
  Vec2F sourcePosition;
  TileDamage tileDamage;
  std::optional<EntityId> sourceEntity;
};

struct CollectLiquidPacket : PacketBase<PacketType::CollectLiquid> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<CollectLiquidPacket, PacketBase<PacketType::CollectLiquid>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CollectLiquidPacket>(); }

  CollectLiquidPacket();
  CollectLiquidPacket(List<Vec2I> tilePositions, LiquidId liquidId);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  List<Vec2I> tilePositions;
  LiquidId liquidId;
};

struct RequestDropPacket : PacketBase<PacketType::RequestDrop> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<RequestDropPacket, PacketBase<PacketType::RequestDrop>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<RequestDropPacket>(); }

  RequestDropPacket();
  RequestDropPacket(EntityId dropEntityId);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  EntityId dropEntityId;
};

struct SpawnEntityPacket : PacketBase<PacketType::SpawnEntity> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<SpawnEntityPacket, PacketBase<PacketType::SpawnEntity>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SpawnEntityPacket>(); }

  SpawnEntityPacket();
  SpawnEntityPacket(EntityType entityType, ByteArray storeData, ByteArray firstNetState);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  EntityType entityType;
  ByteArray storeData;
  ByteArray firstNetState;
};

struct ConnectWirePacket : PacketBase<PacketType::ConnectWire> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ConnectWirePacket, PacketBase<PacketType::ConnectWire>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ConnectWirePacket>(); }

  ConnectWirePacket();
  ConnectWirePacket(WireConnection outputConnection, WireConnection inputConnection);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  WireConnection outputConnection;
  WireConnection inputConnection;
};

struct DisconnectAllWiresPacket : PacketBase<PacketType::DisconnectAllWires> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<DisconnectAllWiresPacket, PacketBase<PacketType::DisconnectAllWires>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<DisconnectAllWiresPacket>(); }

  DisconnectAllWiresPacket();
  DisconnectAllWiresPacket(Vec2I entityPosition, WireNode wireNode);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec2I entityPosition;
  WireNode wireNode;
};

struct WorldClientStateUpdatePacket : PacketBase<PacketType::WorldClientStateUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<WorldClientStateUpdatePacket, PacketBase<PacketType::WorldClientStateUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override {
    return Star::type_name<WorldClientStateUpdatePacket>();
  }

  WorldClientStateUpdatePacket();
  WorldClientStateUpdatePacket(ByteArray worldClientStateDelta);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray worldClientStateDelta;
};

struct FindUniqueEntityPacket : PacketBase<PacketType::FindUniqueEntity> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<FindUniqueEntityPacket, PacketBase<PacketType::FindUniqueEntity>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<FindUniqueEntityPacket>(); }

  FindUniqueEntityPacket();
  FindUniqueEntityPacket(String uniqueEntityId);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String uniqueEntityId;
};

struct WorldStartAcknowledgePacket : PacketBase<PacketType::WorldStartAcknowledge> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<WorldStartAcknowledgePacket, PacketBase<PacketType::WorldStartAcknowledge>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override {
    return Star::type_name<WorldStartAcknowledgePacket>();
  }

  WorldStartAcknowledgePacket();

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;
};

struct PingPacket : PacketBase<PacketType::Ping> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PingPacket, PacketBase<PacketType::Ping>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PingPacket>(); }

  PingPacket();
  PingPacket(std::int64_t time);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  std::int64_t time = 0;
};

struct EntityCreatePacket : PacketBase<PacketType::EntityCreate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<EntityCreatePacket, PacketBase<PacketType::EntityCreate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<EntityCreatePacket>(); }

  EntityCreatePacket();
  EntityCreatePacket(EntityType entityType, ByteArray storeData, ByteArray firstNetState, EntityId entityId);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  EntityType entityType;
  ByteArray storeData;
  ByteArray firstNetState;
  EntityId entityId;
};

// All entity deltas will be sent at the same time for the same connection
// where they are master, any entities whose master is from that connection can
// be assumed to have produced a blank delta.
struct EntityUpdateSetPacket : PacketBase<PacketType::EntityUpdateSet> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<EntityUpdateSetPacket, PacketBase<PacketType::EntityUpdateSet>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<EntityUpdateSetPacket>(); }

  EntityUpdateSetPacket();
  EntityUpdateSetPacket(ConnectionId forConnection);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ConnectionId forConnection;
  HashMap<EntityId, ByteArray> deltas;
};

struct EntityDestroyPacket : PacketBase<PacketType::EntityDestroy> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<EntityDestroyPacket, PacketBase<PacketType::EntityDestroy>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<EntityDestroyPacket>(); }

  EntityDestroyPacket();
  EntityDestroyPacket(EntityId entityId, ByteArray finalNetState, bool death);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  EntityId entityId;
  ByteArray finalNetState;
  // If true, the entity removal is due to death rather simply for example
  // going out of range of the entity monitoring window.
  bool death;
};

struct EntityInteractPacket : PacketBase<PacketType::EntityInteract> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<EntityInteractPacket, PacketBase<PacketType::EntityInteract>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<EntityInteractPacket>(); }

  EntityInteractPacket() = default;
  EntityInteractPacket(InteractRequest interact_request, Uuid request_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  InteractRequest interact_request;
  Uuid request_id = Uuid::nil();
};

struct EntityInteractResultPacket : PacketBase<PacketType::EntityInteractResult> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<EntityInteractResultPacket, PacketBase<PacketType::EntityInteractResult>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<EntityInteractResultPacket>(); }

  EntityInteractResultPacket() = default;
  EntityInteractResultPacket(InteractAction action, Uuid request_id, EntityId source_entity_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  InteractAction action;
  Uuid request_id = Uuid::nil();
  EntityId source_entity_id = null_entity_id;
};

struct HitRequestPacket : PacketBase<PacketType::HitRequest> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<HitRequestPacket, PacketBase<PacketType::HitRequest>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<HitRequestPacket>(); }

  HitRequestPacket();
  HitRequestPacket(RemoteHitRequest remoteHitRequest);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  RemoteHitRequest remoteHitRequest;
};

struct DamageRequestPacket : PacketBase<PacketType::DamageRequest> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<DamageRequestPacket, PacketBase<PacketType::DamageRequest>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<DamageRequestPacket>(); }

  DamageRequestPacket();
  DamageRequestPacket(RemoteDamageRequest remoteDamageRequest);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  RemoteDamageRequest remoteDamageRequest;
};

struct DamageNotificationPacket : PacketBase<PacketType::DamageNotification> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<DamageNotificationPacket, PacketBase<PacketType::DamageNotification>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<DamageNotificationPacket>(); }

  DamageNotificationPacket();
  DamageNotificationPacket(RemoteDamageNotification remoteDamageNotification);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  RemoteDamageNotification remoteDamageNotification;
};

struct EntityMessagePacket : PacketBase<PacketType::EntityMessage> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<EntityMessagePacket, PacketBase<PacketType::EntityMessage>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<EntityMessagePacket>(); }

  EntityMessagePacket();
  EntityMessagePacket(std::variant<EntityId, String> entityId, String message, JsonArray args, Uuid uuid, ConnectionId fromConnection = server_connection_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  std::variant<EntityId, String> entityId;
  String message;
  JsonArray args;
  Uuid uuid;
  ConnectionId fromConnection;
};

struct EntityMessageResponsePacket : PacketBase<PacketType::EntityMessageResponse> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<EntityMessageResponsePacket, PacketBase<PacketType::EntityMessageResponse>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override {
    return Star::type_name<EntityMessageResponsePacket>();
  }

  EntityMessageResponsePacket();
  EntityMessageResponsePacket(std::expected<Json, String> response, Uuid uuid);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  std::expected<Json, String> response;
  Uuid uuid;
};

struct UpdateWorldPropertiesPacket : PacketBase<PacketType::UpdateWorldProperties> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<UpdateWorldPropertiesPacket, PacketBase<PacketType::UpdateWorldProperties>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override {
    return Star::type_name<UpdateWorldPropertiesPacket>();
  }

  UpdateWorldPropertiesPacket();
  UpdateWorldPropertiesPacket(JsonObject const& updatedProperties);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  JsonObject updatedProperties;
};

struct StepUpdatePacket : PacketBase<PacketType::StepUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<StepUpdatePacket, PacketBase<PacketType::StepUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<StepUpdatePacket>(); }

  StepUpdatePacket() = default;
  StepUpdatePacket(double remote_time);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  double remote_time = 0.0;
};

struct SystemWorldStartPacket : PacketBase<PacketType::SystemWorldStart> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<SystemWorldStartPacket, PacketBase<PacketType::SystemWorldStart>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SystemWorldStartPacket>(); }

  SystemWorldStartPacket();
  SystemWorldStartPacket(Vec3I location, List<ByteArray> objectStores, List<ByteArray> shipStores, std::pair<Uuid, SystemLocation> clientShip);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec3I location;
  List<ByteArray> objectStores;
  List<ByteArray> shipStores;
  std::pair<Uuid, SystemLocation> clientShip;
};

struct SystemWorldUpdatePacket : PacketBase<PacketType::SystemWorldUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<SystemWorldUpdatePacket, PacketBase<PacketType::SystemWorldUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SystemWorldUpdatePacket>(); }

  SystemWorldUpdatePacket();
  SystemWorldUpdatePacket(HashMap<Uuid, ByteArray> objectUpdates, HashMap<Uuid, ByteArray> shipUpdates);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  HashMap<Uuid, ByteArray> objectUpdates;
  HashMap<Uuid, ByteArray> shipUpdates;
};

struct SystemObjectCreatePacket : PacketBase<PacketType::SystemObjectCreate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<SystemObjectCreatePacket, PacketBase<PacketType::SystemObjectCreate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SystemObjectCreatePacket>(); }

  SystemObjectCreatePacket();
  SystemObjectCreatePacket(ByteArray objectStore);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray objectStore;
};

struct SystemObjectDestroyPacket : PacketBase<PacketType::SystemObjectDestroy> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<SystemObjectDestroyPacket, PacketBase<PacketType::SystemObjectDestroy>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SystemObjectDestroyPacket>(); }

  SystemObjectDestroyPacket();
  SystemObjectDestroyPacket(Uuid objectUuid);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Uuid objectUuid;
};

struct SystemShipCreatePacket : PacketBase<PacketType::SystemShipCreate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<SystemShipCreatePacket, PacketBase<PacketType::SystemShipCreate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SystemShipCreatePacket>(); }

  SystemShipCreatePacket();
  SystemShipCreatePacket(ByteArray shipStore);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray shipStore;
};

struct SystemShipDestroyPacket : PacketBase<PacketType::SystemShipDestroy> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<SystemShipDestroyPacket, PacketBase<PacketType::SystemShipDestroy>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SystemShipDestroyPacket>(); }

  SystemShipDestroyPacket();
  SystemShipDestroyPacket(Uuid shipUuid);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Uuid shipUuid;
};

struct SystemObjectSpawnPacket : PacketBase<PacketType::SystemObjectSpawn> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<SystemObjectSpawnPacket, PacketBase<PacketType::SystemObjectSpawn>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SystemObjectSpawnPacket>(); }

  SystemObjectSpawnPacket();
  SystemObjectSpawnPacket(String typeName, Uuid uuid, std::optional<Vec2F> position, JsonObject parameters);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String typeName;
  Uuid uuid;
  std::optional<Vec2F> position;
  JsonObject parameters;
};

struct UpdateWorldTemplatePacket : PacketBase<PacketType::UpdateWorldTemplate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<UpdateWorldTemplatePacket, PacketBase<PacketType::UpdateWorldTemplate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<UpdateWorldTemplatePacket>(); }

  UpdateWorldTemplatePacket();
  UpdateWorldTemplatePacket(Json templateData);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Json templateData;
};

struct ClientCustomWorldRequest : PacketBase<PacketType::ClientCustomWorldRequest> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ClientCustomWorldRequest, PacketBase<PacketType::ClientCustomWorldRequest>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ClientCustomWorldRequest>(); }

  ClientCustomWorldRequest();
  ClientCustomWorldRequest(String name);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String name;
};

struct ClientCustomWorldResponse : PacketBase<PacketType::ClientCustomWorldResponse> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ClientCustomWorldResponse, PacketBase<PacketType::ClientCustomWorldResponse>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ClientCustomWorldResponse>(); }

  ClientCustomWorldResponse();
  ClientCustomWorldResponse(String name, WorldChunks chunks);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String name;
  WorldChunks chunks;
};

struct ClientCustomWorldCreate : PacketBase<PacketType::ClientCustomWorldCreate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ClientCustomWorldCreate, PacketBase<PacketType::ClientCustomWorldCreate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ClientCustomWorldCreate>(); }

  ClientCustomWorldCreate();
  ClientCustomWorldCreate(String name, Json templateData);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String name;
  Json templateData;
};

// wrapper packet for client world threads
struct ClientSubWorldPackets : PacketBase<PacketType::ClientSubWorldPackets> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ClientSubWorldPackets, PacketBase<PacketType::ClientSubWorldPackets>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ClientSubWorldPackets>(); }

  ClientSubWorldPackets();
  ClientSubWorldPackets(ClientSubWorldId const& subWorldId, List<std::shared_ptr<Packet>> packets);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ClientSubWorldId subWorldId;
  List<std::shared_ptr<Packet>> packets;
};

struct ClientSubWorldRequest : PacketBase<PacketType::ClientSubWorldRequest> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ClientSubWorldRequest, PacketBase<PacketType::ClientSubWorldRequest>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ClientSubWorldRequest>(); }

  ClientSubWorldRequest();
  ClientSubWorldRequest(ClientSubWorldId const& subWorldId, WorldId worldId);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ClientSubWorldId subWorldId;
  WorldId worldId;
};

struct ClientSubWorldReject : PacketBase<PacketType::ClientSubWorldReject> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ClientSubWorldReject, PacketBase<PacketType::ClientSubWorldReject>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ClientSubWorldReject>(); }

  ClientSubWorldReject();
  ClientSubWorldReject(ClientSubWorldId const& subWorldId);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ClientSubWorldId subWorldId;
};

struct NotifyWorldLoad : PacketBase<PacketType::NotifyWorldLoad> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<NotifyWorldLoad, PacketBase<PacketType::NotifyWorldLoad>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<NotifyWorldLoad>(); }

  NotifyWorldLoad();
  NotifyWorldLoad(WorldId worldId);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  WorldId worldId;
};

struct LogMapUpdate : PacketBase<PacketType::LogMapUpdate> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<LogMapUpdate, PacketBase<PacketType::LogMapUpdate>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<LogMapUpdate>(); }

  LogMapUpdate();
  LogMapUpdate(Map<String,String> map);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Map<String,String> map;
};

struct UniverseMessage : PacketBase<PacketType::UniverseMessage> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<UniverseMessage, PacketBase<PacketType::UniverseMessage>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<UniverseMessage>(); }

  UniverseMessage();
  UniverseMessage(ConnectionId connection, String message, JsonArray args, Uuid uuid, ConnectionId fromConnection = server_connection_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  ConnectionId connection;
  String message;
  JsonArray args;
  Uuid uuid;
  ConnectionId fromConnection;
};

struct UniverseMessageResponse : PacketBase<PacketType::UniverseMessageResponse> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<UniverseMessageResponse, PacketBase<PacketType::UniverseMessageResponse>>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<UniverseMessageResponse>(); }

  UniverseMessageResponse();
  UniverseMessageResponse(std::expected<Json, String> response, Uuid uuid);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  std::expected<Json, String> response;
  Uuid uuid;
};
}

export module star.net_packets;

export namespace Star {
  using ::Star::Packet;
  using ::Star::StarPacketExceptionTag;
  using ::Star::StarPacketException;
  using ::Star::StarProtocolVersion;
  using ::Star::PacketType;
  using ::Star::packet_type_name;
  using ::Star::packet_type_from_name;
  using ::Star::NetCompressionMode;
  using ::Star::NetCompressionModeNames;
  using ::Star::PacketCompressionMode;
  using ::Star::createPacket;
  using ::Star::PacketBase;
  using ::Star::ProtocolRequestPacket;
  using ::Star::ProtocolResponsePacket;
  using ::Star::ServerDisconnectPacket;
  using ::Star::ConnectSuccessPacket;
  using ::Star::ConnectFailurePacket;
  using ::Star::HandshakeChallengePacket;
  using ::Star::ChatReceivePacket;
  using ::Star::UniverseTimeUpdatePacket;
  using ::Star::CelestialResponsePacket;
  using ::Star::PlayerWarpResultPacket;
  using ::Star::PlanetTypeUpdatePacket;
  using ::Star::PausePacket;
  using ::Star::ServerInfoPacket;
  using ::Star::ClientConnectPacket;
  using ::Star::ClientDisconnectRequestPacket;
  using ::Star::HandshakeResponsePacket;
  using ::Star::PlayerWarpPacket;
  using ::Star::FlyShipPacket;
  using ::Star::ChatSendPacket;
  using ::Star::CelestialRequestPacket;
  using ::Star::ClientContextUpdatePacket;
  using ::Star::WorldStartPacket;
  using ::Star::WorldStopPacket;
  using ::Star::WorldLayoutUpdatePacket;
  using ::Star::WorldParametersUpdatePacket;
  using ::Star::CentralStructureUpdatePacket;
  using ::Star::TileArrayUpdatePacket;
  using ::Star::TileUpdatePacket;
  using ::Star::TileLiquidUpdatePacket;
  using ::Star::TileDamageUpdatePacket;
  using ::Star::TileModificationFailurePacket;
  using ::Star::GiveItemPacket;
  using ::Star::EnvironmentUpdatePacket;
  using ::Star::UpdateTileProtectionPacket;
  using ::Star::SetDungeonGravityPacket;
  using ::Star::SetDungeonBreathablePacket;
  using ::Star::SetPlayerStartPacket;
  using ::Star::FindUniqueEntityResponsePacket;
  using ::Star::PongPacket;
  using ::Star::ModifyTileListPacket;
  using ::Star::ReplaceTileListPacket;
  using ::Star::DamageTileGroupPacket;
  using ::Star::CollectLiquidPacket;
  using ::Star::RequestDropPacket;
  using ::Star::SpawnEntityPacket;
  using ::Star::ConnectWirePacket;
  using ::Star::DisconnectAllWiresPacket;
  using ::Star::WorldClientStateUpdatePacket;
  using ::Star::FindUniqueEntityPacket;
  using ::Star::WorldStartAcknowledgePacket;
  using ::Star::PingPacket;
  using ::Star::EntityCreatePacket;
  using ::Star::EntityUpdateSetPacket;
  using ::Star::EntityDestroyPacket;
  using ::Star::EntityInteractPacket;
  using ::Star::EntityInteractResultPacket;
  using ::Star::HitRequestPacket;
  using ::Star::DamageRequestPacket;
  using ::Star::DamageNotificationPacket;
  using ::Star::EntityMessagePacket;
  using ::Star::EntityMessageResponsePacket;
  using ::Star::UpdateWorldPropertiesPacket;
  using ::Star::StepUpdatePacket;
  using ::Star::SystemWorldStartPacket;
  using ::Star::SystemWorldUpdatePacket;
  using ::Star::SystemObjectCreatePacket;
  using ::Star::SystemObjectDestroyPacket;
  using ::Star::SystemShipCreatePacket;
  using ::Star::SystemShipDestroyPacket;
  using ::Star::SystemObjectSpawnPacket;
  using ::Star::UpdateWorldTemplatePacket;
  using ::Star::ClientCustomWorldRequest;
  using ::Star::ClientCustomWorldResponse;
  using ::Star::ClientCustomWorldCreate;
  using ::Star::ClientSubWorldPackets;
  using ::Star::ClientSubWorldRequest;
  using ::Star::ClientSubWorldReject;
  using ::Star::NotifyWorldLoad;
  using ::Star::LogMapUpdate;
  using ::Star::UniverseMessage;
  using ::Star::UniverseMessageResponse;
}
