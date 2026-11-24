module;
import std;
import star.list;
import star.map;
import star.data_stream;
import star.format;
import star.json;

import star.vector;
import star.bi_map;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_group;

namespace Star {

class Assets;

struct TileDamageExceptionTag {
  static constexpr std::string_view name() { return "TileDamageException"; }
};
using TileDamageException = StarError<TileDamageExceptionTag, StarException>;

List<Vec2I> tile_area_brush(float range, Vec2F const& center_offset, bool square_mode);

enum class TileDamageType : std::uint8_t {
  // Damage done that will not actually kill the target
  Protected,
  // Best at chopping down trees, things made of wood, etc.
  Plantish,
  // For digging / drilling through materials
  Blockish,
  // Gravity gun etc
  Beamish,
  // Penetrating damage done passivly by explosions.
  Explosive,
  // Can melt certain block types
  Fire,
  // Can "till" certain materials into others
  Tilling
};
extern std::expected<EnumMap<TileDamageType>, StarException> const tile_damage_type_names;

bool tile_damage_is_penetrating(TileDamageType damage_type);

struct TileDamage {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TileDamage& tile_damage);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TileDamage const& tile_damage);
  TileDamage();
  TileDamage(TileDamageType type, float amount, unsigned harvest_level = 1);

  TileDamageType type;
  float amount;
  unsigned harvest_level;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TileDamage& tile_damage);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TileDamage const& tile_damage);

class TileDamageParameters {
public:
  TileDamageParameters();

  // If 'config' is a string type, it is assumed to be a descriptor file,
  // otherwise it should contain map configuration data.
  [[nodiscard]] static std::expected<TileDamageParameters, StarException> create(Assets const& assets, Json const& config, std::optional<float> health_override = {}, std::optional<unsigned> harvest_level_override = {});

  float damage_fraction(TileDamage const& damage) const;
  float recovery_fraction(float dt) const;
  unsigned required_harvest_level() const;
  float maximum_effect_time() const;
  float total_health() const;

  [[nodiscard]] std::expected<TileDamageParameters, StarException> sum(this TileDamageParameters result, TileDamageParameters const& other);

  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TileDamageParameters& tile_damage);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TileDamageParameters const& tile_damage);

private:
  static std::expected<TileDamageParameters, StarException> parse_config(Json const& config, std::optional<float> health_override, std::optional<unsigned> harvest_level_override);

  Map<TileDamageType, float> m_damages;
  float m_damage_recovery_per_second;
  float m_maximum_effect_time;
  float m_total_health;
  unsigned m_required_harvest_level;
};

class TileDamageStatus {
public:
  TileDamageStatus();

  float damage_percentage() const;
  float damage_effect_percentage() const;
  Vec2F source_position() const;
  TileDamageType damage_type() const;

  void reset();
  void damage(TileDamageParameters const& damage_parameters, Vec2F const& source_position, TileDamage const& damage);
  void recover(TileDamageParameters const& damage_parameters, float dt);

  bool healthy() const;
  bool damaged() const;
  bool damage_protected() const;
  bool dead() const;
  bool harvested() const;

  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TileDamageStatus& tile_damage_status);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TileDamageStatus const& tile_damage_status);

private:
  void update_damage_effect_percentage();

  float m_damage_percentage;
  float m_damage_effect_time_factor;
  bool m_harvested;
  Vec2F m_damage_source_position;
  TileDamageType m_damage_type;
  float m_damage_effect_percentage;
};

class EntityTileDamageStatus : public NetElementGroup {
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  explicit EntityTileDamageStatus(ConstructionKey);
  [[nodiscard]] static std::expected<std::shared_ptr<EntityTileDamageStatus>, StarException> create();

  float damage_percentage() const;
  float damage_effect_percentage() const;
  TileDamageType damage_type() const;

  void reset();
  void damage(TileDamageParameters const& damage_parameters, TileDamage const& damage);
  void recover(TileDamageParameters const& damage_parameters, float dt);

  bool healthy() const;
  bool damaged() const;
  bool damage_protected() const;
  bool dead() const;
  bool harvested() const;

private:
  NetElementFloat m_damage_percentage;
  NetElementFloat m_damage_effect_time_factor;
  NetElementBool m_damage_harvested;
  NetElementEnum<TileDamageType> m_damage_type;
};

inline float TileDamageStatus::damage_percentage() const {
  return m_damage_percentage;
}

inline float TileDamageStatus::damage_effect_percentage() const {
  return m_damage_effect_percentage;
}

inline Vec2F TileDamageStatus::source_position() const {
  return m_damage_source_position;
}

inline TileDamageType TileDamageStatus::damage_type() const {
  return m_damage_type;
}

}

export module star.tile_damage;

export namespace Star {
  using ::Star::TileDamageParameters;
  using ::Star::TileDamageStatus;
  using ::Star::EntityTileDamageStatus;
  using ::Star::TileDamageExceptionTag;
  using ::Star::TileDamageException;
  using ::Star::tile_area_brush;
  using ::Star::TileDamageType;
  using ::Star::tile_damage_type_names;
  using ::Star::tile_damage_is_penetrating;
  using ::Star::TileDamage;
}
