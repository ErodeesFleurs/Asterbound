module;
import std;
import star.string;
import star.list;
import star.json;
import star.format;

import star.vector;
import star.data_stream;
import star.game_types;
import star.damage_types;
import star.poly;
import star.world_geometry;
import star.strong_typedef;
import star.id_map;
import star.status_types;

import star.line;
namespace Star {

struct DamageSource {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageSource const& source);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageSource& source);

  using DamageArea = std::variant<std::monostate, PolyF, Line2F>;
  using Knockback = std::variant<std::monostate, float, Vec2F>;

  [[nodiscard]] static std::expected<DamageSource, StarException> create(Json const& v);

  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  DamageSource& translate(Vec2F const& position);

  [[nodiscard]] std::expected<bool, StarException> intersects_with_poly(WorldGeometry const& geometry, PolyF const& poly) const;
  Vec2F knockback_momentum(WorldGeometry const& geometry, Vec2F const& target_center) const;

  bool operator==(DamageSource const&) const = default;

  DamageType damage_type = DamageType::NoDamage;
  DamageArea damage_area;
  float damage = 0;

  bool track_source_entity = true;
  // The originating entity for the damage, which can be different than the
  // actual causing entity.  Optional, defaults to null_entity_id.
  EntityId source_entity_id = null_entity_id;
  EntityDamageTeam team;

  // Applying damage will block other DamageSources with the same
  // damage_repeat_group from applying damage until the timeout expires, to
  // prevent damages being applied every tick.  This is optional, and if it is
  // omitted then the repeat group will instead be based on the causing entity
  // id.
  std::optional<String> damage_repeat_group;
  // Can override the default repeat damage timeout with a custom timeout.
  std::optional<float> damage_repeat_timeout;

  String damage_source_kind;
  List<EphemeralStatusEffect> status_effects;
  // Either directionless knockback momentum or directional knockback momentum
  Knockback knockback;
  // Should a collision check from the source entity to the impact center be
  // performed before applying the damage?
  bool ray_check = false;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageSource const& source);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageSource& source);

struct DamageRequest {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageRequest const& request);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageRequest& request);

  [[nodiscard]] static std::expected<DamageRequest, StarException> create(Json const& v);

  [[nodiscard]] std::expected<Json, StarException> to_json() const;
  bool operator==(DamageRequest const&) const = default;

  HitType hit_type = HitType::Hit;
  DamageType damage_type = DamageType::Damage;
  float damage = 0;
  Vec2F knockback_momentum;
  // May be different than the entity that actually caused damage, for example,
  // a player firing a projectile.
  EntityId source_entity_id = null_entity_id;
  String damage_source_kind;
  List<EphemeralStatusEffect> status_effects;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageRequest const& request);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageRequest& request);

struct DamageNotification {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageNotification const& notification);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageNotification& notification);

  [[nodiscard]] static std::expected<DamageNotification, StarException> create(Json const& v);

  [[nodiscard]] std::expected<Json, StarException> to_json() const;
  bool operator==(DamageNotification const&) const = default;

  EntityId source_entity_id = null_entity_id;
  EntityId target_entity_id = null_entity_id;
  Vec2F position;
  float damage_dealt = 0;
  float health_lost = 0;
  HitType hit_type = HitType::Hit;
  String damage_source_kind;
  String target_material_kind;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageNotification const& notification);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageNotification& notification);
}

export module star.damage;

export namespace Star {
  using ::Star::DamageSource;
  using ::Star::DamageRequest;
  using ::Star::DamageNotification;
}
