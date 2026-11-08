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
import star.version;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import star.either;
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
import star.pythonic;
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

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> lineTileCollision(WorldGeometry const& worldGeometry, std::shared_ptr<TileSectorArray> const& tileSectorArray, Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet);

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> lineTileCollisionPoint(WorldGeometry const& worldGeometry, std::shared_ptr<TileSectorArray> const& tileSectorArray, Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet);

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<List<Vec2I>, StarException> collidingTilesAlongLine(WorldGeometry const& worldGeometry, std::shared_ptr<TileSectorArray> const& tileSectorArray,
      Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet, std::size_t maxSize, bool includeEdges);

  [[nodiscard]]
  inline std::expected<TileDamageParameters, StarException> tileDamageParameters(Root& root, WorldTile& tile, TileLayer layer, TileDamage const& tileDamage);
  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> damageWouldDestroy(Root& root, std::shared_ptr<TileSectorArray> const& tileSectorArray, Vec2I pos, TileLayer layer, TileDamage const& tileDamage);
  
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

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<float, StarException> lightLevel(Root& root, std::shared_ptr<TileSectorArray> const& tileSectorArray, std::shared_ptr<EntityMap> const& entityMap, WorldGeometry const& worldGeometry,
      std::shared_ptr<WorldTemplate const> const& worldTemplate, std::shared_ptr<Sky const> const& sky, CellularLightIntensityCalculator& lighting, Vec2F pos);

  [[nodiscard]]
  std::expected<std::shared_ptr<InteractiveEntity>, StarException> getInteractiveInRange(WorldGeometry const& geometry, std::shared_ptr<EntityMap> const& entityMap,
      Vec2F const& targetPosition, Vec2F const& sourcePosition, float maxRange);
  [[nodiscard]]
  std::expected<bool, StarException> isTileEntityInRange(WorldGeometry const& geometry, std::shared_ptr<EntityMap> const& entityMap,
      EntityId targetEntity, Vec2F const& sourcePosition, float maxRange);
  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> canReachEntity(WorldGeometry const& geometry, std::shared_ptr<TileSectorArray> const& tileSectorArray,
      std::shared_ptr<EntityMap> const& entityMap, Vec2F const& sourcePosition, float maxRange, EntityId targetEntity, bool preferInteractive = true);

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> tileIsOccupied(std::shared_ptr<TileSectorArray> const& tileSectorArray, std::shared_ptr<EntityMap> const& entityMap,
      Vec2I const& pos, TileLayer layer, bool includeEphemeral, bool checkCollision) {
    auto& tile = tileSectorArray->tile(pos);
    if (layer == TileLayer::Foreground) {
      if (checkCollision ? tile.getCollision() >= CollisionKind::Dynamic : tile.foreground != EmptyMaterialId)
        return true;
      return entityMap->tileIsOccupied(pos, includeEphemeral);
    }
    return tile.background != EmptyMaterialId;
  }

  template <typename TileSectorArray>
  CollisionKind tileCollisionKind(std::shared_ptr<TileSectorArray> const& tileSectorArray, std::shared_ptr<EntityMap> const&,
    Vec2I const& pos) {
    return tileSectorArray->tile(pos).getCollision();
  }

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> rectTileCollision(std::shared_ptr<TileSectorArray> const& tileSectorArray, RectI const& region, CollisionSet const& collisionSet) {
    return tileSectorArray->tileSatisfies(region, [&collisionSet](Vec2I const&, typename TileSectorArray::Tile const& tile) -> std::expected<bool, StarException> {
        return isColliding(tile.getCollision(), collisionSet);
      });
  }

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> lineTileCollision(WorldGeometry const& worldGeometry, std::shared_ptr<TileSectorArray> const& tileSectorArray,
      Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet) {
    auto clear = forBlocksAlongLine<float>(begin, worldGeometry.diff(end, begin), [=](int x, int y) -> std::expected<bool, StarException> {
        return !tileSectorArray->tile({x, y}).isColliding(collisionSet);
      });
    if (!clear)
      return std::unexpected(std::move(clear.error()));
    return !*clear;
  }

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> lineTileCollisionPoint(WorldGeometry const& worldGeometry, std::shared_ptr<TileSectorArray> const& tileSectorArray,
      Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet) {
    std::optional<Vec2I> collidingBlock;
    auto clear = forBlocksAlongLine<float>(begin, worldGeometry.diff(end, begin), [=, &collidingBlock](int x, int y) -> std::expected<bool, StarException> {
        if (tileSectorArray->tile({x, y}).isColliding(collisionSet)) {
          collidingBlock = Vec2I(x, y);
          return false;
        } else {
          return true;
        }
      });

    if (!clear)
      return std::unexpected(std::move(clear.error()));
    if (*clear)
      return std::optional<std::pair<Vec2F, Vec2I>>{};

    Vec2F direction = worldGeometry.diff(end, begin).normalized();
    Vec2F blockPosition = Vec2F(*collidingBlock);

    // find the position of intersecting sides
    if (direction[0] < 0) blockPosition[0] = blockPosition[0] + 1;
    if (direction[1] < 0) blockPosition[1] = blockPosition[1] + 1;
    Vec2F blockDistance = worldGeometry.diff(blockPosition, begin);

    // exclude edges which are in the opposite direction of the line
    if (direction[0] * blockDistance[0] < 0) blockDistance[0] = 0.0f;
    if (direction[1] * blockDistance[1] < 0) blockDistance[1] = 0.0f;

    // line length per block in each axis, don't divide by 0
    float deltaDistX = direction[0] == 0 ? 0.0f : 1.0f / std::abs(direction[0]);
    float deltaDistY = direction[1] == 0 ? 0.0f : 1.0f / std::abs(direction[1]);

    // distance to each edge of the intersect
    Vec2F intersectDist = Vec2F(std::abs(blockDistance[0]) * deltaDistX, std::abs(blockDistance[1]) * deltaDistY);

    Vec2I normal = Vec2I(intersectDist[0] > intersectDist[1] ? 1 : 0, intersectDist[1] > intersectDist[0] ? 1 : 0);
    normal[0] *= std::copysign(1.0f, direction[0]);
    normal[1] *= std::copysign(1.0f, direction[1]);
    Vec2F position = begin + (direction * std::max(intersectDist[0], intersectDist[1]));
    // HACK: Keeps the end position from being right on a tile border, making line check
    // *from* this end position from colliding with the first tile
    if (intersectDist[0] > intersectDist[1])
      position[0] = blockPosition[0] + (normal[0] * -0.0001f);
    else
      position[1] = blockPosition[1] + (normal[1] * -0.0001f);
    return std::pair<Vec2F, Vec2I>(position, normal);
  }

  template <typename TileSectorArray>
  [[nodiscard]]
  inline std::expected<LiquidLevel, StarException> liquidLevel(std::shared_ptr<TileSectorArray> const& tileSectorArray, RectF const& region) {
    if (region.isEmpty())
      return LiquidLevel();

    RectI sampleRect = RectI::integral(region);
    // This is not entirely accurate, even though all liquid types in the
    // region count towards the grand total liquid percentage, only the most
    // common liquid is reported.
    float totalSpace = 0;
    Map<LiquidId, float> totals;
    auto sampled = tileSectorArray->tileEach(sampleRect, [&region, &totalSpace, &totals](Vec2I const& pos, typename TileSectorArray::Tile const& tile) -> std::expected<void, StarException> {
        float blockIncidence = RectF(pos[0], pos[1], pos[0] + 1, pos[1] + 1).overlap(region).volume();
        totalSpace += blockIncidence;
        auto liquidLevel = tile.liquid;
        if (liquidLevel.liquid != EmptyLiquidId)
          totals[liquidLevel.liquid] += std::min(1.0f, liquidLevel.level) * blockIncidence;
        return {};
      });
    if (!sampled)
      return std::unexpected(std::move(sampled.error()));

    float totalLiquidLevel = 0.0f;
    float maximumLiquidLevel = 0.0f;
    LiquidId maximumLiquidId = EmptyLiquidId;
    for (auto const& p : totals) {
      totalLiquidLevel += p.second;
      if (p.second > maximumLiquidLevel) {
        maximumLiquidLevel = p.second;
        maximumLiquidId = p.first;
      }
    }
    return LiquidLevel(maximumLiquidId, totalLiquidLevel / totalSpace);
  }

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<List<Vec2I>, StarException> collidingTilesAlongLine(WorldGeometry const& worldGeometry, std::shared_ptr<TileSectorArray> const& tileSectorArray,
      Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet, std::size_t maxSize, bool includeEdges) {
    List<Vec2I> res;
    auto traversed = forBlocksAlongLine<float>(begin, worldGeometry.diff(end, begin), [&](int x, int y) -> std::expected<bool, StarException> {
        if (res.size() >= maxSize)
          return false;
        if (tileSectorArray->tile({x, y}).isColliding(collisionSet))
          res.push_back({x, y});
        return true;
      });
    if (!traversed)
      return std::unexpected(std::move(traversed.error()));

    if (includeEdges)
      return res;

    for (auto tileIt = res.begin(); tileIt != res.end();) {
      auto tileRect = RectF::withSize(Vec2F(worldGeometry.xwrap(*tileIt)), Vec2F::filled(1.0f));
      Line2F line{worldGeometry.xwrap(begin), worldGeometry.xwrap(end)};
      auto lineSet = worldGeometry.splitLine(line);
      if (!lineSet)
        return std::unexpected(std::move(lineSet.error()));
      if (std::ranges::any_of(*lineSet, [&](Line2F const& line) { return tileRect.edgeIntersection(line).glances; }))
        tileIt = res.erase(tileIt);
      else
        ++tileIt;
    }
    return res;
  }

  inline std::expected<TileDamageParameters, StarException> tileDamageParameters(Root& root, WorldTile& tile, TileLayer layer, TileDamage const& tileDamage) {
    bool foreground = layer == TileLayer::Foreground;
    auto materialDatabase = root.materialDatabase();
    if (!materialDatabase)
      return std::unexpected(std::move(materialDatabase.error()));
    auto target = foreground ? tile.foreground : tile.background;
    auto mod = foreground ? tile.foregroundMod : tile.backgroundMod;
    if (!isRealMod(mod) || tileDamageIsPenetrating(tileDamage.type))
      return (*materialDatabase)->materialDamageParameters(target);
    auto breaksWithTile = (*materialDatabase)->modBreaksWithTile(mod);
    if (!breaksWithTile)
      return std::unexpected(std::move(breaksWithTile.error()));
    auto modDamage = (*materialDatabase)->modDamageParameters(mod);
    if (!modDamage)
      return std::unexpected(std::move(modDamage.error()));
    if (!*breaksWithTile)
      return std::move(*modDamage);
    auto materialDamage = (*materialDatabase)->materialDamageParameters(target);
    if (!materialDamage)
      return std::unexpected(std::move(materialDamage.error()));
    return modDamage->sum(*materialDamage);
  }

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> damageWouldDestroy(Root& root, std::shared_ptr<TileSectorArray> const& tileSectorArray, Vec2I pos, TileLayer layer, TileDamage const& tileDamage) {
    if (auto tile = tileSectorArray->modifyTile(pos)) {
      auto damageParameters = tileDamageParameters(root, tile->get(), layer, tileDamage);
      if (!damageParameters)
        return std::unexpected(std::move(damageParameters.error()));
      auto damageDone = damageParameters->damageDone(tileDamage);
      if (!damageDone)
        return std::unexpected(std::move(damageDone.error()));
      float percentageDelta = *damageDone / damageParameters->totalHealth();
      auto damage = layer == TileLayer::Foreground ? tile->get().foregroundDamage : tile->get().backgroundDamage;
      return percentageDelta + damage.damagePercentage() >= 1.0f;
    }

    return false;
  }

  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<bool, StarException> canPlaceMaterial(Root& root, std::shared_ptr<EntityMap> const& entityMap,
      Vec2I const& pos, TileLayer layer, MaterialId material, bool allowEntityOverlap, bool allowTileOverlap, GetTileFunction& getTile) {
    auto materialDatabase = root.materialDatabase();
    if (!materialDatabase)
      return std::unexpected(std::move(materialDatabase.error()));
    if (!isRealMaterial(material))
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
            if (isConnectableMaterial(foreground ? tile->get().foreground : tile->get().background))
              return true;
          }
        }
      }
      return false;
    };

    auto canPlace = (*materialDatabase)->canPlaceInLayer(material, layer);
    if (!canPlace)
      return std::unexpected(std::move(canPlace.error()));
    if (!*canPlace)
      return false;
    auto tile = getTile(pos);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    if (layer == TileLayer::Background) {
      if (tile->get().background != EmptyMaterialId && tile->get().background != ObjectPlatformMaterialId)
        return false;
      auto adjacent = isAdjacentToConnectable(pos, 1, false);
      if (!adjacent)
        return std::unexpected(std::move(adjacent.error()));
      if (!*adjacent && !isConnectableMaterial(tile->get().foreground))
        return false;
    } else {
      if (tile->get().foreground != EmptyMaterialId && tile->get().foreground != ObjectPlatformMaterialId)
        return false;
      if (!allowTileOverlap) {
        auto occupied = entityMap->tileIsOccupied(pos);
        if (!occupied)
          return std::unexpected(std::move(occupied.error()));
        if (*occupied)
          return false;
      }
      if (!allowEntityOverlap) {
        auto occupied = entityMap->spaceIsOccupied(RectF::withSize(Vec2F(pos), Vec2F(0.999f, 0.999f)));
        if (!occupied)
          return std::unexpected(std::move(occupied.error()));
        if (*occupied)
          return false;
      }
      auto adjacent = isAdjacentToConnectable(pos, 1, true);
      if (!adjacent)
        return std::unexpected(std::move(adjacent.error()));
      if (!*adjacent && !isConnectableMaterial(tile->get().background))
        return false;
    }
    return true;
  }

  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<bool, StarException> perhapsCanPlaceMaterial(Root& root, std::shared_ptr<EntityMap> const& entityMap,
      Vec2I const& pos, TileLayer layer, MaterialId material, bool allowEntityOverlap, bool allowTileOverlap, GetTileFunction& getTile) {
    auto materialDatabase = root.materialDatabase();
    if (!materialDatabase)
      return std::unexpected(std::move(materialDatabase.error()));
    if (!isRealMaterial(material))
      return false;
    auto canPlace = (*materialDatabase)->canPlaceInLayer(material, layer);
    if (!canPlace)
      return std::unexpected(std::move(canPlace.error()));
    if (!*canPlace)
      return false;
    auto tile = getTile(pos);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    if (layer == TileLayer::Background)
      return tile->get().background == EmptyMaterialId || tile->get().background == ObjectPlatformMaterialId;
    if (tile->get().foreground != EmptyMaterialId && tile->get().foreground != ObjectPlatformMaterialId)
      return false;
    if (!allowTileOverlap) {
      auto occupied = entityMap->tileIsOccupied(pos);
      if (!occupied)
        return std::unexpected(std::move(occupied.error()));
      if (*occupied)
        return false;
    }
    if (!allowEntityOverlap) {
      auto occupied = entityMap->spaceIsOccupied(RectF::withSize(Vec2F(pos), Vec2F(0.999f, 0.999f)));
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
    auto materialDatabase = root.materialDatabase();
    if (!materialDatabase)
      return std::unexpected(std::move(materialDatabase.error()));
    auto tile = getTile(pos);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    auto existingColor = tile->get().materialColor(layer);
    auto existingHue = layer == TileLayer::Foreground ? tile->get().foregroundHueShift : tile->get().backgroundHueShift;
    if (existingHue != 0)
      return true;
    if (existingColor == color)
      return false;
    return (*materialDatabase)->isMultiColor(tile->get().material(layer));
  }

  template <typename GetTileFunction>
  [[nodiscard]]
  std::expected<bool, StarException> canPlaceMod(Root& root, Vec2I const& pos, TileLayer layer, ModId mod, GetTileFunction& getTile) {
    if (!isRealMod(mod))
      return false;
    auto materialDatabase = root.materialDatabase();
    if (!materialDatabase)
      return std::unexpected(std::move(materialDatabase.error()));
    auto tile = getTile(pos);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    if (tile->get().mod(layer) == mod)
      return false;
    return (*materialDatabase)->supportsMod(tile->get().material(layer), mod);
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
      if (!isRealMaterial(placeMaterial.material))
        return false;
      auto materialDatabase = root.materialDatabase();
      if (!materialDatabase)
        return std::unexpected(std::move(materialDatabase.error()));
      return (*materialDatabase)->canPlaceInLayer(placeMaterial.material, placeMaterial.layer);
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
    if (tile.material(TileLayer::Background) != EmptyMaterialId)
      return 0.0f;
    if (tile.material(TileLayer::Foreground) != EmptyMaterialId)
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
      auto collision = world.pointCollision(pos);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      foregroundBreathable = !*collision;
    }
    return environmentBreathable && foregroundBreathable && liquidBreathable;
  }

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<float, StarException> lightLevel(Root& root, std::shared_ptr<TileSectorArray> const& tileSectorArray, std::shared_ptr<EntityMap> const& entityMap, WorldGeometry const& worldGeometry,
      std::shared_ptr<WorldTemplate const> const& worldTemplate, std::shared_ptr<Sky const> const& sky, CellularLightIntensityCalculator& lighting, Vec2F pos) {
    if (pos[1] < 0 || pos[1] >= worldGeometry.height())
      return 0;

    // tileEach can't handle rects that are WAY out of range.
    pos = worldGeometry.xwrap(pos);

    Vec3F environmentLight = sky->environmentLight().toRgbF();
    float undergroundLevel = worldTemplate->undergroundLevel();
    auto materialDatabase = root.materialDatabase();
    if (!materialDatabase)
      return std::unexpected(std::move(materialDatabase.error()));
    auto liquidsDatabase = root.liquidsDatabase();
    if (!liquidsDatabase)
      return std::unexpected(std::move(liquidsDatabase.error()));

    lighting.begin(pos);

    // Each column in tileEvalColumns is guaranteed to be no larger than the
    // sector size.
    CellularLightIntensityCalculator::Cell lightingCellColumn[WorldSectorSize];
    auto evaluated = tileSectorArray->tileEvalColumns(lighting.calculationRegion(), [&](Vec2I const& pos, std::span<typename TileSectorArray::Tile> column) -> std::expected<void, StarException> {
        for (std::size_t y = 0; y < column.size(); ++y) {
          auto& tile = column[y];
          auto& cell = lightingCellColumn[y];

          auto backgroundTransparent = (*materialDatabase)->backgroundLightTransparent(tile.background);
          if (!backgroundTransparent)
            return std::unexpected(std::move(backgroundTransparent.error()));
          auto foregroundTransparency = (*materialDatabase)->foregroundLightTransparent(tile.foreground);
          if (!foregroundTransparency)
            return std::unexpected(std::move(foregroundTransparency.error()));
          bool foregroundTransparent = *foregroundTransparency && tile.getCollision() != CollisionKind::Dynamic;

          cell = {(*materialDatabase)->radiantLight(tile.foreground, tile.foregroundMod).sum() / 3.0f, !foregroundTransparent};
          cell.light += (*liquidsDatabase)->radiantLight(tile.liquid).sum() / 3.0f;
          if (foregroundTransparent) {
            cell.light += (*materialDatabase)->radiantLight(tile.background, tile.backgroundMod).sum() / 3.0f;
            if (*backgroundTransparent && pos[1] > undergroundLevel)
              cell.light += environmentLight.sum() / 3.0f;
          }
        }
        lighting.setCellColumn(pos, std::span(lightingCellColumn).first(column.size()));
        return {};
      });

    if (!evaluated)
      return std::unexpected(std::move(evaluated.error()));
    auto entities = entityMap->entityQuery(RectF(lighting.calculationRegion()));
    if (!entities)
      return std::unexpected(std::move(entities.error()));
    for (auto const& entity : *entities) {
      auto lights = entity->lightSources();
      if (!lights)
        return std::unexpected(std::move(lights.error()));
      for (auto const& light : *lights) {
        Vec2F position = worldGeometry.nearestTo(Vec2F(lighting.calculationRegion().min()), light.position);
        if (light.type == LightType::Spread)
          lighting.addSpreadLight(position, light.color.sum() / 3.0f);
        else
          lighting.addPointLight(position, light.color.sum() / 3.0f, light.pointBeam, light.beamAngle, light.beamAmbience);
      }
    }

    return lighting.calculate();
  }

  inline std::expected<std::shared_ptr<InteractiveEntity>, StarException> getInteractiveInRange(WorldGeometry const& geometry, std::shared_ptr<EntityMap> const& entityMap,
      Vec2F const& targetPosition, Vec2F const& sourcePosition, float maxRange) {
    auto entity = entityMap->interactiveEntityNear(targetPosition, maxRange);
    if (!entity)
      return std::unexpected(std::move(entity.error()));
    if (*entity) {
      if (auto tileEntity = as<TileEntity>(*entity)) {
        auto inRange = isTileEntityInRange(geometry, entityMap, tileEntity->entityId(), sourcePosition, maxRange);
        if (!inRange)
          return std::unexpected(std::move(inRange.error()));
        if (*inRange)
          return std::move(*entity);
      } else if (geometry.diffToNearestCoordInBox((*entity)->interactiveBoundBox().translated((*entity)->position()), sourcePosition).magnitude() <= maxRange) {
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
        if (geometry.diff(entity->position() + centerOfTile(space), sourcePosition).magnitude() <= maxRange)
          return true;
      }
    }

    return false;
  }

  template <typename TileSectorArray>
  [[nodiscard]]
  std::expected<bool, StarException> canReachEntity(WorldGeometry const& geometry, std::shared_ptr<TileSectorArray> const& tileSectorArray,
      std::shared_ptr<EntityMap> const& entityMap, Vec2F const& sourcePosition, float maxRange, EntityId targetEntity, bool preferInteractive) {
    auto entity = entityMap->entity(targetEntity);
    if (!entity)
      return false;

    // exclude the final tile from the collision check since many targets will be collidable tile entities, e.g. doors
    auto const canReachTile = [&](Vec2F const& end) -> std::expected<bool, StarException> {
      Vec2I const& endTile = Vec2I::floor(end);
      return forBlocksAlongLine<float>(sourcePosition, geometry.diff(end, sourcePosition), [=](int x, int y) -> std::expected<bool, StarException> {
          Vec2I diff = geometry.diff(endTile, Vec2I(x, y));
          if (diff[0] == 0 && diff[1] == 0)
            return true;
          return !tileSectorArray->tile({x, y}).isColliding(DefaultCollisionSet);
        });
    };

    if (auto tileEntity = as<TileEntity>(entity)) {
      for (auto space : preferInteractive ? tileEntity->interactiveSpaces() : tileEntity->spaces()) {
        auto spacePosition = entity->position() + centerOfTile(space);
        if (geometry.diff(spacePosition, sourcePosition).magnitude() <= maxRange) {
          auto reachable = canReachTile(spacePosition);
          if (!reachable)
            return std::unexpected(std::move(reachable.error()));
          if (*reachable)
            return true;
        }
      }
    } else if (preferInteractive && is<InteractiveEntity>(entity)) {
      auto interactiveEntity = as<InteractiveEntity>(entity);
      auto entityBounds = interactiveEntity->interactiveBoundBox().translated(entity->position());
      auto contains = geometry.rectContains(entityBounds, sourcePosition);
      if (!contains)
        return std::unexpected(std::move(contains.error()));
      if (*contains)
        return true;
      auto intersects = geometry.rectIntersectsCircle(entityBounds, sourcePosition, maxRange);
      if (!intersects)
        return std::unexpected(std::move(intersects.error()));
      if (!*intersects)
        return false;

      auto collision = lineTileCollision(geometry, tileSectorArray, sourcePosition, entityBounds.nearestCoordTo(sourcePosition), DefaultCollisionSet);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      if (!*collision)
        return true;
      collision = lineTileCollision(geometry, tileSectorArray, sourcePosition, Vec2F(entityBounds.xMin(), entityBounds.yMin()), DefaultCollisionSet);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      if (!*collision)
        return true;
      collision = lineTileCollision(geometry, tileSectorArray, sourcePosition, Vec2F(entityBounds.xMin(), entityBounds.yMax()), DefaultCollisionSet);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      if (!*collision)
        return true;
      collision = lineTileCollision(geometry, tileSectorArray, sourcePosition, Vec2F(entityBounds.xMax(), entityBounds.yMax()), DefaultCollisionSet);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      if (!*collision)
        return true;
      collision = lineTileCollision(geometry, tileSectorArray, sourcePosition, Vec2F(entityBounds.xMax(), entityBounds.yMin()), DefaultCollisionSet);
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      return !*collision;
    } else {
      if (geometry.diff(entity->position(), sourcePosition).magnitude() <= maxRange) {
        auto collision = lineTileCollision(geometry, tileSectorArray, sourcePosition, entity->position(), DefaultCollisionSet);
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
  using ::Star::WorldImpl::tileIsOccupied;
  using ::Star::WorldImpl::tileCollisionKind;
  using ::Star::WorldImpl::rectTileCollision;
  using ::Star::WorldImpl::lineTileCollision;
  using ::Star::WorldImpl::lineTileCollisionPoint;
  using ::Star::WorldImpl::collidingTilesAlongLine;
  using ::Star::WorldImpl::tileDamageParameters;
  using ::Star::WorldImpl::damageWouldDestroy;
  using ::Star::WorldImpl::canPlaceMaterial;
  using ::Star::WorldImpl::perhapsCanPlaceMaterial;
  using ::Star::WorldImpl::canPlaceMaterialColorVariant;
  using ::Star::WorldImpl::canPlaceMod;
  using ::Star::WorldImpl::validateTileModification;
  using ::Star::WorldImpl::validateTileReplacement;
  using ::Star::WorldImpl::splitTileModifications;
  using ::Star::WorldImpl::windLevel;
  using ::Star::WorldImpl::breathable;
  using ::Star::WorldImpl::lightLevel;
  using ::Star::WorldImpl::getInteractiveInRange;
  using ::Star::WorldImpl::isTileEntityInRange;
  using ::Star::WorldImpl::canReachEntity;
  using ::Star::WorldImpl::liquidLevel;
}
