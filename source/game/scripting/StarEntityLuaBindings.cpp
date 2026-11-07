module;
import std;
import star.format;
import star.json;
import star.json_extra;
import star.id_map;
import star.lua;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;
import star.a_star;
import star.ordered_set;
import star.audio;
import star.thread;
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
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.lru_cache;
import star.time;
import star.ttl_cache;


import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.lua_game_converters;
import star.uuid;
import star.drawable;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
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
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.aggressive_entity;
import star.scripted_entity;
import star.monster_database;
import star.effect_emitter;
import star.monster;
import star.npc_database;
import star.npc;
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

module star.entity_lua_bindings;

namespace Star::LuaBindings::EntityCallbacks {
  EntityId id(Entity const& entity);
  [[nodiscard]] std::expected<LuaTable, StarException> damageTeam(Entity const& entity, LuaEngine& engine);
  [[nodiscard]] std::expected<bool, StarException> isValidTarget(Entity const& entity, EntityId entityId);
  [[nodiscard]] std::expected<Vec2F, StarException> distanceToEntity(Entity const& entity, EntityId entityId);
  [[nodiscard]] std::expected<bool, StarException> entityInSight(Entity const& entity, EntityId entityId);
}

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeEntityCallbacks(Entity const& entity) {
  LuaCallbacks callbacks;

  if (auto registration =
        callbacks.registerCallbackWithSignature<EntityId>("id", [&entity]() { return EntityCallbacks::id(entity); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<LuaTable, LuaEngine&>(
        "damageTeam", [&entity](LuaEngine& engine) { return EntityCallbacks::damageTeam(entity, engine); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool, EntityId>(
        "isValidTarget", [&entity](EntityId entityId) { return EntityCallbacks::isValidTarget(entity, entityId); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Vec2F, EntityId>(
        "distanceToEntity",
        [&entity](EntityId entityId) { return EntityCallbacks::distanceToEntity(entity, entityId); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool, EntityId>(
        "entityInSight", [&entity](EntityId entityId) { return EntityCallbacks::entityInSight(entity, entityId); });
      !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallback("position", [&entity]() { return entity.position(); }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("entityType", [&entity]() -> std::expected<String, StarException> {
    if (!EntityTypeNames)
      return std::unexpected(EntityTypeNames.error());
    auto name = EntityTypeNames->get_right(entity.entityType());
    if (!name)
      return std::unexpected(std::move(name.error()));
    return name->get();
  }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("uniqueId", [&entity]() { return entity.uniqueId(); }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("persistent", [&entity]() { return entity.persistent(); }); !registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

EntityId LuaBindings::EntityCallbacks::id(Entity const& entity) {
  return entity.entityId();
}

std::expected<LuaTable, StarException> LuaBindings::EntityCallbacks::damageTeam(Entity const& entity, LuaEngine& engine) {
  auto table = engine.createTable();
  if (!table)
    return std::unexpected(std::move(table.error()));
  auto team = entity.getTeam();
  auto type = enumName<TeamTypeNames>(team.type);
  if (!type)
    return std::unexpected(std::move(type.error()));
  if (auto result = table->set("type", *type); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = table->set("team", team.team); !result)
    return std::unexpected(std::move(result.error()));
  return table;
}

std::expected<bool, StarException> LuaBindings::EntityCallbacks::isValidTarget(Entity const& entity, EntityId entityId) {
  auto world = entity.world();
  if (!world)
    return std::unexpected(std::move(world.error()));
  auto target = world->get().entity(entityId);

  if (!target || !entity.getTeam().canDamage(target->getTeam(), false))
    return false;

  if (auto monster = as<Monster>(target))
    return monster->aggressive();

  if (auto npc = as<Npc>(target)) {
    if (auto attackerNpc = asRef<Npc>(entity))
      return npc->aggressive() || attackerNpc->get().aggressive();
    return true;
  }

  return is<Player>(target);
}

std::expected<Vec2F, StarException> LuaBindings::EntityCallbacks::distanceToEntity(Entity const& entity, EntityId entityId) {
  auto world = entity.world();
  if (!world)
    return std::unexpected(std::move(world.error()));
  Vec2F dist;
  if (auto target = world->get().entity(entityId))
    dist = world->get().geometry().diff(target->position(), entity.position());
  return dist;
}

std::expected<bool, StarException> LuaBindings::EntityCallbacks::entityInSight(Entity const& entity, EntityId entityId) {
  auto world = entity.world();
  if (!world)
    return std::unexpected(std::move(world.error()));
  if (auto target = world->get().entity(entityId)) {
    auto collision = world->get().lineTileCollision(target->position(), entity.position());
    if (!collision)
      return std::unexpected(std::move(collision.error()));
    return !*collision;
  }
  return false;
}

}
