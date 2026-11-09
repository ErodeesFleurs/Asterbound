import star.net_element;
import star.format;
import star.json_extra;
import star.json;
import star.xx_hash;
import star.json_rpc;
import star.game_types;
import star.data_stream;
import star.strong_typedef;
import star.id_map;
import star.set;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.data_stream_devices;
import star.thread;
import star.btree_database;
import star.casting;
import star.ordered_set;
import star.list;
import star.string;
import star.version;
import star.rpc_promise;
import star.bi_map;
import star.random;
import star.perlin;
import star.weighted_pool;
import std;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.either;
import star.data_stream_extra;

import star.lua_root;
import star.damage_types;
import star.celestial_coordinate;
import star.uuid;
import star.warping;
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
import star.versioning_database;
import star.world_storage;
import star.player_types;

import star.client_context;
import star.root;

import star.net_compatibility;
import star.byte_array;
import star.algorithm;
namespace Star {

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ShipUpgrades& upgrades) {
  auto level = ds.read(upgrades.shipLevel);
  if (!level)
    return std::unexpected(std::move(level.error()));
  auto fuel = ds.read(upgrades.maxFuel);
  if (!fuel)
    return std::unexpected(std::move(fuel.error()));
  auto crew = ds.read(upgrades.crewSize);
  if (!crew)
    return std::unexpected(std::move(crew.error()));
  auto efficiency = ds.read(upgrades.fuelEfficiency);
  if (!efficiency)
    return std::unexpected(std::move(efficiency.error()));
  auto speed = ds.read(upgrades.shipSpeed);
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  auto capabilities = ds.read(upgrades.capabilities);
  if (!capabilities)
    return std::unexpected(std::move(capabilities.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ShipUpgrades const& upgrades) {
  auto level = ds.write(upgrades.shipLevel);
  if (!level)
    return std::unexpected(std::move(level.error()));
  auto fuel = ds.write(upgrades.maxFuel);
  if (!fuel)
    return std::unexpected(std::move(fuel.error()));
  auto crew = ds.write(upgrades.crewSize);
  if (!crew)
    return std::unexpected(std::move(crew.error()));
  auto efficiency = ds.write(upgrades.fuelEfficiency);
  if (!efficiency)
    return std::unexpected(std::move(efficiency.error()));
  auto speed = ds.write(upgrades.shipSpeed);
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  auto capabilities = ds.write(upgrades.capabilities);
  if (!capabilities)
    return std::unexpected(std::move(capabilities.error()));
  return std::ref(ds);
}

ClientContext::ClientContext(Root& root, Uuid serverUuid, Uuid playerUuid)
  : m_root(root), m_serverUuid(root.random()), m_playerUuid(root.random()) {
  m_serverUuid = std::move(serverUuid);
  m_playerUuid = std::move(playerUuid);
  m_rpc = std::make_shared<JsonRpc>(m_root.logger());

}

std::expected<std::shared_ptr<ClientContext>, StarException> ClientContext::create(Root& root, Uuid serverUuid, Uuid playerUuid) {
  auto context = std::shared_ptr<ClientContext>(new ClientContext(root, std::move(serverUuid), std::move(playerUuid)));
  auto initialized = context->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return context;
}

std::expected<void, StarException> ClientContext::initialize() {
  for (auto element : std::initializer_list<std::reference_wrapper<NetElement>>{
      m_orbitWarpActionNetState, m_playerWorldIdNetState, m_isAdminNetState,
      m_teamNetState, m_shipUpgrades, m_shipCoordinate}) {
    auto added = m_netGroup.addNetElement(element);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return {};
}

Uuid ClientContext::serverUuid() const {
  return m_serverUuid;
}

Uuid ClientContext::playerUuid() const {
  return m_playerUuid;
}

CelestialCoordinate ClientContext::shipCoordinate() const {
  return m_shipCoordinate.get();
}

std::optional<std::pair<WarpAction, WarpMode>> ClientContext::orbitWarpAction() const {
  return m_orbitWarpActionNetState.get();
}

WorldId ClientContext::playerWorldId() const {
  return m_playerWorldIdNetState.get();
}

bool ClientContext::isAdmin() const {
  return m_isAdminNetState.get();
}

EntityDamageTeam ClientContext::team() const {
  return m_teamNetState.get();
}

std::shared_ptr<JsonRpcInterface> ClientContext::rpcInterface() const {
  return m_rpc;
}

WorldChunks ClientContext::newShipUpdates() { return std::exchange(m_newShipUpdates, {}); }

StringMap<WorldChunks> ClientContext::newCustomWorldUpdates() { return std::exchange(m_newCustomWorldUpdates, {}); }

ShipUpgrades ClientContext::shipUpgrades() const {
  return m_shipUpgrades.get();
}

std::expected<void, StarException> ClientContext::readUpdate(ByteArray data, NetCompatibilityRules rules) {
  if (data.empty())
    return {};
  DataStreamBuffer ds(std::move(data));
  ds.set_stream_compatibility_version(rules);
  auto rpcData = ds.read<ByteArray>();
  if (!rpcData)
    return std::unexpected(std::move(rpcData.error()));
  auto received = m_rpc->receive(*rpcData);
  if (!received)
    return std::unexpected(std::move(received.error()));
  auto shipUpdates = ds.read<ByteArray>();
  if (!shipUpdates)
    return std::unexpected(std::move(shipUpdates.error()));
  if (!shipUpdates->empty()) {
    auto decoded = DataStreamBuffer::deserialize<WorldChunks>(shipUpdates->span());
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    m_newShipUpdates.merge(*decoded, true);
  }
  if (rules.version() >= 15) {
    auto customWorldUpdates = ds.read<StringMap<ByteArray>>();
    if (!customWorldUpdates)
      return std::unexpected(std::move(customWorldUpdates.error()));
    for (auto&& p : *customWorldUpdates) {
      auto decoded = DataStreamBuffer::deserialize<WorldChunks>(p.second.span());
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      m_newCustomWorldUpdates[p.first].merge(*decoded, true);
    }
  }
  auto netData = ds.read<ByteArray>();
  if (!netData)
    return std::unexpected(std::move(netData.error()));
  return m_netGroup.readNetState(std::move(*netData), 0.0f, rules);
}

std::expected<ByteArray, StarException> ClientContext::writeUpdate(NetCompatibilityRules) {
  return m_rpc->send();
}

void ClientContext::setConnectionId(ConnectionId connectionId) {
  m_connectionId = connectionId;
}

ConnectionId ClientContext::connectionId() const {
  return m_connectionId;
}

void ClientContext::setNetCompatibilityRules(NetCompatibilityRules netCompatibilityRules) {
  m_netCompatibilityRules = netCompatibilityRules;
}

NetCompatibilityRules ClientContext::netCompatibilityRules() const {
  return m_netCompatibilityRules;
}

}
