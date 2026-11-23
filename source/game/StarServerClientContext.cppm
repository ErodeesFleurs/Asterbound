module;
import std;
import star.byte_array;
import star.celestial_coordinate;
import star.context_net_state;
import star.damage_types;
import star.format;
import star.game_types;
import star.host_address;
import star.json;
import star.json_rpc;
import star.list;
import star.map;
import star.net_compatibility;
import star.player_types;
import star.rpc_promise;
import star.string;
import star.system_world;
import star.uuid;
import star.warping;
import star.world_storage;

namespace Star {

class WorldServerThread;
class SystemWorldServerThread;
class Root;
struct ClientContextUpdatePacket;

template <typename Sink>
concept context_update_sink = requires(Sink& enqueue, std::shared_ptr<ClientContextUpdatePacket> const& packet) {
  { std::invoke(enqueue, packet) } -> std::same_as<std::expected<void, StarException>>;
};

class ServerClientContext {
  struct ConstructionToken {
    explicit ConstructionToken() = default;
  };

public:
  [[nodiscard]] static std::expected<std::shared_ptr<ServerClientContext>, StarException> create(Root& root, ConnectionId client_id, std::optional<HostAddress> remote_address, NetCompatibilityRules net_rules, Uuid player_uuid,
      String player_name, String ship_species, bool can_become_admin, WorldChunks initial_ship_chunks, StringSet valid_ship_species, String account);
  ServerClientContext(ConstructionToken, Root& root, ConnectionId client_id, std::optional<HostAddress> remote_address, NetCompatibilityRules net_rules, Uuid player_uuid,
      String player_name, String ship_species, bool can_become_admin, WorldChunks initial_ship_chunks, StringSet valid_ship_species, String account);

  ConnectionId client_id() const;
  std::optional<HostAddress> const& remote_address() const;
  Uuid const& player_uuid() const;
  String const& player_name() const;
  String ship_species() const;
  bool can_become_admin() const;
  NetCompatibilityRules net_rules() const;
  String descriptive_name() const;
  String const& account() const;

  // Register additional rpc methods from other server side services.
  [[nodiscard]] std::expected<void, StarException> register_rpc_handlers(JsonRpcHandlers const& rpc_handlers);

  // The coordinate for the world which the *player's* ship is currently
  // orbiting, if it is currently orbiting a world.
  CelestialCoordinate ship_coordinate() const;
  void set_ship_coordinate(CelestialCoordinate coordinate);

  SystemLocation ship_location() const;
  void set_ship_location(SystemLocation location);

  // Warp action and warp mode to the planet the player is currently orbiting
  // valid when the player is on any ship world orbiting a location
  std::optional<std::pair<WarpAction, WarpMode>> orbit_warp_action() const;
  void set_orbit_warp_action(std::optional<std::pair<WarpAction, WarpMode>> warp_action);

  bool is_admin() const;
  void set_admin(bool admin);

  bool server_debug() const;
  void set_server_debug(bool server_debug);

  EntityDamageTeam team() const;
  void set_team(EntityDamageTeam team);

  ShipUpgrades ship_upgrades() const;
  float ship_speed() const;
  void set_ship_upgrades(ShipUpgrades upgrades);
  void set_ship_species(String species);

  WorldChunks ship_chunks() const;
  void update_ship_chunks(WorldChunks new_ship_chunks);

  [[nodiscard]] std::expected<void, StarException> read_update(ByteArray const& data);
  // Synchronous local enqueue. The sink must not reenter send_update; failure
  // means the queue did not accept the packet, not that socket I/O failed.
  template <context_update_sink Sink>
  [[nodiscard]] std::expected<void, StarException> send_update(Sink&& enqueue) {
    std::lock_guard lock(m_send_mutex);
    auto flush = [&]() -> std::expected<void, StarException> {
      if (m_staged_update) {
        if (auto sent = std::invoke(enqueue, std::as_const(m_staged_update)); !sent)
          return sent;
        m_staged_update.reset();
      }
      return {};
    };
    if (auto retried = flush(); !retried)
      return retried;
    // A final send must include changes accumulated since the failed enqueue.
    if (auto staged = stage_update(); !staged)
      return staged;
    return flush();
  }

  void set_player_world(std::shared_ptr<WorldServerThread> world_thread);
  std::shared_ptr<WorldServerThread> player_world() const;
  WorldId player_world_id() const;
  void clear_player_world();

  void set_sub_world(ClientSubWorldId sub_world_id, std::shared_ptr<WorldServerThread> world_thread);
  std::shared_ptr<WorldServerThread> sub_world(ClientSubWorldId sub_world_id) const;
  bool has_sub_world(ClientSubWorldId sub_world_id) const;
  void clear_sub_world(ClientSubWorldId sub_world_id);
  List<ClientSubWorldId> sub_worlds() const;

  void set_system_world(std::shared_ptr<SystemWorldServerThread> system_world_thread);
  std::shared_ptr<SystemWorldServerThread> system_world() const;
  void clear_system_world();

  WarpToWorld player_return_warp() const;
  void set_player_return_warp(WarpToWorld warp);

  WarpToWorld player_revive_warp() const;
  void set_player_revive_warp(WarpToWorld warp);

  [[nodiscard]] std::expected<void, StarException> custom_world_requested(String name, RpcPromiseKeeper<WorldChunks> promise);
  [[nodiscard]] std::expected<void, StarException> custom_world_received(String name, WorldChunks chunks);
  [[nodiscard]] std::expected<void, StarException> fail_world_requests();

  std::optional<WorldChunks> custom_world_chunks(String const& name) const;
  void update_custom_world_chunks(String name, WorldChunks new_world_chunks);
  void set_custom_world_active(String const& name, bool active);
  List<String> custom_worlds() const;

  [[nodiscard]] std::expected<void, StarException> clean_inactive_custom_worlds();

  // Server-side persistence excludes ship chunks and upgrades, which belong to the player.
  [[nodiscard]] std::expected<void, StarException> load_server_data(Json const& store);
  [[nodiscard]] std::expected<Json, StarException> store_server_data() const;

  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> creation_time() const;

private:
  [[nodiscard]] std::expected<void, StarException> initialize();
  [[nodiscard]] std::expected<void, StarException> stage_update();

  struct CustomWorld {
    WorldChunks chunks;
    WorldChunks chunks_update;
    bool active = false;
  };

  Root& m_root;
  ConnectionId const m_client_id;
  std::optional<HostAddress> const m_remote_address;
  NetCompatibilityRules const m_net_rules;
  Uuid const m_player_uuid;
  String const m_player_name;
  String const m_account;
  String m_ship_species;
  StringSet const m_valid_ship_species;
  bool const m_can_become_admin;
  bool m_server_debug = false;

  mutable std::mutex m_mutex;
  WorldChunks m_ship_chunks;
  WorldChunks m_ship_chunks_update;
  SystemLocation m_ship_system_location;

  std::recursive_mutex m_rpc_mutex;
  JsonRpc m_rpc;
  std::mutex m_send_mutex;
  ByteArray m_pending_rpc;
  std::shared_ptr<ClientContextUpdatePacket> m_staged_update;

  std::shared_ptr<WorldServerThread> m_world_thread;
  WarpToWorld m_return_warp;
  WarpToWorld m_revive_warp;
  HashMap<ClientSubWorldId, std::shared_ptr<WorldServerThread>> m_sub_world_threads;
  std::shared_ptr<SystemWorldServerThread> m_system_world_thread;

  ContextNetState m_net_state;
  std::uint64_t m_net_version = 0;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> const m_creation_time =
      std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  StringMap<RpcPromiseKeeper<WorldChunks>> m_world_requests;
  StringMap<CustomWorld> m_custom_worlds;
  bool m_custom_chunks_dirty = false;
};

}

export module star.server_client_context;

export namespace Star {
using ::Star::WorldServerThread;
using ::Star::SystemWorldServerThread;
using ::Star::ServerClientContext;
}
