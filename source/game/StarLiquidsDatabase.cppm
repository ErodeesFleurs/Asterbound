module;
import std;
import star.string;
import star.format;
import star.json;
import star.either;
import star.vector;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.list;
import star.map;
import star.ordered_set;
import star.random;
import star.data_stream;
import star.bi_map;


import star.cellular_liquid;
import star.item_descriptor;

namespace Star {
class Assets;
class MaterialDatabase;

struct LiquidExceptionTag {
  static constexpr std::string_view name() { return "LiquidException"; }
};
using LiquidException = StarError<LiquidExceptionTag, StarException>;

using LiquidInteractionResult = Either<MaterialId, LiquidId>;

struct LiquidSettings {
  LiquidSettings();

  String name;
  LiquidId id;
  String path;
  Json config;
  Json descriptions;

  unsigned tickDelta;
  Vec4B liquidColor;
  Vec3F radiantLightLevel;
  ItemDescriptor itemDrop;
  JsonArray statusEffects;

  HashMap<LiquidId, std::optional<LiquidInteractionResult>> interactions;
};

class LiquidsDatabase {
public:
  [[nodiscard]]
  static std::expected<std::shared_ptr<LiquidsDatabase>, StarException> create(std::shared_ptr<Assets const> const& assets, std::shared_ptr<MaterialDatabase const> const& materialDatabase);

  LiquidCellEngineParameters liquidEngineParameters() const;
  float backgroundDrain() const;

  StringList liquidNames() const;

  // Returns settings object for all liquids except "empty"
  List<std::shared_ptr<LiquidSettings const>> allLiquidSettings() const;

  bool isLiquidName(String const& name) const;
  bool isValidLiquidId(LiquidId liquidId) const;

  [[nodiscard]]
  std::expected<LiquidId, StarException> liquidId(String const& str) const;
  [[nodiscard]]
  std::expected<String, StarException> liquidName(LiquidId liquidId) const;
  [[nodiscard]]
  std::expected<String, StarException> liquidDescription(LiquidId liquidId, String const& species) const;
  [[nodiscard]]
  std::expected<String, StarException> liquidDescription(LiquidId liquidId) const;
  [[nodiscard]]
  std::expected<std::optional<String>, StarException> liquidPath(LiquidId liquidId) const;
  [[nodiscard]]
  std::expected<std::optional<Json>, StarException> liquidConfig(LiquidId liquidId) const;

  // Returns null on EmptyLiquidId or invalid liquid id
  std::shared_ptr<LiquidSettings const> liquidSettings(LiquidId liquidId) const;

  Vec3F radiantLight(LiquidLevel level) const;

  std::optional<LiquidInteractionResult> interact(LiquidId target, LiquidId other) const;

private:
  LiquidsDatabase() = default;
  LiquidCellEngineParameters m_liquidEngineParameters;
  float m_backgroundDrain;
  List<std::shared_ptr<LiquidSettings const>> m_settings;
  StringMap<LiquidId> m_liquidNames;
};

inline float LiquidsDatabase::backgroundDrain() const {
  return m_backgroundDrain;
}

inline bool LiquidsDatabase::isLiquidName(String const& name) const {
  return m_liquidNames.contains(name);
}

inline bool LiquidsDatabase::isValidLiquidId(LiquidId liquidId) const {
  return liquidId == EmptyLiquidId || (liquidId < m_settings.size() && m_settings[liquidId]);
}

inline std::shared_ptr<LiquidSettings const> LiquidsDatabase::liquidSettings(LiquidId liquidId) const {
  if (liquidId >= m_settings.size())
    return {};
  return m_settings[liquidId];
}

inline Vec3F LiquidsDatabase::radiantLight(LiquidLevel level) const {
  if (level.liquid < m_settings.size()) {
    if (auto const& settings = m_settings[level.liquid])
      return settings->radiantLightLevel * level.level;
  }

  return Vec3F();
}

}

export module star.liquids_database;

export namespace Star {
  using ::Star::LiquidExceptionTag;
  using ::Star::LiquidException;
  using ::Star::LiquidInteractionResult;
  using ::Star::LiquidSettings;
  using ::Star::LiquidsDatabase;
}
