import std;
import star.algorithm;
import star.byte_array;
import star.celestial_coordinate;
import star.client_context;
import star.context_net_state;
import star.damage_types;
import star.data_stream;
import star.data_stream_devices;
import star.format;
import star.game_types;
import star.json_rpc;
import star.logging;
import star.net_compatibility;
import star.player_types;
import star.string;
import star.uuid;
import star.warping;
import star.world_storage;

namespace Star {

namespace {

// All subframes borrow the packet. Validate the length against the actual
// remaining input before advancing or allocating storage for a decoded value.
std::expected<std::span<char const>, StarException> read_frame(DataStreamReader& stream) {
  auto size = stream.read_vlq_u();
  if (!size)
    return std::unexpected(std::move(size.error()));
  auto pos = stream.pos();
  if (!pos)
    return std::unexpected(std::move(pos.error()));
  auto rest = stream.span().subspan(*pos);
  if (*size > rest.size())
    return std::unexpected(DataStreamException("Truncated client context frame"));
  auto frame = rest.first(static_cast<std::size_t>(*size));
  if (auto advanced = stream.seek(*pos + frame.size()); !advanced)
    return std::unexpected(std::move(advanced.error()));
  return frame;
}

std::expected<void, StarException> require_end(DataStreamReader& stream) {
  auto end = stream.at_end();
  if (!end)
    return std::unexpected(std::move(end.error()));
  if (!*end)
    return std::unexpected(DataStreamException("Trailing bytes in client context update"));
  return {};
}

std::expected<void, StarException> read_chunks(std::span<char const> frame, WorldChunks& chunks) {
  DataStreamReader stream(frame);
  auto decoded = stream.read_map_container(chunks, [&](DataStream&, ByteArray& key, std::optional<ByteArray>& value)
      -> std::expected<void, StarException> {
    auto key_bytes = read_frame(stream);
    if (!key_bytes)
      return std::unexpected(std::move(key_bytes.error()));
    auto present = stream.read<bool>();
    if (!present)
      return std::unexpected(std::move(present.error()));
    if (*present) {
      auto bytes = read_frame(stream);
      if (!bytes)
        return std::unexpected(std::move(bytes.error()));
      value.emplace(*bytes);
    }
    key = ByteArray(*key_bytes);
    return {};
  });
  if (!decoded)
    return decoded;
  return require_end(stream);
}

void merge_chunks(WorldChunks& pending, WorldChunks&& decoded) {
  if (decoded.empty())
    return;
  if (pending.empty()) {
    pending = std::move(decoded);
    return;
  }
  for (auto&& [key, value] : decoded)
    pending.insert_or_assign(key, std::move(value));
}

}

ClientContext::ClientContext(ConstructionKey, Logger& logger, Uuid server_uuid, Uuid player_uuid,
    ConnectionId connection_id, NetCompatibilityRules rules)
  : m_server_uuid(std::move(server_uuid)), m_player_uuid(std::move(player_uuid)),
    m_connection_id(connection_id), m_net_compatibility_rules(rules), m_rpc(std::make_shared<JsonRpc>(logger)) {}

std::expected<std::shared_ptr<ClientContext>, StarException> ClientContext::create(
    Logger& logger, Uuid server_uuid, Uuid player_uuid, ConnectionId connection_id, NetCompatibilityRules rules) {
  auto context = std::make_shared<ClientContext>(ConstructionKey{}, logger,
      std::move(server_uuid), std::move(player_uuid), connection_id, rules);
  if (auto initialized = context->m_staged_state.initialize(); !initialized)
    return std::unexpected(std::move(initialized.error()));
  return context;
}

Uuid ClientContext::server_uuid() const { return m_server_uuid; }
Uuid ClientContext::player_uuid() const { return m_player_uuid; }
ConnectionId ClientContext::connection_id() const { return m_connection_id; }
NetCompatibilityRules ClientContext::net_compatibility_rules() const { return m_net_compatibility_rules; }

CelestialCoordinate ClientContext::ship_coordinate() const { return m_net_state.ship_coordinate.get(); }
std::optional<std::pair<WarpAction, WarpMode>> ClientContext::orbit_warp_action() const {
  return m_net_state.orbit_warp_action.get();
}
WorldId ClientContext::player_world_id() const { return m_net_state.player_world_id.get(); }
bool ClientContext::is_admin() const { return m_net_state.is_admin.get(); }
EntityDamageTeam ClientContext::team() const { return m_net_state.team.get(); }
ShipUpgrades const& ClientContext::ship_upgrades() const { return m_net_state.ship_upgrades.get(); }
unsigned ClientContext::ship_level() const { return ship_upgrades().ship_level; }
bool ClientContext::has_ship_capability(String const& capability) const {
  return ship_upgrades().capabilities.contains(capability);
}
std::shared_ptr<JsonRpcInterface> ClientContext::rpc_interface() const { return m_rpc; }

WorldChunks const& ClientContext::pending_ship_updates() const { return m_pending_ship_updates; }
StringMap<WorldChunks> const& ClientContext::pending_custom_world_updates() const {
  return m_pending_custom_world_updates;
}
void ClientContext::acknowledge_ship_updates() { m_pending_ship_updates.clear(); }
void ClientContext::acknowledge_custom_world_updates(String const& world) { m_pending_custom_world_updates.erase(world); }

std::expected<void, StarException> ClientContext::read_update(std::span<char const> data) {
  if (data.empty())
    return {};
  if (m_dispatching_rpc)
    return std::unexpected(DataStreamException("Cannot apply another context update from its RPC callback"));
  DataStreamReader stream(data);
  stream.set_stream_compatibility_version(m_net_compatibility_rules);
  auto rpc_frame = read_frame(stream);
  if (!rpc_frame)
    return std::unexpected(std::move(rpc_frame.error()));
  auto ship_frame = read_frame(stream);
  if (!ship_frame)
    return std::unexpected(std::move(ship_frame.error()));
  WorldChunks ship_updates;
  if (!ship_frame->empty()) {
    if (auto decoded = read_chunks(*ship_frame, ship_updates); !decoded)
      return decoded;
  }

  StringMap<WorldChunks> custom_updates;
  if (m_net_compatibility_rules.version() >= 15) {
    auto decoded = stream.read_map_container(custom_updates, [&](DataStream&, String& name, WorldChunks& chunks)
        -> std::expected<void, StarException> {
      auto world_name = read_frame(stream);
      if (!world_name)
        return std::unexpected(std::move(world_name.error()));
      auto frame = read_frame(stream);
      if (!frame)
        return std::unexpected(std::move(frame.error()));
      name = String(std::string_view(*world_name));
      return read_chunks(*frame, chunks);
    });
    if (!decoded)
      return decoded;
  }

  auto net_frame = read_frame(stream);
  if (!net_frame)
    return std::unexpected(std::move(net_frame.error()));
  if (auto end = require_end(stream); !end)
    return end;
  if (!net_frame->empty()) {
    if (auto staged = m_staged_state.stage_update(*net_frame, m_net_compatibility_rules); !staged)
      return staged;
  }

  // Do not dispatch even the first RPC until all frame boundaries, actual
  // chunks and net fields are known good. RPC side effects cannot be undone.
  if (!rpc_frame->empty()) {
    m_dispatching_rpc = true;
    auto finished = make_scope_exit([this] { m_dispatching_rpc = false; });
    if (auto received = m_rpc->receive(*rpc_frame); !received)
      return received;
  }
  if (!net_frame->empty())
    m_staged_state.publish_to(m_net_state);
  merge_chunks(m_pending_ship_updates, std::move(ship_updates));
  if (m_pending_custom_world_updates.empty()) {
    m_pending_custom_world_updates = std::move(custom_updates);
  } else {
    for (auto&& [name, chunks] : custom_updates)
      merge_chunks(m_pending_custom_world_updates[name], std::move(chunks));
  }
  return {};
}

std::expected<ByteArray, StarException> ClientContext::write_update() { return m_rpc->send(); }

}
