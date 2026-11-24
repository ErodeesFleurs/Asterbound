module;

import std;
import star.string;
import star.map;
import star.format;
import star.json;
import star.strong_typedef;
import star.data_stream;
import star.id_map;
import star.status_types;

namespace Star {

class Assets;

struct StatusEffectDatabaseExceptionTag {
  static constexpr std::string_view name() { return "StatusEffectDatabaseException"; }
};
using StatusEffectDatabaseException = StarError<StatusEffectDatabaseExceptionTag, StarException>;

// Named, unique, unstackable scripted effects.
struct UniqueStatusEffectConfig {
  String name;
  std::optional<String> blockingStat;
  Json effectConfig;
  float defaultDuration;
  StringList scripts;
  unsigned scriptDelta;
  std::optional<String> animationConfig;

  String label;
  String description;
  std::optional<String> icon;

  JsonObject to_json() const;
};

class StatusEffectDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<StatusEffectDatabase>, StarException> create(std::shared_ptr<Assets const> assets);

  // Definitions are immutable and borrowed for this database generation.
  [[nodiscard]] std::expected<std::reference_wrapper<UniqueStatusEffectConfig const>, StarException>
      unique_effect_config(UniqueStatusEffect const& effect) const &;

private:
  StatusEffectDatabase() = default;
  [[nodiscard]] std::expected<UniqueStatusEffectConfig, StarException> parseUniqueEffect(Json const& config, String const& path) const;

  HashMap<UniqueStatusEffect, UniqueStatusEffectConfig> m_uniqueEffects;
};

}

export module star.status_effect_database;

export namespace Star {
  using ::Star::StatusEffectDatabaseExceptionTag;
  using ::Star::StatusEffectDatabaseException;
  using ::Star::StatusEffectDatabase;
  using ::Star::UniqueStatusEffectConfig;
}
