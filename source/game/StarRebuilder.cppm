module;

import std;
import star.format;
import star.string;
import star.list;
import star.json;
import star.thread;

namespace Star {

class LuaContext;
class LuaRoot;
class Root;
class ItemDatabase;

class Rebuilder {
public:
  [[nodiscard]]
  static std::expected<std::shared_ptr<Rebuilder>, StarException> create(Root& root, String const& id, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog = std::nullopt);
  ~Rebuilder() = default;

  using AttemptCallback = std::function<std::expected<void, StarException>(Json const&)>;
  [[nodiscard]]
  std::expected<bool, StarException> rebuild(Json store, StarException lastError, AttemptCallback attempt) const;

private:
  Rebuilder() = default;
  std::shared_ptr<LuaRoot> m_luaRoot;
  mutable std::recursive_mutex m_luaMutex;
  std::shared_ptr<List<LuaContext>> m_contexts;
};

}

export module star.rebuilder;

export namespace Star {
  using ::Star::LuaContext;
  using ::Star::LuaRoot;
  using ::Star::Rebuilder;
}
