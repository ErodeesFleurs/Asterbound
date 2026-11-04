module;
import std;
import star.json;
import star.format;
import star.data_stream;
import star.bi_map;
import star.game_types;

namespace Star {

enum DamageType : std::uint8_t { NoDamage, Damage, IgnoresDef, Knockback, Environment, Status };
inline constexpr auto DamageTypeNames = sortedEnumNames(std::to_array<std::pair<DamageType, std::string_view>>({
  {DamageType::NoDamage, "NoDamage"},
  {DamageType::Damage, "Damage"},
  {DamageType::IgnoresDef, "IgnoresDef"},
  {DamageType::Knockback, "Knockback"},
  {DamageType::Environment, "Environment"},
  {DamageType::Status, "Status"},
}));

enum class HitType { Hit, StrongHit, WeakHit, ShieldHit, Kill };
inline constexpr auto HitTypeNames = sortedEnumNames(std::to_array<std::pair<HitType, std::string_view>>({
  {HitType::Hit, "Hit"},
  {HitType::StrongHit, "StrongHit"},
  {HitType::WeakHit, "WeakHit"},
  {HitType::ShieldHit, "ShieldHit"},
  {HitType::Kill, "Kill"},
}));

enum class TeamType : std::uint8_t {
  Null,
  // non-PvP-enabled players and player allied NPCs
  Friendly,
  // hostile and neutral NPCs and monsters
  Enemy,
  // PvP-enabled players
  PVP,
  // cannot damage anything, can be damaged by Friendly/PVP/Assistant
  Passive,
  // cannot damage or be damaged
  Ghostly,
  // cannot damage enemies, can be damaged by anything except enemy
  Environment,
  // damages anything except ghostly, damaged by anything except ghostly/passive
  // used for self damage
  Indiscriminate,
  // cannot damage friendlies and cannot be damaged by anything
  Assistant
};
inline constexpr auto TeamTypeNames = sortedEnumNames(std::to_array<std::pair<TeamType, std::string_view>>({
  {TeamType::Null, "null"},
  {TeamType::Friendly, "friendly"},
  {TeamType::Enemy, "enemy"},
  {TeamType::PVP, "pvp"},
  {TeamType::Passive, "passive"},
  {TeamType::Ghostly, "ghostly"},
  {TeamType::Environment, "environment"},
  {TeamType::Indiscriminate, "indiscriminate"},
  {TeamType::Assistant, "assistant"},
}));

typedef std::uint16_t TeamNumber;

struct EntityDamageTeam {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EntityDamageTeam& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EntityDamageTeam const& value);
  EntityDamageTeam();
  explicit EntityDamageTeam(TeamType type, TeamNumber team = 0);
  [[nodiscard]] static std::expected<EntityDamageTeam, StarException> create(Json const& json);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  bool canDamage(EntityDamageTeam victim, bool victimIsSelf) const;

  bool operator==(EntityDamageTeam const& rhs) const;

  TeamType type;
  TeamNumber team;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EntityDamageTeam const& team);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EntityDamageTeam& team);

TeamNumber soloPvpTeam(ConnectionId clientId);
}

export module star.damage_types;

export namespace Star {
  using ::Star::DamageType;
  using ::Star::NoDamage;
  using ::Star::Damage;
  using ::Star::IgnoresDef;
  using ::Star::Knockback;
  using ::Star::Environment;
  using ::Star::Status;
  using ::Star::DamageTypeNames;
  using ::Star::HitType;
  using ::Star::HitTypeNames;
  using ::Star::TeamType;
  using ::Star::TeamTypeNames;
  using ::Star::TeamNumber;
  using ::Star::EntityDamageTeam;
  using ::Star::soloPvpTeam;
}
