module;
import std;
import star.format;
import star.string;
import star.list;
import star.json;

import star.set;
import star.game_types;
import star.message_types;
import star.strong_typedef;
import star.data_stream;
import star.bi_map;
import star.item_descriptor;
import star.vector;
import star.celestial_coordinate;
import star.quest_descriptor;

namespace Star {

class Root;

class Quest;
class Player;
class World;
class UniverseClient;
class Entity;

struct QuestIndicator {
  String indicatorImage;
  Vec2F worldPosition;
};

class QuestManager {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<QuestManager>, StarException> create(Root& root, Player& player);

  [[nodiscard]] std::expected<void, StarException> diskLoad(Json const& quests);
  [[nodiscard]] std::expected<Json, StarException> diskStore();

  void setUniverseClient(std::optional<std::reference_wrapper<UniverseClient>> client);

  [[nodiscard]] std::expected<void, StarException> init(Player& player, World& world);
  [[nodiscard]] std::expected<void, StarException> uninit();
  // Terminal borrow clearing after world/script uninitialization.
  void detachPlayer();

  [[nodiscard]] std::expected<bool, StarException> canStart(QuestArcDescriptor const& questArc) const;

  // Show a dialog offering the player a quest, and later start it if they
  // accept it.
  [[nodiscard]] std::expected<void, StarException> offer(std::shared_ptr<Quest> const& quest);
  StringMap<std::shared_ptr<Quest>> quests() const;
  // Only returns quests that are exclusive to the current server.
  StringMap<std::shared_ptr<Quest>> serverQuests() const;
  [[nodiscard]] std::expected<std::shared_ptr<Quest>, StarException> getQuest(String const& questId) const;

  bool hasQuest(String const& questId) const;
  bool hasAcceptedQuest(String const& questId) const;
  bool isActive(String const& questId) const;
  bool isCurrent(String const& questId) const;
  bool isTracked(String const& questId) const;
  void setAsTracked(std::optional<String> const& questId);
  void markAsRead(String const& questId);
  bool hasCompleted(String const& questId) const;
  bool canTurnIn(String const& questId) const;

  std::optional<std::shared_ptr<Quest>> getFirstNewQuest();
  std::optional<std::shared_ptr<Quest>> getFirstCompletableQuest();
  std::optional<std::shared_ptr<Quest>> getFirstFailableQuest();
  std::optional<std::shared_ptr<Quest>> getFirstMainQuest();

  // Decision leases retain the exact quest allocation, independently of its
  // public ID and of ephemeral quest removal after acknowledgment.
  [[nodiscard]] std::expected<Json, StarException> pendingDialog(String const& kind);
  [[nodiscard]] std::expected<void, StarException> previewDialog(std::shared_ptr<Quest> quest, String const& kind);
  bool dialogValid(std::uint64_t token) const;
  bool acknowledgeDialog(std::uint64_t token);
  [[nodiscard]] std::expected<Json, StarException> dialogCinema(std::uint64_t token) const;
  [[nodiscard]] std::expected<bool, StarException> finishDialog(std::uint64_t token, String const& decision);

  List<std::shared_ptr<Quest>> listActiveQuests() const;
  List<std::shared_ptr<Quest>> listCompletedQuests() const;
  List<std::shared_ptr<Quest>> listFailedQuests() const;

  std::optional<String> currentQuestId() const;
  std::optional<std::shared_ptr<Quest>> currentQuest() const;
  std::optional<String> trackedQuestId() const;
  std::optional<std::shared_ptr<Quest>> trackedQuest() const;
  [[nodiscard]] std::expected<std::optional<QuestIndicator>, StarException> getQuestIndicator(std::shared_ptr<Entity> const& entity) const;

  // Handled at this level to allow multiple active quests to specify interestingObjects
  [[nodiscard]] std::expected<StringSet, StarException> interestingObjects();

  [[nodiscard]] MessageResponseResult receive_message(String const& message, bool local_message, JsonArray const& args = {});
  [[nodiscard]] std::expected<void, StarException> update(float dt);

private:
  QuestManager(Root& root, Player& player);
  Root& m_root;
  [[nodiscard]] std::expected<void, StarException> startInitialQuests();
  void setMostRecentQuestCurrent();
  bool questValidOnServer(std::shared_ptr<Quest> const& quest) const;

  std::optional<std::reference_wrapper<Player>> m_player;
  std::optional<std::reference_wrapper<World>> m_world;
  std::optional<std::reference_wrapper<UniverseClient>> m_client;

  StringMap<std::shared_ptr<Quest>> m_quests;
  struct DialogLease {
    std::shared_ptr<Quest> quest;
    String kind;
    std::uint64_t stateRevision;
    bool preview;
    bool acknowledged = false;
  };
  [[nodiscard]] std::expected<Json, StarException> dialogView(std::uint64_t token, DialogLease const& lease) const;
  std::map<std::uint64_t, DialogLease> m_dialogs;

  std::optional<String> m_trackedQuestId;
  bool m_trackOnWorldQuests;
  std::optional<String> m_onWorldQuestId;
};

}

export module star.quest_manager;

export namespace Star {
using ::Star::Quest;
using ::Star::Player;
using ::Star::World;
using ::Star::UniverseClient;
using ::Star::Entity;
using ::Star::QuestManager;
using ::Star::QuestIndicator;
}
