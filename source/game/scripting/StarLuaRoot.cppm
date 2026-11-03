module;
import std;
import star.format;
import star.string;
import star.thread;
import star.lua;
import star.listener;
import star.byte_array;
import star.type_identity;

namespace Star {

class Root;

class LuaExecutionScope final : public LuaExecutionHost {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<LuaExecutionScope, LuaExecutionHost>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return typeName<LuaExecutionScope>();
  }
  explicit LuaExecutionScope(Root& root) : m_root(root) {}
  Root& root() const { return m_root; }
private:
  Root& m_root;
};

[[nodiscard]] std::expected<std::reference_wrapper<Root>, StarException> luaExecutionRoot(LuaEngine& engine);

// Loads and caches Lua scripts from assets, clearing the cache on root reload.
// The internal engine and all contexts require single-threaded access.
// Root is borrowed and must outlive the engine and every returned context.
class LuaRoot {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<LuaRoot>, StarException> create(Root& root);
  ~LuaRoot();
  Root& root() const;

  bool scriptLoaded(String const& assetPath) const;

  [[nodiscard]] std::expected<void, StarException> restart();
  [[nodiscard]] std::expected<void, StarException> shutdown();

  // A script context can be created from the combination of several scripts,
  // the functions / data in each script will be loaded in order, so that later
  // specified scripts will overwrite previous ones.
  //
  // Top-level scripts and require share per-context load-once state. Modules
  // execute for side effects; cycles fail rather than expose partial modules.
  // Cache invalidation requires a fresh context to load any new module.
  // loadLifetime scopes entry-script async work and inherited callbacks; the host owns later execution.
  [[nodiscard]] std::expected<LuaContext, StarException> createContext(String const& script,
                                                                       std::stop_token loadLifetime = {});
  [[nodiscard]] std::expected<LuaContext, StarException> createContext(StringList const& scriptPaths = {},
                                                                       std::stop_token loadLifetime = {});

  [[nodiscard]] std::expected<void, StarException> collectGarbage(std::optional<unsigned> steps = {});
  void setAutoGarbageCollection(bool autoGarbageColleciton);
  void tuneAutoGarbageCollection(float pause, float stepMultiplier);
  std::size_t luaMemoryUsage() const;

  std::size_t scriptCacheMemoryUsage() const;

  void addCallbacks(String const& groupName, LuaCallbacks const& callbacks);

  LuaEngine& luaEngine() const;
  // Changes only after a replacement engine has been successfully published.
  std::uint64_t generation() const { return m_generation; }
private:
  explicit LuaRoot(Root& root);
  class ScriptCache {
  public:
    explicit ScriptCache(Root& root) : m_root(root) {}
    [[nodiscard]] std::expected<std::shared_ptr<ByteArray const>, StarException>
    script(LuaEngine& engine, String const& path, std::uint64_t revision, StringMap<String>& sourceDiagnostics);
    bool scriptLoaded(String const& path) const;
    void clear();
    std::uint64_t revision() const;
    std::size_t memoryUsage() const;

  private:
    Root& m_root;
    mutable std::mutex mutex;
    std::uint64_t m_revision = 0;
    StringMap<std::shared_ptr<ByteArray const>> scripts;
  };

  Root& m_root;
  LuaEnginePtr m_luaEngine;
  std::uint64_t m_generation = 0;
  StringMap<LuaCallbacks> m_luaCallbacks;
  std::shared_ptr<ScriptCache> m_scriptCache;
  std::shared_ptr<StringMap<String>> m_sourceDiagnostics;

  std::shared_ptr<Listener> m_rootReloadListener;
  ListenerSubscription m_rootReloadSubscription;

  String m_storageDirectory;
};

}

export module star.lua_root;

export namespace Star {
  using ::Star::LuaRoot;
  using ::Star::LuaExecutionScope;
  using ::Star::luaExecutionRoot;
}
