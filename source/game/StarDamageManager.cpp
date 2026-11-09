import std;
import star.format;
import star.json;
import star.string;
import star.id_map;
import star.map;
import star.set;
import star.spatial_hash_2d;
import star.casting;
import star.bi_map;
import star.data_stream;
import star.game_types;
import star.poly;
import star.strong_typedef;
import star.data_stream_extra;
import star.logging;
import star.color;
import star.list;
import star.rpc_promise;
import star.light_source;
import star.entity;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
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
import star.root;

import star.damage_manager;

import star.entity_map;

import star.line;
import star.algorithm;
namespace Star {

ConnectionId RemoteHitRequest::destinationConnection() const {
  return connectionForEntity(causingEntityId);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteHitRequest const& hitRequest) {
  auto result = ds << hitRequest.causingEntityId << hitRequest.targetEntityId << hitRequest.damageRequest;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteHitRequest& hitRequest) {
  auto result = ds >> hitRequest.causingEntityId >> hitRequest.targetEntityId >> hitRequest.damageRequest;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

ConnectionId RemoteDamageRequest::destinationConnection() const {
  return connectionForEntity(targetEntityId);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageRequest const& damageRequest) {
  auto result = ds << damageRequest.causingEntityId << damageRequest.targetEntityId << damageRequest.damageRequest;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageRequest& damageRequest) {
  auto result = ds >> damageRequest.causingEntityId >> damageRequest.targetEntityId >> damageRequest.damageRequest;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageNotification const& notification) {
  auto result = ds << notification.sourceEntityId << notification.damageNotification;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageNotification& notification) {
  auto result = ds >> notification.sourceEntityId >> notification.damageNotification;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

DamageManager::DamageManager(World& world, ConnectionId connectionId) : m_world(world), m_connectionId(connectionId) {}

std::expected<void, StarException> DamageManager::update(float dt) {
  float const DefaultDamageTimeout = 1.0f;

  for (auto damageIt = m_recentEntityDamages.begin(); damageIt != m_recentEntityDamages.end();) {
    auto& events = damageIt->second;
    for (auto eventIt = events.begin(); eventIt != events.end();) {
      auto& event = *eventIt;
      event.timeout -= dt;
      if (event.timeout <= 0.0f || (std::holds_alternative<EntityId>(event.timeoutGroup)
          && !m_world.get().entity(std::get<EntityId>(event.timeoutGroup))))
        eventIt = events.erase(eventIt);
      else
        ++eventIt;
    }
    if (events.empty())
      damageIt = m_recentEntityDamages.erase(damageIt);
    else
      ++damageIt;
  }

  return m_world.get().forAllEntities([&self = *this, DefaultDamageTimeout](std::shared_ptr<Entity> const& causingEntity) -> std::expected<void, StarException> {
    auto sources = causingEntity->damageSources();
    if (!sources) return std::unexpected(std::move(sources.error()));
    for (auto& damageSource : *sources) {
      if (damageSource.trackSourceEntity)
        damageSource.translate(causingEntity->position());

      if (std::holds_alternative<PolyF>(damageSource.damageArea))
        self.m_world.get().root().spatialLogger().log_poly("world", std::get<PolyF>(damageSource.damageArea),
                                                           Color::Orange.toRgba());
      else if (std::holds_alternative<Line2F>(damageSource.damageArea))
        self.m_world.get().root().spatialLogger().log_line("world", std::get<Line2F>(damageSource.damageArea),
                                                           Color::Orange.toRgba());

      auto hits = self.queryHit(damageSource, causingEntity->entityId());
      if (!hits) return std::unexpected(std::move(hits.error()));
      for (auto const& hitResultPair : *hits) {
        auto targetEntity = self.m_world.get().entity(hitResultPair.first);
        if (!self.isAuthoritative(causingEntity, targetEntity))
          continue;

        auto& eventList = self.m_recentEntityDamages[hitResultPair.first];
        // Guard against rapidly repeating damages by either the causing
        // entity id, or optionally the repeat group if specified.
        bool allowDamage = true;
        for (auto const& event : eventList) {
          if (damageSource.damageRepeatGroup) {
            if (std::holds_alternative<String>(event.timeoutGroup) && std::get<String>(event.timeoutGroup) == *damageSource.damageRepeatGroup)
              allowDamage = false;
          } else {
            if (std::holds_alternative<EntityId>(event.timeoutGroup) && std::get<EntityId>(event.timeoutGroup) == causingEntity->entityId())
              allowDamage = false;
          }
        }
        if (allowDamage) {
          float timeout = damageSource.damageRepeatTimeout.value_or(DefaultDamageTimeout);
          if (damageSource.damageRepeatGroup)
            eventList.push_back({*damageSource.damageRepeatGroup, timeout});
          else
            eventList.push_back({causingEntity->entityId(), timeout});

          auto damageRequest = DamageRequest(hitResultPair.second, damageSource.damageType, damageSource.damage,
              damageSource.knockbackMomentum(self.m_world.get().geometry(), targetEntity->position()),
              damageSource.sourceEntityId, damageSource.damageSourceKind, damageSource.statusEffects);
          auto hit = self.addHitRequest({causingEntity->entityId(), targetEntity->entityId(), damageRequest});
          if (!hit) return std::unexpected(std::move(hit.error()));

          if (damageSource.damageType != NoDamage) {
            auto damage = self.addDamageRequest({causingEntity->entityId(), targetEntity->entityId(), std::move(damageRequest)});
            if (!damage) return std::unexpected(std::move(damage.error()));
          }
        }
      }
    }

    auto notifications = causingEntity->selfDamageNotifications();
    if (!notifications) return std::unexpected(std::move(notifications.error()));
    for (auto const& notification : *notifications) {
      auto result = self.addDamageNotification({causingEntity->entityId(), notification});
      if (!result) return std::unexpected(std::move(result.error()));
    }
    return {};
  });
}

std::expected<void, StarException> DamageManager::pushRemoteHitRequest(RemoteHitRequest const& remoteHitRequest) {
  if (remoteHitRequest.destinationConnection() != m_connectionId)
    return std::unexpected(StarException("RemoteDamageRequest routed to wrong DamageManager"));

  if (auto causingEntity = m_world.get().entity(remoteHitRequest.causingEntityId)) {
    if constexpr (Star::DebugEnabled) {
      Star::star_assert_failed((causingEntity->isMaster()));
    }
    return causingEntity->hitOther(remoteHitRequest.targetEntityId, remoteHitRequest.damageRequest);
  }
  return {};
}

std::expected<void, StarException> DamageManager::pushRemoteDamageRequest(RemoteDamageRequest const& remoteDamageRequest) {
  if (remoteDamageRequest.destinationConnection() != m_connectionId)
    return std::unexpected(StarException("RemoteDamageRequest routed to wrong DamageManager"));

  if (auto targetEntity = m_world.get().entity(remoteDamageRequest.targetEntityId)) {
    if constexpr (Star::DebugEnabled) {
      Star::star_assert_failed((targetEntity->isMaster()));
    }
    auto notifications = targetEntity->applyDamage(remoteDamageRequest.damageRequest);
    if (!notifications) return std::unexpected(std::move(notifications.error()));
    for (auto& notification : *notifications) {
      auto result = addDamageNotification({remoteDamageRequest.damageRequest.sourceEntityId, std::move(notification)});
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> DamageManager::pushRemoteDamageNotification(RemoteDamageNotification remoteDamageNotification) {
  if (auto sourceEntity = m_world.get().entity(remoteDamageNotification.sourceEntityId)) {
    if (sourceEntity->isMaster()
        && sourceEntity->entityId() != remoteDamageNotification.damageNotification.targetEntityId) {
      auto result = sourceEntity->damagedOther(remoteDamageNotification.damageNotification);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  m_pendingNotifications.push_back(std::move(remoteDamageNotification.damageNotification));
  return {};
}

List<RemoteHitRequest> DamageManager::pullRemoteHitRequests() { return std::exchange(m_pendingRemoteHitRequests, {}); }

List<RemoteDamageRequest> DamageManager::pullRemoteDamageRequests() {
  return std::exchange(m_pendingRemoteDamageRequests, {});
}

List<RemoteDamageNotification> DamageManager::pullRemoteDamageNotifications() {
  return std::exchange(m_pendingRemoteNotifications, {});
}

List<DamageNotification> DamageManager::pullPendingNotifications() { return std::exchange(m_pendingNotifications, {}); }

std::expected<SmallList<std::pair<EntityId, HitType>, 4>, StarException> DamageManager::queryHit(DamageSource const& source, EntityId causingId) const {
  SmallList<std::pair<EntityId, HitType>, 4> resultList;
  auto doQueryHit = [&source, &resultList, causingId, &world = m_world.get()](std::shared_ptr<Entity> const& targetEntity) -> std::expected<void, StarException> {
    if (targetEntity->entityId() == causingId)
      return {};
    if (!source.team.canDamage(targetEntity->getTeam(), targetEntity->entityId() == source.sourceEntityId))
      return {};
    if (source.rayCheck) {
      if (std::holds_alternative<PolyF>(source.damageArea)) {
        if (auto sourceEntity = world.entity(source.sourceEntityId)) {
          auto overlap = world.geometry().rectOverlap(targetEntity->metaBoundBox().translated(targetEntity->position()), std::get<PolyF>(source.damageArea).boundBox());
          if (!overlap.isEmpty()) {
            auto collision = world.lineTileCollision(overlap.center(), sourceEntity->position());
            if (!collision) return std::unexpected(std::move(collision.error()));
            if (*collision) return {};
          }
        }
      } else if (std::holds_alternative<Line2F>(source.damageArea)) {
        auto hitPoly = targetEntity->hitPoly();
        if (!hitPoly) return std::unexpected(std::move(hitPoly.error()));
        if (*hitPoly) {
          auto intersection = world.geometry().lineIntersectsPolyAt(std::get<Line2F>(source.damageArea), **hitPoly);
          if (!intersection) return std::unexpected(std::move(intersection.error()));
          if (*intersection) {
            auto collision = world.lineTileCollision(std::get<Line2F>(source.damageArea).min(), **intersection);
            if (!collision) return std::unexpected(std::move(collision.error()));
            if (*collision) return {};
          }
        }
      }
    }
    auto hitResult = targetEntity->queryHit(source);
    if (!hitResult) return std::unexpected(std::move(hitResult.error()));
    if (*hitResult)
      resultList.push_back({targetEntity->entityId(), **hitResult});
    return {};
  };
  if (std::holds_alternative<PolyF>(source.damageArea)) {
    auto result = m_world.get().forEachEntity(std::get<PolyF>(source.damageArea).boundBox(), doQueryHit);
    if (!result) return std::unexpected(std::move(result.error()));
  } else if (std::holds_alternative<Line2F>(source.damageArea)) {
    auto const& line = std::get<Line2F>(source.damageArea);
    auto result = m_world.get().forEachEntityLine(line.min(), line.max(), doQueryHit);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return resultList;
}

bool DamageManager::isAuthoritative(std::shared_ptr<Entity> const& causingEntity, std::shared_ptr<Entity> const& targetEntity) {
  // Damage manager is authoritative if either one of the entities is
  // masterOnly, OR the manager is server-side and both entities are
  // server-side master entities, OR the damage manager is server-side and both
  // entities are different clients, OR if the manager is client-side and the
  // source is client-side master and the target is server-side master, OR if
  // the manager is client-side and the target is client-side master.
  //
  // This means that PvE and EvP are both decided on the player doing the
  // hitting or getting hit, and PvP is decided on the server, except for
  // master-only entities whose interactions are always decided on the machine
  // they are residing on.

  auto causingClient = connectionForEntity(causingEntity->entityId());
  auto targetClient = connectionForEntity(targetEntity->entityId());

  if (causingEntity->masterOnly() || targetEntity->masterOnly())
    return true;
  else if (causingClient == ServerConnectionId && targetClient == ServerConnectionId)
    return m_connectionId == ServerConnectionId;
  else if (causingClient != ServerConnectionId && targetClient != ServerConnectionId && causingClient != targetClient)
    return m_connectionId == ServerConnectionId;
  else if (targetClient == ServerConnectionId)
    return causingClient == m_connectionId;
  else
    return targetClient == m_connectionId;
}

std::expected<void, StarException> DamageManager::addHitRequest(RemoteHitRequest const& remoteHitRequest) {
  if (remoteHitRequest.destinationConnection() == m_connectionId)
    return pushRemoteHitRequest(remoteHitRequest);
  m_pendingRemoteHitRequests.push_back(remoteHitRequest);
  return {};
}

std::expected<void, StarException> DamageManager::addDamageRequest(RemoteDamageRequest remoteDamageRequest) {
  if (remoteDamageRequest.destinationConnection() == m_connectionId)
    return pushRemoteDamageRequest(std::move(remoteDamageRequest));
  m_pendingRemoteDamageRequests.push_back(std::move(remoteDamageRequest));
  return {};
}

std::expected<void, StarException> DamageManager::addDamageNotification(RemoteDamageNotification remoteDamageNotification) {
  auto result = pushRemoteDamageNotification(remoteDamageNotification);
  if (!result) return std::unexpected(std::move(result.error()));
  m_pendingRemoteNotifications.push_back(std::move(remoteDamageNotification));
  return {};
}

}
