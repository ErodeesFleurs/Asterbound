import star.format;
import star.json;
import star.xx_hash;
import star.json_extra;
import star.data_stream;
import star.string;
import star.vector;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.strong_typedef;
import star.game_types;
import star.version;
import star.ordered_map;
import star.lru_cache;
import star.bi_map;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.id_map;
import star.set;
import star.casting;
import star.either;
import star.rpc_promise;
import star.a_star;
import star.ordered_set;
import star.audio;
import star.map;
import star.periodic_function;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.image;
import star.net_compatibility;
import std;
import star.worker_pool;

import star.sector_array_2d;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.json_rpc;

import star.lua_root;
import star.uuid;
import star.drawable;
import star.celestial_coordinate;
import star.warping;
import star.animation;
import star.particle;
import star.weather_types;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_damage;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.celestial_types;
import star.sky_parameters;
import star.world_template;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.tile_modification;
import star.world;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.game_timers;
import star.actor_movement_controller;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.player_log;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.liquid_types;
import star.tile_sector_array;
import star.collision_generator;
import star.world_tiles;
import star.chat_types;
import star.wiring;
import star.versioning_database;
import star.world_storage;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;

import star.client_context;
import star.team_client;

import star.algorithm;
namespace Star {
  
// update this in the rare case that backwards compat code has to be set up in TeamManager
extern VersionNumber const TeamClientVersion = 1;
  
TeamClient::TeamClient(Root& root, std::shared_ptr<Player> mainPlayer, std::shared_ptr<ClientContext> clientContext)
  : m_root(root), m_teamLeader(root.random()), m_pendingInvitation(Uuid(root.random()), String()) {
  m_mainPlayer = std::move(mainPlayer);
  m_clientContext = std::move(clientContext);

  m_hasPendingInvitation = false;
  m_pollInvitationsTimer = {};

  m_fullUpdateRunning = false;
  m_fullUpdateTimer = {};

  m_statusUpdateRunning = false;
  m_statusUpdateTimer = {};
}

bool TeamClient::isTeamLeader() {
  if (!m_teamUuid)
    return false;
  return m_teamLeader == m_clientContext->playerUuid();
}

bool TeamClient::isTeamLeader(Uuid const& playerUuid) {
  if (!m_teamUuid)
    return false;
  return m_teamLeader == playerUuid;
}

bool TeamClient::isMemberOfTeam() { return static_cast<bool>(m_teamUuid); }

std::expected<void, StarException> TeamClient::invitePlayer(String const& playerName) {
  if (playerName.empty()) return {};
  JsonObject request;
  request["inviteeName"] = playerName;
  request["inviterUuid"] = m_clientContext->playerUuid().hex();
  auto name = m_mainPlayer->name();
  if (!name) return std::unexpected(std::move(name.error()));
  request["inviterName"] = *name;
  return invokeRemote("team.invite", request, [playerName, &self = *this](Json response) -> std::expected<void, StarException> {
    if (!response)
      self.m_pendingInviteResults.push_back(std::make_pair(playerName, true));
    else if (response == "inviteeNotFound")
      self.m_pendingInviteResults.push_back(std::make_pair(playerName, false));
    else if (response.isType(Json::Type::Array)) {
      self.m_pendingInviteResults.push_back(StringList());
      StringList& invited = std::get<StringList>(self.m_pendingInviteResults.back());
      auto entries = response.toArray();
      if (!entries) return std::unexpected(std::move(entries.error()));
      for (auto& entry : *entries) {
        if (!entry.isType(Json::Type::Array)) continue;
        auto name = entry.get(0, Json());
        if (!name) return std::unexpected(std::move(name.error()));
        if (name->isType(Json::Type::String)) {
          auto text = name->toString();
          if (!text) return std::unexpected(std::move(text.error()));
          invited.push_back(std::move(*text));
        }
      }
    }
    return {};
  });
}

std::expected<void, StarException> TeamClient::acceptInvitation(Uuid const& inviterUuid) {
  JsonObject request;
  request["inviterUuid"] = inviterUuid.hex();
  request["inviteeUuid"] = m_clientContext->playerUuid().hex();
  return invokeRemote("team.acceptInvitation", request, [&self = *this](Json) -> std::expected<void, StarException> { self.forceUpdate(); return {}; });
}

std::optional<Uuid> TeamClient::currentTeam() const {
  return m_teamUuid;
}

std::expected<void, StarException> TeamClient::makeLeader(Uuid const& playerUuid) {
  if (!m_teamUuid)
    return {};
  if (!isTeamLeader())
    return {};
  JsonObject request;
  request["teamUuid"] = m_teamUuid->hex();
  request["playerUuid"] = playerUuid.hex();
  return invokeRemote("team.makeLeader", request, [&self = *this](Json) -> std::expected<void, StarException> { self.forceUpdate(); return {}; });
}

std::expected<void, StarException> TeamClient::removeFromTeam(Uuid const& playerUuid) {
  if (!m_teamUuid)
    return {};
  if (!isTeamLeader() && playerUuid != m_clientContext->playerUuid())
    return {};
  JsonObject request;
  request["teamUuid"] = m_teamUuid->hex();
  request["playerUuid"] = playerUuid.hex();
  return invokeRemote("team.removeFromTeam", request, [&self = *this](Json) -> std::expected<void, StarException> { self.forceUpdate(); return {}; });
}

bool TeamClient::hasInvitationPending() {
  return m_hasPendingInvitation;
}

std::pair<Uuid, String> TeamClient::pullInvitation() {
  m_hasPendingInvitation = false;
  return m_pendingInvitation;
}

List<std::variant<std::pair<String, bool>, StringList>> TeamClient::pullInviteResults() {
  return std::exchange(m_pendingInviteResults, {});
}

std::expected<void, StarException> TeamClient::update() {
  auto responses = handleRpcResponses();
  if (!responses) return std::unexpected(std::move(responses.error()));
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  std::optional<Json> interfaceConfig;
  auto readInterval = [&](String const& key) -> std::expected<float, StarException> {
    if (!interfaceConfig) {
      auto loaded = (*assets)->json("/interface.config");
      if (!loaded)
        return std::unexpected(std::move(loaded.error()));
      interfaceConfig = std::move(*loaded);
    }
    auto value = interfaceConfig->get(key);
    if (!value) {
      auto path = Star::format("/interface.config:{}", key);
      auto error = (*assets)->withAssetContext(
          AssetException(Star::format("Could not read JSON value {}", path), value.error()), "/interface.config", "");
      auto summary = *error.diagnostic_summary();
      summary.kind = ExceptionSummaryKind::Resource;
      summary.asset = std::move(path);
      return std::unexpected(std::move(error).with_summary(std::move(summary)));
    }
    return value->toFloat();
  };
  if (!m_hasPendingInvitation) {
    auto interval = readInterval("invitationPollInterval");
    if (!interval) return std::unexpected(std::move(interval.error()));
    if (std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()) - m_pollInvitationsTimer > std::chrono::duration<double>(*interval)) {
      m_pollInvitationsTimer = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
      JsonObject request;
      request["playerUuid"] = m_clientContext->playerUuid().hex();
      auto invoked = invokeRemote("team.pollInvitation", request, [&self = *this](Json response) -> std::expected<void, StarException> {
        if (response.isNull() || self.m_hasPendingInvitation) return {};
        auto uuidText = response.getString("inviterUuid");
        if (!uuidText) return std::unexpected(std::move(uuidText.error()));
        auto uuid = Uuid::create(*uuidText);
        if (!uuid) return std::unexpected(std::move(uuid.error()));
        auto name = response.getString("inviterName");
        if (!name) return std::unexpected(std::move(name.error()));
        self.m_pendingInvitation = {*uuid, std::move(*name)};
        self.m_hasPendingInvitation = true;
        return {};
      });
      if (!invoked) return std::unexpected(std::move(invoked.error()));
    }
  }
  if (!m_fullUpdateRunning) {
    auto interval = readInterval("fullUpdateInterval");
    if (!interval) return std::unexpected(std::move(interval.error()));
    if (std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()) - m_fullUpdateTimer > std::chrono::duration<double>(*interval)) {
      m_fullUpdateTimer = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
      auto update = pullFullUpdate();
      if (!update) return std::unexpected(std::move(update.error()));
    }
  }
  if (!m_statusUpdateRunning) {
    auto interval = readInterval("statusUpdateInterval");
    if (!interval) return std::unexpected(std::move(interval.error()));
    if (std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()) - m_statusUpdateTimer > std::chrono::duration<double>(*interval)) {
      m_statusUpdateTimer = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
      auto update = statusUpdate();
      if (!update) return std::unexpected(std::move(update.error()));
    }
  }
  return {};
}

std::expected<void, StarException> TeamClient::pullFullUpdate() {
  if (m_fullUpdateRunning) return {};
  m_fullUpdateRunning = true;
  JsonObject request;
  request["playerUuid"] = m_clientContext->playerUuid().hex();
  return invokeRemote("team.fetchTeamStatus", request, [&self = *this](Json response) -> std::expected<void, StarException> {
    self.m_fullUpdateRunning = false;
    auto version = response.optInt("version");
    if (!version) return std::unexpected(std::move(version.error()));
    self.m_teamManagerVersion = version->value_or(0);
    auto teamText = response.optString("teamUuid");
    if (!teamText) return std::unexpected(std::move(teamText.error()));
    auto previousTeam = self.m_teamUuid;
    self.m_teamUuid.reset();
    if (*teamText) {
      auto team = Uuid::create(**teamText);
      if (!team) return std::unexpected(std::move(team.error()));
      self.m_teamUuid = *team;
    }
    if (self.m_teamUuid != previousTeam)
      self.m_fullStatusRequired = true;
    if (self.m_teamUuid) {
      auto leaderText = response.getString("leader");
      if (!leaderText) return std::unexpected(std::move(leaderText.error()));
      auto leader = Uuid::create(*leaderText);
      if (!leader) return std::unexpected(std::move(leader.error()));
      self.m_teamLeader = *leader;
      auto previousMembers = std::exchange(self.m_members, {});
      auto entries = response.getArray("members");
      if (!entries) return std::unexpected(std::move(entries.error()));
      for (auto const& m : *entries) {
        auto name = m.getString("name");
        if (!name) return std::unexpected(std::move(name.error()));
        auto uuidText = m.getString("uuid");
        if (!uuidText) return std::unexpected(std::move(uuidText.error()));
        auto uuid = Uuid::create(*uuidText);
        if (!uuid) return std::unexpected(std::move(uuid.error()));
        auto entity = m.getInt("entity");
        if (!entity) return std::unexpected(std::move(entity.error()));
        auto health = m.getFloat("health");
        if (!health) return std::unexpected(std::move(health.error()));
        auto energy = m.getFloat("energy");
        if (!energy) return std::unexpected(std::move(energy.error()));
        auto worldText = m.getString("world");
        if (!worldText) return std::unexpected(std::move(worldText.error()));
        auto world = parseWorldId(*worldText);
        if (!world) return std::unexpected(std::move(world.error()));
        auto x = m.getFloat("x");
        if (!x) return std::unexpected(std::move(x.error()));
        auto y = m.getFloat("y");
        if (!y) return std::unexpected(std::move(y.error()));
        auto warpText = m.getString("warpMode");
        if (!warpText) return std::unexpected(std::move(warpText.error()));
        if (!WarpModeNames) return std::unexpected(WarpModeNames.error());
        auto warp = WarpModeNames->get_left(*warpText);
        if (!warp) return std::unexpected(std::move(warp.error()));
        auto portraitJson = m.get("portrait");
        if (!portraitJson) return std::unexpected(std::move(portraitJson.error()));
        auto portraitEntries = portraitJson->toArray();
        if (!portraitEntries) return std::unexpected(std::move(portraitEntries.error()));
        auto metadata = self.m_root.imageMetadataDatabase();
        if (!metadata) return std::unexpected(std::move(metadata.error()));
        List<Drawable> portrait;
        for (auto const& value : *portraitEntries) {
          auto drawable = Drawable::create(**metadata, value);
          if (!drawable) return std::unexpected(std::move(drawable.error()));
          portrait.push_back(std::move(*drawable));
        }
        std::uint64_t portraitRevision = 0;
        for (auto const& previous : previousMembers) {
          if (previous.uuid == *uuid && previous.portrait == portrait) {
            portraitRevision = previous.portraitRevision;
            break;
          }
        }
        if (!portraitRevision)
          portraitRevision = ++self.m_nextPortraitRevision;
        self.m_members.push_back(Member{std::move(*name), *uuid, static_cast<EntityId>(*entity), *health, *energy, std::move(*world), Vec2F(*x, *y), warp->get(), std::move(portrait), portraitRevision});
      }
      std::sort(self.m_members.begin(), self.m_members.end(), [](Member const& a, Member const& b) { return a.name < b.name; });
    } else {
      self.clearTeam();
    }
    return {};
  });
}

std::expected<void, StarException> TeamClient::statusUpdate() {
  if (m_statusUpdateRunning)
    return {};
  if (!m_teamUuid)
    return {};
  auto now = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
  bool fullWrite = !m_lastFullStatusUpdate
      || m_fullStatusRequired
      || now - *m_lastFullStatusUpdate >= std::chrono::duration<double>(10.0);
  JsonObject request;
  auto written = writePlayerData(request, m_mainPlayer, fullWrite);
  if (!written)
    return std::unexpected(std::move(written.error()));
  auto invoked = invokeRemote("team.updateStatus", request,
      [&self = *this](Json const&) -> std::expected<void, StarException> {
        self.m_statusUpdateRunning = false;
        return {};
      });
  if (!invoked)
    return std::unexpected(std::move(invoked.error()));
  m_statusUpdateRunning = true;
  if (fullWrite) {
    // Legacy RPC maps remote failures to null too; only local submission is
    // distinguishable. Do not advance this state on encoding/enqueue failure.
    m_lastFullStatusUpdate = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
    m_fullStatusRequired = false;
  }
  return {};
}

List<TeamClient::Member> const& TeamClient::members() const {
  return m_members;
}

JsonArray TeamClient::memberStatus() const {
  JsonArray result;
  result.reserve(m_members.size());
  for (auto const& member : m_members) {
    result.push_back(JsonObject{{"uuid", member.uuid.hex()},
                                {"name", member.name},
                                {"entity", member.entity},
                                {"healthPercentage", member.healthPercentage},
                                {"energyPercentage", member.energyPercentage},
                                {"position", JsonArray{member.position[0], member.position[1]}},
                                {"onWorld", member.world == m_clientContext->playerWorldId()},
                                {"canTravel", member.warpMode != WarpMode::None},
                                {"leader", member.uuid == m_teamLeader},
                                {"self", member.uuid == m_clientContext->playerUuid()},
                                {"portraitRevision", member.portraitRevision}});
  }
  return result;
}

std::expected<std::optional<Json>, StarException> TeamClient::memberPortrait(Uuid const& uuid, std::optional<std::uint64_t> knownRevision) const {
  for (auto const& member : m_members) {
    if (member.uuid != uuid)
      continue;
    if (knownRevision && *knownRevision == member.portraitRevision)
      return {};
    JsonArray portrait;
    portrait.reserve(member.portrait.size());
    for (auto const& drawable : member.portrait) {
      auto json = drawable.toJson();
      if (!json) return std::unexpected(std::move(json.error()));
      portrait.push_back(std::move(*json));
    }
    return JsonObject{{"revision", member.portraitRevision}, {"portrait", std::move(portrait)}};
  }
  return {};
}

void TeamClient::forceUpdate() {
  m_statusUpdateTimer = {};
  m_fullUpdateTimer = {};
  m_pollInvitationsTimer = {};
  m_fullStatusRequired = true;
}

std::expected<void, StarException> TeamClient::invokeRemote(String const& method, Json const& args,
    std::function<std::expected<void, StarException>(Json const&)> responseFunction) {
  auto promise = m_clientContext->rpcInterface()->invokeRemote(method, args);
  if (!promise) return std::unexpected(std::move(promise.error()));
  m_pendingResponses.push_back({std::move(*promise), std::move(responseFunction)});
  return {};
}

std::expected<void, StarException> TeamClient::handleRpcResponses() {
  List<RpcResponseHandler> stillPendingResponses;
  while (m_pendingResponses.size() > 0) {
    auto taken = m_pendingResponses.take_back();
    if (!taken) return std::unexpected(std::move(taken.error()));
    auto handler = std::move(*taken);
    auto finished = handler.first.finished();
    if (!finished) return std::unexpected(std::move(finished.error()));
    if (*finished) {
      auto res = handler.first.result();
      if (!res) return std::unexpected(std::move(res.error()));
      if (*res && handler.second) {
        auto handled = handler.second(**res);
        if (!handled) return std::unexpected(std::move(handled.error()));
      }
    } else {
      stillPendingResponses.push_back(std::move(handler));
    }
  }
  m_pendingResponses = std::move(stillPendingResponses);
  return {};
}

std::expected<void, StarException> TeamClient::writePlayerData(JsonObject& request, std::shared_ptr<Player> player, bool fullWrite) const {
  request["version"] = TeamClientVersion;
  request["playerUuid"] = m_clientContext->playerUuid().hex();
  request["entity"] = player->entityId();
  auto maximumHealth = player->maxHealth();
  if (!maximumHealth) return std::unexpected(std::move(maximumHealth.error()));
  request["health"] = player->health() / *maximumHealth;
  auto maximumEnergy = player->maxEnergy();
  if (!maximumEnergy) return std::unexpected(std::move(maximumEnergy.error()));
  auto energy = player->energy();
  if (!energy) return std::unexpected(std::move(energy.error()));
  request["energy"] = *energy / *maximumEnergy;
  request["x"] = player->position()[0];
  request["y"] = player->position()[1];
  auto world = Star::print_world_id(m_clientContext->playerWorldId());
  if (!world) return std::unexpected(std::move(world.error()));
  request["world"] = *world;

  WarpMode mode = WarpMode::None;
  if (player->log()->introComplete()) {
    if (std::holds_alternative<CelestialWorldId>(m_clientContext->playerWorldId()))
      mode = WarpMode::BeamOnly;
    else
      mode = player->isDeployed() ? WarpMode::DeployOnly : WarpMode::BeamOnly;
  }
  if (!WarpModeNames) return std::unexpected(WarpModeNames.error());
  auto warp = WarpModeNames->get_right(mode);
  if (!warp) return std::unexpected(std::move(warp.error()));
  request["warpMode"] = warp->get();

  if (fullWrite) {
    auto name = player->name();
    if (!name) return std::unexpected(std::move(name.error()));
    request["name"] = *name;
    auto portrait = player->portrait(PortraitMode::Head);
    if (!portrait) return std::unexpected(std::move(portrait.error()));
    JsonArray images;
    for (auto const& drawable : *portrait) {
      auto json = drawable.toJson();
      if (!json) return std::unexpected(std::move(json.error()));
      images.push_back(std::move(*json));
    }
    request["portrait"] = std::move(images);
  }
  return {};
}

void TeamClient::clearTeam() {
  m_teamLeader = Uuid(m_root.random());
  m_teamUuid = {};
  m_members.clear();
  forceUpdate();
}

}
