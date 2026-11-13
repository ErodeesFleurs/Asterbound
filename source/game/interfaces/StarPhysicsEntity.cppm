module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.id_map;
import star.poly;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;


import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;

import star.light_source;

import star.entity;
import star.collision_block;

import star.force_regions;

import star.rect;
namespace Star {

struct PhysicsMovingCollision {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PhysicsMovingCollision& pmc);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PhysicsMovingCollision const& pmc);
  [[nodiscard]] static std::expected<PhysicsMovingCollision, StarException> fromJson(Json const& json);

  RectF boundBox() const;

  void translate(Vec2F const& pos);

  bool operator==(PhysicsMovingCollision const& rhs) const;

  Vec2F position;
  PolyF collision;
  CollisionKind collisionKind;
  PhysicsCategoryFilter categoryFilter;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PhysicsMovingCollision& pmc);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PhysicsMovingCollision const& pmc);

struct MovingCollisionId {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MovingCollisionId& mci);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MovingCollisionId const& mci);
  MovingCollisionId();
  MovingCollisionId(EntityId physicsEntityId, std::size_t collisionIndex);

  bool operator==(MovingCollisionId const& rhs);

  // Returns true if the MovingCollisionId is not empty, i.e. default
  // constructed
  bool valid() const;
  operator bool() const;

  EntityId physicsEntityId;
  std::size_t collisionIndex;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MovingCollisionId& mci);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MovingCollisionId const& mci);

class PhysicsEntity : public virtual Entity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<PhysicsEntity, Entity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PhysicsEntity>(); }

  [[nodiscard]] virtual std::expected<List<PhysicsForceRegion>, StarException> forceRegions() const;

  virtual std::size_t movingCollisionCount() const;
  [[nodiscard]] virtual std::expected<std::optional<PhysicsMovingCollision>, StarException> movingCollision(std::size_t positionIndex) const;
};

}

export module star.physics_entity;

export namespace Star {
  using ::Star::PhysicsEntity;
  using ::Star::PhysicsMovingCollision;
  using ::Star::MovingCollisionId;
}
