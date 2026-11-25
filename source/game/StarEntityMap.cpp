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
  auto boxes = closed_query_bounds(geometry, entity.meta_bound_box().translated(entity.position()));
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

RectF tile_query_bounds(WorldGeometry const& geometry, Vec2I const& pos) {
  std::int64_t x = pos[0];
  std::int64_t width = geometry.width();
  if (width && (x < 0 || x >= width)) {
    x %= width;
    if (x < 0) x += width;
  }
  std::int64_t y = pos[1];
  return RectF(float(x), float(y), float(x + 1), float(y + 1));
}

template <typename Visit>
std::expected<bool, StarException> visit_tile_spaces(WorldGeometry const& geometry, Vec2I const& pos,
    TileEntity& tile, Visit&& visit) {
  auto spaces = tile.spaces();
  auto origin = tile.tilePosition();
  std::int64_t width = geometry.width();
  for (std::size_t i = 0; i < spaces.size(); ++i) {
    auto const& space = spaces[i];
    if (std::int64_t(origin[1]) + space[1] != pos[1]) continue;
    auto dx = std::int64_t(origin[0]) + space[0] - pos[0];
    if (dx != 0 && (!width || dx % width != 0)) continue;
    auto stop = visit();
    if (!stop || *stop) return stop;
    // The callback may move this entity; only the spaces list is a snapshot.
    if (i + 1 < spaces.size()) origin = tile.tilePosition();
  }
  return false;
}

}

int const EntityMap::maximum_entity_bound_box = 10000;

EntityMap::EntityMap(Vec2U const& world_size, EntityId begin_id_space, EntityId end_id_space)
  : m_geometry(world_size),
    m_spatial_map(std::integral_constant<int, 16>{}),
    m_next_id(begin_id_space),
    m_begin_id_space(begin_id_space),
    m_end_id_space(end_id_space) {}

std::expected<EntityId, StarException> EntityMap::reserve_entity_id() {
  for (auto id = m_next_id; id <= m_end_id_space;) {
    auto next_id = cycle_increment(id, m_begin_id_space, m_end_id_space);
    if (id != null_entity_id && !m_spatial_map.contains(id)) {
      m_next_id = next_id;
      return id;
    }
    id = next_id;
    if (id == m_next_id)
      break;
  }
  return std::unexpected(EntityMapException("No more entity id space in EntityMap::reserve_entity_id"));
}

std::expected<std::optional<EntityId>, StarException> EntityMap::maybe_reserve_entity_id(EntityId entity_id) {
  if (entity_id == null_entity_id || m_spatial_map.contains(entity_id))
    return std::optional<EntityId>{};
  else
    return entity_id;
}

std::expected<EntityId, StarException> EntityMap::reserve_entity_id(EntityId entity_id) {
  if (entity_id == null_entity_id)
    return reserve_entity_id();
  auto reserved = maybe_reserve_entity_id(entity_id);
  if (!reserved) return std::unexpected(std::move(reserved.error()));
  if (*reserved)
    return **reserved;

  m_next_id = cycle_increment(entity_id, m_begin_id_space, m_end_id_space);
  return reserve_entity_id();
}


std::expected<void, StarException> EntityMap::add_entity(std::shared_ptr<Entity> entity) {
  auto position = entity->position();
  auto bound_box = entity->meta_bound_box();
  auto entity_id = entity->entityId();
  auto unique_id = entity->uniqueId();

  if (m_spatial_map.contains(entity_id))
    return std::unexpected(EntityMapException::format("Duplicate entity id '{}' in EntityMap::add_entity", entity_id));

  if (bound_box.is_negative() || bound_box.width() > maximum_entity_bound_box || bound_box.height() > maximum_entity_bound_box) {
    return std::unexpected(EntityMapException::format("Entity id: {} type: {} bound box is negative or beyond the "
                                                      "maximum entity bound box size in EntityMap::add_entity",
                                                      entity->entityId(), static_cast<int>(entity->entity_type())));
  }

  if (entity_id == null_entity_id)
    return std::unexpected(EntityMapException::format("Null entity id in EntityMap::add_entity"));

  if (unique_id && m_unique_map.contains_left(*unique_id))
    return std::unexpected(EntityMapException::format("Duplicate entity unique id ({}) on entity id ({}) in EntityMap::add_entity", *unique_id, entity_id));

  auto rects = split_entity_bounds(m_geometry, bound_box.translated(position));
  if (!rects) return std::unexpected(std::move(rects.error()));
  auto indexed = m_spatial_map.set(entity_id, *rects, std::move(entity));
  if (!indexed) return std::unexpected(std::move(indexed.error()));
  // Both identities were checked above. No callbacks or fallible operations
  // intervene between the two publications.
  if (unique_id)
    m_unique_map.overwrite(std::move(*unique_id), entity_id);
  return {};
}

std::shared_ptr<Entity> EntityMap::remove_entity(EntityId entity_id) {
  if (auto entity = m_spatial_map.remove(entity_id)) {
    m_unique_map.erase_right(entity_id);
    return std::move(*entity);
  }
  return {};
}

std::size_t EntityMap::size() const {
  return m_spatial_map.size();
}

List<EntityId> EntityMap::entity_ids() const {
  return m_spatial_map.keys();
}

std::expected<void, StarException> EntityMap::update_all_entities(EntityCallback const& callback, std::function<bool(std::shared_ptr<Entity> const&, std::shared_ptr<Entity> const&)> sort_order) {
  auto update_entity_info = [&](IndexedEntry const& indexed) -> std::expected<void, StarException> {
    auto const& [entity_id, entry] = indexed;
    auto const& entity = entry.value;

    auto position = entity->position();
    auto bound_box = entity->meta_bound_box();

    if (bound_box.is_negative() || bound_box.width() > maximum_entity_bound_box || bound_box.height() > maximum_entity_bound_box) {
      return std::unexpected(EntityMapException::format("Entity id: {} type: {} bound box is negative or beyond the "
                                                        "maximum entity bound box size during refresh",
                                                        entity->entityId(), static_cast<int>(entity->entity_type())));
    }

    if (entity->entityId() != entity_id)
      return std::unexpected(EntityMapException::format("Entity id {} changed to {} while indexed", entity_id, entity->entityId()));

    auto rects = split_entity_bounds(m_geometry, bound_box.translated(position));
    if (!rects) return std::unexpected(std::move(rects.error()));
    auto unique_id = entity->uniqueId();
    auto existing = unique_id ? m_unique_map.maybe_right(*unique_id) : std::nullopt;
    if (existing && *existing != entity_id)
      return std::unexpected(EntityMapException::format("Duplicate entity unique id on entity ids ({}) and ({})", *existing, entity_id));

    if (!std::ranges::equal(*rects, entry.rects)) {
      auto refreshed = m_spatial_map.set(entity_id, *rects);
      if (!refreshed) return std::unexpected(std::move(refreshed.error()));
    }
    if (!unique_id)
      m_unique_map.erase_right(entity_id);
    else if (!existing)
      m_unique_map.overwrite(std::move(*unique_id), entity_id);
    return {};
  };

  // Detach the reusable snapshot while callbacks run: even a nested update
  // must not reorder or reallocate the outer traversal's borrowed entries.
  auto entries = std::move(m_entry_sort_buffer);
  auto recycle = make_scope_exit([&] {
    entries.clear();
    m_entry_sort_buffer = std::move(entries);
  });
  entries.reserve(m_spatial_map.size());
  for (auto const& entry : m_spatial_map.entries())
    entries.push_back(std::cref(entry));

  if (sort_order) {
    entries.sort([&sort_order](IndexedEntry const& a, IndexedEntry const& b) {
      return sort_order(a.second.value, b.second.value);
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

EntityId EntityMap::unique_entity_id(String const& unique_id) const {
  return m_unique_map.maybe_right(unique_id).value_or(null_entity_id);
}

std::shared_ptr<Entity> EntityMap::entity(EntityId entity_id) const {
  auto entity = m_spatial_map.value(entity_id);
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((!entity || entity->entityId() == entity_id));
  }
  return entity;
}

std::shared_ptr<Entity> EntityMap::unique_entity(String const& unique_id) const {
  return entity(unique_entity_id(unique_id));
}

std::expected<List<std::shared_ptr<Entity>>, StarException> EntityMap::entity_query(RectF const& bound_box, EntityFilter const& filter) const {
  List<std::shared_ptr<Entity>> values;
  auto result = for_each_entity(bound_box, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
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

std::expected<List<std::shared_ptr<Entity>>, StarException> EntityMap::entities_at(Vec2F const& pos, EntityFilter const& filter) const {
  auto entity_list = entity_query(RectF::with_center(pos, {0, 0}), filter);
  if (!entity_list) return std::unexpected(std::move(entity_list.error()));
  sort_by_key(*entity_list, [&](std::shared_ptr<Entity> const& entity) -> float {
    return m_geometry.diff(entity->position(), pos).magnitude_squared();
  });
  return std::move(*entity_list);
}

std::expected<List<std::shared_ptr<TileEntity>>, StarException> EntityMap::entities_at_tile(Vec2I const& pos, EntityFilterOf<TileEntity> const& filter) const {
  List<std::shared_ptr<TileEntity>> values;
  auto result = for_each_entity_at_tile(pos, [&](std::shared_ptr<TileEntity> const& entity) -> std::expected<void, StarException> {
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

std::expected<void, StarException> EntityMap::for_each_entity(RectF const& bound_box, EntityCallback const& callback) const {
  if (!callback)
    return std::unexpected(EntityMapException("Missing EntityMap::for_each_entity callback"));
  auto rects = closed_query_bounds(m_geometry, bound_box);
  if (!rects) return std::unexpected(std::move(rects.error()));
  return m_spatial_map.for_each(*rects, callback);
}

std::expected<void, StarException> EntityMap::for_each_entity_line(Vec2F const& begin, Vec2F const& end, EntityCallback const& callback) const {
  if (!callback)
    return std::unexpected(EntityMapException("Missing EntityMap::for_each_entity_line callback"));
  auto lines = split_query_line(m_geometry, begin, end);
  if (!lines) return std::unexpected(std::move(lines.error()));
  return for_each_entity(RectF::bound_box_of(begin, end), [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    auto intersects = line_hits_entity(m_geometry, *lines, *entity);
    if (!intersects) return std::unexpected(std::move(intersects.error()));
    if (*intersects)
      return callback(entity);
    return {};
  });
}

std::expected<void, StarException> EntityMap::for_each_entity_at_tile(Vec2I const& pos, EntityCallbackOf<TileEntity> const& callback) const {
  if (!callback)
    return std::unexpected(EntityMapException("Missing EntityMap::for_each_entity_at_tile callback"));
  return for_each_entity(tile_query_bounds(m_geometry, pos), [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (auto tile = as_ref<TileEntity>(*entity)) {
      std::shared_ptr<TileEntity> owner;
      auto visited = visit_tile_spaces(m_geometry, pos, tile->get(), [&]() -> std::expected<bool, StarException> {
        if (!owner) owner = std::shared_ptr<TileEntity>(entity, std::addressof(tile->get()));
        auto result = callback(owner);
        if (!result) return std::unexpected(std::move(result.error()));
        return false;
      });
      if (!visited) return std::unexpected(std::move(visited.error()));
    }
    return {};
  });
}

std::expected<void, StarException> EntityMap::for_all_entities(EntityCallback const& callback, std::function<bool(std::shared_ptr<Entity> const&, std::shared_ptr<Entity> const&)> sort_order) const {
  if (!callback)
    return std::unexpected(EntityMapException("Missing EntityMap::for_all_entities callback"));
  // Snapshot owner references so adding entities from a callback is safe.
  // Removing entities during callbacks remains unsupported.
  List<std::reference_wrapper<std::shared_ptr<Entity> const>> all_entities;
  all_entities.reserve(m_spatial_map.size());
  for (auto const& entry : m_spatial_map.entries())
    all_entities.push_back(std::cref(entry.second.value));

  if (sort_order) {
    all_entities.sort([&sort_order](auto a, auto b) {
        return sort_order(a.get(), b.get());
      });
  }

  for (auto borrowed : all_entities) {
    auto const& entity = borrowed.get();
    auto result = callback(entity);
    if (!result) {
      if (!EntityTypeNames) return std::unexpected(result.error().with_related(EntityTypeNames.error(), "Naming the failing entity type"));
      auto type = EntityTypeNames->get_right(entity->entity_type());
      if (!type) return std::unexpected(result.error().with_related(type.error(), "Naming the failing entity type"));
      auto name = entity->name();
      if (!name) return std::unexpected(result.error().with_related(name.error(), "Naming the failing entity"));
      auto logged = entity->root().logger().error("[EntityMap] Exception caught running for_all_entities callback for {} entity {} (named \"{}\")",
          type->get(), entity->entityId(), *name);
      if (!logged) return std::unexpected(result.error().with_related(logged.error(), "Logging the entity callback failure"));
      return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<std::shared_ptr<Entity>, StarException> EntityMap::find_entity(RectF const& bound_box, EntityFilter const& filter) const {
  if (!filter)
    return std::unexpected(EntityMapException("Missing EntityMap::find_entity filter"));
  auto bounds = closed_query_bounds(m_geometry, bound_box);
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  std::shared_ptr<Entity> found;
  auto result = m_spatial_map.any_of(*bounds, [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
    auto accepted = filter(entity);
    if (accepted && *accepted) found = entity;
    return accepted;
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return found;
}

std::expected<std::shared_ptr<Entity>, StarException> EntityMap::find_entity_line(Vec2F const& begin, Vec2F const& end, EntityFilter const& filter) const {
  if (!filter)
    return std::unexpected(EntityMapException("Missing EntityMap::find_entity_line filter"));
  auto lines = split_query_line(m_geometry, begin, end);
  if (!lines) return std::unexpected(std::move(lines.error()));
  return find_entity(RectF::bound_box_of(begin, end), [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
    auto intersects = line_hits_entity(m_geometry, *lines, *entity);
    if (!intersects) return std::unexpected(std::move(intersects.error()));
    if (*intersects) return filter(entity);
    return false;
  });
}

std::expected<std::shared_ptr<Entity>, StarException> EntityMap::find_entity_at_tile(Vec2I const& pos, EntityFilterOf<TileEntity> const& filter) const {
  if (!filter)
    return std::unexpected(EntityMapException("Missing EntityMap::find_entity_at_tile filter"));
  return find_entity(tile_query_bounds(m_geometry, pos), [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
    if (auto tile = as_ref<TileEntity>(*entity)) {
      std::shared_ptr<TileEntity> owner;
      return visit_tile_spaces(m_geometry, pos, tile->get(), [&] {
        if (!owner) owner = std::shared_ptr<TileEntity>(entity, std::addressof(tile->get()));
        return filter(owner);
      });
    }
    return false;
  });
}

std::expected<List<std::shared_ptr<Entity>>, StarException> EntityMap::entity_line_query(Vec2F const& begin, Vec2F const& end, EntityFilter const& filter) const {
  List<std::shared_ptr<Entity>> values;
  auto result = for_each_entity_line(begin, end, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
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

std::expected<std::shared_ptr<Entity>, StarException> EntityMap::closest_entity(Vec2F const& center, float radius, EntityFilter const& filter) const {
  std::shared_ptr<Entity> closest;
  float dist_squared = square(radius);
  RectF bound_box(center[0] - radius, center[1] - radius, center[0] + radius, center[1] + radius);
  auto result = for_each_entity(bound_box, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    Vec2F pos = entity->position();
    float this_dist_squared = m_geometry.diff(center, pos).magnitude_squared();
    if (dist_squared > this_dist_squared) {
      if (filter) {
        auto accepted = filter(entity);
        if (!accepted) return std::unexpected(std::move(accepted.error()));
        if (!*accepted) return {};
      }
      dist_squared = this_dist_squared;
      closest = entity;
    }
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return closest;
}
std::expected<std::shared_ptr<InteractiveEntity>, StarException> EntityMap::interactive_entity_near(Vec2F const& pos, float max_radius) const {
  auto radius = Vec2F::filled(max_radius);
  RectF rect(pos - radius, pos + radius);
  std::shared_ptr<InteractiveEntity> interactive_entity;
  double best_distance = max_radius + 100;
  double best_center_distance = max_radius + 100;
  auto result = for_each_entity(rect, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (auto interactive = as_ref<InteractiveEntity>(*entity)) {
      auto& ie = interactive->get();
      if (ie.isInteractive()) {
        if (auto tile = as_ref<TileEntity>(*entity)) {
          auto& tile_entity = tile->get();
          auto spaces = tile_entity.interactiveSpaces();
          if (spaces.empty())
            return {};
          auto tile_position = tile_entity.tilePosition();
          auto center_distance = m_geometry.diff(tile_entity.meta_bound_box().center() + tile_entity.position(), pos).magnitude();
          for (auto const& space : spaces) {
            auto dist = m_geometry.diff(pos, center_of_tile(space + tile_position)).magnitude();
            if ((dist < best_distance) || ((dist == best_distance) && (center_distance < best_center_distance))) {
              interactive_entity = std::shared_ptr<InteractiveEntity>(entity, std::addressof(ie));
              best_distance = dist;
              best_center_distance = center_distance;
            }
          }
        } else {
          auto box = ie.interactiveBoundBox().translated(entity->position());
          auto dist = m_geometry.diff_to_nearest_coord_in_box(box, pos).magnitude();
          auto center_dist = m_geometry.diff(box.center(), pos).magnitude();
          if ((dist < best_distance) || ((dist == best_distance) && (center_dist < best_center_distance))) {
            interactive_entity = std::shared_ptr<InteractiveEntity>(entity, std::addressof(ie));
            best_distance = dist;
            best_center_distance = center_dist;
          }
        }
      }
    }
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  if (best_distance <= max_radius)
    return interactive_entity;
  return {};
}

std::expected<bool, StarException> EntityMap::tile_is_occupied(Vec2I const& pos, bool include_ephemeral) const {
  auto result = find_entity(tile_query_bounds(m_geometry, pos), [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
    if (auto tile = as_ref<TileEntity>(*entity); tile && (include_ephemeral || !tile->get().ephemeral()))
      return visit_tile_spaces(m_geometry, pos, tile->get(), []() -> std::expected<bool, StarException> { return true; });
    return false;
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return static_cast<bool>(*result);
}

std::expected<bool, StarException> EntityMap::space_is_occupied(RectF const& rect, bool include_ephemeral) const {
  auto bounds = closed_query_bounds(m_geometry, rect);
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  return m_spatial_map.any_of(*bounds, [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
    if (!include_ephemeral && entity->ephemeral())
      return false;
    auto boxes = split_entity_bounds(m_geometry, entity->collision_area().translated(entity->position()));
    if (!boxes) return std::unexpected(std::move(boxes.error()));
    for (auto const& box : *boxes)
      for (auto const& query : *bounds)
        if (!box.is_null() && query.intersects(box))
          return true;
    return false;
  });
}

}
