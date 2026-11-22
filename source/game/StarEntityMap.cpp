import std;
import star.optional;
import star.string;
import star.logging;
import star.format;
import star.json;
import star.id_map;
import star.rect;
import star.line;
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

namespace {

std::expected<StaticList<RectF, 2>, StarException> split_entity_bounds(WorldGeometry const& geometry, RectF const& rect) {
  if (rect.is_null())
    return StaticList<RectF, 2>::create({rect});
  auto finite = classify_finite(rect.x_min(), rect.x_max(), rect.y_min(), rect.y_max());
  if (!std::ranges::all_of(finite, std::identity{}) || rect.is_negative())
    return std::unexpected(EntityMapException("Invalid spatial bounds"));

  auto width = static_cast<float>(geometry.width());
  if (width != 0 && rect.width() >= width)
    return StaticList<RectF, 2>::create({RectF(0, rect.y_min(), width, rect.y_max())});
  return geometry.split_rect(rect);
}

// A closed rectangle ending at the world width also contains x=0 (and
// conversely). Mirror the edge on queries, not on every indexed entity.
std::expected<StaticList<RectF, 2>, StarException> closed_query_bounds(WorldGeometry const& geometry, RectF const& rect) {
  auto pieces = split_entity_bounds(geometry, rect);
  if (!pieces) return std::unexpected(std::move(pieces.error()));
  auto width = static_cast<float>(geometry.width());
  if (width != 0 && pieces->size() == 1) {
    auto const& piece = pieces->front();
    if ((piece.x_min() == 0) != (piece.x_max() == width)) {
      float other_edge = piece.x_min() == 0 ? width : 0;
      auto appended = pieces->emplace_back(other_edge, piece.y_min(), other_edge, piece.y_max());
      if (!appended) return std::unexpected(std::move(appended.error()));
    }
  }
  return pieces;
}

std::expected<StaticList<Line2F, 2>, StarException> split_query_line(WorldGeometry const& geometry,
    Vec2F const& begin, Vec2F const& end) {
  auto finite = classify_finite(begin[0], begin[1], end[0], end[1]);
  if (!std::ranges::all_of(finite, std::identity{}))
    return std::unexpected(EntityMapException("Invalid spatial line"));
  return geometry.split_line({begin, end}, false);
}

std::expected<bool, StarException> line_hits_entity(WorldGeometry const& geometry,
    StaticList<Line2F, 2> const& lines, Entity const& entity) {
  // Mirror the entity instead of the line: a diagonal ending on the seam
  // only touches the opposite side at its endpoint, not along a second ray.
  auto boxes = closed_query_bounds(geometry, entity.metaBoundBox().translated(entity.position()));
  if (!boxes) return std::unexpected(std::move(boxes.error()));
  for (auto const& box : *boxes) {
    if (box.is_null()) continue;
    for (auto const& line : lines) {
      if (box.is_empty()) {
        // A point/segment rectangle has no polygon interior. Test its actual
        // endpoints against the line rather than its coincident box edges.
        if (!box.intersects(RectF::bound_box_of(line.min(), line.max()))) continue;
        Vec2D origin(line.min());
        auto direction = Vec2D(line.max()) - origin;
        double a = direction ^ (Vec2D(box.min()) - origin);
        double b = direction ^ (Vec2D(box.max()) - origin);
        if ((a <= 0 && b >= 0) || (a >= 0 && b <= 0)) return true;
      } else if (box.intersects(line)) {
        return true;
      }
    }
  }
  return false;
}

}

int const EntityMap::MaximumEntityBoundBox = 10000;

EntityMap::EntityMap(Vec2U const& worldSize, EntityId beginIdSpace, EntityId endIdSpace)
  : m_geometry(worldSize),
    m_spatialMap(std::integral_constant<int, 16>{}),
    m_nextId(beginIdSpace),
    m_beginIdSpace(beginIdSpace),
    m_endIdSpace(endIdSpace) {}

std::expected<EntityId, StarException> EntityMap::reserveEntityId() {
  if (m_spatialMap.size() >= static_cast<std::size_t>(m_endIdSpace - m_beginIdSpace))
    return std::unexpected(EntityMapException("No more entity id space in EntityMap::reserveEntityId"));

  EntityId id = m_nextId;
  while (m_spatialMap.contains(id))
    id = cycle_increment(id, m_beginIdSpace, m_endIdSpace);
  m_nextId = cycle_increment(id, m_beginIdSpace, m_endIdSpace);

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

  if (boundBox.is_negative() || boundBox.width() > MaximumEntityBoundBox || boundBox.height() > MaximumEntityBoundBox) {
    return std::unexpected(EntityMapException::format("Entity id: {} type: {} bound box is negative or beyond the "
                                                      "maximum entity bound box size in EntityMap::addEntity",
                                                      entity->entityId(), static_cast<int>(entity->entityType())));
  }

  if (entityId == NullEntityId)
    return std::unexpected(EntityMapException::format("Null entity id in EntityMap::addEntity"));

  if (uniqueId && m_uniqueMap.contains_left(*uniqueId))
    return std::unexpected(EntityMapException::format("Duplicate entity unique id ({}) on entity id ({}) in EntityMap::addEntity", *uniqueId, entityId));

  auto rects = split_entity_bounds(m_geometry, boundBox.translated(position));
  if (!rects) return std::unexpected(std::move(rects.error()));
  auto indexed = m_spatialMap.set(entityId, *rects, std::move(entity));
  if (!indexed) return std::unexpected(std::move(indexed.error()));
  // Both identities were checked above. No callbacks or fallible operations
  // intervene between the two publications.
  if (uniqueId)
    m_uniqueMap.overwrite(std::move(*uniqueId), entityId);
  return {};
}

std::shared_ptr<Entity> EntityMap::removeEntity(EntityId entityId) {
  if (auto entity = m_spatialMap.remove(entityId)) {
    m_uniqueMap.erase_right(entityId);
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
  auto update_entity_info = [&](IndexedEntry const& indexed) -> std::expected<void, StarException> {
    auto const& [entity_id, entry] = indexed;
    auto const& entity = entry.value;

    auto position = entity->position();
    auto bound_box = entity->metaBoundBox();

    if (bound_box.is_negative() || bound_box.width() > MaximumEntityBoundBox || bound_box.height() > MaximumEntityBoundBox) {
      return std::unexpected(EntityMapException::format("Entity id: {} type: {} bound box is negative or beyond the "
                                                        "maximum entity bound box size during refresh",
                                                        entity->entityId(), static_cast<int>(entity->entityType())));
    }

    if (entity->entityId() != entity_id)
      return std::unexpected(EntityMapException::format("Entity id {} changed to {} while indexed", entity_id, entity->entityId()));

    auto rects = split_entity_bounds(m_geometry, bound_box.translated(position));
    if (!rects) return std::unexpected(std::move(rects.error()));
    auto unique_id = entity->uniqueId();
    auto existing = unique_id ? m_uniqueMap.maybe_right(*unique_id) : std::nullopt;
    if (existing && *existing != entity_id)
      return std::unexpected(EntityMapException::format("Duplicate entity unique id on entity ids ({}) and ({})", *existing, entity_id));

    if (!std::ranges::equal(*rects, entry.rects)) {
      auto refreshed = m_spatialMap.set(entity_id, *rects);
      if (!refreshed) return std::unexpected(std::move(refreshed.error()));
    }
    if (!unique_id)
      m_uniqueMap.erase_right(entity_id);
    else if (!existing)
      m_uniqueMap.overwrite(std::move(*unique_id), entity_id);
    return {};
  };

  // Detach the reusable snapshot while callbacks run: even a nested update
  // must not reorder or reallocate the outer traversal's borrowed entries.
  auto entries = std::move(m_entry_sort_buffer);
  auto recycle = make_scope_exit([&] {
    entries.clear();
    m_entry_sort_buffer = std::move(entries);
  });
  entries.reserve(m_spatialMap.size());
  for (auto const& entry : m_spatialMap.entries())
    entries.push_back(std::cref(entry));

  if (sortOrder) {
    entries.sort([&sortOrder](IndexedEntry const& a, IndexedEntry const& b) {
      return sortOrder(a.second.value, b.second.value);
    });
  }

  for (auto entry : entries) {
    if (callback) {
      auto result = callback(entry.get().second.value);
      if (!result) return std::unexpected(std::move(result.error()));
    }
    auto result = update_entity_info(entry.get());
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

EntityId EntityMap::uniqueEntityId(String const& uniqueId) const {
  return m_uniqueMap.maybe_right(uniqueId).value_or(NullEntityId);
}

std::shared_ptr<Entity> EntityMap::entity(EntityId entityId) const {
  auto entity = m_spatialMap.value(entityId);
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((!entity || entity->entityId() == entityId));
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
    values.push_back(entity);
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return values;
}

std::expected<List<std::shared_ptr<Entity>>, StarException> EntityMap::entitiesAt(Vec2F const& pos, EntityFilter const& filter) const {
  auto entityList = entityQuery(RectF::with_center(pos, {0, 0}), filter);
  if (!entityList) return std::unexpected(std::move(entityList.error()));
  sort_by_key(*entityList, [&](std::shared_ptr<Entity> const& entity) -> float {
    return vmag_squared(entity->position() - pos);
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
    values.push_back(entity);
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return values;
}

std::expected<void, StarException> EntityMap::forEachEntity(RectF const& boundBox, EntityCallback const& callback) const {
  if (!callback)
    return std::unexpected(EntityMapException("Missing EntityMap::forEachEntity callback"));
  auto rects = closed_query_bounds(m_geometry, boundBox);
  if (!rects) return std::unexpected(std::move(rects.error()));
  return m_spatialMap.for_each(*rects, callback);
}

std::expected<void, StarException> EntityMap::forEachEntityLine(Vec2F const& begin, Vec2F const& end, EntityCallback const& callback) const {
  if (!callback)
    return std::unexpected(EntityMapException("Missing EntityMap::forEachEntityLine callback"));
  auto lines = split_query_line(m_geometry, begin, end);
  if (!lines) return std::unexpected(std::move(lines.error()));
  return forEachEntity(RectF::bound_box_of(begin, end), [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    auto intersects = line_hits_entity(m_geometry, *lines, *entity);
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
    if (auto tile = as_ref<TileEntity>(*entity)) {
      auto& tileEntity = tile->get();
      std::shared_ptr<TileEntity> tileOwner;
      for (Vec2I space : tileEntity.spaces()) {
        if (m_geometry.equal(pos, space + tileEntity.tilePosition())) {
          if (!tileOwner)
            tileOwner = std::shared_ptr<TileEntity>(entity, std::addressof(tileEntity));
          auto result = callback(tileOwner);
          if (!result)
            return std::unexpected(std::move(result.error()));
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
    allEntities.push_back(std::cref(entry.second.value));

  if (sortOrder) {
    allEntities.sort([&sortOrder](auto a, auto b) {
        return sortOrder(a.get(), b.get());
      });
  }

  for (auto borrowed : allEntities) {
    auto const& entity = borrowed.get();
    auto result = callback(entity);
    if (!result) {
      if (!EntityTypeNames) return std::unexpected(result.error().with_related(EntityTypeNames.error(), "Naming the failing entity type"));
      auto type = EntityTypeNames->get_right(entity->entityType());
      if (!type) return std::unexpected(result.error().with_related(type.error(), "Naming the failing entity type"));
      auto name = entity->name();
      if (!name) return std::unexpected(result.error().with_related(name.error(), "Naming the failing entity"));
      auto logged = entity->root().logger().error("[EntityMap] Exception caught running forAllEntities callback for {} entity {} (named \"{}\")",
          type->get(), entity->entityId(), *name);
      if (!logged) return std::unexpected(result.error().with_related(logged.error(), "Logging the entity callback failure"));
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
  auto lines = split_query_line(m_geometry, begin, end);
  if (!lines) return std::unexpected(std::move(lines.error()));
  return findEntity(RectF::bound_box_of(begin, end), [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
    auto intersects = line_hits_entity(m_geometry, *lines, *entity);
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
    if (auto tile = as_ref<TileEntity>(*entity)) {
      auto& tileEntity = tile->get();
      std::shared_ptr<TileEntity> tileOwner;
      for (Vec2I space : tileEntity.spaces()) {
        if (m_geometry.equal(pos, space + tileEntity.tilePosition())) {
          if (!tileOwner)
            tileOwner = std::shared_ptr<TileEntity>(entity, std::addressof(tileEntity));
          auto accepted = filter(tileOwner);
          if (!accepted)
            return std::unexpected(std::move(accepted.error()));
          if (*accepted)
            return true;
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
    values.push_back(entity);
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
    float thisDistSquared = m_geometry.diff(center, pos).magnitude_squared();
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
  auto rect = RectF::with_center(pos, Vec2F::filled(maxRadius));
  std::shared_ptr<InteractiveEntity> interactiveEntity;
  double bestDistance = maxRadius + 100;
  double bestCenterDistance = maxRadius + 100;
  auto result = forEachEntity(rect, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (auto interactive = as_ref<InteractiveEntity>(*entity)) {
      auto& ie = interactive->get();
      if (ie.isInteractive()) {
        if (auto tile = as_ref<TileEntity>(*entity)) {
          auto& tileEntity = tile->get();
          for (Vec2I space : tileEntity.interactiveSpaces()) {
            auto dist = m_geometry.diff(pos, centerOfTile(space + tileEntity.tilePosition())).magnitude();
            auto centerDist =
              m_geometry.diff(tileEntity.metaBoundBox().center() + tileEntity.position(), pos).magnitude();
            if ((dist < bestDistance) || ((dist == bestDistance) && (centerDist < bestCenterDistance))) {
              interactiveEntity = std::shared_ptr<InteractiveEntity>(entity, std::addressof(ie));
              bestDistance = dist;
              bestCenterDistance = centerDist;
            }
          }
        } else {
          auto box = ie.interactiveBoundBox().translated(entity->position());
          auto dist = m_geometry.diff_to_nearest_coord_in_box(box, pos).magnitude();
          auto centerDist = m_geometry.diff(box.center(), pos).magnitude();
          if ((dist < bestDistance) || ((dist == bestDistance) && (centerDist < bestCenterDistance))) {
            interactiveEntity = std::shared_ptr<InteractiveEntity>(entity, std::addressof(ie));
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
    if (auto tile = as_ref<TileEntity>(*entity)) {
      auto& tileEntity = tile->get();
      if (includeEphemeral || !tileEntity.ephemeral()) {
        for (Vec2I space : tileEntity.spaces()) {
          if (m_geometry.equal(pos, space + tileEntity.tilePosition())) {
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
  auto bounds = closed_query_bounds(m_geometry, rect);
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  bool occupied = false;
  auto scanned = m_spatialMap.for_each(*bounds, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (occupied || (!includesEphemeral && entity->ephemeral()))
      return {};
    auto boxes = split_entity_bounds(m_geometry, entity->collisionArea().translated(entity->position()));
    if (!boxes) return std::unexpected(std::move(boxes.error()));
    for (auto const& box : *boxes)
      for (auto const& query : *bounds)
        if (!box.is_null() && query.intersects(box)) {
          occupied = true;
          return {};
        }
    return {};
  });
  if (!scanned) return std::unexpected(std::move(scanned.error()));
  return occupied;
}

}
