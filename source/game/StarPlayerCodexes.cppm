module;
import star.format;
import std;
import star.string;
import star.list;
import star.json;

namespace Star {

class Root;

class Codex;

struct CodexEntry {
  std::shared_ptr<Codex const> codex;
  bool read;
};

enum class CodexLearnResult { Learned, AlreadyKnown, NotFound };

class PlayerCodexes {
public:
  explicit PlayerCodexes(Root& root);
  PlayerCodexes(PlayerCodexes const&) = delete;
  PlayerCodexes& operator=(PlayerCodexes const&) = delete;
  [[nodiscard]] static std::expected<std::shared_ptr<PlayerCodexes>, StarException> create(Root& root, Json const& json);

  Json to_json() const;

  // A player load may replace the domain even when UUID and revision are unchanged.
  std::uint64_t domain_id() const { return m_domain_id; }
  std::uint64_t revision() const { return m_revision; }
  List<CodexEntry> entries() const;
  std::shared_ptr<Codex const> find(String const& id) const;
  bool contains(String const& id) const;
  bool is_read(String const& id) const;
  bool set_read(String const& id, bool read);
  std::shared_ptr<Codex const> first_unread() const;

  [[nodiscard]] std::expected<CodexLearnResult, StarException> learn(String const& id, bool read = false);
  [[nodiscard]] std::expected<void, StarException> learn_initial(String const& species);

private:
  CodexLearnResult insert(String const& id, std::shared_ptr<Codex const> codex, bool read);

  Root& m_root;
  StringMap<CodexEntry> m_codexes;
  std::uint64_t const m_domain_id;
  std::uint64_t m_revision = 0;
  std::size_t m_unread_count = 0;
};

}

export module star.player_codexes;

export namespace Star {
  using ::Star::Codex;
  using ::Star::CodexEntry;
  using ::Star::CodexLearnResult;
  using ::Star::PlayerCodexes;
}
