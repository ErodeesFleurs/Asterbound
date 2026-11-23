module;

import std;
import star.string;
import star.format;

namespace Star {
class Assets;
class Codex;

struct CodexDatabaseExceptionTag {
  static constexpr std::string_view name() { return "CodexDatabaseException"; }
};
using CodexDatabaseException = StarError<CodexDatabaseExceptionTag, StarException>;

class CodexDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  explicit CodexDatabase(ConstructionKey) {}
  CodexDatabase(CodexDatabase const&) = delete;
  CodexDatabase& operator=(CodexDatabase const&) = delete;
  [[nodiscard]] static std::expected<std::shared_ptr<CodexDatabase>, StarException> create(Assets const& assets);

  StringMap<std::shared_ptr<Codex const>> const& entries() const& { return m_entries; }
  StringMap<std::shared_ptr<Codex const>> const& entries() const&& = delete;
  std::shared_ptr<Codex const> find(String const& id) const;

private:
  StringMap<std::shared_ptr<Codex const>> m_entries;
};

}

export module star.codex_database;

export namespace Star {
  using ::Star::CodexDatabaseExceptionTag;
  using ::Star::CodexDatabaseException;
  using ::Star::CodexDatabase;
}
