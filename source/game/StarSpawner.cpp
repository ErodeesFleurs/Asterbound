import star.json;
import star.json_extra;
import star.random;
import star.periodic;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.ttl_cache;
import star.poly;
import star.game_types;
import star.list;
import star.bi_map;
import star.data_stream;
import star.casting;
import star.rpc_promise;
import star.strong_typedef;
import star.vector;
import star.rect;
import star.a_star;
import star.ordered_set;
import star.audio;
import star.map;
import star.set;
import star.color;
import star.string;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.either;
import star.periodic_function;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.format;
import star.version;
import star.lua;
import star.weighted_pool;
import star.logging;

import star.world_geometry;
import star.collision_block;
import star.uuid;
import star.item_descriptor;
import star.drawable;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.damage_types;
import star.status_types;
import star.damage;
import star.entity;
import star.interaction_types;
import star.tile_damage;
import star.interactive_entity;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inspectable_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.game_timers;
import star.actor_movement_controller;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.asset_source;
import star.assets;

import star.configuration;
import star.root;
import star.aggressive_entity;
import star.scripted_entity;
import star.effect_emitter;
import star.monster;

import star.spawn_type_database;
import star.spawner;
import star.monster_database;

import star.static_random;
import star.algorithm;
namespace Star {

std::expected<std::shared_ptr<Spawner>, StarException> Spawner::create(Root& root) {
  auto result = std::shared_ptr<Spawner>(new Spawner(root));
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/spawning.config");
  if (!config) return std::unexpected(std::move(config.error()));
  {
    auto value = config->getUInt("spawnCellSize");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCellSize = std::move(*value);
  }
  {
    auto value = config->getUInt("spawnCellMinimumEmptyTiles");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCellMinimumEmptyTiles = std::move(*value);
  }
  {
    auto value = config->getUInt("spawnCellMinimumLiquidTiles");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCellMinimumLiquidTiles = std::move(*value);
  }
  {
    auto value = config->getUInt("spawnCellMinimumNearSurfaceTiles");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCellMinimumNearSurfaceTiles = std::move(*value);
  }
  {
    auto value = config->getUInt("spawnCellMinimumNearCeilingTiles");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCellMinimumNearCeilingTiles = std::move(*value);
  }
  {
    auto value = config->getUInt("spawnCellMinimumAirTiles");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCellMinimumAirTiles = std::move(*value);
  }
  {
    auto value = config->getUInt("spawnCellMinimumExposedTiles");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCellMinimumExposedTiles = std::move(*value);
  }
  {
    auto value = config->getUInt("spawnCellNearSurfaceDistance");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCellNearSurfaceDistance = std::move(*value);
  }
  {
    auto value = config->getUInt("spawnCellNearCeilingDistance");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCellNearCeilingDistance = std::move(*value);
  }
  {
    auto value = config->getFloat("minimumDayLevel");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_minimumDayLevel = std::move(*value);
  }
  {
    auto value = config->getFloat("minimumLiquidLevel");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_minimumLiquidLevel = std::move(*value);
  }
  {
    auto value = config->getFloat("spawnCheckResolution");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCheckResolution = std::move(*value);
  }
  {
    auto value = config->getInt("spawnSurfaceCheckDistance");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnSurfaceCheckDistance = std::move(*value);
  }
  {
    auto value = config->getInt("spawnCeilingCheckDistance");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCeilingCheckDistance = std::move(*value);
  }
  {
    auto value = config->getFloat("spawnProhibitedCheckPadding");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnProhibitedCheckPadding = std::move(*value);
  }
  {
    auto value = config->getFloat("spawnCellLifetime");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_spawnCellLifetime = std::move(*value);
  }
  {
    auto value = config->getUInt("windowActivationBorder");
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_windowActivationBorder = std::move(*value);
  }
  {
    auto value = config->getBool("defaultActive", true);
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_active = std::move(*value);
  }
  {
    auto value = config->getBool("debug", false);
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_debug = std::move(*value);
  }
  return result;
}

void Spawner::init(std::shared_ptr<SpawnerFacade> facade) {
  m_facade = std::move(facade);
}

std::expected<void, StarException> Spawner::uninit() {
  for (auto entityId : m_spawnedEntities) {
    auto despawned = m_facade->despawnEntity(entityId);
    if (!despawned) return std::unexpected(std::move(despawned.error()));
  }
  m_facade.reset();
  return {};
}

bool Spawner::active() const {
  return m_active;
}

void Spawner::setActive(bool active) {
  m_active = active;
}

std::expected<void, StarException> Spawner::activateRegion(RectF region) {
  auto cells = cellIndexesForRange(region);
  if (!cells) return std::unexpected(std::move(cells.error()));
  for (auto const& cell : *cells) {
    if (m_facade) {
      auto ready = m_facade->signalRegion(cellRegion(cell));
      if (!ready) return std::unexpected(std::move(ready.error()));
      if (*ready) {
        if (m_active && !m_activeSpawnCells.contains(cell)) {
          auto spawned = spawnInCell(cell);
          if (!spawned) return std::unexpected(std::move(spawned.error()));
        }
        m_activeSpawnCells[cell] = m_spawnCellLifetime;
      }
    }
  }
  return {};
}

std::expected<void, StarException> Spawner::activateEmptyRegion(RectF region) {
  auto cells = cellIndexesForRange(region);
  if (!cells) return std::unexpected(std::move(cells.error()));
  for (auto const& cell : *cells) m_activeSpawnCells[cell] = m_spawnCellLifetime;
  return {};
}

std::expected<void, StarException> Spawner::update(float dt) {
  if (!m_facade) return {};
  for (auto const& window : m_facade->clientWindows()) {
    if (window != RectF()) {
      auto activated = activateRegion(window.padded(m_windowActivationBorder));
      if (!activated) return std::unexpected(std::move(activated.error()));
    }
  }
  eraseWhere(m_activeSpawnCells, [dt](auto&& p) { return (p.second -= dt) < 0.0f; });
  for (auto it = m_spawnedEntities.begin(); it != m_spawnedEntities.end();) {
    auto entity = m_facade->getEntity(*it);
    if (!entity) it = m_spawnedEntities.erase(it);
    else if (!m_activeSpawnCells.contains(cellIndexForPosition(entity->position()))) {
      auto despawned = m_facade->despawnEntity(entity->entityId());
      if (!despawned) return std::unexpected(std::move(despawned.error()));
      it = m_spawnedEntities.erase(it);
    } else ++it;
  }
  if (m_active && m_debug) return debugShowSpawnCells();
  return {};
}

Vec2I Spawner::cellIndexForPosition(Vec2F const& position) const {
  return Vec2I::floor(position / m_spawnCellSize);
}

std::expected<List<Vec2I>, StarException> Spawner::cellIndexesForRange(RectF const& range) const {
  List<Vec2I> cellIndexes;
  auto ranges = m_facade->geometry().splitRect(range);
  if (!ranges) return std::unexpected(std::move(ranges.error()));
  for (auto const& srange : *ranges) {
    auto indexes = RectI::integral(RectF(srange).scaled(1.0f / m_spawnCellSize));
    for (int x = indexes.xMin(); x < indexes.xMax(); ++x) {
      for (int y = indexes.yMin(); y < indexes.yMax(); ++y)
        cellIndexes.push_back({x, y});
    }
  }

  return cellIndexes;
}

RectF Spawner::cellRegion(Vec2I const& cellIndex) const {
  return RectF::withSize(Vec2F(cellIndex) * m_spawnCellSize, Vec2F::filled(m_spawnCellSize));
}

std::expected<std::optional<SpawnParameters>, StarException> Spawner::spawnParametersForCell(Vec2I const& cellIndex) const {
  unsigned emptyCount = 0;
  unsigned nearSurfaceCount = 0;
  unsigned nearCeilingCount = 0;
  unsigned airCount = 0;
  unsigned liquidCount = 0;
  unsigned exposedCount = 0;

  auto region = RectI::withSize(cellIndex * m_spawnCellSize, Vec2I::filled(m_spawnCellSize));
  for (int x = region.xMin(); x < region.xMax(); ++x) {
    for (int y = region.yMin(); y < region.yMax(); ++y) {
      // Only empty blocks count towards spawn totals
      auto collision = m_facade->collision({x, y});
      if (!collision) return std::unexpected(std::move(collision.error()));
      if (*collision == CollisionKind::None) {
        ++emptyCount;

        if (m_facade->liquidLevel({x, y}).level > m_minimumLiquidLevel)
          ++liquidCount;

        auto backgroundEmpty = m_facade->isBackgroundEmpty({x, y});
        if (!backgroundEmpty) return std::unexpected(std::move(backgroundEmpty.error()));
        if (*backgroundEmpty) ++exposedCount;

        // The empty block will will either count as an air block, a
        // "near-surface" block, or a "near-ceiling" block.  It will count as a
        // near-surface block if it is within the NearSurfaceDistance of a
        // CollsionKind::Block or CollisionKind::Platform block. If it is not a
        // near-surface block, it will count as a near-ceiling block if it is
        // within the NearCeilingDistance of a CollisionKind::Block.
        bool nearSurface = false;
        for (unsigned sd = 1; sd <= m_spawnCellNearSurfaceDistance; ++sd) {
          auto collision = m_facade->collision({x, y - sd});
          if (!collision) return std::unexpected(std::move(collision.error()));
          if (BlockCollisionSet.contains(*collision) || *collision == CollisionKind::Platform) {
            nearSurface = true;
            break;
          }
        }

        bool nearCeiling = false;
        if (!nearSurface) {
          for (unsigned cd = 1; cd <= m_spawnCellNearCeilingDistance; ++cd) {
            auto collision = m_facade->collision({x, y + cd});
            if (!collision) return std::unexpected(std::move(collision.error()));
            if (BlockCollisionSet.contains(*collision)) {
              nearCeiling = true;
              break;
            }
          }
        }

        if (nearSurface)
          ++nearSurfaceCount;
        else if (nearCeiling)
          ++nearCeilingCount;
        else
          ++airCount;
      }
    }
  }

  Set<SpawnParameters::Area> spawnAreas;
  if (liquidCount > m_spawnCellMinimumLiquidTiles)
    spawnAreas.add(SpawnParameters::Area::Liquid);
  if (nearSurfaceCount > m_spawnCellMinimumNearSurfaceTiles)
    spawnAreas.add(SpawnParameters::Area::Surface);
  if (nearCeilingCount > m_spawnCellMinimumNearCeilingTiles)
    spawnAreas.add(SpawnParameters::Area::Ceiling);
  if (airCount > m_spawnCellMinimumAirTiles)
    spawnAreas.add(SpawnParameters::Area::Air);
  if (emptyCount < m_spawnCellMinimumEmptyTiles)
    spawnAreas.add(SpawnParameters::Area::Solid);

  if (spawnAreas.empty())
    return std::optional<SpawnParameters>();

  SpawnParameters::Region spawnRegion = SpawnParameters::Region::Enclosed;
  if (exposedCount >= m_spawnCellMinimumExposedTiles)
    spawnRegion = SpawnParameters::Region::Exposed;

  SpawnParameters::Time spawnTime = SpawnParameters::Time::Night;
  if (m_facade->dayLevel() >= m_minimumDayLevel)
    spawnTime = SpawnParameters::Time::Day;

  return SpawnParameters(spawnAreas, spawnRegion, spawnTime);
}

std::expected<std::optional<Vec2F>, StarException> Spawner::adjustSpawnRegion(RectF const& spawnRegion, RectF const& boundBox, SpawnParameters const& spawnParameters) const {
  auto checkPosition = [&self = *this, &boundBox, &spawnParameters](Vec2F const& position) -> std::expected<bool, StarException> {
    RectF region = RectF(boundBox).translated(position);

    auto free = self.m_facade->isFreeSpace(region);
    if (!free) return std::unexpected(std::move(free.error()));
    if (!*free) return spawnParameters.areas.contains(SpawnParameters::Area::Solid);

    if (self.m_facade->liquidLevel(Vec2I::floor(region.center())).level >= self.m_minimumLiquidLevel)
      return spawnParameters.areas.contains(SpawnParameters::Area::Liquid);

    auto prohibited = self.m_facade->spawningProhibited(region.padded(self.m_spawnProhibitedCheckPadding));
    if (!prohibited) return std::unexpected(std::move(prohibited.error()));
    if (*prohibited) return false;

    if (spawnParameters.areas.contains(SpawnParameters::Area::Air))
      return true;

    if (spawnParameters.areas.contains(SpawnParameters::Area::Surface)) {
      Vec2F startCheck = {region.center()[0], region.yMin()};
      for (int sd = 0; sd <= self.m_spawnSurfaceCheckDistance; ++sd) {
        auto collision = self.m_facade->collision(Vec2I::floor(startCheck - Vec2F(0, sd)));
        if (!collision) return std::unexpected(std::move(collision.error()));
        if (BlockCollisionSet.contains(*collision) || *collision == CollisionKind::Platform)
          return true;
      }

    } else if (spawnParameters.areas.contains(SpawnParameters::Area::Ceiling)) {
      Vec2F startCheck = {region.center()[0], region.yMax()};
      for (int cd = 0; cd <= self.m_spawnCeilingCheckDistance; ++cd) {
        auto collision = self.m_facade->collision(Vec2I::floor(startCheck + Vec2F(0, cd)));
        if (!collision) return std::unexpected(std::move(collision.error()));
        if (BlockCollisionSet.contains(*collision))
          return true;
      }
    }

    return false;
  };

  List<Vec2F> tryPositions;
  for (float x = spawnRegion.xMin(); x <= spawnRegion.xMax(); x += m_spawnCheckResolution) {
    for (float y = spawnRegion.yMin(); y <= spawnRegion.yMax(); y += m_spawnCheckResolution)
      tryPositions.push_back({x, y});
  }

  this->root().random().shuffle(tryPositions);
  for (auto const& p : tryPositions) {
    auto valid = checkPosition(p);
    if (!valid) return std::unexpected(std::move(valid.error()));
    if (*valid) return p;
  }

  return std::optional<Vec2F>();
}

std::expected<void, StarException> Spawner::spawnInCell(Vec2I const& cell) {
  auto cellParameters = spawnParametersForCell(cell);
  if (!cellParameters) return std::unexpected(std::move(cellParameters.error()));
  if (!*cellParameters) return {};
  if (m_debug) m_debugSpawnInfo[cell] = SpawnCellDebugInfo{**cellParameters, 0, 0};
  auto monsters = root().monsterDatabase();
  if (!monsters) return std::unexpected(std::move(monsters.error()));
  auto types = root().spawnTypeDatabase();
  if (!types) return std::unexpected(std::move(types.error()));
  RectF region = cellRegion(cell);
  auto profile = m_facade->spawnProfile(region.center());
  if (!profile) return std::unexpected(std::move(profile.error()));
  for (auto const& name : profile->spawnTypes) {
    auto type = (*types)->spawnType(name);
    if (!type) return std::unexpected(std::move(type.error()));
    if (!type->spawnParameters.compatible(**cellParameters)) continue;
    if (root().random().randf() < type->spawnChance) {
      std::uint64_t seed = static_random_u64(type->seedMix, m_facade->spawnSeed());
      auto groupSize = root().random().randInt(type->groupSize[0], type->groupSize[1]);
      if (!groupSize) return std::unexpected(std::move(groupSize.error()));
      for (int i = 0; i < *groupSize; ++i) {
        String monsterType;
        if (std::holds_alternative<WeightedPool<String>>(type->monsterType)) monsterType = std::get<WeightedPool<String>>(type->monsterType).select(root().random());
        else monsterType = std::get<String>(type->monsterType);
        auto variant = (*monsters)->monsterVariant(monsterType, seed, type->monsterParameters);
        if (!variant) return std::unexpected(std::move(variant.error()));
        if (!variant->movementSettings.standingPoly)
          return std::unexpected(MonsterException("Monster has no standing polygon"));
        auto box = variant->movementSettings.standingPoly->boundBox();
        if (m_debug) m_debugSpawnInfo[cell].spawnAttempts++;
        auto position = adjustSpawnRegion(region, box, type->spawnParameters);
        if (!position) return std::unexpected(std::move(position.error()));
        if (*position) {
          auto threat = m_facade->threatLevel();
          if (!threat) return std::unexpected(std::move(threat.error()));
          float level = *threat;
          if (m_facade->dayLevel() >= m_minimumDayLevel) {
            auto adjustment = root().random().randf(type->dayLevelAdjustment[0], type->dayLevelAdjustment[1]);
            if (!adjustment) return std::unexpected(std::move(adjustment.error()));
            level += *adjustment;
          } else {
            auto adjustment = root().random().randf(type->nightLevelAdjustment[0], type->nightLevelAdjustment[1]);
            if (!adjustment) return std::unexpected(std::move(adjustment.error()));
            level += *adjustment;
          }
          auto profile = m_facade->spawnProfile(**position);
          if (!profile) return std::unexpected(std::move(profile.error()));
          auto entity = (*monsters)->createMonster(*variant, level, profile->monsterParameters);
          if (!entity) return std::unexpected(std::move(entity.error()));
          (*entity)->setPosition(**position);
          (*entity)->setKeepAlive(true);
          auto id = m_facade->spawnEntity(*entity);
          if (!id) return std::unexpected(std::move(id.error()));
          if (*id != NullEntityId) m_spawnedEntities.add(*id);
          if (m_debug) m_debugSpawnInfo[cell].spawns++;
        }
      }
    }
  }
  return {};
}

std::expected<void, StarException> Spawner::debugShowSpawnCells() {
  eraseWhere(m_debugSpawnInfo, [&cells = m_activeSpawnCells](auto&& p) { return !cells.contains(p.first); });
  auto visible = [&facade = *m_facade](RectF const& region) -> std::expected<bool, StarException> {
    for (auto const& window : facade.clientWindows()) {
      auto intersects = facade.geometry().rectIntersectsRect(window, region);
      if (!intersects) return std::unexpected(std::move(intersects.error()));
      if (*intersects) return true;
    }
    return false;
  };
  for (auto const& info : m_debugSpawnInfo) {
    RectF region = cellRegion(info.first);
    auto isVisible = visible(region);
    if (!isVisible) return std::unexpected(std::move(isVisible.error()));
    if (*isVisible) {
      root().spatialLogger().logPoly("world", PolyF(region), {128, 0, 0, 255});
      StringList areas;
      if (!SpawnParameters::AreaNames) return std::unexpected(SpawnParameters::AreaNames.error());
      for (auto area : info.second.spawnParameters.areas) {
        auto name = SpawnParameters::AreaNames->get_right(area);
        if (!name) return std::unexpected(std::move(name.error()));
        auto shortName = name->get().slice(0, 3);
        if (!shortName) return std::unexpected(std::move(shortName.error()));
        areas.push_back(std::move(*shortName));
      }
      root().spatialLogger().logText("world", Star::format("Areas: {}", areas.join(", ")), region.min() + Vec2F(0.5, 2.5), {255, 255, 255, 255});
      if (!SpawnParameters::RegionNames) return std::unexpected(SpawnParameters::RegionNames.error());
      auto regionName = SpawnParameters::RegionNames->get_right(info.second.spawnParameters.region);
      if (!regionName) return std::unexpected(std::move(regionName.error()));
      root().spatialLogger().logText("world", Star::format("Region: {}", regionName->get()), region.min() + Vec2F(0.5, 1.5), {255, 255, 255, 255});
      if (!SpawnParameters::TimeNames) return std::unexpected(SpawnParameters::TimeNames.error());
      auto timeName = SpawnParameters::TimeNames->get_right(info.second.spawnParameters.time);
      if (!timeName) return std::unexpected(std::move(timeName.error()));
      root().spatialLogger().logText("world", Star::format("Time: {}", timeName->get()), region.min() + Vec2F(0.5, 0.5), {255, 255, 255, 255});
      if (info.second.spawnAttempts > 0)
        root().spatialLogger().logText("world", Star::format("Spawns: {} / {}", info.second.spawns, info.second.spawnAttempts), region.min() + Vec2F(0.5, 3.5), (info.second.spawnAttempts > info.second.spawns) ? Color::Red.toRgba() : Color::Green.toRgba());
    }
  }
  return {};
}

}
