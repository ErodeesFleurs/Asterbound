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

import star.asset_source;
import star.assets;
import star.root;

import star.statistics_database;

namespace Star {

namespace {
std::expected<StringList, StarException> read_triggers(Json const& config) {
  auto triggers = config.get("triggers", Json());
  if (!triggers) return std::unexpected(std::move(triggers.error()));
  if (triggers->is_null()) return StringList{};
  return json_to_string_list(*triggers);
}
}

std::expected<std::shared_ptr<StatisticsDatabase>, StarException> StatisticsDatabase::create(Root& root) {
  auto result = std::make_shared<StatisticsDatabase>(ConstructionKey{}, root);
  if (auto loaded = result->load(); !loaded)
    return std::unexpected(std::move(loaded.error()));
  return result;
}

std::expected<void, StarException> StatisticsDatabase::load() {
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto event_files = (*assets)->scan_extension("event");
  if (!event_files) return std::unexpected(std::move(event_files.error()));
  if (auto result = (*assets)->queue_jsons(event_files->get()); !result) return std::unexpected(std::move(result.error()));
  auto achievement_files = (*assets)->scan_extension("achievement");
  if (!achievement_files) return std::unexpected(std::move(achievement_files.error()));
  if (auto result = (*assets)->queue_jsons(achievement_files->get()); !result) return std::unexpected(std::move(result.error()));
  for (auto const& path : event_files->get()) {
    auto const& file = path.string();
    std::expected<void, StarException> logging_result;
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config) return std::unexpected(std::move(config.error()));
      auto name = config->get_string("eventName");
      if (!name) return std::unexpected(std::move(name.error()));
      auto [entry, inserted] = m_event_paths.try_emplace(std::move(*name), file);
      if (!inserted)
        logging_result = m_root.logger().error("Event {} defined twice, second time from {}", entry->first, file);
      return {};
    }();
    if (!logging_result) return std::unexpected(std::move(logging_result.error()));
    if (!loaded) {
      if (auto result =
            m_root.logger().error_exception(Star::format("Error loading event file {}", file),
                                            ExceptionDiagnostic((*assets)->with_asset_context(
                                              std::move(loaded.error()), file, "Loading statistics event")));
          !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  for (auto const& path : achievement_files->get()) {
    auto const& file = path.string();
    std::expected<void, StarException> logging_result;
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config) return std::unexpected(std::move(config.error()));
      auto name = config->get_string("name");
      if (!name) return std::unexpected(std::move(name.error()));
      auto triggers = read_triggers(*config);
      if (!triggers) return std::unexpected(std::move(triggers.error()));
      auto [entry, inserted] = m_achievement_paths.try_emplace(std::move(*name), file);
      if (!inserted) {
        logging_result = m_root.logger().error("Achievement {} defined twice, second time from {}", entry->first, file);
        return {};
      }
      for (auto& stat : *triggers)
        m_stat_achievements.try_emplace(std::move(stat)).first->second.push_back(entry->first);
      return {};
    }();
    if (!logging_result) return std::unexpected(std::move(logging_result.error()));
    if (!loaded) {
      if (auto result = m_root.logger().error_exception(
            Star::format("Error loading achievement file {}", file),
            ExceptionDiagnostic((*assets)->with_asset_context(std::move(loaded.error()), file, "Loading achievement")));
          !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<std::shared_ptr<StatEvent>, StarException> StatisticsDatabase::event(String const& name) const {
  std::lock_guard locker(m_cache_mutex);
  auto cached = m_event_cache.get(name, [&database = *this](String const& name) -> std::expected<std::shared_ptr<StatEvent>, StarException> {
    if (auto path = database.m_event_paths.find_ref(name)) {
      auto assets = database.m_root.assets();
      if (!assets) return std::unexpected(std::move(assets.error()));
      return read_event(**assets, path->get());
    }
    return std::shared_ptr<StatEvent>{};
  });
  if (!cached) return std::unexpected(std::move(cached.error()));
  return cached->get();
}

std::expected<std::shared_ptr<Achievement>, StarException> StatisticsDatabase::achievement(String const& name) const {
  std::lock_guard locker(m_cache_mutex);
  auto cached = m_achievement_cache.get(name, [&database = *this](String const& name) -> std::expected<std::shared_ptr<Achievement>, StarException> {
    if (auto path = database.m_achievement_paths.find_ref(name)) {
      auto assets = database.m_root.assets();
      if (!assets) return std::unexpected(std::move(assets.error()));
      return read_achievement(**assets, path->get());
    }
    return std::shared_ptr<Achievement>{};
  });
  if (!cached) return std::unexpected(std::move(cached.error()));
  return cached->get();
}

StringList StatisticsDatabase::all_achievements() const {
  return m_achievement_paths.keys();
}

std::span<String const> StatisticsDatabase::achievements_for_stat(String const& stat_name) const {
  if (auto names = m_stat_achievements.find_ref(stat_name))
    return names->get();
  return {};
}

std::expected<std::shared_ptr<StatEvent>, StarException> StatisticsDatabase::read_event(Assets const& assets, String const& path) {
  auto parsed = [&]() -> std::expected<std::shared_ptr<StatEvent>, StarException> {
    auto config = assets.json(path);
    if (!config) return std::unexpected(std::move(config.error()));
    auto name = config->get_string("eventName");
    if (!name) return std::unexpected(std::move(name.error()));
    auto scripts_json = config->get("scripts");
    if (!scripts_json) return std::unexpected(std::move(scripts_json.error()));
    auto scripts = json_to_string_list(*scripts_json);
    if (!scripts) return std::unexpected(std::move(scripts.error()));
    return std::make_shared<StatEvent>(StatEvent{std::move(*name), std::move(*scripts), std::move(*config)});
  }();
  if (!parsed)
    return std::unexpected(assets.with_asset_context(std::move(parsed.error()), path, "Loading statistics event"));
  return parsed;
}

std::expected<std::shared_ptr<Achievement>, StarException> StatisticsDatabase::read_achievement(Assets const& assets, String const& path) {
  auto parsed = [&]() -> std::expected<std::shared_ptr<Achievement>, StarException> {
    auto config = assets.json(path);
    if (!config) return std::unexpected(std::move(config.error()));
    auto name = config->get_string("name");
    if (!name) return std::unexpected(std::move(name.error()));
    auto triggers = read_triggers(*config);
    if (!triggers) return std::unexpected(std::move(triggers.error()));
    auto scripts_json = config->get("scripts");
    if (!scripts_json) return std::unexpected(std::move(scripts_json.error()));
    auto scripts = json_to_string_list(*scripts_json);
    if (!scripts) return std::unexpected(std::move(scripts.error()));
    return std::make_shared<Achievement>(Achievement{std::move(*name), std::move(*triggers), std::move(*scripts), std::move(*config)});
  }();
  if (!parsed)
    return std::unexpected(assets.with_asset_context(std::move(parsed.error()), path, "Loading achievement"));
  return parsed;
}

}
