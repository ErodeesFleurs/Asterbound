module;
import std;
import star.optional;
import star.string;
import star.string_view;
import star.lua;
import star.format;
import star.json;
import star.periodic;
import star.logging;
import star.output_proxy;
import star.thread;
import star.listener;
import star.game_types;
import star.list;
import star.rpc_promise;
import star.lua_root;
import star.lua_converters;
import star.lua_game_converters;
import star.world;
import star.root;
import star.algorithm;

namespace Star {

struct LuaComponentExceptionTag {
  static constexpr std::string_view name() { return "LuaComponentException"; }
};
using LuaComponentException = StarError<LuaComponentExceptionTag, LuaException>;

class ScriptableThread;
class LuaScriptWorld;

// Sole owner of a script session, including lifecycle, execution and reloads.
// Missing entry points succeed; invoke/message failures disable the session,
// while eval failures leave it usable. Active Lua frames are never retired.
class LuaBaseComponent {
public:
  explicit LuaBaseComponent(Root& root);
  Root& root() const;
  // Call uninit explicitly while callbacks are still valid. Destruction does
  // not invoke script entry points.
  virtual ~LuaBaseComponent();

  LuaBaseComponent(LuaBaseComponent const&) = delete;
  LuaBaseComponent& operator=(LuaBaseComponent const&) = delete;

  StringList const& scripts() const;
  [[nodiscard]] std::expected<void, StarException> setScript(String script);
  [[nodiscard]] std::expected<void, StarException> setScripts(StringList scripts);
  [[nodiscard]] std::expected<void, StarException> addCallbacks(String groupName, LuaCallbacks callbacks);
  [[nodiscard]] std::expected<bool, StarException> removeCallbacks(String const& groupName);
  [[nodiscard]] std::expected<void, StarException> addThreadCallbacks(String groupName, LuaCallbacks callbacks);
  [[nodiscard]] std::expected<bool, StarException> removeThreadCallbacks(String const& groupName);

  bool autoReInit() const;
  void setAutoReInit(bool enabled);
  [[nodiscard]] std::expected<void, StarException> setLuaRoot(std::shared_ptr<LuaRoot> luaRoot);
  std::shared_ptr<LuaRoot> const& luaRoot();

  // A missing LuaRoot is the existing successful false initialization outcome.
  [[nodiscard]] std::expected<bool, StarException> init();
  // Runs an optional owner lifecycle hook before uninit, without auto-reloading.
  [[nodiscard]] std::expected<void, StarException> uninit(String const& beforeUninit = {});
  bool initialized() const;
  [[nodiscard]] std::expected<bool, StarException> hasMethod(String const& name);

  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> invoke(String const& name, V&&... args);
  template <typename Ret = LuaValue>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> eval(String const& code);

  std::optional<String> const& error() const;
  std::optional<LuaContext> const& context() const;
  std::optional<LuaContext>& context();
  virtual RpcPromise<Json> threadPassMessage(String const& thread, String const& message, JsonArray const& args);

protected:
  [[nodiscard]] virtual std::expected<void, StarException> contextSetup();
  [[nodiscard]] virtual std::expected<void, StarException> contextShutdown();
  bool hasCallbacks(String const& groupName) const;
  enum class ErrorPolicy { KeepContext, DisableContext };
  enum class Entry { ReadyContext, Lifecycle, Update };
  template <typename Ret, typename Function>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> execute(StringView name, char const* operation,
                                                                         ErrorPolicy policy, Function&& function,
                                                                         Entry entry = Entry::ReadyContext);
  [[nodiscard]] std::expected<void, StarException> shutdownContext(String const& beforeUninit);
  [[nodiscard]] std::expected<void, StarException> retireContext();
  [[nodiscard]] std::expected<void, StarException> finishExecution();
  [[nodiscard]] std::expected<bool, StarException> checkInitialization();
  [[nodiscard]] virtual std::expected<LuaCallbacks, StarException> makeThreadsCallbacks();
  [[nodiscard]] std::expected<void, StarException> cleanThreads();
  [[nodiscard]] std::expected<void, StarException> stopThreads();

private:
  friend class LuaScriptStorage;
  friend class LuaScriptUpdate;
  friend class LuaScriptMessages;
  friend class LuaScriptAnimation;
  friend class LuaScriptWorld;

  Root& m_root;
  StringList m_scripts;
  StringMap<LuaCallbacks> m_callbacks;
  StringMap<LuaCallbacks> m_threadCallbacks;
  std::shared_ptr<LuaRoot> m_luaRoot;
  std::shared_ptr<TrackerListener> m_reloadTracker;
  ListenerSubscription m_reloadSubscription;
  std::uint64_t m_rootGeneration = 0;
  std::optional<LuaContext> m_context;
  std::stop_source m_callbackScope{std::nostopstate};
  unsigned m_activeCalls = 0;
  bool m_initPending = false;
  bool m_shutdownPending = false;
  bool m_shuttingDown = false;
  std::optional<String> m_pendingUninit;
  std::optional<std::reference_wrapper<LuaScriptWorld>> m_pendingWorldUninit;
  std::optional<StarException> m_boundaryError;
  std::optional<String> m_error;
  StringMap<std::shared_ptr<ScriptableThread>> m_threads;
};


// These fixed-address feature values borrow their owning context. They do not
// own a LuaRoot and cannot be detached, copied or installed as dynamic plugins.
class LuaScriptStorage {
public:
  explicit LuaScriptStorage(LuaBaseComponent& host) : m_host(host) {}
  LuaScriptStorage(LuaScriptStorage const&) = delete;
  LuaScriptStorage& operator=(LuaScriptStorage const&) = delete;
  [[nodiscard]] std::expected<JsonObject, StarException> getScriptStorage() const;
  [[nodiscard]] std::expected<void, StarException> setScriptStorage(JsonObject storage);
  [[nodiscard]] std::expected<void, StarException> contextSetup();
  [[nodiscard]] std::expected<JsonObject, StarException> readStorage() const;
  [[nodiscard]] std::expected<void, StarException> finishShutdown(std::expected<JsonObject, StarException> storage, std::expected<void, StarException> shutdown);
private:
  LuaBaseComponent& m_host;
  JsonObject m_storage;
};

class LuaScriptUpdate {
public:
  explicit LuaScriptUpdate(LuaBaseComponent& host);
  LuaScriptUpdate(LuaScriptUpdate const&) = delete;
  LuaScriptUpdate& operator=(LuaScriptUpdate const&) = delete;
  unsigned updateDelta() const;
  float updateDt(float dt) const;
  float updateDt() const;
  void setUpdateDelta(unsigned delta);
  bool updateReady() const;
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args);
  RpcPromise<Json> threadPassMessage(String const& thread, String const& message, JsonArray const& args);
  [[nodiscard]] std::expected<void, StarException> contextSetup();
  [[nodiscard]] std::expected<void, StarException> contextShutdown();
  [[nodiscard]] static std::expected<void, StarException> finishShutdown(std::expected<void, StarException> stopped, std::expected<void, StarException> shutdown);
  [[nodiscard]] std::expected<void, StarException> addThreadsCallbacks(LuaCallbacks& callbacks);
  // Called by the concrete host's destructor, while all callback targets live.
  void destroy();
private:
  struct ThreadMessage {
    String originThread;
    String message;
    JsonArray args;
    RpcPromiseKeeper<Json> promise;
  };
  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> handleThreadMessage(ThreadMessage const& message);
  LuaBaseComponent& m_host;
  Periodic m_updatePeriodic;
  mutable float m_lastDt;
  List<ThreadMessage> m_threadMessages;
  mutable std::recursive_mutex m_threadMessageMutex;
  struct ThreadMessageHandler {
    std::optional<LuaFunction> function;
    String name;
  };
  StringMap<ThreadMessageHandler> m_threadMessageHandlers;
};

class LuaScriptWorld {
public:
  explicit LuaScriptWorld(LuaBaseComponent& host) : m_host(host) {}
  LuaScriptWorld(LuaScriptWorld const&) = delete;
  LuaScriptWorld& operator=(LuaScriptWorld const&) = delete;
  [[nodiscard]] std::expected<void, StarException> init(World& world);
  [[nodiscard]] std::expected<void, StarException> uninit();
private:
  friend class LuaBaseComponent;
  [[nodiscard]] std::expected<void, StarException> finishShutdown();
  LuaBaseComponent& m_host;
  StringSet m_worldThreadCallbacks;
};

class LuaScriptMessages {
public:
  explicit LuaScriptMessages(LuaBaseComponent& host) : m_host(host) {}
  LuaScriptMessages(LuaScriptMessages const&) = delete;
  LuaScriptMessages& operator=(LuaScriptMessages const&) = delete;
  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> handleMessage(String const& message, bool localMessage, JsonArray const& args = {});
  [[nodiscard]] std::expected<void, StarException> contextSetup();
  void contextShutdown();
private:
  struct MessageHandler {
    std::optional<LuaFunction> function;
    String name;
    bool passName = true;
    bool localOnly = false;
  };
  LuaBaseComponent& m_host;
  StringMap<MessageHandler> m_messageHandlers;
};

template <typename Ret, typename Function>
[[nodiscard]] std::expected<std::optional<Ret>, StarException>
LuaBaseComponent::execute(StringView name, char const* operation, ErrorPolicy policy, Function&& function,
                          Entry entry) {
  if (entry != Entry::Lifecycle) {
    auto ready = checkInitialization();
    if (!ready)
      return std::unexpected(std::move(ready.error()));
    if (!*ready && entry == Entry::ReadyContext)
      return std::optional<Ret>{};
  }
  ++m_activeCalls;
  auto result = std::forward<Function>(function)();
  if (!result) {
    result.error() = result.error().withContext(strf("{} '{}'", operation, name));
    if (!m_boundaryError)
      m_boundaryError = result.error();
    if (policy == ErrorPolicy::DisableContext) {
      m_error = printException(result.error(), false);
      m_shutdownPending = true;
    }
  }
  if (--m_activeCalls == 0) {
    if (auto finished = finishExecution(); !finished) {
      auto error = std::move(finished.error());
      if (!result)
        error = error.withContext(strf("Original Lua failure: {}", outputException(result.error(), true)));
      return std::unexpected(std::move(error));
    }
  }
  return result;
}

template <typename Ret, typename... V>
[[nodiscard]] std::expected<std::optional<Ret>, StarException> LuaBaseComponent::invoke(String const& name,
                                                                                        V&&... args) {
  return execute<Ret>(name, "Exception while invoking Lua function", ErrorPolicy::DisableContext,
                      [&] { return m_context->template invokePathIfPresent<Ret>(name, std::forward<V>(args)...); });
}

template <typename Ret>
[[nodiscard]] std::expected<std::optional<Ret>, StarException> LuaBaseComponent::eval(String const& code) {
  return execute<Ret>({}, "Exception while evaluating Lua", ErrorPolicy::KeepContext,
                      [&]() -> std::expected<std::optional<Ret>, StarException> {
                        auto result = m_context->template eval<Ret>(code);
                        if (!result)
                          return std::unexpected(std::move(result.error()));
                        return std::optional<Ret>(std::move(*result));
                      });
}

template <typename Ret, typename... V>
[[nodiscard]] std::expected<std::optional<Ret>, StarException> LuaScriptUpdate::update(V&&... args) {
  return m_host.execute<Ret>(
    "update", "Exception while updating Lua session", LuaBaseComponent::ErrorPolicy::KeepContext,
    [&]() -> std::expected<std::optional<Ret>, StarException> {
      List<ThreadMessage> messages;
      {
        std::unique_lock locker(m_threadMessageMutex, std::defer_lock);
        lockWithDiagnostics(locker);
        messages = std::move(m_threadMessages);
      }
      std::optional<StarException> error;
      for (auto& message : messages) {
        auto response = handleThreadMessage(message);
        std::expected<void, StarException> settled;
        if (!response) {
          settled = message.promise.fail(printException(response.error(), false));
          if (!error)
            error = std::move(response.error());
        } else if (!*response) {
          settled = message.promise.fail("Message not handled");
        } else if (std::holds_alternative<RpcPromise<Json>>(**response)) {
          settled = message.promise.chain(std::get<RpcPromise<Json>>(**response));
        } else {
          settled = message.promise.fulfill(std::get<Json>(**response));
        }
        if (!settled) {
          if (error)
            error =
              error->withContext(strf("Error settling Lua thread message: {}", outputException(settled.error(), true)));
          else
            error = std::move(settled.error());
        }
      }
      if (error)
        return std::unexpected(std::move(*error));
      if (!m_updatePeriodic.tick())
        return std::optional<Ret>{};
      if (auto cleaned = m_host.cleanThreads(); !cleaned)
        return std::unexpected(std::move(cleaned.error()));
      return m_host.template invoke<Ret>("update", std::forward<V>(args)...);
    },
    LuaBaseComponent::Entry::Update);
}

// Fireable-item contexts need world callbacks, but no periodic update queue.
class LuaWorldScript final : public LuaBaseComponent {
public:
  explicit LuaWorldScript(Root& root) : LuaBaseComponent(root), m_world(*this) {}
  [[nodiscard]] std::expected<void, StarException> init(World& world) { return m_world.init(world); }
  [[nodiscard]] std::expected<void, StarException> uninit() { return m_world.uninit(); }
protected:
  using LuaBaseComponent::setLuaRoot;
private:
  LuaScriptWorld m_world;
};

// GUI scripts update without binding a world or exposing entity messages.
class LuaUpdateScript final : public LuaBaseComponent {
public:
  explicit LuaUpdateScript(Root& root) : LuaBaseComponent(root), m_storage(*this), m_update(*this) {}
  ~LuaUpdateScript() override { m_update.destroy(); }
  [[nodiscard]] std::expected<JsonObject, StarException> getScriptStorage() const { return m_storage.getScriptStorage(); }
  [[nodiscard]] std::expected<void, StarException> setScriptStorage(JsonObject storage) { return m_storage.setScriptStorage(std::move(storage)); }
  unsigned updateDelta() const { return m_update.updateDelta(); }
  float updateDt(float dt) const { return m_update.updateDt(dt); }
  float updateDt() const { return m_update.updateDt(); }
  void setUpdateDelta(unsigned delta) { m_update.setUpdateDelta(delta); }
  bool updateReady() const { return m_update.updateReady(); }
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    return m_update.template update<Ret>(std::forward<V>(args)...);
  }
  RpcPromise<Json> threadPassMessage(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.threadPassMessage(thread, message, args);
  }
protected:
  [[nodiscard]] std::expected<void, StarException> contextSetup() override {
    if (auto result = LuaBaseComponent::contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_storage.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> contextShutdown() override {
    auto storage = m_storage.readStorage();
    auto stopped = m_update.contextShutdown();
    return m_storage.finishShutdown(std::move(storage),
        LuaScriptUpdate::finishShutdown(std::move(stopped), LuaBaseComponent::contextShutdown()));
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeThreadsCallbacks() override {
    auto callbacks = LuaBaseComponent::makeThreadsCallbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.addThreadsCallbacks(*callbacks); !result)
      return std::unexpected(std::move(result.error()));
    return callbacks;
  }
private:
  LuaScriptStorage m_storage;
  LuaScriptUpdate m_update;
};

// Universe and worker-thread scripts update and handle messages without a world.
class LuaMessageScript final : public LuaBaseComponent {
public:
  explicit LuaMessageScript(Root& root) : LuaBaseComponent(root), m_update(*this), m_messages(*this) {}
  ~LuaMessageScript() override { m_update.destroy(); }
  unsigned updateDelta() const { return m_update.updateDelta(); }
  float updateDt(float dt) const { return m_update.updateDt(dt); }
  float updateDt() const { return m_update.updateDt(); }
  void setUpdateDelta(unsigned delta) { m_update.setUpdateDelta(delta); }
  bool updateReady() const { return m_update.updateReady(); }
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    return m_update.template update<Ret>(std::forward<V>(args)...);
  }
  RpcPromise<Json> threadPassMessage(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.threadPassMessage(thread, message, args);
  }
  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> handleMessage(String const& message, bool localMessage, JsonArray const& args = {}) {
    return m_messages.handleMessage(message, localMessage, args);
  }
protected:
  [[nodiscard]] std::expected<void, StarException> contextSetup() override {
    if (auto result = LuaBaseComponent::contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_messages.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> contextShutdown() override {
    m_messages.contextShutdown();
    auto stopped = m_update.contextShutdown();
    return LuaScriptUpdate::finishShutdown(std::move(stopped), LuaBaseComponent::contextShutdown());
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeThreadsCallbacks() override {
    auto callbacks = LuaBaseComponent::makeThreadsCallbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.addThreadsCallbacks(*callbacks); !result)
      return std::unexpected(std::move(result.error()));
    return callbacks;
  }
private:
  LuaScriptUpdate m_update;
  LuaScriptMessages m_messages;
};

// Container interfaces bind a world and update, without entity-message state.
class LuaWorldUpdateScript final : public LuaBaseComponent {
public:
  explicit LuaWorldUpdateScript(Root& root) : LuaBaseComponent(root), m_world(*this), m_update(*this) {}
  ~LuaWorldUpdateScript() override { m_update.destroy(); }
  [[nodiscard]] std::expected<void, StarException> init(World& world) { return m_world.init(world); }
  [[nodiscard]] std::expected<void, StarException> uninit() { return m_world.uninit(); }
  unsigned updateDelta() const { return m_update.updateDelta(); }
  float updateDt(float dt) const { return m_update.updateDt(dt); }
  float updateDt() const { return m_update.updateDt(); }
  void setUpdateDelta(unsigned delta) { m_update.setUpdateDelta(delta); }
  bool updateReady() const { return m_update.updateReady(); }
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    return m_update.template update<Ret>(std::forward<V>(args)...);
  }
  RpcPromise<Json> threadPassMessage(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.threadPassMessage(thread, message, args);
  }
protected:
  using LuaBaseComponent::setLuaRoot;
  [[nodiscard]] std::expected<void, StarException> contextSetup() override {
    if (auto result = LuaBaseComponent::contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> contextShutdown() override {
    auto stopped = m_update.contextShutdown();
    return LuaScriptUpdate::finishShutdown(std::move(stopped), LuaBaseComponent::contextShutdown());
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeThreadsCallbacks() override {
    auto callbacks = LuaBaseComponent::makeThreadsCallbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.addThreadsCallbacks(*callbacks); !result)
      return std::unexpected(std::move(result.error()));
    return callbacks;
  }
private:
  LuaScriptWorld m_world;
  LuaScriptUpdate m_update;
};

// Transient entities handle messages without allocating persistent storage.
class LuaEntityScript final : public LuaBaseComponent {
public:
  explicit LuaEntityScript(Root& root) : LuaBaseComponent(root), m_world(*this), m_update(*this), m_messages(*this) {}
  ~LuaEntityScript() override { m_update.destroy(); }
  [[nodiscard]] std::expected<void, StarException> init(World& world) { return m_world.init(world); }
  [[nodiscard]] std::expected<void, StarException> uninit() { return m_world.uninit(); }
  unsigned updateDelta() const { return m_update.updateDelta(); }
  float updateDt(float dt) const { return m_update.updateDt(dt); }
  float updateDt() const { return m_update.updateDt(); }
  void setUpdateDelta(unsigned delta) { m_update.setUpdateDelta(delta); }
  bool updateReady() const { return m_update.updateReady(); }
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    return m_update.template update<Ret>(std::forward<V>(args)...);
  }
  RpcPromise<Json> threadPassMessage(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.threadPassMessage(thread, message, args);
  }
  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> handleMessage(String const& message, bool localMessage, JsonArray const& args = {}) {
    return m_messages.handleMessage(message, localMessage, args);
  }
protected:
  using LuaBaseComponent::setLuaRoot;
  [[nodiscard]] std::expected<void, StarException> contextSetup() override {
    if (auto result = LuaBaseComponent::contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_messages.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> contextShutdown() override {
    m_messages.contextShutdown();
    auto stopped = m_update.contextShutdown();
    return LuaScriptUpdate::finishShutdown(std::move(stopped), LuaBaseComponent::contextShutdown());
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeThreadsCallbacks() override {
    auto callbacks = LuaBaseComponent::makeThreadsCallbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.addThreadsCallbacks(*callbacks); !result)
      return std::unexpected(std::move(result.error()));
    return callbacks;
  }
private:
  LuaScriptWorld m_world;
  LuaScriptUpdate m_update;
  LuaScriptMessages m_messages;
};

// Persistent entities snapshot storage before child-thread shutdown, then commit
// that snapshot after the producer queue has stopped and every promise is settled.
class LuaStoredEntityScript final : public LuaBaseComponent {
public:
  explicit LuaStoredEntityScript(Root& root) : LuaBaseComponent(root), m_world(*this), m_update(*this), m_storage(*this), m_messages(*this) {}
  ~LuaStoredEntityScript() override { m_update.destroy(); }
  [[nodiscard]] std::expected<void, StarException> init(World& world) { return m_world.init(world); }
  [[nodiscard]] std::expected<void, StarException> uninit() { return m_world.uninit(); }
  unsigned updateDelta() const { return m_update.updateDelta(); }
  float updateDt(float dt) const { return m_update.updateDt(dt); }
  float updateDt() const { return m_update.updateDt(); }
  void setUpdateDelta(unsigned delta) { m_update.setUpdateDelta(delta); }
  bool updateReady() const { return m_update.updateReady(); }
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    return m_update.template update<Ret>(std::forward<V>(args)...);
  }
  RpcPromise<Json> threadPassMessage(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.threadPassMessage(thread, message, args);
  }
  [[nodiscard]] std::expected<JsonObject, StarException> getScriptStorage() const { return m_storage.getScriptStorage(); }
  [[nodiscard]] std::expected<void, StarException> setScriptStorage(JsonObject storage) { return m_storage.setScriptStorage(std::move(storage)); }
  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> handleMessage(String const& message, bool localMessage, JsonArray const& args = {}) {
    return m_messages.handleMessage(message, localMessage, args);
  }
protected:
  using LuaBaseComponent::setLuaRoot;
  [[nodiscard]] std::expected<void, StarException> contextSetup() override {
    if (auto result = LuaBaseComponent::contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_storage.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_messages.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> contextShutdown() override {
    m_messages.contextShutdown();
    auto storage = m_storage.readStorage();
    auto stopped = m_update.contextShutdown();
    auto shutdown = LuaScriptUpdate::finishShutdown(std::move(stopped), LuaBaseComponent::contextShutdown());
    return m_storage.finishShutdown(std::move(storage), std::move(shutdown));
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeThreadsCallbacks() override {
    auto callbacks = LuaBaseComponent::makeThreadsCallbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.addThreadsCallbacks(*callbacks); !result)
      return std::unexpected(std::move(result.error()));
    return callbacks;
  }
private:
  LuaScriptWorld m_world;
  LuaScriptUpdate m_update;
  LuaScriptStorage m_storage;
  LuaScriptMessages m_messages;
};

}

export module star.lua_components;

export namespace Star {
  using ::Star::LuaComponentExceptionTag;
  using ::Star::LuaComponentException;
  using ::Star::LuaBaseComponent;
  using ::Star::LuaScriptStorage;
  using ::Star::LuaScriptUpdate;
  using ::Star::LuaScriptWorld;
  using ::Star::LuaScriptMessages;
  using ::Star::LuaWorldScript;
  using ::Star::LuaUpdateScript;
  using ::Star::LuaMessageScript;
  using ::Star::LuaWorldUpdateScript;
  using ::Star::LuaEntityScript;
  using ::Star::LuaStoredEntityScript;
}
