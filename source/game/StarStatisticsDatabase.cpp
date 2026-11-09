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
import star.logging;
import star.output_proxy;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;

import star.asset_source;
import star.assets;
import star.root;

import star.statistics_database;

namespace Star {

StatisticsDatabase::StatisticsDatabase(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<StatisticsDatabase>, StarException> StatisticsDatabase::create(Root& root) {
  auto result = std::shared_ptr<StatisticsDatabase>(new StatisticsDatabase(root));
  if (auto loaded = result->load(); !loaded)
    return std::unexpected(std::move(loaded.error()));
  return result;
}

std::expected<void, StarException> StatisticsDatabase::load() {
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto eventFiles = (*assets)->scanExtension("event");
  if (!eventFiles) return std::unexpected(std::move(eventFiles.error()));
  if (auto result = (*assets)->queueJsons(eventFiles->get()); !result) return std::unexpected(std::move(result.error()));
  auto achievementFiles = (*assets)->scanExtension("achievement");
  if (!achievementFiles) return std::unexpected(std::move(achievementFiles.error()));
  if (auto result = (*assets)->queueJsons(achievementFiles->get()); !result) return std::unexpected(std::move(result.error()));
  for (auto const& path : eventFiles->get()) {
    auto const& file = path.string();
    std::expected<void, StarException> loggingResult;
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config) return std::unexpected(std::move(config.error()));
      auto name = config->getString("eventName");
      if (!name) return std::unexpected(std::move(name.error()));
      if (m_eventPaths.contains(*name))
        loggingResult = m_root.logger().error("Event {} defined twice, second time from {}", *name, file);
      else
        m_eventPaths[*name] = file;
      return {};
    }();
    if (!loggingResult) return std::unexpected(std::move(loggingResult.error()));
    if (!loaded) {
      if (auto result =
            m_root.logger().error_exception(Star::format("Error loading event file {}", file),
                                            ExceptionDiagnostic((*assets)->withAssetContext(
                                              std::move(loaded.error()), file, "Loading statistics event")));
          !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  for (auto const& path : achievementFiles->get()) {
    auto const& file = path.string();
    std::expected<void, StarException> loggingResult;
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config) return std::unexpected(std::move(config.error()));
      auto name = config->getString("name");
      if (!name) return std::unexpected(std::move(name.error()));
      if (m_achievementPaths.contains(*name))
        loggingResult = m_root.logger().error("Achievement {} defined twice, second time from {}", *name, file);
      else
        m_achievementPaths[*name] = file;
      auto triggers = config->getArray("triggers", {});
      if (!triggers) return std::unexpected(std::move(triggers.error()));
      for (auto const& trigger : *triggers) {
        auto stat = trigger.toString();
        if (!stat) return std::unexpected(std::move(stat.error()));
        m_statAchievements[*stat].push_back(*name);
      }
      return {};
    }();
    if (!loggingResult) return std::unexpected(std::move(loggingResult.error()));
    if (!loaded) {
      if (auto result = m_root.logger().error_exception(
            Star::format("Error loading achievement file {}", file),
            ExceptionDiagnostic((*assets)->withAssetContext(std::move(loaded.error()), file, "Loading achievement")));
          !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<std::shared_ptr<StatEvent>, StarException> StatisticsDatabase::event(String const& name) const {
  std::lock_guard locker(m_cacheMutex);
  auto cached = m_eventCache.get(name, [&database = *this](String const& name) -> std::expected<std::shared_ptr<StatEvent>, StarException> {
    if (auto path = database.m_eventPaths.maybe(name)) {
      auto assets = database.m_root.assets();
      if (!assets) return std::unexpected(std::move(assets.error()));
      return readEvent(**assets, *path);
    }
    return std::shared_ptr<StatEvent>{};
  });
  if (!cached) return std::unexpected(std::move(cached.error()));
  return cached->get();
}

std::expected<std::shared_ptr<Achievement>, StarException> StatisticsDatabase::achievement(String const& name) const {
  std::lock_guard locker(m_cacheMutex);
  auto cached = m_achievementCache.get(name, [&database = *this](String const& name) -> std::expected<std::shared_ptr<Achievement>, StarException> {
    if (auto path = database.m_achievementPaths.maybe(name)) {
      auto assets = database.m_root.assets();
      if (!assets) return std::unexpected(std::move(assets.error()));
      return readAchievement(**assets, *path);
    }
    return std::shared_ptr<Achievement>{};
  });
  if (!cached) return std::unexpected(std::move(cached.error()));
  return cached->get();
}

StringList StatisticsDatabase::allAchievements() const {
  return m_achievementPaths.keys();
}

StringList StatisticsDatabase::achievementsForStat(String const& statName) const {
  return m_statAchievements.value(statName);
}

std::expected<std::shared_ptr<StatEvent>, StarException> StatisticsDatabase::readEvent(Assets const& assets, String const& path) {
  auto parsed = [&]() -> std::expected<std::shared_ptr<StatEvent>, StarException> {
    auto config = assets.json(path);
    if (!config) return std::unexpected(std::move(config.error()));
    auto name = config->getString("eventName");
    if (!name) return std::unexpected(std::move(name.error()));
    auto scriptsJson = config->get("scripts");
    if (!scriptsJson) return std::unexpected(std::move(scriptsJson.error()));
    auto scripts = jsonToStringList(*scriptsJson);
    if (!scripts) return std::unexpected(std::move(scripts.error()));
    return std::make_shared<StatEvent>(StatEvent{std::move(*name), std::move(*scripts), std::move(*config)});
  }();
  if (!parsed)
    return std::unexpected(assets.withAssetContext(std::move(parsed.error()), path, "Loading statistics event"));
  return parsed;
}

std::expected<std::shared_ptr<Achievement>, StarException> StatisticsDatabase::readAchievement(Assets const& assets, String const& path) {
  auto parsed = [&]() -> std::expected<std::shared_ptr<Achievement>, StarException> {
    auto config = assets.json(path);
    if (!config) return std::unexpected(std::move(config.error()));
    auto name = config->getString("name");
    if (!name) return std::unexpected(std::move(name.error()));
    auto triggersJson = config->get("triggers");
    if (!triggersJson) return std::unexpected(std::move(triggersJson.error()));
    auto triggers = jsonToStringList(*triggersJson);
    if (!triggers) return std::unexpected(std::move(triggers.error()));
    auto scriptsJson = config->get("scripts");
    if (!scriptsJson) return std::unexpected(std::move(scriptsJson.error()));
    auto scripts = jsonToStringList(*scriptsJson);
    if (!scripts) return std::unexpected(std::move(scripts.error()));
    return std::make_shared<Achievement>(Achievement{std::move(*name), std::move(*triggers), std::move(*scripts), std::move(*config)});
  }();
  if (!parsed)
    return std::unexpected(assets.withAssetContext(std::move(parsed.error()), path, "Loading achievement"));
  return parsed;
}

}
