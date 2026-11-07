module;
import star.format;
import std;
import star.string;
import star.list;
import star.json;

import star.data_stream;
import star.uuid;

namespace Star {

class Root;

class Codex;
struct CodexEntry;
class UniverseClient;

class PlayerCodexes {
public:
  using CodexEntry = std::pair<std::shared_ptr<Codex const>, bool>;

  PlayerCodexes(Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<PlayerCodexes>, StarException> create(Root& root, Json const& json);

  Json toJson() const;

  List<CodexEntry> codexes() const;
  std::shared_ptr<Codex const> codex(String const& codexId) const;

  bool codexKnown(String const& codexId) const;
  [[nodiscard]] std::expected<std::shared_ptr<Codex const>, StarException> learnCodex(String const& codexId, bool markRead = false);

  bool codexRead(String const& codexId) const;
  bool markCodexRead(String const& codexId);
  bool markCodexUnread(String const& codexId);

  [[nodiscard]] std::expected<void, StarException> learnInitialCodexes(String const& playerSpecies);

  std::shared_ptr<Codex const> firstNewCodex() const;

private:
  Root& m_root;
  StringMap<CodexEntry> m_codexes;
};

}

export module star.player_codexes;

export namespace Star {
  using ::Star::Codex;
  using ::Star::CodexEntry;
  using ::Star::UniverseClient;
  using ::Star::PlayerCodexes;
}
