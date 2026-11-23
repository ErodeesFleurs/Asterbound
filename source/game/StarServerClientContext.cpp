import std;
import star.byte_array;
import star.casting;
import star.celestial_coordinate;
import star.container_entity;
import star.context_net_state;
import star.damage_types;
import star.data_stream;
import star.data_stream_devices;
import star.data_stream_extra;
import star.format;
import star.game_types;
import star.host_address;
import star.item;
import star.item_database;
import star.item_descriptor;
import star.json;
import star.json_rpc;
import star.list;
import star.logging;
import star.map;
import star.net_compatibility;
import star.net_element_basic_fields;
import star.net_element_system;
import star.net_packets;
import star.player_types;
import star.root;
import star.rpc_promise;
import star.server_client_context;
import star.set;
import star.string;
import star.system_world;
import star.thread;
import star.universe_settings;
import star.uuid;
import star.warping;
import star.world_server;
import star.world_server_thread;
import star.world_storage;

namespace Star {

ServerClientContext::ServerClientContext(ConstructionToken, Root& root, ConnectionId client_id, std::optional<HostAddress> remote_address, NetCompatibilityRules net_rules, Uuid player_uuid,
    String player_name, String ship_species, bool can_become_admin, WorldChunks initial_ship_chunks, StringSet valid_ship_species, String account)
  : m_root(root),
    m_client_id(client_id),
    m_remote_address(std::move(remote_address)),
    m_net_rules(net_rules),
    m_player_uuid(std::move(player_uuid)),
    m_player_name(std::move(player_name)),
    m_account(std::move(account)),
    m_ship_species(std::move(ship_species)),
    m_valid_ship_species(std::move(valid_ship_species)),
    m_can_become_admin(can_become_admin),
    m_ship_chunks(std::move(initial_ship_chunks)),
    m_rpc(root.logger()) {
}

std::expected<std::shared_ptr<ServerClientContext>, StarException> ServerClientContext::create(
    Root& root, ConnectionId client_id, std::optional<HostAddress> remote_address, NetCompatibilityRules net_rules, Uuid player_uuid,
    String player_name, String ship_species, bool can_become_admin, WorldChunks initial_ship_chunks, StringSet valid_ship_species, String account) {
  auto context = std::make_shared<ServerClientContext>(ConstructionToken{}, root, client_id, std::move(remote_address), net_rules, std::move(player_uuid),
      std::move(player_name), std::move(ship_species), can_become_admin, std::move(initial_ship_chunks), std::move(valid_ship_species), std::move(account));
  auto initialized = context->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return context;
}

std::expected<void, StarException> ServerClientContext::initialize() {
  auto upgrade_handler = m_rpc.register_handler("ship.applyShipUpgrades", [&self = *this](Json const& args) -> std::expected<Json, StarException> {
    std::lock_guard lock(self.m_mutex);
    auto upgrades = self.m_net_state.ship_upgrades.get();
    if (auto applied = upgrades.apply(args); !applied)
      return std::unexpected(std::move(applied.error()));
    if (self.m_net_state.ship_upgrades.get() != upgrades)
      self.m_net_state.ship_upgrades.push(std::move(upgrades));
    return Json(true);
  });
  if (!upgrade_handler)
    return std::unexpected(std::move(upgrade_handler.error()));

  auto species_handler = m_rpc.register_handler("ship.setShipSpecies", [&self = *this](Json const& species) -> std::expected<Json, StarException> {
    auto new_species = species.to_string();
    if (!new_species)
      return std::unexpected(std::move(new_species.error()));
    if (!self.m_valid_ship_species.contains(*new_species)) {
      auto logged = self.m_root.logger().warn("ServerClientContext: <User: {}> tried to set unknown ship species '{}'", self.m_player_name, *new_species);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      return Json(false);
    }
    self.set_ship_species(std::move(*new_species));
    return Json(true);
  });
  if (!species_handler)
    return std::unexpected(std::move(species_handler.error()));

  auto container_handler = m_rpc.register_handler("world.containerPutItems", [&self = *this](Json const& args) -> std::expected<Json, StarException> {
    auto items_json = args.get("items");
    if (!items_json)
      return std::unexpected(std::move(items_json.error()));
    auto items = items_json->array_ptr();
    if (!items)
      return std::unexpected(std::move(items.error()));
    List<ItemDescriptor> overflow;
    overflow.reserve((*items)->size());
    for (auto const& item : **items) {
      auto descriptor = ItemDescriptor::create(item);
      if (!descriptor)
        return std::unexpected(std::move(descriptor.error()));
      overflow.push_back(std::move(*descriptor));
    }
    if (auto world = self.player_world()) {
      auto executed = world->executeAction([&](WorldServerThread&, WorldServer& server) -> std::expected<void, StarException> {
        // The world lock serializes membership changes. An action that waited
        // through a warp/disconnect must not operate on the previous world.
        if (self.player_world() != world || !server.hasClient(self.client_id()))
          return {};
        auto entity_id = args.get_int("entityId");
        if (!entity_id)
          return std::unexpected(std::move(entity_id.error()));
        if (auto container = as<ContainerEntity>(server.entity(*entity_id))) {
          auto item_database = server.root().itemDatabase();
          if (!item_database)
            return std::unexpected(std::move(item_database.error()));
          auto remaining = overflow.begin();
          for (auto& descriptor : overflow) {
            auto item = (*item_database)->item(std::move(descriptor));
            if (!item)
              return std::unexpected(std::move(item.error()));
            auto added = container->addItems(std::move(*item));
            if (!added)
              return std::unexpected(std::move(added.error()));
            auto result = added->result();
            if (!result)
              return std::unexpected(std::move(result.error()));
            if (*result && **result)
              *remaining++ = (**result)->descriptor();
          }
          overflow.erase(remaining, overflow.end());
        }
        return {};
      });
      if (!executed)
        return std::unexpected(std::move(executed.error()));
    }
    return Json(overflow.transformed(std::mem_fn(&ItemDescriptor::toJson)));
  });
  if (!container_handler)
    return std::unexpected(std::move(container_handler.error()));

  auto flag_handler = m_rpc.register_handler("universe.setFlag", [&self = *this](Json const& args) -> std::expected<Json, StarException> {
    auto flag_name = args.to_string();
    if (!flag_name)
      return std::unexpected(std::move(flag_name.error()));
    if (auto world = self.player_world()) {
      auto executed = world->executeAction([&](WorldServerThread&, WorldServer& server) -> std::expected<void, StarException> {
        if (self.player_world() != world || !server.hasClient(self.client_id()))
          return {};
        return server.universeSettings()->setFlag(*flag_name);
      });
      if (!executed)
        return std::unexpected(std::move(executed.error()));
    }
    return Json();
  });
  if (!flag_handler)
    return std::unexpected(std::move(flag_handler.error()));

  return m_net_state.initialize();
}

ConnectionId ServerClientContext::client_id() const {
  return m_client_id;
}

std::optional<HostAddress> const& ServerClientContext::remote_address() const {
  return m_remote_address;
}

Uuid const& ServerClientContext::player_uuid() const {
  return m_player_uuid;
}

String const& ServerClientContext::player_name() const {
  return m_player_name;
}

String const& ServerClientContext::account() const {
  return m_account;
}

String ServerClientContext::ship_species() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_ship_species;
}

bool ServerClientContext::can_become_admin() const {
  return m_can_become_admin;
}

NetCompatibilityRules ServerClientContext::net_rules() const {
  return m_net_rules;
}

String ServerClientContext::descriptive_name() const {
  auto host_name = m_remote_address ? Star::to_string(*m_remote_address) : "local";
  return Star::format("'{}' <{}> ({})", m_player_name, m_client_id, host_name);
}

std::expected<void, StarException> ServerClientContext::register_rpc_handlers(JsonRpcHandlers const& handlers) {
  std::lock_guard lock(m_rpc_mutex);
  return m_rpc.register_handlers(handlers);
}

CelestialCoordinate ServerClientContext::ship_coordinate() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_net_state.ship_coordinate.get();
}

void ServerClientContext::set_ship_coordinate(CelestialCoordinate coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  m_net_state.ship_coordinate.set(coordinate);
}

SystemLocation ServerClientContext::ship_location() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_ship_system_location;
}

void ServerClientContext::set_ship_location(SystemLocation location) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  m_ship_system_location = std::move(location);
}

std::optional<std::pair<WarpAction, WarpMode>> ServerClientContext::orbit_warp_action() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_net_state.orbit_warp_action.get();
}

void ServerClientContext::set_orbit_warp_action(std::optional<std::pair<WarpAction, WarpMode>> warp_action) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (m_net_state.orbit_warp_action.get() != warp_action)
    m_net_state.orbit_warp_action.push(std::move(warp_action));
}

bool ServerClientContext::is_admin() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_net_state.is_admin.get();
}

void ServerClientContext::set_admin(bool admin) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  m_net_state.is_admin.set(admin);
}

bool ServerClientContext::server_debug() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_server_debug;
}

void ServerClientContext::set_server_debug(bool server_debug) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  m_server_debug = server_debug;
}

EntityDamageTeam ServerClientContext::team() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_net_state.team.get();
}

void ServerClientContext::set_team(EntityDamageTeam team) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  m_net_state.team.set(team);
}

ShipUpgrades ServerClientContext::ship_upgrades() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_net_state.ship_upgrades.get();
}

float ServerClientContext::ship_speed() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_net_state.ship_upgrades.get().ship_speed;
}

void ServerClientContext::set_ship_upgrades(ShipUpgrades upgrades) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (m_net_state.ship_upgrades.get() != upgrades)
    m_net_state.ship_upgrades.push(std::move(upgrades));
}

void ServerClientContext::set_ship_species(String species) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  m_ship_species = std::move(species);
}

WorldChunks ServerClientContext::ship_chunks() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_ship_chunks;
}

void ServerClientContext::update_ship_chunks(WorldChunks new_ship_chunks) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  WorldStorage::accumulate_world_chunks_update(m_ship_chunks_update, m_ship_chunks, new_ship_chunks);
  m_ship_chunks = std::move(new_ship_chunks);
}

std::expected<void, StarException> ServerClientContext::read_update(ByteArray const& data) {
  // Handlers run synchronously; never take the context data lock into one.
  std::lock_guard lock(m_rpc_mutex);
  return m_rpc.receive(data.span());
}

std::expected<void, StarException> ServerClientContext::stage_update() {
  // A sender can hold the universe lock. A running handler can be waiting
  // for a world that needs that lock: leave its replies for the next send.
  if (m_pending_rpc.empty()) {
    std::unique_lock rpc_lock(m_rpc_mutex, std::try_to_lock);
    if (rpc_lock && m_rpc.has_pending()) {
      auto replies = m_rpc.send();
      if (!replies)
        return std::unexpected(std::move(replies.error()));
      m_pending_rpc = std::move(*replies);
    }
  }

  std::lock_guard lock(m_mutex);
  auto net = m_net_state.group.write_net_state(m_net_version, m_net_rules);
  if (!net)
    return std::unexpected(std::move(net.error()));
  if (m_pending_rpc.empty() && m_ship_chunks_update.empty() && !m_custom_chunks_dirty && net->first.empty())
    return {};

  DataStreamBuffer frame;
  if (auto written = frame.write(m_pending_rpc); !written)
    return std::unexpected(std::move(written.error()));
  // Only one length-prefixed chunk payload needs scratch space, even when
  // several large worlds change together. Keep all source maps until commit.
  DataStreamBuffer scratch;
  auto write_chunks = [&](WorldChunks const& chunks) -> std::expected<void, StarException> {
    scratch.clear();
    if (!chunks.empty()) {
      if (auto written = scratch.write(chunks); !written)
        return written;
    }
    return frame.write(scratch.data());
  };
  if (auto written = write_chunks(m_ship_chunks_update); !written)
    return written;
  if (m_net_rules.version() >= 15) {
    auto count = m_custom_chunks_dirty ? std::ranges::count_if(m_custom_worlds | std::views::values, [](CustomWorld const& world) {
      return !world.chunks_update.empty();
    }) : 0;
    if (auto written = frame.write_vlq_u(count); !written)
      return std::unexpected(std::move(written.error()));
    if (m_custom_chunks_dirty) {
      for (auto const& [name, world] : m_custom_worlds) {
        if (world.chunks_update.empty())
          continue;
        if (auto written = frame.write(name); !written)
          return written;
        if (auto written = write_chunks(world.chunks_update); !written)
          return written;
      }
    }
  }
  if (auto written = frame.write(net->first); !written)
    return std::unexpected(std::move(written.error()));

  // Publish the complete packet before consuming its sources. Updates made
  // after this point accumulate independently, including during a retry.
  m_staged_update = std::make_shared<ClientContextUpdatePacket>(frame.take_data());
  m_pending_rpc.clear();
  m_ship_chunks_update.clear();
  if (std::exchange(m_custom_chunks_dirty, false)) {
    for (auto& world : m_custom_worlds | std::views::values)
      world.chunks_update.clear();
  }
  m_net_version = net->second;
  return {};
}


void ServerClientContext::set_system_world(std::shared_ptr<SystemWorldServerThread> system_world_thread) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  // The previous thread may join a worker during destruction. Keep it in
  // the argument until the context lock has been released.
  m_system_world_thread.swap(system_world_thread);
}

std::shared_ptr<SystemWorldServerThread> ServerClientContext::system_world() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_system_world_thread;
}

void ServerClientContext::clear_system_world() {
  set_system_world({});
}

void ServerClientContext::set_player_world(std::shared_ptr<WorldServerThread> world_thread) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (m_world_thread == world_thread)
    return;

  m_world_thread.swap(world_thread);
  auto id = m_world_thread ? m_world_thread->worldId() : WorldId{};
  if (m_net_state.player_world_id.get() != id)
    m_net_state.player_world_id.push(std::move(id));
}

std::shared_ptr<WorldServerThread> ServerClientContext::player_world() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_world_thread;
}

WorldId ServerClientContext::player_world_id() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_net_state.player_world_id.get();
}

void ServerClientContext::clear_player_world() {
  set_player_world({});
}

void ServerClientContext::set_sub_world(ClientSubWorldId sub_world_id, std::shared_ptr<WorldServerThread> world_thread) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (world_thread) {
    m_sub_world_threads[sub_world_id].swap(world_thread);
  } else if (auto it = m_sub_world_threads.find(sub_world_id); it != m_sub_world_threads.end()) {
    it->second.swap(world_thread);
    m_sub_world_threads.erase(it);
  }
}

std::shared_ptr<WorldServerThread> ServerClientContext::sub_world(ClientSubWorldId sub_world_id) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_sub_world_threads.value(sub_world_id);
}

bool ServerClientContext::has_sub_world(ClientSubWorldId sub_world_id) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_sub_world_threads.contains(sub_world_id);
}

void ServerClientContext::clear_sub_world(ClientSubWorldId sub_world_id) {
  set_sub_world(sub_world_id, {});
}

List<ClientSubWorldId> ServerClientContext::sub_worlds() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_sub_world_threads.keys();
}

WarpToWorld ServerClientContext::player_return_warp() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_return_warp;
}

void ServerClientContext::set_player_return_warp(WarpToWorld warp) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  m_return_warp = std::move(warp);
}

WarpToWorld ServerClientContext::player_revive_warp() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_revive_warp;
}

void ServerClientContext::set_player_revive_warp(WarpToWorld warp) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  m_revive_warp = std::move(warp);
}

std::expected<void, StarException> ServerClientContext::custom_world_requested(String name, RpcPromiseKeeper<WorldChunks> promise) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto added = m_world_requests.add(std::move(name), std::move(promise));
  if (!added)
    return std::unexpected(std::move(added.error()));
  return {};
}

std::expected<void, StarException> ServerClientContext::custom_world_received(String name, WorldChunks chunks) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (auto promise = m_world_requests.try_extract_value(name)) {
    auto fulfilled = promise->fulfill(chunks);
    if (!fulfilled)
      return std::unexpected(std::move(fulfilled.error()));
  }
  auto added = m_custom_worlds.add(std::move(name), CustomWorld{std::move(chunks), {}});
  if (!added)
    return std::unexpected(std::move(added.error()));
  return {};
}

std::expected<void, StarException> ServerClientContext::fail_world_requests() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  std::optional<StarException> first_error;
  for (auto&& entry : m_world_requests) {
    auto failed = entry.second.fail("Client disconnected");
    if (!failed && !first_error)
      first_error = std::move(failed.error());
  }
  m_world_requests.clear();
  if (first_error)
    return std::unexpected(std::move(*first_error));
  return {};
}

std::optional<WorldChunks> ServerClientContext::custom_world_chunks(String const& name) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (auto world = m_custom_worlds.find_ref(name))
    return world->get().chunks;
  return {};
}

void ServerClientContext::update_custom_world_chunks(String name, WorldChunks new_world_chunks) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto& world = m_custom_worlds[std::move(name)];
  if (m_net_rules.version() >= 15 && WorldStorage::accumulate_world_chunks_update(world.chunks_update, world.chunks, new_world_chunks))
    m_custom_chunks_dirty = true;
  world.chunks = std::move(new_world_chunks);
}

void ServerClientContext::set_custom_world_active(String const& name, bool active) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (auto world = m_custom_worlds.find_ref(name))
    world->get().active = active;
}

List<String> ServerClientContext::custom_worlds() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_custom_worlds.keys();
}

std::expected<void, StarException> ServerClientContext::clean_inactive_custom_worlds() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  for (auto it = m_custom_worlds.begin(); it != m_custom_worlds.end();) {
    if (it->second.active || !it->second.chunks_update.empty()) {
      ++it;
      continue;
    }
    auto logged = m_root.logger().info("Removing inactive custom world '{}' for client '{}'", it->first, m_player_uuid.hex());
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    it = m_custom_worlds.erase(it);
  }
  return {};
}

std::expected<void, StarException> ServerClientContext::load_server_data(Json const& store) {
  auto coordinate_data = store.get("shipCoordinate");
  if (!coordinate_data)
    return std::unexpected(std::move(coordinate_data.error()));
  auto coordinate = CelestialCoordinate::create(*coordinate_data);
  if (!coordinate)
    return std::unexpected(std::move(coordinate.error()));
  auto location_data = store.get("systemLocation");
  if (!location_data)
    return std::unexpected(std::move(location_data.error()));
  auto location = jsonToSystemLocation(*location_data);
  if (!location)
    return std::unexpected(std::move(location.error()));
  auto admin = store.get_bool("isAdmin");
  if (!admin)
    return std::unexpected(std::move(admin.error()));
  auto team_data = store.get("team");
  if (!team_data)
    return std::unexpected(std::move(team_data.error()));
  auto team = EntityDamageTeam::create(*team_data);
  if (!team)
    return std::unexpected(std::move(team.error()));
  auto revive_data = store.get("reviveWarp");
  if (!revive_data)
    return std::unexpected(std::move(revive_data.error()));
  auto revive = WarpToWorld::create(*revive_data);
  if (!revive)
    return std::unexpected(std::move(revive.error()));
  auto return_data = store.get("returnWarp");
  if (!return_data)
    return std::unexpected(std::move(return_data.error()));
  auto return_warp = WarpToWorld::create(*return_data);
  if (!return_warp)
    return std::unexpected(std::move(return_warp.error()));

  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  m_net_state.ship_coordinate.set(*coordinate);
  m_ship_system_location = std::move(*location);
  m_net_state.is_admin.set(*admin);
  m_net_state.team.set(*team);
  m_revive_warp = std::move(*revive);
  m_return_warp = std::move(*return_warp);
  return {};
}

std::expected<Json, StarException> ServerClientContext::store_server_data() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto location = jsonFromSystemLocation(m_ship_system_location);
  auto team_data = m_net_state.team.get().toJson();
  if (!team_data)
    return std::unexpected(std::move(team_data.error()));
  auto revive = m_revive_warp.toJson();
  if (!revive)
    return std::unexpected(std::move(revive.error()));
  auto return_warp = m_return_warp.toJson();
  if (!return_warp)
    return std::unexpected(std::move(return_warp.error()));
  return JsonObject{
    {"shipCoordinate", m_net_state.ship_coordinate.get().to_json()},
    {"systemLocation", std::move(location)},
    {"isAdmin", m_net_state.is_admin.get()},
    {"team", std::move(*team_data)},
    {"reviveWarp", std::move(*revive)},
    {"returnWarp", std::move(*return_warp)}
  };
}

std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> ServerClientContext::creation_time() const {
  return m_creation_time;
}

}
