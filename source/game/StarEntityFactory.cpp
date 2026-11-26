import star.format;
import star.json_extra;
import star.json;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.set;
import star.a_star;
import star.ordered_set;
import star.audio;
import star.map;
import star.color;
import star.string;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.lru_cache;
import star.time;
import star.ttl_cache;
import star.lua;
import star.periodic;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;

import star.lua_root;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.inspectable_entity;
import star.plant;
import star.plant_drop;
import star.uuid;
import star.drawable;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.anchorable_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.platformer_astar_types;
import star.game_timers;
import star.actor_movement_controller;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.radio_message_database;
import star.player;
import star.aggressive_entity;
import star.scripted_entity;
import star.effect_emitter;
import star.monster;
import star.status_effect_entity;
import star.wiring;
import star.wire_entity;
import star.object;
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
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.mobile_entity;
import star.projectile;
import star.item_drop;
import star.npc;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;
import star.stagehand;
import star.vehicle;

import star.projectile_database;
import star.vehicle_database;
import star.versioning_database;
import star.object_database;

import star.player_factory;
import star.entity_factory;
import star.monster_database;
import star.npc_database;


import star.net_compatibility;
import star.byte_array;
namespace Star {

std::expected<EnumMap<EntityType>, StarException> const EntityFactory::entity_storage_identifiers = EnumMap<EntityType>::create({
  {EntityType::Player, "PlayerEntity"},
  {EntityType::Monster, "MonsterEntity"},
  {EntityType::Object, "ObjectEntity"},
  {EntityType::ItemDrop, "ItemDropEntity"},
  {EntityType::Projectile, "ProjectileEntity"},
  {EntityType::Plant, "PlantEntity"},
  {EntityType::PlantDrop, "PlantDropEntity"},
  {EntityType::Npc, "NpcEntity"},
  {EntityType::Stagehand, "StagehandEntity"},
  {EntityType::Vehicle, "VehicleEntity"}
});

EntityFactory::EntityFactory(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<EntityFactory>, StarException> EntityFactory::create(Root& root) {
  struct MakeShared final : EntityFactory {
    explicit MakeShared(Root& root) : EntityFactory(root) {}
  };
  auto factory = std::make_shared<MakeShared>(root);
  auto player = root.playerFactory();
  if (!player) return std::unexpected(std::move(player.error()));
  factory->m_player_factory = std::move(*player);
  auto monster = root.monster_database();
  if (!monster) return std::unexpected(std::move(monster.error()));
  factory->m_monster_database = std::move(*monster);
  auto object = root.objectDatabase();
  if (!object) return std::unexpected(std::move(object.error()));
  factory->m_object_database = std::move(*object);
  auto projectile = root.projectileDatabase();
  if (!projectile) return std::unexpected(std::move(projectile.error()));
  factory->m_projectile_database = std::move(*projectile);
  auto npc = root.npcDatabase();
  if (!npc) return std::unexpected(std::move(npc.error()));
  factory->m_npc_database = std::move(*npc);
  auto vehicle = root.vehicleDatabase();
  if (!vehicle) return std::unexpected(std::move(vehicle.error()));
  factory->m_vehicle_database = std::move(*vehicle);
  auto versioning = root.versioningDatabase();
  if (!versioning) return std::unexpected(std::move(versioning.error()));
  factory->m_versioning_database = std::move(*versioning);
  return factory;
}

std::expected<ByteArray, StarException> EntityFactory::net_store_entity(std::shared_ptr<Entity> const& entity, NetCompatibilityRules rules) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);

  if (!entity)
    return std::unexpected(EntityFactoryException::format("Cannot make net store for a null entity"));
  auto type = entity->entity_type();
  auto store = [&]<typename T>() -> std::expected<ByteArray, StarException> {
    auto typed = convert_ref<T>(*entity);
    if (!typed) return std::unexpected(std::move(typed.error()));
    return typed->get().net_store(rules);
  };
  switch (type) {
    case EntityType::Player:
      return store.template operator()<Player>();
    case EntityType::Monster:
      return store.template operator()<Monster>();
    case EntityType::Object:
      return store.template operator()<Object>();
    case EntityType::Plant:
      return store.template operator()<Plant>();
    case EntityType::PlantDrop:
      return store.template operator()<PlantDrop>();
    case EntityType::Projectile:
      return store.template operator()<Projectile>();
    case EntityType::ItemDrop:
      return store.template operator()<ItemDrop>();
    case EntityType::Npc:
      return store.template operator()<Npc>();
    case EntityType::Stagehand:
      return store.template operator()<Stagehand>();
    case EntityType::Vehicle: {
      auto vehicle = convert<Vehicle>(entity);
      if (!vehicle) return std::unexpected(std::move(vehicle.error()));
      return m_vehicle_database->net_store(*vehicle, rules);
    }
    default:
      break;
  }
  if (!EntityTypeNames) return std::unexpected(EntityTypeNames.error());
  auto name = EntityTypeNames->get_right(type);
  if (!name) return std::unexpected(std::move(name.error()));
  return std::unexpected(EntityFactoryException::format("Don't know how to make net store for entity type '{}'", name->get()));
}

std::expected<std::shared_ptr<Entity>, StarException> EntityFactory::net_load_entity(EntityType type, ByteArray const& net_store, NetCompatibilityRules rules) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);

  switch (type) {
    case EntityType::Player:
      return m_player_factory->net_load_player(net_store, rules);
    case EntityType::Monster:
      return m_monster_database->net_load_monster(net_store, rules);
    case EntityType::Object:
      return m_object_database->net_load_object(net_store, rules);
    case EntityType::Plant:
      return Plant::create(m_root, net_store, rules);
    case EntityType::PlantDrop:
      return PlantDrop::create(m_root, net_store, rules);
    case EntityType::Projectile:
      return m_projectile_database->net_load_projectile(net_store, rules);
    case EntityType::ItemDrop:
      return ItemDrop::create(m_root, net_store, rules);
    case EntityType::Npc:
      return m_npc_database->net_load_npc(net_store, rules);
    case EntityType::Stagehand:
      return Stagehand::create(m_root, net_store, rules);
    case EntityType::Vehicle:
      return m_vehicle_database->net_load(net_store, rules);
    default:
      break;
  }
  if (!EntityTypeNames) return std::unexpected(EntityTypeNames.error());
  auto name = EntityTypeNames->get_right(type);
  if (!name) return std::unexpected(std::move(name.error()));
  return std::unexpected(EntityFactoryException::format("Don't know how to create entity type '{}' from net store", name->get()));
}

std::expected<Json, StarException> EntityFactory::disk_store_entity(std::shared_ptr<Entity> const& entity) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return disk_store_entity_locked(entity);
}

std::expected<Json, StarException> EntityFactory::disk_store_entity_locked(std::shared_ptr<Entity> const& entity) const {

  if (!entity)
    return std::unexpected(EntityFactoryException::format("Cannot make disk store for a null entity"));
  auto type = entity->entity_type();
  auto store = [&]<typename T>() -> std::expected<Json, StarException> {
    auto typed = convert_ref<T>(*entity);
    if (!typed) return std::unexpected(std::move(typed.error()));
    return typed->get().diskStore();
  };
  switch (type) {
    case EntityType::Player:
      return store.template operator()<Player>();
    case EntityType::Monster:
      return store.template operator()<Monster>();
    case EntityType::Object:
      return store.template operator()<Object>();
    case EntityType::Plant:
      return store.template operator()<Plant>();
    case EntityType::ItemDrop: {
      auto drop = convert_ref<ItemDrop>(*entity);
      if (!drop) return std::unexpected(std::move(drop.error()));
      return drop->get().disk_store();
    }
    case EntityType::Npc:
      return store.template operator()<Npc>();
    case EntityType::Stagehand:
      return store.template operator()<Stagehand>();
    case EntityType::Vehicle: {
      auto vehicle = convert<Vehicle>(entity);
      if (!vehicle) return std::unexpected(std::move(vehicle.error()));
      return m_vehicle_database->diskStore(*vehicle);
    }
    default:
      break;
  }
  if (!EntityTypeNames) return std::unexpected(EntityTypeNames.error());
  auto name = EntityTypeNames->get_right(type);
  if (!name) return std::unexpected(std::move(name.error()));
  return std::unexpected(EntityFactoryException::format("Don't know how to make disk store for entity type '{}'", name->get()));
}

std::expected<std::shared_ptr<Entity>, StarException> EntityFactory::disk_load_entity(EntityType type, Json const& disk_store) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return disk_load_entity_locked(type, disk_store);
}

std::expected<std::shared_ptr<Entity>, StarException> EntityFactory::disk_load_entity_locked(EntityType type, Json const& disk_store) const {

  switch (type) {
    case EntityType::Player:
      return m_player_factory->diskLoadPlayer(disk_store);
    case EntityType::Monster:
      return m_monster_database->disk_load_monster(disk_store);
    case EntityType::Object:
      return m_object_database->diskLoadObject(disk_store);
    case EntityType::Plant:
      return Plant::create(m_root, disk_store);
    case EntityType::ItemDrop:
      return ItemDrop::create(m_root, disk_store);
    case EntityType::Npc:
      return m_npc_database->diskLoadNpc(disk_store);
    case EntityType::Stagehand:
      return Stagehand::create(m_root, disk_store);
    case EntityType::Vehicle:
      return m_vehicle_database->diskLoad(disk_store);
    default:
      break;
  }
  if (!EntityTypeNames) return std::unexpected(EntityTypeNames.error());
  auto name = EntityTypeNames->get_right(type);
  if (!name) return std::unexpected(std::move(name.error()));
  return std::unexpected(EntityFactoryException::format("Don't know how to create entity type '{}' from disk store", name->get()));
}

std::expected<Json, StarException> EntityFactory::load_versioned_json(VersionedJson const& versioned_json, EntityType expected_type) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (!entity_storage_identifiers) return std::unexpected(entity_storage_identifiers.error());
  auto identifier = entity_storage_identifiers->get_right(expected_type);
  if (!identifier) return std::unexpected(std::move(identifier.error()));
  return m_versioning_database->loadVersionedJson(versioned_json, identifier->get());
}

std::expected<VersionedJson, StarException> EntityFactory::store_versioned_json(EntityType type, Json const& store) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return store_versioned_json_locked(type, store);
}

std::expected<VersionedJson, StarException> EntityFactory::store_versioned_json_locked(EntityType type, Json const& store) const {
  if (!entity_storage_identifiers) return std::unexpected(entity_storage_identifiers.error());
  auto identifier = entity_storage_identifiers->get_right(type);
  if (!identifier) return std::unexpected(std::move(identifier.error()));
  return m_versioning_database->makeCurrentVersionedJson(identifier->get(), store);
}

std::expected<std::shared_ptr<Entity>, StarException> EntityFactory::load_versioned_entity(VersionedJson const& versioned_json) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (!entity_storage_identifiers) return std::unexpected(entity_storage_identifiers.error());
  auto type = entity_storage_identifiers->get_left(versioned_json.identifier);
  if (!type) return std::unexpected(std::move(type.error()));
  auto store = m_versioning_database->loadVersionedJson(versioned_json, versioned_json.identifier);
  if (!store) return std::unexpected(std::move(store.error()));
  return disk_load_entity_locked(type->get(), *store);
}

std::expected<VersionedJson, StarException> EntityFactory::store_versioned_entity(std::shared_ptr<Entity> const& entity) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto store = disk_store_entity_locked(entity);
  if (!store) return std::unexpected(std::move(store.error()));
  return store_versioned_json_locked(entity->entity_type(), *store);
}

}
