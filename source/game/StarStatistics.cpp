import star.lua_root;
import star.json;
import star.json_extra;
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
import star.listener;
import std;
import star.string;
import star.lua;
import star.format;
import star.version;
import star.file;
import star.logging;
import star.output_proxy;

import star.lua_components;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.statistics_service;

import star.statistics_database;
import star.versioning_database;
import star.config_lua_bindings;
import star.statistics;

namespace Star {

Statistics::Statistics(Root& root, String const& storageDirectory, std::shared_ptr<StatisticsService> service)
  : m_root(root), m_service(std::move(service)), m_storageDirectory(storageDirectory), m_initialized(!m_service) {}

std::expected<std::shared_ptr<Statistics>, StarException> Statistics::create(Root& root, String const& storageDirectory, std::shared_ptr<StatisticsService> service) {
  auto result = std::shared_ptr<Statistics>(new Statistics(root, storageDirectory, std::move(service)));
  if (auto loaded = result->readStatistics(); !loaded)
    return std::unexpected(std::move(loaded.error()));
  auto luaRoot = LuaRoot::create(root);
  if (!luaRoot) return std::unexpected(std::move(luaRoot.error()));
  result->m_luaRoot = std::move(*luaRoot);
  return result;
}

std::expected<void, StarException> Statistics::writeStatistics() {
  auto versioningDatabase = m_root.versioningDatabase();
  if (!versioningDatabase) return std::unexpected(std::move(versioningDatabase.error()));
  String filename = File::relative_to(m_storageDirectory, "statistics");

  JsonObject stats;
  for (auto const& entry : m_stats)
    stats.set(entry.first, entry.second.toJson());
  JsonObject storage = {
      { "stats", std::move(stats) },
      { "achievements", jsonFromStringSet(m_achievements) }
    };

  auto versionedStorage = (*versioningDatabase)->makeCurrentVersionedJson("Statistics", storage);
  if (!versionedStorage) return std::unexpected(std::move(versionedStorage.error()));
  return VersionedJson::write_file(*versionedStorage, filename);
}

Json Statistics::stat(String const& name, Json def) const {
  if (auto value = m_stats.find_ref(name))
    return value->get().value;
  return def;
}

std::optional<String> Statistics::statType(String const& name) const {
  if (auto value = m_stats.find_ref(name))
    return value->get().type;
  return {};
}

bool Statistics::achievementUnlocked(String const& name) const {
  return m_achievements.contains(name);
}

void Statistics::recordEvent(String const& name, Json const& fields) {
  m_pendingEvents.push_back(std::make_pair(name, fields));
}

bool Statistics::reset() {
  if (m_initialized) {
    if (!m_service || m_service->reset()) {
      m_stats = {};
      m_achievements = {};
      return true;
    }
  }
  return false;
}

std::expected<void, StarException> Statistics::update() {
  if (m_service) {
    if (auto error = m_service->error()) {
      if (auto logged = m_root.logger().errorException("Statistics platform service error", ExceptionDiagnostic(*error)); !logged)
        return std::unexpected(std::move(logged.error()));
      // Service failed. Continue with local stats and achievements only.
      m_service = {};
      m_initialized = true;
      return {};
    }
  }

  if (!m_initialized) {
    if (m_service->initialized()) {
      if (auto merged = mergeServiceStatistics(); !merged)
        return std::unexpected(std::move(merged.error()));
      m_initialized = true;
    }
  }

  for (auto event : m_pendingEvents) {
    if (auto processed = processEvent(event.first, event.second); !processed)
      return std::unexpected(std::move(processed.error()));
  }

  for (String const& achievement : m_pendingAchievementChecks) {
    auto checked = checkAchievement(achievement);
    if (!checked) return std::unexpected(std::move(checked.error()));
    if (*checked) {
      if (auto unlocked = unlockAchievement(achievement); !unlocked)
        return std::unexpected(std::move(unlocked.error()));
    }
  }

  if (m_service && (m_pendingEvents.size() > 0 || m_pendingAchievementChecks.size() > 0)) {
    if (auto flushed = m_service->flush(); !flushed)
      return std::unexpected(std::move(flushed.error()));
  }
  m_pendingEvents = {};
  m_pendingAchievementChecks = {};
  return {};
}

std::expected<Statistics::Stat, StarException> Statistics::Stat::fromJson(Json const& json) {
  auto type = json.getString("type");
  if (!type) return std::unexpected(std::move(type.error()));
  auto value = json.get("value");
  if (!value) return std::unexpected(std::move(value.error()));
  return Stat{std::move(*type), std::move(*value)};
}

Json Statistics::Stat::toJson() const {
  return JsonObject {
    {"type", type},
    {"value", value}
  };
}

std::expected<void, StarException> Statistics::processEvent(String const& name, Json const& fields) {
  if (m_service)
    m_service->reportEvent(name, fields);
  if (m_root.logger().loggable(LogLevel::Debug)) {
    auto fieldsText = fields.repr();
    if (!fieldsText) return std::unexpected(std::move(fieldsText.error()));
    if (auto logged = m_root.logger().debug("Event {} {}", name, *fieldsText); !logged)
      return std::unexpected(std::move(logged.error()));
  }
  auto database = m_root.statisticsDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto event = (*database)->event(name);
  if (!event) return std::unexpected(std::move(event.error()));
  if (*event) {
    auto result = runStatScript((*event)->scripts, (*event)->config, "event", name, fields);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> Statistics::setStat(String const& name, String const& type, Json const& value) {
  if (m_root.logger().loggable(LogLevel::Debug)) {
    auto valueText = value.repr();
    if (!valueText) return std::unexpected(std::move(valueText.error()));
    if (auto logged = m_root.logger().debug("Stat {} ({}) : {}", name, type, *valueText); !logged)
      return std::unexpected(std::move(logged.error()));
  }
  m_stats[name] = Stat{type, value};
  if (m_service) {
    auto stored = m_service->setStat(name, type, value);
    if (!stored) return std::unexpected(std::move(stored.error()));
  }
  auto database = m_root.statisticsDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  m_pendingAchievementChecks.insert_range((*database)->achievementsForStat(name));
  return {};
}

std::expected<void, StarException> Statistics::unlockAchievement(String const& name) {
  if (achievementUnlocked(name)) return {};
  m_achievements.add(name);
  if (m_service) {
    auto unlocked = m_service->unlockAchievement(name);
    if (!unlocked) return std::unexpected(std::move(unlocked.error()));
  }
  return m_root.logger().debug("Achievement get {}", name);
}

std::expected<bool, StarException> Statistics::checkAchievement(String const& achievementName) {
  auto database = m_root.statisticsDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto achievement = (*database)->achievement(achievementName);
  if (!achievement) return std::unexpected(std::move(achievement.error()));
  if (!*achievement)
    return std::unexpected(StarException::format("Unknown achievement '{}'", achievementName));
  if (achievementUnlocked((*achievement)->name)) return true;
  auto result = runStatScript<bool>((*achievement)->scripts, (*achievement)->config, "check", achievementName);
  if (!result) return std::unexpected(std::move(result.error()));
  return *result && **result;
}

std::expected<void, StarException> Statistics::readStatistics() {
  auto database = m_root.versioningDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto loaded = [&]() -> std::expected<void, StarException> {
    String filename = File::relative_to(m_storageDirectory, "statistics");
    auto exists = File::exists(filename);
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (!*exists)
      return {};
    auto versioned = VersionedJson::read_file(filename);
    if (!versioned)
      return std::unexpected(std::move(versioned.error()));
    auto storage = (*database)->loadVersionedJson(*versioned, "Statistics");
    if (!storage)
      return std::unexpected(std::move(storage.error()));
    auto stats = storage->getObject("stats", {});
    if (!stats)
      return std::unexpected(std::move(stats.error()));
    StringMap<Stat> parsedStats;
    for (auto const& entry : *stats) {
      auto stat = Stat::fromJson(entry.second);
      if (!stat)
        return std::unexpected(std::move(stat.error()));
      parsedStats[entry.first] = std::move(*stat);
    }
    m_stats = std::move(parsedStats);
    auto achievementsJson = storage->get("achievements", JsonArray{});
    if (!achievementsJson)
      return std::unexpected(std::move(achievementsJson.error()));
    auto achievements = jsonToStringSet(*achievementsJson);
    if (!achievements)
      return std::unexpected(std::move(achievements.error()));
    m_achievements = std::move(*achievements);
    return {};
  }();
  if (!loaded)
    return m_root.logger().warnException("Error loading local player statistics file, resetting", ExceptionDiagnostic(loaded.error()));
  return {};
}

std::expected<void, StarException> Statistics::mergeServiceStatistics() {
  if (!m_service || !m_service->initialized() || m_service->error()) return {};

  // Publish achievements we unlocked when the platform service was unavailable.
  StringSet serviceAchievements = m_service->achievementsUnlocked();
  for (String const& achievement : m_achievements.difference(serviceAchievements)) {
    auto unlocked = m_service->unlockAchievement(achievement);
    if (!unlocked) return std::unexpected(std::move(unlocked.error()));
  }
  // Locally store all the achievements we unlocked in a different install:
  m_achievements.insert_range(serviceAchievements);

  // Publish our local statistics, in case we made progress while the service
  // was unavailable.
  for (auto const& stat : m_stats) {
    auto stored = m_service->setStat(stat.first, stat.second.type, stat.second.value);
    if (!stored) return std::unexpected(std::move(stored.error()));
  }

  // However, don't _pull_ stats from the service - not all stats are recorded
  // so inconsistencies will creep in if we try. For example, if the service
  // is recording the number of poptop kills but not the total number of kills,
  // we could end up with a situation like "2 total kills, 8 poptops killed."

  // The best we can do is let the client be authoritative over its stats and
  // have the service validate changes it receives to make sure they only
  // ever increase.

  return m_service->flush();
}

std::expected<LuaCallbacks, StarException> Statistics::makeStatisticsCallbacks() {
  LuaCallbacks callbacks;

  if (auto result = callbacks.registerCallbackWithSignature<std::expected<void, StarException>, String, String, Json>(
        "setStat",
        [this](String const& name, String const& type, Json const& value) { return setStat(name, type, value); });
      !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<Json, String, Json>(
        "stat", [this](String const& name, Json def) { return stat(name, std::move(def)); });
      !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<std::optional<String>, String>(
        "statType", [this](String const& name) { return statType(name); });
      !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<bool, String>(
        "achievementUnlocked", [this](String const& name) { return achievementUnlocked(name); });
      !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<std::expected<bool, StarException>, String>(
        "checkAchievement", [this](String const& name) { return checkAchievement(name); });
      !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<std::expected<void, StarException>, String>(
        "unlockAchievement", [this](String const& name) { return unlockAchievement(name); });
      !result)
    return std::unexpected(std::move(result.error()));

  return callbacks;
}

template <typename Result, typename... V>
[[nodiscard]] std::expected<std::optional<Result>, StarException> Statistics::runStatScript(StringList const& scripts, Json const& config, String const& functionName, V&&... args) {
  LuaBaseComponent script(m_root);
  if (auto result = script.setLuaRoot(m_luaRoot); !result) return std::unexpected(std::move(result.error()));
  if (auto result = script.setScripts(scripts); !result) return std::unexpected(std::move(result.error()));
  auto configCallbacks = LuaBindings::makeConfigCallbacks([config](String const& name, Json const& def) {
    return config.query(name, def);
  });
  if (!configCallbacks) return std::unexpected(std::move(configCallbacks.error()));
  if (auto result = script.addCallbacks("config", std::move(*configCallbacks)); !result) return std::unexpected(std::move(result.error()));
  auto statisticsCallbacks = makeStatisticsCallbacks();
  if (!statisticsCallbacks) return std::unexpected(std::move(statisticsCallbacks.error()));
  if (auto result = script.addCallbacks("statistics", std::move(*statisticsCallbacks)); !result) return std::unexpected(std::move(result.error()));
  auto initialized = script.init();
  if (!initialized) {
    auto cleanup = script.uninit();
    if (!cleanup && !cleanup.error().is<LuaException>()) return std::unexpected(std::move(cleanup.error()));
    if (initialized.error().is<LuaException>()) return std::optional<Result>{};
    return std::unexpected(std::move(initialized.error()));
  }
  auto result = script.invoke<Result>(functionName, std::forward<V>(args)...);
  auto uninitialized = script.uninit();
  if (!uninitialized && !uninitialized.error().is<LuaException>()) return std::unexpected(std::move(uninitialized.error()));
  if (!result) {
    if (result.error().template is<LuaException>()) return std::optional<Result>{};
    return std::unexpected(std::move(result.error()));
  }
  return result;
}

}
