module;
import star.format;
import star.json;

import star.data_stream;
import star.uuid;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.drawable;
import star.strong_typedef;
import star.vector;
import star.celestial_coordinate;
import star.rpc_promise;
import star.game_types;
import star.warping;
import star.json_rpc;
import std;
import star.version;

namespace Star {

extern VersionNumber const TeamClientVersion;
  
class Root;
class Player;
class ClientContext;

class TeamClient {
public:
  struct Member {
    String name;
    Uuid uuid;
    int entity;
    float healthPercentage;
    float energyPercentage;
    WorldId world;
    Vec2F position;
    WarpMode warpMode;
    List<Drawable> portrait;
    std::uint64_t portraitRevision = 0;
  };

  TeamClient(Root& root, std::shared_ptr<Player> mainPlayer, std::shared_ptr<ClientContext> clientContext);

  [[nodiscard]] std::expected<void, StarException> invitePlayer(String const& playerName);
  [[nodiscard]] std::expected<void, StarException> acceptInvitation(Uuid const& inviterUuid);

  std::optional<Uuid> currentTeam() const;

  [[nodiscard]] std::expected<void, StarException> makeLeader(Uuid const& playerUuid);
  [[nodiscard]] std::expected<void, StarException> removeFromTeam(Uuid const& playerUuid);

  bool isTeamLeader();
  bool isTeamLeader(Uuid const& playerUuid);
  bool isMemberOfTeam();

  bool hasInvitationPending();
  std::pair<Uuid, String> pullInvitation();
  List<std::variant<std::pair<String, bool>, StringList>> pullInviteResults();

  [[nodiscard]] std::expected<void, StarException> update();

  [[nodiscard]] std::expected<void, StarException> pullFullUpdate();
  [[nodiscard]] std::expected<void, StarException> statusUpdate();

  void forceUpdate();

  List<Member> const& members() const;
  JsonArray memberStatus() const;
  [[nodiscard]] std::expected<std::optional<Json>, StarException> memberPortrait(Uuid const& uuid, std::optional<std::uint64_t> knownRevision) const;

private:
  typedef std::pair<RpcPromise<Json>, std::function<std::expected<void, StarException>(Json const&)>> RpcResponseHandler;

  [[nodiscard]] std::expected<void, StarException> invokeRemote(String const& method, Json const& args,
      std::function<std::expected<void, StarException>(Json const&)> responseFunction = {});
  [[nodiscard]] std::expected<void, StarException> handleRpcResponses();

  [[nodiscard]] std::expected<void, StarException> writePlayerData(JsonObject& request, std::shared_ptr<Player> player, bool fullWrite = false) const;

  void clearTeam();

  Root& m_root;
  std::shared_ptr<Player> m_mainPlayer;
  std::shared_ptr<ClientContext> m_clientContext;
  std::optional<Uuid> m_teamUuid;

  Uuid m_teamLeader;

  List<Member> m_members;
  std::uint64_t m_nextPortraitRevision = 0;

  bool m_hasPendingInvitation;
  std::pair<Uuid, String> m_pendingInvitation;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> m_pollInvitationsTimer;
  List<std::variant<std::pair<String, bool>, StringList>> m_pendingInviteResults;

  bool m_fullUpdateRunning;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> m_fullUpdateTimer;

  bool m_statusUpdateRunning;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> m_statusUpdateTimer;
  std::optional<std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>>> m_lastFullStatusUpdate;
  bool m_fullStatusRequired = true;

  List<RpcResponseHandler> m_pendingResponses;
  
  int m_teamManagerVersion = 0;
};

}

export module star.team_client;

export namespace Star {
using ::Star::TeamClientVersion;
using ::Star::Player;
using ::Star::ClientContext;
using ::Star::TeamClient;
}
