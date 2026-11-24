module;
import std;
import star.string;
import star.list;
import star.map;
import star.json;
import star.format;

import star.data_stream;
import star.game_types;
import star.damage_types;
import star.world_geometry;
import star.strong_typedef;
import star.id_map;
import star.status_types;
import star.damage;

namespace Star {

class World;
class Entity;

struct RemoteHitRequest {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteHitRequest const& request);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteHitRequest& request);

  [[nodiscard]] constexpr std::optional<ConnectionId> destination_connection() const {
    return connection_for_entity(causing_entity_id);
  }

  EntityId causing_entity_id = null_entity_id;
  EntityId target_entity_id = null_entity_id;
  DamageRequest damage_request;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteHitRequest const& request);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteHitRequest& request);

struct RemoteDamageRequest {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageRequest const& request);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageRequest& request);

  [[nodiscard]] constexpr std::optional<ConnectionId> destination_connection() const {
    return connection_for_entity(target_entity_id);
  }

  EntityId causing_entity_id = null_entity_id;
  EntityId target_entity_id = null_entity_id;
  DamageRequest damage_request;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageRequest const& request);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageRequest& request);

struct RemoteDamageNotification {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageNotification const& notification);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageNotification& notification);

  EntityId source_entity_id = null_entity_id;
  DamageNotification damage_notification;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageNotification const& notification);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageNotification& notification);

// Right now, handles entity -> entity damage and ensures that no repeat damage
// is applied within the damage cutoff time from the same causing entity.
class DamageManager {
public:
  DamageManager(World& world, ConnectionId connection_id);

  // Notify entities that they have caused damage, apply damage to master
  // entities, produce damage notifications, and run down damage timeouts.
  [[nodiscard]] std::expected<void, StarException> update(float dt);

  // Incoming RemoteHitRequest and RemoteDamageRequest must have the
  // destination_connection equal to this manager's connection.

  [[nodiscard]] std::expected<void, StarException> push_remote_hit_request(RemoteHitRequest const& request);
  [[nodiscard]] std::expected<void, StarException> push_remote_damage_request(RemoteDamageRequest const& request);
  [[nodiscard]] std::expected<void, StarException> push_remote_damage_notification(RemoteDamageNotification const& notification);

  List<RemoteHitRequest> pull_remote_hit_requests();
  List<RemoteDamageRequest> pull_remote_damage_requests();
  List<RemoteDamageNotification> pull_remote_damage_notifications();

  // Pending *local* notifications.  Sum of all notifications either generated
  // locally or received.
  List<DamageNotification> pull_pending_notifications();

private:
  struct EntityDamageEvent {
    std::variant<EntityId, String> timeout_group;
    float timeout;
  };

  struct PendingHit {
    std::shared_ptr<Entity> target;
    EntityId target_id;
    HitType hit_type;
  };

  [[nodiscard]] std::expected<void, StarException> update_entity(Entity& causing_entity);
  [[nodiscard]] std::expected<SmallList<PendingHit, 4>, StarException> query_hit(DamageSource const& source, EntityId causing_id) const;
  bool is_authoritative(Entity const& causing_entity, Entity const& target_entity) const;
  [[nodiscard]] std::expected<void, StarException> dispatch_hit(Entity& causing_entity, Entity& target_entity,
      DamageRequest& request, bool last_hit);
  [[nodiscard]] std::expected<void, StarException> apply_damage(Entity& target_entity, DamageRequest const& request);
  [[nodiscard]] std::expected<void, StarException> notify_source(EntityId source_id, DamageNotification const& notification);
  [[nodiscard]] std::expected<void, StarException> add_damage_notification(EntityId source_id, DamageNotification notification);

  std::reference_wrapper<World> m_world;
  ConnectionId m_connection_id;

  // Maps target entity to all of the recent damage events that entity has
  // received, to prevent rapidly repeating damage.
  HashMap<EntityId, List<EntityDamageEvent>> m_recent_entity_damages;

  List<RemoteHitRequest> m_pending_remote_hit_requests;
  List<RemoteDamageRequest> m_pending_remote_damage_requests;
  List<RemoteDamageNotification> m_pending_remote_notifications;
  List<DamageNotification> m_pending_notifications;
};

}

export module star.damage_manager;

export namespace Star {
using ::Star::World;
using ::Star::Entity;
using ::Star::DamageManager;
using ::Star::RemoteHitRequest;
using ::Star::RemoteDamageRequest;
using ::Star::RemoteDamageNotification;
}
