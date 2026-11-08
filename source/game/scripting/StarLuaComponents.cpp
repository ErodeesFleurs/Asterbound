import star.json;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.output_proxy;
import star.listener;
import std;
import star.string;
import star.lua;
import star.format;
import star.version;
import star.rpc_promise;

import star.lua_components;
import star.lua_root;
import star.lua_converters;
import star.lua_game_converters;
import star.world;
import star.world_lua_bindings;
import star.periodic;
import star.game_types;
import star.uuid;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.utility_lua_bindings;
import star.scriptable_thread;
import star.root_lua_bindings;
import star.algorithm;

namespace Star {

LuaBaseComponent::LuaBaseComponent(Root& root) : m_root(root) {
  setAutoReInit(true);
}

LuaBaseComponent::~LuaBaseComponent() {
  for (auto&& group : m_callbacks)
    group.second.scope.request_stop();
  (void)m_callbackScope.request_stop();
  if (auto stopped = stopThreads(); !stopped)
    EmergencyLog::error("Error stopping Lua component threads: {}", outputException(stopped.error(), true));
  m_threads.clear();
}

Root& LuaBaseComponent::root() const { return m_root; }
StringList const& LuaBaseComponent::scripts() const { return m_scripts; }

std::expected<void, StarException> LuaBaseComponent::setScript(String script) {
  return setScripts({std::move(script)});
}

std::expected<void, StarException> LuaBaseComponent::setScripts(StringList scripts) {
  if (initialized())
    return std::unexpected(LuaComponentException("Cannot call LuaBaseComponent::setScripts when initialized"));
  m_scripts = std::move(scripts);
  return {};
}

std::expected<void, StarException> LuaBaseComponent::addCallbacks(String groupName, LuaCallbacks callbacks) {
  if (m_callbacks.contains(groupName))
    return std::unexpected(LuaComponentException::format("Duplicate callbacks named '{}' in LuaBaseComponent", groupName));
  CallbackGroup group{.callbacks = std::move(callbacks), .scope = {}};
  // Installation can run Lua metamethods; revoke escaped values on rollback or session retirement.
  auto retire = [scope = group.scope]() mutable { scope.request_stop(); };
  auto rollback = finally([&] { retire(); });
  std::stop_callback duringInstallation(m_context ? m_callbackScope.get_token() : std::stop_token{}, retire);
  if (m_context) {
    // Component-owned groups override inherited root/script tables.
    if (auto result = m_context->remove(groupName); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_context->setCallbacks(groupName, group.callbacks.withLifetime(group.scope.get_token()));
        !result)
      return std::unexpected(std::move(result.error()));
  }
  if (group.scope.stop_requested())
    return std::unexpected(LuaComponentException("Lua session retired during callback installation"));
  if (!m_callbacks.insert(std::move(groupName), std::move(group)).second)
    return std::unexpected(LuaComponentException("Callback group changed during installation"));
  rollback.cancel();
  return {};
}

std::expected<bool, StarException> LuaBaseComponent::removeCallbacks(String const& groupName) {
  auto group = m_callbacks.find_ref(groupName);
  if (!group)
    return false;
  if (m_pendingUninit) {
    // A host may unregister its services immediately after requesting uninit
    // from inside Lua. Keep them alive for the deferred shutdown hook.
    if (std::ranges::find(m_pendingCallbackRemovals, groupName) == m_pendingCallbackRemovals.end())
      m_pendingCallbackRemovals.push_back(groupName);
    return true;
  }
  auto scope = group->get().scope;
  if (m_context) {
    if (auto result = m_context->remove(groupName); !result)
      return std::unexpected(std::move(result.error()));
  }
  scope.request_stop();
  auto current = m_callbacks.find_ref(groupName);
  if (current && current->get().scope.get_token() == scope.get_token())
    m_callbacks.remove(groupName);
  return true;
}

bool LuaBaseComponent::hasCallbacks(String const& groupName) const {
  return m_callbacks.contains(groupName);
}

std::expected<void, StarException> LuaBaseComponent::addThreadCallbacks(String groupName, LuaCallbacks callbacks) {
  if (m_threadCallbacks.contains(groupName))
    return std::unexpected(LuaComponentException::format("Duplicate thread callbacks named '{}' in LuaBaseComponent", groupName));
  List<std::shared_ptr<ScriptableThread>> changed;
  for (auto const& p : m_threads) {
    if (auto result = p.second->addCallbacks(groupName, callbacks); !result) {
      for (auto const& thread : changed) {
        if (auto rollback = thread->removeCallbacks(groupName); !rollback)
          result.error() = result.error().withRelated(rollback.error(), "Error rolling back thread callbacks");
      }
      return std::unexpected(std::move(result.error()));
    }
    changed.push_back(p.second);
  }
  m_threadCallbacks.insert(std::move(groupName), std::move(callbacks));
  return {};
}

std::expected<bool, StarException> LuaBaseComponent::removeThreadCallbacks(String const& groupName) {
  auto callbacks = m_threadCallbacks.find_ref(groupName);
  if (!callbacks)
    return false;
  List<std::shared_ptr<ScriptableThread>> changed;
  for (auto const& p : m_threads) {
    if (auto removed = p.second->removeCallbacks(groupName); !removed) {
      for (auto const& thread : changed) {
        if (auto restored = thread->addCallbacks(groupName, callbacks->get()); !restored)
          removed.error() = restored.error().withRelated(removed.error(), "Original thread callback removal failure");
      }
      return std::unexpected(std::move(removed.error()));
    }
    changed.push_back(p.second);
  }
  m_threadCallbacks.remove(groupName);
  return true;
}

bool LuaBaseComponent::autoReInit() const { return static_cast<bool>(m_reloadTracker); }

void LuaBaseComponent::setAutoReInit(bool enabled) {
  m_reloadSubscription.disconnect();
  if (enabled) {
    m_reloadTracker = std::make_shared<TrackerListener>();
    m_reloadSubscription = m_root.registerReloadListener(m_reloadTracker);
  } else {
    m_reloadTracker.reset();
  }
}

std::expected<void, StarException> LuaBaseComponent::setLuaRoot(std::shared_ptr<LuaRoot> luaRoot) {
  if (luaRoot && &luaRoot->root() != &m_root)
    return std::unexpected(LuaComponentException("Lua component and LuaRoot must belong to the same Root"));
  if (m_luaRoot != luaRoot)
    m_rootGeneration = 0;
  m_luaRoot = std::move(luaRoot);
  return {};
}

std::shared_ptr<LuaRoot> const& LuaBaseComponent::luaRoot() { return m_luaRoot; }

std::expected<bool, StarException> LuaBaseComponent::init() {
  if (m_activeCalls != 0) {
    m_initPending = true;
    m_pendingUninit.reset();
    m_pendingWorldUninit.reset();
    return initialized();
  }
  auto result = execute<bool>(
    "init", "Exception while initializing Lua session", ErrorPolicy::DisableContext,
    [&]() -> std::expected<std::optional<bool>, StarException> {
      m_initPending = false;
      if (m_reloadTracker)
        (void)m_reloadTracker->pullTriggered();
      m_rootGeneration = m_luaRoot ? m_luaRoot->generation() : 0;
      if (auto stopped = shutdownContext({}); !stopped)
        return std::unexpected(std::move(stopped.error()));
      if (!m_luaRoot)
        return std::optional<bool>(false);
      m_callbackScope = std::stop_source{};
      auto context = m_luaRoot->createContext(m_scripts, m_callbackScope.get_token());
      if (!context)
        return std::unexpected(context.error().withContext(strf("Creating Lua context for scripts '{}'", m_scripts)));
      m_context = std::move(*context);
      LuaEnginePtr engine(&m_context->engine());
      auto previousLifetime = engine->setPromiseLifetime(m_callbackScope.get_token());
      auto restoreLifetime = finally([&] { engine->setPromiseLifetime(previousLifetime); });
      if (auto setup = contextSetup(); !setup)
        return std::unexpected(std::move(setup.error()));
      if (auto invoked = invoke<LuaValue>("init"); !invoked)
        return std::unexpected(std::move(invoked.error()));
      return std::optional<bool>(true);
    },
    Entry::Lifecycle);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return result->value_or(false) && initialized();
}

std::expected<void, StarException> LuaBaseComponent::uninit(String const& beforeUninit) {
  if (m_activeCalls != 0) {
    if (!m_shuttingDown) {
      m_initPending = false;
      m_pendingUninit = beforeUninit;
    }
    return {};
  }
  auto result = execute<bool>(
    "uninit", "Exception while uninitializing Lua session", ErrorPolicy::KeepContext,
    [&]() -> std::expected<std::optional<bool>, StarException> {
      m_initPending = false;
      if (m_reloadTracker)
        (void)m_reloadTracker->pullTriggered();
      m_rootGeneration = m_luaRoot ? m_luaRoot->generation() : 0;
      if (auto stopped = shutdownContext(beforeUninit); !stopped)
        return std::unexpected(std::move(stopped.error()));
      return std::optional<bool>(true);
    },
    Entry::Lifecycle);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> LuaBaseComponent::shutdownContext(String const& beforeUninit) {
  m_shuttingDown = true;
  auto finished = finally([this] { m_shuttingDown = false; });
  std::optional<StarException> error;
  auto remember = [&](std::expected<void, StarException> result) {
    if (!result) {
      if (!error)
        error = std::move(result.error());
      else
        error = error->withRelated(result.error(), "Additional Lua shutdown failure");
    }
  };
  auto invokeHook = [&](String const& name) {
    auto result = execute<LuaValue>(
      name, "Exception while calling Lua shutdown hook", ErrorPolicy::KeepContext,
      [&] { return m_context->invokePathIfPresent<LuaValue>(name); }, Entry::Lifecycle);
    if (!result)
      remember(std::unexpected(std::move(result.error())));
  };
  if (m_context) {
    if (!beforeUninit.empty())
      invokeHook(beforeUninit);
    if (!m_shutdownPending)
      invokeHook("uninit");
  }
  remember(retireContext());
  m_error.reset();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<bool, StarException> LuaBaseComponent::hasMethod(String const& name) {
  auto result = execute<bool>(name, "Exception while finding Lua method", ErrorPolicy::KeepContext,
                              [&]() -> std::expected<std::optional<bool>, StarException> {
                                auto value = m_context->getPath(name);
                                if (!value)
                                  return std::unexpected(std::move(value.error()));
                                return std::optional<bool>(std::holds_alternative<LuaFunction>(*value));
                              });
  if (!result)
    return std::unexpected(std::move(result.error()));
  return result->value_or(false);
}

bool LuaBaseComponent::initialized() const { return m_context.has_value(); }
std::optional<String> const& LuaBaseComponent::error() const { return m_error; }
std::optional<LuaContext> const& LuaBaseComponent::context() const { return m_context; }
std::optional<LuaContext>& LuaBaseComponent::context() { return m_context; }

std::expected<void, StarException> LuaBaseComponent::contextSetup() {
  auto self = m_context->createTable();
  if (!self)
    return std::unexpected(std::move(self.error()));
  if (auto result = m_context->setPath("self", std::move(*self)); !result)
    return std::unexpected(std::move(result.error()));
  // Install explicit groups before adding missing builtins, so each callback
  // table is constructed once and inherited groups cannot mask fresh targets.
  for (auto&& p : m_callbacks) {
    p.second.scope = std::stop_source{};
    if (auto result = m_context->remove(p.first); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_context->setCallbacks(p.first, p.second.callbacks.withLifetime(p.second.scope.get_token()));
        !result)
      return std::unexpected(std::move(result.error()));
  }
  if (!hasCallbacks("sb")) {
    auto callbacks = LuaBindings::makeUtilityCallbacks(m_root.random(), m_root.logger(), m_root.logMap());
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = addCallbacks("sb", std::move(*callbacks)); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (!hasCallbacks("root")) {
    auto callbacks = LuaBindings::makeRootCallbacks(m_root);
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = addCallbacks("root", std::move(*callbacks)); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (!hasCallbacks("threads")) {
    auto callbacks = makeThreadsCallbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = addCallbacks("threads", std::move(*callbacks)); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> LuaBaseComponent::contextShutdown() { return {}; }

std::expected<void, StarException> LuaBaseComponent::retireContext() {
  std::expected<void, StarException> shutdown;
  if (m_context)
    shutdown = contextShutdown();
  for (auto&& group : m_callbacks)
    group.second.scope.request_stop();
  (void)m_callbackScope.request_stop();
  m_context.reset();
  m_shutdownPending = false;
  auto stopped = stopThreads();
  m_threads.clear();
  if (!shutdown) {
    if (!stopped)
      shutdown.error() =
        shutdown.error().withRelated(stopped.error(), "Error stopping Lua threads");
    return shutdown;
  }
  return stopped;
}

std::expected<void, StarException> LuaBaseComponent::finishExecution() {
  // Teardown callbacks may reenter, but cannot recursively finish the boundary.
  ++m_activeCalls;
  auto finished = finally([this] { --m_activeCalls; });
  std::expected<void, StarException> cleanup;
  if (m_pendingUninit) {
    auto hook = std::move(*m_pendingUninit);
    m_pendingUninit.reset();
    cleanup = shutdownContext(hook);
  } else if (m_shutdownPending) {
    m_shuttingDown = true;
    cleanup = retireContext();
    m_shuttingDown = false;
  }
  if (m_pendingWorldUninit) {
    auto& world = m_pendingWorldUninit->get();
    m_pendingWorldUninit.reset();
    auto detached = world.finishShutdown();
    if (!detached)
      cleanup = cleanup ? std::move(detached)
                        : std::unexpected(detached.error().withRelated(cleanup.error(), "Earlier Lua shutdown failure"));
  }
  for (auto const& group : m_pendingCallbackRemovals) {
    auto removed = removeCallbacks(group);
    if (!removed && cleanup)
      cleanup = std::unexpected(std::move(removed.error()));
  }
  m_pendingCallbackRemovals.clear();
  if (!cleanup && !m_boundaryError)
    m_boundaryError = cleanup.error();
  auto error = std::move(m_boundaryError);
  m_boundaryError.reset();
  if (error) {
    if (auto logged = m_root.logger().errorException("Lua session failure", ExceptionDiagnostic(*error)); !logged) {
      auto failure =
        logged.error().withRelated(*error, "Original Lua session failure");
      if (!cleanup)
        failure = failure.withRelated(cleanup.error(), "Lua session cleanup failure");
      return std::unexpected(std::move(failure));
    }
  }
  return cleanup;
}

std::expected<bool, StarException> LuaBaseComponent::checkInitialization() {
  if (m_activeCalls != 0)
    return initialized() && !m_shutdownPending;
  bool reloaded = m_reloadTracker && m_reloadTracker->pullTriggered();
  bool replaced = m_luaRoot && m_rootGeneration != m_luaRoot->generation();
  if (m_initPending || ((initialized() || error()) && (reloaded || replaced)))
    return init();
  return initialized();
}

std::expected<void, StarException> LuaBaseComponent::cleanThreads() {
  StringList expired;
  for (auto const& p : m_threads) {
    if (p.second->shouldExpire())
      expired.push_back(p.first);
  }
  std::optional<StarException> error;
  for (auto const& name : expired) {
    // Recyclable is published by the worker only after teardown; still join
    // before releasing the parent's last owning reference.
    auto thread = m_threads.try_extract_value(name);
    if (auto stopped = (*thread)->stop(); !stopped) {
      if (!error)
        error = std::move(stopped.error());
      else
        error = error->withRelated(stopped.error(), "Additional Lua thread reclamation failure");
    }
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> LuaBaseComponent::stopThreads() {
  std::optional<StarException> error;
  for (auto const& p : m_threads) {
    if (auto stopped = p.second->stop(); !stopped) {
      if (!error)
        error = std::move(stopped.error());
      else
        error = stopped.error().withRelated(*error, "Earlier Lua thread shutdown failure");
    }
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<LuaCallbacks, StarException> LuaBaseComponent::makeThreadsCallbacks() {
  LuaCallbacks callbacks;
  if (auto result =
        callbacks.registerCallback("create",
                                   [&self = *this](Json parameters) -> std::expected<String, StarException> {
                                     if (auto cleaned = self.cleanThreads(); !cleaned)
                                       return std::unexpected(std::move(cleaned.error()));
                                     auto name = parameters.getString("name", Uuid(self.m_root.random()).hex());
                                     if (!name)
                                       return std::unexpected(std::move(name.error()));
                                     if (auto old = self.m_threads.find_ref(*name)) {
                                       if (auto stopped = old->get()->stop(); !stopped)
                                         return std::unexpected(std::move(stopped.error()));
                                       self.m_threads.remove(*name);
                                     }
                                     auto named = parameters.set("name", *name);
                                     if (!named)
                                       return std::unexpected(std::move(named.error()));
                                     auto thread = ScriptableThread::create(std::move(*named), self);
                                     if (!thread)
                                       return std::unexpected(std::move(thread.error()));
                                     for (auto const& p : self.m_threadCallbacks) {
                                       if (auto added = (*thread)->addCallbacks(p.first, p.second); !added)
                                         return std::unexpected(std::move(added.error()));
                                     }
                                     (*thread)->setPause(false);
                                     if (auto started = (*thread)->start(); !started)
                                       return std::unexpected(std::move(started.error()));
                                     self.m_threads.set(*name, std::move(*thread));
                                     return std::move(*name);
                                   });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setPause",
                                               [&self = *this](String const& name, bool paused) {
                                                 if (auto thread = self.m_threads.find_ref(name))
                                                   thread->get()->setPause(paused);
                                               });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result =
        callbacks.registerCallback("stop",
                                   [&self = *this](String const& name) -> std::expected<void, StarException> {
                                     if (auto thread = self.m_threads.find_ref(name)) {
                                       if (auto stopped = thread->get()->stop(); !stopped)
                                         return std::unexpected(std::move(stopped.error()));
                                       self.m_threads.remove(name);
                                     }
                                     if (auto cleaned = self.cleanThreads(); !cleaned)
                                       return std::unexpected(std::move(cleaned.error()));
                                     return {};
                                   });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback(
        "sendMessage",
        [&self = *this](String const& name, String const& message,
                        LuaVariadic<Json> args) -> std::expected<RpcPromise<Json>, StarException> {
          auto thread = self.m_threads.find_ref(name);
          if (!thread)
            return RpcPromise<Json>::createFailed("Thread does not exist");
          if (thread->get()->shouldExpire())
            return RpcPromise<Json>::createFailed("Thread is stopped");
          auto pair = RpcPromise<Json>::createPair();
          if (auto passed = thread->get()->passMessage(
                {.message = message, .args = JsonArray::from(std::move(args)), .promise = pair.second});
              !passed)
            return std::unexpected(std::move(passed.error()));
          return pair.first;
        });
      !result)
    return std::unexpected(std::move(result.error()));
  return callbacks;
}

RpcPromise<Json> LuaBaseComponent::threadPassMessage(String const&, String const&, JsonArray const&) {
  return RpcPromise<Json>::createFailed("Lua component cannot handle messages");
}


[[nodiscard]] std::expected<JsonObject, StarException> LuaScriptStorage::getScriptStorage() const {
  if (m_host.initialized())
    return m_host.context()->template getPath<JsonObject>("storage");
  return m_storage;
}

[[nodiscard]] std::expected<void, StarException> LuaScriptStorage::setScriptStorage(JsonObject storage) {
  if (m_host.initialized())
    return m_host.context()->setPath("storage", std::move(storage));
  m_storage = std::move(storage);
  return {};
}

[[nodiscard]] std::expected<void, StarException> LuaScriptStorage::contextSetup() {
  return m_host.context()->setPath("storage", m_storage);
}

std::expected<JsonObject, StarException> LuaScriptStorage::readStorage() const {
  return m_host.context()->getPath<JsonObject>("storage");
}

std::expected<void, StarException> LuaScriptStorage::finishShutdown(std::expected<JsonObject, StarException> storage, std::expected<void, StarException> shutdown) {
  if (storage)
    m_storage = std::move(*storage);
  else {
    if (!shutdown)
      return std::unexpected(storage.error().withRelated(shutdown.error(), "Error shutting down Lua storage context"));
    return std::unexpected(std::move(storage.error()));
  }
  return shutdown;
}

LuaScriptUpdate::LuaScriptUpdate(LuaBaseComponent& host) : m_host(host), m_lastDt(host.root().timing().timestep * host.root().timing().timescale) {
  m_updatePeriodic.setStepCount(1);
}

void LuaScriptUpdate::destroy() {
  // Child threads must stop before the parent-message queue is destroyed.
  if (auto stopped = m_host.stopThreads(); !stopped)
    EmergencyLog::error("Error stopping Lua component threads: {}", outputException(stopped.error(), true));
  List<ThreadMessage> messages;
  {
    std::unique_lock locker(m_threadMessageMutex, std::defer_lock);
    lockWithDiagnostics(locker);
    messages = std::move(m_threadMessages);
  }
  for (auto& message : messages) {
    if (auto failed = message.promise.cancel("Lua component destroyed"); !failed)
      EmergencyLog::error("Error settling destroyed Lua component message: {}", outputException(failed.error(), true));
  }
}

[[nodiscard]] std::expected<void, StarException> LuaScriptUpdate::contextSetup() {
  if (!m_host.hasCallbacks("script")) {
    LuaCallbacks callbacks;
    if (auto result = callbacks.registerCallback("updateDt", [&self = *this]() { return self.updateDt(); }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallback("setUpdateDelta", [&self = *this](unsigned delta) { self.setUpdateDelta(delta); }); !result)
      return std::unexpected(std::move(result.error()));
    return m_host.addCallbacks("script", std::move(callbacks));
  }
  return {};
}

unsigned LuaScriptUpdate::updateDelta() const { return m_updatePeriodic.stepCount(); }

float LuaScriptUpdate::updateDt(float dt) const {
  m_lastDt = dt;
  return m_updatePeriodic.stepCount() * dt;
}

float LuaScriptUpdate::updateDt() const { return m_updatePeriodic.stepCount() * m_lastDt; }

void LuaScriptUpdate::setUpdateDelta(unsigned delta) { m_updatePeriodic.setStepCount(delta); }

bool LuaScriptUpdate::updateReady() const { return m_updatePeriodic.ready(); }

RpcPromise<Json> LuaScriptUpdate::threadPassMessage(String const& thread, String const& message, JsonArray const& args) {
  auto pair = RpcPromise<Json>::createPair();
  std::unique_lock locker(m_threadMessageMutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_threadMessages.push_back({.originThread = thread, .message = message, .args = args, .promise = pair.second});
  return pair.first;
}

[[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> LuaScriptUpdate::handleThreadMessage(ThreadMessage const& message) {
  return m_host.execute<ChainableJsonMessageResponse>(
    message.message, "Exception while invoking Lua thread message handler",
    LuaBaseComponent::ErrorPolicy::DisableContext,
    [&]() -> std::expected<std::optional<ChainableJsonMessageResponse>, StarException> {
      auto handler = m_threadMessageHandlers.find_ref(message.message);
      if (!handler || !handler->get().function)
        return std::optional<ChainableJsonMessageResponse>{};
      auto function = *handler->get().function;
      auto result =
        function.invoke<ChainableJsonMessageResponse>(message.originThread, message.message, luaUnpack(message.args));
      if (!result)
        return std::unexpected(std::move(result.error()));
      return std::optional<ChainableJsonMessageResponse>(std::move(*result));
    });
}

[[nodiscard]] std::expected<void, StarException> LuaScriptUpdate::addThreadsCallbacks(LuaCallbacks& callbacks) {
  if (auto result = callbacks.registerCallback("setMessageHandler", [&self = *this](String message, std::optional<LuaFunction> handler) {
    if (handler) {
      ThreadMessageHandler info;
      info.name = message;
      info.function = std::move(handler);
      self.m_threadMessageHandlers.set(message, std::move(info));
    } else {
      self.m_threadMessageHandlers.remove(message);
    }
  }); !result)
    return std::unexpected(std::move(result.error()));
  return {};
}

[[nodiscard]] std::expected<void, StarException> LuaScriptUpdate::contextShutdown() {
  // Stop producers before draining, so child uninit cannot enqueue a late,
  // permanently pending parent message.
  auto stopped = m_host.stopThreads();
  List<ThreadMessage> messages;
  {
    std::unique_lock locker(m_threadMessageMutex, std::defer_lock);
    lockWithDiagnostics(locker);
    messages = std::move(m_threadMessages);
  }
  std::optional<StarException> error;
  if (!stopped)
    error = std::move(stopped.error());
  for (auto& message : messages) {
    if (auto result = message.promise.cancel("Lua component uninitialized"); !result) {
      if (!error)
        error = std::move(result.error());
      else
        error = result.error().withRelated(*error, "Earlier Lua shutdown failure");
    }
  }
  m_threadMessageHandlers.clear();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> LuaScriptUpdate::finishShutdown(std::expected<void, StarException> stopped, std::expected<void, StarException> shutdown) {
  if (!stopped) {
    if (!shutdown)
      return std::unexpected(stopped.error().withRelated(shutdown.error(), "Error shutting down Lua component"));
    return std::unexpected(std::move(stopped.error()));
  }
  return shutdown;
}

[[nodiscard]] std::expected<void, StarException> LuaScriptWorld::init(World& world) {
  if (m_host.initialized()) {
    if (auto result = uninit(); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (auto result = m_host.setLuaRoot(world.luaRoot()); !result)
    return std::unexpected(std::move(result.error()));
  auto worldCallbacks = LuaBindings::makeWorldCallbacks(m_host.root(), world);
  if (!worldCallbacks)
    return std::unexpected(std::move(worldCallbacks.error()));
  if (auto result = m_host.addCallbacks("world", std::move(*worldCallbacks)); !result)
    return std::unexpected(std::move(result.error()));
  auto result = [&]() -> std::expected<void, StarException> {
    auto callbacks = LuaBindings::makeWorldThreadCallbacks(m_host.root(), world);
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto added = m_host.addThreadCallbacks("world", std::move(*callbacks)); !added)
      return std::unexpected(std::move(added.error()));
    auto threadCallbacks = world.luaThreadCallbacks();
    if (!threadCallbacks)
      return std::unexpected(std::move(threadCallbacks.error()));
    for (auto const& p : *threadCallbacks) {
      if (auto added = m_host.addThreadCallbacks(p.first, p.second); !added)
        return std::unexpected(std::move(added.error()));
      m_worldThreadCallbacks.add(p.first);
    }
    auto initialized = m_host.init();
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    return {};
  }();
  if (!result) {
    if (auto rollback = uninit(); !rollback)
      return std::unexpected(result.error().withRelated(rollback.error(), "Error rolling back Lua world component"));
  }
  return result;
}

[[nodiscard]] std::expected<void, StarException> LuaScriptWorld::uninit() {
  m_host.m_pendingWorldUninit = std::ref(*this);
  return m_host.uninit({});
}

[[nodiscard]] std::expected<void, StarException> LuaScriptWorld::finishShutdown() {
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (!error)
        error = std::move(result.error());
      else
        error = result.error().withRelated(*error, "Earlier Lua world shutdown failure");
    }
  };
  remember(m_host.removeCallbacks("world"));
  remember(m_host.removeThreadCallbacks("world"));
  for (auto const& name : m_worldThreadCallbacks)
    remember(m_host.removeThreadCallbacks(name));
  m_worldThreadCallbacks.clear();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

[[nodiscard]] std::expected<void, StarException> LuaScriptMessages::contextSetup() {
  if (m_host.hasCallbacks("message"))
    return {};
  LuaCallbacks callbacks;
  if (auto result = callbacks.registerCallback("setHandler", [&self = *this](std::variant<String, Json> message, std::optional<LuaFunction> handler) -> std::expected<void, StarException> {
    MessageHandler info;
    if (std::holds_alternative<Json>(message)) {
      auto const& config = std::get<Json>(message);
      auto passName = config.getBool("passName", false);
      if (!passName)
        return std::unexpected(std::move(passName.error()));
      auto localOnly = config.getBool("localOnly", false);
      if (!localOnly)
        return std::unexpected(std::move(localOnly.error()));
      auto name = config.getString("name");
      if (!name)
        return std::unexpected(std::move(name.error()));
      info.passName = *passName;
      info.localOnly = *localOnly;
      info.name = std::move(*name);
    } else {
      info.name = std::move(std::get<String>(message));
    }
    if (handler) {
      info.function = std::move(handler);
      auto name = info.name;
      self.m_messageHandlers.set(name, std::move(info));
    } else {
      self.m_messageHandlers.remove(info.name);
    }
    return {};
  }); !result)
    return std::unexpected(std::move(result.error()));
  return m_host.addCallbacks("message", std::move(callbacks));
}

[[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> LuaScriptMessages::handleMessage(String const& message, bool local, JsonArray const& args) {
  return m_host.execute<ChainableJsonMessageResponse>(
    message, "Exception while invoking Lua message handler", LuaBaseComponent::ErrorPolicy::DisableContext,
    [&]() -> std::expected<std::optional<ChainableJsonMessageResponse>, StarException> {
      auto handler = m_messageHandlers.find_ref(message);
      if (!handler || !handler->get().function || (handler->get().localOnly && !local))
        return std::optional<ChainableJsonMessageResponse>{};
      // Lua can replace/remove this handler or request a reload while it runs.
      auto function = *handler->get().function;
      bool localOnly = handler->get().localOnly;
      bool passName = handler->get().passName;
      auto result = [&]() -> std::expected<ChainableJsonMessageResponse, StarException> {
        if (localOnly) {
          if (passName)
            return function.invoke<ChainableJsonMessageResponse>(message, luaUnpack(args));
          return function.invoke<ChainableJsonMessageResponse>(luaUnpack(args));
        }
        if (passName)
          return function.invoke<ChainableJsonMessageResponse>(message, local, luaUnpack(args));
        return function.invoke<ChainableJsonMessageResponse>(local, luaUnpack(args));
      }();
      if (!result)
        return std::unexpected(std::move(result.error()));
      return std::optional<ChainableJsonMessageResponse>(std::move(*result));
    });
}

void LuaScriptMessages::contextShutdown() {
  m_messageHandlers.clear();
}

}
