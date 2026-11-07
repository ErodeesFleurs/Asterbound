import std;
import star.xx_hash;
import star.json;
import star.id_map;
import star.vector;
import star.set;
import star.map;
import star.random;
import star.game_types;
import star.material_types;
import star.poly;
import star.list;
import star.bi_map;
import star.math_common;
import star.format;
import star.version;
import star.rect;
import star.spatial_hash_2d;
import star.casting;
import star.data_stream;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import star.logging;
import star.thread;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.string;
import star.image;
import star.ordered_map;
import star.lru_cache;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.rpc_promise;
import star.either;
import star.net_compatibility;
import star.a_star;
import star.lua;
import star.periodic_function;
import star.audio;
import star.io_device;
import star.ref_ptr;
import star.listener;
import star.periodic;
import star.time;
import star.ttl_cache;

import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.item_descriptor;
import star.drawable;
import star.item;
import star.item_recipe;

import star.dungeon_generator;
import star.inspectable_entity;
import star.plant;
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.cellular_liquid;
import star.plant_database;
import star.biome_placement;

import star.tile_modification;
import star.world;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.warping;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_structure;
import star.lua_components;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.world_render_data;
import star.world_client_state;
import star.interpolation_tracker;
import star.world_server;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.lounging_entities;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.npc_database;
import star.effect_emitter;
import star.npc;
import star.spawn_type_database;
import star.biome;
import star.world_template;
import star.wiring;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.wire_entity;
import star.item_drop;
import star.asset_source;
import star.assets;

import star.configuration;
import star.root;
import star.status_effect_entity;
import star.projectile;
import star.entity_rendering;
import star.object;
import star.container_entity;
import star.container_object;
import star.aggressive_entity;
import star.monster_database;
import star.monster;
import star.stagehand;
import star.vehicle;

import star.material_database;

import star.spawner;
import star.sky;

import star.falling_blocks_agent;
import star.entity_map;

import star.static_random;
import star.algorithm;
namespace Star {
namespace {

bool isPreservedTileCollisionOverride(CollisionKind collision) {
  return collision == CollisionKind::None
      || collision == CollisionKind::Platform
      || collision == CollisionKind::Block;
}

[[nodiscard]] std::expected<CollisionKind, StarException> biomeForegroundCollision(std::shared_ptr<MaterialDatabase const> const& materialDatabase, MaterialId oldForeground, MaterialId newForeground, CollisionKind currentCollision) {
  auto CollisionKindoldCollisionResult1 = materialDatabase->materialCollisionKind(oldForeground);
  if (!CollisionKindoldCollisionResult1) return std::unexpected(std::move(CollisionKindoldCollisionResult1.error()));
  CollisionKind oldCollision = std::move((*CollisionKindoldCollisionResult1));
  auto CollisionKindnewCollisionResult1 = materialDatabase->materialCollisionKind(newForeground);
  if (!CollisionKindnewCollisionResult1) return std::unexpected(std::move(CollisionKindnewCollisionResult1.error()));
  CollisionKind newCollision = std::move((*CollisionKindnewCollisionResult1));

  if (currentCollision != oldCollision && isPreservedTileCollisionOverride(currentCollision))
    return currentCollision;

  return newCollision;
}

}
}

import star.projectile_database;
import star.liquids_database;
import star.vehicle_database;
import star.world_generation;
import star.micro_dungeon;
import star.stagehand_database;
import star.item_database;
import star.object_database;
import star.treasure;


namespace Star {

static int const PlantAdjustmentLimit = 2;


std::expected<std::shared_ptr<LiquidWorld>, StarException> LiquidWorld::create(WorldServer& world) {
  auto result = std::shared_ptr<LiquidWorld>(new LiquidWorld(world));
  auto liquids = world.root().liquidsDatabase();
  if (!liquids) return std::unexpected(std::move(liquids.error()));
  result->m_liquidsDatabase = std::move(*liquids);
  auto materials = world.root().materialDatabase();
  if (!materials) return std::unexpected(std::move(materials.error()));
  result->m_materialDatabase = std::move(*materials);
  return result;
}

Vec2I LiquidWorld::uniqueLocation(Vec2I const& location) const {
  return m_worldServer.geometry().xwrap(location);
}

std::expected<float, StarException> LiquidWorld::drainLevel(Vec2I const& location) const {
  auto const& worldTemplate = m_worldServer.worldTemplate();
  if (location[1] > worldTemplate->undergroundLevel()) {
    auto autoconsttileResult1 = m_worldServer.getServerTile(location);
    if (!autoconsttileResult1) return std::unexpected(std::move(autoconsttileResult1.error()));
    auto const& tile = std::move(autoconsttileResult1->get());
    if (auto condition75 = [&]() -> std::expected<bool, StarException> {
            auto conditionResult1 = m_materialDatabase->blocksLiquidFlow(tile.background);
      if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
      auto condition = std::move(!(*conditionResult1));
      return static_cast<bool>(condition);
    }(); !condition75)
      return std::unexpected(std::move(condition75.error()));
    else if (*condition75) {
      auto autoconstbelowTileResult1 = m_worldServer.getServerTile(location + Vec2I(0, -1));
      if (!autoconstbelowTileResult1) return std::unexpected(std::move(autoconstbelowTileResult1.error()));
      auto const& belowTile = std::move(autoconstbelowTileResult1->get());
      if (auto condition74 = [&]() -> std::expected<bool, StarException> {
        return [&]() -> std::expected<bool, StarException> {
          auto left = [&]() -> std::expected<bool, StarException> {             auto conditionResult1 = m_materialDatabase->blocksLiquidFlow(belowTile.background);
            if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
            auto condition = std::move((*conditionResult1));
            return static_cast<bool>(condition); }();
          if (!left) return std::unexpected(std::move(left.error()));
          if (*left) return true;
          return [&]() -> std::expected<bool, StarException> {
            auto left = [&]() -> std::expected<bool, StarException> {               auto conditionResult1 = m_materialDatabase->blocksLiquidFlow(belowTile.foreground);
              if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
              auto condition = std::move((*conditionResult1));
              return static_cast<bool>(condition); }();
            if (!left) return std::unexpected(std::move(left.error()));
            if (*left) return true;
                        auto condition = std::move(belowTile.liquid.source);
            return static_cast<bool>(condition);
          }();
        }();
      }(); !condition74)
        return std::unexpected(std::move(condition74.error()));
      else if (*condition74)
        return m_liquidsDatabase->backgroundDrain();
    }
  }
  return 0.0f;
}

std::expected<CellularLiquidCell<LiquidId>, StarException> LiquidWorld::cell(Vec2I const& location) const {
  auto autoconsttileResult1 = m_worldServer.getServerTile(location);
  if (!autoconsttileResult1) return std::unexpected(std::move(autoconsttileResult1.error()));
  auto const& tile = std::move(autoconsttileResult1->get());
  if (auto condition76 = [&]() -> std::expected<bool, StarException> {
        auto conditionResult1 = m_materialDatabase->blocksLiquidFlow(tile.foreground);
    if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
    auto condition = std::move((*conditionResult1));
    return static_cast<bool>(condition);
  }(); !condition76)
    return std::unexpected(std::move(condition76.error()));
  else if (*condition76) {
    return CellularLiquidCollisionCell();
  } else {
    if (tile.liquid.source)
      return CellularLiquidSourceCell<LiquidId>{.liquid = tile.liquid.liquid, .pressure = tile.liquid.pressure};
    else if (tile.liquid.liquid != EmptyLiquidId)
      return CellularLiquidFlowCell<LiquidId>{.liquid = tile.liquid.liquid,
                                              .level = tile.liquid.level,
                                              .pressure = tile.liquid.pressure};
    else
      return CellularLiquidFlowCell<LiquidId>{.liquid = {}, .level = 0.0f, .pressure = 0.0f};
  }
}

std::expected<void, StarException> LiquidWorld::setFlow(Vec2I const& location, CellularLiquidFlowCell<LiquidId> const& flow) {
  if (flow.liquid) {
    {
      auto operationResult1 = m_worldServer.setLiquid(location, *flow.liquid, flow.level, flow.pressure);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    auto autoconsttileResult1 = m_worldServer.getServerTile(location);
    if (!autoconsttileResult1) return std::unexpected(std::move(autoconsttileResult1.error()));
    auto const& tile = std::move(autoconsttileResult1->get());
    if (auto materialInteraction = m_materialDatabase->liquidMaterialInteraction(*flow.liquid, tile.background)) {
      if (!materialInteraction->topOnly && tile.liquid.level >= materialInteraction->consumeLiquid) {
        auto modifyTileCheckedResult1 = m_worldServer.modifyServerTile(location);
        if (!modifyTileCheckedResult1) return std::unexpected(std::move(modifyTileCheckedResult1.error()));
        if (auto modifyTile = std::move((*modifyTileCheckedResult1))) {
          modifyTile->get().liquid.take(materialInteraction->consumeLiquid);
          modifyTile->get().background = materialInteraction->transformTo;
          {
            auto operationResult1 = m_worldServer.activateLiquidLocation(location);
            if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          }
        }
      }
    }
    if (auto modInteraction = m_materialDatabase->liquidModInteraction(*flow.liquid, tile.backgroundMod)) {
      if (!modInteraction->topOnly && tile.liquid.level >= modInteraction->consumeLiquid) {
        auto modifyTileCheckedResult1 = m_worldServer.modifyServerTile(location);
        if (!modifyTileCheckedResult1) return std::unexpected(std::move(modifyTileCheckedResult1.error()));
        if (auto modifyTile = std::move((*modifyTileCheckedResult1))) {
          modifyTile->get().liquid.take(modInteraction->consumeLiquid);
          modifyTile->get().backgroundMod = modInteraction->transformTo;
          {
            auto operationResult1 = m_worldServer.activateLiquidLocation(location);
            if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          }
        }
      }
    }
  } else {
    {
      auto operationResult1 = m_worldServer.setLiquid(location, EmptyLiquidId, 0.0f, 0.0f);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  }

  return {};
}

std::expected<void, StarException> LiquidWorld::liquidInteraction(Vec2I const& a, LiquidId aLiquid, Vec2I const& b, LiquidId bLiquid) {
  auto handleInteraction = [&worldServer = m_worldServer](Vec2I const& target, std::optional<LiquidInteractionResult> interaction) -> std::expected<void, StarException> {
    if (interaction) {
      if (interaction->isLeft()) {
        {
          auto replacementMaterial = interaction->left();
          if (!replacementMaterial)
            return std::unexpected(std::move(replacementMaterial.error()));
          auto materialPlaced = worldServer.modifyTile(target,
                                                       PlaceMaterial{.layer = TileLayer::Foreground,
                                                                     .material = replacementMaterial->get(),
                                                                     .materialHueShift = 0},
                                                       false);
          if (!materialPlaced)
            return std::unexpected(std::move(materialPlaced.error()));
        }
      } else {
        auto liquidLevel = worldServer.liquidLevel(target);
        {
          auto operationResult1 = interaction->right();
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          auto operationResult2 = worldServer.setLiquid(target, operationResult1->get(), liquidLevel.level, liquidLevel.level);
          if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        }
      }
    }
    return {};
  };
  {
    auto operationResult1 = handleInteraction(a, m_liquidsDatabase->interact(aLiquid, bLiquid));
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = handleInteraction(b, m_liquidsDatabase->interact(bLiquid, aLiquid));
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }

  return {};
}

std::expected<void, StarException> LiquidWorld::liquidCollision(Vec2I const& liquidPos, LiquidId liquidId, Vec2I const& blockPos) {
  auto autoconstblockTileResult1 = m_worldServer.getServerTile(blockPos);
  if (!autoconstblockTileResult1) return std::unexpected(std::move(autoconstblockTileResult1.error()));
  auto const& blockTile = std::move(autoconstblockTileResult1->get());

  if (auto materialInteraction = m_materialDatabase->liquidMaterialInteraction(liquidId, blockTile.foreground)) {
    if ((!materialInteraction->topOnly || liquidPos[1] > blockPos[1]) && m_worldServer.liquidLevel(liquidPos).level >= materialInteraction->consumeLiquid) {
      auto automodifyLiquidTileResult1 = m_worldServer.modifyServerTile(liquidPos);
      if (!automodifyLiquidTileResult1) return std::unexpected(std::move(automodifyLiquidTileResult1.error()));
      auto modifyLiquidTile = std::move((*automodifyLiquidTileResult1));
      auto automodifyBlockTileResult1 = m_worldServer.modifyServerTile(blockPos);
      if (!automodifyBlockTileResult1) return std::unexpected(std::move(automodifyBlockTileResult1.error()));
      auto modifyBlockTile = std::move((*automodifyBlockTileResult1));
      if (modifyLiquidTile && modifyBlockTile) {
        modifyLiquidTile->get().liquid.take(materialInteraction->consumeLiquid);
        modifyBlockTile->get().foreground = materialInteraction->transformTo;
        if (auto condition77 = [&]() -> std::expected<bool, StarException> {
                    auto conditionResult1 = m_materialDatabase->isMultiColor(materialInteraction->transformTo);
          if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
          auto condition = std::move(!(*conditionResult1));
          return static_cast<bool>(condition);
        }(); !condition77)
          return std::unexpected(std::move(condition77.error()));
        else if (*condition77)
          modifyBlockTile->get().foregroundColorVariant = DefaultMaterialColorVariant;
        {
          auto operationResult1 = m_worldServer.activateLiquidLocation(liquidPos);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
      }
    }
  }
  if (auto modInteraction = m_materialDatabase->liquidModInteraction(liquidId, blockTile.foregroundMod)) {
    if ((!modInteraction->topOnly || liquidPos[1] > blockPos[1]) && m_worldServer.liquidLevel(liquidPos).level >= modInteraction->consumeLiquid) {
      auto automodifyLiquidTileResult1 = m_worldServer.modifyServerTile(liquidPos);
      if (!automodifyLiquidTileResult1) return std::unexpected(std::move(automodifyLiquidTileResult1.error()));
      auto modifyLiquidTile = std::move((*automodifyLiquidTileResult1));
      auto automodifyBlockTileResult1 = m_worldServer.modifyServerTile(blockPos);
      if (!automodifyBlockTileResult1) return std::unexpected(std::move(automodifyBlockTileResult1.error()));
      auto modifyBlockTile = std::move((*automodifyBlockTileResult1));
      if (modifyLiquidTile && modifyBlockTile) {
        modifyLiquidTile->get().liquid.take(modInteraction->consumeLiquid);
        modifyBlockTile->get().foregroundMod = modInteraction->transformTo;
        {
          auto operationResult1 = m_worldServer.activateLiquidLocation(liquidPos);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
      }
    }
  }

  return {};
}

std::expected<std::shared_ptr<FallingBlocksWorld>, StarException> FallingBlocksWorld::create(WorldServer& world) {
  auto result = std::shared_ptr<FallingBlocksWorld>(new FallingBlocksWorld(world));
  auto materials = world.root().materialDatabase();
  if (!materials) return std::unexpected(std::move(materials.error()));
  result->m_materialDatabase = std::move(*materials);
  return result;
}

std::expected<FallingBlockType, StarException> FallingBlocksWorld::blockType(Vec2I const& pos) {
  auto autoconsttileResult1 = m_worldServer.getServerTile(pos, true);
  if (!autoconsttileResult1) return std::unexpected(std::move(autoconsttileResult1.error()));
  auto const& tile = std::move(autoconsttileResult1->get());
  if (tile.rootSource) {
    return FallingBlockType::Immovable;
  } if (tile.foreground == EmptyMaterialId) {
    if (auto condition80 = [&]() -> std::expected<bool, StarException> {
            auto conditionResult1 = m_worldServer.tileIsOccupied(pos, TileLayer::Foreground);
      if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
      auto condition = std::move((*conditionResult1));
      return static_cast<bool>(condition);
    }(); !condition80)
      return std::unexpected(std::move(condition80.error()));
    else if (*condition80)
      return FallingBlockType::Immovable;
    else
      return FallingBlockType::Open;
  } else if (auto condition79 = [&]() -> std::expected<bool, StarException> {
        auto conditionResult1 = m_materialDatabase->isCascadingFallingMaterial(tile.foreground);
    if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
    auto condition = std::move((*conditionResult1));
    return static_cast<bool>(condition);
  }(); !condition79)
    return std::unexpected(std::move(condition79.error()));
  else if (*condition79) {
    return FallingBlockType::Cascading;
  } else if (auto condition78 = [&]() -> std::expected<bool, StarException> {
        auto conditionResult1 = m_materialDatabase->isFallingMaterial(tile.foreground);
    if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
    auto condition = std::move((*conditionResult1));
    return static_cast<bool>(condition);
  }(); !condition78)
    return std::unexpected(std::move(condition78.error()));
  else if (*condition78) {
    return FallingBlockType::Falling;
  } else {
    return FallingBlockType::Immovable;
  }
}

std::expected<void, StarException> FallingBlocksWorld::moveBlock(Vec2I const& from, Vec2I const& to) {
  auto& generationRoot = m_worldServer.root();
  auto autofromTileResult1 = m_worldServer.modifyServerTile(from, true);
  if (!autofromTileResult1) return std::unexpected(std::move(autofromTileResult1.error()));
  auto fromTile = std::move((*autofromTileResult1));
  auto autotoTileResult1 = m_worldServer.modifyServerTile(to, true);
  if (!autotoTileResult1) return std::unexpected(std::move(autotoTileResult1.error()));
  auto toTile = std::move((*autotoTileResult1));
  if (!fromTile || !toTile)
    return {};

  if (m_worldServer.isTileProtected(to)) {
    auto rangeValuesResult1 = m_worldServer.destroyBlock(TileLayer::Foreground, from, true, true);
    if (!rangeValuesResult1) return std::unexpected(std::move(rangeValuesResult1.error()));
    auto rangeValues = std::move((*rangeValuesResult1));
    for (auto const& drop : rangeValues) {
      auto randomizedDrop = ItemDrop::createRandomizedDrop(generationRoot, drop, Vec2F(to));
      if (!randomizedDrop)
        return std::unexpected(std::move(randomizedDrop.error()));
      auto dropAdded = m_worldServer.addEntity((*randomizedDrop));
      if (!dropAdded)
        return std::unexpected(std::move(dropAdded.error()));
    }
  } else {
    toTile->get().foreground = fromTile->get().foreground;
    toTile->get().foregroundMod = NoModId;
    toTile->get().foregroundHueShift = fromTile->get().foregroundHueShift;
    toTile->get().foregroundColorVariant = fromTile->get().foregroundColorVariant;
    {
      auto operationResult1 = m_materialDatabase->materialCollisionKind(toTile->get().foreground);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      toTile->get().updateCollision((*operationResult1));
    }

    fromTile->get().foreground = EmptyMaterialId;
    fromTile->get().foregroundMod = NoModId;
    fromTile->get().updateCollision(CollisionKind::None);
    m_worldServer.requestGlobalBreakCheck();
  }

  return {};
}

DungeonGeneratorWorld::DungeonGeneratorWorld(WorldServer& worldServer, bool markForActivation)
  : m_worldServer(worldServer), m_markForActivation(markForActivation) {}

WorldGeometry DungeonGeneratorWorld::getWorldGeometry() const {
  return m_worldServer.geometry();
}

std::expected<void, StarException> DungeonGeneratorWorld::markRegion(RectI const& region) {
  auto& generationRoot = m_worldServer.root();
  if (!m_markForActivation)
    return {};
  {
    auto logged = generationRoot.logger().debug("Marking {} as dungeon region", region);
    if (!logged) return std::unexpected(std::move(logged.error()));
  }
  {
    auto operationResult1 = m_worldServer.signalRegion(region);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = m_worldServer.activateLiquidRegion(region);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::markTerrain(PolyF const& region) {
  auto& generationRoot = m_worldServer.root();
  auto const& worldTemplate = m_worldServer.worldTemplate();
  if (!m_markForActivation)
    return {};
  {
    auto logged = generationRoot.logger().debug("Marking poly as dungeon terrain region: {}", region);
    if (!logged) return std::unexpected(std::move(logged.error()));
  }
  worldTemplate->addCustomTerrainRegion(region);

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::markSpace(PolyF const& region) {
  auto& generationRoot = m_worldServer.root();
  auto const& worldTemplate = m_worldServer.worldTemplate();
  if (!m_markForActivation)
    return {};
  {
    auto logged = generationRoot.logger().debug("Marking poly as dungeon space region: {}", region);
    if (!logged) return std::unexpected(std::move(logged.error()));
  }
  worldTemplate->addCustomSpaceRegion(region);

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::setForegroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) {
  auto& generationRoot = m_worldServer.root();
  auto tileCheckedResult1 = m_worldServer.modifyServerTile(position);
  if (!tileCheckedResult1) return std::unexpected(std::move(tileCheckedResult1.error()));
  if (auto tile = std::move((*tileCheckedResult1))) {
    {
      auto operationResult1 = m_worldServer.modifyLiquid(position, EmptyLiquidId, 0);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    tile->get().foreground = material;
    tile->get().foregroundHueShift = hueshift;
    tile->get().foregroundColorVariant = colorVariant;
    tile->get().foregroundMod = NoModId;
    tile->get().foregroundModHueShift = MaterialHue();
    auto tilecollisionResult1 = generationRoot.materialDatabase();
    if (!tilecollisionResult1) return std::unexpected(std::move(tilecollisionResult1.error()));
    auto tilecollisionResult2 = (*tilecollisionResult1)->materialCollisionKind(tile->get().foreground);
    if (!tilecollisionResult2) return std::unexpected(std::move(tilecollisionResult2.error()));
    tile->get().collision = std::move((*tilecollisionResult2));
    tile->get().collisionCacheDirty = true;
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::setBackgroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) {
  auto tileCheckedResult1 = m_worldServer.modifyServerTile(position);
  if (!tileCheckedResult1) return std::unexpected(std::move(tileCheckedResult1.error()));
  if (auto tile = std::move((*tileCheckedResult1))) {
    {
      auto operationResult1 = m_worldServer.modifyLiquid(position, EmptyLiquidId, 0);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    tile->get().background = material;
    tile->get().backgroundHueShift = hueshift;
    tile->get().backgroundColorVariant = colorVariant;
    tile->get().backgroundMod = NoModId;
    tile->get().backgroundModHueShift = MaterialHue();
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::placeObject(Vec2I const& pos, String const& objectName, Star::Direction direction, Json const& parameters) {
  auto& generationRoot = m_worldServer.root();
  {
    auto operationResult1 = m_worldServer.signalRegion(RectI::withSize(pos, {1, 1}));
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  auto autoobjectDatabaseResult1 = generationRoot.objectDatabase();
  if (!autoobjectDatabaseResult1) return std::unexpected(std::move(autoobjectDatabaseResult1.error()));
  auto objectDatabase = std::move((*autoobjectDatabaseResult1));
  auto objectCheckedResult1 = objectDatabase->createForPlacement(m_worldServer, objectName, pos, direction, parameters);
  if (!objectCheckedResult1) return std::unexpected(std::move(objectCheckedResult1.error()));
  if (auto object = std::move((*objectCheckedResult1)))
    {
      auto operationResult1 = m_worldServer.addEntity(object);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  else
    {
      auto logged = generationRoot.logger().warn("Failed to place dungeon object: {} direction: {} position: {}",
                                                 objectName, static_cast<int>(direction), pos);
      if (!logged) return std::unexpected(std::move(logged.error()));
    }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::placeVehicle(Vec2F const& pos, String const& vehicleName, Json const& parameters) {
  auto& generationRoot = m_worldServer.root();
  {
    auto operationResult1 = m_worldServer.signalRegion(RectI::withSize(Vec2I(pos), {1, 1}));
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  auto autovehicleDatabaseResult1 = generationRoot.vehicleDatabase();
  if (!autovehicleDatabaseResult1) return std::unexpected(std::move(autovehicleDatabaseResult1.error()));
  auto vehicleDatabase = std::move((*autovehicleDatabaseResult1));
  auto vehicleParameters = parameters.opt().value_or(JsonObject{}).set("persistent", true);
  if (!vehicleParameters) return std::unexpected(std::move(vehicleParameters.error()));
  auto created = vehicleDatabase->create(vehicleName, *vehicleParameters);
  if (!created) return std::unexpected(std::move(created.error()));
  auto vehicle = std::move(*created);
  vehicle->setPosition(pos);
  {
    auto operationResult1 = m_worldServer.addEntity(vehicle);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::placeSurfaceBiomeItems(Vec2I const& pos) {
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto ListBiomeItemPlacementsurfaceItemsResult1 = worldTemplate->potentialBiomeItemsAt(pos[0], pos[1]);
  if (!ListBiomeItemPlacementsurfaceItemsResult1) return std::unexpected(std::move(ListBiomeItemPlacementsurfaceItemsResult1.error()));
  List<BiomeItemPlacement> surfaceItems = std::move((*ListBiomeItemPlacementsurfaceItemsResult1).surfaceBiomeItems);
  {
    auto operationResult1 = placeBiomeItems(pos, surfaceItems);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::placeBiomeTree(Vec2I const& pos) {
  auto& generationRoot = m_worldServer.root();
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto biomeCheckedResult1 = worldTemplate->blockBiome(pos[0], pos[1]);
  if (!biomeCheckedResult1) return std::unexpected(std::move(biomeCheckedResult1.error()));
  if (auto biome = std::move((*biomeCheckedResult1))) {
    {
      auto operationResult1 = m_worldServer.signalRegion(RectI::withSize(pos, {1, 1}));
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    auto seed = worldTemplate->seedFor(pos[0], pos[1]);
    if (auto treeVariant = biome->surfacePlaceables.firstTreeType())
      {
        auto operationResult1 = generationRoot.plantDatabase();
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        auto operationResult2 = (*operationResult1)->createPlant(*treeVariant, seed);
        if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        auto operationResult3 = placePlant((*operationResult2), pos);
        if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
      }
  }

  return {};
}

// yay, copy paste coding, kyren WILL kill me
std::expected<void, StarException> DungeonGeneratorWorld::placePlant(std::shared_ptr<Plant> const& plant, Vec2I const& position) {
  auto& generationRoot = m_worldServer.root();
  if (!plant)
    return {};

  auto spaces = plant->spaces();
  auto roots = plant->roots();
  auto const& primaryRoot = plant->primaryRoot();
  auto autobackgroundResult1 = m_worldServer.getServerTile(position);
  if (!autobackgroundResult1) return std::unexpected(std::move(autobackgroundResult1.error()));
  auto background = std::move(autobackgroundResult1->get().background);
  bool adjustBackground = background == EmptyMaterialId || background == NullMaterialId;

  auto withinAdjustment = [=](Vec2I const& pos) {
    return PlantAdjustmentLimit - std::abs(pos[0]) > 0 && PlantAdjustmentLimit - std::abs(pos[1]) > 0;
  };

  // Bail out if we don't have at least one free space, and root in the primary
  // root position, or if we're in a dungeon region.
  auto autoprimaryTileResult1 = m_worldServer.getServerTile(position);
  if (!autoprimaryTileResult1) return std::unexpected(std::move(autoprimaryTileResult1.error()));
  auto primaryTile = std::move(autoprimaryTileResult1->get());
  auto autorootTileResult1 = m_worldServer.getServerTile(position + primaryRoot);
  if (!autorootTileResult1) return std::unexpected(std::move(autorootTileResult1.error()));
  auto rootTile = std::move(autorootTileResult1->get());
  if (isConnectableMaterial(primaryTile.foreground) || !isConnectableMaterial(rootTile.foreground))
    return {};

  // First bail out if we can't fit anything we're not adjusting
  for (auto space : spaces) {
    Vec2I pspace = space + position;

    if (auto condition83 = [&]() -> std::expected<bool, StarException> {
      return [&]() -> std::expected<bool, StarException> {
        auto left = [&]() -> std::expected<bool, StarException> {           auto condition = std::move(withinAdjustment(space));
          return static_cast<bool>(condition); }();
        if (!left) return std::unexpected(std::move(left.error()));
        if (!*left) return false;
                auto conditionResult1 = m_worldServer.atTile<Plant>(pspace);
        if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
        auto condition = std::move(!(*conditionResult1).empty());
        return static_cast<bool>(condition);
      }();
    }(); !condition83)
      return std::unexpected(std::move(condition83.error()));
    else if (*condition83)
      return {};

    // Bail out if we hit a different plant's root tile, or if we're not in the
    // adjustment space and we hit a non-empty tile.
    auto autotileResult1 = m_worldServer.getServerTile(pspace);
    if (!autotileResult1) return std::unexpected(std::move(autotileResult1.error()));
    auto tile = std::move(autotileResult1->get());
    if (tile.rootSource || (!withinAdjustment(space) && !(tile.foreground == EmptyMaterialId || tile.foreground == NullMaterialId)))
      return {};
  }

  // Check all the roots outside of the adjustment limit
  for (auto root : roots) {
    root += position;
    if (auto condition82 = [&]() -> std::expected<bool, StarException> {
      return [&]() -> std::expected<bool, StarException> {
        auto left = [&]() -> std::expected<bool, StarException> {           auto condition = std::move(!withinAdjustment(root));
          return static_cast<bool>(condition); }();
        if (!left) return std::unexpected(std::move(left.error()));
        if (!*left) return false;
                auto conditionResult1 = m_worldServer.getServerTile(root);
        if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
        auto condition = std::move(!isConnectableMaterial(conditionResult1->get().foreground));
        return static_cast<bool>(condition);
      }();
    }(); !condition82)
      return std::unexpected(std::move(condition82.error()));
    else if (*condition82)
      return {};
  }

  // Clear all the necessary blocks within the adjustment limit
  for (auto space : spaces) {
    if (!withinAdjustment(space))
      continue;

    space += position;
    auto tileCheckedResult1 = m_worldServer.modifyServerTile(space);
    if (!tileCheckedResult1) return std::unexpected(std::move(tileCheckedResult1.error()));
    if (auto tile = std::move((*tileCheckedResult1))) {
      if (isConnectableMaterial(tile->get().foreground))
        tile->get() = primaryTile;
      if (adjustBackground)
        tile->get().background = EmptyMaterialId;
      tile->get().collision = CollisionKind::None;
      auto tilecollisionResult1 = generationRoot.materialDatabase();
      if (!tilecollisionResult1) return std::unexpected(std::move(tilecollisionResult1.error()));
      auto tilecollisionResult2 = (*tilecollisionResult1)->materialCollisionKind(tile->get().foreground);
      if (!tilecollisionResult2) return std::unexpected(std::move(tilecollisionResult2.error()));
      tile->get().collision = std::move((*tilecollisionResult2));
      tile->get().collisionCacheDirty = true;
    } else {
      return {};
    }
  }

  // Make all the root blocks a real material based on the primary root.
  for (auto root : roots) {
    root += position;
    auto tileCheckedResult1 = m_worldServer.modifyServerTile(root);
    if (!tileCheckedResult1) return std::unexpected(std::move(tileCheckedResult1.error()));
    if (auto tile = std::move((*tileCheckedResult1))) {
      if (!isRealMaterial(tile->get().foreground)) {
        tile->get() = rootTile;
        auto tilecollisionResult1 = generationRoot.materialDatabase();
        if (!tilecollisionResult1) return std::unexpected(std::move(tilecollisionResult1.error()));
        auto tilecollisionResult2 = (*tilecollisionResult1)->materialCollisionKind(tile->get().foreground);
        if (!tilecollisionResult2) return std::unexpected(std::move(tilecollisionResult2.error()));
        tile->get().collision = std::move((*tilecollisionResult2));
        tile->get().collisionCacheDirty = true;
      }
    } else {
      return {};
    }
  }
  {
    auto operationResult1 = plant->setTilePosition(position);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = m_worldServer.addEntity(plant);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::placeBiomeItems(Vec2I const& pos, List<BiomeItemPlacement>& potentialItems) {
  auto& generationRoot = m_worldServer.root();
  auto const& worldTemplate = m_worldServer.worldTemplate();
  {
    auto operationResult1 = m_worldServer.signalRegion(RectI::withSize(pos, {1, 1}));
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  std::sort(potentialItems.begin(), potentialItems.end(), std::less<BiomeItemPlacement>());
  for (auto const& placement : potentialItems) {
    auto seed = worldTemplate->seedFor(placement.position[0], placement.position[1]);
    if (std::holds_alternative<GrassVariant>(placement.item)) {
      auto& grass = std::get<GrassVariant>(placement.item);
      {
        auto operationResult1 = generationRoot.plantDatabase();
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        auto operationResult2 = (*operationResult1)->createPlant(grass, seed);
        if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        auto operationResult3 = placePlant((*operationResult2), placement.position);
        if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
      }
    } else if (std::holds_alternative<BushVariant>(placement.item)) {
      auto& bush = std::get<BushVariant>(placement.item);
      {
        auto operationResult1 = generationRoot.plantDatabase();
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        auto operationResult2 = (*operationResult1)->createPlant(bush, seed);
        if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        auto operationResult3 = placePlant((*operationResult2), placement.position);
        if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
      }
    } else if (std::holds_alternative<TreePair>(placement.item)) {
      auto& treePair = std::get<TreePair>(placement.item);
      TreeVariant treeVariant;
      if (seed % 2 == 0)
        treeVariant = treePair.first;
      else
        treeVariant = treePair.second;
      {
        auto operationResult1 = generationRoot.plantDatabase();
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        auto operationResult2 = (*operationResult1)->createPlant(treeVariant, seed);
        if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        auto operationResult3 = placePlant((*operationResult2), placement.position);
        if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
      }
    } else if (std::holds_alternative<ObjectPool>(placement.item)) {
      auto& objectPool = std::get<ObjectPool>(placement.item);
      auto direction = seed % 2 ? Direction::Left : Direction::Right;
      auto objectPair = objectPool.select(seed);
      auto objectCheckedResult1 = generationRoot.objectDatabase();
      if (!objectCheckedResult1) return std::unexpected(std::move(objectCheckedResult1.error()));
      auto objectCheckedResult2 = (*objectCheckedResult1)->createForPlacement(
              m_worldServer, objectPair.first, placement.position, direction, objectPair.second);
      if (!objectCheckedResult2) return std::unexpected(std::move(objectCheckedResult2.error()));
      if (auto object = std::move((*objectCheckedResult2)))
        {
          auto operationResult1 = m_worldServer.addEntity(object);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
    } else if (std::holds_alternative<TreasureBoxSet>(placement.item)) {
      auto& treasureBoxSet = std::get<TreasureBoxSet>(placement.item);
      auto direction = seed % 2 ? Direction::Left : Direction::Right;
      auto treasureContainerCheckedResult1 = generationRoot.treasureDatabase();
      if (!treasureContainerCheckedResult1) return std::unexpected(std::move(treasureContainerCheckedResult1.error()));
      auto treasureContainerCheckedResult2 = (*treasureContainerCheckedResult1)->createTreasureChest(m_worldServer, treasureBoxSet, placement.position, direction, seed);
      if (!treasureContainerCheckedResult2) return std::unexpected(std::move(treasureContainerCheckedResult2.error()));
      if (auto treasureContainer = std::move((*treasureContainerCheckedResult2)))
        {
          auto operationResult1 = m_worldServer.addEntity(treasureContainer);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
    }
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::addDrop(Vec2F const& position, ItemDescriptor const& item) {
  auto& generationRoot = m_worldServer.root();
  {
    auto operationResult1 = ItemDrop::createRandomizedDrop(generationRoot, item, position);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    auto operationResult2 = m_worldServer.addEntity((*operationResult1));
    if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::spawnNpc(Vec2F const& position, Json const& parameters) {
  auto& generationRoot = m_worldServer.root();
  auto autokindResult1 = parameters.getString("kind");
  if (!autokindResult1) return std::unexpected(std::move(autokindResult1.error()));
  auto kind = std::move((*autokindResult1));
  if (auto condition85 = [&]() -> std::expected<bool, StarException> {
        auto conditionResult1 = kind.equals("npc", String::CaseInsensitive);
    if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
    auto condition = std::move((*conditionResult1));
    return static_cast<bool>(condition);
  }(); !condition85)
    return std::unexpected(std::move(condition85.error()));
  else if (*condition85) {
    auto autonpcDatabaseResult1 = generationRoot.npcDatabase();
    if (!autonpcDatabaseResult1) return std::unexpected(std::move(autonpcDatabaseResult1.error()));
    auto npcDatabase = std::move((*autonpcDatabaseResult1));
    auto stduint64_tseedResult1 = parameters.getUInt("seed", generationRoot.random().randu64());
    if (!stduint64_tseedResult1) return std::unexpected(std::move(stduint64_tseedResult1.error()));
    std::uint64_t seed = std::move((*stduint64_tseedResult1));
    auto StringspeciesResult1 = parameters.getString("species");
    if (!StringspeciesResult1) return std::unexpected(std::move(StringspeciesResult1.error()));
    String species = std::move((*StringspeciesResult1));
    auto StringtypeNameResult1 = parameters.getString("typeName", "default");
    if (!StringtypeNameResult1) return std::unexpected(std::move(StringtypeNameResult1.error()));
    String typeName = std::move((*StringtypeNameResult1));
    auto JsonObjectuniqueParametersResult1 = parameters.getObject("parameters", {});
    if (!JsonObjectuniqueParametersResult1) return std::unexpected(std::move(JsonObjectuniqueParametersResult1.error()));
    JsonObject uniqueParameters = std::move((*JsonObjectuniqueParametersResult1));
    if (!uniqueParameters.contains("persistent"))
      uniqueParameters["persistent"] = true;
    auto autonpcResult1 = m_worldServer.threatLevel();
    if (!autonpcResult1) return std::unexpected(std::move(autonpcResult1.error()));
    auto autonpcResult2 = npcDatabase->generateNpcVariant(species, typeName, (*autonpcResult1), seed, uniqueParameters);
    if (!autonpcResult2) return std::unexpected(std::move(autonpcResult2.error()));
    auto autonpcResult3 = npcDatabase->createNpc((*autonpcResult2));
    if (!autonpcResult3) return std::unexpected(std::move(autonpcResult3.error()));
    auto npc = std::move((*autonpcResult3));
    {
      auto operationResult1 = npc->feetOffset();
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      npc->setPosition(position - (*operationResult1));
    }
    {
      auto operationResult1 = m_worldServer.addEntity(npc);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  } else if (auto condition84 = [&]() -> std::expected<bool, StarException> {
        auto conditionResult1 = kind.equals("monster", String::CaseInsensitive);
    if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
    auto condition = std::move((*conditionResult1));
    return static_cast<bool>(condition);
  }(); !condition84)
    return std::unexpected(std::move(condition84.error()));
  else if (*condition84) {
    auto automonsterDatabaseResult1 = generationRoot.monsterDatabase();
    if (!automonsterDatabaseResult1) return std::unexpected(std::move(automonsterDatabaseResult1.error()));
    auto monsterDatabase = std::move((*automonsterDatabaseResult1));
    auto stduint64_tseedResult1 = parameters.getUInt("seed", generationRoot.random().randu64());
    if (!stduint64_tseedResult1) return std::unexpected(std::move(stduint64_tseedResult1.error()));
    std::uint64_t seed = std::move((*stduint64_tseedResult1));
    auto StringtypeNameResult1 = parameters.getString("typeName");
    if (!StringtypeNameResult1) return std::unexpected(std::move(StringtypeNameResult1.error()));
    String typeName = std::move((*StringtypeNameResult1));
    auto JsonObjectuniqueParametersResult1 = parameters.getObject("parameters", {});
    if (!JsonObjectuniqueParametersResult1) return std::unexpected(std::move(JsonObjectuniqueParametersResult1.error()));
    JsonObject uniqueParameters = std::move((*JsonObjectuniqueParametersResult1));
    if (!uniqueParameters.contains("persistent"))
      uniqueParameters["persistent"] = true;
    auto automonsterResult1 = monsterDatabase->monsterVariant(typeName, seed, uniqueParameters);
    if (!automonsterResult1) return std::unexpected(std::move(automonsterResult1.error()));
    auto automonsterResult2 = monsterDatabase->createMonster((*automonsterResult1));
    if (!automonsterResult2) return std::unexpected(std::move(automonsterResult2.error()));
    auto monster = std::move((*automonsterResult2));
    monster->setPosition(position);
    {
      auto operationResult1 = m_worldServer.addEntity(monster);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  } else
    return std::unexpected(StarException(strf("Unknown spawnable kind '{}'", kind)));

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::spawnStagehand(Vec2F const& position, Json const& definition) {
  auto& generationRoot = m_worldServer.root();
  auto autostagehandResult1 = generationRoot.stagehandDatabase();
  if (!autostagehandResult1) return std::unexpected(std::move(autostagehandResult1.error()));
  auto autostagehandResult2 = definition.getString("type");
  if (!autostagehandResult2) return std::unexpected(std::move(autostagehandResult2.error()));
  auto autostagehandResult3 = definition.get("parameters", Json());
  if (!autostagehandResult3) return std::unexpected(std::move(autostagehandResult3.error()));
  auto autostagehandResult4 = (*autostagehandResult1)->createStagehand((*autostagehandResult2), (*autostagehandResult3));
  if (!autostagehandResult4) return std::unexpected(std::move(autostagehandResult4.error()));
  auto stagehand = std::move((*autostagehandResult4));
  stagehand->setPosition(position);
  {
    auto operationResult1 = m_worldServer.addEntity(stagehand);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::setLiquid(Vec2I const& pos, LiquidStore const& liquid) {
  auto ServerTiletileResult1 = m_worldServer.modifyServerTile(pos);
  if (!ServerTiletileResult1) return std::unexpected(std::move(ServerTiletileResult1.error()));
  auto tile = std::move((*ServerTiletileResult1));
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((tile));
  }
  if (tile)
    tile->get().liquid = liquid;

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::setPlayerStart(Vec2F const& startPosition) {  m_worldServer.setPlayerStart(startPosition);

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::connectWireGroup(List<Vec2I> const& wireGroup) {
  auto& generationRoot = m_worldServer.root();
  List<WireConnection> outbounds;
  List<WireConnection> inbounds;

  for (auto entry : wireGroup) {
    bool found = false;
    Vec2F posf = centerOfTile(entry);
    RectF bounds = {posf - Vec2F(16, 16), posf + Vec2F(16, 16)};
    auto rangeValuesResult1 = m_worldServer.query<WireEntity>(bounds);
    if (!rangeValuesResult1) return std::unexpected(std::move(rangeValuesResult1.error()));
    auto rangeValues = std::move((*rangeValuesResult1));
    for (auto entity : rangeValues) {
      for (std::size_t i = 0; i < entity->nodeCount(WireDirection::Input); ++i) {
        if (auto inputNodeMatches = [&]() -> std::expected<bool, StarException> {
              auto nodeOffset = entity->nodePosition({.direction = WireDirection::Input, .nodeIndex = i});
              if (!nodeOffset)
                return std::unexpected(std::move(nodeOffset.error()));
              auto condition = std::move(entity->tilePosition() + (*nodeOffset) == entry);
              return static_cast<bool>(condition);
            }();
            !inputNodeMatches)
          return std::unexpected(std::move(inputNodeMatches.error()));
        else if (*inputNodeMatches) {
          inbounds.append(WireConnection{.entityLocation = entity->tilePosition(), .nodeIndex = i});
          found = true;
        }
      }
      for (std::size_t i = 0; i < entity->nodeCount(WireDirection::Output); ++i) {
        if (auto outputNodeMatches = [&]() -> std::expected<bool, StarException> {
              auto nodeOffset = entity->nodePosition({.direction = WireDirection::Output, .nodeIndex = i});
              if (!nodeOffset)
                return std::unexpected(std::move(nodeOffset.error()));
              auto condition = std::move(entity->tilePosition() + (*nodeOffset) == entry);
              return static_cast<bool>(condition);
            }();
            !outputNodeMatches)
          return std::unexpected(std::move(outputNodeMatches.error()));
        else if (*outputNodeMatches) {
          outbounds.append(WireConnection{.entityLocation = entity->tilePosition(), .nodeIndex = i});
          found = true;
        }
      }
    }
    if (!found)
      {
        auto logged = generationRoot.logger().warn("Dungeon wire endpoint not found. {}", entry);
        if (!logged) return std::unexpected(std::move(logged.error()));
      }
  }

  if (!outbounds.size() || !inbounds.size()) {
    {
      auto logged = generationRoot.logger().warn("Dungeon wires did not make a circuit.");
      if (!logged) return std::unexpected(std::move(logged.error()));
    }
    return {};
  }

  for (auto outbound : outbounds) {
    auto autooutResult1 = m_worldServer.atTile<WireEntity>(outbound.entityLocation);
    if (!autooutResult1) return std::unexpected(std::move(autooutResult1.error()));
    auto autooutResult2 = (*autooutResult1).first();
    if (!autooutResult2) return std::unexpected(std::move(autooutResult2.error()));
    auto out = std::move(autooutResult2->get());
    for (auto inbound : inbounds) {
      auto autoinResult1 = m_worldServer.atTile<WireEntity>(inbound.entityLocation);
      if (!autoinResult1) return std::unexpected(std::move(autoinResult1.error()));
      auto autoinResult2 = (*autoinResult1).first();
      if (!autoinResult2) return std::unexpected(std::move(autoinResult2.error()));
      auto in = std::move(autoinResult2->get());
      {
        auto inputConnectionAdded =
          in->addNodeConnection({.direction = WireDirection::Input, .nodeIndex = inbound.nodeIndex}, outbound);
        if (!inputConnectionAdded)
          return std::unexpected(std::move(inputConnectionAdded.error()));
      }
      {
        auto outputConnectionAdded =
          out->addNodeConnection({.direction = WireDirection::Output, .nodeIndex = outbound.nodeIndex}, inbound);
        if (!outputConnectionAdded)
          return std::unexpected(std::move(outputConnectionAdded.error()));
      }
    }
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::setForegroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) {
  auto ServerTiletileResult1 = m_worldServer.modifyServerTile(position);
  if (!ServerTiletileResult1) return std::unexpected(std::move(ServerTiletileResult1.error()));
  auto tile = std::move((*ServerTiletileResult1));
  if (tile) {
    tile->get().foregroundMod = mod;
    tile->get().foregroundModHueShift = hueshift;
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::setBackgroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) {
  auto ServerTiletileResult1 = m_worldServer.modifyServerTile(position);
  if (!ServerTiletileResult1) return std::unexpected(std::move(ServerTiletileResult1.error()));
  auto tile = std::move((*ServerTiletileResult1));
  if (tile) {
    tile->get().backgroundMod = mod;
    tile->get().foregroundModHueShift = hueshift;
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::setTileProtection(DungeonId dungeonId, bool isProtected) {  {
    auto operationResult1 = m_worldServer.setTileProtection(dungeonId, isProtected);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }

  return {};
}

std::expected<bool, StarException> DungeonGeneratorWorld::checkSolid(Vec2I const& position, TileLayer layer) {
  auto autoconsttileResult1 = m_worldServer.getServerTile(position);
  if (!autoconsttileResult1) return std::unexpected(std::move(autoconsttileResult1.error()));
  auto const& tile = std::move(autoconsttileResult1->get());
  return tile.material(layer) != EmptyMaterialId && tile.material(layer) != NullMaterialId;
}

std::expected<bool, StarException> DungeonGeneratorWorld::checkOpen(Vec2I const& position, TileLayer layer) {
  auto autoconsttileResult1 = m_worldServer.getServerTile(position);
  if (!autoconsttileResult1) return std::unexpected(std::move(autoconsttileResult1.error()));
  auto const& tile = std::move(autoconsttileResult1->get());
  return tile.material(layer) == EmptyMaterialId || tile.material(layer) == NullMaterialId;
}

std::expected<bool, StarException> DungeonGeneratorWorld::checkOceanLiquid(Vec2I const& position) {
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto autoconstblockResult1 = worldTemplate->blockInfo(position[0], position[1]);
  if (!autoconstblockResult1) return std::unexpected(std::move(autoconstblockResult1.error()));
  auto const& block = std::move((*autoconstblockResult1));
  return block.oceanLiquid != EmptyLiquidId && position[1] < block.oceanLiquidLevel;
}

std::expected<DungeonId, StarException> DungeonGeneratorWorld::getDungeonIdAt(Vec2I const& position) {
  auto returnedResult1 = m_worldServer.getServerTile(position);
  if (!returnedResult1) return std::unexpected(std::move(returnedResult1.error()));
  auto returnedValue = std::move(returnedResult1->get().dungeonId);

  return returnedValue;
}

std::expected<void, StarException> DungeonGeneratorWorld::setDungeonIdAt(Vec2I const& position, DungeonId dungeonId) {
  auto tileCheckedResult1 = m_worldServer.modifyServerTile(position);
  if (!tileCheckedResult1) return std::unexpected(std::move(tileCheckedResult1.error()));
  if (auto tile = std::move((*tileCheckedResult1)))
    tile->get().dungeonId = dungeonId;

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::clearTileEntities(RectI const& bounds, Set<Vec2I> const& positions, bool clearAnchoredObjects) {
  auto autoentitiesResult1 = m_worldServer.entityQuery(RectF(bounds).padded(1), entityTypeFilter<TileEntity>());
  if (!autoentitiesResult1) return std::unexpected(std::move(autoentitiesResult1.error()));
  auto entities = std::move((*autoentitiesResult1));
  auto geometry = m_worldServer.geometry();
  entities.filter([positions, geometry, clearAnchoredObjects](std::shared_ptr<Entity> const& entity) {
    auto tileEntity = as<TileEntity>(entity);
    for (auto pos : tileEntity->spaces()) {
      if (positions.contains(geometry.xwrap(pos + tileEntity->tilePosition())))
        return true;
    }
    if (clearAnchoredObjects) {
      for (auto pos : tileEntity->roots()) {
        if (positions.contains(geometry.xwrap(pos + tileEntity->tilePosition())))
          return true;
      }
      if (auto object = as<Object>(entity)) {
        for (auto pos : object->anchorPositions()) {
          if (positions.contains(geometry.xwrap(pos)))
            return true;
        }
      }
    }

    return false;
  });

  for (auto const& entity : entities) {
    auto entityRemoved = m_worldServer.removeEntity(entity->entityId(), false);
    if (!entityRemoved)
      return std::unexpected(std::move(entityRemoved.error()));
  }

  return {};
}

SpawnerWorld::SpawnerWorld(WorldServer& server)
  : m_worldServer(server) {}

WorldGeometry SpawnerWorld::geometry() const {
  return m_worldServer.geometry();
}

List<RectF> SpawnerWorld::clientWindows() const {
  List<RectF> windows;
  for (auto clientId : m_worldServer.clientIds())
    windows.append(m_worldServer.clientWindow(clientId));
  return windows;
}

std::expected<bool, StarException> SpawnerWorld::signalRegion(RectF const& region) const {
  {
  auto conditionResult1 = m_worldServer.signalRegion(RectI::integral(region));
  if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
  auto condition = std::move((*conditionResult1));
  return static_cast<bool>(condition);

  }
}

std::expected<CollisionKind, StarException> SpawnerWorld::collision(Vec2I const& position) const {
  auto returnedResult1 = m_worldServer.getServerTile(position + Vec2I(0, 1));
  if (!returnedResult1) return std::unexpected(std::move(returnedResult1.error()));
  auto returnedValue = std::move(returnedResult1->get().collision);

  return returnedValue;
}

std::expected<bool, StarException> SpawnerWorld::isFreeSpace(RectF const& area) const {
  {
  auto conditionResult1 = m_worldServer.polyCollision(PolyF(area));
  if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
  auto condition = std::move(!(*conditionResult1));
  return static_cast<bool>(condition);

  }
}

std::expected<bool, StarException> SpawnerWorld::isBackgroundEmpty(Vec2I const& pos) const {
  {
  auto conditionResult1 = m_worldServer.getServerTile(pos);
  if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
  auto condition = std::move(conditionResult1->get().background == EmptyMaterialId);
  return static_cast<bool>(condition);

  }
}

LiquidLevel SpawnerWorld::liquidLevel(Vec2I const& position) const {
  return m_worldServer.liquidLevel(position);
}

std::expected<bool, StarException> SpawnerWorld::spawningProhibited(RectF const& area) const {
  RectI region = RectI::integral(area);

  // Don't spawn the entity if its region overlaps with a dungeon
  for (int x = region.xMin(); x < region.xMax(); ++x) {
    for (int y = region.yMin(); y < region.yMax(); ++y) {
      auto autoconsttileResult1 = m_worldServer.getServerTile({x, y});
      if (!autoconsttileResult1) return std::unexpected(std::move(autoconsttileResult1.error()));
      auto const& tile = std::move(autoconsttileResult1->get());
      if (tile.getCollision() == CollisionKind::Null || tile.dungeonId != NoDungeonId)
        return true;
    }
  }

  return false;
}

std::uint64_t SpawnerWorld::spawnSeed() const {
  return m_worldServer.worldTemplate()->worldSeed();
}

std::expected<SpawnProfile, StarException> SpawnerWorld::spawnProfile(Vec2F const& position) const {
  auto const& worldTemplate = m_worldServer.worldTemplate();
  Vec2I ipos = Vec2I::floor(position);
  // Block biome, *not* environment biome, includes things like detached
  // biomes.
  auto biomeCheckedResult1 = worldTemplate->blockBiome(ipos[0], ipos[1]);
  if (!biomeCheckedResult1) return std::unexpected(std::move(biomeCheckedResult1.error()));
  if (auto biome = std::move((*biomeCheckedResult1))) {
    // Dungeons, including ConstructionDungeonId (player constructed areas)
    // should be immune from spawning.
    auto autotileResult1 = m_worldServer.getServerTile(ipos);
    if (!autotileResult1) return std::unexpected(std::move(autotileResult1.error()));
    auto tile = std::move(autotileResult1->get());
    if (tile.dungeonId == NoDungeonId)
      return biome->spawnProfile;
  }
  return {};
}

float SpawnerWorld::dayLevel() const {
  return m_worldServer.sky()->dayLevel();
}

std::expected<float, StarException> SpawnerWorld::threatLevel() const {
  auto returnedResult1 = m_worldServer.threatLevel();
  if (!returnedResult1) return std::unexpected(std::move(returnedResult1.error()));
  auto returnedValue = std::move((*returnedResult1));

  return returnedValue;
}

std::expected<EntityId, StarException> SpawnerWorld::spawnEntity(std::shared_ptr<Entity> entity) const {
  {
    auto operationResult1 = m_worldServer.addEntity(entity);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return entity->entityId();
}

std::expected<void, StarException> SpawnerWorld::despawnEntity(EntityId entityId) {
  {
    auto operationResult1 = m_worldServer.removeEntity(entityId, false);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }

  return {};
}

std::shared_ptr<Entity> SpawnerWorld::getEntity(EntityId entityId) const {
  return m_worldServer.entity(entityId);
}

WorldGenerator::WorldGenerator(WorldServer& server) : m_worldServer(server) {
  m_microDungeonFactory = std::make_shared<MicroDungeonFactory>(m_worldServer.root());
}

std::expected<void, StarException> WorldGenerator::generateSectorLevel(WorldStorage& worldStorage, Sector const& sector, SectorGenerationLevel generationLevel) {
  if (generationLevel == SectorGenerationLevel::BaseTiles) {
    {
      auto operationResult1 = prepareTiles(worldStorage, sector);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  } else if (generationLevel == SectorGenerationLevel::MicroDungeons) {
    if (!worldStorage.floatingDungeonWorld())
      {
        auto operationResult1 = generateMicroDungeons(worldStorage, sector);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
  } else if (generationLevel == SectorGenerationLevel::CaveLiquid) {
    if (!worldStorage.floatingDungeonWorld())
      {
        auto operationResult1 = generateCaveLiquid(worldStorage, sector);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
  } else if (generationLevel == SectorGenerationLevel::Finalize) {
    if (!worldStorage.floatingDungeonWorld())
      {
        auto operationResult1 = prepareSector(worldStorage, sector);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
    else
      {
        auto operationResult1 = prepareSectorBiomeBlocks(worldStorage, sector);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
    {
      auto operationResult1 = m_worldServer.activateLiquidRegion(worldStorage.tileArray()->sectorRegion(sector));
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::sectorLoadLevelChanged(WorldStorage& worldStorage, Sector const& sector, SectorLoadLevel loadLevel) {
  if (loadLevel == SectorLoadLevel::Loaded) {
    if (worldStorage.sectorGenerationLevel(sector) == SectorGenerationLevel::Complete)
      {
        auto operationResult1 = m_worldServer.activateLiquidRegion(worldStorage.tileArray()->sectorRegion(sector));
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::terraformSector(WorldStorage& worldStorage, Sector const& sector) {
  {
    auto operationResult1 = reapplyBiome(worldStorage, sector);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::initEntity(WorldStorage&, EntityId entityId, std::shared_ptr<Entity> const& entity) {
  auto initialized = entity->init(m_worldServer, entityId, EntityMode::Master);
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  if (auto tileEntity = as<TileEntity>(entity))
    {
      auto operationResult1 = m_worldServer.updateTileEntityTiles(tileEntity, false, false);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }

  return {};
}

std::expected<void, StarException> WorldGenerator::destructEntity(WorldStorage&, std::shared_ptr<Entity> const& entity) {
  if (entity->isSlave())
    return std::unexpected(StarException("Cannot destruct slave entity in WorldStorage, something has gone wrong!"));
  if (auto tileEntity = as<TileEntity>(entity))
    {
      auto operationResult1 = m_worldServer.updateTileEntityTiles(tileEntity, true, false);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  auto uninitialized = entity->uninit();
  if (!uninitialized) return std::unexpected(std::move(uninitialized.error()));

  return {};
}

std::expected<bool, StarException> WorldGenerator::entityKeepAlive(WorldStorage&, std::shared_ptr<Entity> const& entity) const {
  return entity->isSlave() || (entity->isMaster() && entity->keepAlive());
}

std::expected<bool, StarException> WorldGenerator::entityPersistent(WorldStorage&, std::shared_ptr<Entity> const& entity) const {
  return entity->isMaster() && entity->persistent();
}

std::expected<RpcPromise<Vec2I>, StarException> WorldGenerator::enqueuePlacement(List<BiomeItemDistribution> distributions, std::optional<DungeonId> id) {
  auto promise = RpcPromise<Vec2I>::createPair();
  m_queuedPlacements.append(QueuedPlacement{
    .distributions = std::move(distributions),
    .dungeonId = id,
    .promise = promise.second,
    .fulfilled = false,
  });
  return promise.first;
}

std::expected<void, StarException> WorldGenerator::replaceBiomeBlocks(ServerTile& tile) {
  auto& generationRoot = m_worldServer.root();
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto automaterialDatabaseResult1 = generationRoot.materialDatabase();
  if (!automaterialDatabaseResult1) return std::unexpected(std::move(automaterialDatabaseResult1.error()));
  auto materialDatabase = std::move((*automaterialDatabaseResult1));
  MaterialId oldForeground = tile.foreground;

  auto blockBiomeCheckedResult1 = worldTemplate->biome(tile.blockBiomeIndex);
  if (!blockBiomeCheckedResult1) return std::unexpected(std::move(blockBiomeCheckedResult1.error()));
  if (auto blockBiome = std::move((*blockBiomeCheckedResult1))) {
    if (tile.foreground == BiomeMaterialId) {
      tile.foreground = blockBiome->mainBlock;
      auto tileforegroundHueShiftResult1 = worldTemplate->biomeMaterialHueShift(tile.blockBiomeIndex, tile.foreground);
      if (!tileforegroundHueShiftResult1) return std::unexpected(std::move(tileforegroundHueShiftResult1.error()));
      tile.foregroundHueShift = std::move((*tileforegroundHueShiftResult1));
    } else if ((tile.foreground >= Biome1MaterialId) && (tile.foreground <= Biome5MaterialId)) {
      auto& subblocks = blockBiome->subBlocks;
      if (subblocks.size())
        tile.foreground = subblocks[(tile.foreground - Biome1MaterialId) % subblocks.size()];
      else
        tile.foreground = blockBiome->mainBlock;
      auto tileforegroundHueShiftResult1 = worldTemplate->biomeMaterialHueShift(tile.blockBiomeIndex, tile.foreground);
      if (!tileforegroundHueShiftResult1) return std::unexpected(std::move(tileforegroundHueShiftResult1.error()));
      tile.foregroundHueShift = std::move((*tileforegroundHueShiftResult1));
    }

    if (tile.background == BiomeMaterialId) {
      tile.background = blockBiome->mainBlock;
      auto tilebackgroundHueShiftResult1 = worldTemplate->biomeMaterialHueShift(tile.blockBiomeIndex, tile.background);
      if (!tilebackgroundHueShiftResult1) return std::unexpected(std::move(tilebackgroundHueShiftResult1.error()));
      tile.backgroundHueShift = std::move((*tilebackgroundHueShiftResult1));
    } else if ((tile.background >= Biome1MaterialId) && (tile.background <= Biome5MaterialId)) {
      auto& subblocks = blockBiome->subBlocks;
      if (subblocks.size())
        tile.background = subblocks[(tile.background - Biome1MaterialId) % subblocks.size()];
      else
        tile.background = blockBiome->mainBlock;
      auto tilebackgroundHueShiftResult1 = worldTemplate->biomeMaterialHueShift(tile.blockBiomeIndex, tile.background);
      if (!tilebackgroundHueShiftResult1) return std::unexpected(std::move(tilebackgroundHueShiftResult1.error()));
      tile.backgroundHueShift = std::move((*tilebackgroundHueShiftResult1));
    }
  } else {
    if (isBiomeMaterial(tile.foreground)) {
      tile.foreground = EmptyMaterialId;
      tile.foregroundHueShift = 0;
    }
    if (isBiomeMod(tile.foregroundMod)) {
      tile.foregroundMod = NoModId;
      tile.foregroundModHueShift = 0;
    }
    if (isBiomeMaterial(tile.background)) {
      tile.background = EmptyMaterialId;
      tile.backgroundHueShift = 0;
    }
    if (isBiomeMod(tile.backgroundMod)) {
      tile.backgroundMod = NoModId;
      tile.backgroundModHueShift = 0;
    }
  }
  {
    auto operationResult1 = biomeForegroundCollision(materialDatabase, oldForeground, tile.foreground, tile.collision);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    tile.updateCollision((*operationResult1));
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::prepareTiles(WorldStorage& worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto& generationRoot = m_worldServer.root();
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto automaterialDatabaseResult1 = generationRoot.materialDatabase();
  if (!automaterialDatabaseResult1) return std::unexpected(std::move(automaterialDatabaseResult1.error()));
  auto materialDatabase = std::move((*automaterialDatabaseResult1));
  auto planet = worldTemplate;
  // Generate sector.
  auto tileArray = worldStorage.tileArray();
  RectI sectorRegion = tileArray->sectorRegion(sector);
  for (int x = sectorRegion.xMin(); x < sectorRegion.xMax(); ++x) {
    for (int y = sectorRegion.yMin(); y < sectorRegion.yMax(); ++y) {
      Vec2I pos(x, y);
      auto tile = tileArray->modifyTile(pos);
      if constexpr (Star::DebugEnabled) {
        Star::starAssertFailed((tile));
      }
      if (!tile)
        continue;
      auto autoblockInfoResult1 = planet->blockInfo(pos[0], pos[1]);
      if (!autoblockInfoResult1) return std::unexpected(std::move(autoblockInfoResult1.error()));
      auto blockInfo = std::move((*autoblockInfoResult1));

      tile->get().blockBiomeIndex = blockInfo.blockBiomeIndex;
      tile->get().environmentBiomeIndex = blockInfo.environmentBiomeIndex;
      tile->get().biomeTransition = blockInfo.biomeTransition;

      if (tile->get().foreground == NullMaterialId) {
        tile->get().foreground = blockInfo.foreground;
        tile->get().foregroundColorVariant = DefaultMaterialColorVariant;
        auto tileforegroundHueShiftResult1 = planet->biomeMaterialHueShift(tile->get().blockBiomeIndex, tile->get().foreground);
        if (!tileforegroundHueShiftResult1) return std::unexpected(std::move(tileforegroundHueShiftResult1.error()));
        tile->get().foregroundHueShift = std::move((*tileforegroundHueShiftResult1));

        if (auto condition89 = [&]() -> std::expected<bool, StarException> {
                    auto conditionResult1 = materialDatabase->supportsMod(tile->get().foreground, blockInfo.foregroundMod);
          if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
          auto condition = std::move((*conditionResult1));
          return static_cast<bool>(condition);
        }(); !condition89)
          return std::unexpected(std::move(condition89.error()));
        else if (*condition89) {
          tile->get().foregroundMod = blockInfo.foregroundMod;
          auto tileforegroundModHueShiftResult1 = planet->biomeModHueShift(tile->get().blockBiomeIndex, tile->get().foregroundMod);
          if (!tileforegroundModHueShiftResult1) return std::unexpected(std::move(tileforegroundModHueShiftResult1.error()));
          tile->get().foregroundModHueShift = std::move((*tileforegroundModHueShiftResult1));
        }
      }

      if (tile->get().background == NullMaterialId) {
        tile->get().background = blockInfo.background;
        tile->get().backgroundColorVariant = DefaultMaterialColorVariant;
        auto tilebackgroundHueShiftResult1 = planet->biomeMaterialHueShift(tile->get().blockBiomeIndex, tile->get().background);
        if (!tilebackgroundHueShiftResult1) return std::unexpected(std::move(tilebackgroundHueShiftResult1.error()));
        tile->get().backgroundHueShift = std::move((*tilebackgroundHueShiftResult1));

        if (auto condition88 = [&]() -> std::expected<bool, StarException> {
                    auto conditionResult1 = materialDatabase->supportsMod(tile->get().background, blockInfo.backgroundMod);
          if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
          auto condition = std::move((*conditionResult1));
          return static_cast<bool>(condition);
        }(); !condition88)
          return std::unexpected(std::move(condition88.error()));
        else if (*condition88) {
          tile->get().backgroundMod = blockInfo.backgroundMod;
          auto tilebackgroundModHueShiftResult1 = planet->biomeModHueShift(tile->get().blockBiomeIndex, tile->get().backgroundMod);
          if (!tilebackgroundModHueShiftResult1) return std::unexpected(std::move(tilebackgroundModHueShiftResult1.error()));
          tile->get().backgroundModHueShift = std::move((*tilebackgroundModHueShiftResult1));
        }
      }

      if (tile->get().foreground != EmptyMaterialId) {
        tile->get().liquid = LiquidStore();
      } else if (blockInfo.oceanLiquid != EmptyLiquidId && pos[1] < blockInfo.oceanLiquidLevel) {
        float pressure = blockInfo.oceanLiquidLevel - pos[1];
        if (tile->get().background == EmptyMaterialId)
          tile->get().liquid = LiquidStore::endless(blockInfo.oceanLiquid, pressure);
        else if (blockInfo.encloseLiquids)
          tile->get().liquid = LiquidStore::filled(blockInfo.oceanLiquid, 1.0f, pressure);
      }
    }
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::generateMicroDungeons(WorldStorage& worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto facade = std::make_shared<DungeonGeneratorWorld>(m_worldServer, false);

  RectI sectorTiles = worldStorage.tileArray()->sectorRegion(sector);
  RectI bounds = sectorTiles.padded(WorldSectorSize - 1);

  struct PlacementEntry {
    BiomeItemPlacement placement;
    std::optional<std::reference_wrapper<QueuedPlacement>> queued;
    std::size_t queueOrder;
  };
  List<PlacementEntry> placementQueue;
  for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
    for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
      auto autopotentialResult1 = worldTemplate->potentialBiomeItemsAt(x, y);
      if (!autopotentialResult1) return std::unexpected(std::move(autopotentialResult1.error()));
      auto potential = std::move((*autopotentialResult1));
      auto rangeValuesResult1 = worldTemplate->validBiomeItems(x, y, potential);
      if (!rangeValuesResult1) return std::unexpected(std::move(rangeValuesResult1.error()));
      auto rangeValues = std::move((*rangeValuesResult1));
      for (auto const& placement : rangeValues)
        placementQueue.append({.placement = placement, .queued = {}, .queueOrder = 0});

      for (std::size_t queueIndex = 0; queueIndex < m_queuedPlacements.size(); ++queueIndex) {
        auto& p = m_queuedPlacements[queueIndex];
        WorldTemplate::PotentialBiomeItems queuedItems;
        {
          auto operationResult1 = worldTemplate->addPotentialBiomeItems(x, y, queuedItems, p.distributions, BiomePlacementArea::Surface);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
        {
          auto operationResult1 = worldTemplate->addPotentialBiomeItems(x, y, queuedItems, p.distributions, BiomePlacementArea::Underground);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
        auto rangeValuesResult1 = worldTemplate->validBiomeItems(x, y, queuedItems);
        if (!rangeValuesResult1) return std::unexpected(std::move(rangeValuesResult1.error()));
        auto rangeValues = std::move((*rangeValuesResult1));
        for (auto placement : rangeValues)
          placementQueue.append(
            {.placement = std::move(placement), .queued = std::ref(p), .queueOrder = queueIndex + 1});
      }
    }
  }

  std::sort(placementQueue.begin(), placementQueue.end(), [](auto const& a, auto const& b) {
    if (a.placement < b.placement) return true;
    if (b.placement < a.placement) return false;
    // Preserve null-before-queued and contiguous queue order without address borrows.
    return a.queueOrder < b.queueOrder;
  });
  for (auto const& p : placementQueue) {
    auto const& placement = p.placement;
    auto queued = p.queued;
    if (queued && queued->get().fulfilled)
      continue;

    if (std::holds_alternative<MicroDungeonNames>(placement.item)) {
      auto seed = worldTemplate->seedFor(placement.position[0], placement.position[1]);
      auto autoconstdungeonNameResult1 = staticRandomFrom(std::get<MicroDungeonNames>(placement.item), seed);
      if (!autoconstdungeonNameResult1) return std::unexpected(std::move(autoconstdungeonNameResult1.error()));
      auto const& dungeonName = std::move(autoconstdungeonNameResult1->get());
      std::optional<DungeonId> dungeonId;
      if constexpr (Star::DebugEnabled) {
        Star::starAssertFailed((!dungeonName.empty()));
      }
      auto generateResultCheckedResult1 = m_worldServer.threatLevel();
      if (!generateResultCheckedResult1) return std::unexpected(std::move(generateResultCheckedResult1.error()));
      auto generateResultCheckedResult2 = m_microDungeonFactory->generate(bounds, dungeonName, placement.position, seed, (*generateResultCheckedResult1), facade);
      if (!generateResultCheckedResult2) return std::unexpected(std::move(generateResultCheckedResult2.error()));
      if (auto generateResult = std::move((*generateResultCheckedResult2))) {
        if (queued) {
          dungeonId = queued->get().dungeonId;
          {
            auto operationResult1 = queued->get().promise.fulfill(placement.position);
            if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          }
          queued->get().fulfilled = true;
        }
        for (auto position : generateResult->second) {
          auto tileCheckedResult1 = m_worldServer.modifyServerTile(position);
          if (!tileCheckedResult1) return std::unexpected(std::move(tileCheckedResult1.error()));
          if (auto tile = std::move((*tileCheckedResult1))) {
            {
              auto operationResult1 = replaceBiomeBlocks(tile->get());
              if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
            }
            tile->get().dungeonId = dungeonId.value_or(tile->get().dungeonId);
          }
        }
      }
    }
  }
  
  // placementQueue's borrows are no longer used once compaction begins.
  auto fulfilledPlacements = std::ranges::remove_if(m_queuedPlacements, [](QueuedPlacement const& p) {
    return p.fulfilled;
  });
  m_queuedPlacements.erase(fulfilledPlacements.begin(), fulfilledPlacements.end());

  return {};
}

std::expected<void, StarException> WorldGenerator::generateCaveLiquid(WorldStorage& worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto& generationRoot = m_worldServer.root();
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto SetVec2IopenNodesResult1 = caveLiquidSeeds(worldStorage, sector);
  if (!SetVec2IopenNodesResult1) return std::unexpected(std::move(SetVec2IopenNodesResult1.error()));
  Set<Vec2I> openNodes = std::move((*SetVec2IopenNodesResult1));

  if (!openNodes.size())
    return {};

  auto tileArray = worldStorage.tileArray();

  Vec2I dimensions(tileArray->size());

  auto wrapCoords = [=](Vec2I const& coord) -> Vec2I { return Vec2I{pmod<int>(coord[0], dimensions[0]), coord[1]}; };

  RectI sectorTiles = tileArray->sectorRegion(sector);
  RectI bounds = sectorTiles.padded(Vec2I(WorldSectorSize - 1, WorldSectorSize - 1));

  bounds.min()[1] = clamp<int>(bounds.min()[1], 0, dimensions[1] - 1);
  bounds.max()[1] = clamp<int>(bounds.max()[1], 0, dimensions[1] - 1);
  auto automaterialDatabaseResult1 = generationRoot.materialDatabase();
  if (!automaterialDatabaseResult1) return std::unexpected(std::move(automaterialDatabaseResult1.error()));
  auto materialDatabase = std::move((*automaterialDatabaseResult1));

  auto samplePoint = sectorTiles.center();
  auto autoblockInfoResult1 = worldTemplate->blockInfo(samplePoint[0], samplePoint[1]);
  if (!autoblockInfoResult1) return std::unexpected(std::move(autoblockInfoResult1.error()));
  auto blockInfo = std::move((*autoblockInfoResult1));
  LiquidId fillLiquid = blockInfo.caveLiquid;
  bool fillMicrodungeons = blockInfo.fillMicrodungeons;
  bool encloseLiquids = blockInfo.encloseLiquids;

  Set<Vec2I> badNodes;

  for (int i = bounds.xMin(); i <= bounds.xMax(); i++) {
    badNodes.add({i, bounds.yMin()});
    badNodes.add({i, bounds.yMax()});
  }
  for (int i = bounds.yMin(); i <= bounds.yMax(); i++) {
    badNodes.add({bounds.xMin(), i});
    badNodes.add({bounds.xMax(), i});
  }

  Set<Vec2I> candidateNodes;

  auto propose = [&](Vec2I const& position) -> std::expected<void, StarException> {
    if (!bounds.contains(position))
      return {};
    if (candidateNodes.contains(position))
      return {};
    if (badNodes.contains(position))
      return {};
    auto tile = tileArray->tile(wrapCoords(position));
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((tile.foreground != NullMaterialId));
    }
    if (tile.foreground != EmptyMaterialId) {
      // Not sure why this doesn't poison solid materials, but it does (occasionally) encounter that case
      if (auto condition90 = [&]() -> std::expected<bool, StarException> {
                auto conditionResult1 = materialDatabase->materialCollisionKind(tile.foreground);
        if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
        auto condition = std::move(!BlockCollisionSet.contains((*conditionResult1)));
        return static_cast<bool>(condition);
      }(); !condition90)
        return std::unexpected(std::move(condition90.error()));
      else if (*condition90)
        badNodes.add(position);
      return {};
    }
    if ((tile.dungeonId != NoDungeonId && (!fillMicrodungeons || tile.dungeonId != BiomeMicroDungeonId))
        || (!encloseLiquids && tile.background == EmptyMaterialId)
        || (tile.liquid.liquid != fillLiquid && tile.liquid.liquid != EmptyLiquidId)) {
      badNodes.add(position);
      return {};
    }
    candidateNodes.add(position);
    openNodes.add(position);
    return {};
  };

  while (openNodes.size()) {
    auto node = *openNodes.begin();
    openNodes.remove(node);
    {
      auto operationResult1 = propose(node + Vec2I(-1, 0));
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    {
      auto operationResult1 = propose(node + Vec2I(1, 0));
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    {
      auto operationResult1 = propose(node + Vec2I(0, -1));
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  }

  Set<Vec2I> visitedNodes;

  auto poison = [&](Vec2I position) {
    if (!bounds.contains(position))
      return;
    if (visitedNodes.contains(position))
      return;
    visitedNodes.add(position);
    auto tile = tileArray->tile(wrapCoords(position));
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((tile.foreground != NullMaterialId));
    }
    if (tile.foreground != EmptyMaterialId)
      return;
    badNodes.add(position);
  };

  while (badNodes.size()) {
    auto node = *badNodes.begin();
    badNodes.remove(node);
    candidateNodes.remove(node);
    poison(node + Vec2I(-1, 0));
    poison(node + Vec2I(1, 0));
    poison(node + Vec2I(0, 1)); // upwards, not downwards
  }

  Set<Vec2I> solidSurroundings(candidateNodes);

  auto solids = [&](Vec2I position) {
    auto tile = tileArray->tile(wrapCoords(position));
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((tile.foreground != NullMaterialId));
    }
    if (tile.foreground != EmptyMaterialId)
      solidSurroundings.add(position);
  };

  for (auto position : candidateNodes) {
    solids(position + Vec2I(1, 0));
    solids(position + Vec2I(-1, 0));
    solids(position + Vec2I(0, 1));
    solids(position + Vec2I(0, -1));
  }
  auto MaterialIdbiomeBlockResult1 = worldTemplate->biome(tileArray->tile(samplePoint).blockBiomeIndex);
  if (!MaterialIdbiomeBlockResult1) return std::unexpected(std::move(MaterialIdbiomeBlockResult1.error()));
  MaterialId biomeBlock = std::move((*MaterialIdbiomeBlockResult1)->mainBlock);
  Map<Vec2I, float> drops = determineLiquidLevel(candidateNodes, solidSurroundings);
  for (auto iter = drops.begin(); iter != drops.end(); ++iter) {
    auto tile = tileArray->modifyTile(wrapCoords(iter->first));
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((tile));
    }
    if (!tile)
      continue;
    if (iter->second)
      tile->get().liquid = LiquidStore::filled(fillLiquid, 1.0f, iter->second);
    if (encloseLiquids && tile->get().background == EmptyMaterialId)
      tile->get().background = biomeBlock;
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::prepareSector(WorldStorage& worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto& generationRoot = m_worldServer.root();
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto automaterialDatabaseResult1 = generationRoot.materialDatabase();
  if (!automaterialDatabaseResult1) return std::unexpected(std::move(automaterialDatabaseResult1.error()));
  auto materialDatabase = std::move((*automaterialDatabaseResult1));
  auto planet = worldTemplate;
  auto tileArray = worldStorage.tileArray();
  RectI sectorTiles = tileArray->sectorRegion(sector);

  for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
    for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
      Vec2I position(x, y);
      auto tile = tileArray->modifyTile(position);
      if constexpr (Star::DebugEnabled) {
        Star::starAssertFailed((tile));
      }
      if (!tile)
        continue;
      if constexpr (Star::DebugEnabled) {
        Star::starAssertFailed((tile->get().foreground != NullMaterialId));
      }

      if (tile->get().liquid.source) {
        auto autoblockInfoResult1 = planet->blockInfo(position[0], position[1]);
        if (!autoblockInfoResult1) return std::unexpected(std::move(autoblockInfoResult1.error()));
        auto blockInfo = std::move((*autoblockInfoResult1));
        // make sure that ocean liquid never exists on tiles without empty
        // background (except in real dungeons)
        if (!isRealDungeon(tile->get().dungeonId) && tile->get().background != EmptyMaterialId)
          tile->get().liquid.source = false;
        // pressurize liquid under the ocean
        if (blockInfo.oceanLiquid != EmptyLiquidId && position[1] < blockInfo.oceanLiquidLevel) {
          float pressure = blockInfo.oceanLiquidLevel - position[1];
          tile->get().liquid.pressure = pressure;
        }
      }

      if (!isRealMaterial(tile->get().foreground))
        tile->get().foregroundColorVariant = DefaultMaterialColorVariant;
      if (!isRealMaterial(tile->get().background))
        tile->get().backgroundColorVariant = DefaultMaterialColorVariant;
      {
        auto operationResult1 = replaceBiomeBlocks(tile->get());
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
      {
        auto operationResult1 = placeBiomeGrass(worldStorage, tile->get(), position);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
      {
        auto operationResult1 = biomeForegroundCollision(materialDatabase, tile->get().foreground, tile->get().foreground, tile->get().collision);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        tile->get().updateCollision((*operationResult1));
      }
    }
  }

  List<BiomeItemPlacement> placementQueue;
  for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
    for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
      auto tile = tileArray->tile(Vec2I(x, y));
      if (tile.dungeonId == NoDungeonId) {
        auto autopotentialResult1 = worldTemplate->potentialBiomeItemsAt(x, y);
        if (!autopotentialResult1) return std::unexpected(std::move(autopotentialResult1.error()));
        auto potential = std::move((*autopotentialResult1));
        auto rangeValuesResult1 = worldTemplate->validBiomeItems(x, y, potential);
        if (!rangeValuesResult1) return std::unexpected(std::move(rangeValuesResult1.error()));
        auto rangeValues = std::move((*rangeValuesResult1));
        for (auto const& placement : rangeValues)
          placementQueue.append(placement);
      }
    }
  }

  std::sort(placementQueue.begin(), placementQueue.end(), std::less<BiomeItemPlacement>());
  for (auto const& placement : placementQueue) {
    auto seed = worldTemplate->seedFor(placement.position[0], placement.position[1]);
    if (std::holds_alternative<GrassVariant>(placement.item)) {
      auto& grass = std::get<GrassVariant>(placement.item);
      {
        auto operationResult1 = generationRoot.plantDatabase();
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        auto operationResult2 = (*operationResult1)->createPlant(grass, seed);
        if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        auto operationResult3 = placePlant(worldStorage, (*operationResult2), placement.position);
        if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
      }
    } else if (std::holds_alternative<BushVariant>(placement.item)) {
      auto& bush = std::get<BushVariant>(placement.item);
      {
        auto operationResult1 = generationRoot.plantDatabase();
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        auto operationResult2 = (*operationResult1)->createPlant(bush, seed);
        if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        auto operationResult3 = placePlant(worldStorage, (*operationResult2), placement.position);
        if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
      }
    } else if (std::holds_alternative<TreePair>(placement.item)) {
      auto& treePair = std::get<TreePair>(placement.item);
      TreeVariant treeVariant;
      if (seed % 2 == 0)
        treeVariant = treePair.first;
      else
        treeVariant = treePair.second;
      {
        auto operationResult1 = generationRoot.plantDatabase();
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        auto operationResult2 = (*operationResult1)->createPlant(treeVariant, seed);
        if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        auto operationResult3 = placePlant(worldStorage, (*operationResult2), placement.position);
        if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
      }
    } else if (std::holds_alternative<ObjectPool>(placement.item)) {
      auto& objectPool = std::get<ObjectPool>(placement.item);
      auto direction = seed % 2 ? Direction::Left : Direction::Right;
      auto objectPair = objectPool.select(seed);
      auto objectCheckedResult1 = generationRoot.objectDatabase();
      if (!objectCheckedResult1) return std::unexpected(std::move(objectCheckedResult1.error()));
      auto objectCheckedResult2 = (*objectCheckedResult1)->createForPlacement(
              m_worldServer, objectPair.first, placement.position, direction, objectPair.second);
      if (!objectCheckedResult2) return std::unexpected(std::move(objectCheckedResult2.error()));
      if (auto object = std::move((*objectCheckedResult2)))
        {
          auto operationResult1 = m_worldServer.addEntity(object);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
    } else if (std::holds_alternative<TreasureBoxSet>(placement.item)) {
      auto& treasureBoxSet = std::get<TreasureBoxSet>(placement.item);
      auto direction = seed % 2 ? Direction::Left : Direction::Right;
      auto treasureContainerCheckedResult1 = generationRoot.treasureDatabase();
      if (!treasureContainerCheckedResult1) return std::unexpected(std::move(treasureContainerCheckedResult1.error()));
      auto treasureContainerCheckedResult2 = (*treasureContainerCheckedResult1)->createTreasureChest(
              m_worldServer, treasureBoxSet, placement.position, direction, seed);
      if (!treasureContainerCheckedResult2) return std::unexpected(std::move(treasureContainerCheckedResult2.error()));
      if (auto treasureContainer = std::move((*treasureContainerCheckedResult2)))
        {
          auto operationResult1 = m_worldServer.addEntity(treasureContainer);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
    }
  }

  for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
    for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
      if (auto tile = worldStorage.tileArray()->modifyTile({x, y}))
        tile->get().collisionCacheDirty = true;
    }
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::prepareSectorBiomeBlocks(WorldStorage& worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto& generationRoot = m_worldServer.root();
  auto tileArray = worldStorage.tileArray();
  auto automaterialDatabaseResult1 = generationRoot.materialDatabase();
  if (!automaterialDatabaseResult1) return std::unexpected(std::move(automaterialDatabaseResult1.error()));
  auto materialDatabase = std::move((*automaterialDatabaseResult1));
  RectI sectorTiles = tileArray->sectorRegion(sector);

  for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
    for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
      Vec2I position(x, y);
      auto tile = tileArray->modifyTile(position);
      {
        auto operationResult1 = replaceBiomeBlocks(tile->get());
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
      {
        auto operationResult1 = placeBiomeGrass(worldStorage, tile->get(), position);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
      {
        auto operationResult1 = biomeForegroundCollision(materialDatabase, tile->get().foreground, tile->get().foreground, tile->get().collision);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        tile->get().updateCollision((*operationResult1));
      }
    }
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::placeBiomeGrass(WorldStorage& worldStorage, ServerTile& tile, Vec2I const& position) {
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto blockBiomeCheckedResult1 = worldTemplate->biome(tile.blockBiomeIndex);
  if (!blockBiomeCheckedResult1) return std::unexpected(std::move(blockBiomeCheckedResult1.error()));
  if (auto blockBiome = std::move((*blockBiomeCheckedResult1))) {
    // determine layer for grass mod calculation
    TileLayer modLayer = tile.foreground != EmptyMaterialId ? TileLayer::Foreground : TileLayer::Background;

    // don't place mods in dungeons unless explicitly specified, also don't
    // touch non-grass mods
    if (tile.mod(modLayer) == BiomeModId || tile.mod(modLayer) == UndergroundBiomeModId
        || (tile.dungeonId == NoDungeonId && tile.mod(modLayer) == NoModId)) {
      // check whether we're floor or ceiling
      auto tileAbove = worldStorage.tileArray()->tile(position + Vec2I(0, 1));
      auto tileBelow = worldStorage.tileArray()->tile(position + Vec2I(0, -1));
      bool isFloor = (tile.foreground != EmptyMaterialId && tileAbove.foreground == EmptyMaterialId) || (tile.background != EmptyMaterialId && tileAbove.background == EmptyMaterialId);
      bool isCeiling = !isFloor && ((tile.foreground != EmptyMaterialId && tileBelow.foreground == EmptyMaterialId) || (tile.background != EmptyMaterialId && tileBelow.background == EmptyMaterialId));

      // get the appropriate placeables for above/below ground
      BiomePlaceables const& placeables =
          ((isFloor && tileAbove.background != EmptyMaterialId) || (isCeiling && tileBelow.background != EmptyMaterialId))
          ? blockBiome->undergroundPlaceables : blockBiome->surfacePlaceables;

      // determine the proper grass mod or lack thereof
      ModId grassModId = NoModId;
      if (isFloor) {
        auto grassChance = staticRandomFloat(worldTemplate->worldSeed(), position[0], position[1]);
        if (isRealMod(placeables.grassMod) && grassChance <= placeables.grassModDensity)
          grassModId = placeables.grassMod;
      } else if (isCeiling) {
        auto grassChance = staticRandomFloat(worldTemplate->worldSeed(), position[0], position[1]);
        if (isRealMod(placeables.ceilingGrassMod) && grassChance <= placeables.ceilingGrassModDensity)
          grassModId = placeables.ceilingGrassMod;
      }

      // set the selected grass mod
      if (modLayer == TileLayer::Foreground) {
        tile.foregroundMod = grassModId;
        tile.backgroundMod = NoModId;
      } else {
        tile.foregroundMod = NoModId;
        tile.backgroundMod = grassModId;
      }
    }

    // update hue shifts appropriately
    auto tileforegroundModHueShiftResult1 = worldTemplate->biomeModHueShift(tile.blockBiomeIndex, tile.foregroundMod);
    if (!tileforegroundModHueShiftResult1) return std::unexpected(std::move(tileforegroundModHueShiftResult1.error()));
    tile.foregroundModHueShift = std::move((*tileforegroundModHueShiftResult1));
    auto tilebackgroundModHueShiftResult1 = worldTemplate->biomeModHueShift(tile.blockBiomeIndex, tile.backgroundMod);
    if (!tilebackgroundModHueShiftResult1) return std::unexpected(std::move(tilebackgroundModHueShiftResult1.error()));
    tile.backgroundModHueShift = std::move((*tilebackgroundModHueShiftResult1));
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::reapplyBiome(WorldStorage& worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto& generationRoot = m_worldServer.root();
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto const& worldLayout = worldTemplate->worldLayout();
  auto automaterialDatabaseResult1 = generationRoot.materialDatabase();
  if (!automaterialDatabaseResult1) return std::unexpected(std::move(automaterialDatabaseResult1.error()));
  auto materialDatabase = std::move((*automaterialDatabaseResult1));
  auto planet = worldTemplate;
  auto tileArray = worldStorage.tileArray();
  RectI sectorTiles = tileArray->sectorRegion(sector);

  // generationRoot.logger().info("Reapplying biome in sector {}...", sectorTiles);
  auto autoentitiesResult1 = m_worldServer.entityQuery(RectF(sectorTiles.padded(1)));
  if (!autoentitiesResult1) return std::unexpected(std::move(autoentitiesResult1.error()));
  auto entities = std::move((*autoentitiesResult1));
  List<std::shared_ptr<TileEntity>> biomeTileEntities;
  for (auto const& entity : entities) {
    if (auto plant = as<Plant>(entity)) {
      biomeTileEntities.append(as<TileEntity>(entity));
    } else if (auto object = as<Object>(entity)) {
      if (object->biomePlaced())
        biomeTileEntities.append(as<TileEntity>(entity));
    }
  }

  List<Vec2I> biomeItemTiles;

  for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
    for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
      Vec2I position(x, y);
      auto ServerTiletileResult1 = m_worldServer.modifyServerTile(position);
      if (!ServerTiletileResult1) return std::unexpected(std::move(ServerTiletileResult1.error()));
      auto tile = std::move((*ServerTiletileResult1));
      if constexpr (Star::DebugEnabled) {
        Star::starAssertFailed((tile));
      }
      if (!tile)
        continue;
      auto autoblockInfoResult1 = planet->blockBiomeInfo(position[0], position[1]);
      if (!autoblockInfoResult1) return std::unexpected(std::move(autoblockInfoResult1.error()));
      auto blockInfo = std::move((*autoblockInfoResult1));
      if (blockInfo.blockBiomeIndex != tile->get().blockBiomeIndex) {
        auto autonewBiomeResult1 = planet->biome(blockInfo.blockBiomeIndex);
        if (!autonewBiomeResult1) return std::unexpected(std::move(autonewBiomeResult1.error()));
        auto newBiome = std::move((*autonewBiomeResult1));
        auto autooldBiomeResult1 = planet->biome(tile->get().blockBiomeIndex);
        if (!autooldBiomeResult1) return std::unexpected(std::move(autooldBiomeResult1.error()));
        auto oldBiome = std::move((*autooldBiomeResult1));
        MaterialId oldForeground = tile->get().foreground;

        for (auto iter = biomeTileEntities.begin(); iter != biomeTileEntities.end();) {
          if ((*iter)->tilePosition() == position) {
            auto autoremovedResult1 = m_worldServer.removeEntity((*iter)->entityId(), false);
            if (!autoremovedResult1) return std::unexpected(std::move(autoremovedResult1.error()));
            iter = biomeTileEntities.erase(iter);
          } else {
            ++iter;
          }
        }

        // update biome index
        tile->get().blockBiomeIndex = blockInfo.blockBiomeIndex;
        tile->get().environmentBiomeIndex = blockInfo.environmentBiomeIndex;
        tile->get().biomeTransition = true;

        // replace biome blocks
        if (tile->get().foreground == oldBiome->mainBlock || oldBiome->subBlocks.contains(tile->get().foreground)) {
          tile->get().foreground = blockInfo.foreground;
          tile->get().foregroundColorVariant = DefaultMaterialColorVariant;
          if (tile->get().foreground == newBiome->mainBlock)
            tile->get().foregroundHueShift = newBiome->materialHueShift;
        }

        if (tile->get().background == oldBiome->mainBlock || oldBiome->subBlocks.contains(tile->get().background)) {
          tile->get().background = blockInfo.background;
          tile->get().backgroundColorVariant = DefaultMaterialColorVariant;
          if (tile->get().background == newBiome->mainBlock)
            tile->get().backgroundHueShift = newBiome->materialHueShift;
        }

        if (tile->get().foreground != EmptyMaterialId || tile->get().background != EmptyMaterialId) {
          // remove old biome mods
          if (tile->get().foregroundMod == oldBiome->surfacePlaceables.grassMod || tile->get().foregroundMod == oldBiome->surfacePlaceables.ceilingGrassMod ||
              tile->get().foregroundMod == oldBiome->undergroundPlaceables.grassMod || tile->get().foregroundMod == oldBiome->undergroundPlaceables.ceilingGrassMod) {

            tile->get().foregroundMod = NoModId;
            tile->get().foregroundModHueShift = 0;
          }

          if (tile->get().backgroundMod == oldBiome->surfacePlaceables.grassMod || tile->get().backgroundMod == oldBiome->surfacePlaceables.ceilingGrassMod ||
              tile->get().backgroundMod == oldBiome->undergroundPlaceables.grassMod || tile->get().backgroundMod == oldBiome->undergroundPlaceables.ceilingGrassMod) {

            tile->get().backgroundMod = NoModId;
            tile->get().backgroundModHueShift = 0;
          }

          // apply new biome mods
          TileLayer modLayer = tile->get().foreground != EmptyMaterialId ? TileLayer::Foreground : TileLayer::Background;

          if (tile->get().mod(modLayer) == NoModId) {
            // check whether we're floor or ceiling
            auto tileAbove = worldStorage.tileArray()->tile(position + Vec2I(0, 1));
            auto tileBelow = worldStorage.tileArray()->tile(position + Vec2I(0, -1));
            bool isFloor = tile->get().foreground != EmptyMaterialId && tileAbove.foreground == EmptyMaterialId;
            bool isCeiling = !isFloor && tile->get().foreground != EmptyMaterialId && tileBelow.foreground == EmptyMaterialId;
            bool isModFloor, isModCeiling;
            if (modLayer == TileLayer::Foreground) {
              isModFloor = isFloor;
              isModCeiling = isCeiling;
            } else {
              isModFloor = tile->get().background != EmptyMaterialId && tileAbove.background == EmptyMaterialId;
              isModCeiling = !isModFloor && tile->get().background != EmptyMaterialId && tileBelow.background == EmptyMaterialId;
            }

            // get the appropriate placeables for above/below ground
            BiomePlaceables const& placeables =
                ((isFloor && tileAbove.background != EmptyMaterialId) || (isCeiling && tileBelow.background != EmptyMaterialId))
                ? newBiome->undergroundPlaceables : newBiome->surfacePlaceables;

            // determine the proper grass mod or lack thereof
            ModId grassModId = NoModId;
            if (isModFloor) {
              auto grassChance = staticRandomFloat(worldTemplate->worldSeed(), position[0], position[1]);
              if (isRealMod(placeables.grassMod) && grassChance <= placeables.grassModDensity)
                grassModId = placeables.grassMod;
            } else if (isModCeiling) {
              auto grassChance = staticRandomFloat(worldTemplate->worldSeed(), position[0], position[1]);
              if (isRealMod(placeables.ceilingGrassMod) && grassChance <= placeables.ceilingGrassModDensity)
                grassModId = placeables.ceilingGrassMod;
            }

            // set the selected grass mod
            if (auto condition94 = [&]() -> std::expected<bool, StarException> {
              return [&]() -> std::expected<bool, StarException> {
                auto left = [&]() -> std::expected<bool, StarException> {                   auto condition = std::move(modLayer == TileLayer::Foreground);
                  return static_cast<bool>(condition); }();
                if (!left) return std::unexpected(std::move(left.error()));
                if (!*left) return false;
                                auto conditionResult1 = materialDatabase->supportsMod(tile->get().foreground, grassModId);
                if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
                auto condition = std::move((*conditionResult1));
                return static_cast<bool>(condition);
              }();
            }(); !condition94)
              return std::unexpected(std::move(condition94.error()));
            else if (*condition94) {
              tile->get().foregroundMod = grassModId;
              tile->get().backgroundMod = NoModId;
            } else if (auto condition93 = [&]() -> std::expected<bool, StarException> {
              return [&]() -> std::expected<bool, StarException> {
                auto left = [&]() -> std::expected<bool, StarException> {                   auto condition = std::move(modLayer == TileLayer::Background);
                  return static_cast<bool>(condition); }();
                if (!left) return std::unexpected(std::move(left.error()));
                if (!*left) return false;
                                auto conditionResult1 = materialDatabase->supportsMod(tile->get().background, grassModId);
                if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
                auto condition = std::move((*conditionResult1));
                return static_cast<bool>(condition);
              }();
            }(); !condition93)
              return std::unexpected(std::move(condition93.error()));
            else if (*condition93) {
              tile->get().foregroundMod = NoModId;
              tile->get().backgroundMod = grassModId;
            }
          }
        } else {
          tile->get().foregroundMod = NoModId;
          tile->get().backgroundMod = NoModId;
        }
        {
          auto operationResult1 = biomeForegroundCollision(materialDatabase, oldForeground, tile->get().foreground, tile->get().collision);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          tile->get().updateCollision((*operationResult1));
        }
      }

      if (tile->get().biomeTransition && !blockInfo.biomeTransition) {
        tile->get().biomeTransition = false;
        if (!isSolidColliding(tile->get().collision))
          biomeItemTiles.append(position);
      }
    }
  }

  auto simplePlacePlant = [&](std::shared_ptr<Plant> const& plant, Vec2I const& position) -> std::expected<bool, StarException> {
      if (!plant)
        return false;

      auto spaces = plant->spaces();
      auto roots = plant->roots();
      auto const& primaryRoot = plant->primaryRoot();
      auto autoblockBiomeResult1 = worldLayout->getBiome(worldStorage.tileArray()->tile(position).blockBiomeIndex);
      if (!autoblockBiomeResult1) return std::unexpected(std::move(autoblockBiomeResult1.error()));
      auto blockBiome = std::move(autoblockBiomeResult1->get());

      auto positionValid = [&](Vec2I const& pos) -> std::expected<bool, StarException> {
          auto primaryTile = worldStorage.tileArray()->tile(pos);
          auto primaryRootTile = worldStorage.tileArray()->tile(pos + primaryRoot);
          if (isConnectableMaterial(primaryTile.foreground) || !isConnectableMaterial(primaryRootTile.foreground))
            return false;

          for (auto root : roots) {
            auto rootTile = worldStorage.tileArray()->tile(root + pos);
            if (!isConnectableMaterial(rootTile.foreground) || rootTile.blockBiomeIndex != primaryTile.blockBiomeIndex ||
                (rootTile.foreground != blockBiome->mainBlock && !blockBiome->subBlocks.contains(rootTile.foreground)))
              return false;
          }

          for (auto space : spaces) {
            Vec2I pspace = space + pos;

            if (auto condition92 = [&]() -> std::expected<bool, StarException> {
                            auto conditionResult1 = m_worldServer.atTile<TileEntity>(pspace);
              if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
              auto condition = std::move(!(*conditionResult1).empty());
              return static_cast<bool>(condition);
            }(); !condition92)
              return std::unexpected(std::move(condition92.error()));
            else if (*condition92)
              return false;

            auto tile = worldStorage.tileArray()->tile(pspace);
            if (tile.foreground != EmptyMaterialId)
              return false;
          }

          return true;
        };

      List<Vec2I> tryPositions = {
          position,
          position + Vec2I{-1, 0},
          position + Vec2I{1, 0},
          position + Vec2I{-2, 0},
          position + Vec2I{2, 0},
          position + Vec2I{-1, 1},
          position + Vec2I{-1, -1},
          position + Vec2I{1, 1},
          position + Vec2I{1, -1}
        };

      for (auto pos : tryPositions) {
        if (auto condition91 = [&]() -> std::expected<bool, StarException> {
                    auto conditionResult1 = positionValid(pos);
          if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
          auto condition = std::move((*conditionResult1));
          return static_cast<bool>(condition);
        }(); !condition91)
          return std::unexpected(std::move(condition91.error()));
        else if (*condition91) {
          {
            auto operationResult1 = plant->setTilePosition(pos);
            if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          }
          {
            auto operationResult1 = m_worldServer.addEntity(plant);
            if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          }
          return true;
        }
      }

      return false;
    };

  auto placeBiomeItem = [&](BiomeItemPlacement biomeItemPlacement, Vec2I position) -> std::expected<void, StarException> {
      auto seed = worldTemplate->seedFor(position[0], position[1]);
      if (std::holds_alternative<GrassVariant>(biomeItemPlacement.item)) {
        auto& grass = std::get<GrassVariant>(biomeItemPlacement.item);
        {
          auto operationResult1 = generationRoot.plantDatabase();
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          auto operationResult2 = (*operationResult1)->createPlant(grass, seed);
          if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
          auto operationResult3 = simplePlacePlant((*operationResult2), position);
          if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
        }
      } else if (std::holds_alternative<BushVariant>(biomeItemPlacement.item)) {
        auto& bush = std::get<BushVariant>(biomeItemPlacement.item);
        {
          auto operationResult1 = generationRoot.plantDatabase();
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          auto operationResult2 = (*operationResult1)->createPlant(bush, seed);
          if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
          auto operationResult3 = simplePlacePlant((*operationResult2), position);
          if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
        }
      } else if (std::holds_alternative<TreePair>(biomeItemPlacement.item)) {
        auto& treePair = std::get<TreePair>(biomeItemPlacement.item);
        TreeVariant treeVariant;
        if (seed % 2 == 0)
          treeVariant = treePair.first;
        else
          treeVariant = treePair.second;
        {
          auto operationResult1 = generationRoot.plantDatabase();
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          auto operationResult2 = (*operationResult1)->createPlant(treeVariant, seed);
          if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
          auto operationResult3 = simplePlacePlant((*operationResult2), position);
          if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
        }
      } else if (std::holds_alternative<ObjectPool>(biomeItemPlacement.item)) {
        auto& objectPool = std::get<ObjectPool>(biomeItemPlacement.item);
        auto direction = seed % 2 ? Direction::Left : Direction::Right;
        auto objectPair = objectPool.select(seed);
        auto objectCheckedResult1 = generationRoot.objectDatabase();
        if (!objectCheckedResult1) return std::unexpected(std::move(objectCheckedResult1.error()));
        auto objectCheckedResult2 = (*objectCheckedResult1)->createForPlacement(m_worldServer, objectPair.first, position, direction, objectPair.second);
        if (!objectCheckedResult2) return std::unexpected(std::move(objectCheckedResult2.error()));
        if (auto object = std::move((*objectCheckedResult2))) {
          if (object->biomePlaced())
            {
              auto operationResult1 = m_worldServer.addEntity(object);
              if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
            }
        }
      }
      return {};
    };

  for (auto position : biomeItemTiles) {
    auto ServerTiletileResult1 = m_worldServer.modifyServerTile(position);
    if (!ServerTiletileResult1) return std::unexpected(std::move(ServerTiletileResult1.error()));
    auto tile = std::move((*ServerTiletileResult1));
    auto autoblockBiomeResult1 = worldLayout->getBiome(tile->get().blockBiomeIndex);
    if (!autoblockBiomeResult1) return std::unexpected(std::move(autoblockBiomeResult1.error()));
    auto blockBiome = std::move(autoblockBiomeResult1->get());
    auto autotileAboveResult1 = m_worldServer.getServerTile(position + Vec2I{0, 1});
    if (!autotileAboveResult1) return std::unexpected(std::move(autotileAboveResult1.error()));
    auto tileAbove = std::move(autotileAboveResult1->get());
    auto autotileBelowResult1 = m_worldServer.getServerTile(position + Vec2I{0, -1});
    if (!autotileBelowResult1) return std::unexpected(std::move(autotileBelowResult1.error()));
    auto tileBelow = std::move(autotileBelowResult1->get());

    if (tile->get().background != EmptyMaterialId) {
      for (auto const& itemDistribution : blockBiome->undergroundPlaceables.itemDistributions) {
        if (itemDistribution.mode() == BiomePlacementMode::Background) {
          auto itemToPlace = itemDistribution.itemToPlace(position[0], position[1]);
          if (!itemToPlace) return std::unexpected(std::move(itemToPlace.error()));
          if (*itemToPlace)
            {
              auto operationResult1 = placeBiomeItem(**itemToPlace, position);
              if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
            }
        }
      }

      if (isSolidColliding(tileAbove.collision)) {
        for (auto const& itemDistribution : blockBiome->undergroundPlaceables.itemDistributions) {
          if (itemDistribution.mode() == BiomePlacementMode::Ceiling) {
            auto itemToPlace = itemDistribution.itemToPlace(position[0], position[1]);
            if (!itemToPlace) return std::unexpected(std::move(itemToPlace.error()));
            if (*itemToPlace)
              {
                auto operationResult1 = placeBiomeItem(**itemToPlace, position);
                if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
              }
          }
        }
      }

      if (isSolidColliding(tileBelow.collision)) {
        for (auto const& itemDistribution : blockBiome->undergroundPlaceables.itemDistributions) {
          if (itemDistribution.mode() == BiomePlacementMode::Floor) {
            auto itemToPlace = itemDistribution.itemToPlace(position[0], position[1]);
            if (!itemToPlace) return std::unexpected(std::move(itemToPlace.error()));
            if (*itemToPlace)
              {
                auto operationResult1 = placeBiomeItem(**itemToPlace, position);
                if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
              }
          }
        }
      }
    } else {
      if (isSolidColliding(tileBelow.collision)) {
        for (auto const& itemDistribution : blockBiome->surfacePlaceables.itemDistributions) {
          if (itemDistribution.mode() == BiomePlacementMode::Floor) {
            auto itemToPlace = itemDistribution.itemToPlace(position[0], position[1]);
            if (!itemToPlace) return std::unexpected(std::move(itemToPlace.error()));
            if (*itemToPlace)
              {
                auto operationResult1 = placeBiomeItem(**itemToPlace, position);
                if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
              }
          }
        }
      }
    }
  }

  return {};
}

std::expected<Set<Vec2I>, StarException> WorldGenerator::caveLiquidSeeds(WorldStorage& worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto const& worldTemplate = m_worldServer.worldTemplate();
  RectI sectorTiles = worldStorage.tileArray()->sectorRegion(sector);
  auto samplePoint = sectorTiles.center();
  auto autoblockInfoResult1 = worldTemplate->blockInfo(samplePoint[0], samplePoint[1]);
  if (!autoblockInfoResult1) return std::unexpected(std::move(autoblockInfoResult1.error()));
  auto blockInfo = std::move((*autoblockInfoResult1));
  float seedDensity = blockInfo.caveLiquidSeedDensity;
  Set<Vec2I> nodes;
  if (seedDensity > 0) {
    int frequency = static_cast<int>(100 / seedDensity);
    for (int y = frequency * std::floor(sectorTiles.min()[1] / frequency); y < sectorTiles.max()[1]; y += frequency) {
      for (int x = frequency * std::floor(sectorTiles.min()[0] / frequency); x < sectorTiles.max()[0]; x += frequency) {
        if (sectorTiles.contains(Vec2I(x, y)))
          nodes.add(Vec2I(x, y));
      }
    }
  }
  return nodes;
}

Map<Vec2I, float> WorldGenerator::determineLiquidLevel(Set<Vec2I> const& spots, Set<Vec2I> const& filled) {
  Set<Vec2I> openSet(spots);
  Map<Vec2I, float> results;

  auto geometry = m_worldServer.geometry();

  while (openSet.size() > 0) {
    Set<Vec2I> cluster;
    Set<Vec2I> openCluster;
    openCluster.add(*(openSet.begin()));
    while (openCluster.size() > 0) {
      Vec2I node = *(openCluster.begin());
      openCluster.remove(node);
      if (openSet.contains(node)) {
        openSet.remove(node);
        cluster.add(node);
        openCluster.add(geometry.xwrap(Vec2I(node.x(), node.y() + 1)));
        openCluster.add(geometry.xwrap(Vec2I(node.x(), node.y() - 1)));
        openCluster.add(geometry.xwrap(Vec2I(node.x() + 1, node.y())));
        openCluster.add(geometry.xwrap(Vec2I(node.x() - 1, node.y())));
      }
    }
    levelCluster(cluster, filled, results);
  }
  return results;
}

void WorldGenerator::levelCluster(Set<Vec2I>& cluster, Set<Vec2I> const& filled, Map<Vec2I, float>& results) {
  int maxY = std::numeric_limits<int>::min();
  int minY = std::numeric_limits<int>::max();
  for (auto iter = cluster.begin(); iter != cluster.end(); iter++) {
    auto droplet = (*iter);
    if (filled.contains(droplet + Vec2I(1, 0)) && filled.contains(droplet + Vec2I(-1, 0))
        && filled.contains(droplet + Vec2I(0, -1))) {
      if (droplet.y() > maxY)
        maxY = droplet.y();
      if (!filled.contains(droplet + Vec2I(0, 1))) {
        if (droplet.y() <= minY)
          minY = droplet.y();
      }
    } else {
      if (droplet.y() <= minY)
        minY = droplet.y() - 1;
    }
  }
  int liquidLevel = std::min(maxY, minY);
  for (auto iter = cluster.begin(); iter != cluster.end(); iter++) {
    int pressure = (liquidLevel - (*iter).y());
    if (pressure >= 0)
      results[*iter] = 1.0f + pressure;
  }
}

std::expected<bool, StarException> WorldGenerator::placePlant(WorldStorage& worldStorage, std::shared_ptr<Plant> const& plant, Vec2I const& position) {
  auto& generationRoot = m_worldServer.root();
  if (!plant)
    return false;

  auto spaces = plant->spaces();
  auto roots = plant->roots();
  auto const& primaryRoot = plant->primaryRoot();
  auto autobackgroundResult1 = m_worldServer.getServerTile(position);
  if (!autobackgroundResult1) return std::unexpected(std::move(autobackgroundResult1.error()));
  auto background = std::move(autobackgroundResult1->get().background);
  bool adjustBackground = background == EmptyMaterialId || background == NullMaterialId;

  auto withinAdjustment = [=](Vec2I const& pos) {
    return PlantAdjustmentLimit - std::abs(pos[0]) > 0 && PlantAdjustmentLimit - std::abs(pos[1]) > 0;
  };

  // Bail out if we don't have at least one free space, and root in the primary
  // root position, or if we're in a dungeon region.
  auto primaryTile = worldStorage.tileArray()->tile(position);
  auto rootTile = worldStorage.tileArray()->tile(position + primaryRoot);
  if (primaryTile.dungeonId != NoDungeonId || rootTile.dungeonId != NoDungeonId)
    return false;
  if (isConnectableMaterial(primaryTile.foreground) || !isConnectableMaterial(rootTile.foreground))
    return false;

  // First bail out if we can't fit anything we're not adjusting
  for (auto space : spaces) {
    Vec2I pspace = space + position;

    if (auto condition95 = [&]() -> std::expected<bool, StarException> {
      return [&]() -> std::expected<bool, StarException> {
        auto left = [&]() -> std::expected<bool, StarException> {           auto condition = std::move(withinAdjustment(space));
          return static_cast<bool>(condition); }();
        if (!left) return std::unexpected(std::move(left.error()));
        if (!*left) return false;
                auto conditionResult1 = m_worldServer.atTile<Plant>(pspace);
        if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
        auto condition = std::move(!(*conditionResult1).empty());
        return static_cast<bool>(condition);
      }();
    }(); !condition95)
      return std::unexpected(std::move(condition95.error()));
    else if (*condition95)
      return false;

    // Bail out if we hit a different plant's root tile, or if we're not in the
    // adjustment space and we hit a non-empty tile.
    auto tile = worldStorage.tileArray()->tile(pspace);
    if (tile.rootSource || (!withinAdjustment(space) && tile.foreground != EmptyMaterialId))
      return false;
  }

  // Check all the roots outside of the adjustment limit
  for (auto root : roots) {
    root += position;
    if (!withinAdjustment(root) && !isConnectableMaterial(worldStorage.tileArray()->tile(root).foreground))
      return false;
  }

  // Clear all the necessary blocks within the adjustment limit
  for (auto space : spaces) {
    if (!withinAdjustment(space))
      continue;

    space += position;
    if (auto tile = worldStorage.tileArray()->modifyTile(space)) {
      if (isConnectableMaterial(tile->get().foreground))
        tile->get() = primaryTile;
      if (adjustBackground)
        tile->get().background = EmptyMaterialId;
      tile->get().collision = CollisionKind::None;
      tile->get().collisionCacheDirty = true;
    } else {
      return false;
    }
  }

  // Make all the root blocks a real material based on the primary root.
  for (auto root : roots) {
    root += position;
    if (auto tile = worldStorage.tileArray()->modifyTile(root)) {
      if (!isRealMaterial(tile->get().foreground)) {
        tile->get() = rootTile;
        auto tilecollisionResult1 = generationRoot.materialDatabase();
        if (!tilecollisionResult1) return std::unexpected(std::move(tilecollisionResult1.error()));
        auto tilecollisionResult2 = (*tilecollisionResult1)->materialCollisionKind(tile->get().foreground);
        if (!tilecollisionResult2) return std::unexpected(std::move(tilecollisionResult2.error()));
        tile->get().collision = std::move((*tilecollisionResult2));
        tile->get().collisionCacheDirty = true;
      }
    } else {
      return false;
    }
  }
  {
    auto operationResult1 = plant->setTilePosition(position);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = m_worldServer.addEntity(plant);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return true;
}

}
