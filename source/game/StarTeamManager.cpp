import star.format;
import star.json;
import star.json_extra;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.json_rpc;
import star.strong_typedef;
import star.game_types;
import std;
import star.map;
import star.version;
import star.random;
import star.ordered_map;
import star.bi_map;
import star.io_device;
import star.ref_ptr;
import star.listener;
import star.text;
import star.logging;

import star.drawable;
import star.uuid;
import star.celestial_coordinate;
import star.warping;
import star.damage_types;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.team_manager;

import star.algorithm;
namespace Star {
  
// update this in the rare case that backwards compat code has to be set up in TeamClient
extern VersionNumber const TeamManagerVersion = 0;

TeamManager::TeamManager(Root& root, unsigned maxTeamSize, double invitationTimeout, bool secureTeams)
  : m_root(root), m_maxTeamSize(maxTeamSize), m_polledInvitationTimeout(invitationTimeout), m_secureTeams(secureTeams), m_pvpTeamCounter(1) {}

std::expected<std::shared_ptr<TeamManager>, StarException> TeamManager::create(Root& root) {
  auto config = root.configuration();
  if (!config) return std::unexpected(std::move(config.error()));
  auto sizeJson = (*config)->get("maxTeamSize");
  if (!sizeJson) return std::unexpected(std::move(sizeJson.error()));
  auto size = sizeJson->toUInt();
  if (!size) return std::unexpected(std::move(size.error()));
  auto timeoutJson = (*config)->getPath("teamInvitationTimeout", Json(600.0));
  if (!timeoutJson) return std::unexpected(std::move(timeoutJson.error()));
  auto timeout = timeoutJson->toDouble();
  if (!timeout) return std::unexpected(std::move(timeout.error()));
  auto secureJson = (*config)->getPath("security.secureTeams");
  if (!secureJson) return std::unexpected(std::move(secureJson.error()));
  auto secure = secureJson->optBool();
  if (!secure) return std::unexpected(std::move(secure.error()));
  return std::shared_ptr<TeamManager>(new TeamManager(root, *size, *timeout, secure->value_or(true)));
}

JsonRpcHandlers TeamManager::rpcHandlers() {
  return JsonRpcHandlers{
    {"team.fetchTeamStatus",
     [this](Json const& request) -> std::expected<Json, StarException> { return fetchTeamStatus(request); }},
    {"team.updateStatus",
     [this](Json const& request) -> std::expected<Json, StarException> { return updateStatus(request); }},
    {"team.invite", [this](Json const& request) -> std::expected<Json, StarException> { return invite(request); }},
    {"team.pollInvitation",
     [this](Json const& request) -> std::expected<Json, StarException> { return pollInvitation(request); }},
    {"team.acceptInvitation",
     [this](Json const& request) -> std::expected<Json, StarException> { return acceptInvitation(request); }},
    {"team.makeLeader",
     [this](Json const& request) -> std::expected<Json, StarException> { return makeLeader(request); }},
    {"team.removeFromTeam",
     [&self = *this](Json request) -> std::expected<Json, StarException> { return self.removeFromTeam(request); }}};
}

JsonRpcHandlers TeamManager::authenticatedRpcHandlers(Uuid const& callerUuid) {
  auto wrap = [&](String const& name, JsonRpcRemoteFunction fn) -> std::pair<String, JsonRpcRemoteFunction> {
    return {name, [callerUuid, fn = std::move(fn)](Json const& args) -> std::expected<Json, StarException> {
        auto authenticated = args.set("_callerUuid", callerUuid.hex());
        if (!authenticated) return std::unexpected(std::move(authenticated.error()));
        return fn(*authenticated);
        }};
  };

  return JsonRpcHandlers{
    wrap("team.fetchTeamStatus",
         [this](Json const& request) -> std::expected<Json, StarException> { return fetchTeamStatus(request); }),
    wrap("team.updateStatus",
         [this](Json const& request) -> std::expected<Json, StarException> { return updateStatus(request); }),
    wrap("team.invite", [this](Json const& request) -> std::expected<Json, StarException> { return invite(request); }),
    wrap("team.pollInvitation",
         [this](Json const& request) -> std::expected<Json, StarException> { return pollInvitation(request); }),
    wrap("team.acceptInvitation",
         [this](Json const& request) -> std::expected<Json, StarException> { return acceptInvitation(request); }),
    wrap("team.makeLeader",
         [this](Json const& request) -> std::expected<Json, StarException> { return makeLeader(request); }),
    wrap("team.removeFromTeam", [&self = *this](Json const& request) -> std::expected<Json, StarException> {
      return self.removeFromTeam(request);
    })};
}

void TeamManager::setConnectedPlayers(StringMap<List<Uuid>> connectedPlayers) {
  m_connectedPlayers = std::move(connectedPlayers);
}

void TeamManager::playerDisconnected(Uuid const& playerUuid) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);

  purgeInvitationsFor(playerUuid);
  purgeInvitationsFrom(playerUuid);

  for (auto teamUuid : m_teams.keys()) {
    auto& team = m_teams.find_ref(teamUuid)->get();
    if (team.members.contains(playerUuid))
      removeFromTeam(playerUuid, teamUuid);
  }
}

TeamNumber TeamManager::getPvpTeam(Uuid const& playerUuid) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);
  for (auto const& teamPair : m_teams) {
    if (teamPair.second.members.contains(playerUuid))
      return teamPair.second.pvpTeamNumber;
  }
  return 0;
}

HashMap<Uuid, TeamNumber> TeamManager::getPvpTeams() {
  HashMap<Uuid, TeamNumber> result;
  for (auto const& teamPair : m_teams) {
    for (auto const& memberPair : teamPair.second.members)
      result[memberPair.first] = teamPair.second.pvpTeamNumber;
  }
  return result;
}

std::optional<Uuid> TeamManager::getTeam(Uuid const& playerUuid) const {
  for (auto const& teamPair : m_teams) {
    if (teamPair.second.members.contains(playerUuid))
      return teamPair.first;
  }
  return {};
}

void TeamManager::purgeInvitationsFor(Uuid const& playerUuid) {
  m_invitations.remove(playerUuid);
  m_polledInvitations.remove(playerUuid);
}

void TeamManager::purgeInvitationsFrom(Uuid const& playerUuid) {
  eraseWhere(m_invitations, [playerUuid](auto const& invitation) {
    return invitation.second.inviterUuid == playerUuid;
  });
  eraseWhere(m_polledInvitations, [playerUuid](auto const& polled) {
    return polled.second.inviterUuid == playerUuid;
  });
}

void TeamManager::expirePolledInvitations() {
  auto now = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
  eraseWhere(m_polledInvitations, [&](auto const& entry) {
    return (now - entry.second.polledAt) >= m_polledInvitationTimeout;
  });
}

bool TeamManager::playerWithUuidExists(Uuid const& playerUuid) const {
  for (auto const& p : m_connectedPlayers) {
    if (p.second.contains(playerUuid))
      return true;
  }
  return false;
}

Uuid TeamManager::createTeam(Uuid const& leaderUuid) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);

  Uuid teamUuid(m_root.random());
  auto& team = m_teams.try_emplace(teamUuid, m_root.random()).first->second; // create

  int limiter = 256;
  while (true) {
    team.pvpTeamNumber = m_pvpTeamCounter++;
    if (m_pvpTeamCounter == 0)
      m_pvpTeamCounter = 1;
    bool done = true;
    for (auto const& [k, t] : m_teams) {
      if (t.pvpTeamNumber == team.pvpTeamNumber) {
        if (k != teamUuid)
          done = false;
      }
    }
    if (done)
      break;
    if (limiter-- == 0) {
      team.pvpTeamNumber = 0;
      break;
    }
  }

  addToTeam(leaderUuid, teamUuid);
  team.leaderUuid = leaderUuid;

  return teamUuid;
}

bool TeamManager::addToTeam(Uuid const& playerUuid, Uuid const& teamUuid) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);

  if (!m_teams.contains(teamUuid))
    return false;

  auto& team = m_teams.find_ref(teamUuid)->get();

  if (team.members.contains(playerUuid))
    return false;

  if (team.members.size() >= m_maxTeamSize)
    return false;

  purgeInvitationsFor(playerUuid);

  for (auto otherTeam : m_teams) {
    List<Uuid> alreadyMemberOf;
    if (otherTeam.second.members.contains(playerUuid))
      alreadyMemberOf.push_back(otherTeam.first);
    for (auto leaveTeamUuid : alreadyMemberOf)
      removeFromTeam(playerUuid, leaveTeamUuid);
  }

  team.members.insert(playerUuid, TeamMember());

  return true;
}

bool TeamManager::removeFromTeam(Uuid const& playerUuid, Uuid const& teamUuid) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);

  if (!m_teams.contains(teamUuid))
    return false;

  auto& team = m_teams.find_ref(teamUuid)->get();

  if (!team.members.contains(playerUuid))
    return false;

  purgeInvitationsFrom(playerUuid);

  team.members.remove(playerUuid);
  if (team.members.size() <= 1)
    m_teams.remove(teamUuid);
  else if (team.leaderUuid == playerUuid) {
    auto leader = team.members.begin();
    std::advance(leader, m_root.random().randUInt(team.members.size() - 1));
    team.leaderUuid = leader->first;
  }

  return true;
}

std::expected<Json, StarException> TeamManager::fetchTeamStatus(Json const& arguments) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);
  auto playerUuidText = arguments.getString("playerUuid");
  if (!playerUuidText) return std::unexpected(std::move(playerUuidText.error()));
  auto playerUuidResult = Uuid::create(*playerUuidText);
  if (!playerUuidResult) return std::unexpected(std::move(playerUuidResult.error()));
  auto playerUuid = *playerUuidResult;

  auto callerHexResult = arguments.optString("_callerUuid");
  if (!callerHexResult) return std::unexpected(std::move(callerHexResult.error()));
  if (auto callerHex = *callerHexResult) {
    auto callerResult = Uuid::create(*callerHex);
    if (!callerResult) return std::unexpected(std::move(callerResult.error()));
    auto callerUuid = *callerResult;
    if (m_secureTeams && callerUuid != playerUuid) {
      auto logged = m_root.logger().warn("TeamManager: Rejected fetchTeamStatus from {} for {}", *callerHex, playerUuid.hex());
      if (!logged) return std::unexpected(std::move(logged.error()));
      return {};
    }
  }

  JsonObject result;
  result["version"] = TeamManagerVersion;
  if (auto teamUuid = getTeam(playerUuid)) {
    auto& team = m_teams.find_ref(*teamUuid)->get();
    auto& playerMem = team.members.find_ref(playerUuid)->get();

    result["teamUuid"] = teamUuid->hex();
    result["leader"] = team.leaderUuid.hex();
    JsonArray members;
    for (auto const& m : team.members) {
      JsonObject member;
      auto const& mem = m.second;
      member["name"] = mem.name;
      member["uuid"] = m.first.hex();
      member["leader"] = m.first == team.leaderUuid;
      member["entity"] = mem.entity;
      member["health"] = mem.healthPercentage;
      member["energy"] = mem.energyPercentage;
      member["x"] = mem.position[0];
      member["y"] = mem.position[1];
      if ((std::holds_alternative<CustomWorldId>(mem.world) || std::holds_alternative<ClientCustomWorldId>(mem.world)) && playerMem.version < 1) {
        // caller is too old to handle custom worlds, pretend they're testarena instead to prevent invalid worldid disconnects
        member["world"] = "InstanceWorld:testarena";
      } else {
          auto world = Star::print_world_id(mem.world);
        if (!world) return std::unexpected(std::move(world.error()));
        member["world"] = *world;
      }
      if (!WarpModeNames) return std::unexpected(WarpModeNames.error());
      auto warp = WarpModeNames->get_right(mem.warpMode);
      if (!warp) return std::unexpected(std::move(warp.error()));
      member["warpMode"] = warp->get();
      auto portrait = jsonFromList(mem.portrait, std::mem_fn(&Drawable::toJson));
      if (!portrait) return std::unexpected(std::move(portrait.error()));
      member["portrait"] = *portrait;
      members.push_back(member);
    }
    result["members"] = members;
  }
  return Json(result);
}

std::expected<Json, StarException> TeamManager::updateStatus(Json const& arguments) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);
  auto playerUuidText = arguments.getString("playerUuid");
  if (!playerUuidText) return std::unexpected(std::move(playerUuidText.error()));
  auto playerUuidResult = Uuid::create(*playerUuidText);
  if (!playerUuidResult) return std::unexpected(std::move(playerUuidResult.error()));
  auto playerUuid = *playerUuidResult;

  auto callerHexResult = arguments.optString("_callerUuid");
  if (!callerHexResult) return std::unexpected(std::move(callerHexResult.error()));
  if (auto callerHex = *callerHexResult) {
    auto callerResult = Uuid::create(*callerHex);
    if (!callerResult) return std::unexpected(std::move(callerResult.error()));
    auto callerUuid = *callerResult;
    if (m_secureTeams && callerUuid != playerUuid) {
      auto logged = m_root.logger().warn("TeamManager: Rejected updateStatus from {} spoofing {}", *callerHex, playerUuid.hex());
      if (!logged) return std::unexpected(std::move(logged.error()));
      return Json("unauthorized");
    }
  }

  if (auto teamUuid = getTeam(playerUuid)) {
    auto& team = m_teams.find_ref(*teamUuid)->get();
    auto& entry = team.members.find_ref(playerUuid)->get();
    auto hasName = arguments.contains("name");
    if (!hasName) return std::unexpected(std::move(hasName.error()));
    if (*hasName) {
      auto value = arguments.getString("name");
      if (!value) return std::unexpected(std::move(value.error()));
      entry.name = std::move(*value);
    }
    auto hasEntity = arguments.contains("entity");
    if (!hasEntity) return std::unexpected(std::move(hasEntity.error()));
    if (*hasEntity) {
      auto value = arguments.getInt("entity");
      if (!value) return std::unexpected(std::move(value.error()));
      entry.entity = *value;
    }
    auto health = arguments.getFloat("health");
    if (!health) return std::unexpected(std::move(health.error()));
    entry.healthPercentage = *health;
    auto energy = arguments.getFloat("energy");
    if (!energy) return std::unexpected(std::move(energy.error()));
    entry.energyPercentage = *energy;
    auto x = arguments.getFloat("x");
    if (!x) return std::unexpected(std::move(x.error()));
    entry.position[0] = *x;
    auto y = arguments.getFloat("y");
    if (!y) return std::unexpected(std::move(y.error()));
    entry.position[1] = *y;
    auto warpText = arguments.getString("warpMode");
    if (!warpText) return std::unexpected(std::move(warpText.error()));
    if (!WarpModeNames) return std::unexpected(WarpModeNames.error());
    auto warp = WarpModeNames->get_left(*warpText);
    if (!warp) return std::unexpected(std::move(warp.error()));
    entry.warpMode = warp->get();
    auto hasWorld = arguments.contains("world");
    if (!hasWorld) return std::unexpected(std::move(hasWorld.error()));
    if (*hasWorld) {
      auto worldText = arguments.getString("world");
      if (!worldText) return std::unexpected(std::move(worldText.error()));
      auto world = parseWorldId(*worldText);
      if (!world) return std::unexpected(std::move(world.error()));
      entry.world = *world;
    }
    auto hasPortrait = arguments.contains("portrait");
    if (!hasPortrait) return std::unexpected(std::move(hasPortrait.error()));
    if (*hasPortrait) {
      auto portraitJson = arguments.get("portrait");
      if (!portraitJson) return std::unexpected(std::move(portraitJson.error()));
      auto metadata = m_root.imageMetadataDatabase();
      if (!metadata) return std::unexpected(std::move(metadata.error()));
      auto portrait = jsonToList<Drawable>(*portraitJson, [&](Json const& value) { return Drawable::create(**metadata, value); });
      if (!portrait) return std::unexpected(std::move(portrait.error()));
      entry.portrait = std::move(*portrait);
    }
    auto hasVersion = arguments.contains("version");
    if (!hasVersion) return std::unexpected(std::move(hasVersion.error()));
    if (*hasVersion) {
      auto value = arguments.getInt("version");
      if (!value) return std::unexpected(std::move(value.error()));
      entry.version = *value;
    }
    return {};
  } else {
    return Json("notAMemberOfTeam");
  }
}

std::expected<Json, StarException> TeamManager::invite(Json const& arguments) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);
  auto inviteeText = arguments.getString("inviteeName");
  if (!inviteeText) return std::unexpected(std::move(inviteeText.error()));
  auto inviteeName = Text::stripEscapeCodes(*inviteeText);

  if (inviteeName.empty())
    return Json("inviteeNotFound");

  auto inviterUuidText = arguments.getString("inviterUuid");
  if (!inviterUuidText) return std::unexpected(std::move(inviterUuidText.error()));
  auto inviterUuidResult = Uuid::create(*inviterUuidText);
  if (!inviterUuidResult) return std::unexpected(std::move(inviterUuidResult.error()));
  auto inviterUuid = *inviterUuidResult;

  auto callerHexResult = arguments.optString("_callerUuid");
  if (!callerHexResult) return std::unexpected(std::move(callerHexResult.error()));
  if (auto callerHex = *callerHexResult) {
    auto callerResult = Uuid::create(*callerHex);
    if (!callerResult) return std::unexpected(std::move(callerResult.error()));
    auto callerUuid = *callerResult;
    if (m_secureTeams && callerUuid != inviterUuid) {
      auto logged = m_root.logger().warn("TeamManager: Rejected invite from {} spoofing inviter {}", *callerHex, inviterUuid.hex());
      if (!logged) return std::unexpected(std::move(logged.error()));
      return Json("unauthorized");
    }
  }

  JsonArray invited;
  for (auto&& entry : m_connectedPlayers) {
    auto matches = Text::stripEscapeCodes(entry.first).starts_with(inviteeName, String::CaseInsensitive);
    if (!matches) return std::unexpected(std::move(matches.error()));
    if (!*matches) continue;

    for (auto& inviteeUuid : entry.second) {
      Invitation invitation(m_root.random());
      invitation.inviterUuid = inviterUuid;
      auto inviterName = arguments.getString("inviterName");
      if (!inviterName) return std::unexpected(std::move(inviterName.error()));
      invitation.inviterName = *inviterName;
      m_invitations.try_emplace(inviteeUuid, m_root.random()).first->second = invitation;
      m_polledInvitations.remove(inviteeUuid);
      invited.push_back(JsonArray{entry.first, inviteeUuid.hex()});
    }
  }

  if (!invited.empty())
    return Json(invited);
  else
    return Json("inviteeNotFound");
}

std::expected<Json, StarException> TeamManager::pollInvitation(Json const& arguments) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);
  auto playerUuidText = arguments.getString("playerUuid");
  if (!playerUuidText) return std::unexpected(std::move(playerUuidText.error()));
  auto playerUuidResult = Uuid::create(*playerUuidText);
  if (!playerUuidResult) return std::unexpected(std::move(playerUuidResult.error()));
  auto playerUuid = *playerUuidResult;

  auto callerHexResult = arguments.optString("_callerUuid");
  if (!callerHexResult) return std::unexpected(std::move(callerHexResult.error()));
  if (auto callerHex = *callerHexResult) {
    auto callerResult = Uuid::create(*callerHex);
    if (!callerResult) return std::unexpected(std::move(callerResult.error()));
    auto callerUuid = *callerResult;
    if (m_secureTeams && callerUuid != playerUuid) {
      auto logged = m_root.logger().warn("TeamManager: Rejected pollInvitation from {} for {}", *callerHex, playerUuid.hex());
      if (!logged) return std::unexpected(std::move(logged.error()));
      return {};
    }
  }

  expirePolledInvitations();

  if (m_invitations.contains(playerUuid)) {
    auto invitation = m_invitations.extract_value(playerUuid);
    if (!invitation) return std::unexpected(std::move(invitation.error()));
    auto invite = std::move(*invitation);
    m_polledInvitations.try_emplace(playerUuid, m_root.random()).first->second = PolledInvitation{invite.inviterUuid, std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now())};
    JsonObject result;
    result["inviterUuid"] = invite.inviterUuid.hex();
    result["inviterName"] = invite.inviterName;
    return Json(result);
  }
  return {};
}

std::expected<Json, StarException> TeamManager::acceptInvitation(Json const& arguments) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);
  auto inviterUuidText = arguments.getString("inviterUuid");
  if (!inviterUuidText) return std::unexpected(std::move(inviterUuidText.error()));
  auto inviterUuidResult = Uuid::create(*inviterUuidText);
  if (!inviterUuidResult) return std::unexpected(std::move(inviterUuidResult.error()));
  auto inviterUuid = *inviterUuidResult;
  auto inviteeUuidText = arguments.getString("inviteeUuid");
  if (!inviteeUuidText) return std::unexpected(std::move(inviteeUuidText.error()));
  auto inviteeUuidResult = Uuid::create(*inviteeUuidText);
  if (!inviteeUuidResult) return std::unexpected(std::move(inviteeUuidResult.error()));
  auto inviteeUuid = *inviteeUuidResult;

  auto callerHexResult = arguments.optString("_callerUuid");
  if (!callerHexResult) return std::unexpected(std::move(callerHexResult.error()));
  if (auto callerHex = *callerHexResult) {
    auto callerResult = Uuid::create(*callerHex);
    if (!callerResult) return std::unexpected(std::move(callerResult.error()));
    auto callerUuid = *callerResult;
    if (m_secureTeams && callerUuid != inviteeUuid) {
      auto logged = m_root.logger().warn("TeamManager: Rejected acceptInvitation from {} spoofing invitee {}", *callerHex, inviteeUuid.hex());
      if (!logged) return std::unexpected(std::move(logged.error()));
      return Json("unauthorized");
    }
  }

  if (!playerWithUuidExists(inviterUuid) || !playerWithUuidExists(inviteeUuid))
    return Json("acceptInvitationFailed");

  expirePolledInvitations();

  auto polledIt = m_polledInvitations.find(inviteeUuid);
  if (polledIt == m_polledInvitations.end() || polledIt->second.inviterUuid != inviterUuid) {
    auto logged = m_root.logger().warn("TeamManager: Rejected acceptInvitation - no valid polled invitation from {} to {}", inviterUuid.hex(), inviteeUuid.hex());
    if (!logged) return std::unexpected(std::move(logged.error()));
    return Json("noInvitationPending");
  }

  m_polledInvitations.remove(inviteeUuid);

  purgeInvitationsFrom(inviteeUuid);

  Uuid teamUuid(m_root.random());
  if (auto existingTeamUuid = getTeam(inviterUuid))
    teamUuid = *existingTeamUuid;
  else
    teamUuid = createTeam(inviterUuid);

  auto success = addToTeam(inviteeUuid, teamUuid);
  return success ? Json() : "acceptInvitationFailed";
}

std::expected<Json, StarException> TeamManager::removeFromTeam(Json const& arguments) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);
  auto playerUuidText = arguments.getString("playerUuid");
  if (!playerUuidText) return std::unexpected(std::move(playerUuidText.error()));
  auto playerUuidResult = Uuid::create(*playerUuidText);
  if (!playerUuidResult) return std::unexpected(std::move(playerUuidResult.error()));
  auto playerUuid = *playerUuidResult;
  auto teamUuidText = arguments.getString("teamUuid");
  if (!teamUuidText) return std::unexpected(std::move(teamUuidText.error()));
  auto teamUuidResult = Uuid::create(*teamUuidText);
  if (!teamUuidResult) return std::unexpected(std::move(teamUuidResult.error()));
  auto teamUuid = *teamUuidResult;

  auto callerHexResult = arguments.optString("_callerUuid");
  if (!callerHexResult) return std::unexpected(std::move(callerHexResult.error()));
  if (auto callerHex = *callerHexResult) {
    auto callerResult = Uuid::create(*callerHex);
    if (!callerResult) return std::unexpected(std::move(callerResult.error()));
    auto callerUuid = *callerResult;
    if (m_secureTeams && callerUuid != playerUuid) {
      if (!m_teams.contains(teamUuid) || m_teams.find_ref(teamUuid)->get().leaderUuid != callerUuid) {
        auto logged = m_root.logger().warn("TeamManager: Rejected removeFromTeam by non-leader {} targeting {}", callerUuid.hex(), playerUuid.hex());
        if (!logged) return std::unexpected(std::move(logged.error()));
        return Json("unauthorized");
      }
    }
  }

  auto success = removeFromTeam(playerUuid, teamUuid);
  return success ? Json() : "removeFromTeamFailed";
}

std::expected<Json, StarException> TeamManager::makeLeader(Json const& arguments) {
  std::unique_lock lock(m_mutex, std::defer_lock);
  lockWithDiagnostics(lock);
  auto playerUuidText = arguments.getString("playerUuid");
  if (!playerUuidText) return std::unexpected(std::move(playerUuidText.error()));
  auto playerUuidResult = Uuid::create(*playerUuidText);
  if (!playerUuidResult) return std::unexpected(std::move(playerUuidResult.error()));
  auto playerUuid = *playerUuidResult;
  auto teamUuidText = arguments.getString("teamUuid");
  if (!teamUuidText) return std::unexpected(std::move(teamUuidText.error()));
  auto teamUuidResult = Uuid::create(*teamUuidText);
  if (!teamUuidResult) return std::unexpected(std::move(teamUuidResult.error()));
  auto teamUuid = *teamUuidResult;

  if (!m_teams.contains(teamUuid))
    return Json("noSuchTeam");

  auto& team = m_teams.find_ref(teamUuid)->get();

  auto callerHexResult = arguments.optString("_callerUuid");
  if (!callerHexResult) return std::unexpected(std::move(callerHexResult.error()));
  if (auto callerHex = *callerHexResult) {
    auto callerResult = Uuid::create(*callerHex);
    if (!callerResult) return std::unexpected(std::move(callerResult.error()));
    auto callerUuid = *callerResult;
    if (m_secureTeams && callerUuid != team.leaderUuid) {
      auto logged = m_root.logger().warn("TeamManager: Rejected makeLeader by non-leader {} in team {}", *callerHex, teamUuid.hex());
      if (!logged) return std::unexpected(std::move(logged.error()));
      return Json("unauthorized");
    }
  }

  if (!team.members.contains(playerUuid))
    return Json("notAMemberOfTeam");

  team.leaderUuid = playerUuid;

  return {};
}

}
