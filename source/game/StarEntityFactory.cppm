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

  [[nodiscard]] std::expected<Json, StarException> diskStoreEntity(std::shared_ptr<Entity> const& entity) const;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> diskLoadEntity(EntityType type, Json const& diskStore) const;

  [[nodiscard]] std::expected<Json, StarException> loadVersionedJson(VersionedJson const& versionedJson, EntityType expectedType) const;
  [[nodiscard]] std::expected<VersionedJson, StarException> storeVersionedJson(EntityType type, Json const& store) const;

  // Versioned loads retain the database's migration and recovery policy.
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> loadVersionedEntity(VersionedJson const& versionedJson) const;
  [[nodiscard]] std::expected<VersionedJson, StarException> storeVersionedEntity(std::shared_ptr<Entity> const& entityPtr) const;

private:
  explicit EntityFactory(Root& root);
  Root& m_root;
  static std::expected<EnumMap<EntityType>, StarException> const EntityStorageIdentifiers;

  mutable std::recursive_mutex m_mutex;

  std::shared_ptr<PlayerFactory const> m_playerFactory;
  std::shared_ptr<MonsterDatabase const> m_monsterDatabase;
  std::shared_ptr<ObjectDatabase const> m_objectDatabase;
  std::shared_ptr<ProjectileDatabase const> m_projectileDatabase;
  std::shared_ptr<NpcDatabase const> m_npcDatabase;
  std::shared_ptr<VehicleDatabase const> m_vehicleDatabase;
  std::shared_ptr<VersioningDatabase const> m_versioningDatabase;
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
