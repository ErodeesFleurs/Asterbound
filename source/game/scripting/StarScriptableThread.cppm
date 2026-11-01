module;
import std;
import star.format;
import star.string;
import star.list;
import star.json;
import star.lua;
import star.thread;
import star.lua_root;
import star.lua_components;
import star.game_types;
import star.rpc_promise;

namespace Star {

// Runs Lua on a separate thread. Worker failures are published through
// errorOccurred and logged once after teardown and accepted-message settlement.
class ScriptableThread : public Thread {
public:
  struct Message {
    String message;
    JsonArray args;
    RpcPromiseKeeper<Json> promise;
  };

  using ScriptComponent = LuaMessageScript;

  [[nodiscard]] static std::expected<std::shared_ptr<ScriptableThread>, StarException> create(Json parameters, LuaBaseComponent& parent);
  ~ScriptableThread();

  [[nodiscard]] std::expected<bool, StarException> start();
  // Stops and joins the worker; closing cancels queued messages, failures fail them.
  [[nodiscard]] std::expected<void, StarException> stop();
  void setPause(bool pause);

  // A script failure stopped the thread.
  bool errorOccurred();
  // Published only after terminal teardown/message settlement. The parent
  // joins before releasing its ownership, including failed worker startup.
  bool shouldExpire();

  // 
  [[nodiscard]] std::expected<void, StarException> passMessage(Message&& message);
  
  // expected to be run before the thread is started or after it is stopped!
  [[nodiscard]] std::expected<void, StarException> addCallbacks(String const& groupName, LuaCallbacks const& callbacks);
  [[nodiscard]] std::expected<void, StarException> removeCallbacks(String const& groupName);

protected:
  [[nodiscard]] std::expected<void, StarException> run() override;

private:
  ScriptableThread(Json parameters, String name, LuaBaseComponent& parent);
  [[nodiscard]] std::expected<void, StarException> update();
  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(String const& message, JsonArray const& args);

  mutable std::recursive_mutex m_mutex;
  
  std::shared_ptr<LuaRoot> m_luaRoot;
  StringMap<std::shared_ptr<ScriptableThread::ScriptComponent>> m_scriptContexts;
  
  Json m_parameters;
  String m_name;
  // log mapping can be disabled on scriptable threads to reduce debug clutter
  bool m_logMapped;
  
  float m_timestep;

  mutable std::recursive_mutex m_messageMutex;
  List<Message> m_messages;

  std::atomic<bool> m_stop;
  std::atomic<bool> m_pause;
  mutable std::atomic<bool> m_errorOccurred;
  mutable std::atomic<bool> m_shouldExpire;
  
  std::reference_wrapper<LuaBaseComponent> m_parent;
  
  StringMap<LuaCallbacks> m_threadCallbacks;
  
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeThreadCallbacks();
  [[nodiscard]] std::expected<Json, StarException> configValue(String const& name, Json def) const;
};

}

export module star.scriptable_thread;

export namespace Star {
  using ::Star::ScriptableThread;
}
