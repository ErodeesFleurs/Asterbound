import std;
import star.format;
import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.data_stream_extra;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
import star.force_regions;
import star.physics_entity;

import star.rect;
namespace Star {

std::expected<PhysicsMovingCollision, StarException> PhysicsMovingCollision::fromJson(Json const& json) {
  PhysicsMovingCollision pmc;
  auto positionJson = json.opt("position");
  if (!positionJson)
    return std::unexpected(std::move(positionJson.error()));
  if (*positionJson) {
    auto position = jsonToVec2F(**positionJson);
    if (!position)
      return std::unexpected(std::move(position.error()));
    pmc.position = *position;
  }
  auto collisionJson = json.get("collision");
  if (!collisionJson)
    return std::unexpected(std::move(collisionJson.error()));
  auto collision = jsonToPolyF(*collisionJson);
  if (!collision)
    return std::unexpected(std::move(collision.error()));
  pmc.collision = std::move(*collision);
  auto kindName = json.getString("collisionKind", "block");
  if (!kindName)
    return std::unexpected(std::move(kindName.error()));
  auto kind = enumValue<CollisionKindNames>(*kindName);
  if (!kind)
    return std::unexpected(std::move(kind.error()));
  pmc.collisionKind = *kind;
  auto filter = jsonToPhysicsCategoryFilter(json);
  if (!filter)
    return std::unexpected(std::move(filter.error()));
  pmc.categoryFilter = std::move(*filter);
  return pmc;
}

RectF PhysicsMovingCollision::boundBox() const {
  return collision.boundBox().translated(position);
}

void PhysicsMovingCollision::translate(Vec2F const& pos) {
  position += pos;
}

bool PhysicsMovingCollision::operator==(PhysicsMovingCollision const& rhs) const {
  return std::tie(position, collision, collisionKind, categoryFilter) == std::tie(rhs.position, rhs.collision, rhs.collisionKind, rhs.categoryFilter);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PhysicsMovingCollision& pmc) {
  PhysicsMovingCollision decoded;
  auto read = ds >> decoded.position >> decoded.collision >> decoded.collisionKind >> decoded.categoryFilter;
  if (!read)
    return std::unexpected(std::move(read.error()));
  pmc = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PhysicsMovingCollision const& pmc) {
  return ds << pmc.position << pmc.collision << pmc.collisionKind << pmc.categoryFilter;
}

MovingCollisionId::MovingCollisionId() : physicsEntityId(NullEntityId), collisionIndex(0) {}

MovingCollisionId::MovingCollisionId(EntityId physicsEntityId, std::size_t collisionIndex)
  : physicsEntityId(physicsEntityId), collisionIndex(collisionIndex) {}

bool MovingCollisionId::operator==(MovingCollisionId const& rhs) {
  return std::tie(physicsEntityId, collisionIndex) == std::tie(rhs.physicsEntityId, rhs.collisionIndex);
}

bool MovingCollisionId::valid() const {
  return physicsEntityId != NullEntityId;
}

MovingCollisionId::operator bool() const {
  return valid();
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MovingCollisionId& mci) {
  MovingCollisionId decoded;
  auto id = ds.read(decoded.physicsEntityId);
  if (!id)
    return std::unexpected(std::move(id.error()));
  auto index = ds.read_vlq_s(decoded.collisionIndex);
  if (!index)
    return std::unexpected(std::move(index.error()));
  mci = decoded;
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MovingCollisionId const& mci) {
  auto id = ds.write(mci.physicsEntityId);
  if (!id)
    return std::unexpected(std::move(id.error()));
  auto index = ds.write_vlq_s(mci.collisionIndex);
  if (!index)
    return std::unexpected(std::move(index.error()));
  return std::ref(ds);
}

std::expected<List<PhysicsForceRegion>, StarException> PhysicsEntity::forceRegions() const {
  return List<PhysicsForceRegion>{};
}

std::size_t PhysicsEntity::movingCollisionCount() const {
  return 0;
}

std::expected<std::optional<PhysicsMovingCollision>, StarException> PhysicsEntity::movingCollision(std::size_t) const {
  return {};
}

}
