module;
import std;
import star.format;
import star.string;
import star.json;

// Match client include order for SIMD intrinsics used by xxhash.
import star.data_stream;
import star.lua;
import star.uuid;

module star.team_client_lua_bindings;
import star.team_client;

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeTeamClientCallbacks(TeamClient& teamClient) {
  LuaCallbacks callbacks;

  if (auto result = callbacks.register_callback("isMemberOfTeam", [&teamClient] { return teamClient.isMemberOfTeam(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("invitePlayer", [&teamClient](String const& name) { return teamClient.invitePlayer(name); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("acceptInvitation", [&teamClient](String const& inviterUuid) -> std::expected<void, StarException> {
      auto uuid = Uuid::from_hex((inviterUuid).utf8());
      if (!uuid) return std::unexpected(std::move(uuid.error()));
      return teamClient.acceptInvitation(*uuid);
    }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("hasInvitationPending", [&teamClient] { return teamClient.hasInvitationPending(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("pullInvitation", [&teamClient]() -> std::optional<Json> {
      if (!teamClient.hasInvitationPending())
        return {};
      auto invitation = teamClient.pullInvitation();
      return JsonObject{{"uuid", invitation.first.hex()}, {"name", std::move(invitation.second)}};
    }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("members", [&teamClient] { return teamClient.memberStatus(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("memberPortrait", [&teamClient](String const& playerUuid, std::optional<std::uint64_t> knownRevision) -> std::expected<std::optional<Json>, StarException> {
      auto uuid = Uuid::from_hex((playerUuid).utf8());
      if (!uuid) return std::unexpected(std::move(uuid.error()));
      return teamClient.memberPortrait(*uuid, knownRevision);
    }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("isTeamLeader", [&teamClient](std::optional<String> const& playerUuid) -> std::expected<bool, StarException> {
      if (playerUuid) {
        auto uuid = Uuid::from_hex((*playerUuid).utf8());
        if (!uuid)
          return std::unexpected(std::move(uuid.error()));
        return teamClient.isTeamLeader(*uuid);
      }
      return teamClient.isTeamLeader();
    }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("currentTeam", [&teamClient]() -> std::optional<String> {
      auto teamUuid = teamClient.currentTeam();
      if (teamUuid)
        return teamUuid->hex();
      return {};
    }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("makeLeader", [&teamClient](String const& playerUuid) -> std::expected<void, StarException> {
      auto uuid = Uuid::from_hex((playerUuid).utf8());
      if (!uuid)
        return std::unexpected(std::move(uuid.error()));
      return teamClient.makeLeader(*uuid);
    }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("removeFromTeam", [&teamClient](String const& playerUuid) -> std::expected<void, StarException> {
      auto uuid = Uuid::from_hex((playerUuid).utf8());
      if (!uuid)
        return std::unexpected(std::move(uuid.error()));
      return teamClient.removeFromTeam(*uuid);
    }); !result)
    return std::unexpected(std::move(result.error()));

  return callbacks;
}

}
