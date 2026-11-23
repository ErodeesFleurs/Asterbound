module;
import std;
import star.byte_array;
import star.celestial_coordinate;
import star.context_net_state;
import star.damage_types;
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

class ClientContext {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  [[nodiscard]] static std::expected<std::shared_ptr<ClientContext>, StarException> create(
      Logger& logger, Uuid server_uuid, Uuid player_uuid, ConnectionId connection_id, NetCompatibilityRules rules);
  ClientContext(ConstructionKey, Logger& logger, Uuid server_uuid, Uuid player_uuid,
      ConnectionId connection_id, NetCompatibilityRules rules);

  Uuid server_uuid() const;
  // The original player owns the saved ship, even after swapping characters.
  Uuid player_uuid() const;
  ConnectionId connection_id() const;
  NetCompatibilityRules net_compatibility_rules() const;

  CelestialCoordinate ship_coordinate() const;
  std::optional<std::pair<WarpAction, WarpMode>> orbit_warp_action() const;
  WorldId player_world_id() const;
  bool is_admin() const;
  EntityDamageTeam team() const;
  ShipUpgrades const& ship_upgrades() const;
  unsigned ship_level() const;
  bool has_ship_capability(String const& capability) const;
  std::shared_ptr<JsonRpcInterface> rpc_interface() const;

  // Updates remain pending until the caller finishes writing the corresponding file.
  WorldChunks const& pending_ship_updates() const;
  StringMap<WorldChunks> const& pending_custom_world_updates() const;
  void acknowledge_ship_updates();
  void acknowledge_custom_world_updates(String const& world);

  [[nodiscard]] std::expected<void, StarException> read_update(std::span<char const> data);
  [[nodiscard]] std::expected<ByteArray, StarException> write_update();

private:
  Uuid m_server_uuid;
  Uuid m_player_uuid;
  ConnectionId m_connection_id;
  NetCompatibilityRules m_net_compatibility_rules;
  std::shared_ptr<JsonRpc> m_rpc;
  bool m_dispatching_rpc = false;
  ContextNetState m_net_state;
  ContextNetState m_staged_state;
  WorldChunks m_pending_ship_updates;
  StringMap<WorldChunks> m_pending_custom_world_updates;
};

}

export module star.client_context;

export namespace Star {
using ::Star::ClientContext;
}
