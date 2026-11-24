module;
import std;
import star.format;
import star.json;
import star.id_map;
import star.thread;
import star.casting;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;

import star.net_compatibility;
import star.byte_array;
namespace Star {

struct VersionedJson;
class Root;
class VehicleDatabase;
class VersioningDatabase;
class PlayerFactory;
class MonsterDatabase;
class ObjectDatabase;
class ProjectileDatabase;
class NpcDatabase;

struct EntityFactoryExceptionTag {
  static constexpr std::string_view name() { return "EntityFactoryException"; }
};
using EntityFactoryException = StarError<EntityFactoryExceptionTag, StarException>;

class EntityFactory {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<EntityFactory>, StarException> create(Root& root);

  [[nodiscard]] std::expected<ByteArray, StarException> net_store_entity(std::shared_ptr<Entity> const& entity, NetCompatibilityRules rules = {}) const;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> net_load_entity(EntityType type, ByteArray const& net_store, NetCompatibilityRules rules = {}) const;

  [[nodiscard]] std::expected<Json, StarException> disk_store_entity(std::shared_ptr<Entity> const& entity) const;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> disk_load_entity(EntityType type, Json const& disk_store) const;

  [[nodiscard]] std::expected<Json, StarException> load_versioned_json(VersionedJson const& versioned_json, EntityType expected_type) const;
  [[nodiscard]] std::expected<VersionedJson, StarException> store_versioned_json(EntityType type, Json const& store) const;

  // Versioned loads retain the database's migration and recovery policy.
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> load_versioned_entity(VersionedJson const& versioned_json) const;
  [[nodiscard]] std::expected<VersionedJson, StarException> store_versioned_entity(std::shared_ptr<Entity> const& entity) const;

private:
  explicit EntityFactory(Root& root);
  std::expected<Json, StarException> disk_store_entity_locked(std::shared_ptr<Entity> const& entity) const;
  std::expected<std::shared_ptr<Entity>, StarException> disk_load_entity_locked(EntityType type, Json const& disk_store) const;
  std::expected<VersionedJson, StarException> store_versioned_json_locked(EntityType type, Json const& store) const;
  Root& m_root;
  static std::expected<EnumMap<EntityType>, StarException> const entity_storage_identifiers;

  mutable std::recursive_mutex m_mutex;

  std::shared_ptr<PlayerFactory const> m_player_factory;
  std::shared_ptr<MonsterDatabase const> m_monster_database;
  std::shared_ptr<ObjectDatabase const> m_object_database;
  std::shared_ptr<ProjectileDatabase const> m_projectile_database;
  std::shared_ptr<NpcDatabase const> m_npc_database;
  std::shared_ptr<VehicleDatabase const> m_vehicle_database;
  std::shared_ptr<VersioningDatabase const> m_versioning_database;
};

}

export module star.entity_factory;

export namespace Star {
  using ::Star::VersionedJson;
  using ::Star::VehicleDatabase;
  using ::Star::VersioningDatabase;
  using ::Star::PlayerFactory;
  using ::Star::MonsterDatabase;
  using ::Star::ObjectDatabase;
  using ::Star::ProjectileDatabase;
  using ::Star::NpcDatabase;
  using ::Star::EntityFactory;
  using ::Star::EntityFactoryExceptionTag;
  using ::Star::EntityFactoryException;
}
