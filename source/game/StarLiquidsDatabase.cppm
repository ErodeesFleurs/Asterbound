module;
import std;
import star.string;
import star.format;
import star.json;
import star.either;
import star.vector;
import star.liquid_types;
import star.material_types;
import star.map;
import star.cellular_liquid;
import star.item_descriptor;

namespace Star {
class Assets;
class MaterialDatabase;

struct LiquidExceptionTag {
  static constexpr std::string_view name() { return "LiquidException"; }
};
using LiquidException = StarError<LiquidExceptionTag, StarException>;

[[nodiscard]] std::expected<float, StarException> load_liquid_bucket_size(Assets const& assets);

using LiquidInteractionResult = Either<MaterialId, LiquidId>;

struct LiquidSettings {
  String name;
  LiquidId id = empty_liquid_id;
  String path;
  Json config;
  String description;

  unsigned tick_delta = 0;
  Vec4B liquid_color;
  Vec3F radiant_light_level;
  ItemDescriptor item_drop;
  JsonArray status_effects;

  HashMap<LiquidId, LiquidInteractionResult> interactions;
};

class LiquidsDatabase {
public:
  [[nodiscard]]
  static std::expected<std::shared_ptr<LiquidsDatabase>, StarException> create(Assets const& assets, MaterialDatabase const& material_database);

  LiquidCellEngineParameters liquid_engine_parameters() const;
  float background_drain() const;

  StringList liquid_names() const;

  // Borrows this database's settings in ascending ID order, excluding "empty".
  [[nodiscard]] std::span<LiquidSettings const> all_liquid_settings() const &;
  std::span<LiquidSettings const> all_liquid_settings() const && = delete;

  bool is_liquid_name(String const& name) const;
  bool is_valid_liquid_id(LiquidId liquid_id) const;

  [[nodiscard]]
  std::expected<LiquidId, StarException> liquid_id(String const& name) const;
  [[nodiscard]]
  std::expected<String, StarException> liquid_name(LiquidId liquid_id) const;
  [[nodiscard]]
  std::expected<String, StarException> liquid_description(LiquidId liquid_id, String const& species) const;
  [[nodiscard]]
  std::expected<String, StarException> liquid_description(LiquidId liquid_id) const;

  // Borrows from this database; absent for empty_liquid_id and unregistered IDs.
  [[nodiscard]] std::optional<std::reference_wrapper<LiquidSettings const>> liquid_settings(LiquidId liquid_id) const &;
  std::optional<std::reference_wrapper<LiquidSettings const>> liquid_settings(LiquidId liquid_id) const && = delete;

  Vec3F radiant_light(LiquidLevel level) const;

  std::optional<LiquidInteractionResult> interact(LiquidId target, LiquidId other) const;

private:
  LiquidsDatabase() = default;
  LiquidsDatabase(LiquidsDatabase const&) = delete;
  LiquidsDatabase& operator=(LiquidsDatabase const&) = delete;

  LiquidCellEngineParameters m_liquid_engine_parameters;
  float m_background_drain = 0;
  std::vector<LiquidSettings> m_settings;
  // Dense slots are numbered from 1; zero means the ID has no settings.
  std::array<LiquidId, std::size_t{1} + std::numeric_limits<LiquidId>::max()> m_setting_slots{};
  StringMap<LiquidId> m_liquid_names;
};

inline float LiquidsDatabase::background_drain() const {
  return m_background_drain;
}

inline std::span<LiquidSettings const> LiquidsDatabase::all_liquid_settings() const & {
  return m_settings;
}

inline bool LiquidsDatabase::is_liquid_name(String const& name) const {
  return m_liquid_names.contains(name);
}

inline bool LiquidsDatabase::is_valid_liquid_id(LiquidId liquid_id) const {
  return liquid_id == empty_liquid_id || m_setting_slots[liquid_id] != 0;
}

inline std::optional<std::reference_wrapper<LiquidSettings const>> LiquidsDatabase::liquid_settings(LiquidId liquid_id) const & {
  if (auto slot = m_setting_slots[liquid_id])
    return std::cref(m_settings[slot - 1]);
  return std::nullopt;
}

inline Vec3F LiquidsDatabase::radiant_light(LiquidLevel level) const {
  if (auto settings = liquid_settings(level.liquid))
    return settings->get().radiant_light_level * level.level;
  return {};
}

}

export module star.liquids_database;

export namespace Star {
  using ::Star::LiquidExceptionTag;
  using ::Star::LiquidException;
  using ::Star::load_liquid_bucket_size;
  using ::Star::LiquidInteractionResult;
  using ::Star::LiquidSettings;
  using ::Star::LiquidsDatabase;
}
