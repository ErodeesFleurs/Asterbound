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

extern VersionNumber const star_protocol_version;

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
extern std::expected<EnumMap<NetCompressionMode>, StarException> const net_compression_mode_names;

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

  PacketCompressionMode compression_mode() const;
  void set_compression_mode(PacketCompressionMode compression_mode);

  PacketCompressionMode compression_mode_setting = PacketCompressionMode::Automatic;
};

// Stages a packet for wire decoding; identity fields are initially unfilled.
[[nodiscard]] std::expected<std::shared_ptr<Packet>, StarException> create_packet(PacketType type);
// Constructs a raw outgoing packet, generating identity fields before JSON load.
[[nodiscard]] std::expected<std::shared_ptr<Packet>, StarException> create_packet(
    RandomService& random, PacketType type, std::optional<Json> const& args);

template <PacketType packet_type_value>
struct PacketBase : public Packet {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PacketBase<packet_type_value>, Packet>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PacketBase<packet_type_value>>(); }

  static PacketType const packet_type = packet_type_value;

  PacketType type() const override { return packet_type; }
};

// The concrete CRTP argument is incomplete until its class body is defined.
template <typename T>
concept packet_class = std::is_class_v<T>;

// Share the concrete query implementation without adding another queryable
// layer between the concrete type, its wire-typed PacketBase, and Packet.
template <packet_class Derived, PacketType packet_type_value>
struct TypedPacket : PacketBase<packet_type_value> {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<Derived, PacketBase<packet_type_value>>(static_cast<Derived const&>(*this), query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<Derived>(); }

protected:
  TypedPacket() = default;
};

struct ProtocolRequestPacket : TypedPacket<ProtocolRequestPacket, PacketType::ProtocolRequest> {
  ProtocolRequestPacket();
  ProtocolRequestPacket(VersionNumber request_protocol_version);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  VersionNumber request_protocol_version;
};

struct ProtocolResponsePacket : TypedPacket<ProtocolResponsePacket, PacketType::ProtocolResponse> {
  ProtocolResponsePacket();
  ProtocolResponsePacket(bool allowed, Json info = {});

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  bool allowed;
  Json info;
};

struct ServerDisconnectPacket : TypedPacket<ServerDisconnectPacket, PacketType::ServerDisconnect> {
  ServerDisconnectPacket();
  ServerDisconnectPacket(String reason);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String reason;
};

struct ConnectSuccessPacket : TypedPacket<ConnectSuccessPacket, PacketType::ConnectSuccess> {
  ConnectSuccessPacket();
  ConnectSuccessPacket(ConnectionId client_id, Uuid server_uuid, CelestialBaseInformation celestial_information);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ConnectionId client_id;
  Uuid server_uuid;
  CelestialBaseInformation celestial_information;
};

struct ConnectFailurePacket : TypedPacket<ConnectFailurePacket, PacketType::ConnectFailure> {
  ConnectFailurePacket();
  ConnectFailurePacket(String reason);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String reason;
};

struct HandshakeChallengePacket : TypedPacket<HandshakeChallengePacket, PacketType::HandshakeChallenge> {
  HandshakeChallengePacket();
  HandshakeChallengePacket(ByteArray const& password_salt);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray password_salt;
};

struct ChatReceivePacket : TypedPacket<ChatReceivePacket, PacketType::ChatReceive> {
  ChatReceivePacket();
  ChatReceivePacket(ChatReceivedMessage received_message);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  ChatReceivedMessage received_message;
};

struct UniverseTimeUpdatePacket : TypedPacket<UniverseTimeUpdatePacket, PacketType::UniverseTimeUpdate> {
  UniverseTimeUpdatePacket();
  UniverseTimeUpdatePacket(double universe_time);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  double universe_time;
  float timescale;
};

struct CelestialResponsePacket : TypedPacket<CelestialResponsePacket, PacketType::CelestialResponse> {
  CelestialResponsePacket();
  CelestialResponsePacket(List<CelestialResponse> responses);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  List<CelestialResponse> responses;
};

struct PlayerWarpResultPacket : TypedPacket<PlayerWarpResultPacket, PacketType::PlayerWarpResult> {
  PlayerWarpResultPacket();
  PlayerWarpResultPacket(bool success, WarpAction warp_action, bool warp_action_invalid);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  bool success;
  WarpAction warp_action;
  bool warp_action_invalid;
};

struct PlanetTypeUpdatePacket : TypedPacket<PlanetTypeUpdatePacket, PacketType::PlanetTypeUpdate> {
  PlanetTypeUpdatePacket();
  PlanetTypeUpdatePacket(CelestialCoordinate coordinate);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  CelestialCoordinate coordinate;
};

struct PausePacket : TypedPacket<PausePacket, PacketType::Pause> {
  PausePacket();
  PausePacket(bool pause, float timescale = 1.0f);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  bool pause = false;
  float timescale = 1.0f;
};

struct ServerInfoPacket : TypedPacket<ServerInfoPacket, PacketType::ServerInfo> {
  ServerInfoPacket();
  ServerInfoPacket(std::uint16_t players, std::uint16_t max_players);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  std::uint16_t players;
  std::uint16_t max_players;
};

struct ClientConnectPacket : TypedPacket<ClientConnectPacket, PacketType::ClientConnect> {
  ClientConnectPacket();
  ClientConnectPacket(ByteArray assets_digest, bool allow_assets_mismatch, Uuid player_uuid, String player_name,
      String ship_species, WorldChunks ship_chunks, ShipUpgrades ship_upgrades, bool intro_complete,
      String account, Json info = {});

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray assets_digest;
  bool allow_assets_mismatch;
  Uuid player_uuid;
  String player_name;
  String ship_species;
  WorldChunks ship_chunks;
  ShipUpgrades ship_upgrades;
  bool intro_complete;
  String account;
  Json info;
};

struct ClientDisconnectRequestPacket : TypedPacket<ClientDisconnectRequestPacket, PacketType::ClientDisconnectRequest> {
  ClientDisconnectRequestPacket();

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;
};

struct HandshakeResponsePacket : TypedPacket<HandshakeResponsePacket, PacketType::HandshakeResponse> {
  [[nodiscard]] static ByteArray password_digest(String const& password, String const& account, ByteArray const& salt);

  HandshakeResponsePacket();
  HandshakeResponsePacket(ByteArray const& pass_hash);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray pass_hash;
};

struct PlayerWarpPacket : TypedPacket<PlayerWarpPacket, PacketType::PlayerWarp> {
  PlayerWarpPacket();
  PlayerWarpPacket(WarpAction action, bool deploy);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  WarpAction action;
  bool deploy;
};

struct FlyShipPacket : TypedPacket<FlyShipPacket, PacketType::FlyShip> {
  FlyShipPacket();
  FlyShipPacket(Vec3I system, SystemLocation location, Json settings = {});

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec3I system;
  SystemLocation location;
  Json settings;
};

struct ChatSendPacket : TypedPacket<ChatSendPacket, PacketType::ChatSend> {
  ChatSendPacket();
  ChatSendPacket(String text, ChatSendMode send_mode);
  ChatSendPacket(String text, ChatSendMode send_mode, JsonObject data);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String text;
  ChatSendMode send_mode;
  JsonObject data;
};

struct CelestialRequestPacket : TypedPacket<CelestialRequestPacket, PacketType::CelestialRequest> {
  CelestialRequestPacket();
  CelestialRequestPacket(List<CelestialRequest> requests);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  List<CelestialRequest> requests;
};

struct ClientContextUpdatePacket : TypedPacket<ClientContextUpdatePacket, PacketType::ClientContextUpdate> {
  ClientContextUpdatePacket();
  ClientContextUpdatePacket(ByteArray update_data);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray update_data;
};

// Sent when a client should initialize themselves on a new world
struct WorldStartPacket : TypedPacket<WorldStartPacket, PacketType::WorldStart> {
  WorldStartPacket();

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Json template_data;
  ByteArray sky_data;
  ByteArray weather_data;
  Vec2F player_start;
  Vec2F player_respawn;
  bool respawn_in_world;
  HashMap<DungeonId, float> dungeon_id_gravity;
  HashMap<DungeonId, bool> dungeon_id_breathable;
  StableHashSet<DungeonId> protected_dungeon_ids;
  Json world_properties;
  ConnectionId client_id;
  bool local_interpolation_mode;
};

// Sent when a client is leaving a world
struct WorldStopPacket : TypedPacket<WorldStopPacket, PacketType::WorldStop> {
  WorldStopPacket();
  WorldStopPacket(String const& reason);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String reason;
};

// Sent when the region data for the client's current world changes
struct WorldLayoutUpdatePacket : TypedPacket<WorldLayoutUpdatePacket, PacketType::WorldLayoutUpdate> {
  WorldLayoutUpdatePacket();
  WorldLayoutUpdatePacket(Json const& layout_data);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Json layout_data;
};

// Sent when the environment status effect list for the client's current world changes
struct WorldParametersUpdatePacket : TypedPacket<WorldParametersUpdatePacket, PacketType::WorldParametersUpdate> {
  WorldParametersUpdatePacket();
  WorldParametersUpdatePacket(ByteArray parameters_data);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray parameters_data;
};

struct CentralStructureUpdatePacket : TypedPacket<CentralStructureUpdatePacket, PacketType::CentralStructureUpdate> {
  CentralStructureUpdatePacket();
  CentralStructureUpdatePacket(Json structure_data);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Json structure_data;
};

struct TileArrayUpdatePacket : TypedPacket<TileArrayUpdatePacket, PacketType::TileArrayUpdate> {
  using TileArray = MultiArray<NetTile, 2>;

  TileArrayUpdatePacket();

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec2I min;
  TileArray array;
};

struct TileUpdatePacket : TypedPacket<TileUpdatePacket, PacketType::TileUpdate> {
  TileUpdatePacket() {}
  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec2I position;
  NetTile tile;
};

struct TileLiquidUpdatePacket : TypedPacket<TileLiquidUpdatePacket, PacketType::TileLiquidUpdate> {
  TileLiquidUpdatePacket();
  TileLiquidUpdatePacket(Vec2I const& position, LiquidNetUpdate liquid_update);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec2I position;
  LiquidNetUpdate liquid_update;
};

struct TileDamageUpdatePacket : TypedPacket<TileDamageUpdatePacket, PacketType::TileDamageUpdate> {
  TileDamageUpdatePacket();
  TileDamageUpdatePacket(Vec2I const& position, TileLayer layer, TileDamageStatus const& tile_damage);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec2I position;
  TileLayer layer;

  TileDamageStatus tile_damage;
};

struct TileModificationFailurePacket : TypedPacket<TileModificationFailurePacket, PacketType::TileModificationFailure> {
  TileModificationFailurePacket();
  TileModificationFailurePacket(TileModificationList modifications);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  TileModificationList modifications;
};

struct GiveItemPacket : TypedPacket<GiveItemPacket, PacketType::GiveItem> {
  GiveItemPacket();
  GiveItemPacket(ItemDescriptor const& item);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  ItemDescriptor item;
};

struct EnvironmentUpdatePacket : TypedPacket<EnvironmentUpdatePacket, PacketType::EnvironmentUpdate> {
  EnvironmentUpdatePacket();
  EnvironmentUpdatePacket(ByteArray sky_delta, ByteArray weather_delta);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray sky_delta;
  ByteArray weather_delta;
};

struct UpdateTileProtectionPacket : TypedPacket<UpdateTileProtectionPacket, PacketType::UpdateTileProtection> {
  UpdateTileProtectionPacket();
  UpdateTileProtectionPacket(DungeonId dungeon_id, bool is_protected);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  DungeonId dungeon_id;
  bool is_protected;
};

struct SetDungeonGravityPacket : TypedPacket<SetDungeonGravityPacket, PacketType::SetDungeonGravity> {
  SetDungeonGravityPacket();
  SetDungeonGravityPacket(DungeonId dungeon_id, std::optional<float> gravity);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  DungeonId dungeon_id;
  std::optional<float> gravity;
};

struct SetDungeonBreathablePacket : TypedPacket<SetDungeonBreathablePacket, PacketType::SetDungeonBreathable> {
  SetDungeonBreathablePacket();
  SetDungeonBreathablePacket(DungeonId dungeon_id, std::optional<bool> breathable);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  DungeonId dungeon_id;
  std::optional<bool> breathable;
};

struct SetPlayerStartPacket : TypedPacket<SetPlayerStartPacket, PacketType::SetPlayerStart> {
  SetPlayerStartPacket();
  SetPlayerStartPacket(Vec2F player_start, bool respawn_in_world);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  Vec2F player_start;
  bool respawn_in_world;
};

struct FindUniqueEntityResponsePacket
    : TypedPacket<FindUniqueEntityResponsePacket, PacketType::FindUniqueEntityResponse> {
  FindUniqueEntityResponsePacket();
  FindUniqueEntityResponsePacket(String unique_entity_id, std::optional<Vec2F> entity_position);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String unique_entity_id;
  std::optional<Vec2F> entity_position;
};

struct PongPacket : TypedPacket<PongPacket, PacketType::Pong> {
  PongPacket();
  PongPacket(std::int64_t time);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  std::int64_t time = 0;
};

struct ModifyTileListPacket : TypedPacket<ModifyTileListPacket, PacketType::ModifyTileList> {
  ModifyTileListPacket();
  ModifyTileListPacket(TileModificationList modifications, bool allow_entity_overlap);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  TileModificationList modifications;
  bool allow_entity_overlap;
};

struct ReplaceTileListPacket : TypedPacket<ReplaceTileListPacket, PacketType::ReplaceTileList> {
  ReplaceTileListPacket();
  ReplaceTileListPacket(TileModificationList modifications, TileDamage tile_damage, bool apply_damage);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  TileModificationList modifications;
  TileDamage tile_damage;
  bool apply_damage;
};

struct DamageTileGroupPacket : TypedPacket<DamageTileGroupPacket, PacketType::DamageTileGroup> {
  DamageTileGroupPacket();
  DamageTileGroupPacket(List<Vec2I> tile_positions, TileLayer layer, Vec2F source_position, TileDamage tile_damage, std::optional<EntityId> source_entity);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  List<Vec2I> tile_positions;
  TileLayer layer;
  Vec2F source_position;
  TileDamage tile_damage;
  std::optional<EntityId> source_entity;
};

struct CollectLiquidPacket : TypedPacket<CollectLiquidPacket, PacketType::CollectLiquid> {
  CollectLiquidPacket();
  CollectLiquidPacket(List<Vec2I> tile_positions, LiquidId liquid_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  List<Vec2I> tile_positions;
  LiquidId liquid_id;
};

struct RequestDropPacket : TypedPacket<RequestDropPacket, PacketType::RequestDrop> {
  RequestDropPacket();
  RequestDropPacket(EntityId drop_entity_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  EntityId drop_entity_id;
};

struct SpawnEntityPacket : TypedPacket<SpawnEntityPacket, PacketType::SpawnEntity> {
  SpawnEntityPacket();
  SpawnEntityPacket(EntityType entity_type, ByteArray store_data, ByteArray first_net_state);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  EntityType entity_type;
  ByteArray store_data;
  ByteArray first_net_state;
};

struct ConnectWirePacket : TypedPacket<ConnectWirePacket, PacketType::ConnectWire> {
  ConnectWirePacket();
  ConnectWirePacket(WireConnection output_connection, WireConnection input_connection);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  WireConnection output_connection;
  WireConnection input_connection;
};

struct DisconnectAllWiresPacket : TypedPacket<DisconnectAllWiresPacket, PacketType::DisconnectAllWires> {
  DisconnectAllWiresPacket();
  DisconnectAllWiresPacket(Vec2I entity_position, WireNode wire_node);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec2I entity_position;
  WireNode wire_node;
};

struct WorldClientStateUpdatePacket : TypedPacket<WorldClientStateUpdatePacket, PacketType::WorldClientStateUpdate> {
  WorldClientStateUpdatePacket();
  WorldClientStateUpdatePacket(ByteArray world_client_state_delta);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray world_client_state_delta;
};

struct FindUniqueEntityPacket : TypedPacket<FindUniqueEntityPacket, PacketType::FindUniqueEntity> {
  FindUniqueEntityPacket();
  FindUniqueEntityPacket(String unique_entity_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String unique_entity_id;
};

struct WorldStartAcknowledgePacket : TypedPacket<WorldStartAcknowledgePacket, PacketType::WorldStartAcknowledge> {
  WorldStartAcknowledgePacket();

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;
};

struct PingPacket : TypedPacket<PingPacket, PacketType::Ping> {
  PingPacket();
  PingPacket(std::int64_t time);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  std::int64_t time = 0;
};

struct EntityCreatePacket : TypedPacket<EntityCreatePacket, PacketType::EntityCreate> {
  EntityCreatePacket();
  EntityCreatePacket(EntityType entity_type, ByteArray store_data, ByteArray first_net_state, EntityId entity_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  EntityType entity_type;
  ByteArray store_data;
  ByteArray first_net_state;
  EntityId entity_id;
};

// All entity deltas will be sent at the same time for the same connection
// where they are master, any entities whose master is from that connection can
// be assumed to have produced a blank delta.
struct EntityUpdateSetPacket : TypedPacket<EntityUpdateSetPacket, PacketType::EntityUpdateSet> {
  EntityUpdateSetPacket();
  EntityUpdateSetPacket(ConnectionId for_connection);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ConnectionId for_connection;
  HashMap<EntityId, ByteArray> deltas;
};

struct EntityDestroyPacket : TypedPacket<EntityDestroyPacket, PacketType::EntityDestroy> {
  EntityDestroyPacket();
  EntityDestroyPacket(EntityId entity_id, ByteArray final_net_state, bool death);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  EntityId entity_id;
  ByteArray final_net_state;
  // If true, the entity removal is due to death rather simply for example
  // going out of range of the entity monitoring window.
  bool death;
};

struct EntityInteractPacket : TypedPacket<EntityInteractPacket, PacketType::EntityInteract> {
  EntityInteractPacket() = default;
  EntityInteractPacket(InteractRequest interact_request, Uuid request_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  InteractRequest interact_request;
  Uuid request_id = Uuid::nil();
};

struct EntityInteractResultPacket : TypedPacket<EntityInteractResultPacket, PacketType::EntityInteractResult> {
  EntityInteractResultPacket() = default;
  EntityInteractResultPacket(InteractAction action, Uuid request_id, EntityId source_entity_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  InteractAction action;
  Uuid request_id = Uuid::nil();
  EntityId source_entity_id = null_entity_id;
};

struct HitRequestPacket : TypedPacket<HitRequestPacket, PacketType::HitRequest> {
  HitRequestPacket();
  HitRequestPacket(RemoteHitRequest remote_hit_request);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  RemoteHitRequest remote_hit_request;
};

struct DamageRequestPacket : TypedPacket<DamageRequestPacket, PacketType::DamageRequest> {
  DamageRequestPacket();
  DamageRequestPacket(RemoteDamageRequest remote_damage_request);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  RemoteDamageRequest remote_damage_request;
};

struct DamageNotificationPacket : TypedPacket<DamageNotificationPacket, PacketType::DamageNotification> {
  DamageNotificationPacket();
  DamageNotificationPacket(RemoteDamageNotification remote_damage_notification);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  RemoteDamageNotification remote_damage_notification;
};

struct EntityMessagePacket : TypedPacket<EntityMessagePacket, PacketType::EntityMessage> {
  EntityMessagePacket();
  EntityMessagePacket(std::variant<EntityId, String> entity_id, String message, JsonArray args, Uuid uuid, ConnectionId from_connection = server_connection_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  std::variant<EntityId, String> entity_id;
  String message;
  JsonArray args;
  Uuid uuid;
  ConnectionId from_connection;
};

struct EntityMessageResponsePacket : TypedPacket<EntityMessageResponsePacket, PacketType::EntityMessageResponse> {
  EntityMessageResponsePacket();
  EntityMessageResponsePacket(std::expected<Json, String> response, Uuid uuid);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  std::expected<Json, String> response;
  Uuid uuid;
};

struct UpdateWorldPropertiesPacket : TypedPacket<UpdateWorldPropertiesPacket, PacketType::UpdateWorldProperties> {
  UpdateWorldPropertiesPacket();
  UpdateWorldPropertiesPacket(JsonObject const& updated_properties);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  JsonObject updated_properties;
};

struct StepUpdatePacket : TypedPacket<StepUpdatePacket, PacketType::StepUpdate> {
  StepUpdatePacket() = default;
  StepUpdatePacket(double remote_time);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  double remote_time = 0.0;
};

struct SystemWorldStartPacket : TypedPacket<SystemWorldStartPacket, PacketType::SystemWorldStart> {
  SystemWorldStartPacket();
  SystemWorldStartPacket(Vec3I location, List<ByteArray> object_stores, List<ByteArray> ship_stores, std::pair<Uuid, SystemLocation> client_ship);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Vec3I location;
  List<ByteArray> object_stores;
  List<ByteArray> ship_stores;
  std::pair<Uuid, SystemLocation> client_ship;
};

struct SystemWorldUpdatePacket : TypedPacket<SystemWorldUpdatePacket, PacketType::SystemWorldUpdate> {
  SystemWorldUpdatePacket();
  SystemWorldUpdatePacket(HashMap<Uuid, ByteArray> object_updates, HashMap<Uuid, ByteArray> ship_updates);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  HashMap<Uuid, ByteArray> object_updates;
  HashMap<Uuid, ByteArray> ship_updates;
};

struct SystemObjectCreatePacket : TypedPacket<SystemObjectCreatePacket, PacketType::SystemObjectCreate> {
  SystemObjectCreatePacket();
  SystemObjectCreatePacket(ByteArray object_store);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray object_store;
};

struct SystemObjectDestroyPacket : TypedPacket<SystemObjectDestroyPacket, PacketType::SystemObjectDestroy> {
  SystemObjectDestroyPacket();
  SystemObjectDestroyPacket(Uuid object_uuid);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Uuid object_uuid;
};

struct SystemShipCreatePacket : TypedPacket<SystemShipCreatePacket, PacketType::SystemShipCreate> {
  SystemShipCreatePacket();
  SystemShipCreatePacket(ByteArray ship_store);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ByteArray ship_store;
};

struct SystemShipDestroyPacket : TypedPacket<SystemShipDestroyPacket, PacketType::SystemShipDestroy> {
  SystemShipDestroyPacket();
  SystemShipDestroyPacket(Uuid ship_uuid);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Uuid ship_uuid;
};

struct SystemObjectSpawnPacket : TypedPacket<SystemObjectSpawnPacket, PacketType::SystemObjectSpawn> {
  SystemObjectSpawnPacket();
  SystemObjectSpawnPacket(String type_name, Uuid uuid, std::optional<Vec2F> position, JsonObject parameters);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String type_name;
  Uuid uuid;
  std::optional<Vec2F> position;
  JsonObject parameters;
};

struct UpdateWorldTemplatePacket : TypedPacket<UpdateWorldTemplatePacket, PacketType::UpdateWorldTemplate> {
  UpdateWorldTemplatePacket();
  UpdateWorldTemplatePacket(Json template_data);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Json template_data;
};

struct ClientCustomWorldRequest : TypedPacket<ClientCustomWorldRequest, PacketType::ClientCustomWorldRequest> {
  ClientCustomWorldRequest();
  ClientCustomWorldRequest(String name);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String name;
};

struct ClientCustomWorldResponse : TypedPacket<ClientCustomWorldResponse, PacketType::ClientCustomWorldResponse> {
  ClientCustomWorldResponse();
  ClientCustomWorldResponse(String name, WorldChunks chunks);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String name;
  WorldChunks chunks;
};

struct ClientCustomWorldCreate : TypedPacket<ClientCustomWorldCreate, PacketType::ClientCustomWorldCreate> {
  ClientCustomWorldCreate();
  ClientCustomWorldCreate(String name, Json template_data);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String name;
  Json template_data;
};

// wrapper packet for client world threads
struct ClientSubWorldPackets : TypedPacket<ClientSubWorldPackets, PacketType::ClientSubWorldPackets> {
  ClientSubWorldPackets();
  ClientSubWorldPackets(ClientSubWorldId const& sub_world_id, List<std::shared_ptr<Packet>> packets);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ClientSubWorldId sub_world_id{};
  List<std::shared_ptr<Packet>> packets;
};

struct ClientSubWorldRequest : TypedPacket<ClientSubWorldRequest, PacketType::ClientSubWorldRequest> {
  ClientSubWorldRequest();
  ClientSubWorldRequest(ClientSubWorldId const& sub_world_id, WorldId world_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ClientSubWorldId sub_world_id;
  WorldId world_id;
};

struct ClientSubWorldReject : TypedPacket<ClientSubWorldReject, PacketType::ClientSubWorldReject> {
  ClientSubWorldReject();
  ClientSubWorldReject(ClientSubWorldId const& sub_world_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  ClientSubWorldId sub_world_id;
};

struct NotifyWorldLoad : TypedPacket<NotifyWorldLoad, PacketType::NotifyWorldLoad> {
  NotifyWorldLoad();
  NotifyWorldLoad(WorldId world_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  WorldId world_id;
};

struct LogMapUpdate : TypedPacket<LogMapUpdate, PacketType::LogMapUpdate> {
  LogMapUpdate();
  LogMapUpdate(Map<String,String> map);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  Map<String,String> map;
};

struct UniverseMessage : TypedPacket<UniverseMessage, PacketType::UniverseMessage> {
  UniverseMessage();
  UniverseMessage(ConnectionId connection, String message, JsonArray args, Uuid uuid, ConnectionId from_connection = server_connection_id);

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  [[nodiscard]] std::expected<void, StarException> read_json(Json const& json) override;
  [[nodiscard]] std::expected<Json, StarException> write_json() const override;

  ConnectionId connection;
  String message;
  JsonArray args;
  Uuid uuid;
  ConnectionId from_connection;
};

struct UniverseMessageResponse : TypedPacket<UniverseMessageResponse, PacketType::UniverseMessageResponse> {
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
  using ::Star::star_protocol_version;
  using ::Star::PacketType;
  using ::Star::packet_type_name;
  using ::Star::packet_type_from_name;
  using ::Star::NetCompressionMode;
  using ::Star::net_compression_mode_names;
  using ::Star::PacketCompressionMode;
  using ::Star::create_packet;
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
