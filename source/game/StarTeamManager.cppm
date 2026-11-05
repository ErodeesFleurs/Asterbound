module;
import star.format;
import star.json;

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
import star.drawable;
import star.uuid;
import star.json_rpc;
import star.strong_typedef;
import star.vector;
import star.celestial_coordinate;
import star.game_types;
import star.warping;
import star.damage_types;
import std;
import star.map;
import star.version;

namespace Star {
  
extern VersionNumber const TeamManagerVersion;
class Root;
class RandomService;

class TeamManager {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<TeamManager>, StarException> create(Root& root);

  JsonRpcHandlers rpcHandlers();

  JsonRpcHandlers authenticatedRpcHandlers(Uuid const& callerUuid);

  void setConnectedPlayers(StringMap<List<Uuid>> connectedPlayers);
  void playerDisconnected(Uuid const& playerUuid);

  TeamNumber getPvpTeam(Uuid const& playerUuid);
  HashMap<Uuid, TeamNumber> getPvpTeams();
  std::optional<Uuid> getTeam(Uuid const& playerUuid) const;

private:
  TeamManager(Root& root, unsigned maxTeamSize, double invitationTimeout, bool secureTeams);
  struct TeamMember {
    String name;
    int entity;
    float healthPercentage;
    float energyPercentage;
    WorldId world;
    Vec2F position;
    WarpMode warpMode;
    
    int version = 0;

    List<Drawable> portrait;
  };

  struct Team {
    explicit Team(RandomService& random) : leaderUuid(random) {}

    Uuid leaderUuid;
    TeamNumber pvpTeamNumber;

    Map<Uuid, TeamMember> members;
  };

  struct Invitation {
    explicit Invitation(RandomService& random) : inviterUuid(random) {}

    Uuid inviterUuid;
    String inviterName;
  };

  struct PolledInvitation {
    explicit PolledInvitation(RandomService& random) : inviterUuid(random) {}
    PolledInvitation(Uuid uuid, std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> time) : inviterUuid(std::move(uuid)), polledAt(time) {}

    Uuid inviterUuid;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> polledAt;
  };

  void purgeInvitationsFor(Uuid const& playerUuid);
  void purgeInvitationsFrom(Uuid const& playerUuid);
  void expirePolledInvitations();

  bool playerWithUuidExists(Uuid const& playerUuid) const;

  Uuid createTeam(Uuid const& leaderUuid);
  bool addToTeam(Uuid const& playerUuid, Uuid const& teamUuid);
  bool removeFromTeam(Uuid const& playerUuid, Uuid const& teamUuid);

  Root& m_root;
  std::recursive_mutex m_mutex;
  Map<Uuid, Team> m_teams;
  StringMap<List<Uuid>> m_connectedPlayers;
  Map<Uuid, Invitation> m_invitations;
  Map<Uuid, PolledInvitation> m_polledInvitations;

  unsigned m_maxTeamSize;
  std::chrono::duration<double> m_polledInvitationTimeout;
  bool m_secureTeams;

  TeamNumber m_pvpTeamCounter;

  [[nodiscard]] std::expected<Json, StarException> fetchTeamStatus(Json const& args);
  [[nodiscard]] std::expected<Json, StarException> updateStatus(Json const& args);
  [[nodiscard]] std::expected<Json, StarException> invite(Json const& args);
  [[nodiscard]] std::expected<Json, StarException> pollInvitation(Json const& args);
  [[nodiscard]] std::expected<Json, StarException> acceptInvitation(Json const& args);
  [[nodiscard]] std::expected<Json, StarException> removeFromTeam(Json const& args);
  [[nodiscard]] std::expected<Json, StarException> makeLeader(Json const& args);
};

}

export module star.team_manager;

export namespace Star {
using ::Star::TeamManagerVersion;
using ::Star::TeamManager;
}
