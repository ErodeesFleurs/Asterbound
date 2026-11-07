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
import star.either;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.memory;
import star.format;
import star.output_proxy;
import star.version;
import star.image;
import star.worker_pool;

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
    [[nodiscard]] std::expected<std::optional<EntityId>, StarException> spawnItem(World& world, Json const& itemType, Vec2F const& worldPosition, std::optional<std::size_t> const& inputCount, Json const& inputParameters, std::optional<Vec2F> const& initialVelocity, std::optional<float> const& intangibleTime);
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
    [[nodiscard]] std::expected<Json, StarException> takeItemDrop(World& world, EntityId entityId, std::optional<EntityId> const& takenBy);
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
    [[nodiscard]] std::expected<bool, StarException> lineTileCollision(World& world, Vec2F const& arg1, Vec2F const& arg2, std::optional<CollisionSet> const& arg3);
    [[nodiscard]] std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> lineTileCollisionPoint(World& world, Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& maybeCollisionSet);
    [[nodiscard]] std::expected<bool, StarException> rectTileCollision(World& world, RectF const& arg1, std::optional<CollisionSet> const& arg2);
    [[nodiscard]] std::expected<bool, StarException> pointCollision(World& world, Vec2F const& point, std::optional<CollisionSet> const& collisionSet);
    [[nodiscard]] std::expected<LuaTupleReturn<std::optional<Vec2F>, std::optional<Vec2F>>, StarException> lineCollision(World& world, Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& maybeCollisionSet);
    [[nodiscard]] std::expected<bool, StarException> polyCollision(World& world, PolyF const& arg1, std::optional<Vec2F> const& arg2, std::optional<CollisionSet> const& arg3);
    [[nodiscard]] std::expected<List<Vec2I>, StarException> collisionBlocksAlongLine(World& world, Vec2F const& arg1, Vec2F const& arg2, std::optional<CollisionSet> const& arg3, std::optional<int> const& arg4);
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
    [[nodiscard]] std::expected<bool, StarException> entityCanDamage(World& world, EntityId sourceId, EntityId targetId);
    [[nodiscard]] std::expected<Json, StarException> entityDamageTeam(World& world, EntityId entityId);
    [[nodiscard]] std::expected<bool, StarException> entityAggressive(World& world, EntityId entityId);
    [[nodiscard]] std::expected<std::optional<LuaString>, StarException> entityType(World& world, LuaEngine& engine, EntityId entityId);
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
    [[nodiscard]] std::expected<RpcPromise<Json>, StarException> sendEntityMessage(World& world, LuaEngine& engine, LuaValue entityId, String const& message, LuaVariadic<Json> args);
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
    [[nodiscard]] std::expected<List<std::pair<Vec2I, LiquidLevel>>, StarException> liquidAlongLine(World& world, Vec2F const& start, Vec2F const& end);
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
          auto entityTypeResult = EntityTypeNames->get_left(type.toString());
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

      if (includedTypes && !includedTypes->contains(entity->entityType()))
        return false;

      if (withoutEntityId && entity->entityId() == *withoutEntityId)
        return false;

      if (callScript) {
        auto scriptedEntity = as<ScriptedEntity>(entity);
        if (!scriptedEntity || !scriptedEntity->isMaster())
          return false;

        auto scriptResult = scriptedEntity->callScript(*callScript, luaUnpack(callScriptArgs));
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
          return geometry.rectIntersectsCircle(
              entity->metaBoundBox().translated(position), radiusQuery->first, radiusQuery->second);
      } else if (boundMode == EntityBoundMode::CollisionArea) {
        // Collision area queries either query based on the collision area if
        // that's given, or as a fallback the regular bound box.
        auto collisionArea = entity->collisionArea();
        if (collisionArea.isNull())
          collisionArea = entity->metaBoundBox();
        collisionArea.translate(position);

        if (lineQuery)
          return geometry.lineIntersectsRect(*lineQuery, collisionArea);
        if (polyQuery)
          return geometry.polyIntersectsPoly(*polyQuery, PolyF(collisionArea));
        if (rectQuery)
          return geometry.rectIntersectsRect(*rectQuery, collisionArea);
        if (radiusQuery)
          return geometry.rectIntersectsCircle(collisionArea, radiusQuery->first, radiusQuery->second);
      } else if (boundMode == EntityBoundMode::Position) {
        if (lineQuery)
          return geometry.lineIntersectsRect(*lineQuery, RectF(position, position));
        if (polyQuery)
          return geometry.polyContains(*polyQuery, position);
        if (rectQuery)
          return geometry.rectContains(*rectQuery, position);
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
      auto queriedResult = world.query<EntityT>(polyQuery->boundBox(), innerSelector);
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
        sortByComputedValue(entities,
            [&world, nearestPosition](std::shared_ptr<EntityT> const& entity) {
              return world.geometry().diff(entity->position(), nearestPosition).magnitude();
            });
      } else if (*order == "random") {
        world.root().random().shuffle(entities);
      } else {
        return std::unexpected(StarException(strf("Unsupported query order {}", order->view())));
      }
    }

    if (entities.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
      return std::unexpected(LuaException("Too many entries for a Lua entity query table"));
    auto table = engine.createTable(static_cast<int>(entities.size()), 0);
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
      auto table = engine.createTable();
      if (!table)
        return std::unexpected(std::move(table.error()));
      options = std::move(*table);
    }
    auto radiusResult = engine.luaMaybeTo<float>(pos2);
    if (!radiusResult)
      return std::unexpected(std::move(radiusResult.error()));
    auto radius = std::move(*radiusResult);
    if (radius) {
      if (auto result = options->set("center", pos1); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = options->set("radius", *radius); !result)
        return std::unexpected(std::move(result.error()));
    } else {
      auto positionResult = engine.luaTo<Vec2F>(pos2);
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
      auto table = engine.createTable();
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

    if (auto result = callbacks.registerCallbackWithSignature<bool, String, Vec2I, std::optional<int>, Json>(
          "placeObject",
          [&world](String const& name, Vec2I const& position, std::optional<int> const& direction,
                   Json const& parameters) {
            return WorldCallbacks::placeObject(world, name, position, direction, parameters);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<std::optional<EntityId>, Json, Vec2F, std::optional<std::size_t>,
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
          callbacks.registerCallbackWithSignature<List<EntityId>, Vec2F, String, float, std::optional<std::uint64_t>>(
            "spawnTreasure",
            [&world](Vec2F const& position, String const& pool, float level, std::optional<std::uint64_t> seed) {
              return WorldCallbacks::spawnTreasure(world, position, pool, level, seed);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<std::optional<EntityId>, String, Vec2F, std::optional<JsonObject>>(
            "spawnMonster",
            [&world](String const& type, Vec2F const& position, std::optional<JsonObject> const& parameters) {
              return WorldCallbacks::spawnMonster(world, type, position, parameters);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<EntityId>, Vec2F, String, String, float,
                                                              std::optional<std::uint64_t>, Json>(
          "spawnNpc",
          [&world](Vec2F const& position, String const& species, String const& type, float level,
                   std::optional<std::uint64_t> seed, Json const& parameters) {
            return WorldCallbacks::spawnNpc(world, position, species, type, level, seed, parameters);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<EntityId>, Vec2F, String, Json>(
          "spawnStagehand",
          [&world](Vec2F const& position, String const& type, Json const& parameters) {
            return WorldCallbacks::spawnStagehand(world, position, type, parameters);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<std::optional<EntityId>, String, Vec2F, std::optional<EntityId>,
                                                  std::optional<Vec2F>, bool, Json>(
            "spawnProjectile",
            [&world](String const& type, Vec2F const& position, std::optional<EntityId> const& source,
                     std::optional<Vec2F> const& direction, bool trackSource, Json const& parameters) {
              return WorldCallbacks::spawnProjectile(world, type, position, source, direction, trackSource, parameters);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<EntityId>, String, Vec2F, Json>(
          "spawnVehicle",
          [&world](String const& type, Vec2F const& position, Json const& parameters) {
            return WorldCallbacks::spawnVehicle(world, type, position, parameters);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, String, Json>(
          "getProperty",
          [&world](String const& name, Json const& def) { return WorldCallbacks::getProperty(world, name, def); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<void, String, Json>(
          "setProperty",
          [&world](String const& name, Json const& value) { return WorldCallbacks::setProperty(world, name, value); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<PlatformerAStar::Path>, Vec2F, Vec2F,
                                                              ActorMovementParameters, PlatformerAStar::Parameters>(
          "findPlatformerPath",
          [&world](Vec2F const& start, Vec2F const& end, ActorMovementParameters&& movement,
                   PlatformerAStar::Parameters&& search) {
            return WorldCallbacks::findPlatformerPath(world, start, end, std::move(movement), std::move(search));
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<std::shared_ptr<PlatformerAStar::PathFinder>, Vec2F, Vec2F,
                                                  ActorMovementParameters, PlatformerAStar::Parameters>(
            "platformerPathStart",
            [&world](Vec2F const& start, Vec2F const& end, ActorMovementParameters&& movement,
                     PlatformerAStar::Parameters&& search) {
              return WorldCallbacks::platformerPathStart(world, start, end, std::move(movement), std::move(search));
            });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallback("name", [&world]() -> std::optional<String> {
        if (auto serverWorld = asRef<WorldServer>(world)) {
          if (auto worldTemplate = serverWorld->get().worldTemplate())
            return worldTemplate->worldName();
        } else if (auto clientWorld = asRef<WorldClient>(world)) {
          if (auto worldTemplate = clientWorld->get().currentTemplate())
            return worldTemplate->worldName();
        }
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallback("type", [&world](LuaEngine& engine) -> std::expected<LuaString, StarException> {
        if (auto serverWorld = asRef<WorldServer>(world)) {
          if (auto worldParameters = serverWorld->get().worldTemplate()->worldParameters())
            return engine.createString(worldParameters->typeName);
        } else if (auto clientWorld = asRef<WorldClient>(world)) {
          if (auto worldParameters = clientWorld->get().currentTemplate()->worldParameters())
            return engine.createString(worldParameters->typeName);
        }
        return engine.createString("unknown");
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallback("inSurfaceLayer", [&world](Vec2I const& position) -> bool {
        if (auto serverWorld = asRef<WorldServer>(world))
          return serverWorld->get().worldTemplate()->inSurfaceLayer(position);
        else if (auto clientWorld = asRef<WorldClient>(world))
          return clientWorld->get().currentTemplate()->inSurfaceLayer(position);
        return false;
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallback("surfaceLevel", [&world]() -> float {
        if (auto serverWorld = asRef<WorldServer>(world))
          return serverWorld->get().worldTemplate()->surfaceLevel();
        else if (auto clientWorld = asRef<WorldClient>(world))
          return clientWorld->get().currentTemplate()->surfaceLevel();
        else
          return world.geometry().size()[1] / 2.0f;
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallback("terrestrial", [&world]() -> bool {
        if (auto serverWorld = asRef<WorldServer>(world)) {
          if (auto worldParameters = serverWorld->get().worldTemplate()->worldParameters())
            return worldParameters->type() == WorldParametersType::TerrestrialWorldParameters;
        } else if (auto clientWorld = asRef<WorldClient>(world)) {
          if (auto worldParameters = clientWorld->get().currentTemplate()->worldParameters())
            return worldParameters->type() == WorldParametersType::TerrestrialWorldParameters;
        }
        return false;
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallback("biomeBlocksAt", [&world](Vec2I position) -> std::expected<std::optional<List<MaterialId>>, StarException> {
        std::shared_ptr<WorldTemplate const> worldTemplate;
        if (auto client = asRef<WorldClient>(world))
          worldTemplate = client->get().currentTemplate();
        else if (auto server = asRef<WorldServer>(world))
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
          List<MaterialId> blocks = {biome->mainBlock};
          blocks.appendAll(biome->subBlocks);
          return blocks;
          }
        }
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));


    if (auto result = callbacks.registerCallback("dungeonId", [&world](Vec2I position) -> std::expected<DungeonId, StarException> {
        if (auto serverWorld = asRef<WorldServer>(world)) {
          return serverWorld->get().dungeonId(position);
        } else {
          auto clientWorld = rconvert<WorldClient>(world);
          if (!clientWorld)
            return std::unexpected(std::move(clientWorld.error()));
          return clientWorld->get().dungeonId(position);
        }
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallback("biomeAt", [&world](Vec2I position) -> std::expected<Json, StarException> {
        std::shared_ptr<WorldTemplate const> worldTemplate;
        if (auto client = asRef<WorldClient>(world))
          worldTemplate = client->get().currentTemplate();
        else if (auto server = asRef<WorldServer>(world))
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
          return biome->toJson();
          }
        }
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallback("blockInfoAt", [&world](Vec2I position) -> std::expected<Json, StarException> {
        std::shared_ptr<WorldTemplate const> worldTemplate;
        if (auto client = asRef<WorldClient>(world))
          worldTemplate = client->get().currentTemplate();
        else if (auto server = asRef<WorldServer>(world))
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

    if (auto clientWorld = asRef<WorldClient>(world)) {
      if (auto result = callbacks.registerCallback("inWorld", [clientWorld]() { return clientWorld->get().inWorld(); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("mainPlayer", [clientWorld]() -> std::optional<int> {
        if (clientWorld->get().isHeadless()) {
          return {};
        } else {
          return clientWorld->get().clientState().playerId(); 
        }
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("headless", [clientWorld]() { return clientWorld->get().isHeadless();  }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("subWorldId", [clientWorld]() { return clientWorld->get().subWorldId();  }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("isClient", []() { return true;  }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("isServer", []() { return false; }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("latency", [clientWorld]() { return clientWorld->get().latency(); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<void, EntityId>(
            "resendEntity",
            [clientWorld](EntityId entity) { return ClientWorldCallbacks::resendEntity(clientWorld->get(), entity); });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<RectI>(
            "clientWindow", [clientWorld]() { return ClientWorldCallbacks::clientWindow(clientWorld->get()); });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("players", [clientWorld]() -> std::expected<List<EntityId>, StarException> {
        List<EntityId> playerIds;
        auto queried = clientWorld->get().forAllEntities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
          if (entity->entityType() == EntityType::Player)
            playerIds.emplace_back(entity->entityId());
          return {};
        });
        if (!queried)
          return std::unexpected(std::move(queried.error()));
        return playerIds;
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("callScriptContext", [clientWorld](String const& contextName, String const& function, LuaVariadic<LuaValue> const& args) -> std::expected<std::optional<LuaValue>, StarException> {
        auto context = clientWorld->get().scriptContext(contextName);
        if (!context)
          return std::unexpected(StarException::format("Context {} does not exist", contextName));
        return context->invoke(function, args);
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("unload", [clientWorld]() -> std::expected<void, StarException> {
        if (!clientWorld->get().isHeadless())
          return std::unexpected(StarException::format("Non-headless worlds cannot be unloaded."));
        clientWorld->get().requestDestroy();
        return {};
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("template", [clientWorld]() {
        return clientWorld->get().currentTemplate()->store();
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result =
            callbacks.registerCallback("setTemplate",
                                       [clientWorld](Json worldTemplate) -> std::expected<void, StarException> {
                                         clientWorld->get().setTemplate(std::move(worldTemplate));
                                         return {};
                                       });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("wire", [clientWorld](Vec2I outputPosition, std::size_t outputIndex, Vec2I inputPosition, std::size_t inputIndex) -> std::expected<void, StarException> {
        clientWorld->get().wire(outputPosition, outputIndex, inputPosition, inputIndex);
        return {};
      }); !result)
        return std::unexpected(std::move(result.error()));
    }

    if (auto serverWorld = asRef<WorldServer>(world)) {
      if (auto result = callbacks.registerCallback("isClient", []() { return false; }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("isServer", []() { return true;  }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallbackWithSignature<String>(
            "id", [serverWorld]() { return ServerWorldCallbacks::id(serverWorld->get()); });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<bool, EntityId, bool>(
            "breakObject",
            [serverWorld](EntityId entity, bool smash) {
              return ServerWorldCallbacks::breakObject(serverWorld->get(), entity, smash);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<bool, RectF>(
            "isVisibleToPlayer",
            [serverWorld](RectF const& region) {
              return ServerWorldCallbacks::isVisibleToPlayer(serverWorld->get(), region);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<bool, RectF>("loadRegion",
                                                                             [serverWorld](RectF const& region) {
                                                                               return ServerWorldCallbacks::loadRegion(
                                                                                 serverWorld->get(), region);
                                                                             });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<bool, RectF>(
            "regionActive",
            [serverWorld](RectF const& region) {
              return ServerWorldCallbacks::regionActive(serverWorld->get(), region);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<void, DungeonId, bool>(
            "setTileProtection",
            [serverWorld](DungeonId dungeon, bool protect) {
              return ServerWorldCallbacks::setTileProtection(serverWorld->get(), dungeon, protect);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<bool, RectI>(
            "isPlayerModified",
            [serverWorld](RectI const& region) {
              return ServerWorldCallbacks::isPlayerModified(serverWorld->get(), region);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<std::optional<LiquidLevel>, Vec2F>(
            "forceDestroyLiquid",
            [serverWorld](Vec2F const& position) {
              return ServerWorldCallbacks::forceDestroyLiquid(serverWorld->get(), position);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<EntityId, String>(
            "loadUniqueEntity",
            [serverWorld](String const& uniqueId) {
              return ServerWorldCallbacks::loadUniqueEntity(serverWorld->get(), uniqueId);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<void, EntityId, String>(
            "setUniqueId",
            [serverWorld](EntityId entity, String&& uniqueId) {
              return ServerWorldCallbacks::setUniqueId(serverWorld->get(), entity, std::move(uniqueId));
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, std::optional<EntityId>>(
            "takeItemDrop",
            [&world](EntityId entity, std::optional<EntityId> const& takenBy) {
              return ServerWorldCallbacks::takeItemDrop(world, entity, takenBy);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<void, Vec2F, std::optional<bool>>(
            "setPlayerStart",
            [serverWorld](Vec2F const& position, std::optional<bool> respawn) {
              return ServerWorldCallbacks::setPlayerStart(serverWorld->get(), position, respawn);
            });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<List<EntityId>>(
            "players", [serverWorld]() { return ServerWorldCallbacks::players(serverWorld->get()); });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<LuaString, LuaEngine&>(
            "fidelity",
            [serverWorld](LuaEngine& engine) { return ServerWorldCallbacks::fidelity(serverWorld->get(), engine); });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result =
            callbacks.registerCallbackWithSignature<std::optional<LuaValue>, String, String, LuaVariadic<LuaValue>>(
              "callScriptContext",
              [serverWorld](String const& context, String const& function, LuaVariadic<LuaValue> const& args) {
                return ServerWorldCallbacks::callScriptContext(serverWorld->get(), context, function, args);
              });
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<bool, ConnectionId, String, Json>(
            "sendPacket",
            [serverWorld](ConnectionId client, String const& type, Json const& data) {
              return ServerWorldCallbacks::sendPacket(serverWorld->get(), client, type, data);
            });
          !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallbackWithSignature<double>("skyTime", [serverWorld]() {
          return serverWorld->get().sky()->epochTime();
        }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallbackWithSignature<void, double>("setSkyTime", [serverWorld](double skyTime) {
          return serverWorld->get().sky()->setEpochTime(skyTime);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallback("expiryTime", [serverWorld]() -> float { return serverWorld->get().expiryTime(); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("setExpiryTime", [serverWorld](float expiryTime) { return serverWorld->get().setExpiryTime(expiryTime); }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallback("wire", [serverWorld](Vec2I outputPosition, std::size_t outputIndex, Vec2I inputPosition, std::size_t inputIndex) -> std::expected<void, StarException> {
        return serverWorld->get().wire(outputPosition, outputIndex, inputPosition, inputIndex);
      }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallback("flyingType", [serverWorld]() -> std::expected<String, StarException> {
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
      if (auto result = callbacks.registerCallback("warpPhase", [serverWorld]() -> std::expected<String, StarException> {
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
      if (auto result = callbacks.registerCallback("setUniverseFlag", [serverWorld](String flagName) { return serverWorld->get().universeSettings()->setFlag(flagName); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("universeFlags", [serverWorld]() { return serverWorld->get().universeSettings()->flags(); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("universeFlagSet", [serverWorld](String const& flagName) { return serverWorld->get().universeSettings()->flags().contains(flagName); }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("placeDungeon", [serverWorld](String dungeonName, Vec2I position, std::optional<DungeonId> dungeonId) -> std::expected<bool, StarException> {
        return serverWorld->get().placeDungeon(dungeonName, position, dungeonId);
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("tryPlaceDungeon", [serverWorld](String dungeonName, Vec2I position, std::optional<DungeonId> dungeonId) -> std::expected<bool, StarException> {
        return serverWorld->get().placeDungeon(dungeonName, position, dungeonId, false);
      }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallback("addBiomeRegion", [serverWorld](Vec2I position, String biomeName, String subBlockSelector, int width) {
          return serverWorld->get().addBiomeRegion(position, biomeName, subBlockSelector, width);
        }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("expandBiomeRegion", [serverWorld](Vec2I position, int width) {
          return serverWorld->get().expandBiomeRegion(position, width);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallback("pregenerateAddBiome", [serverWorld](Vec2I position, int width) {
          return serverWorld->get().pregenerateAddBiome(position, width);
        }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("pregenerateExpandBiome", [serverWorld](Vec2I position, int width) {
          return serverWorld->get().pregenerateExpandBiome(position, width);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallback("setLayerEnvironmentBiome", [serverWorld](Vec2I position) {
          return serverWorld->get().setLayerEnvironmentBiome(position);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallback("setPlanetType", [serverWorld](String planetType, String primaryBiomeName) {
          return serverWorld->get().setPlanetType(planetType, primaryBiomeName);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallback("setDungeonGravity", [serverWorld](DungeonId dungeonId, std::optional<float> gravity) {
          return serverWorld->get().setDungeonGravity(dungeonId, gravity);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallback("setDungeonBreathable", [serverWorld](DungeonId dungeonId, std::optional<bool> breathable) {
          return serverWorld->get().setDungeonBreathable(dungeonId, breathable);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallback("setDungeonId", [serverWorld](RectI tileRegion, DungeonId dungeonId) {
          return serverWorld->get().setDungeonId(tileRegion, dungeonId);
        }); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = callbacks.registerCallback("enqueuePlacement", [serverWorld](List<Json> distributionConfigs, std::optional<DungeonId> id) -> std::expected<RpcPromise<Vec2I>, StarException> {
        List<BiomeItemDistribution> distributions;
        distributions.reserve(distributionConfigs.size());
        for (auto const& config : distributionConfigs) {
          auto distributionResult = BiomeItemDistribution::create(serverWorld->get().root(), config, serverWorld->get().root().random().randu64());
          if (!distributionResult)
            return std::unexpected(std::move(distributionResult.error()));
          auto distribution = std::move(*distributionResult);
          distributions.append(std::move(distribution));
        }
        return serverWorld->get().enqueuePlacement(std::move(distributions), id);
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("template", [serverWorld]() {
        return serverWorld->get().worldTemplate()->store();
      }); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = callbacks.registerCallback("setTemplate", [serverWorld](Json worldTemplate) -> std::expected<void, StarException> {
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
    if (auto result = callbacks.registerCallbackWithSignature<void, Vec2F, Color>(
          "debugPoint",
          [&root](Vec2F const& position, Color const& color) {
            return WorldDebugCallbacks::debugPoint(root, position, color);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<void, Vec2F, Vec2F, Color>(
          "debugLine",
          [&root](Vec2F const& start, Vec2F const& end, Color const& color) {
            return WorldDebugCallbacks::debugLine(root, start, end, color);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<void, PolyF, Color>(
          "debugPoly",
          [&root](PolyF const& poly, Color const& color) { return WorldDebugCallbacks::debugPoly(root, poly, color); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<void, LuaEngine&, LuaVariadic<LuaValue>>(
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
      view.set("position", jsonFromVec2F(entity.position()));
      auto team = entity.getTeam().toJson();
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
    if (auto result = callbacks.registerCallback("entityDamageBar", [&world, damageBarView](EntityId id) -> std::expected<Json, StarException> {
      if (auto entity = world.get<DamageBarEntity>(id))
        return damageBarView(*entity, true);
      return Json();
    }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallback("damageBarEntities",
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
    if (auto result = callbacks.registerCallback("entityPortraitView",
        [&world, portraitCache](LuaEngine& engine, EntityId id, String const& modeName,
            std::optional<std::uint64_t> knownRevision) -> std::expected<std::optional<LuaTable>, StarException> {
      auto entity = world.get<PortraitEntity>(id);
      if (!entity) return {};
      auto mode = enumValue<PortraitModeNames>(modeName);
      if (!mode) return std::unexpected(std::move(mode.error()));
      auto portrait = entity->portrait(*mode);
      if (!portrait) return std::unexpected(std::move(portrait.error()));
      auto& cache = *portraitCache;
      if (cache.entity.lock() != entity || cache.mode != modeName || cache.drawables != *portrait) {
        auto metadata = world.root().imageMetadataDatabase();
        if (!metadata) return std::unexpected(std::move(metadata.error()));
        auto bounds = Drawable::boundBoxAll(**metadata, *portrait, true);
        if (!bounds) return std::unexpected(std::move(bounds.error()));
        cache.entity = entity;
        cache.mode = modeName;
        cache.drawables = std::move(*portrait);
        cache.bounds = *bounds;
        ++cache.revision;
      }
      auto view = engine.createTable(0, 3);
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
          callbacks.registerCallbackWithSignature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "entityQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::entityQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "monsterQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::monsterQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "npcQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::npcQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "objectQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::objectQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "itemDropQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::itemDropQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "playerQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::playerQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<LuaTable, LuaEngine&, Vec2F, LuaValue, std::optional<LuaTable>>(
            "loungeableQuery",
            [&world](LuaEngine& engine, Vec2F const& position, LuaValue const& bound,
                     std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::loungeableQuery(world, engine, position, bound, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<LuaTable, LuaEngine&, Vec2F, Vec2F, std::optional<LuaTable>>(
            "entityLineQuery",
            [&world](LuaEngine& engine, Vec2F const& start, Vec2F const& end, std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::entityLineQuery(world, engine, start, end, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<LuaTable, LuaEngine&, Vec2F, Vec2F, std::optional<LuaTable>>(
            "objectLineQuery",
            [&world](LuaEngine& engine, Vec2F const& start, Vec2F const& end, std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::objectLineQuery(world, engine, start, end, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<LuaTable, LuaEngine&, Vec2F, Vec2F, std::optional<LuaTable>>(
            "npcLineQuery",
            [&world](LuaEngine& engine, Vec2F const& start, Vec2F const& end, std::optional<LuaTable>&& options) {
              return WorldEntityCallbacks::npcLineQuery(world, engine, start, end, std::move(options));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallback("objectAt",
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

    if (auto result = callbacks.registerCallbackWithSignature<std::optional<std::shared_ptr<Entity>>, EntityId>("entity", [&world](EntityId entityId) -> std::optional<std::shared_ptr<Entity>> {
      if (auto entity = world.entity(entityId)) {
        return entity;
      } else {
        return {};
      }
    }); !result)
      return std::unexpected(std::move(result.error()));
    
    if (auto result = callbacks.registerCallback("entities", [&world](LuaEngine& engine, std::optional<LuaTable> options) -> std::expected<LuaTable, StarException> {
      if (!options) {
        auto table = engine.createTable();
        if (!table)
          return std::unexpected(std::move(table.error()));
        options = std::move(*table);
      }
      return entityQueryImpl<Entity>(world, engine, *options, {});
    }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallbackWithSignature<bool, int>(
          "entityExists", [&world](int entity) { return WorldEntityCallbacks::entityExists(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, int, int>(
          "entityCanDamage",
          [&world](int source, int target) { return WorldEntityCallbacks::entityCanDamage(world, source, target); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId>(
          "entityDamageTeam",
          [&world](EntityId entity) { return WorldEntityCallbacks::entityDamageTeam(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, EntityId>(
          "entityAggressive",
          [&world](EntityId entity) { return WorldEntityCallbacks::entityAggressive(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<LuaString>, LuaEngine&, int>(
          "entityType",
          [&world](LuaEngine& engine, int entity) { return WorldEntityCallbacks::entityType(world, engine, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<Vec2F>, int>(
          "entityPosition", [&world](int entity) { return WorldEntityCallbacks::entityPosition(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<Vec2F>, int>(
          "entityVelocity", [&world](int entity) { return WorldEntityCallbacks::entityVelocity(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<RectF>, int>(
          "entityMetaBoundBox",
          [&world](int entity) { return WorldEntityCallbacks::entityMetaBoundBox(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<std::uint64_t>, EntityId, String>(
          "entityCurrency",
          [&world](EntityId entity, String const& currency) {
            return WorldEntityCallbacks::entityCurrency(world, entity, currency);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<std::optional<std::uint64_t>, EntityId, Json, std::optional<bool>>(
            "entityHasCountOfItem",
            [&world](EntityId entity, Json&& descriptor, std::optional<bool> exactMatch) {
              return WorldEntityCallbacks::entityHasCountOfItem(world, entity, std::move(descriptor), exactMatch);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<Vec2F>, EntityId>(
          "entityHealth", [&world](EntityId entity) { return WorldEntityCallbacks::entityHealth(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<String>, EntityId>(
          "entitySpecies", [&world](EntityId entity) { return WorldEntityCallbacks::entitySpecies(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<String>, EntityId>(
          "entityGender", [&world](EntityId entity) { return WorldEntityCallbacks::entityGender(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<String>, EntityId>(
          "entityName", [&world](EntityId entity) { return WorldEntityCallbacks::entityName(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<Json>, EntityId>(
          "entityNametag", [&world](EntityId entity) { return WorldEntityCallbacks::entityNametag(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<String>, EntityId, std::optional<String>>(
          "entityDescription",
          [&world](EntityId entity, std::optional<String> const& species) {
            return WorldEntityCallbacks::entityDescription(world, entity, species);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<List<Drawable>>, EntityId, String>(
          "entityPortrait",
          [&world](EntityId entity, String const& mode) {
            return WorldEntityCallbacks::entityPortrait(world, entity, mode);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<String>, EntityId, String>(
          "entityHandItem",
          [&world](EntityId entity, String const& hand) {
            return WorldEntityCallbacks::entityHandItem(world, entity, hand);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, String>(
          "entityHandItemDescriptor",
          [&world](EntityId entity, String const& hand) {
            return WorldEntityCallbacks::entityHandItemDescriptor(world, entity, hand);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<String>, EntityId>(
          "entityUniqueId", [&world](EntityId entity) { return WorldEntityCallbacks::entityUniqueId(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, String, std::optional<Json>>(
          "getObjectParameter",
          [&world](EntityId entity, String const& name, std::optional<Json> const& def) {
            return WorldEntityCallbacks::getObjectParameter(world, entity, name, def);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, String, std::optional<Json>>(
          "getNpcScriptParameter",
          [&world](EntityId entity, String const& name, std::optional<Json> const& def) {
            return WorldEntityCallbacks::getNpcScriptParameter(world, entity, name, def);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<List<Vec2I>, EntityId>(
          "objectSpaces", [&world](EntityId entity) { return WorldEntityCallbacks::objectSpaces(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<int>, EntityId>(
          "farmableStage", [&world](EntityId entity) { return WorldEntityCallbacks::farmableStage(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<int>, EntityId>(
          "containerSize", [&world](EntityId entity) { return WorldEntityCallbacks::containerSize(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, EntityId>(
          "containerClose", [&world](EntityId entity) { return WorldEntityCallbacks::containerClose(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, EntityId>(
          "containerOpen", [&world](EntityId entity) { return WorldEntityCallbacks::containerOpen(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId>(
          "containerItems", [&world](EntityId entity) { return WorldEntityCallbacks::containerItems(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, std::size_t>(
          "containerItemAt",
          [&world](EntityId entity, std::size_t offset) {
            return WorldEntityCallbacks::containerItemAt(world, entity, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<bool>, EntityId, Json>(
          "containerConsume",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerConsume(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<bool>, EntityId, std::size_t, int>(
          "containerConsumeAt",
          [&world](EntityId entity, std::size_t offset, int count) {
            return WorldEntityCallbacks::containerConsumeAt(world, entity, offset, count);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<std::size_t>, EntityId, Json>(
          "containerAvailable",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerAvailable(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId>(
          "containerTakeAll",
          [&world](EntityId entity) { return WorldEntityCallbacks::containerTakeAll(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, std::size_t>(
          "containerTakeAt",
          [&world](EntityId entity, std::size_t offset) {
            return WorldEntityCallbacks::containerTakeAt(world, entity, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, std::size_t, int>(
          "containerTakeNumItemsAt",
          [&world](EntityId entity, std::size_t offset, int count) {
            return WorldEntityCallbacks::containerTakeNumItemsAt(world, entity, offset, count);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<std::size_t>, EntityId, Json>(
          "containerItemsCanFit",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerItemsCanFit(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, Json>(
          "containerItemsFitWhere",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerItemsFitWhere(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, Json>(
          "containerAddItems",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerAddItems(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, Json>(
          "containerStackItems",
          [&world](EntityId entity, Json const& items) {
            return WorldEntityCallbacks::containerStackItems(world, entity, items);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, Json, std::size_t>(
          "containerPutItemsAt",
          [&world](EntityId entity, Json const& items, std::size_t offset) {
            return WorldEntityCallbacks::containerPutItemsAt(world, entity, items, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, Json, std::size_t>(
          "containerSwapItems",
          [&world](EntityId entity, Json const& items, std::size_t offset) {
            return WorldEntityCallbacks::containerSwapItems(world, entity, items, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, Json, std::size_t>(
          "containerSwapItemsNoCombine",
          [&world](EntityId entity, Json const& items, std::size_t offset) {
            return WorldEntityCallbacks::containerSwapItemsNoCombine(world, entity, items, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Json, EntityId, Json, std::size_t>(
          "containerItemApply",
          [&world](EntityId entity, Json const& items, std::size_t offset) {
            return WorldEntityCallbacks::containerItemApply(world, entity, items, offset);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<std::optional<LuaValue>, EntityId, String, LuaVariadic<LuaValue>>(
            "callScriptedEntity",
            [&world](EntityId entity, String const& function, LuaVariadic<LuaValue> const& args) {
              return WorldEntityCallbacks::callScriptedEntity(world, entity, function, args);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<RpcPromise<Vec2F>, String>(
          "findUniqueEntity",
          [&world](String const& uniqueId) { return WorldEntityCallbacks::findUniqueEntity(world, uniqueId); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<RpcPromise<Json>, LuaEngine&, LuaValue, String, LuaVariadic<Json>>(
            "sendEntityMessage",
            [&world](LuaEngine& engine, LuaValue&& entity, String const& message, LuaVariadic<Json>&& args) {
              return WorldEntityCallbacks::sendEntityMessage(world, engine, std::move(entity), message,
                                                             std::move(args));
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<std::optional<List<EntityId>>, EntityId, std::optional<std::size_t>>(
            "loungingEntities",
            [&world](EntityId entity, std::optional<std::size_t> anchor) {
              return WorldEntityCallbacks::loungingEntities(world, entity, anchor);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<std::optional<bool>, EntityId, std::optional<std::size_t>>(
            "loungeableOccupied",
            [&world](EntityId entity, std::optional<std::size_t> anchor) {
              return WorldEntityCallbacks::loungeableOccupied(world, entity, anchor);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<std::size_t>, EntityId>(
          "loungeableAnchorCount",
          [&world](EntityId entity) { return WorldEntityCallbacks::loungeableAnchorCount(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, EntityId, std::optional<bool>>(
          "isMonster",
          [&world](EntityId entity, std::optional<bool> const& aggressive) {
            return WorldEntityCallbacks::isMonster(world, entity, aggressive);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<String>, EntityId>(
          "monsterType", [&world](EntityId entity) { return WorldEntityCallbacks::monsterType(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<String>, EntityId>(
          "npcType", [&world](EntityId entity) { return WorldEntityCallbacks::npcType(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<String>, EntityId>(
          "stagehandType", [&world](EntityId entity) { return WorldEntityCallbacks::stagehandType(world, entity); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, EntityId, std::optional<int>>(
          "isNpc",
          [&world](EntityId entity, std::optional<int> const& team) {
            return WorldEntityCallbacks::isNpc(world, entity, team);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallback("isEntityInteractive", [&world](EntityId entityId) -> std::optional<bool> {
        if (auto entity = world.get<InteractiveEntity>(entityId))
          return entity->isInteractive();
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallback("entityAimPosition", [&world](EntityId entityId) -> std::expected<std::optional<Vec2F>, StarException> {
        if (auto entity = world.get<ToolUserEntity>(entityId)) {
          auto position = entity->aimPosition();
          if (!position)
            return std::unexpected(std::move(position.error()));
          return *position;
        }
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallback("entityMouthPosition", [&world](EntityId entityId) -> std::expected<std::optional<Vec2F>, StarException> {
        if (auto entity = world.get<ChattyEntity>(entityId)) {
          auto positionResult = entity->mouthPosition();
          if (!positionResult)
            return std::unexpected(std::move(positionResult.error()));
          auto position = std::move(*positionResult);
          return position;
        }
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallback("entityTypeName", [&world](EntityId entityId) -> std::expected<std::optional<String>, StarException> {
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
    if (auto result = callbacks.registerCallback("itemDropItem", [&world](EntityId const& entityId) -> Json {
        if (auto itemDrop = world.get<ItemDrop>(entityId))
          return itemDrop->item()->descriptor().toJson();
        return {};
      }); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }

  std::expected<void, StarException> addWorldEnvironmentCallbacks(LuaCallbacks& callbacks, World& world) {
    if (auto result = callbacks.registerCallbackWithSignature<float, Vec2F>(
          "lightLevel",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::lightLevel(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<float, Vec2F>(
          "windLevel",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::windLevel(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, Vec2F>(
          "breathable",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::breathable(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, Vec2F>(
          "underground",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::underground(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<LuaValue, LuaEngine&, Vec2F, String>(
          "material",
          [&world](LuaEngine& engine, Vec2F const& position, String const& layer) {
            return WorldEnvironmentCallbacks::material(world, engine, position, layer);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<LuaValue, LuaEngine&, Vec2F, String>(
          "mod",
          [&world](LuaEngine& engine, Vec2F const& position, String const& layer) {
            return WorldEnvironmentCallbacks::mod(world, engine, position, layer);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<float, Vec2F, String>(
          "materialHueShift",
          [&world](Vec2F const& position, String const& layer) {
            return WorldEnvironmentCallbacks::materialHueShift(world, position, layer);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<float, Vec2F, String>(
          "modHueShift",
          [&world](Vec2F const& position, String const& layer) {
            return WorldEnvironmentCallbacks::modHueShift(world, position, layer);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<MaterialColorVariant, Vec2F, String>(
          "materialColor",
          [&world](Vec2F const& position, String const& layer) {
            return WorldEnvironmentCallbacks::materialColor(world, position, layer);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<void, Vec2F, String, MaterialColorVariant>(
          "setMaterialColor",
          [&world](Vec2F const& position, String const& layer, MaterialColorVariant color) {
            return WorldEnvironmentCallbacks::setMaterialColor(world, position, layer, color);
          });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallback("oceanLevel", [&world](Vec2I position) -> std::expected<int, StarException> {
        std::shared_ptr<WorldTemplate const> worldTemplate;
        if (auto server = asRef<WorldServer>(world))
          worldTemplate = server->get().worldTemplate();
        else {
          auto client = rconvert<WorldClient>(world);
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

    if (auto result = callbacks.registerCallback("environmentStatusEffects", [&world](Vec2F const& position) {
        return world.environmentStatusEffects(position);
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallback("weatherStatusEffects", [&world](Vec2F const& position) {
        return world.weatherStatusEffects(position);
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallback("exposedToWeather", [&world](Vec2F const& position) {
        return world.exposedToWeather(position);
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallback("activeWeather", [&world](Vec2F const& position) {
        return world.activeWeather(position);
      }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallbackWithSignature<bool, List<Vec2I>, String, Vec2F, String, float,
                                                              std::optional<unsigned>, std::optional<EntityId>>(
          "damageTiles",
          [&world](List<Vec2I> const& positions, String const& layer, Vec2F const& source, String const& type,
                   float damage, std::optional<unsigned> const& harvestLevel, std::optional<EntityId> sourceEntity) {
            return WorldEnvironmentCallbacks::damageTiles(world, positions, layer, source, type, damage, harvestLevel,
                                                          sourceEntity);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, Vec2F, float, String, Vec2F, String, float,
                                                              std::optional<unsigned>, std::optional<EntityId>>(
          "damageTileArea",
          [&world](Vec2F center, float radius, String&& layer, Vec2F source, String&& type, float damage,
                   std::optional<unsigned> const& harvestLevel, std::optional<EntityId> sourceEntity) {
            return WorldEnvironmentCallbacks::damageTileArea(world, center, radius, std::move(layer), source,
                                                             std::move(type), damage, harvestLevel, sourceEntity);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, Vec2I, String, String, std::optional<int>, bool>(
          "placeMaterial",
          [&world](Vec2I const& position, String const& layer, String const& material, std::optional<int> const& hue,
                   bool allowOverlap) {
            return WorldEnvironmentCallbacks::placeMaterial(world, position, layer, material, hue, allowOverlap);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<bool, List<Vec2I>, String, String, std::optional<int>, bool>(
            "replaceMaterials",
            [&world](List<Vec2I> const& positions, String const& layer, String const& material,
                     std::optional<int> const& hue, bool enableDrops) {
              return WorldEnvironmentCallbacks::replaceMaterials(world, positions, layer, material, hue, enableDrops);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<bool, Vec2F, float, String, String, std::optional<int>, bool>(
            "replaceMaterialArea",
            [&world](Vec2F center, float radius, String const& layer, String const& material,
                     std::optional<int> const& hue, bool enableDrops) {
              return WorldEnvironmentCallbacks::replaceMaterialArea(world, center, radius, layer, material, hue,
                                                                    enableDrops);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, Vec2I, String, String, std::optional<int>, bool>(
          "placeMod",
          [&world](Vec2I const& position, String const& layer, String const& mod, std::optional<int> const& hue,
                   bool allowOverlap) {
            return WorldEnvironmentCallbacks::placeMod(world, position, layer, mod, hue, allowOverlap);
          });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallback(
          "radialTileQuery",
          [&world](Vec2F center, float radius, String layerName) -> std::expected<List<Vec2I>, StarException> {
            auto layerPointerResult = enumValue<TileLayerNames>(layerName);
            if (!layerPointerResult)
              return std::unexpected(std::move(layerPointerResult.error()));
            auto layerPointer = std::move(*layerPointerResult);
            auto layer = layerPointer;
            List<Vec2I> tiles;
            for (auto const& tile : tileAreaBrush(radius, center, false)) {
              if (world.material(tile, layer) != EmptyMaterialId)
                tiles.append(tile);
            }
            return tiles;
          });
        !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = callbacks.registerCallbackWithSignature<List<std::pair<Vec2I, LiquidLevel>>, Vec2F, Vec2F>(
          "liquidAlongLine",
          [&world](Vec2F const& start, Vec2F const& end) {
            return WorldEnvironmentCallbacks::liquidAlongLine(world, start, end);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, Vec2I, std::optional<bool>, std::optional<bool>>(
          "tileIsOccupied",
          [&world](Vec2I const& position, std::optional<bool> const& foreground,
                   std::optional<bool> const& background) {
            return WorldEnvironmentCallbacks::tileIsOccupied(world, position, foreground, background);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<float>(
          "threatLevel", [&world]() -> decltype(auto) { return world.threatLevel(); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<double>(
          "time", [&world]() { return WorldEnvironmentCallbacks::time(world); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::uint64_t>(
          "day", [&world]() { return WorldEnvironmentCallbacks::day(world); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<double>(
          "timeOfDay", [&world]() { return WorldEnvironmentCallbacks::timeOfDay(world); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<float>(
          "dayLength", [&world]() { return WorldEnvironmentCallbacks::dayLength(world); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<LiquidLevel>, std::variant<RectF, Vec2I>>(
          "liquidAt",
          [&world](std::variant<RectF, Vec2I>&& bound) {
            return WorldEnvironmentCallbacks::liquidAt(world, std::move(bound));
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<float, Vec2F>(
          "gravity", [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::gravity(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, Vec2F, LiquidId, float>(
          "spawnLiquid",
          [&world](Vec2F const& position, LiquidId liquid, float amount) {
            return WorldEnvironmentCallbacks::spawnLiquid(world, position, liquid, amount);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<LiquidLevel>, Vec2F>(
          "destroyLiquid",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::destroyLiquid(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, Vec2F>(
          "isTileProtected",
          [&world](Vec2F const& position) { return WorldEnvironmentCallbacks::isTileProtected(world, position); });
        !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  
  std::expected<void, StarException> addWorldGeometryCallbacks(LuaCallbacks& callbacks, World& world) {

    if (auto result = callbacks.registerCallback("size", [&world]() -> Vec2U {
        return world.geometry().size();
      }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<float, Vec2F, std::optional<Vec2F>>(
          "magnitude",
          [&world](Vec2F position, std::optional<Vec2F> other) {
            return WorldGeometryCallbacks::magnitude(world, position, other);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<Vec2F, Vec2F, Vec2F>(
          "distance",
          [&world](Vec2F const& start, Vec2F const& end) {
            return WorldGeometryCallbacks::distance(world, start, end);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, PolyF, Vec2F>(
          "polyContains",
          [&world](PolyF const& poly, Vec2F const& position) {
            return WorldGeometryCallbacks::polyContains(world, poly, position);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<LuaValue, LuaEngine&, LuaValue>(
          "xwrap",
          [&world](LuaEngine& engine, LuaValue const& position) {
            return WorldGeometryCallbacks::xwrap(world, engine, position);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<LuaValue, LuaEngine&, std::variant<Vec2F, float>,
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
    if (auto result = callbacks.registerCallbackWithSignature<bool, RectF, std::optional<CollisionSet>>(
          "rectCollision",
          [&world](RectF const& rect, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::rectCollision(world, rect, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, Vec2F, std::optional<CollisionSet>>(
          "pointTileCollision",
          [&world](Vec2F const& point, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::pointTileCollision(world, point, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, Vec2F, Vec2F, std::optional<CollisionSet>>(
          "lineTileCollision",
          [&world](Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::lineTileCollision(world, start, end, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<std::pair<Vec2F, Vec2I>>, Vec2F, Vec2F,
                                                              std::optional<CollisionSet>>(
          "lineTileCollisionPoint",
          [&world](Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::lineTileCollisionPoint(world, start, end, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, RectF, std::optional<CollisionSet>>(
          "rectTileCollision",
          [&world](RectF const& rect, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::rectTileCollision(world, rect, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<bool, Vec2F, std::optional<CollisionSet>>(
          "pointCollision",
          [&world](Vec2F const& point, std::optional<CollisionSet> const& kinds) {
            return WorldCollisionCallbacks::pointCollision(world, point, kinds);
          });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<LuaTupleReturn<std::optional<Vec2F>, std::optional<Vec2F>>, Vec2F,
                                                  Vec2F, std::optional<CollisionSet>>(
            "lineCollision",
            [&world](Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& kinds) {
              return WorldCollisionCallbacks::lineCollision(world, start, end, kinds);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks.registerCallbackWithSignature<bool, PolyF, std::optional<Vec2F>, std::optional<CollisionSet>>(
            "polyCollision",
            [&world](PolyF const& poly, std::optional<Vec2F> const& position,
                     std::optional<CollisionSet> const& kinds) {
              return WorldCollisionCallbacks::polyCollision(world, poly, position, kinds);
            });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result =
          callbacks
            .registerCallbackWithSignature<List<Vec2I>, Vec2F, Vec2F, std::optional<CollisionSet>, std::optional<int>>(
              "collisionBlocksAlongLine",
              [&world](Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& kinds,
                       std::optional<int> const& limit) {
                return WorldCollisionCallbacks::collisionBlocksAlongLine(world, start, end, kinds, limit);
              });
        !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = callbacks.registerCallbackWithSignature<std::optional<Vec2F>, PolyF, Vec2F, float,
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
    return world.geometry().polyContains(poly, pos);
  }

  std::expected<LuaValue, StarException> WorldGeometryCallbacks::xwrap(World& world, LuaEngine& engine, LuaValue const& positionOrX) {
    auto xResult = engine.luaMaybeTo<float>(positionOrX);
    if (!xResult)
      return std::unexpected(std::move(xResult.error()));
    auto x = std::move(*xResult);
    if (x)
      return LuaFloat(world.geometry().xwrap(*x));
    auto positionResult = engine.luaTo<Vec2F>(positionOrX);
    if (!positionResult)
      return std::unexpected(std::move(positionResult.error()));
    auto position = std::move(*positionResult);
    return engine.luaFrom<Vec2F>(world.geometry().xwrap(position));
  }

  std::expected<LuaValue, StarException> WorldGeometryCallbacks::nearestTo(World& world, LuaEngine& engine, std::variant<Vec2F, float> const& sourcePositionOrX, std::variant<Vec2F, float> const& targetPositionOrX) {
    if (std::holds_alternative<Vec2F>(targetPositionOrX)) {
      Vec2F targetPosition = std::get<Vec2F>(targetPositionOrX);
      Vec2F sourcePosition;
      if (std::holds_alternative<Vec2F>(sourcePositionOrX))
        sourcePosition = std::get<Vec2F>(sourcePositionOrX);
      else
        sourcePosition[0] = std::get<float>(sourcePositionOrX);

      return engine.luaFrom<Vec2F>(world.geometry().nearestTo(sourcePosition, targetPosition));

    } else {
      float targetX = std::get<float>(targetPositionOrX);
      float sourceX;
      if (std::holds_alternative<Vec2F>(sourcePositionOrX))
        sourceX = std::get<Vec2F>(sourcePositionOrX)[0];
      else
        sourceX = std::get<float>(sourcePositionOrX);

      return LuaFloat(world.geometry().nearestTo(sourceX, targetX));
    }
  }

  std::expected<bool, StarException> WorldCollisionCallbacks::rectCollision(World& world, RectF const& arg1, std::optional<CollisionSet> const& arg2) {
    PolyF body = PolyF(arg1);

    if (arg2)
      return world.polyCollision(body, *arg2);
    else
      return world.polyCollision(body);
  }

  std::expected<bool, StarException> WorldCollisionCallbacks::pointTileCollision(World& world, Vec2F const& arg1, std::optional<CollisionSet> const& arg2) {
    if (arg2)
      return world.pointTileCollision(arg1, *arg2);
    else
      return world.pointTileCollision(arg1);
  }

  std::expected<bool, StarException> WorldCollisionCallbacks::lineTileCollision(
      World& world, Vec2F const& arg1, Vec2F const& arg2, std::optional<CollisionSet> const& arg3) {
    Vec2F const begin = arg1;
    Vec2F const end = arg2;

    if (arg3)
      return world.lineTileCollision(begin, end, *arg3);
    else
      return world.lineTileCollision(begin, end);
  }

  std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> WorldCollisionCallbacks::lineTileCollisionPoint(World& world, Vec2F const& begin, Vec2F const& end, std::optional<CollisionSet> const& collisionSet) {
    if (collisionSet)
      return world.lineTileCollisionPoint(begin, end, *collisionSet);
    else
      return world.lineTileCollisionPoint(begin, end);
  }

  std::expected<bool, StarException> WorldCollisionCallbacks::rectTileCollision(World& world, RectF const& arg1, std::optional<CollisionSet> const& arg2) {
    RectI const region = RectI::integral(arg1);

    if (arg2)
      return world.rectTileCollision(region, *arg2);
    else
      return world.rectTileCollision(region);
  }

  std::expected<bool, StarException> WorldCollisionCallbacks::pointCollision(World& world, Vec2F const& point, std::optional<CollisionSet> const& collisionSet) {
    return world.pointCollision(point, collisionSet.value_or(DefaultCollisionSet));
  }

  std::expected<LuaTupleReturn<std::optional<Vec2F>, std::optional<Vec2F>>, StarException> WorldCollisionCallbacks::lineCollision(World& world, Vec2F const& start, Vec2F const& end, std::optional<CollisionSet> const& collisionSet) {
    std::optional<Vec2F> point;
    std::optional<Vec2F> normal;
    auto collisionResult = world.lineCollision(Line2F(start, end), collisionSet.value_or(DefaultCollisionSet));
    if (!collisionResult)
      return std::unexpected(std::move(collisionResult.error()));
    auto collision = std::move(*collisionResult);
    if (collision) {
      point = collision->first;
      normal = collision->second;
    }
    return luaTupleReturn(point, normal);
  }

  std::expected<bool, StarException> WorldCollisionCallbacks::polyCollision(
      World& world, PolyF const& arg1, std::optional<Vec2F> const& arg2, std::optional<CollisionSet> const& arg3) {
    PolyF body = arg1;

    Vec2F center;
    if (arg2) {
      center = *arg2;
      body.translate(center);
    }

    if (arg3)
      return world.polyCollision(body, *arg3);
    else
      return world.polyCollision(body);
  }

  std::expected<List<Vec2I>, StarException> WorldCollisionCallbacks::collisionBlocksAlongLine(
      World& world, Vec2F const& arg1, Vec2F const& arg2, std::optional<CollisionSet> const& arg3, std::optional<int> const& arg4) {
    Vec2F const begin = arg1;
    Vec2F const end = arg2;

    CollisionSet collisionSet = arg3.value_or(DefaultCollisionSet);
    int const maxSize = arg4 ? *arg4 : -1;
    return world.collidingTilesAlongLine(begin, end, collisionSet, maxSize);
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
    auto blocksResult = world.forEachCollisionBlock(RectI::integral(poly.boundBox().padded(maximumCorrection + 1.0f)),
        [&](auto const& block) -> std::expected<void, StarException> {
          if (collisionSet.contains(block.kind))
            collisions.append({block.poly, Vec2F(block.space), 0.0f});
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
          cp.sortingDistance = vmagSquared(bodyCenter - cp.center);
        std::sort(collisions.begin(), collisions.end(), [](auto const& a, auto const& b) { return a.sortingDistance < b.sortingDistance; });

        bool anyIntersects = false;
        for (auto const& cp : collisions) {
          PolyF::IntersectResult intersection;
          if (direction)
            intersection = body.directionalSatIntersection(cp.poly, *direction, false);
          else
            intersection = body.satIntersection(cp.poly);

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
      Vec2F dir = Vec2F::withAngle(angle, 1.0f);
      if (auto resolution = resolveCollision(dir, maximumCorrection, 1))
        return position + *resolution;
    }

    return {};
  }

  std::expected<List<std::pair<Vec2I, LiquidLevel>>, StarException> WorldEnvironmentCallbacks::liquidAlongLine(World& world, Vec2F const& start, Vec2F const& end) {
    List<std::pair<Vec2I, LiquidLevel>> levels;
    auto traversed = forBlocksAlongLine<float>(start, world.geometry().diff(end, start), [&](int x, int y) -> std::expected<bool, StarException> {
        auto liquidLevelResult = world.liquidLevel(RectF::withSize(Vec2F(x, y), Vec2F(1, 1)));
        if (!liquidLevelResult)
          return std::unexpected(std::move(liquidLevelResult.error()));
        auto liquidLevel = std::move(*liquidLevelResult);
        if (liquidLevel.liquid != EmptyLiquidId)
          levels.append(std::pair<Vec2I, LiquidLevel>(Vec2I(x, y), liquidLevel));
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
      if (auto logged = world.root().logger().warnException(strf("Could not create placable object of kind '{}'", objectType), ExceptionDiagnostic(result.error())); !logged)
        return std::unexpected(std::move(logged.error()));
      return false;
    }
    return std::move(*result);
  }

  std::expected<std::optional<EntityId>, StarException> WorldCallbacks::spawnItem(World& world,
      Json const& itemType,
      Vec2F const& worldPosition,
      std::optional<std::size_t> const& inputCount,
      Json const& inputParameters,
      std::optional<Vec2F> const& initialVelocity,
      std::optional<float> const& intangibleTime) {
    bool emptyItem = false;
    auto spawn = [&]() -> std::expected<std::optional<EntityId>, StarException> {
      auto descriptorResult = itemType.isType(Json::Type::String)
          ? itemType.toString().and_then([&](String name) { return ItemDescriptor::create(std::move(name), inputCount.value_or(1), inputParameters ? inputParameters : JsonObject()); })
          : ItemDescriptor::create(itemType);
      if (!descriptorResult)
        return std::unexpected(std::move(descriptorResult.error()));
      auto itemDropResult = ItemDrop::createRandomizedDrop(world.root(), *descriptorResult, worldPosition);
      if (!itemDropResult)
        return std::unexpected(std::move(itemDropResult.error()));
      auto itemDrop = std::move(*itemDropResult);
      if (!itemDrop) {
        emptyItem = true;
        return {};
      }
      if (initialVelocity)
        itemDrop->setVelocity(*initialVelocity);
      if (intangibleTime)
        itemDrop->setIntangibleTime(*intangibleTime);
      if (auto operation = world.addEntity(itemDrop); !operation)
        return std::unexpected(std::move(operation.error()));
      return itemDrop->inWorld() ? std::optional<EntityId>(itemDrop->entityId()) : std::nullopt;
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
      if (auto logged = world.root().logger().warnException(strf("Could not spawn Item of kind '{}'", itemKind), ExceptionDiagnostic(result.error())); !logged)
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
      auto treasureResult = treasureDatabase->createTreasure(pool, level, seed.value_or(world.root().random().randu64()));
      if (!treasureResult)
        return std::unexpected(std::move(treasureResult.error()));
      auto treasure = std::move(*treasureResult);
      for (auto const& treasureItem : treasure) {
        auto entityResult = ItemDrop::createRandomizedDrop(world.root(), treasureItem, position);
        if (!entityResult)
          return std::unexpected(std::move(entityResult.error()));
        auto entity = std::move(*entityResult);
        entities.append(entity->entityId());
        if (auto operation = world.addEntity(entity); !operation)
          return std::unexpected(std::move(operation.error()));
      }
      return {};
    };
    if (auto result = spawn(); !result) {
      if (auto operation = world.root().logger().warnException(strf("Could not spawn treasure from pool '{}'", pool), ExceptionDiagnostic(result.error())); !operation)
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
      parameters["aggressive"] = world.root().random().randb();
      if (arg3)
        parameters.merge(*arg3, true);
      float level = 1;
      if (parameters.contains("level")) {
        auto levelJsonResult = parameters.get("level");
        if (!levelJsonResult)
          return std::unexpected(std::move(levelJsonResult.error()));
        auto levelJson = std::move(*levelJsonResult);
        auto parsedLevelResult = levelJson.get().toFloat();
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
      if (auto logged = world.root().logger().warnException(strf("Could not spawn Monster of type '{}'", arg1), ExceptionDiagnostic(result.error())); !logged)
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
    auto seed = arg5 ? *arg5 : world.root().random().randu64();
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
      if (auto logged = world.root().logger().warnException(strf("Could not spawn NPC of species '{}' and type '{}'", arg2, arg3), ExceptionDiagnostic(result.error())); !logged)
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
      if (auto logged = world.root().logger().warnException(strf("Could not spawn Stagehand of type '{}'", typeName), ExceptionDiagnostic(result.error())); !logged)
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
      projectile->setSourceEntity(sourceEntityId.value_or(NullEntityId), trackSourceEntity);
      if (auto operation = world.addEntity(projectile); !operation)
        return std::unexpected(std::move(operation.error()));
      return projectile->inWorld() ? std::optional<EntityId>(projectile->entityId()) : std::nullopt;
    };
    auto result = spawn();
    if (!result) {
      if (auto logged = world.root().logger().warnException(strf("Could not spawn Projectile of type '{}'", projectileType), ExceptionDiagnostic(result.error())); !logged)
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
    if (levelResult->liquid != EmptyLiquidId)
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
    if (liquidLevel.liquid != EmptyLiquidId) {
      auto modifiedResult =
        world.modifyTile(Vec2I::floor(position), PlaceLiquid{.liquid = EmptyLiquidId, .liquidLevel = 0}, true);
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

  std::expected<std::optional<PlatformerAStar::Path>, StarException> WorldCallbacks::findPlatformerPath(World& world,
      Vec2F const& start,
      Vec2F const& end,
      ActorMovementParameters actorMovementParameters,
      PlatformerAStar::Parameters searchParameters) {
    auto pathFinderResult = PlatformerAStar::PathFinder::create(world, start, end, std::move(actorMovementParameters), std::move(searchParameters));
    if (!pathFinderResult)
      return std::unexpected(std::move(pathFinderResult.error()));
    auto pathFinder = std::move(*pathFinderResult);
    if (auto operation = pathFinder->explore({}); !operation)
      return std::unexpected(std::move(operation.error()));
    return pathFinder->result();
  }

  std::expected<std::shared_ptr<PlatformerAStar::PathFinder>, StarException> WorldCallbacks::platformerPathStart(World& world,
      Vec2F const& start,
      Vec2F const& end,
      ActorMovementParameters actorMovementParameters,
      PlatformerAStar::Parameters searchParameters) {
    return PlatformerAStar::PathFinder::create(world, start, end, std::move(actorMovementParameters), std::move(searchParameters));
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
    if (liquidLevel.liquid != EmptyLiquidId) {
      auto modifiedResult =
        world.forceModifyTile(Vec2I::floor(position), PlaceLiquid{.liquid = EmptyLiquidId, .liquidLevel = 0}, true);
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
    if (auto npc = asRef<Npc>(*entity))
      npc->get().setUniqueId(uniqueId);
    else if (auto monster = asRef<Monster>(*entity))
      monster->get().setUniqueId(uniqueId);
    else if (auto object = asRef<Object>(*entity))
      object->get().setUniqueId(uniqueId);
    else if (auto stagehand = asRef<Stagehand>(*entity))
      stagehand->get().setUniqueId(uniqueId);
    else {
      if (!EntityTypeNames)
        return std::unexpected(EntityTypeNames.error());
      auto typePointerResult = EntityTypeNames->get_right(entity->entityType());
      if (!typePointerResult)
        return std::unexpected(std::move(typePointerResult.error()));
      auto typePointer = std::move(*typePointerResult);
      auto const& type = typePointer.get();
      return std::unexpected(StarException::format("Cannot set unique id on entity of type {}", type));
    }
    return {};
  }

  std::expected<Json, StarException> ServerWorldCallbacks::takeItemDrop(World& world, EntityId entityId, std::optional<EntityId> const& takenBy) {
    auto itemDrop = world.get<ItemDrop>(entityId);
    if (itemDrop) {
      auto canTake = itemDrop->canTake();
      if (!canTake)
        return std::unexpected(std::move(canTake.error()));
      if (!*canTake || !itemDrop->isMaster())
        return Json();
      auto itemResult = takenBy ? itemDrop->takeBy(*takenBy) : itemDrop->take();
      if (!itemResult)
        return std::unexpected(std::move(itemResult.error()));
      if (*itemResult)
        return (*itemResult)->descriptor().toJson();
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
    return engine.createString(name);
  }

  std::expected<std::optional<LuaValue>, StarException> ServerWorldCallbacks::callScriptContext(WorldServer& world, String const& contextName, String const& function, LuaVariadic<LuaValue> const& args) {
    auto context = world.scriptContext(contextName);
    if (!context)
      return std::unexpected(StarException::format("Context {} does not exist", contextName));
    return context->invoke(function, args);
  }

  std::expected<bool, StarException> ServerWorldCallbacks::sendPacket(WorldServer& world, ConnectionId clientId, String const& packetType, Json const& packetData) {
    if (!PacketTypeNames)
      return std::unexpected(PacketTypeNames.error());
    auto typePointerResult = PacketTypeNames->get_left(packetType);
    if (!typePointerResult)
      return std::unexpected(std::move(typePointerResult.error()));
    auto typePointer = std::move(*typePointerResult);
    auto type = typePointer.get();
    auto packetResult = createPacket(world.root().random(), type, packetData);
    if (!packetResult)
      return std::unexpected(std::move(packetResult.error()));
    auto packet = std::move(*packetResult);
    return world.sendPacket(clientId, packet);
  }

  std::expected<void, StarException> WorldDebugCallbacks::debugPoint(Root& root, Vec2F const& arg1, Color const& arg2) {
    root.spatialLogger().logPoint("world", arg1, arg2.toRgba());
    return {};
  }

  std::expected<void, StarException> WorldDebugCallbacks::debugLine(Root& root, Vec2F const& arg1, Vec2F const& arg2, Color const& arg3) {
    root.spatialLogger().logLine("world", arg1, arg2, arg3.toRgba());
    return {};
  }

  std::expected<void, StarException> WorldDebugCallbacks::debugPoly(Root& root, PolyF const& poly, Color const& color) {
    root.spatialLogger().logPoly("world", poly, color.toRgba());
    return {};
  }

  std::expected<void, StarException> WorldDebugCallbacks::debugText(Root& root, LuaEngine& engine, LuaVariadic<LuaValue> const& args) {
    if (args.size() < 3)
      return std::unexpected(StarException::format("Too few arguments to debugText: {}", args.size()));
    auto positionResult = engine.luaTo<Vec2F>(args[args.size() - 2]);
    if (!positionResult)
      return std::unexpected(std::move(positionResult.error()));
    auto position = std::move(*positionResult);
    auto colorResult = engine.luaTo<Color>(args[args.size() - 1]);
    if (!colorResult)
      return std::unexpected(std::move(colorResult.error()));
    auto color = std::move(*colorResult);
    auto formatResult = engine.luaTo<String>(args[0]);
    if (!formatResult)
      return std::unexpected(std::move(formatResult.error()));
    auto format = std::move(*formatResult);
    auto textResult = formatLua(format, slice<List<LuaValue>>(args, 1, args.size() - 2));
    if (!textResult)
      return std::unexpected(std::move(textResult.error()));
    auto text = std::move(*textResult);
    root.spatialLogger().logText("world", text, position, color.toRgba());
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
      return std::unexpected(StarException(strf("Unsupported loungeableQuery orientation {}", orientationName)));

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

  std::expected<bool, StarException> WorldEntityCallbacks::entityCanDamage(World& world, EntityId sourceId, EntityId targetId) {
    auto source = world.entity(sourceId);
    auto target = world.entity(targetId);

    if (!source || !target || !source->getTeam().canDamage(target->getTeam(), false))
      return false;

    return true;
  }

  std::expected<Json, StarException> WorldEntityCallbacks::entityDamageTeam(World& world, EntityId entityId) {
    if (auto entity = world.entity(entityId))
      return entity->getTeam().toJson();
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

  std::expected<std::optional<LuaString>, StarException> WorldEntityCallbacks::entityType(World& world, LuaEngine& engine, EntityId entityId) {
    if (auto entity = world.entity(entityId)) {
      if (!EntityTypeNames)
        return std::unexpected(EntityTypeNames.error());
      auto namePointerResult = EntityTypeNames->get_right(entity->entityType());
      if (!namePointerResult)
        return std::unexpected(std::move(namePointerResult.error()));
      auto namePointer = std::move(*namePointerResult);
      auto const& name = namePointer.get();
      return engine.createString(name);
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
      return entity->metaBoundBox();
    } else {
      return {};
    }
  }

  std::expected<std::optional<Vec2F>, StarException> WorldEntityCallbacks::entityVelocity(World& world, EntityId entityId) {
    auto entity = world.entity(entityId);

    if (auto mobileEntity = as<MobileEntity>(entity))
        return mobileEntity->movementController().velocity();

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
    auto namePointerResult = enumName<GenderNames>(*gender);
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
        {"color", jsonFromColor(Color::rgb(*color))},
        {"origin", jsonFromVec2F(*origin)},
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
      auto modePointerResult = enumValue<PortraitModeNames>(portraitMode);
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
      return std::unexpected(StarException(strf("Unknown tool hand {}", handName)));
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
      return std::unexpected(StarException(strf("Unknown tool hand {}", handName)));
    }

    if (auto entity = world.get<ToolUserEntity>(entityId)) {
      if (auto item = entity->handItem(toolHand)) {
        return item->descriptor().toJson();
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
        res.append(itemDb->toJson(item));
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
        return itemDb->toJson(items[offset]);
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
          res.append(itemDb->toJson(item));
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
          return itemDb->toJson(*res);
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
          return itemDb->toJson(*res);
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
      auto toSearchResult = itemDb->fromJson(items);
      if (!toSearchResult)
        return std::unexpected(std::move(toSearchResult.error()));
      auto toSearch = std::move(*toSearchResult);
      return itemBag->itemsCanFit(optionalRef(toSearch));
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
      auto toSearchResult = itemDb->fromJson(items);
      if (!toSearchResult)
        return std::unexpected(std::move(toSearchResult.error()));
      auto toSearch = std::move(*toSearchResult);
      auto res = itemBag->itemsFitWhere(optionalRef(toSearch));
auto slotsResult = jsonFromList<std::size_t>(res.slots);
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
      auto toInsertResult = itemDb->fromJson(items);
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
        return itemDb->toJson(*res);
    }
    return items;
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerStackItems(World& world, EntityId entityId, Json const& items) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      auto toInsertResult = itemDb->fromJson(items);
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
        return itemDb->toJson(*res);
    }
    return items;
  }

  std::expected<Json, StarException> WorldEntityCallbacks::containerPutItemsAt(World& world, EntityId entityId, Json const& items, std::size_t offset) {
    if (auto container = world.get<ContainerObject>(entityId)) {
      auto itemDbResult = world.root().itemDatabase();
      if (!itemDbResult)
        return std::unexpected(std::move(itemDbResult.error()));
      auto itemDb = std::move(*itemDbResult);
      auto toInsertResult = itemDb->fromJson(items);
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
          return itemDb->toJson(*res);
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
      auto toInsertResult = itemDb->fromJson(items);
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
          return itemDb->toJson(*res);
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
      auto toInsertResult = itemDb->fromJson(items);
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
          return itemDb->toJson(*res);
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
      auto toInsertResult = itemDb->fromJson(items);
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
          return itemDb->toJson(*res);
      }
    }
    return items;
  }

  std::expected<std::optional<LuaValue>, StarException> WorldEntityCallbacks::callScriptedEntity(World& world, EntityId entityId, String const& function, LuaVariadic<LuaValue> const& args) {
    auto entity = as<ScriptedEntity>(world.entity(entityId));
    if (!entity || !entity->isMaster())
      return std::unexpected(StarException::format("Entity {} does not exist or is not a local master scripted entity", entityId));
    return entity->callScript(function, args);
  }

  std::expected<RpcPromise<Vec2F>, StarException> WorldEntityCallbacks::findUniqueEntity(World& world, String const& uniqueId) {
    return world.findUniqueEntity(uniqueId);
  }

  std::expected<RpcPromise<Json>, StarException> WorldEntityCallbacks::sendEntityMessage(World& world, LuaEngine& engine, LuaValue entityId, String const& message, LuaVariadic<Json> args) {
    auto arguments = JsonArray::from(std::move(args));
    if (std::holds_alternative<LuaString>(entityId)) {
      auto idResult = engine.luaTo<String>(entityId);
      if (!idResult)
        return std::unexpected(std::move(idResult.error()));
      auto id = std::move(*idResult);
      return world.sendEntityMessage(id, message, std::move(arguments));
    }
    auto idResult = engine.luaTo<EntityId>(entityId);
    if (!idResult)
      return std::unexpected(std::move(idResult.error()));
    auto id = std::move(*idResult);
    return world.sendEntityMessage(id, message, std::move(arguments));
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
      return std::unexpected(StarException(strf("Unsupported material layer {}", layerName)));
    }

    auto materialId = world.material(Vec2I::floor(position), layer);
    if (materialId == NullMaterialId) {
      return LuaNil;
    } else if (materialId == EmptyMaterialId) {
      return false;
    } else {
      auto materialDatabaseResult = world.root().materialDatabase();
      if (!materialDatabaseResult)
        return std::unexpected(std::move(materialDatabaseResult.error()));
      auto materialDatabase = std::move(*materialDatabaseResult);
      auto nameResult = materialDatabase->materialName(materialId);
      if (!nameResult)
        return std::unexpected(std::move(nameResult.error()));
      auto name = std::move(*nameResult);
      return engine.createString(name);
    }
  }

  std::expected<LuaValue, StarException> WorldEnvironmentCallbacks::mod(World& world, LuaEngine& engine, Vec2F const& position, String const& layerName) {
    TileLayer layer;
    if (layerName == "foreground") {
      layer = TileLayer::Foreground;
    } else if (layerName == "background") {
      layer = TileLayer::Background;
    } else {
      return std::unexpected(StarException(strf("Unsupported mod layer {}", layerName)));
    }

    auto modId = world.mod(Vec2I::floor(position), layer);
    if (isRealMod(modId)) {
      auto materialDatabaseResult = world.root().materialDatabase();
      if (!materialDatabaseResult)
        return std::unexpected(std::move(materialDatabaseResult.error()));
      auto materialDatabase = std::move(*materialDatabaseResult);
      auto nameResult = materialDatabase->modName(modId);
      if (!nameResult)
        return std::unexpected(std::move(nameResult.error()));
      auto const& name = nameResult->get();
      return engine.createString(name);
    }

    return LuaNil;
  }

  std::expected<float, StarException> WorldEnvironmentCallbacks::materialHueShift(World& world, Vec2F const& position, String const& layerName) {
    TileLayer layer;
    if (layerName == "foreground") {
      layer = TileLayer::Foreground;
    } else if (layerName == "background") {
      layer = TileLayer::Background;
    } else {
      return std::unexpected(StarException(strf("Unsupported material layer {}", layerName)));
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
      return std::unexpected(StarException(strf("Unsupported material layer {}", layerName)));
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
      return std::unexpected(StarException(strf("Unsupported material layer {}", layerName)));
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
      return std::unexpected(StarException(strf("Unsupported material layer {}", layerName)));
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
      return std::unexpected(StarException(strf("Unsupported tile layer {}", layerName)));
    }

    unsigned harvestLevel = 999;
    if (arg6)
      harvestLevel = *arg6;

    if (!TileDamageTypeNames)
      return std::unexpected(TileDamageTypeNames.error());
    auto damageTypePointerResult = TileDamageTypeNames->get_left(arg4);
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
    auto tiles = tileAreaBrush(radius, center, false);
    return damageTiles(world, tiles, layer, sourcePosition, damageType, damage, harvestLevel, sourceEntity);
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::placeMaterial(World& world, Vec2I const& arg1, String const& arg2, String const& arg3, std::optional<int> const& arg4, bool arg5) {
    auto tilePosition = arg1;

    PlaceMaterial placeMaterial;

    std::string layerName = arg2.utf8();
    auto split = layerName.find_first_of('+');
    if (split != NPos) {
      auto overrideName = layerName.substr(split + 1);
      layerName = layerName.substr(0, split);
      if (overrideName == "empty" || overrideName == "none")
        placeMaterial.collisionOverride = TileCollisionOverride::Empty;
      else if (overrideName == "block")
        placeMaterial.collisionOverride = TileCollisionOverride::Block;
      else if (overrideName == "platform")
        placeMaterial.collisionOverride = TileCollisionOverride::Platform;
      else
        return std::unexpected(StarException(strf("Unsupported collision override {}", overrideName)));
    }

    if (layerName == "foreground")
      placeMaterial.layer = TileLayer::Foreground;
    else if (layerName == "background")
      placeMaterial.layer = TileLayer::Background;
    else
      return std::unexpected(StarException(strf("Unsupported tile layer {}", layerName)));

    auto materialName = arg3;
    auto materialDatabaseResult = world.root().materialDatabase();
      if (!materialDatabaseResult)
        return std::unexpected(std::move(materialDatabaseResult.error()));
      auto materialDatabase = std::move(*materialDatabaseResult);
    if (!materialDatabase->materialNames().contains(materialName))
      return std::unexpected(StarException(strf("Unknown material name {}", materialName)));
    auto idResult = materialDatabase->materialId(materialName);
    if (!idResult)
      return std::unexpected(std::move(idResult.error()));
    auto id = std::move(*idResult);
    placeMaterial.material = id;

    if (arg4)
      placeMaterial.materialHueShift = static_cast<MaterialHue>(*arg4);

    bool allowOverlap = arg5;

    return world.modifyTile(tilePosition, placeMaterial, allowOverlap);
  }

  std::expected<bool, StarException> WorldEnvironmentCallbacks::replaceMaterials(World& world,
      List<Vec2I> const& tilePositions,
      String const& layer,
      String const& materialName,
      std::optional<int> const& hueShift,
      bool enableDrops) {
    PlaceMaterial placeMaterial;

    std::string layerName = layer.utf8();
    auto split = layerName.find_first_of('+');
    if (split != NPos) {
      auto overrideName = layerName.substr(split + 1);
      layerName = layerName.substr(0, split);
      if (overrideName == "empty" || overrideName == "none")
        placeMaterial.collisionOverride = TileCollisionOverride::Empty;
      else if (overrideName == "block")
        placeMaterial.collisionOverride = TileCollisionOverride::Block;
      else if (overrideName == "platform")
        placeMaterial.collisionOverride = TileCollisionOverride::Platform;
      else
        return std::unexpected(StarException(strf("Unsupported collision override {}", overrideName)));
    }

    if (layerName == "foreground")
      placeMaterial.layer = TileLayer::Foreground;
    else if (layerName == "background")
      placeMaterial.layer = TileLayer::Background;
    else
      return std::unexpected(StarException(strf("Unsupported tile layer {}", layerName)));

    auto materialDatabaseResult = world.root().materialDatabase();
      if (!materialDatabaseResult)
        return std::unexpected(std::move(materialDatabaseResult.error()));
      auto materialDatabase = std::move(*materialDatabaseResult);
    if (!materialDatabase->materialNames().contains(materialName))
      return std::unexpected(StarException(strf("Unknown material name {}", materialName)));
    auto idResult = materialDatabase->materialId(materialName);
    if (!idResult)
      return std::unexpected(std::move(idResult.error()));
    auto id = std::move(*idResult);
    placeMaterial.material = id;

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
      modifications.emplaceAppend(pos, placeMaterial);
    }

    if (modifications.empty())
      return true;

    TileDamage damage;
    if (enableDrops) {
      damage.amount = 1.0f;
      damage.harvestLevel = 999;
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
    auto tiles = tileAreaBrush(radius, center, false);
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
      return std::unexpected(StarException(strf("Unsupported tile layer {}", layerName)));
    }

    auto modName = arg3;
    auto materialDatabaseResult = world.root().materialDatabase();
      if (!materialDatabaseResult)
        return std::unexpected(std::move(materialDatabaseResult.error()));
      auto materialDatabase = std::move(*materialDatabaseResult);
    if (!materialDatabase->modNames().contains(modName))
      return std::unexpected(StarException(strf("Unknown mod name {}", modName)));
    auto idResult = materialDatabase->modId(modName);
    if (!idResult)
      return std::unexpected(std::move(idResult.error()));
    auto id = std::move(*idResult);
    placeMod.mod = id;

    if (arg4)
      placeMod.modHueShift = static_cast<MaterialHue>(*arg4);

    bool allowOverlap = arg5;

    return world.modifyTile(tilePosition, placeMod, allowOverlap);
  }
}

}
