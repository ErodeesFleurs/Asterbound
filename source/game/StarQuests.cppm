module;
import std;
import star.format;
import star.lua;
import star.json;

import star.set;
import star.json_rpc;
import star.data_stream;
import star.bi_map;
import star.item_descriptor;
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
import star.vector;
import star.celestial_coordinate;
import star.game_types;
import star.strong_typedef;
import star.quest_descriptor;
import star.lua_components;
import star.lua_actor_movement_component;
import star.strong_typedef;
import star.uuid;
import star.warping;
import star.system_world;

namespace Star {

class Root;

class Player;
class UniverseClient;
class QuestTemplate;
class World;
class Entity;

enum class QuestState {
  // New - being set up and quest hasn't been offered yet (or was offered and declined)
  New,
  // Offer - waiting on the player to accept or decline the quest
  Offer,
  // Active - the quest was accepted and is in progress
  Active,
  // Complete - the quest finished successfully
  Complete,
  // Failed - the quest finished unsuccessfully or the player abandoned it
  Failed
};
extern std::expected<EnumMap<QuestState>, StarException> const QuestStateNames;

class Quest {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<Quest>, StarException> create(Root& root, QuestArcDescriptor const& questArc, std::size_t arcPos, Player& player);

  [[nodiscard]] static std::expected<std::shared_ptr<Quest>, StarException> create(Root& root, Json const& diskStore);
  [[nodiscard]] std::expected<Json, StarException> diskStore() const;

  [[nodiscard]] std::expected<std::shared_ptr<QuestTemplate>, StarException> getTemplate() const;

  [[nodiscard]] std::expected<void, StarException> init(Player& player, World& world, std::optional<std::reference_wrapper<UniverseClient>> client);
  [[nodiscard]] std::expected<void, StarException> uninit();

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(String const& message, bool localMessage, JsonArray const& args = {});
  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> callScript(String const& func, LuaVariadic<LuaValue> const& args);
  [[nodiscard]] std::expected<void, StarException> update(float dt);

  [[nodiscard]] std::expected<void, StarException> offer();
  [[nodiscard]] std::expected<void, StarException> declineOffer();
  [[nodiscard]] std::expected<void, StarException> cancelOffer();
  [[nodiscard]] std::expected<void, StarException> start();
  [[nodiscard]] std::expected<void, StarException> complete(std::optional<std::size_t> followupIndex = {});
  [[nodiscard]] std::expected<void, StarException> fail();
  [[nodiscard]] std::expected<void, StarException> abandon();

  [[nodiscard]] std::expected<bool, StarException> interactWithEntity(EntityId entity);

  // The generated ID for this instance of the quest with these specific
  // parameters. Multiple players iin a universe may have quests with the same
  // questId if the the source of the quest was the same.
  String questId() const;
  // The ID of the template this quest was created from
  String templateId() const;

  StringMap<QuestParam> const& parameters() const;

  QuestState state() const;
  std::uint64_t stateRevision() const { return m_stateRevision; }

  // Whether to show the Complete / Failed dialog
  bool showDialog() const;
  void setDialogShown();

  [[nodiscard]] std::expected<void, StarException> setEntityParameter(String const& paramName, std::shared_ptr<Entity const> const& entity);
  void setParameter(String const& paramName, QuestParam const& paramValue);

  std::optional<List<Drawable>> portrait(String const& portraitName) const;
  std::optional<String> portraitTitle(String const& portraitName) const;

  QuestDescriptor questDescriptor() const;
  QuestArcDescriptor questArcDescriptor() const;
  std::size_t questArcPosition() const;

  std::optional<WorldId> worldId() const;
  std::optional<std::pair<Vec3I, SystemLocation>> location() const;
  std::optional<Uuid> serverUuid() const;
  void setWorldId(std::optional<WorldId> worldId);
  void setLocation(std::optional<std::pair<Vec3I, SystemLocation>> location);
  void setServerUuid(std::optional<Uuid> serverUuid);

  String title() const;
  String text() const;
  String completionText() const;
  String failureText() const;

  std::size_t money() const;
  List<std::shared_ptr<Item const>> const& rewards() const;
  std::uint64_t presentationRevision() const;
  std::uint64_t portraitRevision() const;
  std::uint64_t rewardsRevision() const;

  // The time when this quest last changed state (active/completed/failed)
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> lastUpdatedOn() const;
  bool unread() const;
  void markAsRead();
  bool canTurnIn() const;

  [[nodiscard]] std::expected<String, StarException> questGiverIndicator() const;
  [[nodiscard]] std::expected<String, StarException> questReceiverIndicator() const;

  // The String returned by this method is an image path, not a reference to a configured indicator
  [[nodiscard]] std::expected<std::optional<String>, StarException> customIndicator(std::shared_ptr<Entity> const& entity) const;

  std::optional<JsonArray> const& objectiveList() const;
  std::optional<float> progress() const;
  std::optional<float> compassDirection() const;

  void setObjectiveList(std::optional<JsonArray> const& objectiveList);
  void setProgress(std::optional<float> const& progress);
  void setCompassDirection(std::optional<float> const& compassDirection);

  [[nodiscard]] std::expected<std::optional<String>, StarException> completionCinema() const;
  [[nodiscard]] std::expected<bool, StarException> canBeAbandoned() const;
  bool ephemeral() const;
  bool showInLog() const;
  bool showAcceptDialog() const;
  bool showCompleteDialog() const;
  bool showFailDialog() const;
  bool mainQuest() const;
  bool hideCrossServer() const;

private:
  friend class QuestManager;
  void detachPlayer();
  explicit Quest(Root& root);
  Root& m_root;
  struct DisplayParameters {
    bool ephemeral;
    bool showInLog;
    bool showAcceptDialog;
    bool showCompleteDialog;
    bool showFailDialog;
    bool mainQuest;
    bool hideCrossServer;
  };

  void setState(QuestState state);

  [[nodiscard]] std::expected<void, StarException> initScript();
  [[nodiscard]] std::expected<void, StarException> initScriptBindings();
  [[nodiscard]] std::expected<void, StarException> uninitScript();
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeQuestCallbacks(Player& player);

  [[nodiscard]] std::expected<void, StarException> setEntityParameter(String const& paramName, Entity const& entity);
  [[nodiscard]] std::expected<void, StarException> addReward(ItemDescriptor const& reward);

  String const& defaultCustomIndicator() const;

  std::optional<std::reference_wrapper<Player>> m_player;
  std::optional<std::reference_wrapper<World>> m_world;
  std::optional<std::reference_wrapper<UniverseClient>> m_client;

  QuestState m_state;
  bool m_inited;
  bool m_showDialog;

  QuestArcDescriptor m_arc;
  std::size_t m_arcPos;
  StringMap<QuestParam> m_parameters;
  DisplayParameters m_displayParameters;
  std::optional<WorldId> m_worldId;
  std::optional<std::pair<Vec3I, SystemLocation>> m_location;
  std::optional<Uuid> m_serverUuid;
  std::size_t m_money;
  List<std::shared_ptr<Item const>> m_rewards;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_lastUpdatedOn;
  bool m_unread;
  bool m_canTurnIn;
  StringSet m_indicators;

  String m_trackedIndicator;
  String m_untrackedIndicator;

  String m_title;
  String m_text;
  String m_completionText;
  String m_failureText;
  StringMap<List<Drawable>> m_portraits;
  StringMap<String> m_portraitTitles;

  std::uint64_t m_presentationRevision;
  std::uint64_t m_portraitRevision;
  std::uint64_t m_rewardsRevision;
  std::uint64_t m_stateRevision;

  std::optional<JsonArray> m_objectiveList;
  std::optional<float> m_progress;
  std::optional<float> m_compassDirection;

  LuaActionScript
      m_scriptComponent;
};

// Create an instance of Quest for a specific template with all the parameters filled
// in with examples. Doesn't necessarily make a valid quest that can be completed, since
// its purpose is for previewing dialogs only.
[[nodiscard]] std::expected<std::shared_ptr<Quest>, StarException> createPreviewQuest(
    Root& root, String const& templateId, String const& position, String const& questGiverSpecies, Player& player);
}

export module star.quests;

export namespace Star {
using ::Star::Quest;
using ::Star::Player;
using ::Star::UniverseClient;
using ::Star::QuestTemplate;
using ::Star::QuestState;
using ::Star::QuestStateNames;
using ::Star::createPreviewQuest;
}
