import star.json_extra;
import star.sha256;
import star.xx_hash;
import star.json;
import star.id_map;
import star.data_stream;
import star.list;
import star.bi_map;
import star.game_types;
import star.math_common;
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
import star.net_compatibility;
import std;
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
import star.data_stream_extra;

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

import star.byte_array;
namespace Star {

extern VersionNumber const star_protocol_version = 747;

namespace {
using PacketRandomSource = std::optional<std::reference_wrapper<RandomService>>;

struct PacketRegistration {
  // The type/name pair also serves as the fixed enum-name table.
  using first_type = PacketType;
  using Factory = std::shared_ptr<Packet>(PacketRandomSource);

  PacketType first;
  std::string_view second;
  std::reference_wrapper<Factory> factory;
};

template <typename T>
concept registered_packet = std::default_initializable<T> && std::derived_from<T, PacketBase<T::packet_type>>;

template <typename T, auto identity>
concept packet_identity = std::same_as<decltype(identity), std::nullptr_t> || requires(T& packet) {
  { std::invoke(identity, packet) } -> std::same_as<Uuid&>;
};

template <registered_packet T, auto identity> requires packet_identity<T, identity>
std::shared_ptr<Packet> construct_registered_packet(PacketRandomSource random) {
  auto packet = std::make_shared<T>();
  if constexpr (!std::same_as<decltype(identity), std::nullptr_t>) {
    if (random)
      std::invoke(identity, *packet) = Uuid(random->get());
  }
  return packet;
}

template <registered_packet T, auto identity = nullptr> requires packet_identity<T, identity>
constexpr PacketRegistration register_packet(std::string_view name) {
  return {T::packet_type, name, construct_registered_packet<T, identity>};
}

constexpr auto packet_registrations = std::to_array<PacketRegistration>({
  register_packet<ProtocolRequestPacket>("ProtocolRequest"),
  register_packet<ProtocolResponsePacket>("ProtocolResponse"),
  register_packet<ServerDisconnectPacket>("ServerDisconnect"),
  register_packet<ConnectSuccessPacket, [](ConnectSuccessPacket& packet) -> Uuid& { return packet.server_uuid; }>("ConnectSuccess"),
  register_packet<ConnectFailurePacket>("ConnectFailure"),
  register_packet<HandshakeChallengePacket>("HandshakeChallenge"),
  register_packet<ChatReceivePacket>("ChatReceive"),
  register_packet<UniverseTimeUpdatePacket>("UniverseTimeUpdate"),
  register_packet<CelestialResponsePacket>("CelestialResponse"),
  register_packet<PlayerWarpResultPacket>("PlayerWarpResult"),
  register_packet<PlanetTypeUpdatePacket>("PlanetTypeUpdate"),
  register_packet<PausePacket>("Pause"),
  register_packet<ServerInfoPacket>("ServerInfo"),
  register_packet<ClientConnectPacket, [](ClientConnectPacket& packet) -> Uuid& { return packet.player_uuid; }>("ClientConnect"),
  register_packet<ClientDisconnectRequestPacket>("ClientDisconnectRequest"),
  register_packet<HandshakeResponsePacket>("HandshakeResponse"),
  register_packet<PlayerWarpPacket>("PlayerWarp"),
  register_packet<FlyShipPacket>("FlyShip"),
  register_packet<ChatSendPacket>("ChatSend"),
  register_packet<CelestialRequestPacket>("CelestialRequest"),
  register_packet<ClientContextUpdatePacket>("ClientContextUpdate"),
  register_packet<WorldStartPacket>("WorldStart"),
  register_packet<WorldStopPacket>("WorldStop"),
  register_packet<WorldLayoutUpdatePacket>("WorldLayoutUpdate"),
  register_packet<WorldParametersUpdatePacket>("WorldParametersUpdate"),
  register_packet<CentralStructureUpdatePacket>("CentralStructureUpdate"),
  register_packet<TileArrayUpdatePacket>("TileArrayUpdate"),
  register_packet<TileUpdatePacket>("TileUpdate"),
  register_packet<TileLiquidUpdatePacket>("TileLiquidUpdate"),
  register_packet<TileDamageUpdatePacket>("TileDamageUpdate"),
  register_packet<TileModificationFailurePacket>("TileModificationFailure"),
  register_packet<GiveItemPacket>("GiveItem"),
  register_packet<EnvironmentUpdatePacket>("EnvironmentUpdate"),
  register_packet<UpdateTileProtectionPacket>("UpdateTileProtection"),
  register_packet<SetDungeonGravityPacket>("SetDungeonGravity"),
  register_packet<SetDungeonBreathablePacket>("SetDungeonBreathable"),
  register_packet<SetPlayerStartPacket>("SetPlayerStart"),
  register_packet<FindUniqueEntityResponsePacket>("FindUniqueEntityResponse"),
  register_packet<PongPacket>("Pong"),
  register_packet<ModifyTileListPacket>("ModifyTileList"),
  register_packet<DamageTileGroupPacket>("DamageTileGroup"),
  register_packet<CollectLiquidPacket>("CollectLiquid"),
  register_packet<RequestDropPacket>("RequestDrop"),
  register_packet<SpawnEntityPacket>("SpawnEntity"),
  register_packet<ConnectWirePacket>("ConnectWire"),
  register_packet<DisconnectAllWiresPacket>("DisconnectAllWires"),
  register_packet<WorldClientStateUpdatePacket>("WorldClientStateUpdate"),
  register_packet<FindUniqueEntityPacket>("FindUniqueEntity"),
  register_packet<WorldStartAcknowledgePacket>("WorldStartAcknowledge"),
  register_packet<PingPacket>("Ping"),
  register_packet<EntityCreatePacket>("EntityCreate"),
  register_packet<EntityUpdateSetPacket>("EntityUpdate"),
  register_packet<EntityDestroyPacket>("EntityDestroy"),
  register_packet<EntityInteractPacket, [](EntityInteractPacket& packet) -> Uuid& { return packet.request_id; }>("EntityInteract"),
  register_packet<EntityInteractResultPacket, [](EntityInteractResultPacket& packet) -> Uuid& {
    return packet.request_id;
  }>("EntityInteractResult"),
  register_packet<HitRequestPacket>("HitRequest"),
  register_packet<DamageRequestPacket>("DamageRequest"),
  register_packet<DamageNotificationPacket>("DamageNotification"),
  register_packet<EntityMessagePacket, [](EntityMessagePacket& packet) -> Uuid& { return packet.uuid; }>("EntityMessage"),
  register_packet<EntityMessageResponsePacket, [](EntityMessageResponsePacket& packet) -> Uuid& {
    return packet.uuid;
  }>("EntityMessageResponse"),
  register_packet<UpdateWorldPropertiesPacket>("UpdateWorldProperties"),
  register_packet<StepUpdatePacket>("StepUpdate"),
  register_packet<SystemWorldStartPacket, [](SystemWorldStartPacket& packet) -> Uuid& {
    return packet.client_ship.first;
  }>("SystemWorldStart"),
  register_packet<SystemWorldUpdatePacket>("SystemWorldUpdate"),
  register_packet<SystemObjectCreatePacket>("SystemObjectCreate"),
  register_packet<SystemObjectDestroyPacket, [](SystemObjectDestroyPacket& packet) -> Uuid& {
    return packet.object_uuid;
  }>("SystemObjectDestroy"),
  register_packet<SystemShipCreatePacket>("SystemShipCreate"),
  register_packet<SystemShipDestroyPacket, [](SystemShipDestroyPacket& packet) -> Uuid& {
    return packet.ship_uuid;
  }>("SystemShipDestroy"),
  register_packet<SystemObjectSpawnPacket, [](SystemObjectSpawnPacket& packet) -> Uuid& { return packet.uuid; }>("SystemObjectSpawn"),
  // OpenStarbound packets
  register_packet<ReplaceTileListPacket>("ReplaceTileList"),
  register_packet<UpdateWorldTemplatePacket>("UpdateWorldTemplate"),
  register_packet<ClientCustomWorldRequest>("ClientCustomWorldRequest"),
  register_packet<ClientCustomWorldResponse>("ClientCustomWorldResponse"),
  register_packet<ClientCustomWorldCreate>("ClientCustomWorldCreate"),
  register_packet<ClientSubWorldPackets>("ClientSubWorldPackets"),
  register_packet<ClientSubWorldRequest>("ClientSubWorldRequest"),
  register_packet<ClientSubWorldReject>("ClientSubWorldReject"),
  register_packet<NotifyWorldLoad>("NotifyWorldLoad"),
  register_packet<LogMapUpdate>("LogMapUpdate"),
  register_packet<UniverseMessage, [](UniverseMessage& packet) -> Uuid& { return packet.uuid; }>("UniverseMessage"),
  register_packet<UniverseMessageResponse, [](UniverseMessageResponse& packet) -> Uuid& {
    return packet.uuid;
  }>("UniverseMessageResponse")
});

static_assert(std::to_underlying(PacketType::EntityUpdateSet) == 51);
static_assert([] {
  if (packet_registrations.size() != std::to_underlying(PacketType::UniverseMessageResponse) + 1u)
    return false;
  for (std::size_t i = 0; i < packet_registrations.size(); ++i) {
    if (std::to_underlying(packet_registrations[i].first) != i)
      return false;
  }
  return true;
}(), "Packet registrations must cover the wire IDs in order");

std::expected<std::shared_ptr<Packet>, StarException> construct_packet(PacketType type, PacketRandomSource random = {}) {
  auto wire_id = std::to_underlying(type);
  if (wire_id >= packet_registrations.size())
    return std::unexpected(StarPacketException::format("Unrecognized packet type {}", static_cast<unsigned int>(wire_id)));
  return packet_registrations[wire_id].factory(random);
}
}

std::expected<std::string_view, StarException> packet_type_name(PacketType type) {
  return enum_name<packet_registrations>(type);
}

std::expected<PacketType, StarException> packet_type_from_name(StringView name) {
  return enum_value<packet_registrations>(name);
}

extern std::expected<EnumMap<NetCompressionMode>, StarException> const net_compression_mode_names = EnumMap<NetCompressionMode>::create({
  {NetCompressionMode::None, "None"},
  {NetCompressionMode::Zstd, "Zstd"}
});

Packet::~Packet() = default;

std::expected<void, StarException> Packet::read(DataStream&) { return {}; }
std::expected<void, StarException> Packet::write(DataStream&) const { return {}; }
std::expected<void, StarException> Packet::read_json(Json const&) { return {}; }
std::expected<Json, StarException> Packet::write_json() const  { return JsonObject{}; }

PacketCompressionMode Packet::compression_mode() const { return compression_mode_setting; }
void Packet::set_compression_mode(PacketCompressionMode compression_mode) { compression_mode_setting = compression_mode; }

namespace {
template <stream_readable... Fields>
std::expected<void, StarException> read_packet_fields(DataStream& ds, Fields&... fields) {
  auto references = std::tie(fields...);
  return ds.read(references);
}

template <stream_writable... Fields>
std::expected<void, StarException> write_packet_fields(DataStream& ds, Fields const&... fields) {
  return ds.write(std::tie(fields...));
}

// Empty packet construction stages a subsequent wire/JSON read, not an identity.
// Sending code supplies IDs or uses the explicit random-source factory below.
Uuid unfilled_packet_uuid() {
  return Uuid::nil();
}

// Message responses retain their historical error/success tags, independently
// of the in-memory expected representation. Unknown tags consume no payload.
std::expected<void, StarException> read_message_response(DataStream& ds, std::expected<Json, String>& response) {
  auto tag = ds.read<std::uint8_t>();
  if (!tag)
    return std::unexpected(std::move(tag.error()));
  if (*tag == 1) {
    auto error = ds.read<String>();
    if (!error)
      return std::unexpected(std::move(error.error()));
    response = std::unexpected(std::move(*error));
  } else if (*tag == 2) {
    auto value = ds.read<Json>();
    if (!value)
      return std::unexpected(std::move(value.error()));
    response = std::move(*value);
  }
  return {};
}

std::expected<void, StarException> write_message_response(DataStream& ds, std::expected<Json, String> const& response) {
  auto tag = ds.write<std::uint8_t>(response.has_value() ? 2 : 1);
  if (!tag)
    return std::unexpected(std::move(tag.error()));
  return response.has_value() ? ds.write(*response) : ds.write(response.error());
}
}

std::expected<std::shared_ptr<Packet>, StarException> create_packet(PacketType type) {
  return construct_packet(type);
}

std::expected<std::shared_ptr<Packet>, StarException> create_packet(
    RandomService& random, PacketType type, std::optional<Json> const& args) {
  // Omitted JSON fields retain the generated identities. Even explicit fields
  // or a failing JSON read consume the historical draw before parsing.
  auto created = construct_packet(type, std::ref(random));
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto packet = std::move(*created);

  if (args && !args->is_null()) {
    auto read = packet->read_json(*args);
    if (!read)
      return std::unexpected(std::move(read.error()));
  }

  return packet;
}

ProtocolRequestPacket::ProtocolRequestPacket()
  : request_protocol_version(0) {}

ProtocolRequestPacket::ProtocolRequestPacket(VersionNumber request_protocol_version)
  : request_protocol_version(request_protocol_version) {}

std::expected<void, StarException> ProtocolRequestPacket::read(DataStream& ds) {
  return ds.read(request_protocol_version);
}

std::expected<void, StarException> ProtocolRequestPacket::write(DataStream& ds) const {
  return ds.write(request_protocol_version);
}

ProtocolResponsePacket::ProtocolResponsePacket() : ProtocolResponsePacket(false, {}) {}

ProtocolResponsePacket::ProtocolResponsePacket(bool allowed, Json info) : allowed(allowed), info(std::move(info)) {}

std::expected<void, StarException> ProtocolResponsePacket::read(DataStream& ds) {
  if (auto result = ds.read(allowed); !result)
    return std::unexpected(std::move(result.error()));
  if (compression_mode() != PacketCompressionMode::Enabled)
    return {};

  // Older OpenStarbound compressed replies may omit the info field.
  if (auto external_buffer = as_ref<DataStreamExternalBuffer>(ds)) {
    auto ended = external_buffer->get().at_end();
    if (!ended)
      return std::unexpected(std::move(ended.error()));
    if (*ended)
      return {};
  }
  return ds.read(info);
}

std::expected<void, StarException> ProtocolResponsePacket::write(DataStream& ds) const {
  if (auto result = ds.write(allowed); !result)
    return std::unexpected(std::move(result.error()));
  if (ds.stream_compatibility_version() != legacy_version)
    return ds.write(info);
  return {};
}

ConnectSuccessPacket::ConnectSuccessPacket() : server_uuid(unfilled_packet_uuid()) {}

ConnectSuccessPacket::ConnectSuccessPacket(
    ConnectionId client_id, Uuid server_uuid, CelestialBaseInformation celestial_information)
  : client_id(client_id), server_uuid(std::move(server_uuid)), celestial_information(std::move(celestial_information)) {}

std::expected<void, StarException> ConnectSuccessPacket::read(DataStream& ds) {
  if (auto result = ds.vuread(client_id); !result)
    return std::unexpected(std::move(result.error()));
  return read_packet_fields(ds, server_uuid, celestial_information);
}

std::expected<void, StarException> ConnectSuccessPacket::write(DataStream& ds) const {
  if (auto result = ds.vuwrite(client_id); !result)
    return std::unexpected(std::move(result.error()));
  return write_packet_fields(ds, server_uuid, celestial_information);
}

ConnectFailurePacket::ConnectFailurePacket() = default;

ConnectFailurePacket::ConnectFailurePacket(String reason) : reason(std::move(reason)) {}

std::expected<void, StarException> ConnectFailurePacket::read(DataStream& ds) {
  return ds.read(reason);
}

std::expected<void, StarException> ConnectFailurePacket::write(DataStream& ds) const {
  return ds.write(reason);
}

HandshakeChallengePacket::HandshakeChallengePacket() = default;

HandshakeChallengePacket::HandshakeChallengePacket(ByteArray const& password_salt) : password_salt(password_salt) {}

std::expected<void, StarException> HandshakeChallengePacket::read(DataStream& ds) {
  return ds.read(password_salt);
}

std::expected<void, StarException> HandshakeChallengePacket::write(DataStream& ds) const {
  return ds.write(password_salt);
}

ChatReceivePacket::ChatReceivePacket() = default;

ChatReceivePacket::ChatReceivePacket(ChatReceivedMessage received_message)
    : received_message(std::move(received_message)) {}

std::expected<void, StarException> ChatReceivePacket::read(DataStream& ds) {
  return ds.read(received_message);
}

std::expected<void, StarException> ChatReceivePacket::write(DataStream& ds) const {
  return ds.write(received_message);
}

std::expected<void, StarException> ChatReceivePacket::read_json(Json const& json) {
  auto data = json.get("receivedMessage");
  if (!data)
    return std::unexpected(std::move(data.error()));
  auto decoded = ChatReceivedMessage::create(*data);
  if (!decoded)
    return std::unexpected(std::move(decoded.error()));
  received_message = std::move(*decoded);
  return {};
}

std::expected<Json, StarException> ChatReceivePacket::write_json() const {
  auto encoded = received_message.to_json();
  if (!encoded)
    return std::unexpected(std::move(encoded.error()));
  return JsonObject{
    {"receivedMessage", std::move(*encoded)}
  };
}

UniverseTimeUpdatePacket::UniverseTimeUpdatePacket() {
  universe_time = 0;
}

UniverseTimeUpdatePacket::UniverseTimeUpdatePacket(double universe_time) : universe_time(universe_time) {}

std::expected<void, StarException> UniverseTimeUpdatePacket::read(DataStream& ds) {
  return ds.vfread(universe_time, 0.05);
}

std::expected<void, StarException> UniverseTimeUpdatePacket::write(DataStream& ds) const {
  return ds.vfwrite(universe_time, 0.05);
}

CelestialResponsePacket::CelestialResponsePacket() = default;

CelestialResponsePacket::CelestialResponsePacket(List<CelestialResponse> responses) : responses(std::move(responses)) {}

std::expected<void, StarException> CelestialResponsePacket::read(DataStream& ds) {
  return ds.read(responses);
}

std::expected<void, StarException> CelestialResponsePacket::write(DataStream& ds) const {
  return ds.write(responses);
}

PlayerWarpResultPacket::PlayerWarpResultPacket() : warp_action_invalid(false) {}

PlayerWarpResultPacket::PlayerWarpResultPacket(bool success, WarpAction warp_action, bool warp_action_invalid)
  : success(success), warp_action(std::move(warp_action)), warp_action_invalid(warp_action_invalid) {}

std::expected<void, StarException> PlayerWarpResultPacket::read(DataStream& ds) {
  return read_packet_fields(ds, success, warp_action, warp_action_invalid);
}

std::expected<void, StarException> PlayerWarpResultPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, success, warp_action, warp_action_invalid);
}

PlanetTypeUpdatePacket::PlanetTypeUpdatePacket() = default;

PlanetTypeUpdatePacket::PlanetTypeUpdatePacket(CelestialCoordinate coordinate)
  : coordinate(coordinate) {}

std::expected<void, StarException> PlanetTypeUpdatePacket::read(DataStream& ds) {
  return ds.read(coordinate);
}

std::expected<void, StarException> PlanetTypeUpdatePacket::write(DataStream& ds) const {
  return ds.write(coordinate);
}

PausePacket::PausePacket() = default;

PausePacket::PausePacket(bool pause, float timescale) : pause(pause), timescale(timescale) {}

std::expected<void, StarException> PausePacket::read(DataStream& ds) {
  if (auto result = ds.read(pause); !result)
    return std::unexpected(std::move(result.error()));
  if (ds.stream_compatibility_version() != legacy_version)
    return ds.read(timescale);
  timescale = 1.0f;
  return {};
}

std::expected<void, StarException> PausePacket::write(DataStream& ds) const {
  if (auto result = ds.write(pause); !result)
    return std::unexpected(std::move(result.error()));
  if (ds.stream_compatibility_version() != legacy_version)
    return ds.write(timescale);
  return {};
}

std::expected<void, StarException> PausePacket::read_json(Json const& json) {
  {
    auto result = json.get_bool("pause");
    if (!result)
      return std::unexpected(std::move(result.error()));
    pause = std::move(*result);
  }
  {
    auto result = json.get_float("timescale", 1.0f);
    if (!result)
      return std::unexpected(std::move(result.error()));
    timescale = std::move(*result);
  }
  return {};
}

std::expected<Json, StarException> PausePacket::write_json() const {
  return JsonObject{
    {"pause", pause},
    {"timescale", timescale}
  };
}

ServerInfoPacket::ServerInfoPacket() = default;

ServerInfoPacket::ServerInfoPacket(std::uint16_t players, std::uint16_t max_players) :
  players(players),
  max_players(max_players) {}

std::expected<void, StarException> ServerInfoPacket::read(DataStream& ds) {
  return read_packet_fields(ds, players, max_players);
}

std::expected<void, StarException> ServerInfoPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, players, max_players);
}

std::expected<void, StarException> ServerInfoPacket::read_json(Json const& json) {
  {
    auto result = json.get_uint("players");
    if (!result)
      return std::unexpected(std::move(result.error()));
    players = std::move(*result);
  }
  {
    auto result = json.get_uint("maxPlayers");
    if (!result)
      return std::unexpected(std::move(result.error()));
    max_players = std::move(*result);
  }
  return {};
}

std::expected<Json, StarException> ServerInfoPacket::write_json() const {
  return JsonObject{
    {"players", players},
    {"maxPlayers", max_players}
  };
}

ClientConnectPacket::ClientConnectPacket() : player_uuid(unfilled_packet_uuid()) {}

ClientConnectPacket::ClientConnectPacket(ByteArray assets_digest, bool allow_assets_mismatch, Uuid player_uuid,
    String player_name, String ship_species, WorldChunks ship_chunks, ShipUpgrades ship_upgrades,
    bool intro_complete, String account, Json info)
  : assets_digest(std::move(assets_digest)), allow_assets_mismatch(allow_assets_mismatch), player_uuid(std::move(player_uuid)),
    player_name(std::move(player_name)), ship_species(std::move(ship_species)), ship_chunks(std::move(ship_chunks)),
    ship_upgrades(std::move(ship_upgrades)), intro_complete(std::move(intro_complete)), account(std::move(account)), info(std::move(info)) {}


std::expected<void, StarException> ClientConnectPacket::read(DataStream& ds) {
  if (auto result = read_packet_fields(
      ds, assets_digest, allow_assets_mismatch, player_uuid, player_name, ship_species,
      ship_chunks, ship_upgrades, intro_complete, account); !result)
    return std::unexpected(std::move(result.error()));
  if (ds.stream_compatibility_version() != legacy_version)
    return ds.read(info);
  return {};
}

std::expected<void, StarException> ClientConnectPacket::write(DataStream& ds) const {
  if (auto result = write_packet_fields(
      ds, assets_digest, allow_assets_mismatch, player_uuid, player_name, ship_species,
      ship_chunks, ship_upgrades, intro_complete, account); !result)
    return std::unexpected(std::move(result.error()));
  if (ds.stream_compatibility_version() != legacy_version)
    return ds.write(info);
  return {};
}

ClientDisconnectRequestPacket::ClientDisconnectRequestPacket() = default;

std::expected<void, StarException> ClientDisconnectRequestPacket::read(DataStream& ds) {
  // Packets cannot be empty due to the way packet serialization is handled.
  if (auto result = ds.read<std::uint8_t>(); !result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> ClientDisconnectRequestPacket::write(DataStream& ds) const {
  // Packets cannot be empty due to the way packet serialization is handled.
  return ds.write<std::uint8_t>(0);
}

ByteArray HandshakeResponsePacket::password_digest(String const& password, String const& account, ByteArray const& salt) {
  Sha256Hasher digest;
  digest.push(password);
  digest.push(account);
  digest.push(salt);
  return digest.compute();
}

HandshakeResponsePacket::HandshakeResponsePacket() = default;

HandshakeResponsePacket::HandshakeResponsePacket(ByteArray const& pass_hash) : pass_hash(pass_hash) {}

std::expected<void, StarException> HandshakeResponsePacket::read(DataStream& ds) {
  return ds.read(pass_hash);
}

std::expected<void, StarException> HandshakeResponsePacket::write(DataStream& ds) const {
  return ds.write(pass_hash);
}

PlayerWarpPacket::PlayerWarpPacket() = default;

PlayerWarpPacket::PlayerWarpPacket(WarpAction action, bool deploy) : action(std::move(action)), deploy(std::move(deploy)) {}

std::expected<void, StarException> PlayerWarpPacket::read(DataStream& ds) {
  return read_packet_fields(ds, action, deploy);
}

std::expected<void, StarException> PlayerWarpPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, action, deploy);
}

FlyShipPacket::FlyShipPacket() = default;

FlyShipPacket::FlyShipPacket(Vec3I system, SystemLocation location, Json settings) : system(std::move(system)), location(std::move(location)), settings(std::move(settings)) {}

std::expected<void, StarException> FlyShipPacket::read(DataStream& ds) {
  if (auto result = read_packet_fields(ds, system, location); !result)
    return std::unexpected(std::move(result.error()));
  if (ds.stream_compatibility_version() >= 3)
    return ds.read(settings);
  return {};
}

std::expected<void, StarException> FlyShipPacket::write(DataStream& ds) const {
  if (auto result = write_packet_fields(ds, system, location); !result)
    return std::unexpected(std::move(result.error()));
  if (ds.stream_compatibility_version() >= 3)
    return ds.write(settings);
  return {};
}

ChatSendPacket::ChatSendPacket() : send_mode(ChatSendMode::Broadcast) {}

ChatSendPacket::ChatSendPacket(String text, ChatSendMode send_mode) : text(std::move(text)), send_mode(send_mode) {}

ChatSendPacket::ChatSendPacket(String text, ChatSendMode send_mode, JsonObject data) : text(std::move(text)), send_mode(send_mode), data(std::move(data)) {}

std::expected<void, StarException> ChatSendPacket::read(DataStream& ds) {
  String decoded_text;
  ChatSendMode decoded_mode;
  JsonObject decoded_data;
  if (auto result = read_packet_fields(ds, decoded_text, decoded_mode); !result)
    return std::unexpected(std::move(result.error()));
  if (ds.stream_compatibility_version() >= 5) {
    if (auto result = ds.read(decoded_data); !result)
      return std::unexpected(std::move(result.error()));
  }
  text = std::move(decoded_text);
  send_mode = decoded_mode;
  data = std::move(decoded_data);
  return {};
}

std::expected<void, StarException> ChatSendPacket::write(DataStream& ds) const {
  if (auto result = write_packet_fields(ds, text, send_mode); !result)
    return std::unexpected(std::move(result.error()));
  if (ds.stream_compatibility_version() >= 5)
    return ds.write(data);
  return {};
}

CelestialRequestPacket::CelestialRequestPacket() = default;

CelestialRequestPacket::CelestialRequestPacket(List<CelestialRequest> requests) : requests(std::move(requests)) {}

std::expected<void, StarException> CelestialRequestPacket::read(DataStream& ds) {
  return ds.read(requests);
}

std::expected<void, StarException> CelestialRequestPacket::write(DataStream& ds) const {
  return ds.write(requests);
}

ClientContextUpdatePacket::ClientContextUpdatePacket() = default;

ClientContextUpdatePacket::ClientContextUpdatePacket(ByteArray update_data) : update_data(std::move(update_data)) {}

std::expected<void, StarException> ClientContextUpdatePacket::read(DataStream& ds) {
  return ds.read(update_data);
}

std::expected<void, StarException> ClientContextUpdatePacket::write(DataStream& ds) const {
  return ds.write(update_data);
}

WorldStartPacket::WorldStartPacket() : client_id(), local_interpolation_mode() {}

std::expected<void, StarException> WorldStartPacket::read(DataStream& ds) {
  return read_packet_fields(ds, template_data, sky_data, weather_data, player_start, player_respawn,
      respawn_in_world, world_properties, dungeon_id_gravity, dungeon_id_breathable, protected_dungeon_ids,
      client_id, local_interpolation_mode);
}

std::expected<void, StarException> WorldStartPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, template_data, sky_data, weather_data, player_start, player_respawn,
      respawn_in_world, world_properties, dungeon_id_gravity, dungeon_id_breathable, protected_dungeon_ids,
      client_id, local_interpolation_mode);
}

WorldStopPacket::WorldStopPacket() = default;

WorldStopPacket::WorldStopPacket(String const& reason) : reason(reason) {}

std::expected<void, StarException> WorldStopPacket::read(DataStream& ds) {
  return ds.read(reason);
}

std::expected<void, StarException> WorldStopPacket::write(DataStream& ds) const {
  return ds.write(reason);
}

WorldLayoutUpdatePacket::WorldLayoutUpdatePacket() = default;

WorldLayoutUpdatePacket::WorldLayoutUpdatePacket(Json const& layout_data) : layout_data(layout_data) {}

std::expected<void, StarException> WorldLayoutUpdatePacket::read(DataStream& ds) {
  return ds.read(layout_data);
}

std::expected<void, StarException> WorldLayoutUpdatePacket::write(DataStream& ds) const {
  return ds.write(layout_data);
}

WorldParametersUpdatePacket::WorldParametersUpdatePacket() = default;

WorldParametersUpdatePacket::WorldParametersUpdatePacket(ByteArray parameters_data) : parameters_data(std::move(parameters_data)) {}

std::expected<void, StarException> WorldParametersUpdatePacket::read(DataStream& ds) {
  return ds.read(parameters_data);
}

std::expected<void, StarException> WorldParametersUpdatePacket::write(DataStream& ds) const {
  return ds.write(parameters_data);
}

CentralStructureUpdatePacket::CentralStructureUpdatePacket() = default;

CentralStructureUpdatePacket::CentralStructureUpdatePacket(Json structure_data) : structure_data(std::move(structure_data)) {}

std::expected<void, StarException> CentralStructureUpdatePacket::read(DataStream& ds) {
  return ds.read(structure_data);
}

std::expected<void, StarException> CentralStructureUpdatePacket::write(DataStream& ds) const {
  return ds.write(structure_data);
}

TileArrayUpdatePacket::TileArrayUpdatePacket() = default;

std::expected<void, StarException> TileArrayUpdatePacket::read(DataStream& ds) {
  if (auto result = ds.viread(min[0]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.viread(min[1]); !result)
    return std::unexpected(std::move(result.error()));

  std::size_t width, height;
  if (auto result = ds.vuread(width); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.vuread(height); !result)
    return std::unexpected(std::move(result.error()));
  auto resized = array.set_size(width, height);
  if (!resized)
    return std::unexpected(std::move(resized.error()));
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      if (auto result = ds.read(array(x, y)); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> TileArrayUpdatePacket::write(DataStream& ds) const {
  if (auto result = ds.viwrite(min[0]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.viwrite(min[1]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.vuwrite(array.extent(0)); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.vuwrite(array.extent(1)); !result)
    return std::unexpected(std::move(result.error()));
  for (std::size_t y = 0; y < array.extent(1); ++y) {
    for (std::size_t x = 0; x < array.extent(0); ++x) {
      if (auto result = ds.write(array(x, y)); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> TileUpdatePacket::read(DataStream& ds) {
  if (auto result = ds.viread(position[0]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.viread(position[1]); !result)
    return std::unexpected(std::move(result.error()));
  return ds.read(tile);
}

std::expected<void, StarException> TileUpdatePacket::write(DataStream& ds) const {
  if (auto result = ds.viwrite(position[0]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.viwrite(position[1]); !result)
    return std::unexpected(std::move(result.error()));
  return ds.write(tile);
}

TileLiquidUpdatePacket::TileLiquidUpdatePacket() = default;

TileLiquidUpdatePacket::TileLiquidUpdatePacket(Vec2I const& position, LiquidNetUpdate liquid_update)
  : position(position), liquid_update(liquid_update) {}

std::expected<void, StarException> TileLiquidUpdatePacket::read(DataStream& ds) {
  if (auto result = ds.viread(position[0]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.viread(position[1]); !result)
    return std::unexpected(std::move(result.error()));
  return read_packet_fields(ds, liquid_update.liquid, liquid_update.level);
}

std::expected<void, StarException> TileLiquidUpdatePacket::write(DataStream& ds) const {
  if (auto result = ds.viwrite(position[0]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.viwrite(position[1]); !result)
    return std::unexpected(std::move(result.error()));
  return write_packet_fields(ds, liquid_update.liquid, liquid_update.level);
}

TileDamageUpdatePacket::TileDamageUpdatePacket() : layer(TileLayer::Foreground) {}

TileDamageUpdatePacket::TileDamageUpdatePacket(
    Vec2I const& position, TileLayer layer, TileDamageStatus const& tile_damage)
  : position(position), layer(layer), tile_damage(tile_damage) {}

std::expected<void, StarException> TileDamageUpdatePacket::read(DataStream& ds) {
  return read_packet_fields(ds, position, layer, tile_damage);
}

std::expected<void, StarException> TileDamageUpdatePacket::write(DataStream& ds) const {
  return write_packet_fields(ds, position, layer, tile_damage);
}

TileModificationFailurePacket::TileModificationFailurePacket() = default;

TileModificationFailurePacket::TileModificationFailurePacket(TileModificationList modifications)
    : modifications(std::move(modifications)) {}

std::expected<void, StarException> TileModificationFailurePacket::read(DataStream& ds) {
  return ds.read_container(modifications);
}

std::expected<void, StarException> TileModificationFailurePacket::write(DataStream& ds) const {
  return ds.write_container(modifications);
}

GiveItemPacket::GiveItemPacket() = default;

GiveItemPacket::GiveItemPacket(ItemDescriptor const& item) : item(item) {}

std::expected<void, StarException> GiveItemPacket::read(DataStream& ds) {
  return ds.read(item);
}

std::expected<void, StarException> GiveItemPacket::write(DataStream& ds) const {
  return ds.write(item);
}

std::expected<void, StarException> GiveItemPacket::read_json(Json const& json) {
  auto data = json.get("item");
  if (!data)
    return std::unexpected(std::move(data.error()));
  auto decoded = ItemDescriptor::create(*data);
  if (!decoded)
    return std::unexpected(std::move(decoded.error()));
  item = std::move(*decoded);
  return {};
}

std::expected<Json, StarException> GiveItemPacket::write_json() const {
  return JsonObject{
    {"item", item.to_json()}
  };
}

EnvironmentUpdatePacket::EnvironmentUpdatePacket() = default;

EnvironmentUpdatePacket::EnvironmentUpdatePacket(ByteArray sky_delta, ByteArray weather_delta)
  : sky_delta(std::move(sky_delta)), weather_delta(std::move(weather_delta)) {}

std::expected<void, StarException> EnvironmentUpdatePacket::read(DataStream& ds) {
  return read_packet_fields(ds, sky_delta, weather_delta);
}

std::expected<void, StarException> EnvironmentUpdatePacket::write(DataStream& ds) const {
  return write_packet_fields(ds, sky_delta, weather_delta);
}

ModifyTileListPacket::ModifyTileListPacket() : allow_entity_overlap() {}

ModifyTileListPacket::ModifyTileListPacket(TileModificationList modifications, bool allow_entity_overlap)
    : modifications(std::move(modifications)), allow_entity_overlap(allow_entity_overlap) {}

std::expected<void, StarException> ModifyTileListPacket::read(DataStream& ds) {
  if (auto result = ds.read_container(modifications); !result)
    return std::unexpected(std::move(result.error()));
  return ds.read(allow_entity_overlap);
}

std::expected<void, StarException> ModifyTileListPacket::write(DataStream& ds) const {
  if (auto result = ds.write_container(modifications); !result)
    return std::unexpected(std::move(result.error()));
  return ds.write(allow_entity_overlap);
}

ReplaceTileListPacket::ReplaceTileListPacket() : apply_damage() {}

ReplaceTileListPacket::ReplaceTileListPacket(TileModificationList modifications, TileDamage tile_damage,
                                             bool apply_damage)
    : modifications(std::move(modifications)), tile_damage(std::move(tile_damage)), apply_damage(apply_damage) {}

std::expected<void, StarException> ReplaceTileListPacket::read(DataStream& ds) {
  if (auto result = ds.read_container(modifications); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(tile_damage); !result)
    return std::unexpected(std::move(result.error()));
  if (ds.stream_compatibility_version() >= 7)
    return ds.read(apply_damage);
  apply_damage = false;
  return {};
}

std::expected<void, StarException> ReplaceTileListPacket::write(DataStream& ds) const {
  if (auto result = ds.write_container(modifications); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(tile_damage); !result)
    return std::unexpected(std::move(result.error()));
  if (ds.stream_compatibility_version() >= 7)
    return ds.write(apply_damage);
  return {};
}

DamageTileGroupPacket::DamageTileGroupPacket() : layer(TileLayer::Foreground) {}

DamageTileGroupPacket::DamageTileGroupPacket(
    List<Vec2I> tile_positions, TileLayer layer, Vec2F source_position, TileDamage tile_damage, std::optional<EntityId> source_entity)
  : tile_positions(std::move(tile_positions)), layer(layer), source_position(source_position), tile_damage(std::move(tile_damage)), source_entity(std::move(source_entity)) {}

std::expected<void, StarException> DamageTileGroupPacket::read(DataStream& ds) {
  if (auto result = ds.read_container(tile_positions); !result)
    return std::unexpected(std::move(result.error()));
  return read_packet_fields(ds, layer, source_position, tile_damage, source_entity);
}

std::expected<void, StarException> DamageTileGroupPacket::write(DataStream& ds) const {
  if (auto result = ds.write_container(tile_positions); !result)
    return std::unexpected(std::move(result.error()));
  return write_packet_fields(ds, layer, source_position, tile_damage, source_entity);
}

CollectLiquidPacket::CollectLiquidPacket() = default;

CollectLiquidPacket::CollectLiquidPacket(List<Vec2I> tile_positions, LiquidId liquid_id)
  : tile_positions(std::move(tile_positions)), liquid_id(liquid_id) {}

std::expected<void, StarException> CollectLiquidPacket::read(DataStream& ds) {
  if (auto result = ds.read_container(tile_positions); !result)
    return std::unexpected(std::move(result.error()));
  return ds.read(liquid_id);
}

std::expected<void, StarException> CollectLiquidPacket::write(DataStream& ds) const {
  if (auto result = ds.write_container(tile_positions); !result)
    return std::unexpected(std::move(result.error()));
  return ds.write(liquid_id);
}

RequestDropPacket::RequestDropPacket() {
  drop_entity_id = null_entity_id;
}

RequestDropPacket::RequestDropPacket(EntityId drop_entity_id) : drop_entity_id(drop_entity_id) {}

std::expected<void, StarException> RequestDropPacket::read(DataStream& ds) {
  return ds.viread(drop_entity_id);
}

std::expected<void, StarException> RequestDropPacket::write(DataStream& ds) const {
  return ds.viwrite(drop_entity_id);
}

SpawnEntityPacket::SpawnEntityPacket() = default;

SpawnEntityPacket::SpawnEntityPacket(EntityType entity_type, ByteArray store_data, ByteArray first_net_state)
  : entity_type(entity_type), store_data(std::move(store_data)), first_net_state(std::move(first_net_state)) {}

std::expected<void, StarException> SpawnEntityPacket::read(DataStream& ds) {
  return read_packet_fields(ds, entity_type, store_data, first_net_state);
}

std::expected<void, StarException> SpawnEntityPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, entity_type, store_data, first_net_state);
}

EntityInteractPacket::EntityInteractPacket(InteractRequest interact_request, Uuid request_id)
    : interact_request(interact_request), request_id(request_id) {}

std::expected<void, StarException> EntityInteractPacket::read(DataStream& ds) {
  auto request = ds.read<InteractRequest>();
  if (!request)
    return std::unexpected(std::move(request.error()));
  auto id = ds.read<Uuid>();
  if (!id)
    return std::unexpected(std::move(id.error()));
  interact_request = *request;
  request_id = *id;
  return {};
}

std::expected<void, StarException> EntityInteractPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, interact_request, request_id);
}

EntityInteractResultPacket::EntityInteractResultPacket(InteractAction action, Uuid request_id, EntityId source_entity_id)
    : action(std::move(action)), request_id(request_id), source_entity_id(source_entity_id) {}

std::expected<void, StarException> EntityInteractResultPacket::read(DataStream& ds) {
  auto decoded = ds.read<InteractAction>();
  if (!decoded)
    return std::unexpected(std::move(decoded.error()));
  auto id = ds.read<Uuid>();
  if (!id)
    return std::unexpected(std::move(id.error()));
  auto source = ds.read<EntityId>();
  if (!source)
    return std::unexpected(std::move(source.error()));
  action = std::move(*decoded);
  request_id = *id;
  source_entity_id = *source;
  return {};
}

std::expected<void, StarException> EntityInteractResultPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, action, request_id, source_entity_id);
}

EntityCreatePacket::EntityCreatePacket() {
  entity_id = null_entity_id;
}

ServerDisconnectPacket::ServerDisconnectPacket() = default;

ServerDisconnectPacket::ServerDisconnectPacket(String reason) : reason(std::move(reason)) {}

std::expected<void, StarException> ServerDisconnectPacket::read(DataStream& ds) {
  return ds.read(reason);
}

std::expected<void, StarException> ServerDisconnectPacket::write(DataStream& ds) const {
  return ds.write(reason);
}

ConnectWirePacket::ConnectWirePacket() = default;

ConnectWirePacket::ConnectWirePacket(WireConnection output_connection, WireConnection input_connection)
  : output_connection(output_connection), input_connection(input_connection) {}

std::expected<void, StarException> ConnectWirePacket::read(DataStream& ds) {
  return read_packet_fields(ds, output_connection, input_connection);
}

std::expected<void, StarException> ConnectWirePacket::write(DataStream& ds) const {
  return write_packet_fields(ds, output_connection, input_connection);
}

DisconnectAllWiresPacket::DisconnectAllWiresPacket() = default;

DisconnectAllWiresPacket::DisconnectAllWiresPacket(Vec2I entity_position, WireNode wire_node)
  : entity_position(entity_position), wire_node(wire_node) {}

std::expected<void, StarException> DisconnectAllWiresPacket::read(DataStream& ds) {
  if (auto result = ds.viread(entity_position[0]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.viread(entity_position[1]); !result)
    return std::unexpected(std::move(result.error()));
  return ds.read(wire_node);
}

std::expected<void, StarException> DisconnectAllWiresPacket::write(DataStream& ds) const {
  if (auto result = ds.viwrite(entity_position[0]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.viwrite(entity_position[1]); !result)
    return std::unexpected(std::move(result.error()));
  return ds.write(wire_node);
}

WorldClientStateUpdatePacket::WorldClientStateUpdatePacket() = default;

WorldClientStateUpdatePacket::WorldClientStateUpdatePacket(ByteArray world_client_state_delta)
  : world_client_state_delta(std::move(world_client_state_delta)) {}

std::expected<void, StarException> WorldClientStateUpdatePacket::read(DataStream& ds) {
  return ds.read(world_client_state_delta);
}

std::expected<void, StarException> WorldClientStateUpdatePacket::write(DataStream& ds) const {
  return ds.write(world_client_state_delta);
}

FindUniqueEntityPacket::FindUniqueEntityPacket() = default;

FindUniqueEntityPacket::FindUniqueEntityPacket(String unique_entity_id)
  : unique_entity_id(std::move(unique_entity_id)) {}

std::expected<void, StarException> FindUniqueEntityPacket::read(DataStream& ds) {
  return ds.read(unique_entity_id);
}

std::expected<void, StarException> FindUniqueEntityPacket::write(DataStream& ds) const {
  return ds.write(unique_entity_id);
}

WorldStartAcknowledgePacket::WorldStartAcknowledgePacket() = default;

std::expected<void, StarException> WorldStartAcknowledgePacket::read(DataStream& ds) {
  // Packets can't be empty, read the trash data
  if (auto result = ds.read<bool>(); !result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> WorldStartAcknowledgePacket::write(DataStream& ds) const {
  // Packets can't be empty, write some trash data
  return ds.write<bool>(false);
}

PingPacket::PingPacket() = default;
PingPacket::PingPacket(std::int64_t time) : time(time) {}

std::expected<void, StarException> PingPacket::read(DataStream& ds) {
  if (ds.stream_compatibility_version() == legacy_version) {
    // Packets can't be empty, read the trash data
    if (auto result = ds.read<bool>(); !result)
      return std::unexpected(std::move(result.error()));
    time = 0;
  } else {
    if (auto result = ds.read_vlq_i(time); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> PingPacket::write(DataStream& ds) const {
  if (ds.stream_compatibility_version() == legacy_version) {
    // Packets can't be empty, write some trash data
    if (auto result = ds.write<bool>(false); !result)
      return std::unexpected(std::move(result.error()));
  } else {
    if (auto result = ds.write_vlq_i(time); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

EntityCreatePacket::EntityCreatePacket(EntityType entity_type, ByteArray store_data, ByteArray first_net_state, EntityId entity_id)
  : entity_type(entity_type), store_data(std::move(store_data)), first_net_state(std::move(first_net_state)), entity_id(entity_id) {}

std::expected<void, StarException> EntityCreatePacket::read(DataStream& ds) {
  if (auto result = read_packet_fields(ds, entity_type, store_data, first_net_state); !result)
    return std::unexpected(std::move(result.error()));
  return ds.viread(entity_id);
}

std::expected<void, StarException> EntityCreatePacket::write(DataStream& ds) const {
  if (auto result = write_packet_fields(ds, entity_type, store_data, first_net_state); !result)
    return std::unexpected(std::move(result.error()));
  return ds.viwrite(entity_id);
}

EntityUpdateSetPacket::EntityUpdateSetPacket() : EntityUpdateSetPacket(server_connection_id) {}

EntityUpdateSetPacket::EntityUpdateSetPacket(ConnectionId for_connection) : for_connection(for_connection) {}

std::expected<void, StarException> EntityUpdateSetPacket::read(DataStream& ds) {
  {
    auto result = ds.vuread(for_connection);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  auto decoded = ds.read_map_container(
    deltas, [](DataStream& ds, EntityId& entity_id, ByteArray& delta) -> std::expected<void, StarException> {
      auto id = ds.viread(entity_id);
      if (!id)
        return std::unexpected(std::move(id.error()));
      return ds.read(delta);
    });
  if (!decoded)
    return std::unexpected(std::move(decoded.error()));
  return {};
}

std::expected<void, StarException> EntityUpdateSetPacket::write(DataStream& ds) const {
  {
    auto result = ds.vuwrite(for_connection);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  auto encoded = ds.write_map_container(
    deltas, [](DataStream& ds, EntityId const& entity_id, ByteArray const& delta) -> std::expected<void, StarException> {
      auto id = ds.viwrite(entity_id);
      if (!id)
        return std::unexpected(std::move(id.error()));
      return ds.write(delta);
    });
  if (!encoded)
    return std::unexpected(std::move(encoded.error()));
  return {};
}

EntityDestroyPacket::EntityDestroyPacket() {
  entity_id = null_entity_id;
  death = false;
}

EntityDestroyPacket::EntityDestroyPacket(EntityId entity_id, ByteArray final_net_state, bool death)
  : entity_id(entity_id), final_net_state(std::move(final_net_state)), death(death) {}

std::expected<void, StarException> EntityDestroyPacket::read(DataStream& ds) {
  if (auto result = ds.viread(entity_id); !result)
    return std::unexpected(std::move(result.error()));
  return read_packet_fields(ds, final_net_state, death);
}

std::expected<void, StarException> EntityDestroyPacket::write(DataStream& ds) const {
  if (auto result = ds.viwrite(entity_id); !result)
    return std::unexpected(std::move(result.error()));
  return write_packet_fields(ds, final_net_state, death);
}

HitRequestPacket::HitRequestPacket() = default;

HitRequestPacket::HitRequestPacket(RemoteHitRequest remote_hit_request) : remote_hit_request(std::move(remote_hit_request)) {}

std::expected<void, StarException> HitRequestPacket::read(DataStream& ds) {
  return ds.read(remote_hit_request);
}

std::expected<void, StarException> HitRequestPacket::write(DataStream& ds) const {
  return ds.write(remote_hit_request);
}

DamageRequestPacket::DamageRequestPacket() = default;

DamageRequestPacket::DamageRequestPacket(RemoteDamageRequest remote_damage_request)
  : remote_damage_request(std::move(remote_damage_request)) {}

std::expected<void, StarException> DamageRequestPacket::read(DataStream& ds) {
  return ds.read(remote_damage_request);
}

std::expected<void, StarException> DamageRequestPacket::write(DataStream& ds) const {
  return ds.write(remote_damage_request);
}

DamageNotificationPacket::DamageNotificationPacket() = default;

DamageNotificationPacket::DamageNotificationPacket(RemoteDamageNotification remote_damage_notification)
  : remote_damage_notification(std::move(remote_damage_notification)) {}

std::expected<void, StarException> DamageNotificationPacket::read(DataStream& ds) {
  return ds.read(remote_damage_notification);
}

std::expected<void, StarException> DamageNotificationPacket::write(DataStream& ds) const {
  return ds.write(remote_damage_notification);
}

EntityMessagePacket::EntityMessagePacket() : uuid(unfilled_packet_uuid()) {}

EntityMessagePacket::EntityMessagePacket(std::variant<EntityId, String> entity_id, String message, JsonArray args,
                                         Uuid uuid, ConnectionId from_connection)
    : entity_id(std::move(entity_id)), message(std::move(message)), args(std::move(args)), uuid(uuid),
      from_connection(from_connection) {}

std::expected<void, StarException> EntityMessagePacket::read(DataStream& ds) {
  return read_packet_fields(ds, entity_id, message, args, uuid, from_connection);
}

std::expected<void, StarException> EntityMessagePacket::write(DataStream& ds) const {
  return write_packet_fields(ds, entity_id, message, args, uuid, from_connection);
}

std::expected<void, StarException> EntityMessagePacket::read_json(Json const& json) {
  auto json_entity_id = json.get("entityId");
  if (!json_entity_id)
    return std::unexpected(std::move(json_entity_id.error()));
  if (json_entity_id->can_convert(Json::Type::Int)) {
    auto id = json_entity_id->to_int();
    if (!id)
      return std::unexpected(std::move(id.error()));
    entity_id = static_cast<EntityId>(*id);
  } else {
    auto id = json_entity_id->to_string();
    if (!id)
      return std::unexpected(std::move(id.error()));
    entity_id = std::move(*id);
  }
  auto decoded_message = json.get_string("message");
  if (!decoded_message)
    return std::unexpected(std::move(decoded_message.error()));
  message = std::move(*decoded_message);
  auto decoded_args = json.get_array("args");
  if (!decoded_args)
    return std::unexpected(std::move(decoded_args.error()));
  args = std::move(*decoded_args);
  auto uuid_string = json.get_string("uuid");
  if (!uuid_string)
    return std::unexpected(std::move(uuid_string.error()));
  auto decoded_uuid = Uuid::from_hex((*uuid_string).utf8());
  if (!decoded_uuid)
    return std::unexpected(std::move(decoded_uuid.error()));
  uuid = std::move(*decoded_uuid);
  auto from = json.get_uint("fromConnection");
  if (!from)
    return std::unexpected(std::move(from.error()));
  from_connection = *from;
  return {};
}

std::expected<Json, StarException> EntityMessagePacket::write_json() const {
  return JsonObject{
    {"entityId", std::holds_alternative<EntityId>(entity_id) ? Json(std::get<EntityId>(entity_id)) : Json(std::get<String>(entity_id))},
    {"message", message},
    {"args", args},
    {"uuid", uuid.hex()},
    {"fromConnection", from_connection}
  };
}

EntityMessageResponsePacket::EntityMessageResponsePacket()
    : response(std::unexpected(String{})), uuid(unfilled_packet_uuid()) {}

EntityMessageResponsePacket::EntityMessageResponsePacket(std::expected<Json, String> response, Uuid uuid)
    : response(std::move(response)), uuid(uuid) {}

std::expected<void, StarException> EntityMessageResponsePacket::read(DataStream& ds) {
  if (auto decoded = read_message_response(ds, response); !decoded)
    return std::unexpected(std::move(decoded.error()));
  return ds.read(uuid);
}

std::expected<void, StarException> EntityMessageResponsePacket::write(DataStream& ds) const {
  if (auto encoded = write_message_response(ds, response); !encoded)
    return std::unexpected(std::move(encoded.error()));
  return ds.write(uuid);
}

UpdateWorldPropertiesPacket::UpdateWorldPropertiesPacket() = default;

UpdateWorldPropertiesPacket::UpdateWorldPropertiesPacket(JsonObject const& updated_properties)
  : updated_properties(updated_properties) {}

std::expected<void, StarException> UpdateWorldPropertiesPacket::read(DataStream& ds) {
  return ds.read_map_container(updated_properties);
}

std::expected<void, StarException> UpdateWorldPropertiesPacket::write(DataStream& ds) const {
  return ds.write_map_container(updated_properties);
}


std::expected<void, StarException> UpdateWorldPropertiesPacket::read_json(Json const& json) {
  {
    auto result = json.get_object("updatedProperties");
    if (!result)
      return std::unexpected(std::move(result.error()));
    updated_properties = std::move(*result);
  }
  return {};
}

std::expected<Json, StarException> UpdateWorldPropertiesPacket::write_json() const {
  return JsonObject{
    {"updatedProperties", updated_properties},
  };
}

UpdateTileProtectionPacket::UpdateTileProtectionPacket() = default;

UpdateTileProtectionPacket::UpdateTileProtectionPacket(DungeonId dungeon_id, bool is_protected)
  : dungeon_id(dungeon_id), is_protected(is_protected) {}

std::expected<void, StarException> UpdateTileProtectionPacket::read(DataStream& ds) {
  return read_packet_fields(ds, dungeon_id, is_protected);
}

std::expected<void, StarException> UpdateTileProtectionPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, dungeon_id, is_protected);
}

std::expected<void, StarException> UpdateTileProtectionPacket::read_json(Json const& json) {
  {
    auto result = json.get_uint("dungeonId");
    if (!result)
      return std::unexpected(std::move(result.error()));
    dungeon_id = std::move(*result);
  }
  {
    auto result = json.get_bool("isProtected");
    if (!result)
      return std::unexpected(std::move(result.error()));
    is_protected = std::move(*result);
  }
  return {};
}

std::expected<Json, StarException> UpdateTileProtectionPacket::write_json() const {
  return JsonObject{
    {"dungeonId", dungeon_id},
    {"isProtected", is_protected}
  };
}

SetDungeonGravityPacket::SetDungeonGravityPacket() = default;

SetDungeonGravityPacket::SetDungeonGravityPacket(DungeonId dungeon_id, std::optional<float> gravity)
  : dungeon_id(std::move(dungeon_id)), gravity(std::move(gravity)) {}

std::expected<void, StarException> SetDungeonGravityPacket::read(DataStream& ds) {
  return read_packet_fields(ds, dungeon_id, gravity);
}

std::expected<void, StarException> SetDungeonGravityPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, dungeon_id, gravity);
}

std::expected<void, StarException> SetDungeonGravityPacket::read_json(Json const& json) {
  {
    auto result = json.get_uint("dungeonId");
    if (!result)
      return std::unexpected(std::move(result.error()));
    dungeon_id = std::move(*result);
  }
  {
    auto result = json.opt_float("gravity");
    if (!result)
      return std::unexpected(std::move(result.error()));
    gravity = std::move(*result);
  }
  return {};
}

std::expected<Json, StarException> SetDungeonGravityPacket::write_json() const {
  return JsonObject{
    {"dungeonId", dungeon_id},
    {"gravity", json_from_optional<float>(gravity)}
  };
}

SetDungeonBreathablePacket::SetDungeonBreathablePacket() = default;

SetDungeonBreathablePacket::SetDungeonBreathablePacket(DungeonId dungeon_id, std::optional<bool> breathable)
  : dungeon_id(std::move(dungeon_id)), breathable(std::move(breathable)) {}

std::expected<void, StarException> SetDungeonBreathablePacket::read(DataStream& ds) {
  return read_packet_fields(ds, dungeon_id, breathable);
}

std::expected<void, StarException> SetDungeonBreathablePacket::write(DataStream& ds) const {
  return write_packet_fields(ds, dungeon_id, breathable);
}

std::expected<void, StarException> SetDungeonBreathablePacket::read_json(Json const& json) {
  {
    auto result = json.get_uint("dungeonId");
    if (!result)
      return std::unexpected(std::move(result.error()));
    dungeon_id = std::move(*result);
  }
  {
    auto result = json.opt_bool("breathable");
    if (!result)
      return std::unexpected(std::move(result.error()));
    breathable = std::move(*result);
  }
  return {};
}

std::expected<Json, StarException> SetDungeonBreathablePacket::write_json() const {
  return JsonObject{
    {"dungeonId", dungeon_id},
    {"breathable", json_from_optional<bool>(breathable)}
  };
}

SetPlayerStartPacket::SetPlayerStartPacket() = default;

SetPlayerStartPacket::SetPlayerStartPacket(Vec2F player_start, bool respawn_in_world) : player_start(player_start), respawn_in_world(respawn_in_world) {}

std::expected<void, StarException> SetPlayerStartPacket::read(DataStream& ds) {
  return read_packet_fields(ds, player_start, respawn_in_world);
}

std::expected<void, StarException> SetPlayerStartPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, player_start, respawn_in_world);
}

std::expected<void, StarException> SetPlayerStartPacket::read_json(Json const& json) {
  auto start_data = json.get("playerStart");
  if (!start_data)
    return std::unexpected(std::move(start_data.error()));
  auto start = json_to_vec2f(*start_data);
  if (!start)
    return std::unexpected(std::move(start.error()));
  player_start = *start;
  {
    auto result = json.get_bool("respawnInWorld");
    if (!result)
      return std::unexpected(std::move(result.error()));
    respawn_in_world = std::move(*result);
  }
  return {};
}

std::expected<Json, StarException> SetPlayerStartPacket::write_json() const {
  return JsonObject{
    {"playerStart", json_from_vec2f(player_start)},
    {"respawnInWorld", respawn_in_world}
  };
}

FindUniqueEntityResponsePacket::FindUniqueEntityResponsePacket() = default;

FindUniqueEntityResponsePacket::FindUniqueEntityResponsePacket(String unique_entity_id, std::optional<Vec2F> entity_position)
  : unique_entity_id(std::move(unique_entity_id)), entity_position(std::move(entity_position)) {}

std::expected<void, StarException> FindUniqueEntityResponsePacket::read(DataStream& ds) {
  return read_packet_fields(ds, unique_entity_id, entity_position);
}

std::expected<void, StarException> FindUniqueEntityResponsePacket::write(DataStream& ds) const {
  return write_packet_fields(ds, unique_entity_id, entity_position);
}

PongPacket::PongPacket() = default;
PongPacket::PongPacket(std::int64_t time) : time(time) {}


std::expected<void, StarException> PongPacket::read(DataStream& ds) {
  if (ds.stream_compatibility_version() == legacy_version) {
    // Packets can't be empty, read the trash data
    if (auto result = ds.read<bool>(); !result)
      return std::unexpected(std::move(result.error()));
    time = 0;
  } else {
    if (auto result = ds.read_vlq_i(time); !result)
      return std::unexpected(std::move(result.error()));
  }

  return {};
}

std::expected<void, StarException> PongPacket::write(DataStream& ds) const {
  if (ds.stream_compatibility_version() == legacy_version) {
    // Packets can't be empty, write some trash data
    if (auto result = ds.write<bool>(false); !result)
      return std::unexpected(std::move(result.error()));
  } else {
    if (auto result = ds.write_vlq_i(time); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

namespace {
constexpr double legacy_steps_per_second = 60.0;
}

StepUpdatePacket::StepUpdatePacket(double remote_time) : remote_time(remote_time) {}

std::expected<void, StarException> StepUpdatePacket::read(DataStream& ds) {
  if (ds.stream_compatibility_version() == legacy_version) {
    auto steps = ds.read_vlq_u();
    if (!steps)
      return std::unexpected(std::move(steps.error()));
    remote_time = static_cast<double>(*steps) / legacy_steps_per_second;
  } else {
    auto seconds = ds.read<double>();
    if (!seconds)
      return std::unexpected(std::move(seconds.error()));
    if (!is_finite(*seconds))
      return std::unexpected(StarPacketException("StepUpdate time must be finite"));
    remote_time = *seconds;
  }
  return {};
}

std::expected<void, StarException> StepUpdatePacket::write(DataStream& ds) const {
  if (!is_finite(remote_time))
    return std::unexpected(StarPacketException("StepUpdate time must be finite"));
  if (ds.stream_compatibility_version() != legacy_version)
    return ds.write(remote_time);

  // This binary64 boundary scales to 2^64, while its predecessor scales
  // to 2^64 - 4096. Check before multiplication and the unsigned cast.
  if (remote_time < 0.0 || remote_time >= 0x1p64 / legacy_steps_per_second)
    return std::unexpected(StarPacketException("StepUpdate time is outside the legacy step range"));
  return ds.vuwrite(static_cast<std::uint64_t>(std::round(remote_time * legacy_steps_per_second)));
}

SystemWorldStartPacket::SystemWorldStartPacket() : client_ship(unfilled_packet_uuid(), SystemLocation()) {}

SystemWorldStartPacket::SystemWorldStartPacket(Vec3I location, List<ByteArray> object_stores, List<ByteArray> ship_stores, std::pair<Uuid, SystemLocation> client_ship)
  : location(std::move(location)), object_stores(std::move(object_stores)), ship_stores(std::move(ship_stores)), client_ship(std::move(client_ship)) {}

std::expected<void, StarException> SystemWorldStartPacket::read(DataStream& ds) {
  return read_packet_fields(ds, location, object_stores, ship_stores, client_ship);
}

std::expected<void, StarException> SystemWorldStartPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, location, object_stores, ship_stores, client_ship);
}

SystemWorldUpdatePacket::SystemWorldUpdatePacket() = default;

SystemWorldUpdatePacket::SystemWorldUpdatePacket(HashMap<Uuid, ByteArray> object_updates, HashMap<Uuid, ByteArray> ship_updates)
  : object_updates(std::move(object_updates)), ship_updates(std::move(ship_updates)) {}

std::expected<void, StarException> SystemWorldUpdatePacket::read(DataStream& ds) {
  return read_packet_fields(ds, object_updates, ship_updates);
}

std::expected<void, StarException> SystemWorldUpdatePacket::write(DataStream& ds) const {
  return write_packet_fields(ds, object_updates, ship_updates);
}

SystemObjectCreatePacket::SystemObjectCreatePacket() = default;

SystemObjectCreatePacket::SystemObjectCreatePacket(ByteArray object_store) : object_store(std::move(object_store)) {}

std::expected<void, StarException> SystemObjectCreatePacket::read(DataStream& ds) {
  return ds.read(object_store);
}

std::expected<void, StarException> SystemObjectCreatePacket::write(DataStream& ds) const {
  return ds.write(object_store);
}

SystemObjectDestroyPacket::SystemObjectDestroyPacket() : object_uuid(unfilled_packet_uuid()) {}

SystemObjectDestroyPacket::SystemObjectDestroyPacket(Uuid object_uuid) : object_uuid(std::move(object_uuid)) {}

std::expected<void, StarException> SystemObjectDestroyPacket::read(DataStream& ds) {
  return ds.read(object_uuid);
}

std::expected<void, StarException> SystemObjectDestroyPacket::write(DataStream& ds) const {
  return ds.write(object_uuid);
}

SystemShipCreatePacket::SystemShipCreatePacket() = default;

SystemShipCreatePacket::SystemShipCreatePacket(ByteArray ship_store) : ship_store(std::move(ship_store)) {}

std::expected<void, StarException> SystemShipCreatePacket::read(DataStream& ds) {
  return ds.read(ship_store);
}

std::expected<void, StarException> SystemShipCreatePacket::write(DataStream& ds) const {
  return ds.write(ship_store);
}

SystemShipDestroyPacket::SystemShipDestroyPacket() : ship_uuid(unfilled_packet_uuid()) {}

SystemShipDestroyPacket::SystemShipDestroyPacket(Uuid ship_uuid) : ship_uuid(std::move(ship_uuid)) {}

std::expected<void, StarException> SystemShipDestroyPacket::read(DataStream& ds) {
  return ds.read(ship_uuid);
}

std::expected<void, StarException> SystemShipDestroyPacket::write(DataStream& ds) const {
  return ds.write(ship_uuid);
}

SystemObjectSpawnPacket::SystemObjectSpawnPacket() : uuid(unfilled_packet_uuid()) {}

SystemObjectSpawnPacket::SystemObjectSpawnPacket(String type_name, Uuid uuid, std::optional<Vec2F> position, JsonObject parameters)
  : type_name(std::move(type_name)), uuid(std::move(uuid)), position(std::move(position)), parameters(std::move(parameters)) {}

std::expected<void, StarException> SystemObjectSpawnPacket::read(DataStream& ds) {
  return read_packet_fields(ds, type_name, uuid, position, parameters);
}

std::expected<void, StarException> SystemObjectSpawnPacket::write(DataStream& ds) const {
  return write_packet_fields(ds, type_name, uuid, position, parameters);
}

UpdateWorldTemplatePacket::UpdateWorldTemplatePacket() = default;

UpdateWorldTemplatePacket::UpdateWorldTemplatePacket(Json template_data) : template_data(std::move(template_data)) {}

std::expected<void, StarException> UpdateWorldTemplatePacket::read(DataStream& ds) {
  return ds.read(template_data);
}

std::expected<void, StarException> UpdateWorldTemplatePacket::write(DataStream& ds) const {
  return ds.write(template_data);
}

ClientCustomWorldRequest::ClientCustomWorldRequest() = default;

ClientCustomWorldRequest::ClientCustomWorldRequest(String name) : name(std::move(name)) {}

std::expected<void, StarException> ClientCustomWorldRequest::read(DataStream& ds) {
  return ds.read(name);
}

std::expected<void, StarException> ClientCustomWorldRequest::write(DataStream& ds) const {
  return ds.write(name);
}

ClientCustomWorldResponse::ClientCustomWorldResponse() = default;

ClientCustomWorldResponse::ClientCustomWorldResponse(String name, WorldChunks chunks) : name(std::move(name)), chunks(std::move(chunks)) {}

std::expected<void, StarException> ClientCustomWorldResponse::read(DataStream& ds) {
  return read_packet_fields(ds, name, chunks);
}

std::expected<void, StarException> ClientCustomWorldResponse::write(DataStream& ds) const {
  return write_packet_fields(ds, name, chunks);
}

ClientCustomWorldCreate::ClientCustomWorldCreate() = default;

ClientCustomWorldCreate::ClientCustomWorldCreate(String name, Json template_data) : name(std::move(name)), template_data(std::move(template_data)) {}

std::expected<void, StarException> ClientCustomWorldCreate::read(DataStream& ds) {
  return read_packet_fields(ds, name, template_data);
}

std::expected<void, StarException> ClientCustomWorldCreate::write(DataStream& ds) const {
  return write_packet_fields(ds, name, template_data);
}

ClientSubWorldPackets::ClientSubWorldPackets() = default;

ClientSubWorldPackets::ClientSubWorldPackets(ClientSubWorldId const& sub_world_id, List<std::shared_ptr<Packet>> packets) : sub_world_id(sub_world_id), packets(std::move(packets)) {}

std::expected<void, StarException> ClientSubWorldPackets::read(DataStream& ds) {
  auto decoded_id = ds.read<ClientSubWorldId>();
  if (!decoded_id)
    return std::unexpected(std::move(decoded_id.error()));
  auto count = ds.read<std::uint32_t>();
  if (!count)
    return std::unexpected(std::move(count.error()));
  List<std::shared_ptr<Packet>> decoded;
  if (auto buffered = as_ref<DataStreamExternalBuffer>(ds)) {
    auto available = buffered->get().remaining();
    if (!available)
      return std::unexpected(std::move(available.error()));
    // Every child has at least a type byte. Never allocate from count alone.
    if (*count > *available)
      return std::unexpected(StarPacketException("Subworld packet count exceeds remaining bytes"));
    decoded.reserve(std::min<std::size_t>(*count, *available / sizeof(std::shared_ptr<Packet>)));
  }
  for (std::uint32_t i = 0; i < *count; ++i) {
    auto packet_type = ds.read<PacketType>();
    if (!packet_type)
      return std::unexpected(std::move(packet_type.error()));
    auto packet = create_packet(*packet_type);
    if (!packet)
      return std::unexpected(std::move(packet.error()));
    if (auto result = (*packet)->read(ds); !result)
      return std::unexpected(std::move(result.error()));
    decoded.push_back(std::move(*packet));
  }
  sub_world_id = *decoded_id;
  packets = std::move(decoded);
  return {};
}

std::expected<void, StarException> ClientSubWorldPackets::write(DataStream& ds) const {
  if (!std::in_range<std::uint32_t>(packets.size()))
    return std::unexpected(StarPacketException("Subworld packet count exceeds wire range"));
  if (auto result = ds.write(sub_world_id); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(static_cast<std::uint32_t>(packets.size())); !result)
    return std::unexpected(std::move(result.error()));
  for (auto const& packet : packets) {
    if (auto result = ds.write(packet->type()); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = packet->write(ds); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

ClientSubWorldRequest::ClientSubWorldRequest() = default;

ClientSubWorldRequest::ClientSubWorldRequest(ClientSubWorldId const& sub_world_id, WorldId world_id) : sub_world_id(sub_world_id), world_id(std::move(world_id)) {}

std::expected<void, StarException> ClientSubWorldRequest::read(DataStream& ds) {
  return read_packet_fields(ds, sub_world_id, world_id);
}

std::expected<void, StarException> ClientSubWorldRequest::write(DataStream& ds) const {
  return write_packet_fields(ds, sub_world_id, world_id);
}

ClientSubWorldReject::ClientSubWorldReject() = default;

ClientSubWorldReject::ClientSubWorldReject(ClientSubWorldId const& sub_world_id) : sub_world_id(sub_world_id) {}

std::expected<void, StarException> ClientSubWorldReject::read(DataStream& ds) {
  return ds.read(sub_world_id);
}

std::expected<void, StarException> ClientSubWorldReject::write(DataStream& ds) const {
  return ds.write(sub_world_id);
}

NotifyWorldLoad::NotifyWorldLoad() = default;

NotifyWorldLoad::NotifyWorldLoad(WorldId world_id) : world_id(std::move(world_id)) {}

std::expected<void, StarException> NotifyWorldLoad::read(DataStream& ds) {
  return ds.read(world_id);
}

std::expected<void, StarException> NotifyWorldLoad::write(DataStream& ds) const {
  return ds.write(world_id);
}

LogMapUpdate::LogMapUpdate() = default;

LogMapUpdate::LogMapUpdate(Map<String,String> map) : map(std::move(map)) {}

std::expected<void, StarException> LogMapUpdate::read(DataStream& ds) {
  return ds.read(map);
}

std::expected<void, StarException> LogMapUpdate::write(DataStream& ds) const {
  return ds.write(map);
}

UniverseMessage::UniverseMessage() : uuid(unfilled_packet_uuid()) {}

UniverseMessage::UniverseMessage(ConnectionId connection, String message, JsonArray args, Uuid uuid, ConnectionId from_connection)
  : connection(connection), message(std::move(message)), args(std::move(args)), uuid(uuid), from_connection(from_connection) {}

std::expected<void, StarException> UniverseMessage::read(DataStream& ds) {
  return read_packet_fields(ds, connection, message, args, uuid, from_connection);
}

std::expected<void, StarException> UniverseMessage::write(DataStream& ds) const {
  return write_packet_fields(ds, connection, message, args, uuid, from_connection);
}

std::expected<void, StarException> UniverseMessage::read_json(Json const& json) {
  {
    auto result = json.get_uint("connection");
    if (!result)
      return std::unexpected(std::move(result.error()));
    connection = std::move(*result);
  }
  {
    auto result = json.get_string("message");
    if (!result)
      return std::unexpected(std::move(result.error()));
    message = std::move(*result);
  }
  {
    auto result = json.get_array("args");
    if (!result)
      return std::unexpected(std::move(result.error()));
    args = std::move(*result);
  }
  auto uuid_string = json.get_string("uuid");
  if (!uuid_string)
    return std::unexpected(std::move(uuid_string.error()));
  auto decoded_uuid = Uuid::from_hex((*uuid_string).utf8());
  if (!decoded_uuid)
    return std::unexpected(std::move(decoded_uuid.error()));
  uuid = std::move(*decoded_uuid);
  {
    auto result = json.get_uint("fromConnection");
    if (!result)
      return std::unexpected(std::move(result.error()));
    from_connection = std::move(*result);
  }
  return {};
}

std::expected<Json, StarException> UniverseMessage::write_json() const {
  return JsonObject{
    {"connection", connection},
    {"message", message},
    {"args", args},
    {"uuid", uuid.hex()},
    {"fromConnection", from_connection}
  };
}

UniverseMessageResponse::UniverseMessageResponse() : response(std::unexpected(String{})), uuid(unfilled_packet_uuid()) {}

UniverseMessageResponse::UniverseMessageResponse(std::expected<Json, String> response, Uuid uuid)
    : response(std::move(response)), uuid(uuid) {}

std::expected<void, StarException> UniverseMessageResponse::read(DataStream& ds) {
  if (auto decoded = read_message_response(ds, response); !decoded)
    return std::unexpected(std::move(decoded.error()));
  return ds.read(uuid);
}

std::expected<void, StarException> UniverseMessageResponse::write(DataStream& ds) const {
  if (auto encoded = write_message_response(ds, response); !encoded)
    return std::unexpected(std::move(encoded.error()));
  return ds.write(uuid);
}

}
