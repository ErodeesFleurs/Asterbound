module;
import std;
import star.format;
import star.string;
import star.json;

import star.thread;
import star.ordered_map;
import star.lru_cache;

namespace Star {

class Assets;
class Root;

struct StatEvent {
  String event_name;
  StringList scripts;
  Json config;
};

struct Achievement {
  String name;
  StringList triggers;
  StringList scripts;
  Json config;
};

class StatisticsDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  [[nodiscard]] static std::expected<std::shared_ptr<StatisticsDatabase>, StarException> create(Root& root);
  StatisticsDatabase(ConstructionKey, Root& root) : m_root(root) {}

  [[nodiscard]] std::expected<std::shared_ptr<StatEvent>, StarException> event(String const& name) const;

  [[nodiscard]] std::expected<std::shared_ptr<Achievement>, StarException> achievement(String const& name) const;
  StringList all_achievements() const;
  // The index is immutable after creation; the view is valid while this database lives.
  std::span<String const> achievements_for_stat(String const& stat_name) const;

private:
  [[nodiscard]] std::expected<void, StarException> load();
  [[nodiscard]] static std::expected<std::shared_ptr<StatEvent>, StarException> read_event(Assets const& assets, String const& path);
  [[nodiscard]] static std::expected<std::shared_ptr<Achievement>, StarException> read_achievement(Assets const& assets, String const& path);

  Root& m_root;
  StringMap<String> m_event_paths;
  StringMap<String> m_achievement_paths;
  StringMap<StringList> m_stat_achievements;
  mutable std::mutex m_cache_mutex;
  mutable HashLruCache<String, std::shared_ptr<StatEvent>> m_event_cache;
  mutable HashLruCache<String, std::shared_ptr<Achievement>> m_achievement_cache;
};

}

export module star.statistics_database;

export namespace Star {
  using ::Star::StatEvent;
  using ::Star::Achievement;
  using ::Star::StatisticsDatabase;
}
