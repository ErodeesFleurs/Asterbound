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
import star.game_types;
import star.damage_types;

namespace Star {

class World;
class Entity;

struct RemoteHitRequest {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteHitRequest const& hitRequest);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteHitRequest& hitRequest);

  [[nodiscard]] constexpr std::optional<ConnectionId> destination_connection() const {
    return connection_for_entity(causingEntityId);
  }

  EntityId causingEntityId;
  EntityId targetEntityId;
  DamageRequest damageRequest;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteHitRequest const& hitRequest);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteHitRequest& hitRequest);

struct RemoteDamageRequest {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageRequest const& damageRequest);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageRequest& damageRequest);

  [[nodiscard]] constexpr std::optional<ConnectionId> destination_connection() const {
    return connection_for_entity(targetEntityId);
  }

  EntityId causingEntityId;
  EntityId targetEntityId;
  DamageRequest damageRequest;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageRequest const& damageRequest);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageRequest& damageRequest);

struct RemoteDamageNotification {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageNotification const& damageNotification);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageNotification& damageNotification);

  EntityId sourceEntityId;
  DamageNotification damageNotification;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageNotification const& damageNotification);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageNotification& damageNotification);

// Right now, handles entity -> entity damage and ensures that no repeat damage
// is applied within the damage cutoff time from the same causing entity.
class DamageManager {
public:
  DamageManager(World& world, ConnectionId connectionId);

  // Notify entities that they have caused damage, apply damage to master
  // entities, produce damage notifications, and run down damage timeouts.
  [[nodiscard]] std::expected<void, StarException> update(float dt);

  // Incoming RemoteHitRequest and RemoteDamageRequest must have the
  // destination_connection equal to the DamageManager's connectionId

  [[nodiscard]] std::expected<void, StarException> pushRemoteHitRequest(RemoteHitRequest const& remoteHitRequest);
  [[nodiscard]] std::expected<void, StarException> pushRemoteDamageRequest(RemoteDamageRequest const& remoteDamageRequest);
  [[nodiscard]] std::expected<void, StarException> pushRemoteDamageNotification(RemoteDamageNotification remoteDamageNotification);

  List<RemoteHitRequest> pullRemoteHitRequests();
  List<RemoteDamageRequest> pullRemoteDamageRequests();
  List<RemoteDamageNotification> pullRemoteDamageNotifications();

  // Pending *local* notifications.  Sum of all notifications either generated
  // locally or recieved.
  List<DamageNotification> pullPendingNotifications();

private:
  struct EntityDamageEvent {
    std::variant<EntityId, String> timeoutGroup;
    float timeout;
  };

  // Searches for and queries for hit to any entity within range of the
  // damage source.  Skips over source.sourceEntityId, if set.
  [[nodiscard]] std::expected<SmallList<std::pair<EntityId, HitType>, 4>, StarException> queryHit(DamageSource const& source, EntityId causingId) const;

  bool isAuthoritative(std::shared_ptr<Entity> const& causingEntity, std::shared_ptr<Entity> const& targetEntity);

  [[nodiscard]] std::expected<void, StarException> addHitRequest(RemoteHitRequest const& remoteHitRequest);
  [[nodiscard]] std::expected<void, StarException> addDamageRequest(RemoteDamageRequest remoteDamageRequest);
  [[nodiscard]] std::expected<void, StarException> addDamageNotification(RemoteDamageNotification remoteDamageNotification);

  std::reference_wrapper<World> m_world;
  ConnectionId m_connectionId;

  // Maps target entity to all of the recent damage events that entity has
  // received, to prevent rapidly repeating damage.
  HashMap<EntityId, List<EntityDamageEvent>> m_recentEntityDamages;

  List<RemoteHitRequest> m_pendingRemoteHitRequests;
  List<RemoteDamageRequest> m_pendingRemoteDamageRequests;
  List<RemoteDamageNotification> m_pendingRemoteNotifications;
  List<DamageNotification> m_pendingNotifications;
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
