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
import star.version;
import star.casting;

import star.lua_root;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

namespace Star {

std::expected<std::reference_wrapper<Root>, StarException> luaExecutionRoot(LuaEngine& engine) {
  if (auto host = engine.executionHost()) {
    if (auto scope = asRef<LuaExecutionScope>(host->get()))
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
  luaRoot->m_rootReloadListener = std::make_shared<CallbackListener>([cache = luaRoot->m_scriptCache]() -> std::expected<void, StarException> {
    cache->clear();
    return {};
  });
  luaRoot->m_rootReloadSubscription = root.registerReloadListener(luaRoot->m_rootReloadListener);
  return luaRoot;
}

LuaRoot::~LuaRoot() {
  if (auto result = shutdown(); !result) {
    if (auto logged = m_root.logger().errorException("Error while shutting down Lua root", ExceptionDiagnostic(result.error())); !logged)
      EmergencyLog::error("Could not log Lua root shutdown failure: {}; original failure: {}", outputException(logged.error(), true), outputException(result.error(), true));
  }
}

Root& LuaRoot::root() const {
  return m_root;
}

bool LuaRoot::scriptLoaded(String const& assetPath) const {
  return m_scriptCache->scriptLoaded(assetPath);
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
  auto safe = safeScripts->toBool();
  if (!safe)
    return std::unexpected(std::move(safe.error()));
  auto engine = LuaEngine::create(*safe);
  if (!engine)
    return std::unexpected(std::move(engine.error()));
  (*engine)->setExecutionHost(std::make_unique<LuaExecutionScope>(m_root));
  auto sourceDiagnostics = std::make_shared<StringMap<String>>();
  (*engine)->setSourceResolver([sourceDiagnostics](String const& source) -> std::expected<String, StarException> {
    if (auto diagnostic = sourceDiagnostics->maybeRef(source))
      return diagnostic->get();
    return String();
  });
  (*engine)->setImageResolver([&root = m_root](String const& path) -> std::expected<std::shared_ptr<Image const>, StarException> {
    auto assets = root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    return (*assets)->image(path);
  });

  auto recursion = (*configuration)->get("scriptRecursionLimit");
  if (!recursion)
    return std::unexpected(std::move(recursion.error()));
  auto recursionLimit = recursion->toUInt();
  if (!recursionLimit)
    return std::unexpected(std::move(recursionLimit.error()));
  auto instruction = (*configuration)->get("scriptInstructionLimit");
  if (!instruction)
    return std::unexpected(std::move(instruction.error()));
  auto instructionLimit = instruction->toUInt();
  if (!instructionLimit)
    return std::unexpected(std::move(instructionLimit.error()));
  auto profiling = (*configuration)->get("scriptProfilingEnabled");
  if (!profiling)
    return std::unexpected(std::move(profiling.error()));
  auto profilingEnabled = profiling->toBool();
  if (!profilingEnabled)
    return std::unexpected(std::move(profilingEnabled.error()));
  auto interval = (*configuration)->get("scriptInstructionMeasureInterval");
  if (!interval)
    return std::unexpected(std::move(interval.error()));
  auto measureInterval = interval->toUInt();
  if (!measureInterval)
    return std::unexpected(std::move(measureInterval.error()));
  (*engine)->setRecursionLimit(*recursionLimit);
  (*engine)->setInstructionLimit(*instructionLimit);
  (*engine)->setProfilingEnabled(*profilingEnabled);
  (*engine)->setInstructionMeasureInterval(*measureInterval);
  m_luaEngine = std::move(*engine);
  m_sourceDiagnostics = std::move(sourceDiagnostics);
  ++m_generation;
  return {};
}

std::expected<void, StarException> LuaRoot::shutdown() {
  m_scriptCache->clear();
  if (!m_luaEngine)
    return {};

  // Detach even when writing the profile fails; no half-shutdown engine remains.
  auto engine = std::move(m_luaEngine);
  auto profile = engine->getProfile();
  if (profile.empty())
    return {};
  profile.sort([](auto const& a, auto const& b) { return a.totalTime > b.totalTime; });
  std::function<Json(LuaProfileEntry const&)> jsonFromProfileEntry = [&](LuaProfileEntry const& entry) -> Json {
    JsonObject value;
    value.set("function", entry.name.value_or("<function>"));
    value.set("scope", entry.nameScope.value_or("?"));
    value.set("source", strf("{}:{}", entry.source, entry.sourceLine));
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
  auto directory = File::isDirectory(m_storageDirectory);
  if (!directory)
    return std::unexpected(std::move(directory.error()));
  if (!*directory) {
    if (auto logged = m_root.logger().info("Creating lua storage directory"); !logged)
      return std::unexpected(std::move(logged.error()));
    if (auto result = File::makeDirectory(m_storageDirectory); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto timestamp = Time::localTime(std::chrono::system_clock::now());
  if (!timestamp)
    return std::unexpected(std::move(timestamp.error()));
  auto wholeSeconds = std::chrono::floor<std::chrono::seconds>(*timestamp);
  auto millis = *timestamp - wholeSeconds;
  String filename = strf("{:%Y-%m-%d-%H-%M-%S}-{:03d}.luaprofile", wholeSeconds, millis.count());
  String path = File::relativeTo(m_storageDirectory, filename);
  if (auto logged = m_root.logger().info("Writing lua profile {}", filename); !logged)
    return std::unexpected(std::move(logged.error()));
  return File::writeFile(*summary, path);
}

std::expected<LuaContext, StarException> LuaRoot::createContext(String const& script, std::stop_token loadLifetime) {
  return createContext(StringList{script}, std::move(loadLifetime));
}

std::expected<LuaContext, StarException> LuaRoot::createContext(StringList const& scriptPaths,
                                                                std::stop_token loadLifetime) {
  if (!m_luaEngine)
    return std::unexpected(LuaException("Cannot create a context from a shut down Lua root"));
  LuaEnginePtr engine = m_luaEngine;
  auto previousLifetime = engine->setPromiseLifetime(loadLifetime);
  auto restored = finally([&] { engine->setPromiseLifetime(std::move(previousLifetime)); });
  auto created = engine->createContext();
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto context = std::move(*created);
  auto cache = m_scriptCache;
  auto sourceDiagnostics = m_sourceDiagnostics;
  if (auto required = context.setRequireFunction(
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
            return std::unexpected(LuaException::format("Cyclic Lua require: {} -> {}", chain, path));
          }
          auto script = cache->script(environment.stack().engine(), path, revision, *sourceDiagnostics);
          if (!script)
            return std::unexpected(std::move(script.error()));
          loading.push_back(path);
          auto popped = finally([&] { loading.pop_back(); });
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
      auto exists = (*assets)->assetExists(path);
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
      ? context.setCallbacks(callbacks.first, callbacks.second.withLifetime(loadLifetime))
      : context.setCallbacks(callbacks.first, callbacks.second);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return context;
}

std::expected<void, StarException> LuaRoot::collectGarbage(std::optional<unsigned> steps) {
  if (m_luaEngine)
    return m_luaEngine->collectGarbage(steps);
  return {};
}

void LuaRoot::setAutoGarbageCollection(bool enabled) {
  if (m_luaEngine)
    m_luaEngine->setAutoGarbageCollection(enabled);
}

void LuaRoot::tuneAutoGarbageCollection(float pause, float multiplier) {
  if (m_luaEngine)
    m_luaEngine->tuneAutoGarbageCollection(pause, multiplier);
}

std::size_t LuaRoot::luaMemoryUsage() const {
  return m_luaEngine ? m_luaEngine->memoryUsage() : 0;
}

std::size_t LuaRoot::scriptCacheMemoryUsage() const {
  return m_luaEngine ? m_scriptCache->memoryUsage() : 0;
}

void LuaRoot::addCallbacks(String const& groupName, LuaCallbacks const& callbacks) {
  m_luaCallbacks[groupName] = callbacks;
}

LuaEngine& LuaRoot::luaEngine() const {
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
    if (auto cached = scripts.maybeRef(path))
      return cached->get();
  }
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  // Capture the indexed provider before reading or compiling can fail.
  // Hot reload keeps the asset index stable; snapshots survive cache clearing
  // and are owned separately from Root by the engine's resolver.
  if (!sourceDiagnostics.contains(path)) {
    auto descriptor = (*assets)->assetDescriptor(path);
    if (descriptor && *descriptor) {
      auto sourcePath = (*assets)->assetSourcePath((*descriptor)->source);
      auto pathRef = sourcePath
        ? std::optional<std::reference_wrapper<String const>>(std::cref(*sourcePath))
        : std::nullopt;
      sourceDiagnostics.set(path, String(Assets::sourceDiagnostic(*(*descriptor)->source, pathRef)));
    }
  }
  auto bytes = (*assets)->bytes(path);
  if (!bytes) {
    if (auto diagnostic = sourceDiagnostics.maybeRef(path)) {
      auto summary = *bytes.error().diagnosticSummary();
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
      return std::unexpected(std::move(bytes.error()).withSummary(std::move(summary)).withContext(
          strf("Lua source '{}': {}", path, diagnostic->get())));
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

bool LuaRoot::ScriptCache::scriptLoaded(String const& path) const {
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

std::size_t LuaRoot::ScriptCache::memoryUsage() const {
  std::lock_guard locker(mutex);
  std::size_t total = 0;
  for (auto const& p : scripts)
    total += p.second->size();
  return total;
}

}
