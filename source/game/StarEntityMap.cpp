import std;
import star.optional;
import star.string;
import star.logging;
import star.format;
import star.json;
import star.id_map;
import star.rect;
import star.map;
import star.set;
import star.spatial_hash_2d;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.ordered_map;
import star.rpc_promise;
import star.lua;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.root;
import star.physics_entity;
import star.mobile_entity;
import star.scripted_entity;
import star.status_effect_entity;
import star.movement_controller;
import star.animation;
import star.particle;
import star.lua_components;
import star.effect_emitter;
import star.projectile;

import star.entity_map;

import star.math_common;
import star.algorithm;
namespace Star {

float const EntityMapSpatialHashSectorSize = 16.0f;
int const EntityMap::MaximumEntityBoundBox = 10000;

EntityMap::EntityMap(Vec2U const& worldSize, EntityId beginIdSpace, EntityId endIdSpace)
  : m_geometry(worldSize),
    m_spatialMap(EntityMapSpatialHashSectorSize),
    m_nextId(beginIdSpace),
    m_beginIdSpace(beginIdSpace),
    m_endIdSpace(endIdSpace) {}

std::expected<EntityId, StarException> EntityMap::reserveEntityId() {
  if (m_spatialMap.size() >= static_cast<std::size_t>(m_endIdSpace - m_beginIdSpace))
    return std::unexpected(EntityMapException("No more entity id space in EntityMap::reserveEntityId"));

  EntityId id = m_nextId;
  while (m_spatialMap.contains(id))
    id = cycleIncrement(id, m_beginIdSpace, m_endIdSpace);
  m_nextId = cycleIncrement(id, m_beginIdSpace, m_endIdSpace);

  return id;
}

std::expected<std::optional<EntityId>, StarException> EntityMap::maybeReserveEntityId(EntityId entityId) {
  if (m_spatialMap.size() >= static_cast<std::size_t>(m_endIdSpace - m_beginIdSpace))
    return std::unexpected(EntityMapException("No more entity id space in EntityMap::reserveEntityId"));

  if (entityId == NullEntityId || m_spatialMap.contains(entityId))
    return std::optional<EntityId>{};
  else
    return entityId;
}

std::expected<EntityId, StarException> EntityMap::reserveEntityId(EntityId entityId) {
  if (entityId == NullEntityId)
    return reserveEntityId();
  auto reserved = maybeReserveEntityId(entityId);
  if (!reserved) return std::unexpected(std::move(reserved.error()));
  if (*reserved)
    return **reserved;

  m_nextId = entityId;
  return reserveEntityId();
}


std::expected<void, StarException> EntityMap::addEntity(std::shared_ptr<Entity> entity) {
  auto position = entity->position();
  auto boundBox = entity->metaBoundBox();
  auto entityId = entity->entityId();
  auto uniqueId = entity->uniqueId();

  if (m_spatialMap.contains(entityId))
    return std::unexpected(EntityMapException::format("Duplicate entity id '{}' in EntityMap::addEntity", entityId));

  if (boundBox.isNegative() || boundBox.width() > MaximumEntityBoundBox || boundBox.height() > MaximumEntityBoundBox) {
    return std::unexpected(EntityMapException::format("Entity id: {} type: {} bound box is negative or beyond the "
                                                      "maximum entity bound box size in EntityMap::addEntity",
                                                      entity->entityId(), static_cast<int>(entity->entityType())));
  }

  if (entityId == NullEntityId)
    return std::unexpected(EntityMapException::format("Null entity id in EntityMap::addEntity"));

  if (uniqueId && m_uniqueMap.hasLeftValue(*uniqueId))
    return std::unexpected(EntityMapException::format("Duplicate entity unique id ({}) on entity id ({}) in EntityMap::addEntity", *uniqueId, entityId));

  auto rects = m_geometry.splitRect(boundBox, position);
  if (!rects) return std::unexpected(std::move(rects.error()));
  if (uniqueId) {
    auto inserted = m_uniqueMap.add(*uniqueId, entityId);
    if (!inserted) return std::unexpected(std::move(inserted.error()));
  }
  m_spatialMap.set(entityId, *rects, std::move(entity));
  return {};
}

std::shared_ptr<Entity> EntityMap::removeEntity(EntityId entityId) {
  if (auto entity = m_spatialMap.remove(entityId)) {
    m_uniqueMap.removeRight(entityId);
    return std::move(*entity);
  }
  return {};
}

std::size_t EntityMap::size() const {
  return m_spatialMap.size();
}

List<EntityId> EntityMap::entityIds() const {
  return m_spatialMap.keys();
}

std::expected<void, StarException> EntityMap::updateAllEntities(EntityCallback const& callback, std::function<bool(std::shared_ptr<Entity> const&, std::shared_ptr<Entity> const&)> sortOrder) {
  auto updateEntityInfo = [&](SpatialMap::Entry const& entry) -> std::expected<void, StarException> {
    auto const& entity = entry.value;

    auto position = entity->position();
    auto boundBox = entity->metaBoundBox();

    if (boundBox.isNegative() || boundBox.width() > MaximumEntityBoundBox || boundBox.height() > MaximumEntityBoundBox) {
      return std::unexpected(EntityMapException::format("Entity id: {} type: {} bound box is negative or beyond the "
                                                        "maximum entity bound box size in EntityMap::addEntity",
                                                        entity->entityId(), static_cast<int>(entity->entityType())));
    }

    auto entityId = entity->entityId();
    if (entityId == NullEntityId)
      return std::unexpected(EntityMapException::format("Null entity id in EntityMap::setEntityInfo"));

    auto rects = m_geometry.splitRect(boundBox, position);
    if (!rects) return std::unexpected(std::move(rects.error()));
    if (!std::ranges::equal(*rects, entry.rects)) {
      auto result = m_spatialMap.set(entityId, *rects);
      if (!result) return std::unexpected(std::move(result.error()));
    }

    auto uniqueId = entity->uniqueId();
    if (uniqueId) {
      if (auto existingEntityId = m_uniqueMap.maybeRight(*uniqueId)) {
        if (entityId != *existingEntityId)
          return std::unexpected(EntityMapException::format("Duplicate entity unique id on entity ids ({}) and ({})", *existingEntityId, entityId));
      } else {
        m_uniqueMap.removeRight(entityId);
        auto result = m_uniqueMap.add(*uniqueId, entityId);
        if (!result) return std::unexpected(std::move(result.error()));
      }
    } else {
      m_uniqueMap.removeRight(entityId);
    }
    return {};
  };

  // Snapshot entry references so adding entities from a callback is safe.
  // Removing entities during callbacks remains unsupported.
  m_entrySortBuffer.clear();
  for (auto const& entry : m_spatialMap.entries())
    m_entrySortBuffer.append(std::cref(entry.second));

  if (sortOrder) {
    m_entrySortBuffer.sort([&sortOrder](auto a, auto b) {
        return sortOrder(a.get().value, b.get().value);
      });
  }

  for (auto entry : m_entrySortBuffer) {
    if (callback) {
      auto result = callback(entry.get().value);
      if (!result) return std::unexpected(std::move(result.error()));
    }
    auto result = updateEntityInfo(entry.get());
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

EntityId EntityMap::uniqueEntityId(String const& uniqueId) const {
  return m_uniqueMap.maybeRight(uniqueId).value_or(NullEntityId);
}

std::shared_ptr<Entity> EntityMap::entity(EntityId entityId) const {
  auto entity = m_spatialMap.value(entityId);
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((!entity || entity->entityId() == entityId));
  }
  return entity;
}

std::shared_ptr<Entity> EntityMap::uniqueEntity(String const& uniqueId) const {
  return entity(uniqueEntityId(uniqueId));
}

std::expected<List<std::shared_ptr<Entity>>, StarException> EntityMap::entityQuery(RectF const& boundBox, EntityFilter const& filter) const {
  List<std::shared_ptr<Entity>> values;
  auto result = forEachEntity(boundBox, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (filter) {
      auto accepted = filter(entity);
      if (!accepted) return std::unexpected(std::move(accepted.error()));
      if (!*accepted) return {};
    }
    values.append(entity);
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return values;
}

std::expected<List<std::shared_ptr<Entity>>, StarException> EntityMap::entitiesAt(Vec2F const& pos, EntityFilter const& filter) const {
  auto entityList = entityQuery(RectF::withCenter(pos, {0, 0}), filter);
  if (!entityList) return std::unexpected(std::move(entityList.error()));
  sortByComputedValue(*entityList, [&](std::shared_ptr<Entity> const& entity) -> float {
    return vmagSquared(entity->position() - pos);
  });
  return std::move(*entityList);
}

std::expected<List<std::shared_ptr<TileEntity>>, StarException> EntityMap::entitiesAtTile(Vec2I const& pos, EntityFilterOf<TileEntity> const& filter) const {
  List<std::shared_ptr<TileEntity>> values;
  auto result = forEachEntityAtTile(pos, [&](std::shared_ptr<TileEntity> const& entity) -> std::expected<void, StarException> {
    if (filter) {
      auto accepted = filter(entity);
      if (!accepted) return std::unexpected(std::move(accepted.error()));
      if (!*accepted) return {};
    }
    values.append(entity);
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return values;
}

std::expected<void, StarException> EntityMap::forEachEntity(RectF const& boundBox, EntityCallback const& callback) const {
  if (!callback)
    return std::unexpected(EntityMapException("Missing EntityMap::forEachEntity callback"));
  auto rects = m_geometry.splitRect(boundBox);
  if (!rects) return std::unexpected(std::move(rects.error()));
  return m_spatialMap.forEach(*rects, callback);
}

std::expected<void, StarException> EntityMap::forEachEntityLine(Vec2F const& begin, Vec2F const& end, EntityCallback const& callback) const {
  if (!callback)
    return std::unexpected(EntityMapException("Missing EntityMap::forEachEntityLine callback"));
  return forEachEntity(RectF::boundBoxOf(begin, end), [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    auto intersects = m_geometry.lineIntersectsRect({begin, end}, entity->metaBoundBox().translated(entity->position()));
    if (!intersects) return std::unexpected(std::move(intersects.error()));
    if (*intersects)
      return callback(entity);
    return {};
  });
}

std::expected<void, StarException> EntityMap::forEachEntityAtTile(Vec2I const& pos, EntityCallbackOf<TileEntity> const& callback) const {
  if (!callback)
    return std::unexpected(EntityMapException("Missing EntityMap::forEachEntityAtTile callback"));
  RectF rect(Vec2F(pos[0], pos[1]), Vec2F(pos[0] + 1, pos[1] + 1));
  return forEachEntity(rect, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (auto tileEntity = as<TileEntity>(entity)) {
      for (Vec2I space : tileEntity->spaces()) {
        if (m_geometry.equal(pos, space + tileEntity->tilePosition())) {
          auto result = callback(tileEntity);
          if (!result) return std::unexpected(std::move(result.error()));
        }
      }
    }
    return {};
  });
}

std::expected<void, StarException> EntityMap::forAllEntities(EntityCallback const& callback, std::function<bool(std::shared_ptr<Entity> const&, std::shared_ptr<Entity> const&)> sortOrder) const {
  if (!callback)
    return std::unexpected(EntityMapException("Missing EntityMap::forAllEntities callback"));
  // Snapshot owner references so adding entities from a callback is safe.
  // Removing entities during callbacks remains unsupported.
  List<std::reference_wrapper<std::shared_ptr<Entity> const>> allEntities;
  allEntities.reserve(m_spatialMap.size());
  for (auto const& entry : m_spatialMap.entries())
    allEntities.append(std::cref(entry.second.value));

  if (sortOrder) {
    allEntities.sort([&sortOrder](auto a, auto b) {
        return sortOrder(a.get(), b.get());
      });
  }

  for (auto borrowed : allEntities) {
    auto const& entity = borrowed.get();
    auto result = callback(entity);
    if (!result) {
      if (!EntityTypeNames) return std::unexpected(EntityTypeNames.error());
      auto type = EntityTypeNames->getRight(entity->entityType());
      if (!type) return std::unexpected(std::move(type.error()));
      auto name = entity->name();
      if (!name) return std::unexpected(std::move(name.error()));
      auto logged = entity->root().logger().error("[EntityMap] Exception caught running forAllEntities callback for {} entity {} (named \"{}\")",
          type->get(), entity->entityId(), *name);
      if (!logged) return std::unexpected(std::move(logged.error()));
      return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<std::shared_ptr<Entity>, StarException> EntityMap::findEntity(RectF const& boundBox, EntityFilter const& filter) const {
  if (!filter)
    return std::unexpected(EntityMapException("Missing EntityMap::findEntity filter"));
  std::shared_ptr<Entity> found;
  auto result = forEachEntity(boundBox, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (found) return {};
    auto accepted = filter(entity);
    if (!accepted) return std::unexpected(std::move(accepted.error()));
    if (*accepted) found = entity;
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return found;
}

std::expected<std::shared_ptr<Entity>, StarException> EntityMap::findEntityLine(Vec2F const& begin, Vec2F const& end, EntityFilter const& filter) const {
  if (!filter)
    return std::unexpected(EntityMapException("Missing EntityMap::findEntityLine filter"));
  return findEntity(RectF::boundBoxOf(begin, end), [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
    auto intersects = m_geometry.lineIntersectsRect({begin, end}, entity->metaBoundBox().translated(entity->position()));
    if (!intersects) return std::unexpected(std::move(intersects.error()));
    if (*intersects) return filter(entity);
    return false;
  });
}

std::expected<std::shared_ptr<Entity>, StarException> EntityMap::findEntityAtTile(Vec2I const& pos, EntityFilterOf<TileEntity> const& filter) const {
  if (!filter)
    return std::unexpected(EntityMapException("Missing EntityMap::findEntityAtTile filter"));
  RectF rect(Vec2F(pos[0], pos[1]), Vec2F(pos[0] + 1, pos[1] + 1));
  return findEntity(rect, [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
    if (auto tileEntity = as<TileEntity>(entity)) {
      for (Vec2I space : tileEntity->spaces()) {
        if (m_geometry.equal(pos, space + tileEntity->tilePosition())) {
          auto accepted = filter(tileEntity);
          if (!accepted) return std::unexpected(std::move(accepted.error()));
          if (*accepted) return true;
        }
      }
    }
    return false;
  });
}

std::expected<List<std::shared_ptr<Entity>>, StarException> EntityMap::entityLineQuery(Vec2F const& begin, Vec2F const& end, EntityFilter const& filter) const {
  List<std::shared_ptr<Entity>> values;
  auto result = forEachEntityLine(begin, end, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (filter) {
      auto accepted = filter(entity);
      if (!accepted) return std::unexpected(std::move(accepted.error()));
      if (!*accepted) return {};
    }
    values.append(entity);
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return values;
}

std::expected<std::shared_ptr<Entity>, StarException> EntityMap::closestEntity(Vec2F const& center, float radius, EntityFilter const& filter) const {
  std::shared_ptr<Entity> closest;
  float distSquared = square(radius);
  RectF boundBox(center[0] - radius, center[1] - radius, center[0] + radius, center[1] + radius);
  auto result = forEachEntity(boundBox, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    Vec2F pos = entity->position();
    float thisDistSquared = m_geometry.diff(center, pos).magnitudeSquared();
    if (distSquared > thisDistSquared) {
      if (filter) {
        auto accepted = filter(entity);
        if (!accepted) return std::unexpected(std::move(accepted.error()));
        if (!*accepted) return {};
      }
      distSquared = thisDistSquared;
      closest = entity;
    }
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return closest;
}
std::expected<std::shared_ptr<InteractiveEntity>, StarException> EntityMap::interactiveEntityNear(Vec2F const& pos, float maxRadius) const {
  auto rect = RectF::withCenter(pos, Vec2F::filled(maxRadius));
  std::shared_ptr<InteractiveEntity> interactiveEntity;
  double bestDistance = maxRadius + 100;
  double bestCenterDistance = maxRadius + 100;
  auto result = forEachEntity(rect, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
      if (auto ie = as<InteractiveEntity>(entity)) {
        if (ie->isInteractive()) {
          if (auto tileEntity = as<TileEntity>(entity)) {
            for (Vec2I space : tileEntity->interactiveSpaces()) {
              auto dist = m_geometry.diff(pos, centerOfTile(space + tileEntity->tilePosition())).magnitude();
              auto centerDist = m_geometry.diff(tileEntity->metaBoundBox().center() + tileEntity->position(), pos).magnitude();
              if ((dist < bestDistance) || ((dist == bestDistance) && (centerDist < bestCenterDistance))) {
                interactiveEntity = ie;
                bestDistance = dist;
                bestCenterDistance = centerDist;
              }
            }
          } else {
            auto box = ie->interactiveBoundBox().translated(entity->position());
            auto dist = m_geometry.diffToNearestCoordInBox(box, pos).magnitude();
            auto centerDist = m_geometry.diff(box.center(), pos).magnitude();
            if ((dist < bestDistance) || ((dist == bestDistance) && (centerDist < bestCenterDistance))) {
              interactiveEntity = ie;
              bestDistance = dist;
              bestCenterDistance = centerDist;
            }
          }
        }
      }
      return {};
    });
  if (!result) return std::unexpected(std::move(result.error()));
  if (bestDistance <= maxRadius)
    return interactiveEntity;
  return {};
}

std::expected<bool, StarException> EntityMap::tileIsOccupied(Vec2I const& pos, bool includeEphemeral) const {
  RectF rect(Vec2F(pos[0], pos[1]), Vec2F(pos[0] + 1, pos[1] + 1));
  auto result = findEntity(rect, [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
      if (auto tileEntity = as<TileEntity>(entity)) {
        if (includeEphemeral || !tileEntity->ephemeral()) {
          for (Vec2I space : tileEntity->spaces()) {
            if (m_geometry.equal(pos, space + tileEntity->tilePosition())) {
              return true;
            }
          }
        }
      }
      return false;
    });
  if (!result) return std::unexpected(std::move(result.error()));
  return static_cast<bool>(*result);
}

std::expected<bool, StarException> EntityMap::spaceIsOccupied(RectF const& rect, bool includesEphemeral) const {
  auto entities = entityQuery(rect, {});
  if (!entities) return std::unexpected(std::move(entities.error()));
  for (auto const& entity : *entities) {
    if (!includesEphemeral && entity->ephemeral())
      continue;

    auto rects = m_geometry.splitRect(entity->collisionArea(), entity->position());
    if (!rects) return std::unexpected(std::move(rects.error()));
    for (RectF const& c : *rects) {
      if (!c.isNull() && rect.intersects(c))
        return true;
    }
  }
  return false;
}

}
