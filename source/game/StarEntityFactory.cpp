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
import star.either;
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
import star.version;

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

std::expected<EnumMap<EntityType>, StarException> const EntityFactory::EntityStorageIdentifiers = EnumMap<EntityType>::create({
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
  auto factory = std::shared_ptr<EntityFactory>(new EntityFactory(root));
  auto player = root.playerFactory();
  if (!player) return std::unexpected(std::move(player.error()));
  factory->m_playerFactory = std::move(*player);
  auto monster = root.monsterDatabase();
  if (!monster) return std::unexpected(std::move(monster.error()));
  factory->m_monsterDatabase = std::move(*monster);
  auto object = root.objectDatabase();
  if (!object) return std::unexpected(std::move(object.error()));
  factory->m_objectDatabase = std::move(*object);
  auto projectile = root.projectileDatabase();
  if (!projectile) return std::unexpected(std::move(projectile.error()));
  factory->m_projectileDatabase = std::move(*projectile);
  auto npc = root.npcDatabase();
  if (!npc) return std::unexpected(std::move(npc.error()));
  factory->m_npcDatabase = std::move(*npc);
  auto vehicle = root.vehicleDatabase();
  if (!vehicle) return std::unexpected(std::move(vehicle.error()));
  factory->m_vehicleDatabase = std::move(*vehicle);
  auto versioning = root.versioningDatabase();
  if (!versioning) return std::unexpected(std::move(versioning.error()));
  factory->m_versioningDatabase = std::move(*versioning);
  return factory;
}

std::expected<ByteArray, StarException> EntityFactory::netStoreEntity(std::shared_ptr<Entity> const& entity, NetCompatibilityRules rules) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);

  if (auto player = as<Player>(entity)) {
    return player->netStore(rules);
  } else if (auto monster = as<Monster>(entity)) {
    return monster->netStore(rules);
  } else if (auto object = as<Object>(entity)) {
    return object->netStore(rules);
  } else if (auto plant = as<Plant>(entity)) {
    return plant->netStore(rules);
  } else if (auto plantDrop = as<PlantDrop>(entity)) {
    return plantDrop->netStore(rules);
  } else if (auto projectile = as<Projectile>(entity)) {
    return projectile->netStore(rules);
  } else if (auto itemDrop = as<ItemDrop>(entity)) {
    return itemDrop->netStore(rules);
  } else if (auto npc = as<Npc>(entity)) {
    return npc->netStore(rules);
  } else if (auto stagehand = as<Stagehand>(entity)) {
    return stagehand->netStore(rules);
  } else if (auto vehicle = as<Vehicle>(entity)) {
    return m_vehicleDatabase->netStore(vehicle, rules);
  } else {
    if (!EntityTypeNames) return std::unexpected(EntityTypeNames.error());
    auto name = EntityTypeNames->get_right(entity->entityType());
    if (!name) return std::unexpected(std::move(name.error()));
    return std::unexpected(EntityFactoryException::format("Don't know how to make net store for entity type '{}'", name->get()));
  }
}

std::expected<std::shared_ptr<Entity>, StarException> EntityFactory::netLoadEntity(EntityType type, ByteArray const& netStore, NetCompatibilityRules rules) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);

  if (type == EntityType::Player) {
    return m_playerFactory->netLoadPlayer(netStore, rules);
  } else if (type == EntityType::Monster) {
    return m_monsterDatabase->netLoadMonster(netStore, rules);
  } else if (type == EntityType::Object) {
    return m_objectDatabase->netLoadObject(netStore, rules);
  } else if (type == EntityType::Plant) {
    return Plant::create(m_root, netStore, rules);
  } else if (type == EntityType::PlantDrop) {
    return PlantDrop::create(m_root, netStore, rules);
  } else if (type == EntityType::Projectile) {
    return m_projectileDatabase->netLoadProjectile(netStore, rules);
  } else if (type == EntityType::ItemDrop) {
    return ItemDrop::create(m_root, netStore, rules);
  } else if (type == EntityType::Npc) {
    return m_npcDatabase->netLoadNpc(netStore, rules);
  } else if (type == EntityType::Stagehand) {
    return Stagehand::create(m_root, netStore, rules);
  } else if (type == EntityType::Vehicle) {
    return m_vehicleDatabase->netLoad(netStore, rules);
  } else {
    if (!EntityTypeNames) return std::unexpected(EntityTypeNames.error());
    auto name = EntityTypeNames->get_right(type);
    if (!name) return std::unexpected(std::move(name.error()));
    return std::unexpected(EntityFactoryException::format("Don't know how to create entity type '{}' from net store", name->get()));
  }
}

std::expected<Json, StarException> EntityFactory::diskStoreEntity(std::shared_ptr<Entity> const& entity) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);

  if (auto player = as<Player>(entity)) {
    return player->diskStore();
  } else if (auto monster = as<Monster>(entity)) {
    return monster->diskStore();
  } else if (auto object = as<Object>(entity)) {
    return object->diskStore();
  } else if (auto plant = as<Plant>(entity)) {
    return plant->diskStore();
  } else if (auto itemDrop = as<ItemDrop>(entity)) {
    return itemDrop->diskStore();
  } else if (auto npc = as<Npc>(entity)) {
    return npc->diskStore();
  } else if (auto stagehand = as<Stagehand>(entity)) {
    return stagehand->diskStore();
  } else if (auto vehicle = as<Vehicle>(entity)) {
    return m_vehicleDatabase->diskStore(vehicle);
  } else {
    if (!EntityTypeNames) return std::unexpected(EntityTypeNames.error());
    auto name = EntityTypeNames->get_right(entity->entityType());
    if (!name) return std::unexpected(std::move(name.error()));
    return std::unexpected(EntityFactoryException::format("Don't know how to make disk store for entity type '{}'", name->get()));
  }
}

std::expected<std::shared_ptr<Entity>, StarException> EntityFactory::diskLoadEntity(EntityType type, Json const& diskStore) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);

  if (type == EntityType::Player) {
    return m_playerFactory->diskLoadPlayer(diskStore);
  } else if (type == EntityType::Monster) {
    return m_monsterDatabase->diskLoadMonster(diskStore);
  } else if (type == EntityType::Object) {
    return m_objectDatabase->diskLoadObject(diskStore);
  } else if (type == EntityType::Plant) {
    return Plant::create(m_root, diskStore);
  } else if (type == EntityType::ItemDrop) {
    return ItemDrop::create(m_root, diskStore);
  } else if (type == EntityType::Npc) {
    return m_npcDatabase->diskLoadNpc(diskStore);
  } else if (type == EntityType::Stagehand) {
    return Stagehand::create(m_root, diskStore);
  } else if (type == EntityType::Vehicle) {
    return m_vehicleDatabase->diskLoad(diskStore);
  } else {
    if (!EntityTypeNames) return std::unexpected(EntityTypeNames.error());
    auto name = EntityTypeNames->get_right(type);
    if (!name) return std::unexpected(std::move(name.error()));
    return std::unexpected(EntityFactoryException::format("Don't know how to create entity type '{}' from disk store", name->get()));
  }
}

std::expected<Json, StarException> EntityFactory::loadVersionedJson(VersionedJson const& versionedJson, EntityType expectedType) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (!EntityStorageIdentifiers) return std::unexpected(EntityStorageIdentifiers.error());
  auto identifier = EntityStorageIdentifiers->get_right(expectedType);
  if (!identifier) return std::unexpected(std::move(identifier.error()));
  return m_versioningDatabase->loadVersionedJson(versionedJson, identifier->get());
}

std::expected<VersionedJson, StarException> EntityFactory::storeVersionedJson(EntityType type, Json const& store) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (!EntityStorageIdentifiers) return std::unexpected(EntityStorageIdentifiers.error());
  auto identifier = EntityStorageIdentifiers->get_right(type);
  if (!identifier) return std::unexpected(std::move(identifier.error()));
  return m_versioningDatabase->makeCurrentVersionedJson(identifier->get(), store);
}

std::expected<std::shared_ptr<Entity>, StarException> EntityFactory::loadVersionedEntity(VersionedJson const& versionedJson) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (!EntityStorageIdentifiers) return std::unexpected(EntityStorageIdentifiers.error());
  auto type = EntityStorageIdentifiers->get_left(versionedJson.identifier);
  if (!type) return std::unexpected(std::move(type.error()));
  auto store = loadVersionedJson(versionedJson, type->get());
  if (!store) return std::unexpected(std::move(store.error()));
  return diskLoadEntity(type->get(), *store);
}

std::expected<VersionedJson, StarException> EntityFactory::storeVersionedEntity(std::shared_ptr<Entity> const& entityPtr) const {
  auto store = diskStoreEntity(entityPtr);
  if (!store) return std::unexpected(std::move(store.error()));
  return storeVersionedJson(entityPtr->entityType(), *store);
}

}
