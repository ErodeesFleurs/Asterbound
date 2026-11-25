import star.json_extra;
import star.json;
import star.xx_hash;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.optional;
import star.game_types;
import star.material_types;
import star.rpc_promise;
import star.vector;
import star.rect;
import star.bi_map;
import star.a_star;
import star.data_stream;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.poly;
import star.color;
import star.string;
import star.lua;
import star.weighted_pool;
import star.logging;
import star.parametric_function;
import star.perlin;
import star.strong_typedef;
import star.set;
import star.math_common;
import star.periodic;
import star.periodic_function;
import star.audio;
import star.map;
import star.ordered_set;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.memory;
import star.format;
import star.output_proxy;
import star.image;
import star.worker_pool;
import star.encode;
import star.byte_array;
import star.entity_factory;
import star.plant_drop;

import std;
import star.sector_array_2d;
import star.net_compatibility;
import star.btree;
import star.data_stream_devices;
import star.btree_database;

import star.utility_lua_bindings;
import star.lua_root;

import star.lua_actor_movement_component;
import star.item;
import star.lua_components;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.entity_rendering;
import star.object;
import star.container_entity;
import star.container_object;
import star.loungeable_object;
import star.farmable_object;
import star.item_database;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.tile_modification;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.item_descriptor;
import star.item_recipe;
import star.drawable;
import star.collision_block;
import star.platformer_astar;
import star.blocks_along_line;
import star.entity_rendering_types;
import star.lounging_entities;
import star.uuid;
import star.humanoid;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.ai_types;
import star.player_types;
import star.radio_message_database;
import star.player;
import star.aggressive_entity;
import star.monster_database;
import star.effect_emitter;
import star.monster;
import star.npc_database;
import star.npc;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;
import star.stagehand;
import star.projectile;
import star.tile_sector_array;
import star.liquid_types;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.chat_types;
import star.warping;
import star.plant;
import star.versioning_database;
import star.world_storage;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.cellular_light_array;
import star.cellular_lighting;
import star.cellular_liquid;
import star.world_structure;
import star.sky_render_data;
import star.parallax;
import star.world_render_data;
import star.world_client_state;
import star.interpolation_tracker;
import star.spawner;
import star.world_server;
import star.ambient;
import star.weather;
import star.world_client;
import star.world_template;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.item_drop;
import star.lua_game_converters;
import star.universe_settings;
import star.plant_database;
import star.biome_placement;
import star.spawn_type_database;
import star.biome;
import star.vehicle;
import star.treasure;

import star.projectile_database;
import star.vehicle_database;
import star.stagehand_database;
import star.material_database;
import star.object_database;
import star.image_metadata_database;


import star.player_inventory;
import star.item_bag;

import star.sky;


import star.line;
import star.algorithm;
import star.pythonic;
namespace Star {

class World;
class WorldServer;
class WorldClient;
class Item;
class ScriptedEntity;

namespace LuaBindings {

  [[nodiscard]] std::expected<void, StarException> addWorldDebugCallbacks(Root& root, LuaCallbacks& callbacks);
  [[nodiscard]] std::expected<void, StarException> addWorldEntityCallbacks(LuaCallbacks& callbacks, World& world);
  [[nodiscard]] std::expected<void, StarException> addWorldEnvironmentCallbacks(LuaCallbacks& callbacks, World& world);
  [[nodiscard]] std::expected<void, StarException> addWorldGeometryCallbacks(LuaCallbacks& callbacks, World& world);
  [[nodiscard]] std::expected<void, StarException> addWorldCollisionCallbacks(LuaCallbacks& callbacks, World& world);

  namespace WorldCallbacks {
    [[nodiscard]] std::expected<bool, StarException> placeObject(World& world, String const& arg1, Vec2I const& arg2, std::optional<int> const& arg3, Json const& arg4);
    [[nodiscard]] std::expected<std::optional<EntityId>, StarException> spawnItem(World& world, Json const& itemType, Vec2F const& world_position, std::optional<std::size_t> const& inputCount, Json const& inputParameters, std::optional<Vec2F> const& initial_velocity, std::optional<float> const& intangibleTime);
    [[nodiscard]] std::expected<List<EntityId>, StarException> spawnTreasure(World& world, Vec2F const& position, String const& pool, float level, std::optional<std::uint64_t> seed);
    [[nodiscard]] std::expected<std::optional<EntityId>, StarException> spawnMonster(World& world, String const& arg1, Vec2F const& arg2, std::optional<JsonObject> const& arg3);
    [[nodiscard]] std::expected<std::optional<EntityId>, StarException> spawnNpc(World& world, Vec2F const& arg1, String const& arg2, String const& arg3, float arg4, std::optional<std::uint64_t> arg5, Json const& arg6);
    [[nodiscard]] std::expected<std::optional<EntityId>, StarException> spawnStagehand(World& world, Vec2F const& spawnPosition, String const& typeName, Json const& overrides);
    [[nodiscard]] std::expected<std::optional<EntityId>, StarException> spawnProjectile(World& world, String const& arg1, Vec2F const& arg2, std::optional<EntityId> const& arg3, std::optional<Vec2F> const& arg4, bool arg5, Json const& arg6);
    [[nodiscard]] std::expected<std::optional<EntityId>, StarException> spawnVehicle(World& world, String const& vehicleName, Vec2F const& pos, Json const& extraConfig);
    [[nodiscard]] std::expected<Json, StarException> getProperty(World& world, String const& arg1, Json const& arg2);
    [[nodiscard]] std::expected<void, StarException> setProperty(World& world, String const& arg1, Json const& arg2);
    [[nodiscard]] std::expected<std::optional<PlatformerAStar::Path>, StarException> findPlatformerPath(World& world, Vec2F const& start, Vec2F const& end, ActorMovementParameters actorMovementParameters, PlatformerAStar::Parameters searchParameters);
    [[nodiscard]] std::expected<std::shared_ptr<PlatformerAStar::PathFinder>, StarException> platformerPathStart(World& world, Vec2F const& start, Vec2F const& end, ActorMovementParameters actorMovementParameters, PlatformerAStar::Parameters searchParameters);
  }

  namespace ClientWorldCallbacks {
    [[nodiscard]] std::expected<void, StarException> resendEntity(WorldClient& world, EntityId arg1);
    [[nodiscard]] std::expected<RectI, StarException> clientWindow(WorldClient& world);
  }

  namespace ServerWorldCallbacks {
    [[nodiscard]] std::expected<String, StarException> id(WorldServer& world);
    [[nodiscard]] std::expected<bool, StarException> breakObject(WorldServer& world, EntityId arg1, bool arg2);
    [[nodiscard]] std::expected<bool, StarException> isVisibleToPlayer(WorldServer& world, RectF const& arg1);
    [[nodiscard]] std::expected<bool, StarException> loadRegion(WorldServer& world, RectF const& arg1);
    [[nodiscard]] std::expected<bool, StarException> regionActive(WorldServer& world, RectF const& arg1);
    [[nodiscard]] std::expected<void, StarException> setTileProtection(WorldServer& world, DungeonId arg1, bool arg2);
    [[nodiscard]] std::expected<bool, StarException> isPlayerModified(WorldServer& world, RectI const& region);
    [[nodiscard]] std::expected<std::optional<LiquidLevel>, StarException> forceDestroyLiquid(WorldServer& world, Vec2F const& position);
    [[nodiscard]] std::expected<EntityId, StarException> loadUniqueEntity(WorldServer& world, String const& uniqueId);
    [[nodiscard]] std::expected<void, StarException> setUniqueId(WorldServer& world, EntityId entityId, std::optional<String> const& uniqueId);
    [[nodiscard]] std::expected<Json, StarException> take_item_drop(World& world, EntityId entity_id, std::optional<EntityId> const& taken_by);
    [[nodiscard]] std::expected<void, StarException> setPlayerStart(WorldServer& world, Vec2F const& playerStart, std::optional<bool> respawnInWorld);
    [[nodiscard]] std::expected<List<EntityId>, StarException> players(WorldServer& world);
    [[nodiscard]] std::expected<LuaString, StarException> fidelity(WorldServer& world, LuaEngine& engine);
    [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> callScriptContext(WorldServer& world, String const& contextName, String const& function, LuaVariadic<LuaValue> const& args);
    [[nodiscard]] std::expected<bool, StarException> sendPacket(WorldServer& world, ConnectionId clientId, String const& packetType, Json const& packetData);
  }

  namespace WorldGeometryCallbacks {
    [[nodiscard]] std::expected<float, StarException> magnitude(World& world, Vec2F pos1, std::optional<Vec2F> pos2);
    [[nodiscard]] std::expected<Vec2F, StarException> distance(World& world, Vec2F const& arg1, Vec2F const& arg2);
    [[nodiscard]] std::expected<bool, StarException> polyContains(World& world, PolyF const& poly, Vec2F const& pos);
    [[nodiscard]] std::expected<LuaValue, StarException> xwrap(World& world, LuaEngine& engine, LuaValue const& positionOrX);
    [[nodiscard]] std::expected<LuaValue, StarException> nearestTo(World& world, LuaEngine& engine, std::variant<Vec2F, float> const& sourcePositionOrX, std::variant<Vec2F, float> const& targetPositionOrX);
  }

  namespace WorldCollisionCallbacks {
    [[nodiscard]] std::expected<bool, StarException> rectCollision(World& world, RectF const& arg1, std::optional<CollisionSet> const& arg2);
    [[nodiscard]] std::expected<bool, StarException> pointTileCollision(World& world, Vec2F const& arg1, std::optional<CollisionSet> const& arg2);
    [[nodiscard]] std::expected<bool, StarException> line_tile_collision(World& world, Vec2F const& begin, Vec2F const& end, std::optional<CollisionSet> const& collision_set);
    [[nodiscard]] std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> line_tile_collision_point(World& world, Vec2F const& begin, Vec2F const& end, std::optional<CollisionSet> const& collision_set);
    [[nodiscard]] std::expected<bool, StarException> rectTileCollision(World& world, RectF const& arg1, std::optional<CollisionSet> const& arg2);
    [[nodiscard]] std::expected<LuaTupleReturn<std::optional<Vec2F>, std::optional<Vec2F>>, StarException> line_collision(World& world, Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& kinds);
    [[nodiscard]] std::expected<bool, StarException> poly_collision(World& world, PolyF const& poly, std::optional<Vec2F> const& offset, std::optional<CollisionSet> const& kinds);
    [[nodiscard]] std::expected<List<Vec2I>, StarException> collision_blocks_along_line(World& world, Vec2F const& begin, Vec2F const& end, std::optional<CollisionSet> const& collision_set, std::optional<int> const& max_size);
    [[nodiscard]] std::expected<std::optional<Vec2F>, StarException> resolvePolyCollision(World& world, PolyF poly, Vec2F const& position, float maximumCorrection, std::optional<CollisionSet> const& collisionSet);
  }

  namespace WorldDebugCallbacks {
    [[nodiscard]] std::expected<void, StarException> debugPoint(Root& root, Vec2F const& arg1, Color const& arg2);
    [[nodiscard]] std::expected<void, StarException> debugLine(Root& root, Vec2F const& arg1, Vec2F const& arg2, Color const& arg3);
    [[nodiscard]] std::expected<void, StarException> debugPoly(Root& root, PolyF const& poly, Color const& color);
    [[nodiscard]] std::expected<void, StarException> debugText(Root& root, LuaEngine& engine, LuaVariadic<LuaValue> const& args);
  }

  namespace WorldEntityCallbacks {
    [[nodiscard]] std::expected<LuaTable, StarException> entityQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options);
    [[nodiscard]] std::expected<LuaTable, StarException> monsterQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options);
    [[nodiscard]] std::expected<LuaTable, StarException> npcQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options);
    [[nodiscard]] std::expected<LuaTable, StarException> objectQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options);
    [[nodiscard]] std::expected<LuaTable, StarException> itemDropQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options);
    [[nodiscard]] std::expected<LuaTable, StarException> playerQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options);
    [[nodiscard]] std::expected<LuaTable, StarException> loungeableQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options);
    [[nodiscard]] std::expected<LuaTable, StarException> entityLineQuery(World& world, LuaEngine& engine, Vec2F const& pos1, Vec2F const& pos2, std::optional<LuaTable> options);
    [[nodiscard]] std::expected<LuaTable, StarException> objectLineQuery(World& world, LuaEngine& engine, Vec2F const& pos1, Vec2F const& pos2, std::optional<LuaTable> options);
    [[nodiscard]] std::expected<LuaTable, StarException> npcLineQuery(World& world, LuaEngine& engine, Vec2F const& pos1, Vec2F const& pos2, std::optional<LuaTable> options);
    [[nodiscard]] std::expected<bool, StarException> entityExists(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<Json>, StarException> entitySnapshot(World& world, EntityId entityId);
    [[nodiscard]] std::expected<bool, StarException> entityCanDamage(World& world, EntityId sourceId, EntityId targetId);
    [[nodiscard]] std::expected<Json, StarException> entityDamageTeam(World& world, EntityId entityId);
    [[nodiscard]] std::expected<bool, StarException> entityAggressive(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<LuaString>, StarException> entity_type(World& world, LuaEngine& engine, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<Vec2F>, StarException> entityPosition(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<Vec2F>, StarException> entityVelocity(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<RectF>, StarException> entityMetaBoundBox(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<std::uint64_t>, StarException> entityCurrency(World& world, EntityId entityId, String const& currencyType);
    [[nodiscard]] std::expected<std::optional<std::uint64_t>, StarException> entityHasCountOfItem(World& world, EntityId entityId, Json descriptor, std::optional<bool> exactMatch);
    [[nodiscard]] std::expected<std::optional<Vec2F>, StarException> entityHealth(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<String>, StarException> entitySpecies(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<String>, StarException> entityGender(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<String>, StarException> entityName(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<Json>, StarException> entityNametag(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<String>, StarException> entityDescription(World& world, EntityId entityId, std::optional<String> const& species);
    [[nodiscard]] std::expected<std::optional<List<Drawable>>, StarException>
    entityPortrait(World& world, EntityId entityId, String const& portraitMode);
    [[nodiscard]] std::expected<std::optional<String>, StarException> entityHandItem(World& world, EntityId entityId, String const& handName);
    [[nodiscard]] std::expected<Json, StarException> entityHandItemDescriptor(World& world, EntityId entityId, String const& handName);
    [[nodiscard]] std::expected<std::optional<String>, StarException> entityUniqueId(World& world, EntityId entityId);
    [[nodiscard]] std::expected<Json, StarException> getObjectParameter(World& world, EntityId entityId, String const& parameterName, std::optional<Json> const& defaultValue);
    [[nodiscard]] std::expected<Json, StarException> getNpcScriptParameter(World& world, EntityId entityId, String const& parameterName, std::optional<Json> const& defaultValue);
    [[nodiscard]] std::expected<List<Vec2I>, StarException> objectSpaces(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<int>, StarException> farmableStage(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<int>, StarException> containerSize(World& world, EntityId entityId);
    [[nodiscard]] std::expected<bool, StarException> containerClose(World& world, EntityId entityId);
    [[nodiscard]] std::expected<bool, StarException> containerOpen(World& world, EntityId entityId);
    [[nodiscard]] std::expected<Json, StarException> containerItems(World& world, EntityId entityId);
    [[nodiscard]] std::expected<Json, StarException> containerItemAt(World& world, EntityId entityId, std::size_t offset);
    [[nodiscard]] std::expected<std::optional<bool>, StarException> containerConsume(World& world, EntityId entityId, Json const& items);
    [[nodiscard]] std::expected<std::optional<bool>, StarException> containerConsumeAt(World& world, EntityId entityId, std::size_t offset, int count);
    [[nodiscard]] std::expected<std::optional<std::size_t>, StarException> containerAvailable(World& world, EntityId entityId, Json const& items);
    [[nodiscard]] std::expected<Json, StarException> containerTakeAll(World& world, EntityId entityId);
    [[nodiscard]] std::expected<Json, StarException> containerTakeAt(World& world, EntityId entityId, std::size_t offset);
    [[nodiscard]] std::expected<Json, StarException> containerTakeNumItemsAt(World& world, EntityId entityId, std::size_t offset, int const& count);
    [[nodiscard]] std::expected<std::optional<std::size_t>, StarException> containerItemsCanFit(World& world, EntityId entityId, Json const& items);
    [[nodiscard]] std::expected<Json, StarException> containerItemsFitWhere(World& world, EntityId entityId, Json const& items);
    [[nodiscard]] std::expected<Json, StarException> containerAddItems(World& world, EntityId entityId, Json const& items);
    [[nodiscard]] std::expected<Json, StarException> containerStackItems(World& world, EntityId entityId, Json const& items);
    [[nodiscard]] std::expected<Json, StarException> containerPutItemsAt(World& world, EntityId entityId, Json const& items, std::size_t offset);
    [[nodiscard]] std::expected<Json, StarException> containerSwapItems(World& world, EntityId entityId, Json const& items, std::size_t offset);
    [[nodiscard]] std::expected<Json, StarException> containerSwapItemsNoCombine(World& world, EntityId entityId, Json const& items, std::size_t offset);
    [[nodiscard]] std::expected<Json, StarException> containerItemApply(World& world, EntityId entityId, Json const& items, std::size_t offset);
    [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> callScriptedEntity(World& world, EntityId entityId, String const& function, LuaVariadic<LuaValue> const& args);
    [[nodiscard]] std::expected<RpcPromise<Vec2F>, StarException> findUniqueEntity(World& world, String const& uniqueId);
    [[nodiscard]] std::expected<RpcPromise<Json>, StarException> send_entity_message(World& world, LuaEngine& engine, LuaValue target_id, String const& message, LuaVariadic<Json> args);
    [[nodiscard]] std::expected<std::optional<List<EntityId>>, StarException> loungingEntities(World& world, EntityId entityId, std::optional<std::size_t> anchorIndex);
    [[nodiscard]] std::expected<std::optional<bool>, StarException> loungeableOccupied(World& world, EntityId entityId, std::optional<std::size_t> anchorIndex);
    [[nodiscard]] std::expected<std::optional<std::size_t>, StarException> loungeableAnchorCount(World& world, EntityId entityId);
    [[nodiscard]] std::expected<bool, StarException> isMonster(World& world, EntityId entityId, std::optional<bool> const& aggressive);
    [[nodiscard]] std::expected<std::optional<String>, StarException> monsterType(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<String>, StarException> npcType(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<String>, StarException> stagehandType(World& world, EntityId entityId);
    [[nodiscard]] std::expected<bool, StarException> isNpc(World& world, EntityId entityId, std::optional<int> const& damageTeam);
  }

  namespace WorldEnvironmentCallbacks {
    [[nodiscard]] std::expected<float, StarException> lightLevel(World& world, Vec2F const& position);
    [[nodiscard]] std::expected<float, StarException> windLevel(World& world, Vec2F const& position);
    [[nodiscard]] std::expected<bool, StarException> breathable(World& world, Vec2F const& position);
    [[nodiscard]] std::expected<bool, StarException> underground(World& world, Vec2F const& position);
    [[nodiscard]] std::expected<LuaValue, StarException> material(World& world, LuaEngine& engine, Vec2F const& position, String const& layerName);
    [[nodiscard]] std::expected<LuaValue, StarException> mod(World& world, LuaEngine& engine, Vec2F const& position, String const& layerName);
    [[nodiscard]] std::expected<float, StarException> materialHueShift(World& world, Vec2F const& position, String const& layerName);
    [[nodiscard]] std::expected<float, StarException> modHueShift(World& world, Vec2F const& position, String const& layerName);
    [[nodiscard]] std::expected<MaterialColorVariant, StarException> materialColor(World& world, Vec2F const& position, String const& layerName);
    [[nodiscard]] std::expected<void, StarException> setMaterialColor(World& world, Vec2F const& position, String const& layerName, MaterialColorVariant color);
    [[nodiscard]] std::expected<bool, StarException> damageTiles(World& world, List<Vec2I> const& arg1, String const& arg2, Vec2F const& arg3, String const& arg4, float arg5, std::optional<unsigned> const& arg6, std::optional<EntityId> sourceEntity);
    [[nodiscard]] std::expected<bool, StarException> damageTileArea(World& world, Vec2F center, float radius, String layer, Vec2F sourcePosition, String damageType, float damage, std::optional<unsigned> const& harvestLevel, std::optional<EntityId> sourceEntity);
    [[nodiscard]] std::expected<bool, StarException> placeMaterial(World& world, Vec2I const& arg1, String const& arg2, String const& arg3, std::optional<int> const& arg4, bool arg5);
    [[nodiscard]] std::expected<bool, StarException> replaceMaterials(World& world, List<Vec2I> const& tilePositions, String const& layer, String const& materialName, std::optional<int> const& hueShift, bool enableDrops);
    [[nodiscard]] std::expected<bool, StarException> replaceMaterialArea(World& world, Vec2F center, float radius, String const& layer, String const& materialName, std::optional<int> const& hueShift, bool enableDrops);
    [[nodiscard]] std::expected<bool, StarException> placeMod(World& world, Vec2I const& arg1, String const& arg2, String const& arg3, std::optional<int> const& arg4, bool arg5);
    [[nodiscard]] std::expected<List<std::pair<Vec2I, LiquidLevel>>, StarException> liquid_along_line(World& world, Vec2F const& start, Vec2F const& end);
    [[nodiscard]] std::expected<bool, StarException> tileIsOccupied(World& world, Vec2I const& arg1, std::optional<bool> const& arg2, std::optional<bool> const& arg3);
    [[nodiscard]] std::expected<double, StarException> time(World& world);
    [[nodiscard]] std::expected<std::uint64_t, StarException> day(World& world);
    [[nodiscard]] std::expected<double, StarException> timeOfDay(World& world);
    [[nodiscard]] std::expected<float, StarException> dayLength(World& world);
    [[nodiscard]] std::expected<std::optional<LiquidLevel>, StarException> liquidAt(World& world, std::variant<RectF, Vec2I> boundBoxOrPoint);
    [[nodiscard]] std::expected<float, StarException> gravity(World& world, Vec2F const& arg1);
    [[nodiscard]] std::expected<bool, StarException> spawnLiquid(World& world, Vec2F const& arg1, LiquidId arg2, float arg3);
    [[nodiscard]] std::expected<std::optional<LiquidLevel>, StarException> destroyLiquid(World& world, Vec2F const& position);
    bool isTileProtected(World& world, Vec2F const& position);
  }
}

}

namespace Star {
namespace LuaBindings {

  enum class EntityBoundMode { MetaBoundBox, CollisionArea, Position };

  auto const EntityBoundModeNames = EnumMap<EntityBoundMode>::create({
    {EntityBoundMode::MetaBoundBox, "MetaBoundBox"},
    {EntityBoundMode::CollisionArea, "CollisionArea"},
    {EntityBoundMode::Position, "Position"}
  });

  template <typename EntityT>
  using Selector = std::function<std::expected<bool, StarException>(std::shared_ptr<EntityT> const&)>;

  template <typename EntityT>
  [[nodiscard]] std::expected<LuaTable, StarException> entityQueryImpl(World& world, LuaEngine& engine, LuaTable const& options, Selector<EntityT> selector) {
    auto withoutEntityIdResult = options.get<std::optional<EntityId>>("withoutEntityId");
    if (!withoutEntityIdResult)
      return std::unexpected(std::move(withoutEntityIdResult.error()));
    auto withoutEntityId = std::move(*withoutEntityIdResult);
    std::optional<Set<EntityType>> includedTypes;
    auto typesResult = options.get<std::optional<LuaTable>>("includedTypes");
    if (!typesResult)
      return std::unexpected(std::move(typesResult.error()));
    auto types = std::move(*typesResult);
    if (types) {
      includedTypes = Set<EntityType>();
      auto iteration = types->iterate([&includedTypes](LuaValue const&, LuaString const& type) -> std::expected<void, StarException> {
        if (type == "mobile") {
          includedTypes->add(EntityType::Player);
          includedTypes->add(EntityType::Monster);
          includedTypes->add(EntityType::Npc);
          includedTypes->add(EntityType::Projectile);
          includedTypes->add(EntityType::ItemDrop);
          includedTypes->add(EntityType::PlantDrop);
          includedTypes->add(EntityType::Vehicle);
        } else if (type == "creature") {
          includedTypes->add(EntityType::Player);
          includedTypes->add(EntityType::Monster);
          includedTypes->add(EntityType::Npc);
        } else {
          if (!EntityTypeNames)
            return std::unexpected(EntityTypeNames.error());
          auto entityTypeResult = EntityTypeNames->get_left(type.to_string());
          if (!entityTypeResult)
            return std::unexpected(std::move(entityTypeResult.error()));
          auto entityType = std::move(*entityTypeResult);
          includedTypes->add(entityType.get());
        }
        return {};
      });
      if (!iteration)
        return std::unexpected(std::move(iteration.error()));
    }

    auto callScriptNameResult = options.get<std::optional<String>>("callScript");
    if (!callScriptNameResult)
      return std::unexpected(std::move(callScriptNameResult.error()));
    auto callScript = std::move(*callScriptNameResult);
    auto callScriptArgsOptionalResult = options.get<std::optional<List<LuaValue>>>("callScriptArgs");
    if (!callScriptArgsOptionalResult)
      return std::unexpected(std::move(callScriptArgsOptionalResult.error()));
    auto callScriptArgsOptional = std::move(*callScriptArgsOptionalResult);
    auto callScriptArgs = std::move(callScriptArgsOptional).value_or(List<LuaValue>{});
    auto callScriptResultOptionalResult = options.get<std::optional<LuaValue>>("callScriptResult");
    if (!callScriptResultOptionalResult)
      return std::unexpected(std::move(callScriptResultOptionalResult.error()));
    auto callScriptResultOptional = std::move(*callScriptResultOptionalResult);
    auto callScriptResult = callScriptResultOptional.value_or(LuaBoolean(true));

    auto lineQueryResult = options.get<std::optional<Line2F>>("line");
    if (!lineQueryResult)
      return std::unexpected(std::move(lineQueryResult.error()));
    auto lineQuery = std::move(*lineQueryResult);
    auto polyQueryResult = options.get<std::optional<PolyF>>("poly");
    if (!polyQueryResult)
      return std::unexpected(std::move(polyQueryResult.error()));
    auto polyQuery = std::move(*polyQueryResult);
    auto rectQueryResult = options.get<std::optional<RectF>>("rect");
    if (!rectQueryResult)
      return std::unexpected(std::move(rectQueryResult.error()));
    auto rectQuery = std::move(*rectQueryResult);
    std::optional<std::pair<Vec2F, float>> radiusQuery;
    auto radiusResult = options.get<std::optional<float>>("radius");
    if (!radiusResult)
      return std::unexpected(std::move(radiusResult.error()));
    auto radius = std::move(*radiusResult);
    if (radius) {
      auto centerResult = options.get<Vec2F>("center");
      if (!centerResult)
        return std::unexpected(std::move(centerResult.error()));
      auto center = std::move(*centerResult);
      radiusQuery = std::make_pair(center, *radius);
    }
    
    bool globalQuery = !lineQuery && !polyQuery && !rectQuery && !radiusQuery;

    auto boundModeNameResult = options.get<std::optional<String>>("boundMode");
    if (!boundModeNameResult)
      return std::unexpected(std::move(boundModeNameResult.error()));
    auto boundModeName = std::move(*boundModeNameResult);
    if (!EntityBoundModeNames)
      return std::unexpected(EntityBoundModeNames.error());
    auto boundModePointerResult = EntityBoundModeNames->get_left(boundModeName.value_or("CollisionArea"));
    if (!boundModePointerResult)
      return std::unexpected(std::move(boundModePointerResult.error()));
    auto boundModePointer = std::move(*boundModePointerResult);
    auto boundMode = boundModePointer.get();
    auto orderResult = options.get<std::optional<LuaString>>("order");
    if (!orderResult)
      return std::unexpected(std::move(orderResult.error()));
    auto order = std::move(*orderResult);

    auto geometry = world.geometry();

    auto innerSelector = [=, &callScriptArgs](std::shared_ptr<EntityT> const& entity) -> std::expected<bool, StarException> {
      if (selector) {
        auto selectedResult = selector(entity);
        if (!selectedResult)
          return std::unexpected(std::move(selectedResult.error()));
        auto selected = std::move(*selectedResult);
        if (!selected)
          return false;
      }

      if (includedTypes && !includedTypes->contains(entity->entity_type()))
        return false;

      if (withoutEntityId && entity->entityId() == *withoutEntityId)
        return false;

      if (callScript) {
        auto scriptedEntity = as<ScriptedEntity>(entity);
        if (!scriptedEntity || !scriptedEntity->isMaster())
          return false;

        auto scriptResult = scriptedEntity->call_script(*callScript, lua_unpack(callScriptArgs));
        if (!scriptResult)
          return std::unexpected(std::move(scriptResult.error()));
        auto& res = *scriptResult;
        if (!res || *res != callScriptResult)
          return false;
      }

      auto position = entity->position();
      if (globalQuery) {
        return true;
      } else if (boundMode == EntityBoundMode::MetaBoundBox) {
        // If using MetaBoundBox, the regular line / box query methods already
        // enforce collision with MetaBoundBox
        if (radiusQuery)
          return geometry.rect_intersects_circle(
              entity->meta_bound_box().translated(position), radiusQuery->first, radiusQuery->second);
      } else if (boundMode == EntityBoundMode::CollisionArea) {
        // Collision area queries either query based on the collision area if
        // that's given, or as a fallback the regular bound box.
        auto collisionArea = entity->collision_area();
        if (collisionArea.is_null())
          collisionArea = entity->meta_bound_box();
        collisionArea.translate(position);

        if (lineQuery)
          return geometry.line_intersects_rect(*lineQuery, collisionArea);
        if (polyQuery)
          return geometry.poly_intersects_poly(*polyQuery, PolyF(collisionArea));
        if (rectQuery)
          return geometry.rect_intersects_rect(*rectQuery, collisionArea);
        if (radiusQuery)
          return geometry.rect_intersects_circle(collisionArea, radiusQuery->first, radiusQuery->second);
      } else if (boundMode == EntityBoundMode::Position) {
        if (lineQuery)
          return geometry.line_intersects_rect(*lineQuery, RectF(position, position));
        if (polyQuery)
          return geometry.poly_contains(*polyQuery, position);
        if (rectQuery)
          return geometry.rect_contains(*rectQuery, position);
        if (radiusQuery)
          return geometry.diff(radiusQuery->first, position).magnitude() <= radiusQuery->second;
      }

      return true;
    };

    List<std::shared_ptr<EntityT>> entities;

    if (lineQuery) {
      auto queriedResult = world.lineQuery<EntityT>(lineQuery->min(), lineQuery->max(), innerSelector);
      if (!queriedResult)
        return std::unexpected(std::move(queriedResult.error()));
      auto queried = std::move(*queriedResult);
      entities = std::move(queried);
    } else if (polyQuery) {
      auto queriedResult = world.query<EntityT>(polyQuery->bounding_box(), innerSelector);
      if (!queriedResult)
        return std::unexpected(std::move(queriedResult.error()));
      auto queried = std::move(*queriedResult);
      entities = std::move(queried);
    } else if (rectQuery) {
      auto queriedResult = world.query<EntityT>(*rectQuery, innerSelector);
      if (!queriedResult)
        return std::unexpected(std::move(queriedResult.error()));
      auto queried = std::move(*queriedResult);
      entities = std::move(queried);
    } else if (radiusQuery) {
      RectF region(radiusQuery->first - Vec2F::filled(radiusQuery->second),
          radiusQuery->first + Vec2F::filled(radiusQuery->second));
      auto queriedResult = world.query<EntityT>(region, innerSelector);
      if (!queriedResult)
        return std::unexpected(std::move(queriedResult.error()));
      auto queried = std::move(*queriedResult);
      entities = std::move(queried);
    } else {
      // global query
      auto queried = world.forAllEntities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
        if (auto ofType = as<EntityT>(entity)) {
          auto selectedResult = innerSelector(ofType);
          if (!selectedResult)
            return std::unexpected(std::move(selectedResult.error()));
          auto selected = std::move(*selectedResult);
          if (selected)
            entities.emplace_back(ofType);
        }
        return {};
      });
      if (!queried)
        return std::unexpected(std::move(queried.error()));
    }

    if (order) {
      if (*order == "nearest") {
        Vec2F nearestPosition;
        if (lineQuery)
          nearestPosition = lineQuery->min();
        else if (polyQuery)
          nearestPosition = polyQuery->center();
        else if (rectQuery)
          nearestPosition = rectQuery->center();
        else if (radiusQuery)
          nearestPosition = radiusQuery->first;
        sort_by_key(entities,
            [&world, nearestPosition](std::shared_ptr<EntityT> const& entity) {
              return world.geometry().diff(entity->position(), nearestPosition).magnitude();
            });
      } else if (*order == "random") {
        world.root().random().shuffle(entities);
      } else {
        return std::unexpected(StarException(Star::format("Unsupported query order {}", order->view())));
      }
    }

    if (entities.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
      return std::unexpected(LuaException("Too many entries for a Lua entity query table"));
    auto table = engine.create_table(static_cast<int>(entities.size()), 0);
    if (!table)
      return std::unexpected(std::move(table.error()));
    LuaTable entityIds = std::move(*table);
    int entityIdsIndex = 1;
    for (auto const& entity : entities) {
      if (auto result = entityIds.set(entityIdsIndex++, entity->entityId()); !result)
        return std::unexpected(std::move(result.error()));
    }

    return entityIds;
  }

  template <typename EntityT>
  [[nodiscard]] std::expected<LuaTable, StarException> entityQuery(World& world,
      LuaEngine& engine,
      Vec2F const& pos1,
      LuaValue const& pos2,
      std::optional<LuaTable> options,
      Selector<EntityT> selector = {}) {
    if (!options) {
      auto table = engine.create_table();
      if (!table)
        return std::unexpected(std::move(table.error()));
      options = std::move(*table);
    }
    auto radiusResult = engine.lua_maybe_to<float>(pos2);
    if (!radiusResult)
      return std::unexpected(std::move(radiusResult.error()));
    auto radius = std::move(*radiusResult);
    if (radius) {
      if (auto result = options->set("center", pos1); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = options->set("radius", *radius); !result)
        return std::unexpected(std::move(result.error()));
    } else {
      auto positionResult = engine.lua_to<Vec2F>(pos2);
      if (!positionResult)
        return std::unexpected(std::move(positionResult.error()));
      auto position = std::move(*positionResult);
      if (auto result = options->set("rect", RectF(pos1, position)); !result)
        return std::unexpected(std::move(result.error()));
    }
    return entityQueryImpl<EntityT>(world, engine, *options, std::move(selector));
  }

  template <typename EntityT>
  [[nodiscard]] std::expected<LuaTable, StarException> entityLineQuery(World& world,
      LuaEngine& engine,
      Vec2F const& point1,
      Vec2F const& point2,
      std::optional<LuaTable> options,
      Selector<EntityT> selector = {}) {
    if (!options) {
      auto table = engine.create_table();
      if (!table)
        return std::unexpected(std::move(table.error()));
      options = std::move(*table);
    }
    if (auto result = options->set("line", Line2F(point1, point2)); !result)
      return std::unexpected(std::move(result.error()));
    return entityQueryImpl<EntityT>(world, engine, *options, std::move(selector));
  }
  
  // Thread-safe callbacks that can be used from any thread under the world
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeWorldThreadCallbacks(Root& root, World& world) {
    LuaCallbacks callbacks;
    
    if (auto result = addWorldDebugCallbacks(root, callbacks); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = addWorldGeometryCallbacks(callbacks, world); !result)
      return std::unexpected(std::move(result.error()));
    
    return callbacks;
  }

  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeWorldCallbacks(Root& root, World& world) {
    auto baseCallbacks = makeWorldThreadCallbacks(root, world);
    if (!baseCallbacks)
      return std::unexpected(std::move(baseCallbacks.error()));
    LuaCallbacks callbacks = std::move(*baseCallbacks);

    if (auto result = addWorldEnvironmentCallbacks(callbacks, world); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = addWorldEntityCallbacks(callbacks, world); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = addWorldCollisionCallbacks(callbacks, world); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback_with_signature<bool, String, Vec2I, std::optional<int>, Json>(
          "placeObject",
          [&world](String const& name, Vec2I const& position, std::optional<int> const& direction,
                   Json const& parameters) {
            return WorldCallbacks::placeObject(world, name, position, direction, parameters);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<std::optional<EntityId>, Json, Vec2F, std::optional<std::size_t>,
                                                  Json, std::optional<Vec2F>, std::optional<float>>(
            "spawnItem",
            [&world](Json const& item, Vec2F const& position, std::optional<std::size_t> const& count,
                     Json const& parameters, std::optional<Vec2F> const& velocity,
                     std::optional<float> const& intangibleTime) {
              return WorldCallbacks::spawnItem(world, item, position, count, parameters, velocity, intangibleTime);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<List<EntityId>, Vec2F, String, float, std::optional<std::uint64_t>>(
            "spawnTreasure",
            [&world](Vec2F const& position, String const& pool, float level, std::optional<std::uint64_t> seed) {
              return WorldCallbacks::spawnTreasure(world, position, pool, level, seed);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<std::optional<EntityId>, String, Vec2F, std::optional<JsonObject>>(
            "spawnMonster",
            [&world](String const& type, Vec2F const& position, std::optional<JsonObject> const& parameters) {
              return WorldCallbacks::spawnMonster(world, type, position, parameters);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<EntityId>, Vec2F, String, String, float,
                                                              std::optional<std::uint64_t>, Json>(
          "spawnNpc",
          [&world](Vec2F const& position, String const& species, String const& type, float level,
                   std::optional<std::uint64_t> seed, Json const& parameters) {
            return WorldCallbacks::spawnNpc(world, position, species, type, level, seed, parameters);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<EntityId>, Vec2F, String, Json>(
          "spawnStagehand",
          [&world](Vec2F const& position, String const& type, Json const& parameters) {
            return WorldCallbacks::spawnStagehand(world, position, type, parameters);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<std::optional<EntityId>, String, Vec2F, std::optional<EntityId>,
                                                  std::optional<Vec2F>, bool, Json>(
            "spawnProjectile",
            [&world](String const& type, Vec2F const& position, std::optional<EntityId> const& source,
                     std::optional<Vec2F> const& direction, bool trackSource, Json const& parameters) {
              return WorldCallbacks::spawnProjectile(world, type, position, source, direction, trackSource, parameters);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<EntityId>, String, Vec2F, Json>(
          "spawnVehicle",
          [&world](String const& type, Vec2F const& position, Json const& parameters) {
            return WorldCallbacks::spawnVehicle(world, type, position, parameters);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, String, Json>(
          "getProperty",
          [&world](String const& name, Json const& def) { return WorldCallbacks::getProperty(world, name, def); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<void, String, Json>(
          "setProperty",
          [&world](String const& name, Json const& value) { return WorldCallbacks::setProperty(world, name, value); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<PlatformerAStar::Path>, Vec2F, Vec2F,
                                                              ActorMovementParameters, PlatformerAStar::Parameters>(
          "findPlatformerPath",
          [&world](Vec2F const& start, Vec2F const& end, ActorMovementParameters&& movement,
                   PlatformerAStar::Parameters&& search) {
            return WorldCallbacks::findPlatformerPath(world, start, end, std::move(movement), std::move(search));
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<std::shared_ptr<PlatformerAStar::PathFinder>, Vec2F, Vec2F,
                                                  ActorMovementParameters, PlatformerAStar::Parameters>(
            "platformerPathStart",
            [&world](Vec2F const& start, Vec2F const& end, ActorMovementParameters&& movement,
                     PlatformerAStar::Parameters&& search) {
              return WorldCallbacks::platformerPathStart(world, start, end, std::move(movement), std::move(search));
            });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback("name",
                                                 [&world]() -> std::optional<String> {
                                                   if (auto serverWorld = as_ref<WorldServer>(world)) {
                                                     if (auto worldTemplate = serverWorld->get().worldTemplate())
                                                       return worldTemplate->worldName();
                                                   } else if (auto clientWorld = as_ref<WorldClient>(world)) {
                                                     if (auto worldTemplate = clientWorld->get().currentTemplate())
                                                       return worldTemplate->worldName();
                                                   }
                                                   return {};
                                                 });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback(
          "type",
          [&world](LuaEngine& engine) -> std::expected<LuaString, StarException> {
            if (auto serverWorld = as_ref<WorldServer>(world)) {
              if (auto worldParameters = serverWorld->get().worldTemplate()->worldParameters())
                return engine.create_string(worldParameters->typeName);
            } else if (auto clientWorld = as_ref<WorldClient>(world)) {
              if (auto worldParameters = clientWorld->get().currentTemplate()->worldParameters())
                return engine.create_string(worldParameters->typeName);
            }
            return engine.create_string("unknown");
          });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result =
          callbacks.register_callback("inSurfaceLayer",
                                     [&world](Vec2I const& position) -> bool {
                                       if (auto serverWorld = as_ref<WorldServer>(world))
                                         return serverWorld->get().worldTemplate()->inSurfaceLayer(position);
                                       else if (auto clientWorld = as_ref<WorldClient>(world))
                                         return clientWorld->get().currentTemplate()->inSurfaceLayer(position);
                                       return false;
                                     });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback("surfaceLevel",
                                                 [&world]() -> float {
                                                   if (auto serverWorld = as_ref<WorldServer>(world))
                                                     return serverWorld->get().worldTemplate()->surfaceLevel();
                                                   else if (auto clientWorld = as_ref<WorldClient>(world))
                                                     return clientWorld->get().currentTemplate()->surfaceLevel();
                                                   else
                                                     return world.geometry().size()[1] / 2.0f;
                                                 });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback(
          "terrestrial",
          [&world]() -> bool {
            if (auto serverWorld = as_ref<WorldServer>(world)) {
              if (auto worldParameters = serverWorld->get().worldTemplate()->worldParameters())
                return worldParameters->type() == WorldParametersType::TerrestrialWorldParameters;
            } else if (auto clientWorld = as_ref<WorldClient>(world)) {
              if (auto worldParameters = clientWorld->get().currentTemplate()->worldParameters())
                return worldParameters->type() == WorldParametersType::TerrestrialWorldParameters;
            }
            return false;
          });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback("biomeBlocksAt", [&world](Vec2I position) -> std::expected<std::optional<List<MaterialId>>, StarException> {
        std::shared_ptr<WorldTemplate const> worldTemplate;
        if (auto client = as_ref<WorldClient>(world))
          worldTemplate = client->get().currentTemplate();
        else if (auto server = as_ref<WorldServer>(world))
          worldTemplate = server->get().worldTemplate();
        if (worldTemplate) {
          auto blockResult = worldTemplate->blockInfo(position[0], position[1]);
          if (!blockResult)
            return std::unexpected(std::move(blockResult.error()));
          auto block = std::move(*blockResult);
          auto biomeResult = worldTemplate->biome(block.blockBiomeIndex);
          if (!biomeResult)
            return std::unexpected(std::move(biomeResult.error()));
          auto biome = std::move(*biomeResult);
          if (biome) {
          List<MaterialId> blocks = {biome->main_block};
          blocks.append_range(biome->sub_blocks);
          return blocks;
          }
        }
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback("dungeonId",
                                                 [&world](Vec2I position) -> std::expected<DungeonId, StarException> {
                                                   if (auto serverWorld = as_ref<WorldServer>(world)) {
                                                     return serverWorld->get().dungeonId(position);
                                                   } else {
                                                     auto clientWorld = convert_ref<WorldClient>(world);
                                                     if (!clientWorld)
                                                       return std::unexpected(std::move(clientWorld.error()));
                                                     return clientWorld->get().dungeonId(position);
                                                   }
                                                 });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback("biomeAt", [&world](Vec2I position) -> std::expected<Json, StarException> {
        std::shared_ptr<WorldTemplate const> worldTemplate;
        if (auto client = as_ref<WorldClient>(world))
          worldTemplate = client->get().currentTemplate();
        else if (auto server = as_ref<WorldServer>(world))
          worldTemplate = server->get().worldTemplate();
        if (worldTemplate) {
          auto blockResult = worldTemplate->blockInfo(position[0], position[1]);
          if (!blockResult)
            return std::unexpected(std::move(blockResult.error()));
          auto block = std::move(*blockResult);
          auto biomeResult = worldTemplate->biome(block.blockBiomeIndex);
          if (!biomeResult)
            return std::unexpected(std::move(biomeResult.error()));
          auto biome = std::move(*biomeResult);
          if (biome) {
          return biome->to_json();
          }
        }
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback("blockInfoAt", [&world](Vec2I position) -> std::expected<Json, StarException> {
        std::shared_ptr<WorldTemplate const> worldTemplate;
        if (auto client = as_ref<WorldClient>(world))
          worldTemplate = client->get().currentTemplate();
        else if (auto server = as_ref<WorldServer>(world))
          worldTemplate = server->get().worldTemplate();
        if (!worldTemplate)
          return {};
        auto blockResult = worldTemplate->blockInfo(position[0], position[1]);
        if (!blockResult)
          return std::unexpected(std::move(blockResult.error()));
        auto block = std::move(*blockResult);
        return block.toJson();
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto clientWorld = as_ref<WorldClient>(world)) {
      if (auto result = callbacks.register_callback("inWorld", [clientWorld]() { return clientWorld->get().inWorld(); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("mainPlayer", [clientWorld]() -> std::optional<int> {
        if (clientWorld->get().isHeadless()) {
          return {};
        } else {
          return clientWorld->get().clientState().playerId(); 
        }
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("headless", [clientWorld]() { return clientWorld->get().isHeadless();  }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("subWorldId", [clientWorld]() { return clientWorld->get().subWorldId();  }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("isClient", []() { return true;  }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("isServer", []() { return false; }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("latency", [clientWorld]() { return clientWorld->get().latency(); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<void, EntityId>(
            "resendEntity",
            [clientWorld](EntityId entity) { return ClientWorldCallbacks::resendEntity(clientWorld->get(), entity); });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<RectI>(
            "clientWindow", [clientWorld]() { return ClientWorldCallbacks::clientWindow(clientWorld->get()); });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("players", [clientWorld]() -> std::expected<List<EntityId>, StarException> {
        List<EntityId> playerIds;
        auto queried = clientWorld->get().forAllEntities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
          if (entity->entity_type() == EntityType::Player)
            playerIds.emplace_back(entity->entityId());
          return {};
        });
        if (!queried)
          return std::unexpected(std::move(queried.error()));
        return playerIds;
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("callScriptContext", [clientWorld](String const& contextName, String const& function, LuaVariadic<LuaValue> const& args) -> std::expected<std::optional<LuaValue>, StarException> {
        auto context = clientWorld->get().scriptContext(contextName);
        if (!context)
          return std::unexpected(StarException::format("Context {} does not exist", contextName));
        return context->invoke(function, args);
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("unload", [clientWorld]() -> std::expected<void, StarException> {
        if (!clientWorld->get().isHeadless())
          return std::unexpected(StarException::format("Non-headless worlds cannot be unloaded."));
        clientWorld->get().requestDestroy();
        return {};
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("template", [clientWorld]() {
        return clientWorld->get().currentTemplate()->store();
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result =
            callbacks.register_callback("setTemplate",
                                       [clientWorld](Json worldTemplate) -> std::expected<void, StarException> {
                                         clientWorld->get().setTemplate(std::move(worldTemplate));
                                         return {};
                                       });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("wire", [clientWorld](Vec2I outputPosition, std::size_t outputIndex, Vec2I inputPosition, std::size_t inputIndex) -> std::expected<void, StarException> {
        clientWorld->get().wire(outputPosition, outputIndex, inputPosition, inputIndex);
        return {};
      }); !result)
        return std::unexpected(std::move(result.error()));
    }

    if (auto serverWorld = as_ref<WorldServer>(world)) {
      if (auto result = callbacks.register_callback("isClient", []() { return false; }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("isServer", []() { return true;  }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback_with_signature<String>(
            "id", [serverWorld]() { return ServerWorldCallbacks::id(serverWorld->get()); });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<bool, EntityId, bool>(
            "breakObject",
            [serverWorld](EntityId entity, bool smash) {
              return ServerWorldCallbacks::breakObject(serverWorld->get(), entity, smash);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<bool, RectF>(
            "isVisibleToPlayer",
            [serverWorld](RectF const& region) {
              return ServerWorldCallbacks::isVisibleToPlayer(serverWorld->get(), region);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<bool, RectF>("loadRegion",
                                                                             [serverWorld](RectF const& region) {
                                                                               return ServerWorldCallbacks::loadRegion(
                                                                                 serverWorld->get(), region);
                                                                             });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<bool, RectF>(
            "regionActive",
            [serverWorld](RectF const& region) {
              return ServerWorldCallbacks::regionActive(serverWorld->get(), region);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<void, DungeonId, bool>(
            "setTileProtection",
            [serverWorld](DungeonId dungeon, bool protect) {
              return ServerWorldCallbacks::setTileProtection(serverWorld->get(), dungeon, protect);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<bool, RectI>(
            "isPlayerModified",
            [serverWorld](RectI const& region) {
              return ServerWorldCallbacks::isPlayerModified(serverWorld->get(), region);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<std::optional<LiquidLevel>, Vec2F>(
            "forceDestroyLiquid",
            [serverWorld](Vec2F const& position) {
              return ServerWorldCallbacks::forceDestroyLiquid(serverWorld->get(), position);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<EntityId, String>(
            "loadUniqueEntity",
            [serverWorld](String const& uniqueId) {
              return ServerWorldCallbacks::loadUniqueEntity(serverWorld->get(), uniqueId);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<void, EntityId, String>(
            "setUniqueId",
            [serverWorld](EntityId entity, String&& uniqueId) {
              return ServerWorldCallbacks::setUniqueId(serverWorld->get(), entity, std::move(uniqueId));
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<Json, EntityId, std::optional<EntityId>>(
            "takeItemDrop",
            [&world](EntityId entity, std::optional<EntityId> const& taken_by) {
              return ServerWorldCallbacks::take_item_drop(world, entity, taken_by);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<void, Vec2F, std::optional<bool>>(
            "setPlayerStart",
            [serverWorld](Vec2F const& position, std::optional<bool> respawn) {
              return ServerWorldCallbacks::setPlayerStart(serverWorld->get(), position, respawn);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<List<EntityId>>(
            "players", [serverWorld]() { return ServerWorldCallbacks::players(serverWorld->get()); });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<LuaString, LuaEngine&>(
            "fidelity",
            [serverWorld](LuaEngine& engine) { return ServerWorldCallbacks::fidelity(serverWorld->get(), engine); });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result =
            callbacks.register_callback_with_signature<std::optional<LuaValue>, String, String, LuaVariadic<LuaValue>>(
              "callScriptContext",
              [serverWorld](String const& context, String const& function, LuaVariadic<LuaValue> const& args) {
                return ServerWorldCallbacks::callScriptContext(serverWorld->get(), context, function, args);
              });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<bool, ConnectionId, String, Json>(
            "sendPacket",
            [serverWorld](ConnectionId client, String const& type, Json const& data) {
              return ServerWorldCallbacks::sendPacket(serverWorld->get(), client, type, data);
            });
          !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback_with_signature<double>("skyTime", [serverWorld]() {
          return serverWorld->get().sky()->epochTime();
        }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback_with_signature<void, double>("setSkyTime", [serverWorld](double skyTime) {
          return serverWorld->get().sky()->setEpochTime(skyTime);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback("expiryTime", [serverWorld]() -> float { return serverWorld->get().expiry_time(); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("setExpiryTime", [serverWorld](float expiry_time) { return serverWorld->get().set_expiry_time(expiry_time); }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback("wire", [serverWorld](Vec2I outputPosition, std::size_t outputIndex, Vec2I inputPosition, std::size_t inputIndex) -> std::expected<void, StarException> {
        return serverWorld->get().wire(outputPosition, outputIndex, inputPosition, inputIndex);
      }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback("flyingType", [serverWorld]() -> std::expected<String, StarException> {
        if (!FlyingTypeNames)
          return std::unexpected(FlyingTypeNames.error());
        auto namePointerResult = FlyingTypeNames->get_right(serverWorld->get().sky()->flyingType());
        if (!namePointerResult)
          return std::unexpected(std::move(namePointerResult.error()));
        auto namePointer = std::move(*namePointerResult);
        auto const& name = namePointer.get();
        return name;
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("warpPhase", [serverWorld]() -> std::expected<String, StarException> {
        if (!WarpPhaseNames)
          return std::unexpected(WarpPhaseNames.error());
        auto namePointerResult = WarpPhaseNames->get_right(serverWorld->get().sky()->warpPhase());
        if (!namePointerResult)
          return std::unexpected(std::move(namePointerResult.error()));
        auto namePointer = std::move(*namePointerResult);
        auto const& name = namePointer.get();
        return name;
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("setUniverseFlag", [serverWorld](String flagName) { return serverWorld->get().universeSettings()->setFlag(flagName); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("universeFlags", [serverWorld]() { return serverWorld->get().universeSettings()->flags(); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("universeFlagSet", [serverWorld](String const& flagName) { return serverWorld->get().universeSettings()->flags().contains(flagName); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("placeDungeon", [serverWorld](String dungeonName, Vec2I position, std::optional<DungeonId> dungeonId) -> std::expected<bool, StarException> {
        return serverWorld->get().placeDungeon(dungeonName, position, dungeonId);
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("tryPlaceDungeon", [serverWorld](String dungeonName, Vec2I position, std::optional<DungeonId> dungeonId) -> std::expected<bool, StarException> {
        return serverWorld->get().placeDungeon(dungeonName, position, dungeonId, false);
      }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback("addBiomeRegion", [serverWorld](Vec2I position, String biomeName, String subBlockSelector, int width) {
          return serverWorld->get().addBiomeRegion(position, biomeName, subBlockSelector, width);
        }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("expandBiomeRegion", [serverWorld](Vec2I position, int width) {
          return serverWorld->get().expandBiomeRegion(position, width);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback("pregenerateAddBiome", [serverWorld](Vec2I position, int width) {
          return serverWorld->get().pregenerateAddBiome(position, width);
        }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("pregenerateExpandBiome", [serverWorld](Vec2I position, int width) {
          return serverWorld->get().pregenerateExpandBiome(position, width);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback("setLayerEnvironmentBiome", [serverWorld](Vec2I position) {
          return serverWorld->get().setLayerEnvironmentBiome(position);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback("setPlanetType", [serverWorld](String planetType, String primaryBiomeName) {
          return serverWorld->get().setPlanetType(planetType, primaryBiomeName);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback("setDungeonGravity", [serverWorld](DungeonId dungeonId, std::optional<float> gravity) {
          return serverWorld->get().setDungeonGravity(dungeonId, gravity);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback("setDungeonBreathable", [serverWorld](DungeonId dungeonId, std::optional<bool> breathable) {
          return serverWorld->get().setDungeonBreathable(dungeonId, breathable);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback("setDungeonId", [serverWorld](RectI tileRegion, DungeonId dungeonId) {
          return serverWorld->get().setDungeonId(tileRegion, dungeonId);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.register_callback("enqueuePlacement", [serverWorld](List<Json> distributionConfigs, std::optional<DungeonId> id) -> std::expected<RpcPromise<Vec2I>, StarException> {
        List<BiomeItemDistribution> distributions;
        distributions.reserve(distributionConfigs.size());
        for (auto const& config : distributionConfigs) {
          auto distributionResult = BiomeItemDistribution::create(serverWorld->get().root(), config, serverWorld->get().root().random().next_u64());
          if (!distributionResult)
            return std::unexpected(std::move(distributionResult.error()));
          auto distribution = std::move(*distributionResult);
          distributions.push_back(std::move(distribution));
        }
        return serverWorld->get().enqueuePlacement(std::move(distributions), id);
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("template", [serverWorld]() {
        return serverWorld->get().worldTemplate()->store();
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.register_callback("setTemplate", [serverWorld](Json worldTemplate) -> std::expected<void, StarException> {
        auto newTemplateResult = WorldTemplate::create(serverWorld->get().root(), worldTemplate);
        if (!newTemplateResult)
          return std::unexpected(std::move(newTemplateResult.error()));
        auto newTemplate = std::move(*newTemplateResult);
        return serverWorld->get().setTemplate(std::move(newTemplate));
      }); !result)
        return std::unexpected(std::move(result.error()));
    }

    return callbacks;
  }

  std::expected<void, StarException> addWorldDebugCallbacks(Root& root, LuaCallbacks& callbacks) {
    if (auto result = callbacks.register_callback_with_signature<void, Vec2F, Color>(
          "debugPoint",
          [&root](Vec2F const& position, Color const& color) {
            return WorldDebugCallbacks::debugPoint(root, position, color);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<void, Vec2F, Vec2F, Color>(
          "debugLine",
          [&root](Vec2F const& start, Vec2F const& end, Color const& color) {
            return WorldDebugCallbacks::debugLine(root, start, end, color);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<void, PolyF, Color>(
          "debugPoly",
          [&root](PolyF const& poly, Color const& color) { return WorldDebugCallbacks::debugPoly(root, poly, color); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<void, LuaEngine&, LuaVariadic<LuaValue>>(
          "debugText",
          [&root](LuaEngine& engine, LuaVariadic<LuaValue> const& args) {
            return WorldDebugCallbacks::debugText(root, engine, args);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }

  std::expected<void, StarException> addWorldEntityCallbacks(LuaCallbacks& callbacks, World& world) {
    // Compact, unsorted domain views. Selection, ordering, timers and bar
    // interpolation belong to the caller, not to the world service.
    auto damageBarView = [](DamageBarEntity const& entity, bool details) -> std::expected<Json, StarException> {
      if (!DamageBarTypeNames) return std::unexpected(DamageBarTypeNames.error());
      auto type = DamageBarTypeNames->get_right(entity.damageBar());
      if (!type) return std::unexpected(std::move(type.error()));
      JsonObject view;
      view.reserve(details ? 7 : 4);
      view.set("id", entity.entityId());
      view.set("type", type->get());
      view.set("position", json_from_vec2f(entity.position()));
      auto team = entity.getTeam().to_json();
      if (!team) return std::unexpected(std::move(team.error()));
      view.set("team", std::move(*team));
      if (details) {
        auto maximum = entity.maxHealth();
        if (!maximum) return std::unexpected(std::move(maximum.error()));
        auto name = entity.name();
        if (!name) return std::unexpected(std::move(name.error()));
        view.set("health", entity.health());
        view.set("maxHealth", *maximum);
        view.set("name", std::move(*name));
      }
      return view;
    };
    if (auto result = callbacks.register_callback("entityDamageBar", [&world, damageBarView](EntityId id) -> std::expected<Json, StarException> {
      if (auto entity = world.get<DamageBarEntity>(id))
        return damageBarView(*entity, true);
      return Json();
    }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback("damageBarEntities",
        [&world, damageBarView](std::optional<RectF> region, std::optional<String> typeName) -> std::expected<Json, StarException> {
      std::optional<DamageBarType> type;
      if (typeName) {
        if (!DamageBarTypeNames) return std::unexpected(DamageBarTypeNames.error());
        auto parsed = DamageBarTypeNames->get_left(*typeName);
        if (!parsed) return std::unexpected(std::move(parsed.error()));
        type = parsed->get();
      }
      JsonArray views;
      auto append = [&](std::shared_ptr<Entity> const& value) -> std::expected<void, StarException> {
        if (auto entity = as<DamageBarEntity>(value); entity && (!type || entity->damageBar() == *type)) {
          auto view = damageBarView(*entity, false);
          if (!view) return std::unexpected(std::move(view.error()));
          views.emplace_back(std::move(*view));
        }
        return {};
      };
      if (region) {
        auto entities = world.query<DamageBarEntity>(*region);
        if (!entities) return std::unexpected(std::move(entities.error()));
        views.reserve(entities->size());
        for (auto const& entity : *entities)
          if (auto added = append(entity); !added) return std::unexpected(std::move(added.error()));
      } else {
        auto collected = world.forAllEntities(append);
        if (!collected) return std::unexpected(std::move(collected.error()));
      }
      return views;
    }); !result)
      return std::unexpected(std::move(result.error()));

    // A one-entry cache bounds memory independently of the number of queried
    // entities. Animation still comes from the real portrait provider each
    // call; unchanged drawables do not cross Lua or repeat cropped-bound work.
    struct PortraitViewCache {
      std::weak_ptr<PortraitEntity> entity;
      String mode;
      List<Drawable> drawables;
      RectF bounds;
      std::uint64_t revision{};
    };
    auto portraitCache = std::make_shared<PortraitViewCache>();
    if (auto result = callbacks.register_callback("entityPortraitView",
        [&world, portraitCache](LuaEngine& engine, EntityId id, String const& modeName,
            std::optional<std::uint64_t> knownRevision) -> std::expected<std::optional<LuaTable>, StarException> {
      auto entity = world.get<PortraitEntity>(id);
      if (!entity) return {};
      auto mode = enum_value<portrait_mode_names>(modeName);
      if (!mode) return std::unexpected(std::move(mode.error()));
      auto portrait = entity->portrait(*mode);
      if (!portrait) return std::unexpected(std::move(portrait.error()));
      auto& cache = *portraitCache;
      if (cache.entity.lock() != entity || cache.mode != modeName || cache.drawables != *portrait) {
        auto metadata = world.root().image_metadata_database();
        if (!metadata) return std::unexpected(std::move(metadata.error()));
        auto bounds = Drawable::bound_box_all(**metadata, *portrait, true);
        if (!bounds) return std::unexpected(std::move(bounds.error()));
        cache.entity = entity;
        cache.mode = modeName;
        cache.drawables = std::move(*portrait);
        cache.bounds = *bounds;
        ++cache.revision;
      }
      auto view = engine.create_table(0, 3);
      if (!view) return std::unexpected(std::move(view.error()));
      if (auto set = view->set("revision", cache.revision); !set) return std::unexpected(std::move(set.error()));
      if (!knownRevision || *knownRevision != cache.revision) {
        if (auto set = view->set("drawables", cache.drawables); !set) return std::unexpected(std::move(set.error()));
        if (auto set = view->set("bounds", cache.bounds); !set) return std::unexpected(std::move(set.error()));
      }
      return std::move(*view);
    }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result =
          callbacks.register_callback_with_signature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "entityQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::entityQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "monsterQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::monsterQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "npcQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::npcQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "objectQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::objectQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "itemDropQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::itemDropQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "playerQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::playerQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "loungeableQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::loungeableQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<LuaTable, LuaEngine&, Vec2F, Vec2F, std::optional<LuaTable>>(
            "entityLineQuery",
            [&world](LuaEngine& engine, Vec2F const& start, Vec2F const& end, std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::entityLineQuery(world, engine, start, end, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<LuaTable, LuaEngine&, Vec2F, Vec2F, std::optional<LuaTable>>(
            "objectLineQuery",
            [&world](LuaEngine& engine, Vec2F const& start, Vec2F const& end, std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::objectLineQuery(world, engine, start, end, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<LuaTable, LuaEngine&, Vec2F, Vec2F, std::optional<LuaTable>>(
            "npcLineQuery",
            [&world](LuaEngine& engine, Vec2F const& start, Vec2F const& end, std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::npcLineQuery(world, engine, start, end, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback("objectAt",
        [&world](Vec2I const& tilePosition) -> std::expected<std::optional<int>, StarException> {
          auto objectResult = world.findEntityAtTile(tilePosition, [](std::shared_ptr<TileEntity> const& entity) -> std::expected<bool, StarException> { return is<Object>(entity); });
          if (!objectResult)
            return std::unexpected(std::move(objectResult.error()));
          auto object = std::move(*objectResult);
          if (object)
            return object->entityId();
          return {};
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback_with_signature<std::optional<std::shared_ptr<Entity>>, EntityId>("entity", [&world](EntityId entityId) -> std::optional<std::shared_ptr<Entity>> {
      if (auto entity = world.entity(entityId)) {
        return entity;
      } else {
        return {};
      }
    }); !result)
      return std::unexpected(std::move(result.error()));
    
    if (auto result = callbacks.register_callback("entities", [&world](LuaEngine& engine, std::optional<LuaTable> options) -> std::expected<LuaTable, StarException> {
      if (!options) {
        auto table = engine.create_table();
        if (!table)
          return std::unexpected(std::move(table.error()));
        options = std::move(*table);
      }
      return entityQueryImpl<Entity>(world, engine, *options, {});
    }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback_with_signature<bool, int>(
          "entityExists", [&world](int entity) { return WorldEntityCallbacks::entityExists(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<Json>, EntityId>(
          "entitySnapshot",
          [&world](EntityId entity) { return WorldEntityCallbacks::entitySnapshot(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, int, int>(
          "entityCanDamage",
          [&world](int source, int target) { return WorldEntityCallbacks::entityCanDamage(world, source, target); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId>(
          "entityDamageTeam",
          [&world](EntityId entity) { return WorldEntityCallbacks::entityDamageTeam(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, EntityId>(
          "entityAggressive",
          [&world](EntityId entity) { return WorldEntityCallbacks::entityAggressive(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<LuaString>, LuaEngine&, int>(
          "entityType",
          [&world](LuaEngine& engine, int entity) { return WorldEntityCallbacks::entity_type(world, engine, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<Vec2F>, int>(
          "entityPosition", [&world](int entity) { return WorldEntityCallbacks::entityPosition(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<Vec2F>, int>(
          "entityVelocity", [&world](int entity) { return WorldEntityCallbacks::entityVelocity(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<RectF>, int>(
          "entityMetaBoundBox",
          [&world](int entity) { return WorldEntityCallbacks::entityMetaBoundBox(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<std::uint64_t>, EntityId, String>(
          "entityCurrency",
          [&world](EntityId entity, String const& currency) {
            return WorldEntityCallbacks::entityCurrency(world, entity, currency);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<std::optional<std::uint64_t>, EntityId, Json, std::optional<bool>>(
            "entityHasCountOfItem",
            [&world](EntityId entity, Json&& descriptor, std::optional<bool> exactMatch) {
              return WorldEntityCallbacks::entityHasCountOfItem(world, entity, std::move(descriptor), exactMatch);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<Vec2F>, EntityId>(
          "entityHealth", [&world](EntityId entity) { return WorldEntityCallbacks::entityHealth(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<String>, EntityId>(
          "entitySpecies", [&world](EntityId entity) { return WorldEntityCallbacks::entitySpecies(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<String>, EntityId>(
          "entityGender", [&world](EntityId entity) { return WorldEntityCallbacks::entityGender(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<String>, EntityId>(
          "entityName", [&world](EntityId entity) { return WorldEntityCallbacks::entityName(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<Json>, EntityId>(
          "entityNametag", [&world](EntityId entity) { return WorldEntityCallbacks::entityNametag(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<String>, EntityId, std::optional<String>>(
          "entityDescription",
          [&world](EntityId entity, std::optional<String> const& species) {
            return WorldEntityCallbacks::entityDescription(world, entity, species);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<List<Drawable>>, EntityId, String>(
          "entityPortrait",
          [&world](EntityId entity, String const& mode) {
            return WorldEntityCallbacks::entityPortrait(world, entity, mode);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<String>, EntityId, String>(
          "entityHandItem",
          [&world](EntityId entity, String const& hand) {
            return WorldEntityCallbacks::entityHandItem(world, entity, hand);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, String>(
          "entityHandItemDescriptor",
          [&world](EntityId entity, String const& hand) {
            return WorldEntityCallbacks::entityHandItemDescriptor(world, entity, hand);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<String>, EntityId>(
          "entityUniqueId", [&world](EntityId entity) { return WorldEntityCallbacks::entityUniqueId(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, String, std::optional<Json>>(
          "getObjectParameter",
          [&world](EntityId entity, String const& name, std::optional<Json> const& def) {
            return WorldEntityCallbacks::getObjectParameter(world, entity, name, def);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, String, std::optional<Json>>(
          "getNpcScriptParameter",
          [&world](EntityId entity, String const& name, std::optional<Json> const& def) {
            return WorldEntityCallbacks::getNpcScriptParameter(world, entity, name, def);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<List<Vec2I>, EntityId>(
          "objectSpaces", [&world](EntityId entity) { return WorldEntityCallbacks::objectSpaces(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<int>, EntityId>(
          "farmableStage", [&world](EntityId entity) { return WorldEntityCallbacks::farmableStage(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<int>, EntityId>(
          "containerSize", [&world](EntityId entity) { return WorldEntityCallbacks::containerSize(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, EntityId>(
          "containerClose", [&world](EntityId entity) { return WorldEntityCallbacks::containerClose(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, EntityId>(
          "containerOpen", [&world](EntityId entity) { return WorldEntityCallbacks::containerOpen(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId>(
          "containerItems", [&world](EntityId entity) { return WorldEntityCallbacks::containerItems(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, std::size_t>(
          "containerItemAt",
          [&world](EntityId entity, std::size_t offset) {
            return WorldEntityCallbacks::containerItemAt(world, entity, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<bool>, EntityId, Json>(
          "containerConsume",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerConsume(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<bool>, EntityId, std::size_t, int>(
          "containerConsumeAt",
          [&world](EntityId entity, std::size_t offset, int count) {
            return WorldEntityCallbacks::containerConsumeAt(world, entity, offset, count);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<std::size_t>, EntityId, Json>(
          "containerAvailable",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerAvailable(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId>(
          "containerTakeAll",
          [&world](EntityId entity) { return WorldEntityCallbacks::containerTakeAll(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, std::size_t>(
          "containerTakeAt",
          [&world](EntityId entity, std::size_t offset) {
            return WorldEntityCallbacks::containerTakeAt(world, entity, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, std::size_t, int>(
          "containerTakeNumItemsAt",
          [&world](EntityId entity, std::size_t offset, int count) {
            return WorldEntityCallbacks::containerTakeNumItemsAt(world, entity, offset, count);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<std::size_t>, EntityId, Json>(
          "containerItemsCanFit",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerItemsCanFit(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, Json>(
          "containerItemsFitWhere",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerItemsFitWhere(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, Json>(
          "containerAddItems",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerAddItems(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, Json>(
          "containerStackItems",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerStackItems(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, Json, std::size_t>(
          "containerPutItemsAt",
          [&world](EntityId entity, Json const& items, std::size_t offset) {
            return WorldEntityCallbacks::containerPutItemsAt(world, entity, items, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, Json, std::size_t>(
          "containerSwapItems",
          [&world](EntityId entity, Json const& items, std::size_t offset) {
            return WorldEntityCallbacks::containerSwapItems(world, entity, items, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, Json, std::size_t>(
          "containerSwapItemsNoCombine",
          [&world](EntityId entity, Json const& items, std::size_t offset) {
            return WorldEntityCallbacks::containerSwapItemsNoCombine(world, entity, items, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Json, EntityId, Json, std::size_t>(
          "containerItemApply",
          [&world](EntityId entity, Json const& items, std::size_t offset) {
            return WorldEntityCallbacks::containerItemApply(world, entity, items, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<std::optional<LuaValue>, EntityId, String, LuaVariadic<LuaValue>>(
            "callScriptedEntity",
            [&world](EntityId entity, String const& function, LuaVariadic<LuaValue> const& args) {
              return WorldEntityCallbacks::callScriptedEntity(world, entity, function, args);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<RpcPromise<Vec2F>, String>(
          "findUniqueEntity",
          [&world](String const& uniqueId) { return WorldEntityCallbacks::findUniqueEntity(world, uniqueId); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<RpcPromise<Json>, LuaEngine&, LuaValue, String, LuaVariadic<Json>>(
            "sendEntityMessage",
            [&world](LuaEngine& engine, LuaValue&& entity, String const& message, LuaVariadic<Json>&& args) {
              return WorldEntityCallbacks::send_entity_message(world, engine, std::move(entity), message,
                                                             std::move(args));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<std::optional<List<EntityId>>, EntityId, std::optional<std::size_t>>(
            "loungingEntities",
            [&world](EntityId entity, std::optional<std::size_t> anchor) {
              return WorldEntityCallbacks::loungingEntities(world, entity, anchor);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<std::optional<bool>, EntityId, std::optional<std::size_t>>(
            "loungeableOccupied",
            [&world](EntityId entity, std::optional<std::size_t> anchor) {
              return WorldEntityCallbacks::loungeableOccupied(world, entity, anchor);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<std::size_t>, EntityId>(
          "loungeableAnchorCount",
          [&world](EntityId entity) { return WorldEntityCallbacks::loungeableAnchorCount(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, EntityId, std::optional<bool>>(
          "isMonster",
          [&world](EntityId entity, std::optional<bool> const& aggressive) {
            return WorldEntityCallbacks::isMonster(world, entity, aggressive);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<String>, EntityId>(
          "monsterType", [&world](EntityId entity) { return WorldEntityCallbacks::monsterType(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<String>, EntityId>(
          "npcType", [&world](EntityId entity) { return WorldEntityCallbacks::npcType(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<String>, EntityId>(
          "stagehandType", [&world](EntityId entity) { return WorldEntityCallbacks::stagehandType(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, EntityId, std::optional<int>>(
          "isNpc",
          [&world](EntityId entity, std::optional<int> const& team) {
            return WorldEntityCallbacks::isNpc(world, entity, team);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback("isEntityInteractive", [&world](EntityId entityId) -> std::optional<bool> {
        if (auto entity = world.get<InteractiveEntity>(entityId))
          return entity->isInteractive();
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback("entityAimPosition", [&world](EntityId entityId) -> std::expected<std::optional<Vec2F>, StarException> {
        if (auto entity = world.get<ToolUserEntity>(entityId)) {
          auto position = entity->aimPosition();
          if (!position)
            return std::unexpected(std::move(position.error()));
          return *position;
        }
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback("entityMouthPosition", [&world](EntityId entityId) -> std::expected<std::optional<Vec2F>, StarException> {
        if (auto entity = world.get<ChattyEntity>(entityId)) {
          auto positionResult = entity->mouth_position();
          if (!positionResult)
            return std::unexpected(std::move(positionResult.error()));
          auto position = std::move(*positionResult);
          return position;
        }
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback("entityTypeName", [&world](EntityId entityId) -> std::expected<std::optional<String>, StarException> {
        auto entity = world.entity(entityId);
        if (auto monster = as<Monster>(entity))
          return monster->typeName();
        if (auto npc = as<Npc>(entity))
          return npc->npcType();
        if (auto vehicle = as<Vehicle>(entity))
          return vehicle->name();
        if (auto object = as<Object>(entity))
          return object->name();
        if (auto itemDrop = as<ItemDrop>(entity)) {
          if (itemDrop->item())
            return itemDrop->item()->name();
        }
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback("itemDropItem", [&world](EntityId const& entityId) -> Json {
        if (auto itemDrop = world.get<ItemDrop>(entityId))
          return itemDrop->item()->descriptor().to_json();
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }

  std::expected<void, StarException> addWorldEnvironmentCallbacks(LuaCallbacks& callbacks, World& world) {
    if (auto result = callbacks.register_callback_with_signature<float, Vec2F>(
          "lightLevel",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::lightLevel(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<float, Vec2F>(
          "windLevel",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::windLevel(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, Vec2F>(
          "breathable",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::breathable(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, Vec2F>(
          "underground",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::underground(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<LuaValue, LuaEngine&, Vec2F, String>(
          "material",
          [&world](LuaEngine& engine, Vec2F const& position, String const& layer) {
            return WorldEnvironmentCallbacks::material(world, engine, position, layer);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<LuaValue, LuaEngine&, Vec2F, String>(
          "mod",
          [&world](LuaEngine& engine, Vec2F const& position, String const& layer) {
            return WorldEnvironmentCallbacks::mod(world, engine, position, layer);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<float, Vec2F, String>(
          "materialHueShift",
          [&world](Vec2F const& position, String const& layer) {
            return WorldEnvironmentCallbacks::materialHueShift(world, position, layer);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<float, Vec2F, String>(
          "modHueShift",
          [&world](Vec2F const& position, String const& layer) {
            return WorldEnvironmentCallbacks::modHueShift(world, position, layer);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<MaterialColorVariant, Vec2F, String>(
          "materialColor",
          [&world](Vec2F const& position, String const& layer) {
            return WorldEnvironmentCallbacks::materialColor(world, position, layer);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<void, Vec2F, String, MaterialColorVariant>(
          "setMaterialColor",
          [&world](Vec2F const& position, String const& layer, MaterialColorVariant color) {
            return WorldEnvironmentCallbacks::setMaterialColor(world, position, layer, color);
          });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback("oceanLevel", [&world](Vec2I position) -> std::expected<int, StarException> {
        std::shared_ptr<WorldTemplate const> worldTemplate;
        if (auto server = as_ref<WorldServer>(world))
          worldTemplate = server->get().worldTemplate();
        else {
          auto client = convert_ref<WorldClient>(world);
          if (!client)
            return std::unexpected(std::move(client.error()));
          worldTemplate = client->get().currentTemplate();
        }
        auto blockResult = worldTemplate->blockInfo(position[0], position[1]);
        if (!blockResult)
          return std::unexpected(std::move(blockResult.error()));
        auto block = std::move(*blockResult);
        return block.oceanLiquidLevel;
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback("environmentStatusEffects", [&world](Vec2F const& position) {
        return world.environmentStatusEffects(position);
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback("weatherStatusEffects", [&world](Vec2F const& position) {
        return world.weatherStatusEffects(position);
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback("exposedToWeather", [&world](Vec2F const& position) {
        return world.exposedToWeather(position);
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback("activeWeather", [&world](Vec2F const& position) {
        return world.activeWeather(position);
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback_with_signature<bool, List<Vec2I>, String, Vec2F, String, float,
                                                              std::optional<unsigned>, std::optional<EntityId>>(
          "damageTiles",
          [&world](List<Vec2I> const& positions, String const& layer, Vec2F const& source, String const& type,
                   float damage, std::optional<unsigned> const& harvestLevel, std::optional<EntityId> sourceEntity) {
            return WorldEnvironmentCallbacks::damageTiles(world, positions, layer, source, type, damage, harvestLevel,
                                                          sourceEntity);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, Vec2F, float, String, Vec2F, String, float,
                                                              std::optional<unsigned>, std::optional<EntityId>>(
          "damageTileArea",
          [&world](Vec2F center, float radius, String&& layer, Vec2F source, String&& type, float damage,
                   std::optional<unsigned> const& harvestLevel, std::optional<EntityId> sourceEntity) {
            return WorldEnvironmentCallbacks::damageTileArea(world, center, radius, std::move(layer), source,
                                                             std::move(type), damage, harvestLevel, sourceEntity);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, Vec2I, String, String, std::optional<int>, bool>(
          "placeMaterial",
          [&world](Vec2I const& position, String const& layer, String const& material, std::optional<int> const& hue,
                   bool allowOverlap) {
            return WorldEnvironmentCallbacks::placeMaterial(world, position, layer, material, hue, allowOverlap);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<bool, List<Vec2I>, String, String, std::optional<int>, bool>(
            "replaceMaterials",
            [&world](List<Vec2I> const& positions, String const& layer, String const& material,
                     std::optional<int> const& hue, bool enableDrops) {
              return WorldEnvironmentCallbacks::replaceMaterials(world, positions, layer, material, hue, enableDrops);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<bool, Vec2F, float, String, String, std::optional<int>, bool>(
            "replaceMaterialArea",
            [&world](Vec2F center, float radius, String const& layer, String const& material,
                     std::optional<int> const& hue, bool enableDrops) {
              return WorldEnvironmentCallbacks::replaceMaterialArea(world, center, radius, layer, material, hue,
                                                                    enableDrops);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, Vec2I, String, String, std::optional<int>, bool>(
          "placeMod",
          [&world](Vec2I const& position, String const& layer, String const& mod, std::optional<int> const& hue,
                   bool allowOverlap) {
            return WorldEnvironmentCallbacks::placeMod(world, position, layer, mod, hue, allowOverlap);
          });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback(
          "radialTileQuery",
          [&world](Vec2F center, float radius, String layerName) -> std::expected<List<Vec2I>, StarException> {
            auto layerPointerResult = enum_value<tile_layer_names>(layerName);
            if (!layerPointerResult)
              return std::unexpected(std::move(layerPointerResult.error()));
            auto layerPointer = std::move(*layerPointerResult);
            auto layer = layerPointer;
            List<Vec2I> tiles;
            for (auto const& tile : tile_area_brush(radius, center, false)) {
              if (world.material(tile, layer) != empty_material_id)
                tiles.push_back(tile);
            }
            return tiles;
          });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.register_callback_with_signature<List<std::pair<Vec2I, LiquidLevel>>, Vec2F, Vec2F>(
          "liquidAlongLine",
          [&world](Vec2F const& start, Vec2F const& end) {
            return WorldEnvironmentCallbacks::liquid_along_line(world, start, end);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, Vec2I, std::optional<bool>, std::optional<bool>>(
          "tileIsOccupied",
          [&world](Vec2I const& position, std::optional<bool> const& foreground,
                   std::optional<bool> const& background) {
            return WorldEnvironmentCallbacks::tileIsOccupied(world, position, foreground, background);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<float>(
          "threatLevel", [&world]() -> decltype(auto) { return world.threatLevel(); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<double>(
          "time", [&world]() { return WorldEnvironmentCallbacks::time(world); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::uint64_t>(
          "day", [&world]() { return WorldEnvironmentCallbacks::day(world); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<double>(
          "timeOfDay", [&world]() { return WorldEnvironmentCallbacks::timeOfDay(world); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<float>(
          "dayLength", [&world]() { return WorldEnvironmentCallbacks::dayLength(world); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<LiquidLevel>, std::variant<RectF, Vec2I>>(
          "liquidAt",
          [&world](std::variant<RectF, Vec2I>&& bound) {
            return WorldEnvironmentCallbacks::liquidAt(world, std::move(bound));
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<float, Vec2F>(
          "gravity", [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::gravity(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, Vec2F, LiquidId, float>(
          "spawnLiquid",
          [&world](Vec2F const& position, LiquidId liquid, float amount) {
            return WorldEnvironmentCallbacks::spawnLiquid(world, position, liquid, amount);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<LiquidLevel>, Vec2F>(
          "destroyLiquid",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::destroyLiquid(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, Vec2F>(
          "isTileProtected",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::isTileProtected(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  
  std::expected<void, StarException> addWorldGeometryCallbacks(LuaCallbacks& callbacks, World& world) {

    if (auto result = callbacks.register_callback("size", [&world]() -> Vec2U {
        return world.geometry().size();
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<float, Vec2F, std::optional<Vec2F>>(
          "magnitude",
          [&world](Vec2F position, std::optional<Vec2F> other) {
            return WorldGeometryCallbacks::magnitude(world, position, other);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<Vec2F, Vec2F, Vec2F>(
          "distance",
          [&world](Vec2F const& start, Vec2F const& end) {
            return WorldGeometryCallbacks::distance(world, start, end);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, PolyF, Vec2F>(
          "polyContains",
          [&world](PolyF const& poly, Vec2F const& position) {
            return WorldGeometryCallbacks::polyContains(world, poly, position);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<LuaValue, LuaEngine&, LuaValue>(
          "xwrap",
          [&world](LuaEngine& engine, LuaValue const& position) {
            return WorldGeometryCallbacks::xwrap(world, engine, position);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<LuaValue, LuaEngine&, std::variant<Vec2F, float>,
                                                              std::variant<Vec2F, float>>(
          "nearestTo",
          [&world](LuaEngine& engine, std::variant<Vec2F, float> const& source,
                   std::variant<Vec2F, float> const& target) {
            return WorldGeometryCallbacks::nearestTo(world, engine, source, target);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  
  std::expected<void, StarException> addWorldCollisionCallbacks(LuaCallbacks& callbacks, World& world) {
    if (auto result = callbacks.register_callback_with_signature<bool, RectF, std::optional<CollisionSet>>(
          "rectCollision",
          [&world](RectF const& rect, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::rectCollision(world, rect, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, Vec2F, std::optional<CollisionSet>>(
          "pointTileCollision",
          [&world](Vec2F const& point, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::pointTileCollision(world, point, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, Vec2F, Vec2F, std::optional<CollisionSet>>(
          "lineTileCollision",
          [&world](Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::line_tile_collision(world, start, end, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<std::pair<Vec2F, Vec2I>>, Vec2F, Vec2F,
                                                              std::optional<CollisionSet>>(
          "lineTileCollisionPoint",
          [&world](Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::line_tile_collision_point(world, start, end, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, RectF, std::optional<CollisionSet>>(
          "rectTileCollision",
          [&world](RectF const& rect, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::rectTileCollision(world, rect, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<bool, Vec2F, std::optional<CollisionSet>>(
          "pointCollision",
          [&world](Vec2F const& point, std::optional<CollisionSet> const& kinds) {
            return world.point_collision(point, kinds.value_or(DefaultCollisionSet));
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<LuaTupleReturn<std::optional<Vec2F>, std::optional<Vec2F>>, Vec2F,
                                                  Vec2F, std::optional<CollisionSet>>(
            "lineCollision",
            [&world](Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& kinds) {
              return WorldCollisionCallbacks::line_collision(world, start, end, kinds);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.register_callback_with_signature<bool, PolyF, std::optional<Vec2F>, std::optional<CollisionSet>>(
            "polyCollision",
            [&world](PolyF const& poly, std::optional<Vec2F> const& position,
                     std::optional<CollisionSet> const& kinds) {
              return WorldCollisionCallbacks::poly_collision(world, poly, position, kinds);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks
            .register_callback_with_signature<List<Vec2I>, Vec2F, Vec2F, std::optional<CollisionSet>, std::optional<int>>(
              "collisionBlocksAlongLine",
              [&world](Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& kinds,
                       std::optional<int> const& limit) {
                return WorldCollisionCallbacks::collision_blocks_along_line(world, start, end, kinds, limit);
              });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.register_callback_with_signature<std::optional<Vec2F>, PolyF, Vec2F, float,
                                                              std::optional<CollisionSet>>(
          "resolvePolyCollision",
          [&world](PolyF&& poly, Vec2F const& position, float correction, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::resolvePolyCollision(world, std::move(poly), position, correction, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }

  std::expected<float, StarException> WorldGeometryCallbacks::magnitude(World& world, Vec2F pos1, std::optional<Vec2F> pos2) {
    if (pos2)
      return world.geometry().diff(pos1, *pos2).magnitude();
    else
      return pos1.magnitude();
  }

  std::expected<Vec2F, StarException> WorldGeometryCallbacks::distance(World& world, Vec2F const& arg1, Vec2F const& arg2) {
    return world.geometry().diff(arg1, arg2);
  }

  std::expected<bool, StarException> WorldGeometryCallbacks::polyContains(World& world, PolyF const& poly, Vec2F const& pos) {
    return world.geometry().poly_contains(poly, pos);
  }

  std::expected<LuaValue, StarException> WorldGeometryCallbacks::xwrap(World& world, LuaEngine& engine, LuaValue const& positionOrX) {
    auto xResult = engine.lua_maybe_to<float>(positionOrX);
    if (!xResult)
      return std::unexpected(std::move(xResult.error()));
    auto x = std::move(*xResult);
    if (x)
      return LuaFloat(world.geometry().xwrap(*x));
    auto positionResult = engine.lua_to<Vec2F>(positionOrX);
    if (!positionResult)
      return std::unexpected(std::move(positionResult.error()));
    auto position = std::move(*positionResult);
    return engine.lua_from<Vec2F>(world.geometry().xwrap(position));
  }

  std::expected<LuaValue, StarException> WorldGeometryCallbacks::nearestTo(World& world, LuaEngine& engine, std::variant<Vec2F, float> const& sourcePositionOrX, std::variant<Vec2F, float> const& targetPositionOrX) {
    if (std::holds_alternative<Vec2F>(targetPositionOrX)) {
      Vec2F targetPosition = std::get<Vec2F>(targetPositionOrX);
      Vec2F sourcePosition;
      if (std::holds_alternative<Vec2F>(sourcePositionOrX))
        sourcePosition = std::get<Vec2F>(sourcePositionOrX);
      else
        sourcePosition[0] = std::get<float>(sourcePositionOrX);

      return engine.lua_from<Vec2F>(world.geometry().nearest_to(sourcePosition, targetPosition));

    } else {
      float targetX = std::get<float>(targetPositionOrX);
      float sourceX;
      if (std::holds_alternative<Vec2F>(sourcePositionOrX))
        sourceX = std::get<Vec2F>(sourcePositionOrX)[0];
      else
        sourceX = std::get<float>(sourcePositionOrX);

      return LuaFloat(world.geometry().nearest_to(sourceX, targetX));
    }
  }

  std::expected<bool, StarException> WorldCollisionCallbacks::rectCollision(World& world, RectF const& arg1, std::optional<CollisionSet> const& arg2) {
    PolyF body = PolyF(arg1);

    return world.poly_collision(body, arg2.value_or(DefaultCollisionSet));
  }

  std::expected<bool, StarException> WorldCollisionCallbacks::pointTileCollision(World& world, Vec2F const& arg1, std::optional<CollisionSet> const& arg2) {
    if (arg2)
      return world.pointTileCollision(arg1, *arg2);
    else
      return world.pointTileCollision(arg1);
  }

  std::expected<bool, StarException> WorldCollisionCallbacks::line_tile_collision(
      World& world, Vec2F const& begin, Vec2F const& end, std::optional<CollisionSet> const& collision_set) {
    return world.lineTileCollision(begin, end, collision_set ? *collision_set : DefaultCollisionSet);
  }

  std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> WorldCollisionCallbacks::line_tile_collision_point(
      World& world, Vec2F const& begin, Vec2F const& end, std::optional<CollisionSet> const& collision_set) {
    return world.lineTileCollisionPoint(begin, end, collision_set ? *collision_set : DefaultCollisionSet);
  }

  std::expected<bool, StarException> WorldCollisionCallbacks::rectTileCollision(World& world, RectF const& arg1, std::optional<CollisionSet> const& arg2) {
    RectI const region = RectI::integral(arg1);

    if (arg2)
      return world.rectTileCollision(region, *arg2);
    else
      return world.rectTileCollision(region);
  }

  std::expected<LuaTupleReturn<std::optional<Vec2F>, std::optional<Vec2F>>, StarException> WorldCollisionCallbacks::line_collision(
      World& world, Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& kinds) {
    auto hit = world.line_collision(Line2F(start, end), kinds.value_or(DefaultCollisionSet));
    if (!hit)
      return std::unexpected(std::move(hit.error()));
    if (*hit)
      return lua_tuple_return(std::optional<Vec2F>((*hit)->first), (*hit)->second);
    return lua_tuple_return(std::optional<Vec2F>{}, std::optional<Vec2F>{});
  }

  std::expected<bool, StarException> WorldCollisionCallbacks::poly_collision(
      World& world, PolyF const& poly, std::optional<Vec2F> const& offset, std::optional<CollisionSet> const& kinds) {
    auto collision_set = kinds.value_or(DefaultCollisionSet);
    if (!offset)
      return world.poly_collision(poly, collision_set);
    PolyF translated = poly;
    translated.translate(*offset);
    return world.poly_collision(translated, collision_set);
  }

  std::expected<List<Vec2I>, StarException> WorldCollisionCallbacks::collision_blocks_along_line(
      World& world, Vec2F const& begin, Vec2F const& end, std::optional<CollisionSet> const& collision_set, std::optional<int> const& max_size) {
    return world.collidingTilesAlongLine(begin, end, collision_set ? *collision_set : DefaultCollisionSet, max_size.value_or(-1));
  }

  std::expected<std::optional<Vec2F>, StarException> WorldCollisionCallbacks::resolvePolyCollision(
      World& world, PolyF poly, Vec2F const& position, float maximumCorrection, std::optional<CollisionSet> const& maybeCollisionSet) {
    struct CollisionPoly {
      PolyF poly;
      Vec2F center;
      float sortingDistance;
    };

    poly.translate(position);
    List<CollisionPoly> collisions;
    CollisionSet collisionSet = maybeCollisionSet.value_or(DefaultCollisionSet);
    auto blocksResult = world.for_each_collision_block(RectI::integral(poly.bounding_box().padded(maximumCorrection + 1.0f)),
        [&](auto const& block) -> std::expected<void, StarException> {
          if (collisionSet.contains(block.kind))
            collisions.push_back({block.poly, Vec2F(block.space), 0.0f});
          return {};
        });
    if (!blocksResult)
      return std::unexpected(std::move(blocksResult.error()));

    auto resolveCollision = [&](std::optional<Vec2F> direction, float maximumDistance, int loops) -> std::optional<Vec2F> {
      PolyF body = poly;
      Vec2F correction;
      for (int i = 0; i < loops; ++i) {
        Vec2F bodyCenter = body.center();
        for (auto& cp : collisions)
          cp.sortingDistance = vmag_squared(bodyCenter - cp.center);
        std::sort(collisions.begin(), collisions.end(), [](auto const& a, auto const& b) { return a.sortingDistance < b.sortingDistance; });

        bool anyIntersects = false;
        for (auto const& cp : collisions) {
          PolyF::IntersectResult intersection;
          if (direction)
            intersection = body.directional_sat_intersection(cp.poly, *direction, false);
          else
            intersection = body.sat_intersection(cp.poly);

          if (intersection.intersects) {
            anyIntersects = true;
            body.translate(intersection.overlap);
            correction += intersection.overlap;
            if (vmag(correction) > maximumDistance)
              return {};
          }
        }

        if (!anyIntersects)
          return correction;
      }

      for (auto const& cp : collisions) {
        if (body.intersects(cp.poly))
          return {};
      }

      return correction;
    };

    // First try any-directional SAT separation for two loops
    if (auto resolution = resolveCollision({}, maximumCorrection, 2))
      return position + *resolution;

    // Then, try direction-limiting SAT in cardinals, then 45 degs, then in
    // between, for 16 total angles in a circle.
    for (int i : {4, 8, 12, 0, 2, 6, 10, 14, 1, 3, 7, 5, 15, 13, 9, 11}) {
      float angle = i * std::numbers::pi / 8;
      Vec2F dir = Vec2F::with_angle(angle, 1.0f);
      if (auto resolution = resolveCollision(dir, maximumCorrection, 1))
        return position + *resolution;
    }

    return {};
  }

  std::expected<List<std::pair<Vec2I, LiquidLevel>>, StarException> WorldEnvironmentCallbacks::liquid_along_line(World& world, Vec2F const& start, Vec2F const& end) {
    List<std::pair<Vec2I, LiquidLevel>> levels;
    auto traversed = for_each_block_along_line(start, world.geometry().diff(end, start), [&world, &levels](int x, int y) -> std::expected<bool, StarException> {
      auto level = world.liquidLevel(RectF::with_size(Vec2F(x, y), Vec2F(1, 1)));
      if (!level)
        return std::unexpected(std::move(level.error()));
      if (level->liquid != empty_liquid_id)
        levels.emplace_back(Vec2I(x, y), std::move(*level));
      return true;
    });
    if (!traversed)
      return std::unexpected(std::move(traversed.error()));
    return levels;
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::tileIsOccupied(
      World& world, Vec2I const& arg1, std::optional<bool> const& arg2, std::optional<bool> const& arg3) {
    Vec2I const tile = arg1;
    bool const tileLayerBool = arg2.value_or(true);
    bool const includeEphemeral = arg3.value_or(false);

    TileLayer const tileLayer = tileLayerBool ? TileLayer::Foreground : TileLayer::Background;

    return world.tileIsOccupied(tile, tileLayer, includeEphemeral);
  }

  std::expected<bool, StarException> WorldCallbacks::placeObject(World& world,
      String const& objectType,
      Vec2I const& worldPosition,
      std::optional<int> const& objectDirection,
      Json const& objectParameters) {
    auto objectDatabaseResult = world.root().objectDatabase();
    if (!objectDatabaseResult)
      return std::unexpected(std::move(objectDatabaseResult.error()));
    auto objectDatabase = std::move(*objectDatabaseResult);
    auto spawn = [&]() -> std::expected<bool, StarException> {
      Direction direction = objectDirection && *objectDirection < 0 ? Direction::Left : Direction::Right;
      auto placedObjectResult = objectDatabase->createForPlacement(world, objectType, worldPosition, direction, objectParameters ? objectParameters : JsonObject());
      if (!placedObjectResult)
        return std::unexpected(std::move(placedObjectResult.error()));
      auto placedObject = std::move(*placedObjectResult);
      if (!placedObject)
        return false;
      if (auto operation = world.addEntity(placedObject); !operation)
        return std::unexpected(std::move(operation.error()));
      return true;
    };
    auto result = spawn();
    if (!result) {
      if (auto logged = world.root().logger().warn_exception(
            Star::format("Could not create placable object of kind '{}'", objectType),
            ExceptionDiagnostic(result.error()));
          !logged)
        return std::unexpected(std::move(logged.error()));
      return false;
    }
    return std::move(*result);
  }

  std::expected<std::optional<EntityId>, StarException> WorldCallbacks::spawnItem(World& world,
      Json const& itemType,
      Vec2F const& world_position,
      std::optional<std::size_t> const& inputCount,
      Json const& inputParameters,
      std::optional<Vec2F> const& initial_velocity,
      std::optional<float> const& intangibleTime) {
    bool emptyItem = false;
    auto spawn = [&]() -> std::expected<std::optional<EntityId>, StarException> {
      auto descriptor_result = itemType.is_type(Json::Type::String)
          ? itemType.to_string().and_then([&](String name) { return ItemDescriptor::create(std::move(name), inputCount.value_or(1), inputParameters ? inputParameters : JsonObject()); })
          : ItemDescriptor::create(itemType);
      if (!descriptor_result)
        return std::unexpected(std::move(descriptor_result.error()));
      auto item_drop_result = ItemDrop::create_randomized_drop(world.root(), *descriptor_result, world_position);
      if (!item_drop_result)
        return std::unexpected(std::move(item_drop_result.error()));
      auto item_drop = std::move(*item_drop_result);
      if (!item_drop) {
        emptyItem = true;
        return {};
      }
      if (initial_velocity)
        item_drop->set_velocity(*initial_velocity);
      if (intangibleTime)
        item_drop->set_intangible_time(*intangibleTime);
      if (auto operation = world.addEntity(item_drop); !operation)
        return std::unexpected(std::move(operation.error()));
      return item_drop->inWorld() ? std::optional<EntityId>(item_drop->entityId()) : std::nullopt;
    };
    auto result = spawn();
    if (emptyItem) {
      auto logged = world.root().logger().warn("Could not spawn item, item empty in WorldCallbacks::spawnItem");
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
    if (!result) {
      auto itemKindResult = itemType.repr();
      if (!itemKindResult)
        return std::unexpected(std::move(itemKindResult.error()));
      auto itemKind = std::move(*itemKindResult);
      if (auto logged = world.root().logger().warn_exception(
            Star::format("Could not spawn Item of kind '{}'", itemKind), ExceptionDiagnostic(result.error()));
          !logged)
        return std::unexpected(std::move(logged.error()));
      return {};
    }
    return std::move(*result);
  }

  std::expected<List<EntityId>, StarException> WorldCallbacks::spawnTreasure(
      World& world, Vec2F const& position, String const& pool, float level, std::optional<std::uint64_t> seed) {
    List<EntityId> entities;
    auto treasureDatabaseResult = world.root().treasureDatabase();
    if (!treasureDatabaseResult)
      return std::unexpected(std::move(treasureDatabaseResult.error()));
    auto treasureDatabase = std::move(*treasureDatabaseResult);
    auto spawn = [&]() -> std::expected<void, StarException> {
      auto treasureResult = treasureDatabase->createTreasure(pool, level, seed.value_or(world.root().random().next_u64()));
      if (!treasureResult)
        return std::unexpected(std::move(treasureResult.error()));
      auto treasure = std::move(*treasureResult);
      for (auto const& treasure_item : treasure) {
        auto entity_result = ItemDrop::create_randomized_drop(world.root(), treasure_item, position);
        if (!entity_result)
          return std::unexpected(std::move(entity_result.error()));
        auto entity = std::move(*entity_result);
        entities.push_back(entity->entityId());
        if (auto operation = world.addEntity(entity); !operation)
          return std::unexpected(std::move(operation.error()));
      }
      return {};
    };
    if (auto result = spawn(); !result) {
      if (auto operation = world.root().logger().warn_exception(
            Star::format("Could not spawn treasure from pool '{}'", pool), ExceptionDiagnostic(result.error()));
          !operation)
        return std::unexpected(std::move(operation.error()));
    }
    return entities;
  }

  std::expected<std::optional<EntityId>, StarException> WorldCallbacks::spawnMonster(
      World& world, String const& arg1, Vec2F const& arg2, std::optional<JsonObject> const& arg3) {
    auto monsterDatabaseResult = world.root().monsterDatabase();
    if (!monsterDatabaseResult)
      return std::unexpected(std::move(monsterDatabaseResult.error()));
    auto monsterDatabase = std::move(*monsterDatabaseResult);
    auto spawn = [&]() -> std::expected<std::optional<EntityId>, StarException> {
      JsonObject parameters;
      parameters["aggressive"] = world.root().random().next_bool();
      if (arg3)
        parameters.merge(*arg3, true);
      float level = 1;
      if (parameters.contains("level")) {
        auto levelJsonResult = parameters.get("level");
        if (!levelJsonResult)
          return std::unexpected(std::move(levelJsonResult.error()));
        auto levelJson = std::move(*levelJsonResult);
        auto parsedLevelResult = levelJson.get().to_float();
        if (!parsedLevelResult)
          return std::unexpected(std::move(parsedLevelResult.error()));
        auto parsedLevel = std::move(*parsedLevelResult);
        level = parsedLevel;
      }
      auto variantResult = monsterDatabase->randomMonster(arg1, parameters);
      if (!variantResult)
        return std::unexpected(std::move(variantResult.error()));
      auto variant = std::move(*variantResult);
      auto monsterResult = monsterDatabase->createMonster(variant, level);
      if (!monsterResult)
        return std::unexpected(std::move(monsterResult.error()));
      auto monster = std::move(*monsterResult);
      monster->setPosition(arg2);
      if (auto operation = world.addEntity(monster); !operation)
        return std::unexpected(std::move(operation.error()));
      return monster->inWorld() ? std::optional<EntityId>(monster->entityId()) : std::nullopt;
    };
    auto result = spawn();
    if (!result) {
      if (auto logged = world.root().logger().warn_exception(Star::format("Could not spawn Monster of type '{}'", arg1),
                                                             ExceptionDiagnostic(result.error()));
          !logged)
        return std::unexpected(std::move(logged.error()));
      return {};
    }
    return std::move(*result);
  }

  std::expected<std::optional<EntityId>, StarException> WorldCallbacks::spawnNpc(World& world,
      Vec2F const& arg1,
      String const& arg2,
      String const& arg3,
      float arg4,
      std::optional<std::uint64_t> arg5,
      Json const& arg6) {
    auto seed = arg5 ? *arg5 : world.root().random().next_u64();
    auto npcDatabaseResult = world.root().npcDatabase();
    if (!npcDatabaseResult)
      return std::unexpected(std::move(npcDatabaseResult.error()));
    auto npcDatabase = std::move(*npcDatabaseResult);
    auto spawn = [&]() -> std::expected<std::optional<EntityId>, StarException> {
      auto variantResult = npcDatabase->generateNpcVariant(arg2, arg3, arg4, seed, arg6 ? arg6 : JsonObject());
      if (!variantResult)
        return std::unexpected(std::move(variantResult.error()));
      auto variant = std::move(*variantResult);
      auto npcResult = npcDatabase->createNpc(variant);
      if (!npcResult)
        return std::unexpected(std::move(npcResult.error()));
      auto npc = std::move(*npcResult);
      npc->setPosition(arg1);
      if (auto operation = world.addEntity(npc); !operation)
        return std::unexpected(std::move(operation.error()));
      return npc->inWorld() ? std::optional<EntityId>(npc->entityId()) : std::nullopt;
    };
    auto result = spawn();
    if (!result) {
      if (auto logged = world.root().logger().warn_exception(
            Star::format("Could not spawn NPC of species '{}' and type '{}'", arg2, arg3),
            ExceptionDiagnostic(result.error()));
          !logged)
        return std::unexpected(std::move(logged.error()));
      return {};
    }
    return std::move(*result);
  }

  std::expected<std::optional<EntityId>, StarException> WorldCallbacks::spawnStagehand(
      World& world, Vec2F const& spawnPosition, String const& typeName, Json const& overrides) {
    auto stagehandDatabaseResult = world.root().stagehandDatabase();
    if (!stagehandDatabaseResult)
      return std::unexpected(std::move(stagehandDatabaseResult.error()));
    auto stagehandDatabase = std::move(*stagehandDatabaseResult);
    auto spawn = [&]() -> std::expected<std::optional<EntityId>, StarException> {
      auto stagehandResult = stagehandDatabase->createStagehand(typeName, overrides);
      if (!stagehandResult)
        return std::unexpected(std::move(stagehandResult.error()));
      auto stagehand = std::move(*stagehandResult);
      stagehand->setPosition(spawnPosition);
      if (auto operation = world.addEntity(stagehand); !operation)
        return std::unexpected(std::move(operation.error()));
      return stagehand->inWorld() ? std::optional<EntityId>(stagehand->entityId()) : std::nullopt;
    };
    auto result = spawn();
    if (!result) {
      if (auto logged = world.root().logger().warn_exception(
            Star::format("Could not spawn Stagehand of type '{}'", typeName), ExceptionDiagnostic(result.error()));
          !logged)
        return std::unexpected(std::move(logged.error()));
      return {};
    }
    return std::move(*result);
  }

  std::expected<std::optional<EntityId>, StarException> WorldCallbacks::spawnProjectile(World& world,
      String const& projectileType,
      Vec2F const& spawnPosition,
      std::optional<EntityId> const& sourceEntityId,
      std::optional<Vec2F> const& projectileDirection,
      bool trackSourceEntity,
      Json const& projectileParameters) {
    auto spawn = [&]() -> std::expected<std::optional<EntityId>, StarException> {
      auto projectileDatabaseResult = world.root().projectileDatabase();
      if (!projectileDatabaseResult)
        return std::unexpected(std::move(projectileDatabaseResult.error()));
      auto projectileDatabase = std::move(*projectileDatabaseResult);
      auto projectileResult = projectileDatabase->createProjectile(projectileType, projectileParameters ? projectileParameters : JsonObject());
      if (!projectileResult)
        return std::unexpected(std::move(projectileResult.error()));
      auto projectile = std::move(*projectileResult);
      projectile->setInitialPosition(spawnPosition);
      projectile->setInitialDirection(projectileDirection.value_or(Vec2F{}));
      projectile->setSourceEntity(sourceEntityId.value_or(null_entity_id), trackSourceEntity);
      if (auto operation = world.addEntity(projectile); !operation)
        return std::unexpected(std::move(operation.error()));
      return projectile->inWorld() ? std::optional<EntityId>(projectile->entityId()) : std::nullopt;
    };
    auto result = spawn();
    if (!result) {
      if (auto logged = world.root().logger().warn_exception(
            Star::format("Could not spawn Projectile of type '{}'", projectileType),
            ExceptionDiagnostic(result.error()));
          !logged)
        return std::unexpected(std::move(logged.error()));
      return {};
    }
    return std::move(*result);
  }

  std::expected<std::optional<EntityId>, StarException> WorldCallbacks::spawnVehicle(
      World& world, String const& vehicleName, Vec2F const& pos, Json const& extraConfig) {
    auto vehicleDatabaseResult = world.root().vehicleDatabase();
    if (!vehicleDatabaseResult)
      return std::unexpected(std::move(vehicleDatabaseResult.error()));
    auto vehicleDatabase = std::move(*vehicleDatabaseResult);
    auto vehicleResult = vehicleDatabase->create(vehicleName, extraConfig);
    if (!vehicleResult)
      return std::unexpected(std::move(vehicleResult.error()));
    auto vehicle = std::move(*vehicleResult);
    vehicle->setPosition(pos);
    if (auto operation = world.addEntity(vehicle); !operation)
      return std::unexpected(std::move(operation.error()));
    return vehicle->inWorld() ? std::optional<EntityId>(vehicle->entityId()) : std::nullopt;
  }

  std::expected<double, StarException> WorldEnvironmentCallbacks::time(World& world) {
    return world.epochTime();
  }

  std::expected<std::uint64_t, StarException> WorldEnvironmentCallbacks::day(World& world) {
    return world.day();
  }

  std::expected<double, StarException> WorldEnvironmentCallbacks::timeOfDay(World& world) {
    return world.timeOfDay() / world.dayLength();
  }

  std::expected<float, StarException> WorldEnvironmentCallbacks::dayLength(World& world) {
    return world.dayLength();
  }

  std::expected<Json, StarException> WorldCallbacks::getProperty(World& world, String const& arg1, Json const& arg2) {
    return world.getProperty(arg1, arg2);
  }

  std::expected<void, StarException> WorldCallbacks::setProperty(World& world, String const& arg1, Json const& arg2) {
    return world.setProperty(arg1, arg2);
  }

  std::expected<std::optional<LiquidLevel>, StarException> WorldEnvironmentCallbacks::liquidAt(World& world, std::variant<RectF, Vec2I> boundBoxOrPoint) {
    auto levelResult = std::visit([&world](auto const& value) -> std::expected<LiquidLevel, StarException> { return world.liquidLevel(value); }, boundBoxOrPoint);
    if (!levelResult)
      return std::unexpected(std::move(levelResult.error()));
    if (levelResult->liquid != empty_liquid_id)
      return *levelResult;
    return {};
  }

  std::expected<float, StarException> WorldEnvironmentCallbacks::gravity(World& world, Vec2F const& arg1) {
    return world.gravity(arg1);
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::spawnLiquid(World& world, Vec2F const& position, LiquidId liquid, float quantity) {
    return world.modifyTile(Vec2I::floor(position), PlaceLiquid{.liquid = liquid, .liquidLevel = quantity}, true);
  }

  std::expected<std::optional<LiquidLevel>, StarException> WorldEnvironmentCallbacks::destroyLiquid(World& world, Vec2F const& position) {
    auto liquidLevel = world.liquidLevel(Vec2I::floor(position));
    if (liquidLevel.liquid != empty_liquid_id) {
      auto modifiedResult =
        world.modifyTile(Vec2I::floor(position), PlaceLiquid{.liquid = empty_liquid_id, .liquidLevel = 0}, true);
      if (!modifiedResult)
        return std::unexpected(std::move(modifiedResult.error()));
      auto modified = std::move(*modifiedResult);
      if (modified)
        return liquidLevel;
    }
    return {};
  }

  bool WorldEnvironmentCallbacks::isTileProtected(World& world, Vec2F const& position) {
    return world.isTileProtected(Vec2I::floor(position));
  }

  std::expected<std::optional<PlatformerAStar::Path>, StarException>
  WorldCallbacks::findPlatformerPath(World& world, Vec2F const& start, Vec2F const& end,
                                     ActorMovementParameters actor_movement_parameters,
                                     PlatformerAStar::Parameters search_parameters) {
    auto path_finder_result = PlatformerAStar::PathFinder::create(
      world, start, end, std::move(actor_movement_parameters), std::move(search_parameters));
    if (!path_finder_result)
      return std::unexpected(std::move(path_finder_result.error()));
    auto path_finder = std::move(*path_finder_result);
    if (auto operation = path_finder->explore({}); !operation)
      return std::unexpected(std::move(operation.error()));
    return path_finder->extract_result();
  }

  std::expected<std::shared_ptr<PlatformerAStar::PathFinder>, StarException>
  WorldCallbacks::platformerPathStart(World& world, Vec2F const& start, Vec2F const& end,
                                      ActorMovementParameters actor_movement_parameters,
                                      PlatformerAStar::Parameters search_parameters) {
    return PlatformerAStar::PathFinder::create(world, start, end, std::move(actor_movement_parameters),
                                               std::move(search_parameters));
  }

  std::expected<void, StarException> ClientWorldCallbacks::resendEntity(WorldClient& world, EntityId arg1) {
    return world.resendEntity(arg1);
  }

  std::expected<RectI, StarException> ClientWorldCallbacks::clientWindow(WorldClient& world) {
    return world.clientWindow();
  }

  std::expected<String, StarException> ServerWorldCallbacks::id(WorldServer& world) {
    return world.worldId();
  }

  std::expected<bool, StarException> ServerWorldCallbacks::breakObject(WorldServer& world, EntityId arg1, bool arg2) {
    if (auto entity = world.get<Object>(arg1)) {
      bool smash = arg2;
      if (auto operation = entity->breakObject(smash); !operation)
        return std::unexpected(std::move(operation.error()));
      return true;
    }
    return false;
  }

  std::expected<bool, StarException> ServerWorldCallbacks::isVisibleToPlayer(WorldServer& world, RectF const& arg1) {
    return world.isVisibleToPlayer(arg1);
  }

  std::expected<bool, StarException> ServerWorldCallbacks::loadRegion(WorldServer& world, RectF const& arg1) {
    return world.signalRegion(RectI::integral(arg1));
  }

  std::expected<bool, StarException> ServerWorldCallbacks::regionActive(WorldServer& world, RectF const& arg1) {
    return world.regionActive(RectI::integral(arg1));
  }

  std::expected<void, StarException> ServerWorldCallbacks::setTileProtection(WorldServer& world, DungeonId arg1, bool arg2) {
    DungeonId dungeonId = arg1;
    bool isProtected = arg2;
    return world.setTileProtection(dungeonId, isProtected);
  }

  std::expected<bool, StarException> ServerWorldCallbacks::isPlayerModified(WorldServer& world, RectI const& region) {
    return world.isPlayerModified(region);
  }

  std::expected<std::optional<LiquidLevel>, StarException> ServerWorldCallbacks::forceDestroyLiquid(WorldServer& world, Vec2F const& position) {
    auto liquidLevel = world.liquidLevel(Vec2I::floor(position));
    if (liquidLevel.liquid != empty_liquid_id) {
      auto modifiedResult =
        world.forceModifyTile(Vec2I::floor(position), PlaceLiquid{.liquid = empty_liquid_id, .liquidLevel = 0}, true);
      if (!modifiedResult)
        return std::unexpected(std::move(modifiedResult.error()));
      auto modified = std::move(*modifiedResult);
      if (modified)
        return liquidLevel;
    }
    return {};
  }

  std::expected<EntityId, StarException> ServerWorldCallbacks::loadUniqueEntity(WorldServer& world, String const& uniqueId) {
    return world.loadUniqueEntity(uniqueId);
  }

  std::expected<void, StarException> ServerWorldCallbacks::setUniqueId(WorldServer& world, EntityId entityId, std::optional<String> const& uniqueId) {
    auto entity = world.entity(entityId);
    if (!entity)
      return std::unexpected(StarException::format("No such entity with id {}", entityId));
    if (auto npc = as_ref<Npc>(*entity))
      npc->get().setUniqueId(uniqueId);
    else if (auto monster = as_ref<Monster>(*entity))
      monster->get().setUniqueId(uniqueId);
    else if (auto object = as_ref<Object>(*entity))
      object->get().setUniqueId(uniqueId);
    else if (auto stagehand = as_ref<Stagehand>(*entity))
      stagehand->get().setUniqueId(uniqueId);
    else {
      if (!EntityTypeNames)
        return std::unexpected(EntityTypeNames.error());
      auto typePointerResult = EntityTypeNames->get_right(entity->entity_type());
      if (!typePointerResult)
        return std::unexpected(std::move(typePointerResult.error()));
      auto typePointer = std::move(*typePointerResult);
      auto const& type = typePointer.get();
      return std::unexpected(StarException::format("Cannot set unique id on entity of type {}", type));
    }
    return {};
  }

  std::expected<Json, StarException> ServerWorldCallbacks::take_item_drop(World& world, EntityId entity_id, std::optional<EntityId> const& taken_by) {
    if (auto item_drop = world.get<ItemDrop>(entity_id); item_drop && item_drop->isMaster()) {
      auto taken = taken_by ? item_drop->take_by(*taken_by) : item_drop->take();
      if (!taken)
        return std::unexpected(std::move(taken.error()));
      if (*taken)
        return (*taken)->descriptor().to_json();
    }
    return Json();
  }

  std::expected<void, StarException> ServerWorldCallbacks::setPlayerStart(WorldServer& world, Vec2F const& playerStart, std::optional<bool> respawnInWorld) {
    world.setPlayerStart(playerStart, respawnInWorld.value_or(false));
    return {};
  }

  std::expected<List<EntityId>, StarException> ServerWorldCallbacks::players(WorldServer& world) {
    return world.players();
  }

  std::expected<LuaString, StarException> ServerWorldCallbacks::fidelity(WorldServer& world, LuaEngine& engine) {
    if (!WorldServerFidelityNames)
      return std::unexpected(WorldServerFidelityNames.error());
    auto namePointerResult = WorldServerFidelityNames->get_right(world.fidelity());
    if (!namePointerResult)
      return std::unexpected(std::move(namePointerResult.error()));
    auto namePointer = std::move(*namePointerResult);
    auto const& name = namePointer.get();
    return engine.create_string(name);
  }

  std::expected<std::optional<LuaValue>, StarException> ServerWorldCallbacks::callScriptContext(WorldServer& world, String const& contextName, String const& function, LuaVariadic<LuaValue> const& args) {
    auto context = world.scriptContext(contextName);
    if (!context)
      return std::unexpected(StarException::format("Context {} does not exist", contextName));
    return context->invoke(function, args);
  }

  std::expected<bool, StarException> ServerWorldCallbacks::sendPacket(WorldServer& world, ConnectionId clientId, String const& packetType, Json const& packetData) {
    auto type = packet_type_from_name(packetType);
    if (!type)
      return std::unexpected(std::move(type.error()));
    auto packetResult = createPacket(world.root().random(), *type, packetData);
    if (!packetResult)
      return std::unexpected(std::move(packetResult.error()));
    auto packet = std::move(*packetResult);
    return world.sendPacket(clientId, packet);
  }

  std::expected<void, StarException> WorldDebugCallbacks::debugPoint(Root& root, Vec2F const& arg1, Color const& arg2) {
    root.spatialLogger().log_point("world", arg1, arg2.to_rgba());
    return {};
  }

  std::expected<void, StarException> WorldDebugCallbacks::debugLine(Root& root, Vec2F const& arg1, Vec2F const& arg2, Color const& arg3) {
    root.spatialLogger().log_line("world", arg1, arg2, arg3.to_rgba());
    return {};
  }

  std::expected<void, StarException> WorldDebugCallbacks::debugPoly(Root& root, PolyF const& poly, Color const& color) {
    root.spatialLogger().log_poly("world", poly, color.to_rgba());
    return {};
  }

  std::expected<void, StarException> WorldDebugCallbacks::debugText(Root& root, LuaEngine& engine, LuaVariadic<LuaValue> const& args) {
    if (args.size() < 3)
      return std::unexpected(StarException::format("Too few arguments to debugText: {}", args.size()));
    auto positionResult = engine.lua_to<Vec2F>(args[args.size() - 2]);
    if (!positionResult)
      return std::unexpected(std::move(positionResult.error()));
    auto position = std::move(*positionResult);
    auto colorResult = engine.lua_to<Color>(args[args.size() - 1]);
    if (!colorResult)
      return std::unexpected(std::move(colorResult.error()));
    auto color = std::move(*colorResult);
    auto formatResult = engine.lua_to<String>(args[0]);
    if (!formatResult)
      return std::unexpected(std::move(formatResult.error()));
    auto format = std::move(*formatResult);
    auto textResult = format_lua(format, std::span<LuaValue const>(args).subspan(1, args.size() - 3));
    if (!textResult)
      return std::unexpected(std::move(textResult.error()));
    auto text = std::move(*textResult);
    root.spatialLogger().log_text("world", std::move(text), position, color.to_rgba());
    return {};
  }

  std::expected<LuaTable, StarException> WorldEntityCallbacks::entityQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options) {
    return LuaBindings::entityQuery<Entity>(world, engine, pos1, pos2, std::move(options));
  }

  std::expected<LuaTable, StarException> WorldEntityCallbacks::monsterQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options) {
    return LuaBindings::entityQuery<Monster>(world, engine, pos1, pos2, std::move(options));
  }

  std::expected<LuaTable, StarException> WorldEntityCallbacks::npcQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options) {
    return LuaBindings::entityQuery<Npc>(world, engine, pos1, pos2, std::move(options));
  }

  std::expected<LuaTable, StarException> WorldEntityCallbacks::objectQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options) {
    String objectName;
    if (options) {
      auto nameResult = options->get<std::optional<String>>("name");
      if (!nameResult)
        return std::unexpected(std::move(nameResult.error()));
      auto name = std::move(*nameResult);
      objectName = name.value_or(String{});
    }

    return LuaBindings::entityQuery<Object>(world,
        engine,
        pos1,
        pos2,
        std::move(options),
        [&objectName](std::shared_ptr<Object> const& entity) -> std::expected<bool, StarException> {
          if (objectName.empty())
            return true;
          auto nameResult = entity->name();
          if (!nameResult)
            return std::unexpected(std::move(nameResult.error()));
          auto name = std::move(*nameResult);
          return name == objectName;
        });
  }

  std::expected<LuaTable, StarException> WorldEntityCallbacks::itemDropQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options) {
    return LuaBindings::entityQuery<ItemDrop>(world, engine, pos1, pos2, std::move(options));
  }

  std::expected<LuaTable, StarException> WorldEntityCallbacks::playerQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options) {
    return LuaBindings::entityQuery<Player>(world, engine, pos1, pos2, std::move(options));
  }

  std::expected<LuaTable, StarException> WorldEntityCallbacks::loungeableQuery(World& world, LuaEngine& engine, Vec2F const& pos1, LuaValue const& pos2, std::optional<LuaTable> options) {
    String orientationName;
    if (options) {
      auto nameResult = options->get<std::optional<String>>("orientation");
      if (!nameResult)
        return std::unexpected(std::move(nameResult.error()));
      auto name = std::move(*nameResult);
      orientationName = name.value_or(String{});
    }

    LoungeOrientation orientation = LoungeOrientation::None;
    if (orientationName == "sit")
      orientation = LoungeOrientation::Sit;
    else if (orientationName == "lay")
      orientation = LoungeOrientation::Lay;
    else if (orientationName == "stand")
      orientation = LoungeOrientation::Stand;
    else if (orientationName.empty())
      orientation = LoungeOrientation::None;
    else
      return std::unexpected(StarException(Star::format("Unsupported loungeableQuery orientation {}", orientationName)));

    auto filter = [orientation](std::shared_ptr<LoungeableObject> const& entity) -> std::expected<bool, StarException> {
      auto loungeable = as<LoungeableEntity>(entity);
      if (!loungeable || loungeable->anchorCount() == 0)
        return false;

      if (orientation == LoungeOrientation::None)
        return true;
      auto posResult = loungeable->loungeAnchor(0);
      if (!posResult)
        return std::unexpected(std::move(posResult.error()));
      auto pos = std::move(*posResult);
      return pos && pos->orientation == orientation;
    };

    return LuaBindings::entityQuery<LoungeableObject>(world, engine, pos1, pos2, std::move(options), filter);
  }

  std::expected<LuaTable, StarException> WorldEntityCallbacks::entityLineQuery(World& world, LuaEngine& engine, Vec2F const& point1, Vec2F const& point2, std::optional<LuaTable> options) {
    return LuaBindings::entityLineQuery<Entity>(world, engine, point1, point2, std::move(options));
  }

  std::expected<LuaTable, StarException> WorldEntityCallbacks::objectLineQuery(World& world, LuaEngine& engine, Vec2F const& point1, Vec2F const& point2, std::optional<LuaTable> options) {
    return LuaBindings::entityLineQuery<Object>(world, engine, point1, point2, std::move(options));
  }

  std::expected<LuaTable, StarException> WorldEntityCallbacks::npcLineQuery(World& world, LuaEngine& engine, Vec2F const& point1, Vec2F const& point2, std::optional<LuaTable> options) {
    return LuaBindings::entityLineQuery<Npc>(world, engine, point1, point2, std::move(options));
  }

  std::expected<bool, StarException> WorldEntityCallbacks::entityExists(World& world, EntityId entityId) {
    return static_cast<bool>(world.entity(entityId));
  }

  std::expected<std::optional<Json>, StarException> WorldEntityCallbacks::entitySnapshot(World& world, EntityId entityId) {
    auto entity = world.entity(entityId);
    if (!entity)
      return {};
    if (!EntityTypeNames)
      return std::unexpected(EntityTypeNames.error());
    auto type = EntityTypeNames->get_right(entity->entity_type());
    if (!type)
      return std::unexpected(std::move(type.error()));
    auto factory = world.root().entityFactory();
    if (!factory)
      return std::unexpected(std::move(factory.error()));

    bool master = entity->isMaster();
    bool transient = entity->entity_type() == EntityType::Projectile || entity->entity_type() == EntityType::PlantDrop;
    JsonObject snapshot{
      {"schema", "asterbound.entitySnapshot"}, {"schemaVersion", 1},
      {"type", type->get()}, {"entityId", entityId}, {"position", json_from_vec2f(entity->position())},
      {"authority", std::string_view(master ? "master" : "replica")}, {"complete", master && !transient},
      {"format", std::string_view(transient ? "net" : "disk")}
    };
    JsonArray warnings;
    if (transient) {
      NetCompatibilityRules rules;
      auto store = (*factory)->net_store_entity(entity, rules);
      if (!store)
        return std::unexpected(std::move(store.error()));
      // These types keep their live state directly in NetElements. Unlike
      // write_net_state(0), this read does not advance their replication version.
      auto state = [&]() -> std::expected<std::pair<ByteArray, std::uint64_t>, StarException> {
        if (auto projectile = as<Projectile>(entity))
          return projectile->snapshot_net_state(rules);
        if (auto plantDrop = as<PlantDrop>(entity))
          return plantDrop->snapshot_net_state(rules);
        return std::unexpected(EntityException("Transient entity type does not match its native class"));
      }();
      if (!state)
        return std::unexpected(std::move(state.error()));
      snapshot["data"] = JsonObject{
        {"encoding", "base64"}, {"compatibilityVersion", rules.version()},
        {"netStore", base64_encode(*store)}, {"netState", base64_encode(state->first)},
        {"netStateVersion", state->second}
      };
      warnings.push_back("Network reconstruction record, not a persistent save. netState is a full read_net_state record; netStateVersion is observational, not a continuation token.");
    } else {
      // The existing dispatcher includes Player::diskStore on replicas too.
      // Keep its errors (including live Lua storage conversion errors) intact.
      auto store = (*factory)->store_versioned_entity(entity);
      if (!store)
        return std::unexpected(std::move(store.error()));
      snapshot["data"] = store->toJson();
      warnings.push_back("Single-entity engine persistence state only; external assets and world relationships are not included.");
    }
    if (!master) {
      warnings.push_back("Local replica only: fields not synchronized to this world may be absent, stale, or local defaults; this is not the remote authoritative save.");
      if (entity->entity_type() == EntityType::Player)
        warnings.push_back("Player disk serialization includes locally held inventory, quests, and script storage, but cannot recover inventory, quests, script storage, or other private fields the remote player has not transmitted.");
    }
    snapshot["warnings"] = std::move(warnings);
    return std::optional<Json>(Json(std::move(snapshot)));
  }

  std::expected<bool, StarException> WorldEntityCallbacks::entityCanDamage(World& world, EntityId sourceId, EntityId targetId) {
    auto source = world.entity(sourceId);
    auto target = world.entity(targetId);

    if (!source || !target || !source->getTeam().can_damage(target->getTeam(), false))
      return false;

    return true;
  }

  std::expected<Json, StarException> WorldEntityCallbacks::entityDamageTeam(World& world, EntityId entityId) {
    if (auto entity = world.entity(entityId))
      return entity->getTeam().to_json();
    return {};
  }

  std::expected<bool, StarException> WorldEntityCallbacks::entityAggressive(World& world, EntityId entityId) {
    auto entity = world.entity(entityId);
    if (auto monster = as<Monster>(entity))
      return monster->aggressive();
    if (auto npc = as<Npc>(entity))
      return npc->aggressive();
    return false;
  }

  std::expected<std::optional<LuaString>, StarException> WorldEntityCallbacks::entity_type(World& world, LuaEngine& engine, EntityId entityId) {
    if (auto entity = world.entity(entityId)) {
      if (!EntityTypeNames)
        return std::unexpected(EntityTypeNames.error());
      auto namePointerResult = EntityTypeNames->get_right(entity->entity_type());
      if (!namePointerResult)
        return std::unexpected(std::move(namePointerResult.error()));
      auto namePointer = std::move(*namePointerResult);
      auto const& name = namePointer.get();
      return engine.create_string(name);
    }
    return {};
  }

  std::expected<std::optional<Vec2F>, StarException> WorldEntityCallbacks::entityPosition(World& world, EntityId entityId) {
    if (auto entity = world.entity(entityId)) {
      return entity->position();
    } else {
      return {};
    }
  }

  std::expected<std::optional<RectF>, StarException> WorldEntityCallbacks::entityMetaBoundBox(World& world, EntityId entityId) {
    if (auto entity = world.entity(entityId)) {
      return entity->meta_bound_box();
    } else {
      return {};
    }
  }

  std::expected<std::optional<Vec2F>, StarException> WorldEntityCallbacks::entityVelocity(World& world, EntityId entityId) {
    auto entity = world.entity(entityId);

    if (auto mobileEntity = as<MobileEntity>(entity))
        return mobileEntity->movement_controller().velocity();

    return {};
  }

  std::expected<std::optional<std::uint64_t>, StarException> WorldEntityCallbacks::entityCurrency(World& world, EntityId entityId, String const& currencyType) {
    if (auto player = world.get<Player>(entityId)) {
      return player->currency(currencyType);
    } else {
      return {};
    }
  }

  std::expected<std::optional<std::uint64_t>, StarException> WorldEntityCallbacks::entityHasCountOfItem(World& world, EntityId entityId, Json descriptor, std::optional<bool> exactMatch) {
    if (auto player = world.get<Player>(entityId)) {
      auto itemDescriptorResult = ItemDescriptor::create(descriptor);
      if (!itemDescriptorResult)
        return std::unexpected(std::move(itemDescriptorResult.error()));
      auto itemDescriptor = std::move(*itemDescriptorResult);
      return player->inventory()->hasCountOfItem(itemDescriptor, exactMatch.value_or(false));
    } else {
      return {};
    }
  }

  std::expected<std::optional<Vec2F>, StarException> WorldEntityCallbacks::entityHealth(World& world, EntityId entityId) {
    if (auto entity = world.get<DamageBarEntity>(entityId)) {
      auto maximumResult = entity->maxHealth();
      if (!maximumResult)
        return std::unexpected(std::move(maximumResult.error()));
      auto maximum = std::move(*maximumResult);
      return Vec2F(entity->health(), maximum);
    }
    return {};
  }

  std::expected<std::optional<String>, StarException> WorldEntityCallbacks::entitySpecies(World& world, EntityId entityId) {
    if (auto player = world.get<Player>(entityId)) {
      return player->species();
    } else if (auto npc = world.get<Npc>(entityId)) {
      return npc->species();
    } else {
      return {};
    }
  }

  std::expected<std::optional<String>, StarException> WorldEntityCallbacks::entityGender(World& world, EntityId entityId) {
    std::optional<Gender> gender;
    if (auto player = world.get<Player>(entityId))
      gender = player->gender();
    else if (auto npc = world.get<Npc>(entityId))
      gender = npc->gender();
    if (!gender)
      return {};
    auto namePointerResult = enum_name<gender_names>(*gender);
    if (!namePointerResult)
      return std::unexpected(std::move(namePointerResult.error()));
    auto namePointer = std::move(*namePointerResult);
    String name(namePointer);
    return name;
  }

  std::expected<std::optional<String>, StarException> WorldEntityCallbacks::entityName(World& world, EntityId entityId) {
    if (auto entity = world.entity(entityId)) {
      auto nameResult = entity->name();
      if (!nameResult)
        return std::unexpected(std::move(nameResult.error()));
      auto name = std::move(*nameResult);
      return name;
    }
    return {};
  }

  std::expected<std::optional<Json>, StarException> WorldEntityCallbacks::entityNametag(World& world, EntityId entityId) {
    auto entity = world.entity(entityId);

    Json result;
    if (auto nametagEntity = as<NametagEntity>(entity)) {
      auto nametag = nametagEntity->nametag();
      if (!nametag)
        return std::unexpected(std::move(nametag.error()));
      auto color = nametagEntity->nametagColor();
      if (!color)
        return std::unexpected(std::move(color.error()));
      auto origin = nametagEntity->nametagOrigin();
      if (!origin)
        return std::unexpected(std::move(origin.error()));
      result = JsonObject{
        {"nametag", std::move(*nametag)},
        {"displayed", nametagEntity->displayNametag()},
        {"color", json_from_color(Color::rgb(*color))},
        {"origin", json_from_vec2f(*origin)},
      };
      if (auto status = nametagEntity->statusText())
        {
        auto updated = result.set("status", *status);
        if (!updated)
          return std::unexpected(std::move(updated.error()));
        result = std::move(*updated);
      }
    }

    return result;
  }

  std::expected<std::optional<String>, StarException> WorldEntityCallbacks::entityDescription(World& world, EntityId entityId, std::optional<String> const& species) {
    if (auto entity = world.entity(entityId)) {
      if (auto inspectableEntity = as<InspectableEntity>(entity)) {
        if (species)
          {
          auto descriptionResult = inspectableEntity->inspectionDescription(*species);
          if (!descriptionResult)
            return std::unexpected(std::move(descriptionResult.error()));
          auto description = std::move(*descriptionResult);
          return description;
        }
      }

      auto valueResult = entity->description();
      if (!valueResult)
        return std::unexpected(std::move(valueResult.error()));
      auto value = std::move(*valueResult);
      return value;
    }

    return {};
  }

  std::expected<std::optional<List<Drawable>>, StarException>
  WorldEntityCallbacks::entityPortrait(World& world, EntityId entityId, String const& portraitMode) {
    if (auto entity = as<PortraitEntity>(world.entity(entityId))) {
      auto modePointerResult = enum_value<portrait_mode_names>(portraitMode);
      if (!modePointerResult)
        return std::unexpected(std::move(modePointerResult.error()));
      auto modePointer = std::move(*modePointerResult);
      auto mode = modePointer;
      auto portraitResult = entity->portrait(mode);
      if (!portraitResult)
        return std::unexpected(std::move(portraitResult.error()));
      return std::move(*portraitResult);
    }
    return {};
  }

  std::expected<std::optional<String>, StarException> WorldEntityCallbacks::entityHandItem(World& world, EntityId entityId, String const& handName) {
    ToolHand toolHand;
    if (handName == "primary") {
      toolHand = ToolHand::Primary;
    } else if (handName == "alt") {
      toolHand = ToolHand::Alt;
    } else {
      return std::unexpected(StarException(Star::format("Unknown tool hand {}", handName)));
    }

    if (auto entity = world.get<ToolUserEntity>(entityId)) {
      if (auto item = entity->handItem(toolHand)) {
        return item->name();
      }
    }

    return {};
  }

  std::expected<Json, StarException> WorldEntityCallbacks::entityHandItemDescriptor(World& world, EntityId entityId, String const& handName) {
    ToolHand toolHand;
    if (handName == "primary") {
      toolHand = ToolHand::Primary;
    } else if (handName == "alt") {
      toolHand = ToolHand::Alt;
    } else {
      return std::unexpected(StarException(Star::format("Unknown tool hand {}", handName)));
    }

    if (auto entity = world.get<ToolUserEntity>(entityId)) {
      if (auto item = entity->handItem(toolHand)) {
        return item->descriptor().to_json();
      }
    }

    return Json();
  }

  std::expected<std::optional<String>, StarException> WorldEntityCallbacks::entityUniqueId(World& world,
                                                                                           EntityId entityId) {
    if (auto entity = world.entity(entityId))
      return entity->uniqueId();
    return {};
  }

  std::expected<Json, StarException> WorldEntityCallbacks::getObjectParameter(World& world, EntityId entityId, String const& parameterName, std::optional<Json> const& defaultValue) {
    Json val = Json();

    if (auto objectEntity = as<Object>(world.entity(entityId))) {
      auto parameterResult = objectEntity->configValue(parameterName);
      if (!parameterResult)
        return std::unexpected(std::move(parameterResult.error()));
      auto parameter = std::move(*parameterResult);
      val = std::move(parameter);
      if (!val && defaultValue)
        val = *defaultValue;
    }

    return val;
  }

  std::expected<Json, StarException> WorldEntityCallbacks::getNpcScriptParameter(World& world, EntityId entityId, String const& parameterName, std::optional<Json> const& defaultValue) {
    Json val = Json();

    if (auto npcEntity = as<Npc>(world.entity(entityId))) {
      auto parameterResult = npcEntity->scriptConfigParameter(parameterName);
      if (!parameterResult)
        return std::unexpected(std::move(parameterResult.error()));
      auto parameter = std::move(*parameterResult);
      val = std::move(parameter);
      if (!val && defaultValue)
        val = *defaultValue;
    }

    return val;
  }

  std::expected<List<Vec2I>, StarException> WorldEntityCallbacks::objectSpaces(World& world, EntityId entityId) {
    if (auto tileEntity = as<TileEntity>(world.entity(entityId)))
      return tileEntity->spaces();
    return {};
  }

  std::expected<std::optional<int>, StarException> WorldEntityCallbacks::farmableStage(World& world, EntityId entityId) {
    if (auto farmable = world.get<FarmableObject>(entityId)) {
      return farmable->stage();
    }

    return {};
  }

  std::expected<std::optional<int>, StarException> WorldEntityCallbacks::containerSize(World& world, EntityId entityId) {
    if (auto container = world.get<ContainerObject>(entityId))
      return container->containerSize();

    return {};
  }

  std::expected<bool, StarException> WorldEntityCallbacks::containerClose(World& world, EntityId entityId) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      if (auto operation = container->containerClose(); !operation)
        return std::unexpected(std::move(operation.error()));
      return true;
    }

    return false;
  }

  std::expected<bool, StarException> WorldEntityCallbacks::containerOpen(World& world, EntityId entityId) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      if (auto operation = container->containerOpen(); !operation)
        return std::unexpected(std::move(operation.error()));
      return true;
    }

    return false;
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerItems(World& world, EntityId entityId) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      JsonArray res;
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      for (auto const& item : container->itemBag()->items())
        res.push_back(itemDb->to_json(item));
      return res;
    }

    return Json();
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerItemAt(World& world, EntityId entityId, std::size_t offset) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      auto items = container->itemBag()->items();
      if (offset < items.size()) {
        return itemDb->to_json(items[offset]);
      }
    }

    return Json();
  }

  std::expected<std::optional<bool>, StarException> WorldEntityCallbacks::containerConsume(World& world, EntityId entityId, Json const& items) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto toConsumeResult = ItemDescriptor::create(items);
      if (!toConsumeResult)
        return std::unexpected(std::move(toConsumeResult.error()));
      auto toConsume = std::move(*toConsumeResult);
      auto promiseResult = container->consumeItems(toConsume);
      if (!promiseResult)
        return std::unexpected(std::move(promiseResult.error()));
      auto promise = std::move(*promiseResult);
      return promise.result();
    }

    return {};
  }

  std::expected<std::optional<bool>, StarException> WorldEntityCallbacks::containerConsumeAt(World& world, EntityId entityId, std::size_t offset, int count) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      if (offset < container->containerSize()) {
        auto promiseResult = container->consumeItems(offset, count);
        if (!promiseResult)
          return std::unexpected(std::move(promiseResult.error()));
        auto promise = std::move(*promiseResult);
        return promise.result();
      }
    }

    return {};
  }

  std::expected<std::optional<std::size_t>, StarException> WorldEntityCallbacks::containerAvailable(World& world, EntityId entityId, Json const& items) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemBag = container->itemBag();
      auto toCheckResult = ItemDescriptor::create(items);
      if (!toCheckResult)
        return std::unexpected(std::move(toCheckResult.error()));
      auto toCheck = std::move(*toCheckResult);
      return itemBag->available(toCheck);
    }

    return {};
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerTakeAll(World& world, EntityId entityId) {
    auto itemDbResult = world.root().itemDatabase();
    if (!itemDbResult)
      return std::unexpected(std::move(itemDbResult.error()));
    auto itemDb = std::move(*itemDbResult);
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto promiseResult = container->clearContainer();
      if (!promiseResult)
        return std::unexpected(std::move(promiseResult.error()));
      auto promise = std::move(*promiseResult);
      auto itemListResult = promise.result();
      if (!itemListResult)
        return std::unexpected(std::move(itemListResult.error()));
      auto itemList = std::move(*itemListResult);
      if (itemList) {
        JsonArray res;
        for (auto const& item : *itemList)
          res.push_back(itemDb->to_json(item));
        return res;
      }
    }
    return Json();
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerTakeAt(World& world, EntityId entityId, std::size_t offset) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      if (offset < container->containerSize()) {
        auto promiseResult = container->takeItems(offset);
        if (!promiseResult)
          return std::unexpected(std::move(promiseResult.error()));
        auto promise = std::move(*promiseResult);
        auto resResult = promise.result();
        if (!resResult)
          return std::unexpected(std::move(resResult.error()));
        auto res = std::move(*resResult);
        if (res)
          return itemDb->to_json(*res);
      }
    }
    return Json();
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerTakeNumItemsAt(World& world, EntityId entityId, std::size_t offset, int const& count) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      if (offset < container->containerSize()) {
        auto promiseResult = container->takeItems(offset, count);
        if (!promiseResult)
          return std::unexpected(std::move(promiseResult.error()));
        auto promise = std::move(*promiseResult);
        auto resResult = promise.result();
        if (!resResult)
          return std::unexpected(std::move(resResult.error()));
        auto res = std::move(*resResult);
        if (res)
          return itemDb->to_json(*res);
      }
    }
    return Json();
  }

  std::expected<std::optional<std::size_t>, StarException> WorldEntityCallbacks::containerItemsCanFit(World& world, EntityId entityId, Json const& items) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      auto itemBag = container->itemBag();
      auto toSearchResult = itemDb->from_json(items);
      if (!toSearchResult)
        return std::unexpected(std::move(toSearchResult.error()));
      auto toSearch = std::move(*toSearchResult);
      return itemBag->items_can_fit(optional_ref(toSearch));
    }

    return {};
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerItemsFitWhere(World& world, EntityId entityId, Json const& items) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      auto itemBag = container->itemBag();
      auto toSearchResult = itemDb->from_json(items);
      if (!toSearchResult)
        return std::unexpected(std::move(toSearchResult.error()));
      auto toSearch = std::move(*toSearchResult);
      auto res = itemBag->items_fit_where(optional_ref(toSearch));
auto slotsResult = json_from_list<std::size_t>(res.slots);
      if (!slotsResult)
        return std::unexpected(std::move(slotsResult.error()));
      auto slots = std::move(*slotsResult);
      return JsonObject{{"leftover", res.leftover}, {"slots", std::move(slots)}};
    }

    return Json();
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerAddItems(World& world, EntityId entityId, Json const& items) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      auto toInsertResult = itemDb->from_json(items);
      if (!toInsertResult)
        return std::unexpected(std::move(toInsertResult.error()));
      auto toInsert = std::move(*toInsertResult);
      auto promiseResult = container->addItems(toInsert);
      if (!promiseResult)
        return std::unexpected(std::move(promiseResult.error()));
      auto promise = std::move(*promiseResult);
      auto resResult = promise.result();
      if (!resResult)
        return std::unexpected(std::move(resResult.error()));
      auto res = std::move(*resResult);
      if (res)
        return itemDb->to_json(*res);
    }
    return items;
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerStackItems(World& world, EntityId entityId, Json const& items) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      auto toInsertResult = itemDb->from_json(items);
      if (!toInsertResult)
        return std::unexpected(std::move(toInsertResult.error()));
      auto toInsert = std::move(*toInsertResult);
      auto promiseResult = container->addItems(toInsert);
      if (!promiseResult)
        return std::unexpected(std::move(promiseResult.error()));
      auto promise = std::move(*promiseResult);
      auto resResult = promise.result();
      if (!resResult)
        return std::unexpected(std::move(resResult.error()));
      auto res = std::move(*resResult);
      if (res)
        return itemDb->to_json(*res);
    }
    return items;
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerPutItemsAt(World& world, EntityId entityId, Json const& items, std::size_t offset) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      auto toInsertResult = itemDb->from_json(items);
      if (!toInsertResult)
        return std::unexpected(std::move(toInsertResult.error()));
      auto toInsert = std::move(*toInsertResult);
      if (offset < container->containerSize()) {
        auto promiseResult = container->putItems(offset, toInsert);
        if (!promiseResult)
          return std::unexpected(std::move(promiseResult.error()));
        auto promise = std::move(*promiseResult);
        auto resResult = promise.result();
        if (!resResult)
          return std::unexpected(std::move(resResult.error()));
        auto res = std::move(*resResult);
        if (res)
          return itemDb->to_json(*res);
      }
    }
    return items;
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerSwapItems(World& world, EntityId entityId, Json const& items, std::size_t offset) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      auto toInsertResult = itemDb->from_json(items);
      if (!toInsertResult)
        return std::unexpected(std::move(toInsertResult.error()));
      auto toInsert = std::move(*toInsertResult);
      if (offset < container->containerSize()) {
        auto promiseResult = container->swapItems(offset, toInsert, true);
        if (!promiseResult)
          return std::unexpected(std::move(promiseResult.error()));
        auto promise = std::move(*promiseResult);
        auto resResult = promise.result();
        if (!resResult)
          return std::unexpected(std::move(resResult.error()));
        auto res = std::move(*resResult);
        if (res)
          return itemDb->to_json(*res);
      }
    }
    return items;
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerSwapItemsNoCombine(World& world, EntityId entityId, Json const& items, std::size_t offset) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      auto toInsertResult = itemDb->from_json(items);
      if (!toInsertResult)
        return std::unexpected(std::move(toInsertResult.error()));
      auto toInsert = std::move(*toInsertResult);
      if (offset < container->containerSize()) {
        auto promiseResult = container->swapItems(offset, toInsert, false);
        if (!promiseResult)
          return std::unexpected(std::move(promiseResult.error()));
        auto promise = std::move(*promiseResult);
        auto resResult = promise.result();
        if (!resResult)
          return std::unexpected(std::move(resResult.error()));
        auto res = std::move(*resResult);
        if (res)
          return itemDb->to_json(*res);
      }
    }
    return items;
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerItemApply(World& world, EntityId entityId, Json const& items, std::size_t offset) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      auto toInsertResult = itemDb->from_json(items);
      if (!toInsertResult)
        return std::unexpected(std::move(toInsertResult.error()));
      auto toInsert = std::move(*toInsertResult);
      if (offset < container->containerSize()) {
        auto promiseResult = container->swapItems(offset, toInsert, false);
        if (!promiseResult)
          return std::unexpected(std::move(promiseResult.error()));
        auto promise = std::move(*promiseResult);
        auto resResult = promise.result();
        if (!resResult)
          return std::unexpected(std::move(resResult.error()));
        auto res = std::move(*resResult);
        if (res)
          return itemDb->to_json(*res);
      }
    }
    return items;
  }

  std::expected<std::optional<LuaValue>, StarException> WorldEntityCallbacks::callScriptedEntity(World& world, EntityId entityId, String const& function, LuaVariadic<LuaValue> const& args) {
    auto entity = as<ScriptedEntity>(world.entity(entityId));
    if (!entity || !entity->isMaster())
      return std::unexpected(StarException::format("Entity {} does not exist or is not a local master scripted entity", entityId));
    return entity->call_script(function, args);
  }

  std::expected<RpcPromise<Vec2F>, StarException> WorldEntityCallbacks::findUniqueEntity(World& world, String const& uniqueId) {
    return world.findUniqueEntity(uniqueId);
  }

  std::expected<RpcPromise<Json>, StarException> WorldEntityCallbacks::send_entity_message(World& world, LuaEngine& engine, LuaValue target_id, String const& message, LuaVariadic<Json> args) {
    auto arguments = JsonArray::from(std::move(args));
    if (std::holds_alternative<LuaString>(target_id)) {
      auto id_result = engine.lua_to<String>(target_id);
      if (!id_result)
        return std::unexpected(std::move(id_result.error()));
      auto id = std::move(*id_result);
      return world.send_entity_message(id, message, std::move(arguments));
    }
    auto id_result = engine.lua_to<EntityId>(target_id);
    if (!id_result)
      return std::unexpected(std::move(id_result.error()));
    auto id = std::move(*id_result);
    return world.send_entity_message(id, message, std::move(arguments));
  }

  std::expected<std::optional<List<EntityId>>, StarException> WorldEntityCallbacks::loungingEntities(World& world, EntityId entityId, std::optional<std::size_t> anchorIndex) {
    if (auto entity = world.get<LoungeableEntity>(entityId)) {
      auto entitiesResult = entity->entitiesLoungingIn(anchorIndex.value_or(std::size_t{}));
      if (!entitiesResult)
        return std::unexpected(std::move(entitiesResult.error()));
      auto entities = std::move(*entitiesResult);
      return entities.values();
    }
    return {};
  }

  std::expected<std::optional<bool>, StarException> WorldEntityCallbacks::loungeableOccupied(World& world, EntityId entityId, std::optional<std::size_t> anchorIndex) {
    auto entity = world.get<LoungeableEntity>(entityId);
    auto anchor = anchorIndex.value_or(std::size_t{});
    if (entity && entity->anchorCount() > anchor) {
      auto entitiesResult = entity->entitiesLoungingIn(anchor);
      if (!entitiesResult)
        return std::unexpected(std::move(entitiesResult.error()));
      auto entities = std::move(*entitiesResult);
      return !entities.empty();
    }
    return {};
  }

  std::expected<std::optional<std::size_t>, StarException> WorldEntityCallbacks::loungeableAnchorCount(World& world, EntityId entityId) {
    if (auto entity = world.get<LoungeableEntity>(entityId))
      return entity->anchorCount();
    return {};
  }

  std::expected<bool, StarException> WorldEntityCallbacks::isMonster(World& world, EntityId entityId, std::optional<bool> const& aggressive) {
    if (auto entity = world.get<Monster>(entityId))
      return !aggressive || *aggressive == entity->aggressive();

    return false;
  }

  std::expected<std::optional<String>, StarException> WorldEntityCallbacks::monsterType(World& world, EntityId entityId) {
    if (auto monster = world.get<Monster>(entityId))
      return monster->typeName();

    return {};
  }

  std::expected<std::optional<String>, StarException> WorldEntityCallbacks::npcType(World& world, EntityId entityId) {
    if (auto npc = world.get<Npc>(entityId))
      return npc->npcType();

    return {};
  }

  std::expected<std::optional<String>, StarException> WorldEntityCallbacks::stagehandType(World& world, EntityId entityId) {
    if (auto stagehand = world.get<Stagehand>(entityId))
      return stagehand->typeName();

    return {};
  }

  std::expected<bool, StarException> WorldEntityCallbacks::isNpc(World& world, EntityId entityId, std::optional<int> const& damageTeam) {
    if (auto entity = world.get<Npc>(entityId)) {
      return !damageTeam || *damageTeam == entity->getTeam().team;
    }

    return false;
  }

  std::expected<float, StarException> WorldEnvironmentCallbacks::lightLevel(World& world, Vec2F const& position) {
    return world.lightLevel(position);
  }

  std::expected<float, StarException> WorldEnvironmentCallbacks::windLevel(World& world, Vec2F const& position) {
    return world.windLevel(position);
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::breathable(World& world, Vec2F const& position) {
    return world.breathable(position);
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::underground(World& world, Vec2F const& position) {
    return world.isUnderground(position);
  }

  std::expected<LuaValue, StarException> WorldEnvironmentCallbacks::material(World& world, LuaEngine& engine, Vec2F const& position, String const& layerName) {
    TileLayer layer;
    if (layerName == "foreground") {
      layer = TileLayer::Foreground;
    } else if (layerName == "background") {
      layer = TileLayer::Background;
    } else {
      return std::unexpected(StarException(Star::format("Unsupported material layer {}", layerName)));
    }

    auto material_id = world.material(Vec2I::floor(position), layer);
    if (material_id == null_material_id) {
      return lua_nil;
    } else if (material_id == empty_material_id) {
      return false;
    } else {
      auto materials = world.root().material_database();
      if (!materials)
        return std::unexpected(std::move(materials.error()));
      auto name = (*materials)->material_name(material_id);
      if (!name)
        return std::unexpected(std::move(name.error()));
      return engine.create_string(*name);
    }
  }

  std::expected<LuaValue, StarException> WorldEnvironmentCallbacks::mod(World& world, LuaEngine& engine, Vec2F const& position, String const& layerName) {
    TileLayer layer;
    if (layerName == "foreground") {
      layer = TileLayer::Foreground;
    } else if (layerName == "background") {
      layer = TileLayer::Background;
    } else {
      return std::unexpected(StarException(Star::format("Unsupported mod layer {}", layerName)));
    }

    auto mod_id = world.mod(Vec2I::floor(position), layer);
    if (is_real_mod(mod_id)) {
      auto materials = world.root().material_database();
      if (!materials)
        return std::unexpected(std::move(materials.error()));
      auto name = (*materials)->mod_name(mod_id);
      if (!name)
        return std::unexpected(std::move(name.error()));
      return engine.create_string(name->get());
    }

    return lua_nil;
  }

  std::expected<float, StarException> WorldEnvironmentCallbacks::materialHueShift(World& world, Vec2F const& position, String const& layerName) {
    TileLayer layer;
    if (layerName == "foreground") {
      layer = TileLayer::Foreground;
    } else if (layerName == "background") {
      layer = TileLayer::Background;
    } else {
      return std::unexpected(StarException(Star::format("Unsupported material layer {}", layerName)));
    }

    return world.materialHueShift(Vec2I::floor(position), layer);
  }

  std::expected<float, StarException> WorldEnvironmentCallbacks::modHueShift(World& world, Vec2F const& position, String const& layerName) {
    TileLayer layer;
    if (layerName == "foreground") {
      layer = TileLayer::Foreground;
    } else if (layerName == "background") {
      layer = TileLayer::Background;
    } else {
      return std::unexpected(StarException(Star::format("Unsupported material layer {}", layerName)));
    }

    return world.modHueShift(Vec2I::floor(position), layer);
  }

  std::expected<MaterialColorVariant, StarException> WorldEnvironmentCallbacks::materialColor(World& world, Vec2F const& position, String const& layerName) {
    TileLayer layer;
    if (layerName == "foreground") {
      layer = TileLayer::Foreground;
    } else if (layerName == "background") {
      layer = TileLayer::Background;
    } else {
      return std::unexpected(StarException(Star::format("Unsupported material layer {}", layerName)));
    }

    return world.colorVariant(Vec2I::floor(position), layer);
  }

  std::expected<void, StarException> WorldEnvironmentCallbacks::setMaterialColor(World& world, Vec2F const& position, String const& layerName, MaterialColorVariant color) {
    TileLayer layer;
    if (layerName == "foreground") {
      layer = TileLayer::Foreground;
    } else if (layerName == "background") {
      layer = TileLayer::Background;
    } else {
      return std::unexpected(StarException(Star::format("Unsupported material layer {}", layerName)));
    }

    if (auto operation =
          world.modifyTile(Vec2I::floor(position), PlaceMaterialColor{.layer = layer, .color = color}, true);
        !operation)
      return std::unexpected(std::move(operation.error()));
    return {};
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::damageTiles(World& world,
      List<Vec2I> const& arg1,
      String const& arg2,
      Vec2F const& arg3,
      String const& arg4,
      float arg5,
      std::optional<unsigned> const& arg6,
      std::optional<EntityId> sourceEntity) {
    List<Vec2I> const& tilePositions = arg1;

    TileLayer layer;
    auto layerName = arg2;
    if (layerName == "foreground") {
      layer = TileLayer::Foreground;
    } else if (layerName == "background") {
      layer = TileLayer::Background;
    } else {
      return std::unexpected(StarException(Star::format("Unsupported tile layer {}", layerName)));
    }

    unsigned harvestLevel = 999;
    if (arg6)
      harvestLevel = *arg6;

    if (!tile_damage_type_names)
      return std::unexpected(tile_damage_type_names.error());
    auto damageTypePointerResult = tile_damage_type_names->get_left(arg4);
    if (!damageTypePointerResult)
      return std::unexpected(std::move(damageTypePointerResult.error()));
    auto damageTypePointer = std::move(*damageTypePointerResult);
    auto damageType = damageTypePointer.get();
    auto tileDamage = TileDamage(damageType, arg5, harvestLevel);
    auto resResult = world.damageTiles(tilePositions, layer, arg3, tileDamage, sourceEntity);
    if (!resResult)
      return std::unexpected(std::move(resResult.error()));
    auto res = std::move(*resResult);
    return res != TileDamageResult::None;
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::damageTileArea(World& world,
      Vec2F center,
      float radius,
      String layer,
      Vec2F sourcePosition,
      String damageType,
      float damage,
      std::optional<unsigned> const& harvestLevel,
      std::optional<EntityId> sourceEntity) {
    auto tiles = tile_area_brush(radius, center, false);
    return damageTiles(world, tiles, layer, sourcePosition, damageType, damage, harvestLevel, sourceEntity);
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::placeMaterial(World& world, Vec2I const& arg1, String const& arg2, String const& arg3, std::optional<int> const& arg4, bool arg5) {
    auto tilePosition = arg1;

    PlaceMaterial placeMaterial;

    std::string layerName = arg2.utf8();
    auto split = layerName.find_first_of('+');
    if (split != npos) {
      auto overrideName = layerName.substr(split + 1);
      layerName = layerName.substr(0, split);
      if (overrideName == "empty" || overrideName == "none")
        placeMaterial.collisionOverride = TileCollisionOverride::Empty;
      else if (overrideName == "block")
        placeMaterial.collisionOverride = TileCollisionOverride::Block;
      else if (overrideName == "platform")
        placeMaterial.collisionOverride = TileCollisionOverride::Platform;
      else
        return std::unexpected(StarException(Star::format("Unsupported collision override {}", overrideName)));
    }

    if (layerName == "foreground")
      placeMaterial.layer = TileLayer::Foreground;
    else if (layerName == "background")
      placeMaterial.layer = TileLayer::Background;
    else
      return std::unexpected(StarException(Star::format("Unsupported tile layer {}", layerName)));

    auto const& material_name = arg3;
    auto materials = world.root().material_database();
    if (!materials)
      return std::unexpected(std::move(materials.error()));
    if (!(*materials)->is_material_name(material_name))
      return std::unexpected(StarException(Star::format("Unknown material name {}", material_name)));
    auto material_id = (*materials)->material_id(material_name);
    if (!material_id)
      return std::unexpected(std::move(material_id.error()));
    placeMaterial.material = *material_id;

    if (arg4)
      placeMaterial.materialHueShift = static_cast<MaterialHue>(*arg4);

    bool allowOverlap = arg5;

    return world.modifyTile(tilePosition, placeMaterial, allowOverlap);
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::replaceMaterials(World& world,
      List<Vec2I> const& tilePositions,
      String const& layer,
      String const& material_name,
      std::optional<int> const& hueShift,
      bool enableDrops) {
    PlaceMaterial placeMaterial;

    std::string layerName = layer.utf8();
    auto split = layerName.find_first_of('+');
    if (split != npos) {
      auto overrideName = layerName.substr(split + 1);
      layerName = layerName.substr(0, split);
      if (overrideName == "empty" || overrideName == "none")
        placeMaterial.collisionOverride = TileCollisionOverride::Empty;
      else if (overrideName == "block")
        placeMaterial.collisionOverride = TileCollisionOverride::Block;
      else if (overrideName == "platform")
        placeMaterial.collisionOverride = TileCollisionOverride::Platform;
      else
        return std::unexpected(StarException(Star::format("Unsupported collision override {}", overrideName)));
    }

    if (layerName == "foreground")
      placeMaterial.layer = TileLayer::Foreground;
    else if (layerName == "background")
      placeMaterial.layer = TileLayer::Background;
    else
      return std::unexpected(StarException(Star::format("Unsupported tile layer {}", layerName)));

    auto materials = world.root().material_database();
    if (!materials)
      return std::unexpected(std::move(materials.error()));
    if (!(*materials)->is_material_name(material_name))
      return std::unexpected(StarException(Star::format("Unknown material name {}", material_name)));
    auto material_id = (*materials)->material_id(material_name);
    if (!material_id)
      return std::unexpected(std::move(material_id.error()));
    placeMaterial.material = *material_id;

    if (hueShift)
      placeMaterial.materialHueShift = static_cast<MaterialHue>(*hueShift);

    TileModificationList modifications;
    for (auto pos : tilePositions) {
      auto connectableResult = world.isTileConnectable(pos, placeMaterial.layer, true);
      if (!connectableResult)
        return std::unexpected(std::move(connectableResult.error()));
      auto connectable = std::move(*connectableResult);
      if (!connectable)
        continue;
      modifications.emplace_back(pos, placeMaterial);
    }

    if (modifications.empty())
      return true;

    TileDamage damage;
    if (enableDrops) {
      damage.amount = 1.0f;
      damage.harvest_level = 999;
    } else {
      damage.amount = -1.0f;
    }

    auto failedResult = world.replaceTiles(modifications, damage);
    if (!failedResult)
      return std::unexpected(std::move(failedResult.error()));
    auto failed = std::move(*failedResult);
    return failed.empty();
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::replaceMaterialArea(World& world,
      Vec2F center,
      float radius,
      String const& layer,
      String const& materialName,
      std::optional<int> const& hueShift,
      bool enableDrops) {
    auto tiles = tile_area_brush(radius, center, false);
    return replaceMaterials(world, tiles, layer, materialName, hueShift, enableDrops);
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::placeMod(World& world, Vec2I const& arg1, String const& arg2, String const& arg3, std::optional<int> const& arg4, bool arg5) {
    auto tilePosition = arg1;

    PlaceMod placeMod;

    auto layerName = arg2;
    if (layerName == "foreground") {
      placeMod.layer = TileLayer::Foreground;
    } else if (layerName == "background") {
      placeMod.layer = TileLayer::Background;
    } else {
      return std::unexpected(StarException(Star::format("Unsupported tile layer {}", layerName)));
    }

    auto const& mod_name = arg3;
    auto materials = world.root().material_database();
    if (!materials)
      return std::unexpected(std::move(materials.error()));
    if (!(*materials)->is_mod_name(mod_name))
      return std::unexpected(StarException(Star::format("Unknown mod name {}", mod_name)));
    auto mod_id = (*materials)->mod_id(mod_name);
    if (!mod_id)
      return std::unexpected(std::move(mod_id.error()));
    placeMod.mod = *mod_id;

    if (arg4)
      placeMod.modHueShift = static_cast<MaterialHue>(*arg4);

    bool allowOverlap = arg5;

    return world.modifyTile(tilePosition, placeMod, allowOverlap);
  }
}

}
