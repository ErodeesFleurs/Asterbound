module;
import std;
import star.string;
import star.list;
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

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;

import star.algorithm;
namespace Star {

class TileEntity;
class InteractiveEntity;

struct EntityMapExceptionTag {
  static constexpr std::string_view name() { return "EntityMapException"; }
};
using EntityMapException = StarError<EntityMapExceptionTag, StarException>;

// Class used by WorldServer and WorldClient to store entites organized in a
// spatial hash.  Provides convenient ways of querying entities based on
// different selection criteria.
//
// Several of the methods in EntityMap take callbacks or filters that will be
// called while iterating over internal structures.  They are all designed so
// that adding new entities is safe to do from the callback, but removing
// entities is never safe to do from any callback function.
class EntityMap {
public:
  static float const SpatialHashSectorSize;
  static int const MaximumEntityBoundBox;

  // beginIdSpace and endIdSpace is the *inclusive* range for new enittyIds.
  EntityMap(Vec2U const& worldSize, EntityId beginIdSpace, EntityId endIdSpace);

  // Get the next free id in the entity id space.
  [[nodiscard]] std::expected<EntityId, StarException> reserveEntityId();
  // Or a specific one, can fail.
  [[nodiscard]] std::expected<std::optional<EntityId>, StarException> maybeReserveEntityId(EntityId entityId);
  // If it doesn't matter that we don't get the one want
  [[nodiscard]] std::expected<EntityId, StarException> reserveEntityId(EntityId entityId);

  // Add an entity to this EntityMap.  The entity must already be initialized
  // and have a unique EntityId returned by reserveEntityId.
  [[nodiscard]] std::expected<void, StarException> addEntity(std::shared_ptr<Entity> entity);
  std::shared_ptr<Entity> removeEntity(EntityId entityId);

  std::size_t size() const;
  List<EntityId> entityIds() const;

  // Iterates through the entity map optionally in the given order, updating
  // the spatial information for each entity along the way.
  [[nodiscard]] std::expected<void, StarException> updateAllEntities(EntityCallback const& callback = {}, std::function<bool(std::shared_ptr<Entity> const&, std::shared_ptr<Entity> const&)> sortOrder = {});

  // If the given unique entity is in this map, then return its entity id
  EntityId uniqueEntityId(String const& uniqueId) const;

  std::shared_ptr<Entity> entity(EntityId entityId) const;
  std::shared_ptr<Entity> uniqueEntity(String const& uniqueId) const;

  // Queries entities based on metaBoundBox
  [[nodiscard]] std::expected<List<std::shared_ptr<Entity>>, StarException> entityQuery(RectF const& boundBox, EntityFilter const& filter = {}) const;

  // A fuzzy query of the entities at this position, sorted by closeness.
  [[nodiscard]] std::expected<List<std::shared_ptr<Entity>>, StarException> entitiesAt(Vec2F const& pos, EntityFilter const& filter = {}) const;

  [[nodiscard]] std::expected<List<std::shared_ptr<TileEntity>>, StarException> entitiesAtTile(Vec2I const& pos, EntityFilterOf<TileEntity> const& filter = {}) const;

  // Sort of a fuzzy line intersection test.  Tests if a given line intersects
  // the bounding box of any entities, and returns them.
  [[nodiscard]] std::expected<List<std::shared_ptr<Entity>>, StarException> entityLineQuery(Vec2F const& begin, Vec2F const& end, EntityFilter const& filter = {}) const;

  // Callback versions of query functions.
  [[nodiscard]] std::expected<void, StarException> forEachEntity(RectF const& boundBox, EntityCallback const& callback) const;
  [[nodiscard]] std::expected<void, StarException> forEachEntityLine(Vec2F const& begin, Vec2F const& end, EntityCallback const& callback) const;
  // Returns tile-based entities that occupy the given tile position.
  [[nodiscard]] std::expected<void, StarException> forEachEntityAtTile(Vec2I const& pos, EntityCallbackOf<TileEntity> const& callback) const;

  // Iterate through all the entities, optionally in the given sort order.
  [[nodiscard]] std::expected<void, StarException> forAllEntities(EntityCallback const& callback, std::function<bool(std::shared_ptr<Entity> const&, std::shared_ptr<Entity> const&)> sortOrder = {}) const;

  // Stops searching when filter returns true, and returns the entity which
  // caused it.
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> findEntity(RectF const& boundBox, EntityFilter const& filter) const;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> findEntityLine(Vec2F const& begin, Vec2F const& end, EntityFilter const& filter) const;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> findEntityAtTile(Vec2I const& pos, EntityFilterOf<TileEntity> const& filter) const;

  // Closest entity that satisfies the given selector, if given.
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> closestEntity(Vec2F const& center, float radius, EntityFilter const& filter = {}) const;

  // Returns interactive entity that is near the given world position
  [[nodiscard]] std::expected<std::shared_ptr<InteractiveEntity>, StarException> interactiveEntityNear(Vec2F const& pos, float maxRadius = 1.5f) const;

  // Whether or not any tile entity occupies this tile
  [[nodiscard]] std::expected<bool, StarException> tileIsOccupied(Vec2I const& pos, bool includeEphemeral = false) const;

  // Intersects any entity's collision area
  [[nodiscard]] std::expected<bool, StarException> spaceIsOccupied(RectF const& rect, bool includeEphemeral = false) const;

  // Convenience template methods that filter based on dynamic cast and deal
  // with pointers to a derived entity type.

  template <typename EntityT>
  std::shared_ptr<EntityT> get(EntityId entityId) const;

  template <typename EntityT>
  std::shared_ptr<EntityT> getUnique(String const& uniqueId) const;

  template <typename EntityT>
  [[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> query(RectF const& boundBox, EntityFilterOf<EntityT> const& filter = {}) const;

  template <typename EntityT>
  [[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> all(EntityFilterOf<EntityT> const& filter = {}) const;

  template <typename EntityT>
  [[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> lineQuery(Vec2F const& begin, Vec2F const& end, EntityFilterOf<EntityT> const& filter = {}) const;

  template <typename EntityT>
  [[nodiscard]] std::expected<std::shared_ptr<EntityT>, StarException> closest(Vec2F const& center, float radius, EntityFilterOf<EntityT> const& filter = {}) const;

  template <typename EntityT>
  [[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> atTile(Vec2I const& pos) const;

private:
  using SpatialMap = SpatialHash2D<EntityId, float, std::shared_ptr<Entity>>;

  WorldGeometry m_geometry;

  SpatialMap m_spatialMap;
  BiHashMap<String, EntityId> m_uniqueMap;

  EntityId m_nextId;
  EntityId m_beginIdSpace;
  EntityId m_endIdSpace;

  List<std::reference_wrapper<SpatialMap::Entry const>> m_entrySortBuffer;
};

template <typename EntityT>
std::shared_ptr<EntityT> EntityMap::get(EntityId entityId) const {
  return as<EntityT>(entity(entityId));
}

template <typename EntityT>
std::shared_ptr<EntityT> EntityMap::getUnique(String const& uniqueId) const {
  return as<EntityT>(uniqueEntity(uniqueId));
}

template <typename EntityT>
[[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> EntityMap::query(RectF const& boundBox, EntityFilterOf<EntityT> const& filter) const {
  List<std::shared_ptr<EntityT>> entities;
  auto result = forEachEntity(boundBox, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (auto e = as<EntityT>(entity)) {
      if (filter) {
        auto accepted = filter(e);
        if (!accepted) return std::unexpected(std::move(accepted.error()));
        if (!*accepted) return {};
      }
      entities.append(std::move(e));
    }
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return entities;
}

template <typename EntityT>
[[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> EntityMap::all(EntityFilterOf<EntityT> const& filter) const {
  List<std::shared_ptr<EntityT>> entities;
  auto result = forAllEntities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (auto e = as<EntityT>(entity)) {
      if (filter) {
        auto accepted = filter(e);
        if (!accepted) return std::unexpected(std::move(accepted.error()));
        if (!*accepted) return {};
      }
      entities.append(e);
    }
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return entities;
}

template <typename EntityT>
[[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> EntityMap::lineQuery(Vec2F const& begin, Vec2F const& end, EntityFilterOf<EntityT> const& filter) const {
  auto result = entityLineQuery(begin, end, entityTypeFilter(filter));
  if (!result) return std::unexpected(std::move(result.error()));
  List<std::shared_ptr<EntityT>> entities;
  for (auto const& entity : *result)
    entities.append(as<EntityT>(entity));
  return entities;
}

template <typename EntityT>
[[nodiscard]] std::expected<std::shared_ptr<EntityT>, StarException> EntityMap::closest(Vec2F const& center, float radius, EntityFilterOf<EntityT> const& filter) const {
  auto result = closestEntity(center, radius, entityTypeFilter(filter));
  if (!result) return std::unexpected(std::move(result.error()));
  return as<EntityT>(*result);
}

template <typename EntityT>
[[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> EntityMap::atTile(Vec2I const& pos) const {
  List<std::shared_ptr<EntityT>> list;
  auto result = forEachEntityAtTile(pos, [&](std::shared_ptr<TileEntity> const& entity) -> std::expected<void, StarException> {
    if (auto e = as<EntityT>(entity))
      list.append(std::move(e));
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return list;
}

}

export module star.entity_map;

export namespace Star {
  using ::Star::EntityMapExceptionTag;
  using ::Star::EntityMapException;
  using ::Star::EntityMap;
  using ::Star::TileEntity;
  using ::Star::InteractiveEntity;
}
