import std;
import star.json;
import star.json_extra;
import star.list;
import star.rpc_promise;
import star.string;
import star.thread;
import star.lua;
import star.logging;
import star.output_proxy;
import star.game_types;
import star.format;
import star.scriptable_thread;
import star.lua_root;
import star.lua_converters;
import star.lua_components;
import star.root;
import star.tick_rate_monitor;
import star.config_lua_bindings;

namespace Star {

ScriptableThread::ScriptableThread(Json parameters, String name, LuaBaseComponent& parent)
  : Thread("ScriptableThread: " + name), m_parameters(std::move(parameters)),
    m_name(std::move(name)), m_logMapped(true), m_timestep(1.0f / 60.0f),
    m_pause(false), m_errorOccurred(false), m_shouldExpire(false), m_parent(parent) {}

std::expected<std::shared_ptr<ScriptableThread>, StarException> ScriptableThread::create(Json parameters, LuaBaseComponent& parent) {
  auto name = parameters.getString("name");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto thread = std::shared_ptr<ScriptableThread>(new ScriptableThread(std::move(parameters), std::move(*name), parent));
  auto root = LuaRoot::create(parent.root());
  if (!root)
    return std::unexpected(std::move(root.error()));
  thread->m_luaRoot = std::move(*root);
  auto pause = thread->m_parameters.getFloat("luaGcPause", 1.2f);
  if (!pause)
    return std::unexpected(std::move(pause.error()));
  auto step = thread->m_parameters.getFloat("luaGcStepMultiplier", 1.2f);
  if (!step)
    return std::unexpected(std::move(step.error()));
  thread->m_luaRoot->tuneAutoGarbageCollection(*pause, *step);
  auto logMapped = thread->m_parameters.getBool("logMapped", true);
  if (!logMapped)
    return std::unexpected(std::move(logMapped.error()));
  thread->m_logMapped = *logMapped;
  auto tickRate = thread->m_parameters.getFloat("tickRate", 60.0f);
  if (!tickRate)
    return std::unexpected(std::move(tickRate.error()));
  thread->m_timestep = 1.0f / *tickRate;
  auto limit = thread->m_parameters.optUInt("instructionLimit");
  if (!limit)
    return std::unexpected(std::move(limit.error()));
  if (*limit)
    thread->m_luaRoot->luaEngine().setInstructionLimit(**limit);
  auto callbacks = thread->makeThreadCallbacks();
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  thread->m_luaRoot->addCallbacks("thread", *callbacks);
  auto configCallbacks = LuaBindings::makeConfigCallbacks([owner = std::ref(*thread)](String const& name, Json const& def) {
    return owner.get().configValue(name, def);
  });
  if (!configCallbacks)
    return std::unexpected(std::move(configCallbacks.error()));
  thread->m_luaRoot->addCallbacks("config", *configCallbacks);
  return thread;
}

ScriptableThread::~ScriptableThread() {
  if (auto result = stop(); !result)
    EmergencyLog::error("Error stopping ScriptableThread: {}", outputException(result.error(), true));
  m_scriptContexts.clear();
}

std::expected<void, StarException> ScriptableThread::addCallbacks(String const& group, LuaCallbacks const& callbacks) {
  if (m_threadCallbacks.contains(group))
    return {};
  List<std::shared_ptr<ScriptComponent>> changed;
  for (auto const& p : m_scriptContexts) {
    auto result = p.second->addCallbacks(group, callbacks);
    if (result) {
      result = p.second->addThreadCallbacks(group, callbacks);
      if (!result) {
        if (auto rollback = p.second->removeCallbacks(group); !rollback)
          EmergencyLog::error("Error rolling back script thread callbacks: {}", outputException(rollback.error(), true));
      }
    }
    if (!result) {
      for (auto const& component : changed) {
        if (auto rollback = component->removeCallbacks(group); !rollback)
          EmergencyLog::error("Error rolling back script thread callbacks: {}", outputException(rollback.error(), true));
        if (auto rollback = component->removeThreadCallbacks(group); !rollback)
          EmergencyLog::error("Error rolling back nested thread callbacks: {}", outputException(rollback.error(), true));
      }
      return std::unexpected(std::move(result.error()));
    }
    changed.append(p.second);
  }
  m_threadCallbacks.insert(group, callbacks);
  return {};
}

std::expected<void, StarException> ScriptableThread::removeCallbacks(String const& group) {
  auto callbacks = m_threadCallbacks.maybeRef(group);
  if (!callbacks)
    return {};
  struct RemovedCallbacks {
    std::shared_ptr<ScriptComponent> component;
    bool callbacks;
    bool threadCallbacks;
  };
  List<RemovedCallbacks> changed;
  auto rollback = [&](StarException error) -> std::expected<void, StarException> {
    for (auto const& removed : changed) {
      if (removed.callbacks) {
        if (auto restored = removed.component->addCallbacks(group, callbacks->get()); !restored)
          error = restored.error().withRelated(error, "Original script callback removal failure");
      }
      if (removed.threadCallbacks) {
        if (auto restored = removed.component->addThreadCallbacks(group, callbacks->get()); !restored)
          error = restored.error().withRelated(error, "Original script callback removal failure");
      }
    }
    return std::unexpected(std::move(error));
  };
  for (auto const& p : m_scriptContexts) {
    auto removed = p.second->removeCallbacks(group);
    if (!removed)
      return rollback(std::move(removed.error()));
    changed.append({p.second, *removed, false});
    auto removedThread = p.second->removeThreadCallbacks(group);
    if (!removedThread)
      return rollback(std::move(removedThread.error()));
    changed.back().threadCallbacks = *removedThread;
  }
  m_threadCallbacks.remove(group);
  return {};
}

std::expected<bool, StarException> ScriptableThread::start() {
  if (!isJoined())
    return false;
  {
    std::scoped_lock locks(m_messageMutex, m_sleepMutex);
    if (m_stopSource.stop_requested())
      m_stopSource = std::stop_source{};
    m_errorOccurred = false;
    m_shouldExpire = false;
  }
  auto started = Thread::start();
  if (!started) {
    requestStop();
    m_errorOccurred = true;
    if (auto stopped = stop(); !stopped)
      started.error() = started.error().withRelated(stopped.error(), "Error settling failed worker startup");
    m_shouldExpire.store(true, std::memory_order_release);
  }
  return started;
}

void ScriptableThread::requestStop() {
  {
    std::lock_guard lock(m_sleepMutex);
    m_stopSource.request_stop();
  }
  m_stopped.notify_one();
}

std::expected<void, StarException> ScriptableThread::stop() {
  requestStop();
  auto joined = Thread::join();
  List<Message> messages;
  {
    std::unique_lock locker(m_messageMutex, std::defer_lock);
    lockWithDiagnostics(locker);
    messages = std::move(m_messages);
  }
  std::optional<StarException> error;
  if (!joined)
    error = std::move(joined.error());
  for (auto& message : messages) {
    auto settled = m_errorOccurred ? message.promise.fail("Thread failed")
                                   : message.promise.cancel("Thread is stopped").transform([](bool) {});
    if (auto failed = std::move(settled); !failed) {
      if (!error)
        error = std::move(failed.error());
      else
        EmergencyLog::error("Error failing stopped thread message: {}", outputException(failed.error(), true));
    }
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

void ScriptableThread::setPause(bool pause) { m_pause = pause; }
bool ScriptableThread::errorOccurred() { return m_errorOccurred; }
bool ScriptableThread::shouldExpire() { return m_shouldExpire.load(std::memory_order_acquire); }

std::expected<void, StarException> ScriptableThread::passMessage(Message&& message) {
  std::unique_lock locker(m_messageMutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (m_errorOccurred)
    return message.promise.fail("Thread failed");
  if (m_stopSource.stop_requested() || m_shouldExpire)
    return message.promise.cancel("Thread is stopped").transform([](bool) {});
  m_messages.append(std::move(message));
  return {};
}

std::expected<void, StarException> ScriptableThread::run() {
  auto stopToken = m_stopSource.get_token();
  auto result = [&]() -> std::expected<void, StarException> {
    auto scripts = m_parameters.getObject("scripts");
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    for (auto const& p : *scripts) {
      auto component = std::make_shared<ScriptComponent>(m_luaRoot->root());
      if (auto set = component->setLuaRoot(m_luaRoot); !set)
        return std::unexpected(std::move(set.error()));
      auto array = p.second.toArray();
      if (!array)
        return std::unexpected(std::move(array.error()));
      auto paths = jsonToStringList(*array);
      if (!paths)
        return std::unexpected(std::move(paths.error()));
      if (auto set = component->setScripts(std::move(*paths)); !set)
        return std::unexpected(std::move(set.error()));
      for (auto const& callbacks : m_threadCallbacks) {
        if (auto added = component->addCallbacks(callbacks.first, callbacks.second); !added)
          return std::unexpected(std::move(added.error()));
        if (auto added = component->addThreadCallbacks(callbacks.first, callbacks.second); !added)
          return std::unexpected(std::move(added.error()));
      }
      m_scriptContexts.set(p.first, component);
      if (auto initialized = component->init(); !initialized) {
        // The component already logged and disabled a failed script. Preserve
        // the old per-context isolation; infrastructure failures stop the thread.
        if (!initialized.error().is<LuaException>())
          return std::unexpected(std::move(initialized.error()));
      }
    }
    auto window = m_parameters.getDouble("updateMeasureWindow", 0.5);
    if (!window)
      return std::unexpected(std::move(window.error()));
    TickRateApproacher approacher(1.0f / m_timestep, *window);
    while (!stopToken.stop_requested() && !m_errorOccurred) {
      if (m_logMapped)
        m_luaRoot->root().logMap().set(strf("lua_{}_update", m_name), strf("{:4.2f}Hz", approacher.rate()));
      if (auto updated = update(); !updated)
        return std::unexpected(std::move(updated.error()));
      approacher.setTargetTickRate(1.0f / m_timestep);
      approacher.tick();
      std::int64_t spare = std::floor(approacher.spareTime() * 1000);
      if (spare > 0) {
        std::unique_lock lock(m_sleepMutex);
        m_stopped.wait_for(lock, std::chrono::milliseconds(spare), [&] { return stopToken.stop_requested(); });
      }
    }
    return {};
  }();
  if (!result)
    m_errorOccurred = true;
  // Always tear down all contexts and settle messages, including failed startup.
  for (auto const& p : m_scriptContexts) {
    if (auto uninitialized = p.second->uninit(); !uninitialized) {
      if (result)
        result = std::unexpected(std::move(uninitialized.error()));
      else
        result.error() = result.error().withRelated(uninitialized.error(), "Error uninitializing ScriptableThread");
    }
  }
  m_scriptContexts.clear();
  List<Message> messages;
  {
    std::unique_lock locker(m_messageMutex, std::defer_lock);
    lockWithDiagnostics(locker);
    requestStop();
    messages = std::move(m_messages);
  }
  for (auto& message : messages) {
    auto settled = m_errorOccurred ? message.promise.fail("Thread failed")
                                   : message.promise.cancel("Thread is stopped").transform([](bool) {});
    if (auto failed = std::move(settled); !failed) {
      if (result)
        result = std::unexpected(std::move(failed.error()));
      else
        result.error() = result.error().withRelated(failed.error(), "Error settling ScriptableThread message");
    }
  }
  if (!result) {
    m_errorOccurred = true;
    if (auto logged = m_luaRoot->root().logger().errorException("ScriptableThread exception caught", ExceptionDiagnostic(result.error()));
        !logged)
      result.error() = result.error().withRelated(logged.error(), "Could not log ScriptableThread failure");
    else
      // This boundary handled the failure; Thread must not log it a second time.
      result = {};
  }
  // Release-publish only after all borrowed contexts and accepted messages
  // have been retired. The parent must still join before reclaiming us.
  m_shouldExpire.store(true, std::memory_order_release);
  return result;
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> ScriptableThread::receiveMessage(String const& message, JsonArray const& args) {
  for (auto const& p : m_scriptContexts) {
    auto result = p.second->handleMessage(message, true, args);
    if (!result) {
      if (result.error().is<LuaException>())
        continue; // The failed component logged/reset itself; try the next one.
      return std::unexpected(std::move(result.error()));
    }
    if (*result)
      return result;
  }
  return std::optional<ChainableJsonMessageResponse>{};
}

std::expected<void, StarException> ScriptableThread::update() {
  std::optional<StarException> error;
  if (m_timestep > 0.0f && !m_pause) {
    for (auto const& p : m_scriptContexts) {
      if (auto updated = p.second->update(p.second->updateDt(m_timestep)); !updated) {
        if (updated.error().is<LuaException>())
          continue; // Retain the existing logged per-context recovery boundary.
        if (!error)
          error = std::move(updated.error());
        else
          EmergencyLog::error("Error updating ScriptableThread context: {}", outputException(updated.error(), true));
      }
    }
  }
  List<Message> messages;
  {
    std::unique_lock locker(m_messageMutex, std::defer_lock);
    lockWithDiagnostics(locker);
    messages = std::move(m_messages);
  }
  for (auto& message : messages) {
    auto response = receiveMessage(message.message, message.args);
    std::expected<void, StarException> settled;
    if (!response) {
      settled = message.promise.fail(printException(response.error(), false));
      if (!error)
        error = std::move(response.error());
    } else if (!*response) {
      settled = message.promise.fail("Message not handled by thread");
    } else if (std::holds_alternative<RpcPromise<Json>>(**response)) {
      settled = message.promise.chain(std::get<RpcPromise<Json>>(**response));
    } else {
      settled = message.promise.fulfill(std::get<Json>(**response));
    }
    if (!settled) {
      if (!error)
        error = std::move(settled.error());
      else
        EmergencyLog::error("Error settling ScriptableThread message: {}", outputException(settled.error(), true));
    }
  }
  if (m_logMapped)
    m_luaRoot->root().logMap().set(strf("lua_{}_lua_mem", m_name), m_luaRoot->luaMemoryUsage());
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<LuaCallbacks, StarException> ScriptableThread::makeThreadCallbacks() {
  LuaCallbacks callbacks;
  if (auto result = callbacks.registerCallback("stop", [&self = *this]() {
    self.requestStop();
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("sendParentMessage", [&self = *this](String const& message, LuaVariadic<Json> args) {
    return self.m_parent.get().threadPassMessage(self.m_name, message, JsonArray::from(std::move(args)));
  }); !result)
    return std::unexpected(std::move(result.error()));
  return callbacks;
}

std::expected<Json, StarException> ScriptableThread::configValue(String const& name, Json def) const {
  return m_parameters.get(name, std::move(def));
}

}
