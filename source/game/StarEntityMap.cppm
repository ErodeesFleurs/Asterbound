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
import star.tile_entity;

import star.algorithm;
namespace Star {

template <typename T>
concept entity_query_target = std::is_class_v<T> && capability_castable<T, Entity>;
class InteractiveEntity;

struct EntityMapExceptionTag {
  static constexpr std::string_view name() { return "EntityMapException"; }
};
using EntityMapException = StarError<EntityMapExceptionTag, StarException>;

// Class used by WorldServer and WorldClient to store entities organized in a
// spatial hash.  Provides convenient ways of querying entities based on
// different selection criteria.
//
// Several of the methods in EntityMap take callbacks or filters that will be
// called while iterating over internal structures.  They are all designed so
// that adding new entities is safe to do from the callback, but removing
// entities is never safe to do from any callback function.
class EntityMap {
public:
  static int const maximum_entity_bound_box;

  // begin_id_space and end_id_space is the *inclusive* range for new entity IDs.
  EntityMap(Vec2U const& world_size, EntityId begin_id_space, EntityId end_id_space);

  // Get the next free id in the entity id space.
  [[nodiscard]] std::expected<EntityId, StarException> reserve_entity_id();
  // Or a specific one, can fail.
  [[nodiscard]] std::expected<std::optional<EntityId>, StarException> maybe_reserve_entity_id(EntityId entity_id);
  // If it doesn't matter that we don't get the one want
  [[nodiscard]] std::expected<EntityId, StarException> reserve_entity_id(EntityId entity_id);

  // Add an entity to this EntityMap.  The entity must already be initialized
  // and have a unique EntityId returned by reserve_entity_id.
  [[nodiscard]] std::expected<void, StarException> add_entity(std::shared_ptr<Entity> entity);
  std::shared_ptr<Entity> remove_entity(EntityId entity_id);

  std::size_t size() const;
  List<EntityId> entity_ids() const;

  // Iterates through the entity map optionally in the given order, updating
  // the spatial information for each entity along the way.
  [[nodiscard]] std::expected<void, StarException> update_all_entities(EntityCallback const& callback = {}, std::function<bool(std::shared_ptr<Entity> const&, std::shared_ptr<Entity> const&)> sort_order = {});

  // If the given unique entity is in this map, then return its entity id
  EntityId unique_entity_id(String const& unique_id) const;

  std::shared_ptr<Entity> entity(EntityId entity_id) const;
  std::shared_ptr<Entity> unique_entity(String const& unique_id) const;

  // Queries entities based on metaBoundBox
  [[nodiscard]] std::expected<List<std::shared_ptr<Entity>>, StarException> entity_query(RectF const& bound_box, EntityFilter const& filter = {}) const;

  // A fuzzy query of the entities at this position, sorted by closeness.
  [[nodiscard]] std::expected<List<std::shared_ptr<Entity>>, StarException> entities_at(Vec2F const& pos, EntityFilter const& filter = {}) const;

  [[nodiscard]] std::expected<List<std::shared_ptr<TileEntity>>, StarException> entities_at_tile(Vec2I const& pos, EntityFilterOf<TileEntity> const& filter = {}) const;

  // Sort of a fuzzy line intersection test.  Tests if a given line intersects
  // the bounding box of any entities, and returns them.
  [[nodiscard]] std::expected<List<std::shared_ptr<Entity>>, StarException> entity_line_query(Vec2F const& begin, Vec2F const& end, EntityFilter const& filter = {}) const;

  // Callback versions of query functions.
  [[nodiscard]] std::expected<void, StarException> for_each_entity(RectF const& bound_box, EntityCallback const& callback) const;
  [[nodiscard]] std::expected<void, StarException> for_each_entity_line(Vec2F const& begin, Vec2F const& end, EntityCallback const& callback) const;
  // Returns tile-based entities that occupy the given tile position.
  [[nodiscard]] std::expected<void, StarException> for_each_entity_at_tile(Vec2I const& pos, EntityCallbackOf<TileEntity> const& callback) const;

  // Iterate through all the entities, optionally in the given sort order.
  [[nodiscard]] std::expected<void, StarException> for_all_entities(EntityCallback const& callback, std::function<bool(std::shared_ptr<Entity> const&, std::shared_ptr<Entity> const&)> sort_order = {}) const;

  // Stops searching when filter returns true, and returns the entity which
  // caused it.
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> find_entity(RectF const& bound_box, EntityFilter const& filter) const;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> find_entity_line(Vec2F const& begin, Vec2F const& end, EntityFilter const& filter) const;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> find_entity_at_tile(Vec2I const& pos, EntityFilterOf<TileEntity> const& filter) const;

  // Closest entity that satisfies the given selector, if given.
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> closest_entity(Vec2F const& center, float radius, EntityFilter const& filter = {}) const;

  // Returns interactive entity that is near the given world position
  [[nodiscard]] std::expected<std::shared_ptr<InteractiveEntity>, StarException> interactive_entity_near(Vec2F const& pos, float max_radius = 1.5f) const;

  // Whether or not any tile entity occupies this tile
  [[nodiscard]] std::expected<bool, StarException> tile_is_occupied(Vec2I const& pos, bool include_ephemeral = false) const;

  // Intersects any entity's collision area
  [[nodiscard]] std::expected<bool, StarException> space_is_occupied(RectF const& rect, bool include_ephemeral = false) const;

  // Convenience template methods that filter based on dynamic cast and deal
  // with pointers to a derived entity type.

  template <entity_query_target EntityT>
  std::shared_ptr<EntityT> get(EntityId entity_id) const;

  template <entity_query_target EntityT>
  std::shared_ptr<EntityT> get_unique(String const& unique_id) const;

  template <entity_query_target EntityT>
  [[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> query(RectF const& bound_box, EntityFilterOf<EntityT> const& filter = {}) const;

  template <entity_query_target EntityT>
  [[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> all(EntityFilterOf<EntityT> const& filter = {}) const;

  template <entity_query_target EntityT>
  [[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> line_query(Vec2F const& begin, Vec2F const& end, EntityFilterOf<EntityT> const& filter = {}) const;

  template <entity_query_target EntityT>
  [[nodiscard]] std::expected<std::shared_ptr<EntityT>, StarException> closest(Vec2F const& center, float radius, EntityFilterOf<EntityT> const& filter = {}) const;

  template <typename EntityT> requires (std::is_class_v<EntityT> && capability_castable<EntityT, TileEntity>)
  [[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> at_tile(Vec2I const& pos) const;

private:
  using SpatialMap = SpatialHash2D<EntityId, float, std::shared_ptr<Entity>>;
  using IndexedEntry = SpatialMap::EntryMap::value_type;

  WorldGeometry m_geometry;

  SpatialMap m_spatial_map;
  BiHashMap<String, EntityId> m_unique_map;

  EntityId m_next_id;
  EntityId m_begin_id_space;
  EntityId m_end_id_space;

  List<std::reference_wrapper<IndexedEntry const>> m_entry_sort_buffer;
};

template <entity_query_target EntityT>
std::shared_ptr<EntityT> EntityMap::get(EntityId entity_id) const {
  return as<EntityT>(entity(entity_id));
}

template <entity_query_target EntityT>
std::shared_ptr<EntityT> EntityMap::get_unique(String const& unique_id) const {
  return as<EntityT>(unique_entity(unique_id));
}

template <entity_query_target EntityT>
struct TypedEntityCollector {
  List<std::shared_ptr<EntityT>>& entities;
  EntityFilterOf<EntityT> const& filter;

  std::expected<void, StarException> operator()(std::shared_ptr<Entity> const& entity) const {
    if (auto typed = as<EntityT>(entity)) {
      if (filter) {
        auto accepted = filter(typed);
        if (!accepted) return std::unexpected(std::move(accepted.error()));
        if (!*accepted) return {};
      }
      entities.push_back(std::move(typed));
    }
    return {};
  }
};

template <entity_query_target EntityT>
std::expected<List<std::shared_ptr<EntityT>>, StarException> EntityMap::query(RectF const& bound_box, EntityFilterOf<EntityT> const& filter) const {
  List<std::shared_ptr<EntityT>> entities;
  auto result = for_each_entity(bound_box, TypedEntityCollector<EntityT>{entities, filter});
  if (!result) return std::unexpected(std::move(result.error()));
  return entities;
}

template <entity_query_target EntityT>
[[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> EntityMap::all(EntityFilterOf<EntityT> const& filter) const {
  List<std::shared_ptr<EntityT>> entities;
  auto result = for_all_entities(TypedEntityCollector<EntityT>{entities, filter});
  if (!result) return std::unexpected(std::move(result.error()));
  return entities;
}

template <entity_query_target EntityT>
[[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> EntityMap::line_query(Vec2F const& begin, Vec2F const& end, EntityFilterOf<EntityT> const& filter) const {
  List<std::shared_ptr<EntityT>> entities;
  auto result = for_each_entity_line(begin, end, TypedEntityCollector<EntityT>{entities, filter});
  if (!result) return std::unexpected(std::move(result.error()));
  return entities;
}

template <entity_query_target EntityT>
[[nodiscard]] std::expected<std::shared_ptr<EntityT>, StarException> EntityMap::closest(Vec2F const& center, float radius, EntityFilterOf<EntityT> const& filter) const {
  auto result = closest_entity(center, radius, entityTypeFilter(filter));
  if (!result) return std::unexpected(std::move(result.error()));
  return as<EntityT>(*result);
}

template <typename EntityT> requires (std::is_class_v<EntityT> && capability_castable<EntityT, TileEntity>)
[[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> EntityMap::at_tile(Vec2I const& pos) const {
  List<std::shared_ptr<EntityT>> list;
  auto result = for_each_entity_at_tile(pos, [&](std::shared_ptr<TileEntity> const& entity) -> std::expected<void, StarException> {
    if (auto e = as<EntityT>(entity))
      list.push_back(std::move(e));
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
