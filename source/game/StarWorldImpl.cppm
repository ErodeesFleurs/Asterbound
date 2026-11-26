module;
import star.format;
import star.json;
import star.xx_hash;
import star.json_extra;
import star.id_map;
import star.rect;
import star.map;
import star.set;
import star.spatial_hash_2d;
import star.casting;
import star.list;
import star.bi_map;
import star.game_types;
import star.material_types;
import star.math_common;
import star.vector;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import star.image;
import star.color;
import star.thread;
import star.data_stream;
import std;
import star.worker_pool;

import star.sector_array_2d;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.lru_cache;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.blocks_along_line;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.spawn_type_database;
import star.biome;
import star.celestial_types;
import star.sky_parameters;
import star.world_template;
import star.cellular_light_array;
import star.cellular_lighting;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;
import star.tile_modification;
import star.liquids_database;
import star.material_database;
import star.sky;
import star.entity_map;
import star.world;

import star.line;
namespace Star {

namespace WorldImpl {
  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> tileIsOccupied(std::shared_ptr<TileSectorArray> const& tileSectorArray, std::shared_ptr<EntityMap> const& entityMap,
      Vec2I const& pos, TileLayer layer, bool includeEphemeral, bool checkCollision = false);

  template <typename TileSectorArray>
  CollisionKind tileCollisionKind(std::shared_ptr<TileSectorArray> const& tileSectorArray, std::shared_ptr<EntityMap> const& entityMap,
    Vec2I const& pos);

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> rectTileCollision(std::shared_ptr<TileSectorArray> const& tileSectorArray, RectI const& region, CollisionSet const& collisionSet);

  template <typename Tiles>
  concept collision_tile_source = requires(Tiles const& tiles, Vec2I const& position, CollisionSet const& kinds) {
    { tiles.tile(position).isColliding(kinds) } -> std::same_as<bool>;
  };

  template <typename Tiles>
  concept world_tile_array = std::derived_from<typename Tiles::Tile, WorldTile>
      && std::same_as<Tiles, TileSectorArray<typename Tiles::Tile, Tiles::sector_size>>;

  template <world_tile_array Tiles>
  void invalidate_collision_cache(Tiles& tiles, RectI const& changed) {
    auto size = tiles.size();
    if (changed.is_empty() || !size[0] || !size[1]) return;
    constexpr std::int64_t radius = CollisionGenerator::block_influence_radius;
    auto width = static_cast<std::int64_t>(size[0]);
    auto span = std::min(width, std::int64_t(changed.x_max()) - changed.x_min() + 2 * radius);
    auto first = std::int64_t(changed.x_min()) - radius;
    if (span == width) {
      first = 0;
    } else if (!std::in_range<int>(first) || !std::in_range<int>(first + span)) {
      first %= width;
      if (first + span > width) first -= width;
    }
    auto y_min = std::clamp<std::int64_t>(std::int64_t(changed.y_min()) - radius, 0, size[1]);
    auto y_max = std::clamp<std::int64_t>(std::int64_t(changed.y_max()) + radius, 0, size[1]);
    RectI const affected(static_cast<int>(first), static_cast<int>(y_min), static_cast<int>(first + span), static_cast<int>(y_max));
    tiles.visit_loaded_sectors(affected,
        [](typename Tiles::Array& sector, RectI const& window, Vec2I const&) {
      for (int x = window.x_min(); x < window.x_max(); ++x)
        for (int y = window.y_min(); y < window.y_max(); ++y)
          sector(x, y).collision_cache_dirty = true;
    });
  }

  template <world_tile_array Tiles>
  [[nodiscard]] std::expected<void, StarException> refresh_collision_cache(Tiles& tiles, RectI const& region,
      CollisionGenerator const& generator) {
    return tiles.visit_loaded_sectors(region, [&](typename Tiles::Array& sector, RectI const& window,
        Vec2I const& source_min) -> std::expected<void, StarException> {
      RectI dirty = RectI::null();
      for (int x = window.x_min(); x < window.x_max(); ++x)
        for (int y = window.y_min(); y < window.y_max(); ++y)
          if (sector(x, y).collision_cache_dirty)
            dirty.combine(RectI(x, y, x + 1, y + 1));
      if (dirty.is_null()) return {};

      // Only output is clipped to the window. The generator still samples its
      // complete halo, including adjacent sectors and wrapped coordinates.
      auto blocks = generator.get_blocks(RectI::with_size(source_min + (dirty.min() - window.min()), dirty.size()));
      if (!blocks) return std::unexpected(std::move(blocks.error()));
      for (int x = dirty.x_min(); x < dirty.x_max(); ++x) {
        for (int y = dirty.y_min(); y < dirty.y_max(); ++y) {
          auto& tile = sector(x, y);
          tile.collision_cache.clear();
          tile.collision_cache_dirty = true;
        }
      }
      for (auto& block : *blocks) {
        auto local = (block.space - source_min) + window.min();
        auto appended = sector(local[0], local[1]).collision_cache.push_back(std::move(block));
        if (!appended) return std::unexpected(std::move(appended.error()));
      }
      for (int x = dirty.x_min(); x < dirty.x_max(); ++x)
        for (int y = dirty.y_min(); y < dirty.y_max(); ++y)
          sector(x, y).collision_cache_dirty = false;
      return {};
    });
  }

  template <world_tile_array Tiles, typename KindFor>
    requires std::invocable<KindFor&, Vec2I const&, typename Tiles::Tile const&>
        && std::same_as<std::invoke_result_t<KindFor&, Vec2I const&, typename Tiles::Tile const&>, CollisionKind>
  [[nodiscard]] std::expected<bool, StarException> collision_blocks_satisfy(Tiles& tiles, RectI const& region,
      CollisionGenerator const& generator, KindFor&& kind_for,
      std::function<std::expected<bool, StarException>(CollisionBlock const&)> const& predicate) {
    if (!predicate) return std::unexpected(StarException("Empty collision block predicate"));
    if (auto refreshed = refresh_collision_cache(tiles, region, generator); !refreshed)
      return std::unexpected(std::move(refreshed.error()));
    return tiles.tile_satisfies(region, [&](Vec2I const& pos, typename Tiles::Tile const& tile) -> std::expected<bool, StarException> {
      if (std::invoke(kind_for, pos, tile) == CollisionKind::Null)
        return predicate(CollisionBlock::null_block(pos));
      if constexpr (Star::debug_enabled) Star::star_assert_failed(!tile.collision_cache_dirty);
      for (auto const& block : tile.collision_cache) {
        auto matched = predicate(block);
        if (!matched || *matched) return matched;
      }
      return false;
    });
  }

  // The result borrows either the catalog or the caller's combined storage.
  [[nodiscard]]
  inline std::expected<std::reference_wrapper<TileDamageParameters const>, StarException> tile_damage_parameters(
      MaterialDatabase const& materials, WorldTile const& tile, TileLayer layer, TileDamageType damage_type,
      std::optional<TileDamageParameters>& combined);
  template <typename Tiles>
  concept damage_tile_source = std::derived_from<typename Tiles::Tile, WorldTile>
      && requires(Tiles& tiles, Vec2I const& pos) {
        { tiles.modify_tile(pos) } -> std::same_as<std::optional<std::reference_wrapper<typename Tiles::Tile>>>;
      };
  template <damage_tile_source TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> damage_would_destroy(Root& root, TileSectorArray& tile_sector_array, Vec2I pos, TileLayer layer, TileDamage const& tile_damage);
  
  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<bool, StarException> canPlaceMaterial(Root& root, std::shared_ptr<EntityMap> const& entityMap,
      Vec2I const& pos, TileLayer layer, MaterialId material, bool allowEntityOverlap, bool allowTileOverlap, GetTileFunction& getTile);
  // returns true if this material could be placed if in the same batch other
  // tiles can be placed
  // that connect to it
  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<bool, StarException> perhapsCanPlaceMaterial(Root& root, std::shared_ptr<EntityMap> const& entityMap,
      Vec2I const& pos, TileLayer layer, MaterialId material, bool allowEntityOverlap, bool allowTileOverlap, GetTileFunction& getTile);
  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<bool, StarException> canPlaceMaterialColorVariant(Root& root, Vec2I const& pos, TileLayer layer, MaterialColorVariant color, GetTileFunction& getTile);
  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<bool, StarException> canPlaceMod(Root& root, Vec2I const& pos, TileLayer layer, ModId mod, GetTileFunction& getTile);
  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<std::pair<bool, bool>, StarException> validateTileModification(Root& root, std::shared_ptr<EntityMap> const& entityMap, Vec2I const& pos, TileModification const& modification, bool allowEntityOverlap, GetTileFunction& getTile);
  [[nodiscard]]
  std::expected<bool, StarException> validateTileReplacement(Root& root, TileModification const& modification);
  // Split modification list into good and bad
  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<std::pair<TileModificationList, TileModificationList>, StarException> splitTileModifications(Root& root, std::shared_ptr<EntityMap> const& entityMap, TileModificationList const& modificationList,
    bool allowEntityOverlap, GetTileFunction& getTile, std::function<std::expected<bool, StarException>(Vec2I pos, TileModification modification)> extraCheck = {});

  template <typename TileSectorArray>
  float windLevel(std::shared_ptr<TileSectorArray> const& tileSectorArray, Vec2F const& position, float weatherWindLevel);


  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> breathable(Root& root, World const& world, std::shared_ptr<TileSectorArray> const& tileSectorArray, HashMap<DungeonId, bool> const& breathableMap,
      std::shared_ptr<WorldTemplate const> const& worldTemplate, Vec2F const& pos);

  template <world_tile_array Tiles>
  [[nodiscard]]
  std::expected<float, StarException> light_level(Root& root, std::shared_ptr<Tiles> const& tiles, std::shared_ptr<EntityMap> const& entity_map, WorldGeometry const& world_geometry,
      std::shared_ptr<WorldTemplate const> const& world_template, std::shared_ptr<Sky const> const& sky, CellularLightIntensityCalculator& lighting, Vec2F pos);

  [[nodiscard]]
  std::expected<std::shared_ptr<InteractiveEntity>, StarException> getInteractiveInRange(WorldGeometry const& geometry, std::shared_ptr<EntityMap> const& entityMap,
      Vec2F const& targetPosition, Vec2F const& sourcePosition, float maxRange);
  [[nodiscard]]
  std::expected<bool, StarException> isTileEntityInRange(WorldGeometry const& geometry, std::shared_ptr<EntityMap> const& entityMap,
      EntityId targetEntity, Vec2F const& sourcePosition, float maxRange);

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> tileIsOccupied(std::shared_ptr<TileSectorArray> const& tileSectorArray, std::shared_ptr<EntityMap> const& entityMap,
      Vec2I const& pos, TileLayer layer, bool includeEphemeral, bool checkCollision) {
    auto& tile = tileSectorArray->tile(pos);
    if (layer == TileLayer::Foreground) {
      if (checkCollision ? tile.getCollision() >= CollisionKind::Dynamic : tile.foreground != empty_material_id)
        return true;
      return entityMap->tile_is_occupied(pos, includeEphemeral);
    }
    return tile.background != empty_material_id;
  }

  template <typename TileSectorArray>
  CollisionKind tileCollisionKind(std::shared_ptr<TileSectorArray> const& tileSectorArray, std::shared_ptr<EntityMap> const&,
    Vec2I const& pos) {
    return tileSectorArray->tile(pos).getCollision();
  }

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> rectTileCollision(std::shared_ptr<TileSectorArray> const& tileSectorArray, RectI const& region, CollisionSet const& collisionSet) {
    return tileSectorArray->tile_satisfies(region, [&collisionSet](Vec2I const&, typename TileSectorArray::Tile const& tile) -> std::expected<bool, StarException> {
        return isColliding(tile.getCollision(), collisionSet);
      });
  }

  template <collision_tile_source Tiles>
  [[nodiscard]]
  std::expected<bool, StarException> line_tile_collision_along_delta(Tiles const& tiles,
      Vec2F const& begin, Vec2F const& delta, CollisionSet const& collision_set) {
    auto clear = for_each_block_along_line(begin, delta, [&tiles, &collision_set](int x, int y) -> std::expected<bool, StarException> {
      return !tiles.tile({x, y}).isColliding(collision_set);
    });
    if (!clear)
      return std::unexpected(std::move(clear.error()));
    return !*clear;
  }

  template <collision_tile_source Tiles>
  [[nodiscard]]
  std::expected<bool, StarException> line_tile_collision(WorldGeometry const& geometry, Tiles const& tiles,
      Vec2F const& begin, Vec2F const& end, CollisionSet const& collision_set) {
    return line_tile_collision_along_delta(tiles, begin, geometry.diff(end, begin), collision_set);
  }

  template <collision_tile_source Tiles>
  [[nodiscard]]
  std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> line_tile_collision_point(WorldGeometry const& geometry,
      Tiles const& tiles, Vec2F const& begin, Vec2F const& end, CollisionSet const& collision_set) {
    Vec2I colliding_block;
    auto delta = geometry.diff(end, begin);
    auto clear = for_each_block_along_line(begin, delta, [&tiles, &collision_set, &colliding_block](int x, int y) -> std::expected<bool, StarException> {
      if (!tiles.tile({x, y}).isColliding(collision_set))
        return true;
      colliding_block = Vec2I(x, y);
      return false;
    });

    if (!clear)
      return std::unexpected(std::move(clear.error()));
    if (*clear)
      return std::optional<std::pair<Vec2F, Vec2I>>{};

    Vec2F direction = delta.normalized();
    Vec2F block_position(colliding_block);

    // find the position of intersecting sides
    if (direction[0] < 0) ++block_position[0];
    if (direction[1] < 0) ++block_position[1];
    Vec2F block_distance = geometry.diff(block_position, begin);

    // exclude edges which are in the opposite direction of the line
    if (direction[0] * block_distance[0] < 0) block_distance[0] = 0.0f;
    if (direction[1] * block_distance[1] < 0) block_distance[1] = 0.0f;

    // line length per block in each axis, don't divide by 0
    float delta_dist_x = direction[0] == 0 ? 0.0f : 1.0f / std::abs(direction[0]);
    float delta_dist_y = direction[1] == 0 ? 0.0f : 1.0f / std::abs(direction[1]);

    // distance to each edge of the intersect
    Vec2F intersect_dist(std::abs(block_distance[0]) * delta_dist_x, std::abs(block_distance[1]) * delta_dist_y);

    Vec2I normal(intersect_dist[0] > intersect_dist[1], intersect_dist[1] > intersect_dist[0]);
    normal[0] *= std::copysign(1.0f, direction[0]);
    normal[1] *= std::copysign(1.0f, direction[1]);
    Vec2F position = begin + direction * std::max(intersect_dist[0], intersect_dist[1]);
    // HACK: Keeps the end position from being right on a tile border, making line check
    // *from* this end position from colliding with the first tile
    if (intersect_dist[0] > intersect_dist[1])
      position[0] = block_position[0] + (normal[0] * -0.0001f);
    else
      position[1] = block_position[1] + (normal[1] * -0.0001f);
    return std::pair<Vec2F, Vec2I>(position, normal);
  }

  template <world_tile_array Tiles>
    requires requires(typename Tiles::Tile const& tile) {
      requires std::derived_from<decltype(tile.liquid), LiquidLevel>;
    }
  [[nodiscard]]
  std::expected<LiquidLevel, StarException> liquid_level(Tiles const& tiles, RectF const& region) {
#pragma clang fp reassociate(off)
    if (region.is_empty())
      return LiquidLevel();

    float total_space = 0.0f;
    LiquidId first_id = empty_liquid_id;
    float first_total = 0.0f;
    unsigned min_id = 0;
    unsigned max_id = 0;
    // The usual empty and single-liquid samples do not initialize the table.
    std::optional<std::array<float, std::numeric_limits<LiquidId>::max() + 1>> mixed_totals;
    auto sampled = tiles.tile_each(RectI::integral(region), [&](Vec2I const& pos, typename Tiles::Tile const& tile) -> std::expected<void, StarException> {
      float incidence = RectF(pos[0], pos[1], pos[0] + 1, pos[1] + 1).overlap(region).volume();
      total_space += incidence;
      LiquidLevel const& liquid = tile.liquid;
      if (liquid.liquid == empty_liquid_id)
        return {};

      if (!mixed_totals) {
        if (first_id == empty_liquid_id)
          first_id = liquid.liquid;
        if (liquid.liquid == first_id) {
          first_total += std::min(1.0f, liquid.level) * incidence;
          return {};
        }

        mixed_totals.emplace();
        (*mixed_totals)[first_id] = first_total;
        min_id = max_id = first_id;
      }
      unsigned id = liquid.liquid;
      (*mixed_totals)[id] += std::min(1.0f, liquid.level) * incidence;
      min_id = std::min(min_id, id);
      max_id = std::max(max_id, id);
      return {};
    });
    if (!sampled)
      return std::unexpected(std::move(sampled.error()));
    if (total_space == 0.0f || first_id == empty_liquid_id)
      return LiquidLevel();
    if (!mixed_totals)
      return LiquidLevel(first_total > 0.0f ? first_id : empty_liquid_id, first_total / total_space);

    // All liquids contribute to the level. Reduce by ascending ID to keep the
    // summation order and let the lowest ID win equal positive volumes.
    float total_level = 0.0f;
    float max_level = 0.0f;
    LiquidId dominant_id = empty_liquid_id;
    for (unsigned id = min_id; id <= max_id; ++id) {
      float volume = (*mixed_totals)[id];
      total_level += volume;
      if (volume > max_level) {
        max_level = volume;
        dominant_id = static_cast<LiquidId>(id);
      }
    }
    return LiquidLevel(dominant_id, total_level / total_space);
  }

  template <collision_tile_source Tiles>
  [[nodiscard]]
  std::expected<List<Vec2I>, StarException> colliding_tiles_along_line(WorldGeometry const& geometry, Tiles const& tiles,
      Vec2F const& begin, Vec2F const& end, CollisionSet const& collision_set, std::size_t max_size, bool include_edges) {
    List<Vec2I> candidates;
    auto traversed = for_each_block_along_line(begin, geometry.diff(end, begin),
        [&tiles, &collision_set, &candidates, &max_size](int x, int y) -> std::expected<bool, StarException> {
      if (candidates.size() < max_size && tiles.tile({x, y}).isColliding(collision_set))
        candidates.emplace_back(x, y);
      return candidates.size() < max_size;
    });
    if (!traversed)
      return std::unexpected(std::move(traversed.error()));
    if (include_edges || candidates.empty())
      return candidates;

    // The limit applies to candidates, not to the surviving non-glancing hits.
    auto segments = geometry.split_line(Line2F{geometry.xwrap(begin), geometry.xwrap(end)});
    if (!segments)
      return std::unexpected(std::move(segments.error()));
    auto tail = std::ranges::remove_if(candidates, [&geometry, &segments](Vec2I const& tile) {
      auto bounds = RectF::with_size(Vec2F(geometry.xwrap(tile)), Vec2F::filled(1.0f));
      return std::ranges::any_of(*segments, [&bounds](Line2F const& segment) { return bounds.edge_intersection(segment).glances; });
    });
    candidates.erase(tail.begin(), tail.end());
    return candidates;
  }

  inline std::expected<std::reference_wrapper<TileDamageParameters const>, StarException> tile_damage_parameters(
      MaterialDatabase const& materials, WorldTile const& tile, TileLayer layer, TileDamageType damage_type,
      std::optional<TileDamageParameters>& combined) {
    auto target = tile.material(layer);
    auto mod = tile.mod(layer);
    if (!is_real_mod(mod) || tile_damage_is_penetrating(damage_type))
      return materials.material_damage_parameters(target);
    auto breaks_with_tile = materials.mod_breaks_with_tile(mod);
    if (!breaks_with_tile)
      return std::unexpected(std::move(breaks_with_tile.error()));
    auto mod_damage = materials.mod_damage_parameters(mod);
    if (!mod_damage)
      return std::unexpected(std::move(mod_damage.error()));
    if (!*breaks_with_tile)
      return *mod_damage;
    auto material_damage = materials.material_damage_parameters(target);
    if (!material_damage)
      return std::unexpected(std::move(material_damage.error()));
    auto sum = mod_damage->get().sum(material_damage->get());
    if (!sum)
      return std::unexpected(std::move(sum.error()));
    return std::cref(combined.emplace(std::move(*sum)));
  }

  template <damage_tile_source TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> damage_would_destroy(Root& root, TileSectorArray& tile_sector_array, Vec2I pos, TileLayer layer, TileDamage const& tile_damage) {
    if (auto tile = tile_sector_array.modify_tile(pos)) {
      auto materials = root.material_database();
      if (!materials)
        return std::unexpected(std::move(materials.error()));
      std::optional<TileDamageParameters> combined;
      auto damage_parameters = tile_damage_parameters(**materials, tile->get(), layer, tile_damage.type, combined);
      if (!damage_parameters)
        return std::unexpected(std::move(damage_parameters.error()));
      auto const& damage = layer == TileLayer::Foreground ? tile->get().foregroundDamage : tile->get().backgroundDamage;
      return damage_parameters->get().damage_fraction(tile_damage) + damage.damage_percentage() >= 1.0f;
    }

    return false;
  }

  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<bool, StarException> canPlaceMaterial(Root& root, std::shared_ptr<EntityMap> const& entityMap,
      Vec2I const& pos, TileLayer layer, MaterialId material, bool allowEntityOverlap, bool allowTileOverlap, GetTileFunction& getTile) {
    auto material_database = root.material_database();
    if (!material_database)
      return std::unexpected(std::move(material_database.error()));
    if (!is_real_material(material))
      return false;

    auto isAdjacentToConnectable = [&](Vec2I const& pos, int distance, bool foreground) -> std::expected<bool, StarException> {
      if (pos.y() - distance < 0)
        return true;
      int maxY = pos.y() + distance + 1;
      int maxX = pos.x() + distance + 1;
      for (int y = pos.y() - distance; y != maxY; ++y) {
        Vec2I tPos = {0, y};
        for (int x = pos.x() - distance; x != maxX; ++x) {
          tPos[0] = x;
          if (tPos != pos) {
            auto tile = getTile(tPos);
            if (!tile)
              return std::unexpected(std::move(tile.error()));
            if (is_connectable_material(foreground ? tile->get().foreground : tile->get().background))
              return true;
          }
        }
      }
      return false;
    };

    auto can_place = (*material_database)->can_place_in_layer(material, layer);
    if (!can_place)
      return std::unexpected(std::move(can_place.error()));
    if (!*can_place)
      return false;
    auto tile = getTile(pos);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    if (layer == TileLayer::Background) {
      if (tile->get().background != empty_material_id && tile->get().background != object_platform_material_id)
        return false;
      auto adjacent = isAdjacentToConnectable(pos, 1, false);
      if (!adjacent)
        return std::unexpected(std::move(adjacent.error()));
      if (!*adjacent && !is_connectable_material(tile->get().foreground))
        return false;
    } else {
      if (tile->get().foreground != empty_material_id && tile->get().foreground != object_platform_material_id)
        return false;
      if (!allowTileOverlap) {
        auto occupied = entityMap->tile_is_occupied(pos);
        if (!occupied)
          return std::unexpected(std::move(occupied.error()));
        if (*occupied)
          return false;
      }
      if (!allowEntityOverlap) {
        auto occupied = entityMap->space_is_occupied(RectF::with_size(Vec2F(pos), Vec2F(0.999f, 0.999f)));
        if (!occupied)
          return std::unexpected(std::move(occupied.error()));
        if (*occupied)
          return false;
      }
      auto adjacent = isAdjacentToConnectable(pos, 1, true);
      if (!adjacent)
        return std::unexpected(std::move(adjacent.error()));
      if (!*adjacent && !is_connectable_material(tile->get().background))
        return false;
    }
    return true;
  }

  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<bool, StarException> perhapsCanPlaceMaterial(Root& root, std::shared_ptr<EntityMap> const& entityMap,
      Vec2I const& pos, TileLayer layer, MaterialId material, bool allowEntityOverlap, bool allowTileOverlap, GetTileFunction& getTile) {
    auto material_database = root.material_database();
    if (!material_database)
      return std::unexpected(std::move(material_database.error()));
    if (!is_real_material(material))
      return false;
    auto can_place = (*material_database)->can_place_in_layer(material, layer);
    if (!can_place)
      return std::unexpected(std::move(can_place.error()));
    if (!*can_place)
      return false;
    auto tile = getTile(pos);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    if (layer == TileLayer::Background)
      return tile->get().background == empty_material_id || tile->get().background == object_platform_material_id;
    if (tile->get().foreground != empty_material_id && tile->get().foreground != object_platform_material_id)
      return false;
    if (!allowTileOverlap) {
      auto occupied = entityMap->tile_is_occupied(pos);
      if (!occupied)
        return std::unexpected(std::move(occupied.error()));
      if (*occupied)
        return false;
    }
    if (!allowEntityOverlap) {
      auto occupied = entityMap->space_is_occupied(RectF::with_size(Vec2F(pos), Vec2F(0.999f, 0.999f)));
      if (!occupied)
        return std::unexpected(std::move(occupied.error()));
      if (*occupied)
        return false;
    }
    return true;
  }

  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<bool, StarException> canPlaceMaterialColorVariant(Root& root, Vec2I const& pos, TileLayer layer, MaterialColorVariant color, GetTileFunction& getTile) {
    auto material_database = root.material_database();
    if (!material_database)
      return std::unexpected(std::move(material_database.error()));
    auto tile = getTile(pos);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    auto existingColor = tile->get().materialColor(layer);
    auto existingHue = layer == TileLayer::Foreground ? tile->get().foregroundHueShift : tile->get().backgroundHueShift;
    if (existingHue != 0)
      return true;
    if (existingColor == color)
      return false;
    return (*material_database)->is_multi_color(tile->get().material(layer));
  }

  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<bool, StarException> canPlaceMod(Root& root, Vec2I const& pos, TileLayer layer, ModId mod, GetTileFunction& getTile) {
    if (!is_real_mod(mod))
      return false;
    auto material_database = root.material_database();
    if (!material_database)
      return std::unexpected(std::move(material_database.error()));
    auto tile = getTile(pos);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    if (tile->get().mod(layer) == mod)
      return false;
    return (*material_database)->supports_mod(tile->get().material(layer), mod);
  }

  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<std::pair<bool, bool>, StarException> validateTileModification(Root& root, std::shared_ptr<EntityMap> const& entityMap, Vec2I const& pos, TileModification const& modification, bool allowEntityOverlap, GetTileFunction& getTile) {
    std::expected<bool, StarException> good = false;
    bool perhaps = false;
    if (std::holds_alternative<PlaceMaterial>(modification)) {
      auto const& placeMaterial = std::get<PlaceMaterial>(modification);
      bool allowTileOverlap = placeMaterial.collisionOverride != TileCollisionOverride::None && collisionKindFromOverride(placeMaterial.collisionOverride) < CollisionKind::Dynamic;
      auto possible = WorldImpl::perhapsCanPlaceMaterial(root, entityMap, pos, placeMaterial.layer, placeMaterial.material, allowEntityOverlap, allowTileOverlap, getTile);
      if (!possible)
        return std::unexpected(std::move(possible.error()));
      perhaps = *possible;
      if (perhaps)
        good = WorldImpl::canPlaceMaterial(root, entityMap, pos, placeMaterial.layer, placeMaterial.material, allowEntityOverlap, allowTileOverlap, getTile);
    } else if (std::holds_alternative<PlaceMod>(modification)) {
      auto const& placeMod = std::get<PlaceMod>(modification);
      good = WorldImpl::canPlaceMod(root, pos, placeMod.layer, placeMod.mod, getTile);
    } else if (std::holds_alternative<PlaceMaterialColor>(modification)) {
      auto const& placeMaterialColor = std::get<PlaceMaterialColor>(modification);
      good = WorldImpl::canPlaceMaterialColorVariant(root, pos, placeMaterialColor.layer, placeMaterialColor.color, getTile);
    } else if (std::holds_alternative<PlaceLiquid>(modification)) {
      auto tile = getTile(pos);
      if (!tile)
        return std::unexpected(std::move(tile.error()));
      good = tile->get().collision == CollisionKind::None;
    }
    if (!good)
      return std::unexpected(std::move(good.error()));
    return std::pair<bool, bool>(*good, perhaps);
  }

  inline std::expected<bool, StarException> validateTileReplacement(Root& root, TileModification const& modification) {
    if (std::holds_alternative<PlaceMaterial>(modification)) {
      auto const& placeMaterial = std::get<PlaceMaterial>(modification);
      if (!is_real_material(placeMaterial.material))
        return false;
      auto material_database = root.material_database();
      if (!material_database)
        return std::unexpected(std::move(material_database.error()));
      return (*material_database)->can_place_in_layer(placeMaterial.material, placeMaterial.layer);
    }
    return false;
  }

  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<std::pair<TileModificationList, TileModificationList>, StarException> splitTileModifications(Root& root, std::shared_ptr<EntityMap> const& entityMap, TileModificationList const& modificationList,
    bool allowEntityOverlap, GetTileFunction& getTile, std::function<std::expected<bool, StarException>(Vec2I pos, TileModification modification)> extraCheck) {
    TileModificationList success;
    TileModificationList unknown;
    TileModificationList failures;
    for (auto const& pair : modificationList) {

      bool good = false, perhaps = false;
      std::expected<bool, StarException> allowed = true;
      if (extraCheck)
        allowed = extraCheck(pair.first, pair.second);
      if (!allowed)
        return std::unexpected(std::move(allowed.error()));
      if (*allowed) {
        auto valid = validateTileModification(root, entityMap, pair.first, pair.second, allowEntityOverlap, getTile);
        if (!valid)
          return std::unexpected(std::move(valid.error()));
        std::tie(good, perhaps) = *valid;
      }

      if (good)
        success.push_back(pair);
      else if (perhaps)
        unknown.push_back(pair);
      else
        failures.push_back(pair);
    }

    if (!success.empty())
      success.append_range(unknown);
    else
      failures.append_range(unknown);

    return std::pair<TileModificationList, TileModificationList>(std::move(success), std::move(failures));
  }

  template <typename TileSectorArray>
  float windLevel(std::shared_ptr<TileSectorArray> const& tileSectorArray, Vec2F const& position, float weatherWindLevel) {
    auto const& tile = tileSectorArray->tile(Vec2I::floor(position));
    if (tile.material(TileLayer::Background) != empty_material_id)
      return 0.0f;
    if (tile.material(TileLayer::Foreground) != empty_material_id)
      return 0.0f;
    return weatherWindLevel;
  }

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> breathable(Root&, World const& world, std::shared_ptr<TileSectorArray> const& tileSectorArray, HashMap<DungeonId, bool> const& breathableMap,
      std::shared_ptr<WorldTemplate const> const& worldTemplate, Vec2F const& pos) {
    Vec2I ipos = Vec2I::floor(pos);
    float remainder = pos[1] - ipos[1];
    auto const& tile = tileSectorArray->tile(ipos);
    bool environmentBreathable = breathableMap.maybe(tile.dungeonId).value_or(worldTemplate->breathable(ipos[0], ipos[1]));
    bool liquidBreathable = remainder >= tile.liquid.level;
    bool foregroundBreathable = true;
    if (tile.getCollision() == CollisionKind::Block) {
      auto collision = world.point_collision(pos);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      foregroundBreathable = !*collision;
    }
    return environmentBreathable && foregroundBreathable && liquidBreathable;
  }

  template <world_tile_array Tiles>
  [[nodiscard]]
  std::expected<float, StarException> light_level(Root& root, std::shared_ptr<Tiles> const& tiles, std::shared_ptr<EntityMap> const& entity_map, WorldGeometry const& world_geometry,
      std::shared_ptr<WorldTemplate const> const& world_template, std::shared_ptr<Sky const> const& sky, CellularLightIntensityCalculator& lighting, Vec2F pos) {
    if (pos[1] < 0 || pos[1] >= world_geometry.height())
      return 0;

    // Normalize the floating-point input before constructing integer lighting bounds.
    pos = world_geometry.xwrap(pos);

    Vec3F environment_light = sky->environmentLight().to_rgb_f();
    float underground_level = world_template->undergroundLevel();
    auto materials = root.material_database();
    if (!materials)
      return std::unexpected(std::move(materials.error()));
    auto liquids = root.liquids_database();
    if (!liquids)
      return std::unexpected(std::move(liquids.error()));

    auto begun = lighting.begin(pos);
    if (!begun) return std::unexpected(std::move(begun.error()));

    // Each column in tile_eval_columns is guaranteed to be no larger than the
    // sector size.
    std::array<CellularLightIntensityCalculator::Cell, Tiles::sector_size> lighting_column;
    auto evaluated = tiles->tile_eval_columns(lighting.calculation_region(), [&](Vec2I const& pos, std::span<typename Tiles::Tile> column) -> std::expected<void, StarException> {
        for (std::size_t y = 0; y < column.size(); ++y) {
          auto& tile = column[y];
          auto& cell = lighting_column[y];

          auto background_transparent = (*materials)->background_light_transparent(tile.background);
          if (!background_transparent)
            return std::unexpected(std::move(background_transparent.error()));
          auto foreground_transparency = (*materials)->foreground_light_transparent(tile.foreground);
          if (!foreground_transparency)
            return std::unexpected(std::move(foreground_transparency.error()));
          bool foreground_transparent = *foreground_transparency && tile.getCollision() != CollisionKind::Dynamic;

          cell = {(*materials)->radiant_light(tile.foreground, tile.foregroundMod).sum() / 3.0f, !foreground_transparent};
          cell.light += (*liquids)->radiant_light(tile.liquid).sum() / 3.0f;
          if (foreground_transparent) {
            cell.light += (*materials)->radiant_light(tile.background, tile.backgroundMod).sum() / 3.0f;
            if (*background_transparent && pos[1] > underground_level)
              cell.light += environment_light.sum() / 3.0f;
          }
        }
        lighting.set_cell_column(pos, std::span(lighting_column).first(column.size()));
        return {};
      });

    if (!evaluated)
      return std::unexpected(std::move(evaluated.error()));
    auto entities = entity_map->entity_query(RectF(lighting.calculation_region()));
    if (!entities)
      return std::unexpected(std::move(entities.error()));
    for (auto const& entity : *entities) {
      auto lights = entity->light_sources();
      if (!lights)
        return std::unexpected(std::move(lights.error()));
      for (auto const& light : *lights) {
        Vec2F position = world_geometry.nearest_to(Vec2F(lighting.calculation_region().min()), light.position);
        if (light.type == LightType::Spread)
          lighting.add_spread_light(position, light.color.sum() / 3.0f);
        else
          lighting.add_point_light(position, light.color.sum() / 3.0f, light.point_beam, light.beam_angle, light.beam_ambience);
      }
    }

    return lighting.calculate();
  }

  inline std::expected<std::shared_ptr<InteractiveEntity>, StarException> getInteractiveInRange(WorldGeometry const& geometry, std::shared_ptr<EntityMap> const& entityMap,
      Vec2F const& targetPosition, Vec2F const& sourcePosition, float maxRange) {
    auto entity = entityMap->interactive_entity_near(targetPosition, maxRange);
    if (!entity)
      return std::unexpected(std::move(entity.error()));
    if (*entity) {
      if (auto tileEntity = as<TileEntity>(*entity)) {
        auto inRange = isTileEntityInRange(geometry, entityMap, tileEntity->entityId(), sourcePosition, maxRange);
        if (!inRange)
          return std::unexpected(std::move(inRange.error()));
        if (*inRange)
          return std::move(*entity);
      } else if (geometry.diff_to_nearest_coord_in_box((*entity)->interactive_bound_box().translated((*entity)->position()), sourcePosition).magnitude() <= maxRange) {
        return std::move(*entity);
      }
    }
    return std::shared_ptr<InteractiveEntity>{};
  }

  inline std::expected<bool, StarException> isTileEntityInRange(WorldGeometry const& geometry, std::shared_ptr<EntityMap> const& entityMap, EntityId targetEntity,
      Vec2F const& sourcePosition, float maxRange) {
    if (auto entity = entityMap->get<TileEntity>(targetEntity)) {
      // If any of the entity spaces is within interaction range, then assume the
      // whole entity is in interaction range
      for (auto space : entity->spaces()) {
        if (geometry.diff(entity->position() + center_of_tile(space), sourcePosition).magnitude() <= maxRange)
          return true;
      }
    }

    return false;
  }

  template <collision_tile_source Tiles>
  [[nodiscard]]
  std::expected<bool, StarException> can_reach_entity(WorldGeometry const& geometry, Tiles const& tiles,
      EntityMap const& entity_map, Vec2F const& source_position, float max_range, EntityId target_entity, bool prefer_interactive = true) {
    auto entity = entity_map.entity(target_entity);
    if (!entity)
      return false;

    // Exclude the final tile: the target itself can be a collidable tile entity (e.g. a door).
    auto can_reach_tile = [&geometry, &tiles, &source_position](Vec2F const& end, Vec2F const& delta) -> std::expected<bool, StarException> {
      auto end_tile = Vec2I::floor(end);
      return for_each_block_along_line(source_position, delta, [&geometry, &tiles, &end_tile](int x, int y) -> std::expected<bool, StarException> {
        if (geometry.diff(end_tile, Vec2I(x, y)) == Vec2I(0, 0))
          return true;
        return !tiles.tile({x, y}).isColliding(DefaultCollisionSet);
      });
    };

    if (auto tile_entity = as_ref<TileEntity>(*entity)) {
      for (auto space : prefer_interactive ? tile_entity->get().interactiveSpaces() : tile_entity->get().spaces()) {
        auto space_position = entity->position() + center_of_tile(space);
        auto delta = geometry.diff(space_position, source_position);
        if (delta.magnitude() <= max_range) {
          auto reachable = can_reach_tile(space_position, delta);
          if (!reachable)
            return std::unexpected(std::move(reachable.error()));
          if (*reachable)
            return true;
        }
      }
    } else if (auto interactive_entity = prefer_interactive ? as_ref<InteractiveEntity>(*entity) : std::nullopt) {
      auto entity_bounds = interactive_entity->get().interactive_bound_box().translated(entity->position());
      auto contains = geometry.rect_contains(entity_bounds, source_position);
      if (!contains)
        return std::unexpected(std::move(contains.error()));
      if (*contains)
        return true;
      auto intersects = geometry.rect_intersects_circle(entity_bounds, source_position, max_range);
      if (!intersects)
        return std::unexpected(std::move(intersects.error()));
      if (!*intersects)
        return false;

      auto collision = line_tile_collision(geometry, tiles, source_position, entity_bounds.nearest_coord_to(source_position), DefaultCollisionSet);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      if (!*collision)
        return true;
      collision = line_tile_collision(geometry, tiles, source_position, Vec2F(entity_bounds.x_min(), entity_bounds.y_min()), DefaultCollisionSet);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      if (!*collision)
        return true;
      collision = line_tile_collision(geometry, tiles, source_position, Vec2F(entity_bounds.x_min(), entity_bounds.y_max()), DefaultCollisionSet);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      if (!*collision)
        return true;
      collision = line_tile_collision(geometry, tiles, source_position, Vec2F(entity_bounds.x_max(), entity_bounds.y_max()), DefaultCollisionSet);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      if (!*collision)
        return true;
      collision = line_tile_collision(geometry, tiles, source_position, Vec2F(entity_bounds.x_max(), entity_bounds.y_min()), DefaultCollisionSet);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      return !*collision;
    } else {
      auto delta = geometry.diff(entity->position(), source_position);
      if (delta.magnitude() <= max_range) {
        auto collision = line_tile_collision_along_delta(tiles, source_position, delta, DefaultCollisionSet);
        if (!collision)
          return std::unexpected(std::move(collision.error()));
        return !*collision;
      }
    }

    return false;
  }
}

}

export module star.world_impl;

export namespace Star::WorldImpl {
  using ::Star::WorldImpl::invalidate_collision_cache;
  using ::Star::WorldImpl::refresh_collision_cache;
  using ::Star::WorldImpl::collision_blocks_satisfy;
  using ::Star::WorldImpl::tileIsOccupied;
  using ::Star::WorldImpl::tileCollisionKind;
  using ::Star::WorldImpl::rectTileCollision;
  using ::Star::WorldImpl::collision_tile_source;
  using ::Star::WorldImpl::line_tile_collision;
  using ::Star::WorldImpl::line_tile_collision_point;
  using ::Star::WorldImpl::colliding_tiles_along_line;
  using ::Star::WorldImpl::tile_damage_parameters;
  using ::Star::WorldImpl::damage_would_destroy;
  using ::Star::WorldImpl::canPlaceMaterial;
  using ::Star::WorldImpl::perhapsCanPlaceMaterial;
  using ::Star::WorldImpl::canPlaceMaterialColorVariant;
  using ::Star::WorldImpl::canPlaceMod;
  using ::Star::WorldImpl::validateTileModification;
  using ::Star::WorldImpl::validateTileReplacement;
  using ::Star::WorldImpl::splitTileModifications;
  using ::Star::WorldImpl::windLevel;
  using ::Star::WorldImpl::breathable;
  using ::Star::WorldImpl::light_level;
  using ::Star::WorldImpl::getInteractiveInRange;
  using ::Star::WorldImpl::isTileEntityInRange;
  using ::Star::WorldImpl::can_reach_entity;
  using ::Star::WorldImpl::liquid_level;
}
