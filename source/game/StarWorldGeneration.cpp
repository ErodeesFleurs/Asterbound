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
import star.world_impl;
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

bool is_preserved_tile_collision_override(CollisionKind collision) {
  return collision == CollisionKind::None
      || collision == CollisionKind::Platform
      || collision == CollisionKind::Block;
}

[[nodiscard]] std::expected<CollisionKind, StarException> biome_foreground_collision(
    MaterialDatabase const& materials, MaterialId old_foreground, MaterialId new_foreground, CollisionKind current_collision) {
  auto old_collision = materials.material_collision_kind(old_foreground);
  if (!old_collision) return std::unexpected(std::move(old_collision.error()));
  if (old_foreground == new_foreground)
    return is_preserved_tile_collision_override(current_collision) ? current_collision : *old_collision;
  auto new_collision = materials.material_collision_kind(new_foreground);
  if (!new_collision) return std::unexpected(std::move(new_collision.error()));

  if (current_collision != *old_collision && is_preserved_tile_collision_override(current_collision))
    return current_collision;

  return *new_collision;
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

static std::expected<std::shared_ptr<Object>, StarException> create_biome_object(
    WorldServer& world, ObjectPool const& pool, Vec2I const& position, std::uint64_t seed) {
  auto objects = world.root().object_database();
  if (!objects) return std::unexpected(std::move(objects.error()));
  auto direction = seed % 2 ? Direction::Left : Direction::Right;
  // An empty pool historically passed a default name and parameters to placement.
  if (pool.empty()) return (*objects)->create_for_placement(world, String{}, position, direction, Json{});
  auto const& [name, parameters] = pool.items()[pool.select_index(seed)].second;
  return (*objects)->create_for_placement(world, name, position, direction, parameters);
}


std::expected<std::shared_ptr<LiquidWorld>, StarException> LiquidWorld::create(WorldServer& world) {
  struct MakeShared final : LiquidWorld {
    explicit MakeShared(WorldServer& world) : LiquidWorld(world) {}
  };
  auto liquids = world.root().liquids_database();
  if (!liquids) return std::unexpected(std::move(liquids.error()));
  auto materials = world.root().material_database();
  if (!materials) return std::unexpected(std::move(materials.error()));
  auto result = std::make_shared<MakeShared>(world);
  result->m_liquids_database = std::move(*liquids);
  result->m_material_database = std::move(*materials);
  return result;
}

Vec2I LiquidWorld::unique_location(Vec2I const& location) const {
  return m_world_server.geometry().xwrap(location);
}

std::expected<float, StarException> LiquidWorld::drain_level(Vec2I const& location) const {
  auto world_template = m_world_server.worldTemplate();
  if (!(location[1] > world_template->undergroundLevel()))
    return 0.0f;
  auto tile = m_world_server.getServerTile(location);
  if (!tile) return std::unexpected(std::move(tile.error()));
  auto blocked = m_material_database->blocks_liquid_flow(tile->get().background);
  if (!blocked) return std::unexpected(std::move(blocked.error()));
  if (*blocked) return 0.0f;

  auto below = m_world_server.getServerTile(location + Vec2I(0, -1));
  if (!below) return std::unexpected(std::move(below.error()));
  auto const& below_tile = below->get();
  auto background_blocks = m_material_database->blocks_liquid_flow(below_tile.background);
  if (!background_blocks) return std::unexpected(std::move(background_blocks.error()));
  if (*background_blocks) return m_liquids_database->background_drain();
  auto foreground_blocks = m_material_database->blocks_liquid_flow(below_tile.foreground);
  if (!foreground_blocks) return std::unexpected(std::move(foreground_blocks.error()));
  return *foreground_blocks || below_tile.liquid.source ? m_liquids_database->background_drain() : 0.0f;
}

std::expected<CellularLiquidCell<LiquidId>, StarException> LiquidWorld::cell(Vec2I const& location) const {
  auto result = m_world_server.getServerTile(location);
  if (!result) return std::unexpected(std::move(result.error()));
  auto const& tile = result->get();
  auto blocked = m_material_database->blocks_liquid_flow(tile.foreground);
  if (!blocked) return std::unexpected(std::move(blocked.error()));
  if (*blocked) return CellularLiquidCollisionCell{};
  if (tile.liquid.source)
    return CellularLiquidSourceCell<LiquidId>{.liquid = tile.liquid.liquid, .pressure = tile.liquid.pressure};
  if (tile.liquid.liquid != empty_liquid_id)
    return CellularLiquidFlowCell<LiquidId>{.liquid = tile.liquid.liquid,
                                           .level = tile.liquid.level,
                                           .pressure = tile.liquid.pressure};
  return CellularLiquidFlowCell<LiquidId>{};
}

std::expected<void, StarException> LiquidWorld::set_flow(Vec2I const& location, CellularLiquidFlowCell<LiquidId> const& flow) {
  if (!flow.liquid)
    return m_world_server.setLiquid(location, empty_liquid_id, 0.0f, 0.0f);
  auto written = m_world_server.setLiquid(location, *flow.liquid, flow.level, flow.pressure);
  if (!written) return std::unexpected(std::move(written.error()));
  auto result = m_world_server.getServerTile(location);
  if (!result) return std::unexpected(std::move(result.error()));
  auto const& tile = result->get();
  if (auto interaction = m_material_database->liquid_material_interaction(*flow.liquid, tile.background);
      interaction && !interaction->top_only && tile.liquid.level >= interaction->consume_liquid) {
    auto modified = m_world_server.modifyServerTile(location);
    if (!modified) return std::unexpected(std::move(modified.error()));
    if (*modified) {
      auto& target = (*modified)->get();
      target.liquid.drain(interaction->consume_liquid);
      target.background = interaction->transform_to;
      auto activated = m_world_server.activateLiquidLocation(location);
      if (!activated) return std::unexpected(std::move(activated.error()));
    }
  }
  if (auto interaction = m_material_database->liquid_mod_interaction(*flow.liquid, tile.backgroundMod);
      interaction && !interaction->top_only && tile.liquid.level >= interaction->consume_liquid) {
    auto modified = m_world_server.modifyServerTile(location);
    if (!modified) return std::unexpected(std::move(modified.error()));
    if (*modified) {
      auto& target = (*modified)->get();
      target.liquid.drain(interaction->consume_liquid);
      target.backgroundMod = interaction->transform_to;
      return m_world_server.activateLiquidLocation(location);
    }
  }
  return {};
}

std::expected<void, StarException> LiquidWorld::liquid_interaction(Vec2I const& a, LiquidId a_liquid, Vec2I const& b, LiquidId b_liquid) {
  auto handle_interaction = [this](Vec2I const& target, LiquidId current, LiquidId other) -> std::expected<void, StarException> {
    auto interaction = m_liquids_database->interact(current, other);
    if (!interaction)
      return {};
    if (auto material = interaction->maybe_left_ref()) {
      auto placed = m_world_server.modifyTile(target, PlaceMaterial{.layer = TileLayer::Foreground,
                                                                   .material = material->get(),
                                                                   .materialHueShift = 0}, false);
      if (!placed)
        return std::unexpected(std::move(placed.error()));
      return {};
    }
    auto level = m_world_server.liquidLevel(target).level;
    return m_world_server.setLiquid(target, interaction->right_ref(), level, level);
  };
  if (auto result = handle_interaction(a, a_liquid, b_liquid); !result)
    return std::unexpected(std::move(result.error()));
  return handle_interaction(b, b_liquid, a_liquid);
}

std::expected<void, StarException> LiquidWorld::liquid_collision(Vec2I const& liquid_pos, LiquidId liquid_id, Vec2I const& block_pos) {
  auto result = m_world_server.getServerTile(block_pos);
  if (!result) return std::unexpected(std::move(result.error()));
  auto const& block = result->get();
  if (auto interaction = m_material_database->liquid_material_interaction(liquid_id, block.foreground);
      interaction && (!interaction->top_only || liquid_pos[1] > block_pos[1])
      && m_world_server.liquidLevel(liquid_pos).level >= interaction->consume_liquid) {
    auto liquid_tile = m_world_server.modifyServerTile(liquid_pos);
    if (!liquid_tile) return std::unexpected(std::move(liquid_tile.error()));
    auto block_tile = m_world_server.modifyServerTile(block_pos);
    if (!block_tile) return std::unexpected(std::move(block_tile.error()));
    if (*liquid_tile && *block_tile) {
      (*liquid_tile)->get().liquid.drain(interaction->consume_liquid);
      auto& target = (*block_tile)->get();
      target.foreground = interaction->transform_to;
      auto multicolor = m_material_database->is_multi_color(interaction->transform_to);
      if (!multicolor) return std::unexpected(std::move(multicolor.error()));
      if (!*multicolor) target.foregroundColorVariant = default_material_color_variant;
      auto activated = m_world_server.activateLiquidLocation(liquid_pos);
      if (!activated) return std::unexpected(std::move(activated.error()));
    }
  }
  if (auto interaction = m_material_database->liquid_mod_interaction(liquid_id, block.foregroundMod);
      interaction && (!interaction->top_only || liquid_pos[1] > block_pos[1])
      && m_world_server.liquidLevel(liquid_pos).level >= interaction->consume_liquid) {
    auto liquid_tile = m_world_server.modifyServerTile(liquid_pos);
    if (!liquid_tile) return std::unexpected(std::move(liquid_tile.error()));
    auto block_tile = m_world_server.modifyServerTile(block_pos);
    if (!block_tile) return std::unexpected(std::move(block_tile.error()));
    if (*liquid_tile && *block_tile) {
      (*liquid_tile)->get().liquid.drain(interaction->consume_liquid);
      (*block_tile)->get().foregroundMod = interaction->transform_to;
      return m_world_server.activateLiquidLocation(liquid_pos);
    }
  }
  return {};
}

std::expected<std::shared_ptr<FallingBlocksWorld>, StarException> FallingBlocksWorld::create(WorldServer& world) {
  struct MakeShared final : FallingBlocksWorld {
    explicit MakeShared(WorldServer& world) : FallingBlocksWorld(world) {}
  };
  auto materials = world.root().material_database();
  if (!materials) return std::unexpected(std::move(materials.error()));
  auto result = std::make_shared<MakeShared>(world);
  result->m_material_database = std::move(*materials);
  return result;
}

std::expected<FallingBehavior, StarException> FallingBlocksWorld::source_behavior(Vec2I const& pos) {
  auto geometry = m_world_server.geometry();
  auto location = geometry.xwrap(pos);
  auto y = location[1];
  if (y < 0 || std::cmp_greater(y, geometry.height()))
    return FallingBehavior::Stationary;
  if (std::cmp_less(y, geometry.height())) {
    auto tile = m_world_server.getServerTile(location, true);
    if (!tile) return std::unexpected(std::move(tile.error()));
    if (!tile->get().rootSource && tile->get().foreground != empty_material_id) {
      auto behavior = m_material_database->falling_behavior(tile->get().foreground);
      if (!behavior || *behavior != FallingBehavior::Stationary)
        return behavior;
    }
  }

  // The old below probe also requested its sector. Keep that request when
  // skipping a stationary source would otherwise leave a different sector asleep.
  if (y > 0 && (y % world_sector_size == 0 || std::cmp_equal(y, geometry.height()))) {
    auto signaled = m_world_server.signalRegion(RectI::with_size({location[0], y - 1}, {1, 1}));
    if (!signaled) return std::unexpected(std::move(signaled.error()));
  }
  return FallingBehavior::Stationary;
}

std::expected<bool, StarException> FallingBlocksWorld::target_open(Vec2I const& pos) {
  auto geometry = m_world_server.geometry();
  if (pos[1] < 0 || std::cmp_greater_equal(pos[1], geometry.height())) return false;
  auto location = geometry.xwrap(pos);
  auto tile = m_world_server.getServerTile(location, true);
  if (!tile) return std::unexpected(std::move(tile.error()));
  if (tile->get().rootSource || tile->get().foreground != empty_material_id) return false;
  auto occupied = m_world_server.tileIsOccupied(location, TileLayer::Foreground);
  if (!occupied) return std::unexpected(std::move(occupied.error()));
  return !*occupied;
}

std::expected<BlockMoveResult, BlockMoveError> FallingBlocksWorld::move_block(Vec2I const& from, Vec2I const& to) {
  auto geometry = m_world_server.geometry();
  auto source_pos = geometry.xwrap(from);
  auto target_pos = geometry.xwrap(to);
  auto writable_tile = [&](Vec2I const& pos) -> std::expected<std::optional<std::reference_wrapper<ServerTile>>, StarException> {
    if (pos[1] < 0 || std::cmp_greater_equal(pos[1], geometry.height())) return {};
    return m_world_server.modifyServerTile(pos, true);
  };
  auto source_tile = writable_tile(source_pos);
  if (!source_tile) return std::unexpected(BlockMoveError{std::move(source_tile.error())});
  auto target_tile = writable_tile(target_pos);
  if (!target_tile) return std::unexpected(BlockMoveError{std::move(target_tile.error())});
  if (!*source_tile || !*target_tile) return BlockMoveResult::Unchanged;
  auto& source = (*source_tile)->get();
  auto& target = (*target_tile)->get();
  if (source.foreground == empty_material_id || target.foreground != empty_material_id)
    return BlockMoveResult::Unchanged;

  if (m_world_server.isTileProtected(target_pos)) {
    auto material = source.foreground;
    auto mod = source.foregroundMod;
    auto drops = m_world_server.destroyBlock(TileLayer::Foreground, source_pos, true, true);
    // destroyBlock can fail either before mutation or while notifying neighbors.
    // A mod-only break is committed work even though the material still stands.
    auto result = source.foreground != material || source.foregroundMod != mod
        ? BlockMoveResult::Changed : BlockMoveResult::Unchanged;
    if (!drops) return std::unexpected(BlockMoveError{std::move(drops.error()), result});
    for (auto const& drop : *drops) {
      auto entity = ItemDrop::create_randomized_drop(m_world_server.root(), drop, Vec2F(target_pos));
      if (!entity) return std::unexpected(BlockMoveError{std::move(entity.error()), result});
      auto added = m_world_server.addEntity(*entity);
      if (!added) return std::unexpected(BlockMoveError{std::move(added.error()), result});
    }
    return result;
  }

  auto collision = m_material_database->material_collision_kind(source.foreground);
  if (!collision) return std::unexpected(BlockMoveError{std::move(collision.error())});
  target.foreground = source.foreground;
  target.foregroundMod = no_mod_id;
  target.foregroundHueShift = source.foregroundHueShift;
  target.foregroundColorVariant = source.foregroundColorVariant;
  target.update_collision(*collision);
  source.foreground = empty_material_id;
  source.foregroundMod = no_mod_id;
  source.update_collision(CollisionKind::None);
  m_world_server.request_global_break_check();
  return BlockMoveResult::Changed;
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
      auto operationResult1 = m_worldServer.modifyLiquid(position, empty_liquid_id, 0);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    tile->get().foreground = material;
    tile->get().foregroundHueShift = hueshift;
    tile->get().foregroundColorVariant = colorVariant;
    tile->get().foregroundMod = no_mod_id;
    tile->get().foregroundModHueShift = MaterialHue();
    auto material_database_result = generationRoot.material_database();
    if (!material_database_result) return std::unexpected(std::move(material_database_result.error()));
    auto tile_collision_result = (*material_database_result)->material_collision_kind(tile->get().foreground);
    if (!tile_collision_result) return std::unexpected(std::move(tile_collision_result.error()));
    tile->get().update_collision(*tile_collision_result);
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::setBackgroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) {
  auto tileCheckedResult1 = m_worldServer.modifyServerTile(position);
  if (!tileCheckedResult1) return std::unexpected(std::move(tileCheckedResult1.error()));
  if (auto tile = std::move((*tileCheckedResult1))) {
    {
      auto operationResult1 = m_worldServer.modifyLiquid(position, empty_liquid_id, 0);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    tile->get().background = material;
    tile->get().backgroundHueShift = hueshift;
    tile->get().backgroundColorVariant = colorVariant;
    tile->get().backgroundMod = no_mod_id;
    tile->get().backgroundModHueShift = MaterialHue();
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::placeObject(Vec2I const& pos, String const& objectName, Star::Direction direction, Json const& parameters) {
  auto& generationRoot = m_worldServer.root();
  {
    auto operationResult1 = m_worldServer.signalRegion(RectI::with_size(pos, {1, 1}));
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  auto autoobjectDatabaseResult1 = generationRoot.object_database();
  if (!autoobjectDatabaseResult1) return std::unexpected(std::move(autoobjectDatabaseResult1.error()));
  auto objectDatabase = std::move((*autoobjectDatabaseResult1));
  auto objectCheckedResult1 = objectDatabase->create_for_placement(m_worldServer, objectName, pos, direction, parameters);
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
    auto operationResult1 = m_worldServer.signalRegion(RectI::with_size(Vec2I(pos), {1, 1}));
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
  auto potential = m_worldServer.worldTemplate()->potential_biome_items_at(pos[0], pos[1]);
  if (!potential) return std::unexpected(std::move(potential.error()));
  return place_biome_items(pos, potential->surface_biome_items);
}

std::expected<void, StarException> DungeonGeneratorWorld::placeBiomeTree(Vec2I const& pos) {
  auto const& world_template = m_worldServer.worldTemplate();
  auto biome = world_template->blockBiome(pos[0], pos[1]);
  if (!biome) return std::unexpected(std::move(biome.error()));
  if (*biome) {
    auto signalled = m_worldServer.signalRegion(RectI::with_size(pos, {1, 1}));
    if (!signalled) return std::unexpected(std::move(signalled.error()));
    auto seed = world_template->seedFor(pos[0], pos[1]);
    if (auto tree = (*biome)->surface_placeables.first_tree()) {
      auto plants = m_worldServer.root().plantDatabase();
      if (!plants) return std::unexpected(std::move(plants.error()));
      auto plant = (*plants)->createPlant(tree->get(), seed);
      if (!plant) return std::unexpected(std::move(plant.error()));
      return placePlant(*plant, pos);
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
  bool adjustBackground = background == empty_material_id || background == null_material_id;

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
  if (is_connectable_material(primaryTile.foreground) || !is_connectable_material(rootTile.foreground))
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
    if (tile.rootSource || (!withinAdjustment(space) && !(tile.foreground == empty_material_id || tile.foreground == null_material_id)))
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
        auto condition = std::move(!is_connectable_material(conditionResult1->get().foreground));
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
      if (is_connectable_material(tile->get().foreground))
        tile->get() = primaryTile;
      if (adjustBackground)
        tile->get().background = empty_material_id;
      tile->get().collision = CollisionKind::None;
      auto material_database_result = generationRoot.material_database();
      if (!material_database_result) return std::unexpected(std::move(material_database_result.error()));
      auto tile_collision_result = (*material_database_result)->material_collision_kind(tile->get().foreground);
      if (!tile_collision_result) return std::unexpected(std::move(tile_collision_result.error()));
      tile->get().update_collision(*tile_collision_result);
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
      if (!is_real_material(tile->get().foreground)) {
        tile->get() = rootTile;
        auto material_database_result = generationRoot.material_database();
        if (!material_database_result) return std::unexpected(std::move(material_database_result.error()));
        auto tile_collision_result = (*material_database_result)->material_collision_kind(tile->get().foreground);
        if (!tile_collision_result) return std::unexpected(std::move(tile_collision_result.error()));
        tile->get().update_collision(*tile_collision_result);
      }
    } else {
      return {};
    }
  }
  {
    auto operationResult1 = plant->set_tile_position(position);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = m_worldServer.addEntity(plant);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::place_biome_items(Vec2I const& pos, List<BiomeItemPlacement>& potential_items) {
  auto& generation_root = m_worldServer.root();
  auto const& world_template = m_worldServer.worldTemplate();
  {
    auto operation_result1 = m_worldServer.signalRegion(RectI::with_size(pos, {1, 1}));
    if (!operation_result1) return std::unexpected(std::move(operation_result1.error()));
  }
  std::sort(potential_items.begin(), potential_items.end(), std::less<BiomeItemPlacement>());
  for (auto const& placement : potential_items) {
    auto seed = world_template->seedFor(placement.position[0], placement.position[1]);
    if (std::holds_alternative<GrassVariant>(placement.item)) {
      auto& grass = std::get<GrassVariant>(placement.item);
      {
        auto operation_result1 = generation_root.plantDatabase();
        if (!operation_result1) return std::unexpected(std::move(operation_result1.error()));
        auto operation_result2 = (*operation_result1)->createPlant(grass, seed);
        if (!operation_result2) return std::unexpected(std::move(operation_result2.error()));
        auto operation_result3 = placePlant((*operation_result2), placement.position);
        if (!operation_result3) return std::unexpected(std::move(operation_result3.error()));
      }
    } else if (std::holds_alternative<BushVariant>(placement.item)) {
      auto& bush = std::get<BushVariant>(placement.item);
      {
        auto operation_result1 = generation_root.plantDatabase();
        if (!operation_result1) return std::unexpected(std::move(operation_result1.error()));
        auto operation_result2 = (*operation_result1)->createPlant(bush, seed);
        if (!operation_result2) return std::unexpected(std::move(operation_result2.error()));
        auto operation_result3 = placePlant((*operation_result2), placement.position);
        if (!operation_result3) return std::unexpected(std::move(operation_result3.error()));
      }
    } else if (std::holds_alternative<TreePair>(placement.item)) {
      auto const& tree_pair = std::get<TreePair>(placement.item);
      auto const& tree_variant = seed % 2 == 0 ? tree_pair.first : tree_pair.second;
      {
        auto operation_result1 = generation_root.plantDatabase();
        if (!operation_result1) return std::unexpected(std::move(operation_result1.error()));
        auto operation_result2 = (*operation_result1)->createPlant(tree_variant, seed);
        if (!operation_result2) return std::unexpected(std::move(operation_result2.error()));
        auto operation_result3 = placePlant((*operation_result2), placement.position);
        if (!operation_result3) return std::unexpected(std::move(operation_result3.error()));
      }
    } else if (std::holds_alternative<ObjectPool>(placement.item)) {
      auto object = create_biome_object(m_worldServer, std::get<ObjectPool>(placement.item), placement.position, seed);
      if (!object) return std::unexpected(std::move(object.error()));
      if (*object) {
        auto added = m_worldServer.addEntity(*object);
        if (!added) return std::unexpected(std::move(added.error()));
      }
    } else if (std::holds_alternative<TreasureBoxSet>(placement.item)) {
      auto const& treasure_set = std::get<TreasureBoxSet>(placement.item).name;
      auto direction = seed % 2 ? Direction::Left : Direction::Right;
      auto treasures = generation_root.treasureDatabase();
      if (!treasures) return std::unexpected(std::move(treasures.error()));
      auto chest = (*treasures)->createTreasureChest(m_worldServer, treasure_set, placement.position, direction, seed);
      if (!chest) return std::unexpected(std::move(chest.error()));
      if (auto treasure_container = std::move((*chest)))
        {
          auto operation_result1 = m_worldServer.addEntity(treasure_container);
          if (!operation_result1) return std::unexpected(std::move(operation_result1.error()));
        }
    }
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::addDrop(Vec2F const& position, ItemDescriptor const& item) {
  auto& generation_root = m_worldServer.root();
  {
    auto drop_result = ItemDrop::create_randomized_drop(generation_root, item, position);
    if (!drop_result) return std::unexpected(std::move(drop_result.error()));
    auto operationResult2 = m_worldServer.addEntity((*drop_result));
    if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
  }

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::spawnNpc(Vec2F const& position, Json const& parameters) {
  auto& generationRoot = m_worldServer.root();
  auto autokindResult1 = parameters.get_string("kind");
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
    auto autonpcDatabaseResult1 = generationRoot.npc_database();
    if (!autonpcDatabaseResult1) return std::unexpected(std::move(autonpcDatabaseResult1.error()));
    auto npcDatabase = std::move((*autonpcDatabaseResult1));
    auto stduint64_tseedResult1 = parameters.get_uint("seed", generationRoot.random().next_u64());
    if (!stduint64_tseedResult1) return std::unexpected(std::move(stduint64_tseedResult1.error()));
    std::uint64_t seed = std::move((*stduint64_tseedResult1));
    auto StringspeciesResult1 = parameters.get_string("species");
    if (!StringspeciesResult1) return std::unexpected(std::move(StringspeciesResult1.error()));
    String species = std::move((*StringspeciesResult1));
    auto StringtypeNameResult1 = parameters.get_string("typeName", "default");
    if (!StringtypeNameResult1) return std::unexpected(std::move(StringtypeNameResult1.error()));
    String typeName = std::move((*StringtypeNameResult1));
    auto JsonObjectuniqueParametersResult1 = parameters.get_object("parameters", {});
    if (!JsonObjectuniqueParametersResult1) return std::unexpected(std::move(JsonObjectuniqueParametersResult1.error()));
    JsonObject uniqueParameters = std::move((*JsonObjectuniqueParametersResult1));
    if (!uniqueParameters.contains("persistent"))
      uniqueParameters["persistent"] = true;
    auto autonpcResult1 = m_worldServer.threatLevel();
    if (!autonpcResult1) return std::unexpected(std::move(autonpcResult1.error()));
    auto autonpcResult2 = npcDatabase->generate_npc_variant(species, typeName, (*autonpcResult1), seed, uniqueParameters);
    if (!autonpcResult2) return std::unexpected(std::move(autonpcResult2.error()));
    auto autonpcResult3 = npcDatabase->create_npc(std::move(*autonpcResult2));
    if (!autonpcResult3) return std::unexpected(std::move(autonpcResult3.error()));
    auto npc = std::move((*autonpcResult3));
    {
      auto operationResult1 = npc->feet_offset();
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      npc->set_position(position - (*operationResult1));
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
    auto database = generationRoot.monster_database();
    if (!database) return std::unexpected(std::move(database.error()));
    auto seed = parameters.get_uint("seed", generationRoot.random().next_u64());
    if (!seed) return std::unexpected(std::move(seed.error()));
    auto type_name = parameters.get_string("typeName");
    if (!type_name) return std::unexpected(std::move(type_name.error()));
    auto unique_parameters = parameters.get_object("parameters", {});
    if (!unique_parameters) return std::unexpected(std::move(unique_parameters.error()));
    if (!unique_parameters->contains("persistent"))
      (*unique_parameters)["persistent"] = true;
    auto variant = (*database)->monster_variant(*type_name, *seed, std::move(*unique_parameters));
    if (!variant) return std::unexpected(std::move(variant.error()));
    auto created = (*database)->create_monster(std::move(*variant));
    if (!created) return std::unexpected(std::move(created.error()));
    auto monster = std::move(*created);
    monster->setPosition(position);
    {
      auto operationResult1 = m_worldServer.addEntity(monster);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  } else
    return std::unexpected(StarException(Star::format("Unknown spawnable kind '{}'", kind)));

  return {};
}

std::expected<void, StarException> DungeonGeneratorWorld::spawnStagehand(Vec2F const& position, Json const& definition) {
  auto& generationRoot = m_worldServer.root();
  auto autostagehandResult1 = generationRoot.stagehandDatabase();
  if (!autostagehandResult1) return std::unexpected(std::move(autostagehandResult1.error()));
  auto autostagehandResult2 = definition.get_string("type");
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
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((tile));
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
    Vec2F posf = center_of_tile(entry);
    RectF bounds = {posf - Vec2F(16, 16), posf + Vec2F(16, 16)};
    auto rangeValuesResult1 = m_worldServer.query<WireEntity>(bounds);
    if (!rangeValuesResult1) return std::unexpected(std::move(rangeValuesResult1.error()));
    auto rangeValues = std::move((*rangeValuesResult1));
    for (auto entity : rangeValues) {
      for (std::size_t i = 0; i < entity->node_count(WireDirection::Input); ++i) {
        if (auto inputNodeMatches = [&]() -> std::expected<bool, StarException> {
              auto nodeOffset = entity->node_position({.direction = WireDirection::Input, .node_index = i});
              if (!nodeOffset)
                return std::unexpected(std::move(nodeOffset.error()));
              auto condition = std::move(entity->tile_position() + (*nodeOffset) == entry);
              return static_cast<bool>(condition);
            }();
            !inputNodeMatches)
          return std::unexpected(std::move(inputNodeMatches.error()));
        else if (*inputNodeMatches) {
          inbounds.push_back(WireConnection{.entity_location = entity->tile_position(), .node_index = i});
          found = true;
        }
      }
      for (std::size_t i = 0; i < entity->node_count(WireDirection::Output); ++i) {
        if (auto outputNodeMatches = [&]() -> std::expected<bool, StarException> {
              auto nodeOffset = entity->node_position({.direction = WireDirection::Output, .node_index = i});
              if (!nodeOffset)
                return std::unexpected(std::move(nodeOffset.error()));
              auto condition = std::move(entity->tile_position() + (*nodeOffset) == entry);
              return static_cast<bool>(condition);
            }();
            !outputNodeMatches)
          return std::unexpected(std::move(outputNodeMatches.error()));
        else if (*outputNodeMatches) {
          outbounds.push_back(WireConnection{.entity_location = entity->tile_position(), .node_index = i});
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
    auto autooutResult1 = m_worldServer.atTile<WireEntity>(outbound.entity_location);
    if (!autooutResult1) return std::unexpected(std::move(autooutResult1.error()));
    auto autooutResult2 = (*autooutResult1).first();
    if (!autooutResult2) return std::unexpected(std::move(autooutResult2.error()));
    auto out = std::move(autooutResult2->get());
    for (auto inbound : inbounds) {
      auto autoinResult1 = m_worldServer.atTile<WireEntity>(inbound.entity_location);
      if (!autoinResult1) return std::unexpected(std::move(autoinResult1.error()));
      auto autoinResult2 = (*autoinResult1).first();
      if (!autoinResult2) return std::unexpected(std::move(autoinResult2.error()));
      auto in = std::move(autoinResult2->get());
      {
        auto inputConnectionAdded =
          in->add_node_connection({.direction = WireDirection::Input, .node_index = inbound.node_index}, outbound);
        if (!inputConnectionAdded)
          return std::unexpected(std::move(inputConnectionAdded.error()));
      }
      {
        auto outputConnectionAdded =
          out->add_node_connection({.direction = WireDirection::Output, .node_index = outbound.node_index}, inbound);
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
  return tile.material(layer) != empty_material_id && tile.material(layer) != null_material_id;
}

std::expected<bool, StarException> DungeonGeneratorWorld::checkOpen(Vec2I const& position, TileLayer layer) {
  auto autoconsttileResult1 = m_worldServer.getServerTile(position);
  if (!autoconsttileResult1) return std::unexpected(std::move(autoconsttileResult1.error()));
  auto const& tile = std::move(autoconsttileResult1->get());
  return tile.material(layer) == empty_material_id || tile.material(layer) == null_material_id;
}

std::expected<bool, StarException> DungeonGeneratorWorld::checkOceanLiquid(Vec2I const& position) {
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto autoconstblockResult1 = worldTemplate->blockInfo(position[0], position[1]);
  if (!autoconstblockResult1) return std::unexpected(std::move(autoconstblockResult1.error()));
  auto const& block = std::move((*autoconstblockResult1));
  return block.oceanLiquid != empty_liquid_id && position[1] < block.oceanLiquidLevel;
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
      if (positions.contains(geometry.xwrap(pos + tileEntity->tile_position())))
        return true;
    }
    if (clearAnchoredObjects) {
      for (auto pos : tileEntity->roots()) {
        if (positions.contains(geometry.xwrap(pos + tileEntity->tile_position())))
          return true;
      }
      if (auto object = as<Object>(entity)) {
        for (auto pos : object->anchor_positions()) {
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
    windows.push_back(m_worldServer.clientWindow(clientId));
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
  auto collision = m_worldServer.poly_collision(PolyF(area));
  if (!collision)
    return std::unexpected(std::move(collision.error()));
  return !*collision;
}

std::expected<bool, StarException> SpawnerWorld::isBackgroundEmpty(Vec2I const& pos) const {
  {
  auto conditionResult1 = m_worldServer.getServerTile(pos);
  if (!conditionResult1) return std::unexpected(std::move(conditionResult1.error()));
  auto condition = std::move(conditionResult1->get().background == empty_material_id);
  return static_cast<bool>(condition);

  }
}

LiquidLevel SpawnerWorld::liquidLevel(Vec2I const& position) const {
  return m_worldServer.liquidLevel(position);
}

std::expected<bool, StarException> SpawnerWorld::spawningProhibited(RectF const& area) const {
  RectI region = RectI::integral(area);

  // Don't spawn the entity if its region overlaps with a dungeon
  for (int x = region.x_min(); x < region.x_max(); ++x) {
    for (int y = region.y_min(); y < region.y_max(); ++y) {
      auto autoconsttileResult1 = m_worldServer.getServerTile({x, y});
      if (!autoconsttileResult1) return std::unexpected(std::move(autoconsttileResult1.error()));
      auto const& tile = std::move(autoconsttileResult1->get());
      if (tile.getCollision() == CollisionKind::Null || tile.dungeonId != no_dungeon_id)
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
    // Dungeons, including construction_dungeon_id (player constructed areas)
    // should be immune from spawning.
    auto autotileResult1 = m_worldServer.getServerTile(ipos);
    if (!autotileResult1) return std::unexpected(std::move(autotileResult1.error()));
    auto tile = std::move(autotileResult1->get());
    if (tile.dungeonId == no_dungeon_id)
      return biome->spawn_profile;
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

WorldGenerator::WorldGenerator(WorldServer& server)
  : m_worldServer(server), m_micro_dungeon_factory(server.root()) {}

std::expected<void, StarException> WorldGenerator::generateSectorLevel(WorldStorage& worldStorage, Sector const& sector, SectorGenerationLevel generationLevel) {
  if (generationLevel == SectorGenerationLevel::BaseTiles) {
    {
      auto operationResult1 = prepareTiles(worldStorage, sector);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  } else if (generationLevel == SectorGenerationLevel::MicroDungeons) {
    if (!worldStorage.floatingDungeonWorld())
      return generate_micro_dungeons(worldStorage, sector);
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
      auto operationResult1 = m_worldServer.activateLiquidRegion(worldStorage.tileArray()->sector_region(sector));
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::sectorLoadLevelChanged(WorldStorage& worldStorage, Sector const& sector, SectorLoadLevel loadLevel) {
  if (loadLevel == SectorLoadLevel::Loaded) {
    if (worldStorage.sectorGenerationLevel(sector) == SectorGenerationLevel::Complete)
      {
        auto operationResult1 = m_worldServer.activateLiquidRegion(worldStorage.tileArray()->sector_region(sector));
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::terraformSector(WorldStorage& world_storage, Sector const& sector) {
  return reapply_biome(world_storage, sector);
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

std::expected<RpcPromise<Vec2I>, StarException> WorldGenerator::enqueue_placement(List<BiomeItemDistribution> distributions, std::optional<DungeonId> id) {
  if (std::ranges::none_of(distributions, &BiomeItemDistribution::can_place_micro_dungeon))
    return std::unexpected(StarException("Queued placement cannot produce a microdungeon"));

  auto promise = RpcPromise<Vec2I>::make_pair();
  m_queued_placements.push_back(std::make_shared<QueuedPlacement>(
      std::move(distributions), id, std::move(promise.second)));
  return std::move(promise.first);
}

std::expected<void, StarException> WorldGenerator::replace_biome_blocks(ServerTile& tile) {
  auto& root = m_worldServer.root();
  auto const world_template = m_worldServer.worldTemplate();
  auto material_database = root.material_database();
  if (!material_database) return std::unexpected(std::move(material_database.error()));

  auto biome_result = world_template->biome(tile.blockBiomeIndex);
  if (!biome_result) return std::unexpected(std::move(biome_result.error()));
  auto const& block_biome = *biome_result;
  struct Layer {
    MaterialId material;
    MaterialHue hue;
  };
  auto resolve_layer = [&](MaterialId material, MaterialHue hue) -> Layer {
    if (!is_biome_material(material)) return {material, hue};
    if (!block_biome) return {empty_material_id, 0};
    auto const& sub_blocks = block_biome->sub_blocks;
    auto resolved = material == biome_material_id || sub_blocks.empty() ? block_biome->main_block
        : sub_blocks[static_cast<std::size_t>(material - biome1_material_id) % sub_blocks.size()];
    return {resolved, block_biome->hue_for_material(resolved)};
  };

  auto foreground = resolve_layer(tile.foreground, tile.foregroundHueShift);
  auto background = resolve_layer(tile.background, tile.backgroundHueShift);
  auto collision = biome_foreground_collision(**material_database, tile.foreground, foreground.material, tile.collision);
  if (!collision) return std::unexpected(std::move(collision.error()));
  // A failed lookup must leave the original placeholders intact for replay.
  tile.foreground = foreground.material;
  tile.foregroundHueShift = foreground.hue;
  tile.background = background.material;
  tile.backgroundHueShift = background.hue;
  if (!block_biome) {
    if (is_biome_mod(tile.foregroundMod)) {
      tile.foregroundMod = no_mod_id;
      tile.foregroundModHueShift = 0;
    }
    if (is_biome_mod(tile.backgroundMod)) {
      tile.backgroundMod = no_mod_id;
      tile.backgroundModHueShift = 0;
    }
  }
  tile.update_collision(*collision);

  return {};
}

std::expected<void, StarException> WorldGenerator::prepareTiles(WorldStorage& worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto& generationRoot = m_worldServer.root();
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto material_database_result = generationRoot.material_database();
  if (!material_database_result) return std::unexpected(std::move(material_database_result.error()));
  auto material_database = std::move((*material_database_result));
  auto planet = worldTemplate;
  // Generate sector.
  auto tileArray = worldStorage.tileArray();
  RectI sectorRegion = tileArray->sector_region(sector);
  for (int x = sectorRegion.x_min(); x < sectorRegion.x_max(); ++x) {
    for (int y = sectorRegion.y_min(); y < sectorRegion.y_max(); ++y) {
      Vec2I pos(x, y);
      auto tile = tileArray->modify_tile(pos);
      if constexpr (Star::debug_enabled) {
        Star::star_assert_failed((tile));
      }
      if (!tile)
        continue;
      auto autoblockInfoResult1 = planet->blockInfo(pos[0], pos[1]);
      if (!autoblockInfoResult1) return std::unexpected(std::move(autoblockInfoResult1.error()));
      auto blockInfo = std::move((*autoblockInfoResult1));

      tile->get().blockBiomeIndex = blockInfo.blockBiomeIndex;
      tile->get().environmentBiomeIndex = blockInfo.environmentBiomeIndex;
      tile->get().biomeTransition = blockInfo.biomeTransition;

      if (tile->get().foreground == null_material_id) {
        tile->get().foreground = blockInfo.foreground;
        tile->get().foregroundColorVariant = default_material_color_variant;
        auto tileforegroundHueShiftResult1 = planet->biome_material_hue_shift(tile->get().blockBiomeIndex, tile->get().foreground);
        if (!tileforegroundHueShiftResult1) return std::unexpected(std::move(tileforegroundHueShiftResult1.error()));
        tile->get().foregroundHueShift = std::move((*tileforegroundHueShiftResult1));

        auto supports_mod = material_database->supports_mod(tile->get().foreground, blockInfo.foregroundMod);
        if (!supports_mod) return std::unexpected(std::move(supports_mod.error()));
        if (*supports_mod) {
          tile->get().foregroundMod = blockInfo.foregroundMod;
          auto tileforegroundModHueShiftResult1 = planet->biomeModHueShift(tile->get().blockBiomeIndex, tile->get().foregroundMod);
          if (!tileforegroundModHueShiftResult1) return std::unexpected(std::move(tileforegroundModHueShiftResult1.error()));
          tile->get().foregroundModHueShift = std::move((*tileforegroundModHueShiftResult1));
        }
      }

      if (tile->get().background == null_material_id) {
        tile->get().background = blockInfo.background;
        tile->get().backgroundColorVariant = default_material_color_variant;
        auto tilebackgroundHueShiftResult1 = planet->biome_material_hue_shift(tile->get().blockBiomeIndex, tile->get().background);
        if (!tilebackgroundHueShiftResult1) return std::unexpected(std::move(tilebackgroundHueShiftResult1.error()));
        tile->get().backgroundHueShift = std::move((*tilebackgroundHueShiftResult1));

        auto supports_mod = material_database->supports_mod(tile->get().background, blockInfo.backgroundMod);
        if (!supports_mod) return std::unexpected(std::move(supports_mod.error()));
        if (*supports_mod) {
          tile->get().backgroundMod = blockInfo.backgroundMod;
          auto tilebackgroundModHueShiftResult1 = planet->biomeModHueShift(tile->get().blockBiomeIndex, tile->get().backgroundMod);
          if (!tilebackgroundModHueShiftResult1) return std::unexpected(std::move(tilebackgroundModHueShiftResult1.error()));
          tile->get().backgroundModHueShift = std::move((*tilebackgroundModHueShiftResult1));
        }
      }

      if (tile->get().foreground != empty_material_id) {
        tile->get().liquid = LiquidStore();
      } else if (blockInfo.oceanLiquid != empty_liquid_id && pos[1] < blockInfo.oceanLiquidLevel) {
        float pressure = blockInfo.oceanLiquidLevel - pos[1];
        if (tile->get().background == empty_material_id)
          tile->get().liquid = LiquidStore::endless(blockInfo.oceanLiquid, pressure);
        else if (blockInfo.encloseLiquids)
          tile->get().liquid = LiquidStore::filled(blockInfo.oceanLiquid, 1.0f, pressure);
      }
    }
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::generate_micro_dungeons(WorldStorage& world_storage, Sector const& sector) {
  for (auto cursor = m_active_micro_dungeon_sector; cursor; cursor = cursor->get().previous) {
    if (cursor->get().sector == sector)
      return std::unexpected(StarException("Microdungeon generation for this sector is already in progress"));
  }
  ActiveMicroDungeonSector const active{sector, m_active_micro_dungeon_sector};
  m_active_micro_dungeon_sector = std::cref(active);
  auto leave_sector = make_scope_exit([this, &active] { m_active_micro_dungeon_sector = active.previous; });

  auto world_template = m_worldServer.worldTemplate();
  RectI sector_tiles = world_storage.tileArray()->sector_region(active.sector);
  RectI bounds = sector_tiles.padded(world_sector_size - 1);
  if (auto retry = m_micro_dungeon_retries.find(active.sector); retry != m_micro_dungeon_retries.end()) {
    // Nested generation of other sectors cannot invalidate a map node.
    auto ran = run_micro_dungeon_trials(*world_template, bounds, retry->second);
    if (!ran) return std::unexpected(std::move(ran.error()));
    m_micro_dungeon_retries.erase(retry);
  } else {
    MicroDungeonProgress progress;
    auto collected = collect_micro_dungeon_trials(*world_template, sector_tiles, progress);
    if (!collected) return std::unexpected(std::move(collected.error()));
    auto ran = run_micro_dungeon_trials(*world_template, bounds, progress);
    if (!ran) {
      // Flush writes to the world before biome replacement. Preserve the exact
      // sorted inputs and cursors once any of those writes have succeeded.
      if (progress.painted)
        m_micro_dungeon_retries.try_emplace(active.sector, std::move(progress));
      return std::unexpected(std::move(ran.error()));
    }
  }

  auto fulfilled = std::ranges::remove_if(m_queued_placements, [](std::shared_ptr<QueuedPlacement> const& placement) {
    return placement->state == QueuedPlacementState::Fulfilled;
  });
  m_queued_placements.erase(fulfilled.begin(), fulfilled.end());
  return {};
}

std::expected<void, StarException> WorldGenerator::collect_micro_dungeon_trials(WorldTemplate const& world_template,
    RectI const& sector_tiles, MicroDungeonProgress& progress) const {
  progress.queue = m_queued_placements;

  WorldTemplate::PotentialMicroDungeonItems scratch;
  for (int x = sector_tiles.x_min(); x < sector_tiles.x_max(); ++x) {
    for (int y = sector_tiles.y_min(); y < sector_tiles.y_max(); ++y) {
      auto potential = world_template.potential_micro_dungeon_items_at(x, y, scratch);
      if (!potential) return std::unexpected(std::move(potential.error()));
      auto validity = world_template.biome_item_validity_at(x, y);
      if (!validity) return std::unexpected(std::move(validity.error()));
      auto append_valid = [&](std::size_t queue_order) {
        scratch.append_valid(*validity, [&](WorldTemplate::MicroDungeonItem&& item) {
          progress.trials.push_back({std::move(item), queue_order});
        });
      };
      append_valid(0);

      for (std::size_t index = 0; index < progress.queue.size(); ++index) {
        auto sampled = world_template.add_potential_micro_dungeon_items(x, y, scratch, progress.queue[index]->distributions);
        if (!sampled) return std::unexpected(std::move(sampled.error()));
        append_valid(index + 1);
      }
    }
  }

  // Non-dungeon candidates must participate in this non-stable sort: their
  // equal keys can affect the order of otherwise indistinguishable anchors.
  std::sort(progress.trials.begin(), progress.trials.end(), [](MicroDungeonTrial const& a, MicroDungeonTrial const& b) {
    if (a.item.priority < b.item.priority) return true;
    if (b.item.priority < a.item.priority) return false;
    return a.queue_order < b.queue_order;
  });
  return {};
}

std::expected<void, StarException> WorldGenerator::run_micro_dungeon_trials(WorldTemplate const& world_template,
    RectI const& bounds, MicroDungeonProgress& progress) {
  std::optional<DungeonGeneratorWorld> facade;
  for (; progress.next_trial < progress.trials.size(); ++progress.next_trial) {
    auto const& [item, queue_order] = progress.trials[progress.next_trial];
    if (!item.dungeon_names)
      continue;

    std::optional<std::reference_wrapper<QueuedPlacement>> queued;
    if (queue_order != 0)
      queued = std::ref(*progress.queue[queue_order - 1]);

    if (!progress.modified_tiles) {
      if (queued && queued->get().state != QueuedPlacementState::Pending)
        continue;
      auto seed = world_template.seedFor(item.position[0], item.position[1]);
      auto selected = static_random_from(*item.dungeon_names, seed);
      if (!selected) return std::unexpected(std::move(selected.error()));
      auto threat = m_worldServer.threatLevel();
      if (!threat) return std::unexpected(std::move(threat.error()));
      if (!facade)
        facade.emplace(m_worldServer, false);

      // Reserve the request through draw callbacks and any suspended
      // postprocessing; another sector must not draw it at a different site.
      if (queued)
        queued->get().state = QueuedPlacementState::Placing;
      auto drawn = m_micro_dungeon_factory.generate(bounds, selected->get(), item.position, seed, *threat, *facade);
      if (!drawn || !*drawn) {
        if (queued)
          queued->get().state = QueuedPlacementState::Pending;
        if (!drawn) return std::unexpected(std::move(drawn.error()));
        continue;
      }
      progress.modified_tiles.emplace(std::move((*drawn)->modified_tiles));
      progress.painted = true;
    }

    auto const& modified_tiles = *progress.modified_tiles;
    auto tile_position = modified_tiles.begin();
    std::ranges::advance(tile_position, progress.next_tile);
    for (; tile_position != modified_tiles.end(); ++tile_position, ++progress.next_tile) {
      auto tile = m_worldServer.modifyServerTile(*tile_position);
      if (!tile) return std::unexpected(std::move(tile.error()));
      if (*tile) {
        auto& tile_value = (*tile)->get();
        auto replaced = replace_biome_blocks(tile_value);
        if (!replaced) return std::unexpected(std::move(replaced.error()));
        if (queued && queued->get().dungeon_id)
          tile_value.dungeonId = *queued->get().dungeon_id;
      }
    }

    if (queued) {
      auto fulfilled = queued->get().promise.fulfill(item.position);
      if (!fulfilled) return std::unexpected(std::move(fulfilled.error()));
      queued->get().state = QueuedPlacementState::Fulfilled;
    }
    progress.modified_tiles.reset();
    progress.next_tile = 0;
  }
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

  auto wrapCoords = [=](Vec2I const& coord) -> Vec2I { return Vec2I{positive_mod<int>(coord[0], dimensions[0]), coord[1]}; };

  RectI sectorTiles = tileArray->sector_region(sector);
  RectI bounds = sectorTiles.padded(Vec2I(world_sector_size - 1, world_sector_size - 1));

  bounds.min()[1] = clamp<int>(bounds.min()[1], 0, dimensions[1] - 1);
  bounds.max()[1] = clamp<int>(bounds.max()[1], 0, dimensions[1] - 1);
  auto material_database_result = generationRoot.material_database();
  if (!material_database_result) return std::unexpected(std::move(material_database_result.error()));
  auto material_database = std::move((*material_database_result));

  auto samplePoint = sectorTiles.center();
  auto autoblockInfoResult1 = worldTemplate->blockInfo(samplePoint[0], samplePoint[1]);
  if (!autoblockInfoResult1) return std::unexpected(std::move(autoblockInfoResult1.error()));
  auto blockInfo = std::move((*autoblockInfoResult1));
  LiquidId fillLiquid = blockInfo.caveLiquid;
  bool fillMicrodungeons = blockInfo.fillMicrodungeons;
  bool encloseLiquids = blockInfo.encloseLiquids;

  Set<Vec2I> badNodes;

  for (int i = bounds.x_min(); i <= bounds.x_max(); i++) {
    badNodes.add({i, bounds.y_min()});
    badNodes.add({i, bounds.y_max()});
  }
  for (int i = bounds.y_min(); i <= bounds.y_max(); i++) {
    badNodes.add({bounds.x_min(), i});
    badNodes.add({bounds.x_max(), i});
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
    if constexpr (Star::debug_enabled) {
      Star::star_assert_failed((tile.foreground != null_material_id));
    }
    if (tile.foreground != empty_material_id) {
      // Not sure why this doesn't poison solid materials, but it does (occasionally) encounter that case
      auto collision = material_database->material_collision_kind(tile.foreground);
      if (!collision) return std::unexpected(std::move(collision.error()));
      if (!BlockCollisionSet.contains(*collision))
        badNodes.add(position);
      return {};
    }
    if ((tile.dungeonId != no_dungeon_id && (!fillMicrodungeons || tile.dungeonId != biome_micro_dungeon_id))
        || (!encloseLiquids && tile.background == empty_material_id)
        || (tile.liquid.liquid != fillLiquid && tile.liquid.liquid != empty_liquid_id)) {
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
    if constexpr (Star::debug_enabled) {
      Star::star_assert_failed((tile.foreground != null_material_id));
    }
    if (tile.foreground != empty_material_id)
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
    if constexpr (Star::debug_enabled) {
      Star::star_assert_failed((tile.foreground != null_material_id));
    }
    if (tile.foreground != empty_material_id)
      solidSurroundings.add(position);
  };

  for (auto position : candidateNodes) {
    solids(position + Vec2I(1, 0));
    solids(position + Vec2I(-1, 0));
    solids(position + Vec2I(0, 1));
    solids(position + Vec2I(0, -1));
  }
  auto sample_biome = worldTemplate->biome(tileArray->tile(samplePoint).blockBiomeIndex);
  if (!sample_biome) return std::unexpected(std::move(sample_biome.error()));
  MaterialId biome_block = (*sample_biome)->main_block;
  Map<Vec2I, float> drops = determineLiquidLevel(candidateNodes, solidSurroundings);
  for (auto iter = drops.begin(); iter != drops.end(); ++iter) {
    auto tile = tileArray->modify_tile(wrapCoords(iter->first));
    if constexpr (Star::debug_enabled) {
      Star::star_assert_failed((tile));
    }
    if (!tile)
      continue;
    if (iter->second)
      tile->get().liquid = LiquidStore::filled(fillLiquid, 1.0f, iter->second);
    if (encloseLiquids && tile->get().background == empty_material_id)
      tile->get().background = biome_block;
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::prepareSector(WorldStorage& worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto& generation_root = m_worldServer.root();
  auto const& world_template = m_worldServer.worldTemplate();
  auto material_database_result = generation_root.material_database();
  if (!material_database_result) return std::unexpected(std::move(material_database_result.error()));
  auto planet = world_template;
  auto tileArray = worldStorage.tileArray();
  RectI sectorTiles = tileArray->sector_region(sector);
  WorldImpl::invalidate_collision_cache(*tileArray, sectorTiles);

  for (int x = sectorTiles.x_min(); x < sectorTiles.x_max(); ++x) {
    for (int y = sectorTiles.y_min(); y < sectorTiles.y_max(); ++y) {
      Vec2I position(x, y);
      auto tile = tileArray->modify_tile(position);
      if constexpr (Star::debug_enabled) {
        Star::star_assert_failed((tile));
      }
      if (!tile)
        continue;
      if constexpr (Star::debug_enabled) {
        Star::star_assert_failed((tile->get().foreground != null_material_id));
      }

      if (tile->get().liquid.source) {
        auto autoblockInfoResult1 = planet->blockInfo(position[0], position[1]);
        if (!autoblockInfoResult1) return std::unexpected(std::move(autoblockInfoResult1.error()));
        auto blockInfo = std::move((*autoblockInfoResult1));
        // make sure that ocean liquid never exists on tiles without empty
        // background (except in real dungeons)
        if (!is_real_dungeon(tile->get().dungeonId) && tile->get().background != empty_material_id)
          tile->get().liquid.source = false;
        // pressurize liquid under the ocean
        if (blockInfo.oceanLiquid != empty_liquid_id && position[1] < blockInfo.oceanLiquidLevel) {
          float pressure = blockInfo.oceanLiquidLevel - position[1];
          tile->get().liquid.pressure = pressure;
        }
      }

      if (!is_real_material(tile->get().foreground))
        tile->get().foregroundColorVariant = default_material_color_variant;
      if (!is_real_material(tile->get().background))
        tile->get().backgroundColorVariant = default_material_color_variant;
      {
        auto operationResult1 = replace_biome_blocks(tile->get());
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
      {
        auto operationResult1 = placeBiomeGrass(worldStorage, tile->get(), position);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
      {
        auto collision = biome_foreground_collision(**material_database_result, tile->get().foreground, tile->get().foreground, tile->get().collision);
        if (!collision) return std::unexpected(std::move(collision.error()));
        tile->get().update_collision(*collision);
      }
    }
  }

  List<BiomeItemPlacement> placement_queue;
  for (int x = sectorTiles.x_min(); x < sectorTiles.x_max(); ++x) {
    for (int y = sectorTiles.y_min(); y < sectorTiles.y_max(); ++y) {
      auto tile = tileArray->tile(Vec2I(x, y));
      if (tile.dungeonId == no_dungeon_id) {
        auto potential = world_template->potential_biome_items_at(x, y);
        if (!potential) return std::unexpected(std::move(potential.error()));
        auto placements = world_template->valid_biome_items(x, y, std::move(*potential));
        if (!placements) return std::unexpected(std::move(placements.error()));
        for (auto& placement : *placements)
          placement_queue.push_back(std::move(placement));
      }
    }
  }

  std::sort(placement_queue.begin(), placement_queue.end(), std::less<BiomeItemPlacement>());
  for (auto const& placement : placement_queue) {
    auto seed = world_template->seedFor(placement.position[0], placement.position[1]);
    if (std::holds_alternative<GrassVariant>(placement.item)) {
      auto& grass = std::get<GrassVariant>(placement.item);
      {
        auto operationResult1 = generation_root.plantDatabase();
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        auto operationResult2 = (*operationResult1)->createPlant(grass, seed);
        if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        auto operationResult3 = placePlant(worldStorage, (*operationResult2), placement.position);
        if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
      }
    } else if (std::holds_alternative<BushVariant>(placement.item)) {
      auto& bush = std::get<BushVariant>(placement.item);
      {
        auto operationResult1 = generation_root.plantDatabase();
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        auto operationResult2 = (*operationResult1)->createPlant(bush, seed);
        if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        auto operationResult3 = placePlant(worldStorage, (*operationResult2), placement.position);
        if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
      }
    } else if (std::holds_alternative<TreePair>(placement.item)) {
      auto const& tree_pair = std::get<TreePair>(placement.item);
      auto const& tree_variant = seed % 2 == 0 ? tree_pair.first : tree_pair.second;
      {
        auto operation_result1 = generation_root.plantDatabase();
        if (!operation_result1) return std::unexpected(std::move(operation_result1.error()));
        auto operation_result2 = (*operation_result1)->createPlant(tree_variant, seed);
        if (!operation_result2) return std::unexpected(std::move(operation_result2.error()));
        auto operation_result3 = placePlant(worldStorage, (*operation_result2), placement.position);
        if (!operation_result3) return std::unexpected(std::move(operation_result3.error()));
      }
    } else if (std::holds_alternative<ObjectPool>(placement.item)) {
      auto object = create_biome_object(m_worldServer, std::get<ObjectPool>(placement.item), placement.position, seed);
      if (!object) return std::unexpected(std::move(object.error()));
      if (*object) {
        auto added = m_worldServer.addEntity(*object);
        if (!added) return std::unexpected(std::move(added.error()));
      }
    } else if (std::holds_alternative<TreasureBoxSet>(placement.item)) {
      auto const& treasure_set = std::get<TreasureBoxSet>(placement.item).name;
      auto direction = seed % 2 ? Direction::Left : Direction::Right;
      auto treasures = generation_root.treasureDatabase();
      if (!treasures) return std::unexpected(std::move(treasures.error()));
      auto chest = (*treasures)->createTreasureChest(
              m_worldServer, treasure_set, placement.position, direction, seed);
      if (!chest) return std::unexpected(std::move(chest.error()));
      if (auto treasure_container = std::move((*chest)))
        {
          auto operation_result1 = m_worldServer.addEntity(treasure_container);
          if (!operation_result1) return std::unexpected(std::move(operation_result1.error()));
        }
    }
  }


  return {};
}

std::expected<void, StarException> WorldGenerator::prepareSectorBiomeBlocks(WorldStorage& worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto& generationRoot = m_worldServer.root();
  auto tileArray = worldStorage.tileArray();
  auto material_database_result = generationRoot.material_database();
  if (!material_database_result) return std::unexpected(std::move(material_database_result.error()));
  RectI sectorTiles = tileArray->sector_region(sector);
  WorldImpl::invalidate_collision_cache(*tileArray, sectorTiles);

  for (int x = sectorTiles.x_min(); x < sectorTiles.x_max(); ++x) {
    for (int y = sectorTiles.y_min(); y < sectorTiles.y_max(); ++y) {
      Vec2I position(x, y);
      auto tile = tileArray->modify_tile(position);
      {
        auto operationResult1 = replace_biome_blocks(tile->get());
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
      {
        auto operationResult1 = placeBiomeGrass(worldStorage, tile->get(), position);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
      {
        auto collision = biome_foreground_collision(**material_database_result, tile->get().foreground, tile->get().foreground, tile->get().collision);
        if (!collision) return std::unexpected(std::move(collision.error()));
        tile->get().update_collision(*collision);
      }
    }
  }

  return {};
}

std::expected<void, StarException> WorldGenerator::placeBiomeGrass(WorldStorage& worldStorage, ServerTile& tile, Vec2I const& position) {
  auto const& worldTemplate = m_worldServer.worldTemplate();
  auto biome_result = worldTemplate->biome(tile.blockBiomeIndex);
  if (!biome_result) return std::unexpected(std::move(biome_result.error()));
  if (auto block_biome = std::move((*biome_result))) {
    // determine layer for grass mod calculation
    TileLayer mod_layer = tile.foreground != empty_material_id ? TileLayer::Foreground : TileLayer::Background;

    // don't place mods in dungeons unless explicitly specified, also don't
    // touch non-grass mods
    if (tile.mod(mod_layer) == biome_mod_id || tile.mod(mod_layer) == underground_biome_mod_id
        || (tile.dungeonId == no_dungeon_id && tile.mod(mod_layer) == no_mod_id)) {
      // check whether we're floor or ceiling
      auto tile_above = worldStorage.tileArray()->tile(position + Vec2I(0, 1));
      auto tile_below = worldStorage.tileArray()->tile(position + Vec2I(0, -1));
      bool is_floor = (tile.foreground != empty_material_id && tile_above.foreground == empty_material_id) || (tile.background != empty_material_id && tile_above.background == empty_material_id);
      bool is_ceiling = !is_floor && ((tile.foreground != empty_material_id && tile_below.foreground == empty_material_id) || (tile.background != empty_material_id && tile_below.background == empty_material_id));

      // get the appropriate placeables for above/below ground
      BiomePlaceables const& placeables =
          ((is_floor && tile_above.background != empty_material_id) || (is_ceiling && tile_below.background != empty_material_id))
          ? block_biome->underground_placeables : block_biome->surface_placeables;

      // determine the proper grass mod or lack thereof
      ModId grass_mod_id = no_mod_id;
      if (is_floor) {
        auto grass_chance = static_random_float(worldTemplate->worldSeed(), position[0], position[1]);
        if (is_real_mod(placeables.grass_mod) && grass_chance <= placeables.grass_mod_density)
          grass_mod_id = placeables.grass_mod;
      } else if (is_ceiling) {
        auto grass_chance = static_random_float(worldTemplate->worldSeed(), position[0], position[1]);
        if (is_real_mod(placeables.ceiling_grass_mod) && grass_chance <= placeables.ceiling_grass_mod_density)
          grass_mod_id = placeables.ceiling_grass_mod;
      }

      // set the selected grass mod
      if (mod_layer == TileLayer::Foreground) {
        tile.foregroundMod = grass_mod_id;
        tile.backgroundMod = no_mod_id;
      } else {
        tile.foregroundMod = no_mod_id;
        tile.backgroundMod = grass_mod_id;
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

std::expected<void, StarException> WorldGenerator::reapply_biome(WorldStorage& world_storage, ServerTileSectorArray::Sector const& sector) {
  auto& generation_root = m_worldServer.root();
  auto const& world_template = m_worldServer.worldTemplate();
  auto const& world_layout = world_template->worldLayout();
  auto materials_result = generation_root.material_database();
  if (!materials_result) return std::unexpected(std::move(materials_result.error()));
  auto const& material_database = **materials_result;
  auto planet = world_template;
  auto tile_array = world_storage.tileArray();
  RectI sector_tiles = tile_array->sector_region(sector);

  // generation_root.logger().info("Reapplying biome in sector {}...", sector_tiles);
  auto entities_result = m_worldServer.entityQuery(RectF(sector_tiles.padded(1)));
  if (!entities_result) return std::unexpected(std::move(entities_result.error()));
  auto entities = std::move((*entities_result));
  List<std::shared_ptr<TileEntity>> biome_tile_entities;
  for (auto const& entity : entities) {
    if (auto plant = as<Plant>(entity)) {
      biome_tile_entities.push_back(as<TileEntity>(entity));
    } else if (auto object = as<Object>(entity)) {
      if (object->biome_placed())
        biome_tile_entities.push_back(as<TileEntity>(entity));
    }
  }

  List<Vec2I> biome_item_tiles;

  for (int x = sector_tiles.x_min(); x < sector_tiles.x_max(); ++x) {
    for (int y = sector_tiles.y_min(); y < sector_tiles.y_max(); ++y) {
      Vec2I position(x, y);
      auto tile_result = m_worldServer.modifyServerTile(position);
      if (!tile_result) return std::unexpected(std::move(tile_result.error()));
      auto tile = std::move((*tile_result));
      if constexpr (Star::debug_enabled) {
        Star::star_assert_failed((tile));
      }
      if (!tile)
        continue;
      auto block_result = planet->blockBiomeInfo(position[0], position[1]);
      if (!block_result) return std::unexpected(std::move(block_result.error()));
      auto block_info = std::move((*block_result));
      if (block_info.blockBiomeIndex != tile->get().blockBiomeIndex) {
        auto new_result = planet->biome(block_info.blockBiomeIndex);
        if (!new_result) return std::unexpected(std::move(new_result.error()));
        auto new_biome = std::move((*new_result));
        auto old_result = planet->biome(tile->get().blockBiomeIndex);
        if (!old_result) return std::unexpected(std::move(old_result.error()));
        auto old_biome = std::move((*old_result));
        MaterialId old_foreground = tile->get().foreground;

        for (auto iter = biome_tile_entities.begin(); iter != biome_tile_entities.end();) {
          if ((*iter)->tile_position() == position) {
            auto removed = m_worldServer.removeEntity((*iter)->entityId(), false);
            if (!removed) return std::unexpected(std::move(removed.error()));
            iter = biome_tile_entities.erase(iter);
          } else {
            ++iter;
          }
        }

        // update biome index
        tile->get().blockBiomeIndex = block_info.blockBiomeIndex;
        tile->get().environmentBiomeIndex = block_info.environmentBiomeIndex;
        tile->get().biomeTransition = true;

        // replace biome blocks
        if (tile->get().foreground == old_biome->main_block || old_biome->sub_blocks.contains(tile->get().foreground)) {
          tile->get().foreground = block_info.foreground;
          tile->get().foregroundColorVariant = default_material_color_variant;
          if (tile->get().foreground == new_biome->main_block)
            tile->get().foregroundHueShift = new_biome->material_hue_shift;
        }

        if (tile->get().background == old_biome->main_block || old_biome->sub_blocks.contains(tile->get().background)) {
          tile->get().background = block_info.background;
          tile->get().backgroundColorVariant = default_material_color_variant;
          if (tile->get().background == new_biome->main_block)
            tile->get().backgroundHueShift = new_biome->material_hue_shift;
        }

        if (tile->get().foreground != empty_material_id || tile->get().background != empty_material_id) {
          // remove old biome mods
          if (tile->get().foregroundMod == old_biome->surface_placeables.grass_mod || tile->get().foregroundMod == old_biome->surface_placeables.ceiling_grass_mod ||
              tile->get().foregroundMod == old_biome->underground_placeables.grass_mod || tile->get().foregroundMod == old_biome->underground_placeables.ceiling_grass_mod) {

            tile->get().foregroundMod = no_mod_id;
            tile->get().foregroundModHueShift = 0;
          }

          if (tile->get().backgroundMod == old_biome->surface_placeables.grass_mod || tile->get().backgroundMod == old_biome->surface_placeables.ceiling_grass_mod ||
              tile->get().backgroundMod == old_biome->underground_placeables.grass_mod || tile->get().backgroundMod == old_biome->underground_placeables.ceiling_grass_mod) {

            tile->get().backgroundMod = no_mod_id;
            tile->get().backgroundModHueShift = 0;
          }

          // apply new biome mods
          TileLayer mod_layer = tile->get().foreground != empty_material_id ? TileLayer::Foreground : TileLayer::Background;

          if (tile->get().mod(mod_layer) == no_mod_id) {
            // check whether we're floor or ceiling
            auto tile_above = world_storage.tileArray()->tile(position + Vec2I(0, 1));
            auto tile_below = world_storage.tileArray()->tile(position + Vec2I(0, -1));
            bool is_floor = tile->get().foreground != empty_material_id && tile_above.foreground == empty_material_id;
            bool is_ceiling = !is_floor && tile->get().foreground != empty_material_id && tile_below.foreground == empty_material_id;
            bool is_mod_floor, is_mod_ceiling;
            if (mod_layer == TileLayer::Foreground) {
              is_mod_floor = is_floor;
              is_mod_ceiling = is_ceiling;
            } else {
              is_mod_floor = tile->get().background != empty_material_id && tile_above.background == empty_material_id;
              is_mod_ceiling = !is_mod_floor && tile->get().background != empty_material_id && tile_below.background == empty_material_id;
            }

            // get the appropriate placeables for above/below ground
            BiomePlaceables const& placeables =
                ((is_floor && tile_above.background != empty_material_id) || (is_ceiling && tile_below.background != empty_material_id))
                ? new_biome->underground_placeables : new_biome->surface_placeables;

            // determine the proper grass mod or lack thereof
            ModId grass_mod_id = no_mod_id;
            if (is_mod_floor) {
              auto grass_chance = static_random_float(world_template->worldSeed(), position[0], position[1]);
              if (is_real_mod(placeables.grass_mod) && grass_chance <= placeables.grass_mod_density)
                grass_mod_id = placeables.grass_mod;
            } else if (is_mod_ceiling) {
              auto grass_chance = static_random_float(world_template->worldSeed(), position[0], position[1]);
              if (is_real_mod(placeables.ceiling_grass_mod) && grass_chance <= placeables.ceiling_grass_mod_density)
                grass_mod_id = placeables.ceiling_grass_mod;
            }

            // set the selected grass mod
            if (auto condition94 = [&]() -> std::expected<bool, StarException> {
              return [&]() -> std::expected<bool, StarException> {
                auto left = [&]() -> std::expected<bool, StarException> {                   auto condition = std::move(mod_layer == TileLayer::Foreground);
                  return static_cast<bool>(condition); }();
                if (!left) return std::unexpected(std::move(left.error()));
                if (!*left) return false;
                                auto condition_result1 = material_database.supports_mod(tile->get().foreground, grass_mod_id);
                if (!condition_result1) return std::unexpected(std::move(condition_result1.error()));
                auto condition = std::move((*condition_result1));
                return static_cast<bool>(condition);
              }();
            }(); !condition94)
              return std::unexpected(std::move(condition94.error()));
            else if (*condition94) {
              tile->get().foregroundMod = grass_mod_id;
              tile->get().backgroundMod = no_mod_id;
            } else if (auto condition93 = [&]() -> std::expected<bool, StarException> {
              return [&]() -> std::expected<bool, StarException> {
                auto left = [&]() -> std::expected<bool, StarException> {                   auto condition = std::move(mod_layer == TileLayer::Background);
                  return static_cast<bool>(condition); }();
                if (!left) return std::unexpected(std::move(left.error()));
                if (!*left) return false;
                                auto condition_result1 = material_database.supports_mod(tile->get().background, grass_mod_id);
                if (!condition_result1) return std::unexpected(std::move(condition_result1.error()));
                auto condition = std::move((*condition_result1));
                return static_cast<bool>(condition);
              }();
            }(); !condition93)
              return std::unexpected(std::move(condition93.error()));
            else if (*condition93) {
              tile->get().foregroundMod = no_mod_id;
              tile->get().backgroundMod = grass_mod_id;
            }
          }
        } else {
          tile->get().foregroundMod = no_mod_id;
          tile->get().backgroundMod = no_mod_id;
        }
        {
          auto collision = biome_foreground_collision(material_database, old_foreground, tile->get().foreground, tile->get().collision);
          if (!collision) return std::unexpected(std::move(collision.error()));
          tile->get().update_collision(*collision);
        }
      }

      if (tile->get().biomeTransition && !block_info.biomeTransition) {
        tile->get().biomeTransition = false;
        if (!isSolidColliding(tile->get().collision))
          biome_item_tiles.push_back(position);
      }
    }
  }

  auto simple_place_plant = [&](std::shared_ptr<Plant> const& plant, Vec2I const& position) -> std::expected<bool, StarException> {
    if (!plant) return false;
    auto spaces = plant->spaces();
    auto roots = plant->roots();
    auto primary_root = plant->primaryRoot();
    auto biome = world_layout->getBiome(world_storage.tileArray()->tile(position).blockBiomeIndex);
    if (!biome) return std::unexpected(std::move(biome.error()));
    auto block_biome = biome->get();

    auto position_valid = [&](Vec2I const& pos) -> std::expected<bool, StarException> {
      auto const& primary_tile = world_storage.tileArray()->tile(pos);
      auto const& primary_root_tile = world_storage.tileArray()->tile(pos + primary_root);
      if (is_connectable_material(primary_tile.foreground) || !is_connectable_material(primary_root_tile.foreground))
        return false;

      for (auto const& root : roots) {
        auto const& root_tile = world_storage.tileArray()->tile(root + pos);
        if (!is_connectable_material(root_tile.foreground) || root_tile.blockBiomeIndex != primary_tile.blockBiomeIndex ||
            (root_tile.foreground != block_biome->main_block && !block_biome->sub_blocks.contains(root_tile.foreground)))
          return false;
      }
      for (auto const& space : spaces) {
        auto tile_pos = space + pos;
        auto occupying = m_worldServer.atTile<TileEntity>(tile_pos);
        if (!occupying) return std::unexpected(std::move(occupying.error()));
        if (!occupying->empty() || world_storage.tileArray()->tile(tile_pos).foreground != empty_material_id)
          return false;
      }
      return true;
    };

    static std::array<Vec2I, 9> const offsets = {{
      {0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {-1, 1}, {-1, -1}, {1, 1}, {1, -1}
    }};
    for (auto const& offset : offsets) {
      auto pos = position + offset;
      auto valid = position_valid(pos);
      if (!valid) return std::unexpected(std::move(valid.error()));
      if (!*valid) continue;
      auto positioned = plant->set_tile_position(pos);
      if (!positioned) return std::unexpected(std::move(positioned.error()));
      auto added = m_worldServer.addEntity(plant);
      if (!added) return std::unexpected(std::move(added.error()));
      return true;
    }
    return false;
  };

  auto place_biome_item = [&](BiomeItem const& item, Vec2I const& position) -> std::expected<void, StarException> {
    auto seed = world_template->seedFor(position[0], position[1]);
    auto place_plant = [&](auto const& variant) -> std::expected<void, StarException>
        requires requires(PlantDatabase const& database) { database.createPlant(variant, seed); } {
      auto plants = generation_root.plantDatabase();
      if (!plants) return std::unexpected(std::move(plants.error()));
      auto plant = (*plants)->createPlant(variant, seed);
      if (!plant) return std::unexpected(std::move(plant.error()));
      auto placed = simple_place_plant(*plant, position);
      if (!placed) return std::unexpected(std::move(placed.error()));
      return {};
    };

    if (std::holds_alternative<GrassVariant>(item)) return place_plant(std::get<GrassVariant>(item));
    if (std::holds_alternative<BushVariant>(item)) return place_plant(std::get<BushVariant>(item));
    if (std::holds_alternative<TreePair>(item)) {
      auto const& pair = std::get<TreePair>(item);
      return place_plant(seed % 2 == 0 ? pair.first : pair.second);
    }
    if (std::holds_alternative<ObjectPool>(item)) {
      auto object = create_biome_object(m_worldServer, std::get<ObjectPool>(item), position, seed);
      if (!object) return std::unexpected(std::move(object.error()));
      if (*object && (*object)->biome_placed()) {
        auto added = m_worldServer.addEntity(*object);
        if (!added) return std::unexpected(std::move(added.error()));
      }
    }
    return {};
  };

  for (auto const& position : biome_item_tiles) {
    auto tile = m_worldServer.modifyServerTile(position);
    if (!tile) return std::unexpected(std::move(tile.error()));
    if (!*tile) continue;
    auto biome = world_layout->getBiome((*tile)->get().blockBiomeIndex);
    if (!biome) return std::unexpected(std::move(biome.error()));
    // Keep the distribution owner alive across entity placement callbacks.
    auto block_biome = biome->get();
    auto above = m_worldServer.getServerTile(position + Vec2I{0, 1});
    if (!above) return std::unexpected(std::move(above.error()));
    bool solid_above = isSolidColliding(above->get().collision);
    auto below = m_worldServer.getServerTile(position + Vec2I{0, -1});
    if (!below) return std::unexpected(std::move(below.error()));
    bool solid_below = isSolidColliding(below->get().collision);

    auto place_mode = [&](BiomePlaceables const& placeables, BiomePlacementMode mode) -> std::expected<void, StarException> {
      for (auto const& distribution : placeables.item_distributions) {
        if (distribution.mode() != mode) continue;
        auto item = distribution.item_at(position[0], position[1]);
        if (!item) return std::unexpected(std::move(item.error()));
        if (*item) {
          auto placed = place_biome_item((*item)->get(), position);
          if (!placed) return std::unexpected(std::move(placed.error()));
        }
      }
      return {};
    };

    if ((*tile)->get().background != empty_material_id) {
      auto background = place_mode(block_biome->underground_placeables, BiomePlacementMode::Background);
      if (!background) return std::unexpected(std::move(background.error()));
      if (solid_above) {
        auto ceiling = place_mode(block_biome->underground_placeables, BiomePlacementMode::Ceiling);
        if (!ceiling) return std::unexpected(std::move(ceiling.error()));
      }
      if (solid_below) {
        auto floor = place_mode(block_biome->underground_placeables, BiomePlacementMode::Floor);
        if (!floor) return std::unexpected(std::move(floor.error()));
      }
    } else if (solid_below) {
      auto floor = place_mode(block_biome->surface_placeables, BiomePlacementMode::Floor);
      if (!floor) return std::unexpected(std::move(floor.error()));
    }
  }

  return {};
}

std::expected<Set<Vec2I>, StarException> WorldGenerator::caveLiquidSeeds(WorldStorage& worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto const& worldTemplate = m_worldServer.worldTemplate();
  RectI sectorTiles = worldStorage.tileArray()->sector_region(sector);
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
  bool adjustBackground = background == empty_material_id || background == null_material_id;

  auto withinAdjustment = [=](Vec2I const& pos) {
    return PlantAdjustmentLimit - std::abs(pos[0]) > 0 && PlantAdjustmentLimit - std::abs(pos[1]) > 0;
  };

  // Bail out if we don't have at least one free space, and root in the primary
  // root position, or if we're in a dungeon region.
  auto primaryTile = worldStorage.tileArray()->tile(position);
  auto rootTile = worldStorage.tileArray()->tile(position + primaryRoot);
  if (primaryTile.dungeonId != no_dungeon_id || rootTile.dungeonId != no_dungeon_id)
    return false;
  if (is_connectable_material(primaryTile.foreground) || !is_connectable_material(rootTile.foreground))
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
    if (tile.rootSource || (!withinAdjustment(space) && tile.foreground != empty_material_id))
      return false;
  }

  // Check all the roots outside of the adjustment limit
  for (auto root : roots) {
    root += position;
    if (!withinAdjustment(root) && !is_connectable_material(worldStorage.tileArray()->tile(root).foreground))
      return false;
  }

  // Clear all the necessary blocks within the adjustment limit
  for (auto space : spaces) {
    if (!withinAdjustment(space))
      continue;

    space += position;
    if (auto tile = worldStorage.tileArray()->modify_tile(space)) {
      auto before = tile->get().getCollision();
      if (is_connectable_material(tile->get().foreground))
        tile->get() = primaryTile;
      if (adjustBackground)
        tile->get().background = empty_material_id;
      tile->get().update_collision(CollisionKind::None);
      if (tile->get().getCollision() != before)
        WorldImpl::invalidate_collision_cache(*worldStorage.tileArray(), RectI::with_size(space, {1, 1}));
    } else {
      return false;
    }
  }

  // Make all the root blocks a real material based on the primary root.
  for (auto root : roots) {
    root += position;
    if (auto tile = worldStorage.tileArray()->modify_tile(root)) {
      if (!is_real_material(tile->get().foreground)) {
        auto material_database_result = generationRoot.material_database();
        if (!material_database_result) return std::unexpected(std::move(material_database_result.error()));
        auto tile_collision_result = (*material_database_result)->material_collision_kind(rootTile.foreground);
        if (!tile_collision_result) return std::unexpected(std::move(tile_collision_result.error()));
        auto before = tile->get().getCollision();
        tile->get() = rootTile;
        tile->get().update_collision(*tile_collision_result);
        if (tile->get().getCollision() != before)
          WorldImpl::invalidate_collision_cache(*worldStorage.tileArray(), RectI::with_size(root, {1, 1}));
      }
    } else {
      return false;
    }
  }
  {
    auto operationResult1 = plant->set_tile_position(position);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = m_worldServer.addEntity(plant);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return true;
}

}
