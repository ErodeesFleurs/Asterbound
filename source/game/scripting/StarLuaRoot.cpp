import star.json_extra;
import star.algorithm;
import star.byte_array;
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
import star.time;
import star.string;
import star.set;
import star.file;
import star.lua;
import star.format;
import star.casting;

import star.lua_root;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

namespace Star {

std::expected<std::reference_wrapper<Root>, StarException> lua_execution_root(LuaEngine& engine) {
  if (auto host = engine.execution_host()) {
    if (auto scope = as_ref<LuaExecutionScope>(host->get()))
      return std::ref(scope->get().root());
  }
  return std::unexpected(LuaException("Game Lua conversion requires an explicit game execution scope"));
}

LuaRoot::LuaRoot(Root& root)
  : m_root(root), m_scriptCache(std::make_shared<ScriptCache>(root)),
    m_storageDirectory(root.toStoragePath("lua")) {}

std::expected<std::shared_ptr<LuaRoot>, StarException> LuaRoot::create(Root& root) {
  auto luaRoot = std::shared_ptr<LuaRoot>(new LuaRoot(root));
  if (auto result = luaRoot->restart(); !result)
    return std::unexpected(std::move(result.error()));
  luaRoot->m_root_reload_listener = std::make_shared<CallbackListener>([cache = luaRoot->m_scriptCache]() -> std::expected<void, StarException> {
    cache->clear();
    return {};
  });
  luaRoot->m_root_reload_subscription = root.subscribe_reload(luaRoot->m_root_reload_listener);
  return luaRoot;
}

LuaRoot::~LuaRoot() {
  if (auto result = shutdown(); !result) {
    if (auto logged =
          m_root.logger().error_exception("Error while shutting down Lua root", ExceptionDiagnostic(result.error()));
        !logged)
      EmergencyLog::error("Could not log Lua root shutdown failure: {}; original failure: {}", Star::output_exception(logged.error(), true), Star::output_exception(result.error(), true));
  }
}

Root& LuaRoot::root() const {
  return m_root;
}

bool LuaRoot::script_loaded(String const& assetPath) const {
  return m_scriptCache->script_loaded(assetPath);
}

std::expected<void, StarException> LuaRoot::restart() {
  if (auto result = shutdown(); !result)
    return std::unexpected(std::move(result.error()));

  auto configuration = m_root.configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto safeScripts = (*configuration)->get("safeScripts");
  if (!safeScripts)
    return std::unexpected(std::move(safeScripts.error()));
  auto safe = safeScripts->to_bool();
  if (!safe)
    return std::unexpected(std::move(safe.error()));
  auto engine = LuaEngine::create(*safe);
  if (!engine)
    return std::unexpected(std::move(engine.error()));
  (*engine)->set_execution_host(std::make_unique<LuaExecutionScope>(m_root));
  auto sourceDiagnostics = std::make_shared<StringMap<String>>();
  (*engine)->set_source_resolver([sourceDiagnostics](String const& source) -> std::expected<String, StarException> {
    if (auto diagnostic = sourceDiagnostics->find_ref(source))
      return diagnostic->get();
    return String();
  });
  (*engine)->set_image_resolver([&root = m_root](String const& path) -> std::expected<std::shared_ptr<Image const>, StarException> {
    auto assets = root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    return (*assets)->image(path);
  });

  auto recursion = (*configuration)->get("scriptRecursionLimit");
  if (!recursion)
    return std::unexpected(std::move(recursion.error()));
  auto recursionLimit = recursion->to_uint();
  if (!recursionLimit)
    return std::unexpected(std::move(recursionLimit.error()));
  auto instruction = (*configuration)->get("scriptInstructionLimit");
  if (!instruction)
    return std::unexpected(std::move(instruction.error()));
  auto instructionLimit = instruction->to_uint();
  if (!instructionLimit)
    return std::unexpected(std::move(instructionLimit.error()));
  auto profiling = (*configuration)->get("scriptProfilingEnabled");
  if (!profiling)
    return std::unexpected(std::move(profiling.error()));
  auto profilingEnabled = profiling->to_bool();
  if (!profilingEnabled)
    return std::unexpected(std::move(profilingEnabled.error()));
  auto interval = (*configuration)->get("scriptInstructionMeasureInterval");
  if (!interval)
    return std::unexpected(std::move(interval.error()));
  auto measureInterval = interval->to_uint();
  if (!measureInterval)
    return std::unexpected(std::move(measureInterval.error()));
  (*engine)->set_recursion_limit(*recursionLimit);
  (*engine)->set_instruction_limit(*instructionLimit);
  (*engine)->set_profiling_enabled(*profilingEnabled);
  (*engine)->set_instruction_measure_interval(*measureInterval);
  m_luaEngine = std::move(*engine);
  m_sourceDiagnostics = std::move(sourceDiagnostics);
  if (m_generation != 0)
    m_root.logger().reset_exception_repeats();
  ++m_generation;
  return {};
}

std::expected<void, StarException> LuaRoot::shutdown() {
  m_scriptCache->clear();
  if (!m_luaEngine)
    return {};

  // Detach even when writing the profile fails; no half-shutdown engine remains.
  auto engine = std::move(m_luaEngine);
  auto profile = engine->get_profile();
  if (profile.empty())
    return {};
  profile.sort([](auto const& a, auto const& b) { return a.totalTime > b.totalTime; });
  std::function<Json(LuaProfileEntry const&)> jsonFromProfileEntry = [&](LuaProfileEntry const& entry) -> Json {
    JsonObject value;
    value.set("function", entry.name.value_or("<function>"));
    value.set("scope", entry.nameScope.value_or("?"));
    value.set("source", Star::format("{}:{}", entry.source, entry.sourceLine));
    value.set("self", entry.selfTime);
    value.set("total", entry.totalTime);
    // The detached engine and profile retain the entire call tree during export.
    std::vector<std::reference_wrapper<LuaProfileEntry const>> calls;
    calls.reserve(entry.calls.size());
    for (auto const& p : entry.calls)
      calls.push_back(std::cref(*p.second));
    std::sort(calls.begin(), calls.end(), [](auto a, auto b) { return a.get().totalTime > b.get().totalTime; });
    JsonArray storedCalls;
    storedCalls.reserve(calls.size());
    std::ranges::transform(calls, std::back_inserter(storedCalls),
        [&](auto call) { return jsonFromProfileEntry(call.get()); });
    value.set("calls", std::move(storedCalls));
    return value;
  };
  auto summary = Json(profile.transformed(jsonFromProfileEntry)).repr(1);
  if (!summary)
    return std::unexpected(std::move(summary.error()));
  auto directory = File::is_directory(m_storageDirectory);
  if (!directory)
    return std::unexpected(std::move(directory.error()));
  if (!*directory) {
    if (auto logged = m_root.logger().info("Creating lua storage directory"); !logged)
      return std::unexpected(std::move(logged.error()));
    if (auto result = File::create_directory(m_storageDirectory); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto timestamp = Time::local_time(std::chrono::system_clock::now());
  if (!timestamp)
    return std::unexpected(std::move(timestamp.error()));
  auto wholeSeconds = std::chrono::floor<std::chrono::seconds>(*timestamp);
  auto millis = *timestamp - wholeSeconds;
  String filename = Star::format("{:%Y-%m-%d-%H-%M-%S}-{:03d}.luaprofile", wholeSeconds, millis.count());
  String path = File::relative_to(m_storageDirectory, filename);
  if (auto logged = m_root.logger().info("Writing lua profile {}", filename); !logged)
    return std::unexpected(std::move(logged.error()));
  return File::write_file(*summary, path);
}

std::expected<LuaContext, StarException> LuaRoot::create_context(String const& script, std::stop_token loadLifetime) {
  return create_context(StringList{script}, std::move(loadLifetime));
}

std::expected<LuaContext, StarException> LuaRoot::create_context(StringList const& scriptPaths,
                                                                std::stop_token loadLifetime) {
  if (!m_luaEngine)
    return std::unexpected(LuaException("Cannot create a context from a shut down Lua root"));
  LuaEnginePtr engine = m_luaEngine;
  auto previousLifetime = engine->set_promise_lifetime(loadLifetime);
  auto restored = make_scope_exit([&] { engine->set_promise_lifetime(std::move(previousLifetime)); });
  auto created = engine->create_context();
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto context = std::move(*created);
  auto cache = m_scriptCache;
  auto sourceDiagnostics = m_sourceDiagnostics;
  if (auto required = context.set_require_function(
        [cache, sourceDiagnostics, revision = cache->revision(), loaded = HashSet<String, hash<String>, std::equal_to<>>{},
         loading = std::vector<String>{}](LuaStackValue environment,
                                          StringView module) mutable -> std::expected<void, StarException> {
          if (loaded.contains(module))
            return {};
          String path(module.utf8());
          if (std::ranges::find(loading, path) != loading.end()) {
            String chain;
            for (auto const& entry : loading) {
              if (!chain.empty())
                chain += " -> ";
              chain += entry;
            }
            auto error = LuaException::format("Cyclic Lua require: {} -> {}", chain, path);
            auto summary = *error.diagnostic_summary();
            summary.kind = ExceptionSummaryKind::ScriptRuntime;
            return std::unexpected(std::move(error).with_summary(std::move(summary)));
          }
          auto script = cache->script(environment.stack().engine(), path, revision, *sourceDiagnostics);
          if (!script)
            return std::unexpected(std::move(script.error()));
          loading.push_back(path);
          auto popped = make_scope_exit([&] { loading.pop_back(); });
          if (auto result = environment.stack().load(environment, (*script)->span(), path); !result)
            return std::unexpected(std::move(result.error()));
          if (cache->revision() != revision)
            return std::unexpected(
              LuaException::format("Lua script cache changed while loading '{}'; recreate the context", path));
          (void)loaded.insert(std::move(path));
          return {};
        });
      !required)
    return std::unexpected(std::move(required.error()));
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  if (!scriptPaths.empty()) {
    // Retain the loader while entry scripts can replace their global require.
    auto require = context.get<LuaFunction>("require");
    if (!require)
      return std::unexpected(std::move(require.error()));
    for (auto const& path : scriptPaths) {
      if (AssetPath::is_remote(path))
        return std::unexpected(LuaException("Embedded assets cannot be loaded as scripts"));
      auto exists = (*assets)->asset_exists(path);
      if (!exists)
        return std::unexpected(std::move(exists.error()));
      if (*exists) {
        if (auto result = require->invoke<void>(path); !result)
          return std::unexpected(std::move(result.error()));
      } else {
        if (auto logged = m_root.logger().error("Script '{}' does not exist", path); !logged)
          return std::unexpected(std::move(logged.error()));
      }
    }
  }
  for (auto const& callbacks : m_luaCallbacks) {
    auto result = loadLifetime.stop_possible()
      ? context.set_callbacks(callbacks.first, callbacks.second.with_lifetime(loadLifetime))
      : context.set_callbacks(callbacks.first, callbacks.second);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return context;
}

std::expected<void, StarException> LuaRoot::collect_garbage(std::optional<unsigned> steps) {
  if (m_luaEngine)
    return m_luaEngine->collect_garbage(steps);
  return {};
}

void LuaRoot::set_auto_garbage_collection(bool enabled) {
  if (m_luaEngine)
    m_luaEngine->set_auto_garbage_collection(enabled);
}

void LuaRoot::tune_auto_garbage_collection(float pause, float multiplier) {
  if (m_luaEngine)
    m_luaEngine->tune_auto_garbage_collection(pause, multiplier);
}

std::size_t LuaRoot::lua_memory_usage() const {
  return m_luaEngine ? m_luaEngine->memory_usage() : 0;
}

std::size_t LuaRoot::script_cache_memory_usage() const {
  return m_luaEngine ? m_scriptCache->memory_usage() : 0;
}

void LuaRoot::add_callbacks(String const& groupName, LuaCallbacks const& callbacks) {
  m_luaCallbacks[groupName] = callbacks;
}

LuaEngine& LuaRoot::lua_engine() const {
  return *m_luaEngine;
}

std::expected<std::shared_ptr<ByteArray const>, StarException>
LuaRoot::ScriptCache::script(LuaEngine& engine, String const& path, std::uint64_t revision,
                            StringMap<String>& sourceDiagnostics) {
  auto stale = [&] {
    return LuaException::format("Lua script cache changed before loading '{}'; recreate the context", path);
  };
  {
    std::lock_guard locker(mutex);
    if (m_revision != revision)
      return std::unexpected(stale());
    if (auto cached = scripts.find_ref(path))
      return cached->get();
  }
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  // Capture the indexed provider before reading or compiling can fail.
  // Hot reload keeps the asset index stable; snapshots survive cache clearing
  // and are owned separately from Root by the engine's resolver.
  if (!sourceDiagnostics.contains(path)) {
    auto descriptor = (*assets)->asset_descriptor(path);
    if (descriptor && *descriptor) {
      auto sourcePath = (*assets)->asset_source_path((*descriptor)->source);
      auto pathRef = sourcePath
        ? std::optional<std::reference_wrapper<String const>>(std::cref(*sourcePath))
        : std::nullopt;
      sourceDiagnostics.set(path, String(Assets::source_diagnostic(*(*descriptor)->source, pathRef)));
    }
  }
  auto bytes = (*assets)->bytes(path);
  if (!bytes) {
    if (auto diagnostic = sourceDiagnostics.find_ref(path)) {
      auto summary = *bytes.error().diagnostic_summary();
      bool found = false;
      for (auto& location : summary.locations) {
        if (location.source == path.utf8()) {
          found = true;
          if (location.provider.empty())
            location.provider = diagnostic->get().utf8();
        }
      }
      if (!found)
        summary.locations.push_back({.source = path.utf8(), .line = -1, .provider = diagnostic->get().utf8()});
      return std::unexpected(std::move(bytes.error()).with_summary(std::move(summary)).with_context(
          Star::format("Lua source '{}': {}", path, diagnostic->get())));
    }
    return std::unexpected(std::move(bytes.error()));
  }
  auto compiled = engine.compile((*bytes)->span(), path);
  if (!compiled)
    return std::unexpected(std::move(compiled.error()));
  auto script = std::make_shared<ByteArray const>(std::move(*compiled));
  {
    std::lock_guard locker(mutex);
    if (m_revision != revision)
      return std::unexpected(stale());
    scripts.set(path, script);
  }
  return script;
}

bool LuaRoot::ScriptCache::script_loaded(String const& path) const {
  std::lock_guard locker(mutex);
  return scripts.contains(path);
}

void LuaRoot::ScriptCache::clear() {
  std::lock_guard locker(mutex);
  ++m_revision;
  scripts.clear();
}

std::uint64_t LuaRoot::ScriptCache::revision() const {
  std::lock_guard locker(mutex);
  return m_revision;
}

std::size_t LuaRoot::ScriptCache::memory_usage() const {
  std::lock_guard locker(mutex);
  std::size_t total = 0;
  for (auto const& p : scripts)
    total += p.second->size();
  return total;
}

}
