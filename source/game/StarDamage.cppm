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

import star.either;
import star.line;
namespace Star {

struct DamageSource {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageSource const& damageSource);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageSource& damageSource);

  using DamageArea = std::variant<std::monostate, PolyF, Line2F>;
  using Knockback = std::variant<std::monostate, float, Vec2F>;

  DamageSource();
  [[nodiscard]] static std::expected<DamageSource, StarException> create(Json const& v);
  DamageSource(DamageType damageType,
      DamageArea damageArea,
      float damage,
      bool trackSourceEntity,
      EntityId sourceEntityId,
      EntityDamageTeam team,
      std::optional<String> damageRepeatGroup,
      std::optional<float> damageRepeatTimeout,
      String damageSourceKind,
      List<EphemeralStatusEffect> statusEffects,
      Knockback knockback,
      bool rayCheck);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  DamageSource& translate(Vec2F const& position);

  [[nodiscard]] std::expected<bool, StarException> intersectsWithPoly(WorldGeometry const& worldGeometry, PolyF const& poly) const;
  Vec2F knockbackMomentum(WorldGeometry const& worldGeometry, Vec2F const& targetCenter) const;

  bool operator==(DamageSource const& rhs) const;

  DamageType damageType;
  DamageArea damageArea;
  float damage;

  bool trackSourceEntity;
  // The originating entity for the damage, which can be different than the
  // actual causing entity.  Optional, defaults to NullEntityId.
  EntityId sourceEntityId;
  EntityDamageTeam team;

  // Applying damage will block other DamageSources with the same
  // damageRepeatGroup from applying damage until the timeout expires, to
  // prevent damages being applied every tick.  This is optional, and if it is
  // omitted then the repeat group will instead be based on the causing entity
  // id.
  std::optional<String> damageRepeatGroup;
  // Can override the default repeat damage timeout with a custom timeout.
  std::optional<float> damageRepeatTimeout;

  String damageSourceKind;
  List<EphemeralStatusEffect> statusEffects;
  // Either directionless knockback momentum or directional knockback momentum
  Knockback knockback;
  // Should a collision check from the source entity to the impact center be
  // peformed before applying the damage?
  bool rayCheck;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageSource const& damageSource);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageSource& damageSource);

struct DamageRequest {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageRequest const& damageRequest);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageRequest& damageRequest);

  DamageRequest();
  [[nodiscard]] static std::expected<DamageRequest, StarException> create(Json const& v);
  DamageRequest(HitType hitType,
      DamageType type,
      float damage,
      Vec2F const& knockbackMomentum,
      EntityId sourceEntityId,
      String const& damageSourceKind,
      List<EphemeralStatusEffect> const& statusEffects);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  HitType hitType;
  DamageType damageType;
  float damage;
  Vec2F knockbackMomentum;
  // May be different than the entity that actually caused damage, for example,
  // a player firing a projectile.
  EntityId sourceEntityId;
  String damageSourceKind;
  List<EphemeralStatusEffect> statusEffects;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageRequest const& damageRequest);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageRequest& damageRequest);

struct DamageNotification {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageNotification const& damageNotification);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageNotification& damageNotification);

  DamageNotification();
  [[nodiscard]] static std::expected<DamageNotification, StarException> create(Json const& v);
  DamageNotification(EntityId sourceEntityId,
      EntityId targetEntityId,
      Vec2F position,
      float damageDealt,
      float healthLost,
      HitType hitType,
      String damageSourceKind,
      String targetMaterialKind);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  EntityId sourceEntityId;
  EntityId targetEntityId;
  Vec2F position;
  float damageDealt;
  float healthLost;
  HitType hitType;
  String damageSourceKind;
  String targetMaterialKind;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageNotification const& damageNotification);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageNotification& damageNotification);
}

export module star.damage;

export namespace Star {
  using ::Star::DamageSource;
  using ::Star::DamageRequest;
  using ::Star::DamageNotification;
}
