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
import star.rect;
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

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteHitRequest const& request) {
  auto result = ds << request.causing_entity_id << request.target_entity_id << request.damage_request;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteHitRequest& request) {
  auto result = ds >> request.causing_entity_id >> request.target_entity_id >> request.damage_request;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageRequest const& request) {
  auto result = ds << request.causing_entity_id << request.target_entity_id << request.damage_request;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageRequest& request) {
  auto result = ds >> request.causing_entity_id >> request.target_entity_id >> request.damage_request;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RemoteDamageNotification const& notification) {
  auto result = ds << notification.source_entity_id << notification.damage_notification;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RemoteDamageNotification& notification) {
  auto result = ds >> notification.source_entity_id >> notification.damage_notification;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

namespace {
bool still_in_world(Entity const& entity, EntityId id, World const& world) {
  if (entity.entityId() != id)
    return false;
  auto binding = entity.maybeWorld();
  return binding && std::addressof(binding->get()) == std::addressof(world);
}
} // namespace

DamageManager::DamageManager(World& world, ConnectionId connection_id) : m_world(world), m_connection_id(connection_id) {}

std::expected<void, StarException> DamageManager::update(float dt) {
  auto& world = m_world.get();
  for (auto bucket = m_recent_entity_damages.begin(); bucket != m_recent_entity_damages.end();) {
    auto& events = bucket->second;
    auto kept = events.begin();
    for (auto it = events.begin(); it != events.end(); ++it) {
      it->timeout -= dt;
      if (it->timeout <= 0.0f || (std::holds_alternative<EntityId>(it->timeout_group)
          && !world.entity(std::get<EntityId>(it->timeout_group))))
        continue;
      if (kept != it)
        *kept = std::move(*it);
      ++kept;
    }
    events.erase(kept, events.end());
    if (events.empty())
      bucket = m_recent_entity_damages.erase(bucket);
    else
      ++bucket;
  }

  for (auto id : world.entity_ids()) {
    if (auto entity = world.entity(id)) {
      auto result = update_entity(*entity);
      if (!result) return result;
    }
  }
  return {};
}

std::expected<void, StarException> DamageManager::update_entity(Entity& causing_entity) {
  auto& world = m_world.get();
  auto const causing_id = causing_entity.entityId();
  auto sources = causing_entity.damage_sources();
  if (!sources) return std::unexpected(std::move(sources.error()));
  for (auto& source : *sources) {
    if (!still_in_world(causing_entity, causing_id, world))
      return {};
    if (source.track_source_entity)
      source.translate(causing_entity.position());

    if (std::holds_alternative<PolyF>(source.damage_area))
      world.root().spatialLogger().log_poly("world", std::get<PolyF>(source.damage_area), Color::palette_orange.to_rgba());
    else if (std::holds_alternative<Line2F>(source.damage_area))
      world.root().spatialLogger().log_line("world", std::get<Line2F>(source.damage_area), Color::palette_orange.to_rgba());

    auto hits = query_hit(source, causing_id);
    if (!hits) return std::unexpected(std::move(hits.error()));
    // Query callbacks have finished. The source's payload can now be reused
    // for every target; only queued requests need their own copy.
    std::optional<DamageRequest> request;
    auto remaining = hits->size();
    for (auto const& [target, target_id, hit_type] : *hits) {
      --remaining;
      if (!still_in_world(causing_entity, causing_id, world))
        return {};
      // An earlier hit callback may have removed a later target.
      if (!still_in_world(*target, target_id, world) || !is_authoritative(causing_entity, *target))
        continue;

      auto& events = m_recent_entity_damages[target_id];
      if (std::ranges::any_of(events, [&](EntityDamageEvent const& event) {
        if (source.damage_repeat_group)
          return std::holds_alternative<String>(event.timeout_group)
              && std::get<String>(event.timeout_group) == *source.damage_repeat_group;
        return std::holds_alternative<EntityId>(event.timeout_group)
            && std::get<EntityId>(event.timeout_group) == causing_id;
      }))
        continue;
      float timeout = source.damage_repeat_timeout.value_or(1.0f);
      if (source.damage_repeat_group)
        events.push_back({*source.damage_repeat_group, timeout});
      else
        events.push_back({causing_id, timeout});

      auto momentum = source.knockback_momentum(world.geometry(), target->position());
      if (!request)
        request.emplace(hit_type, source.damage_type, source.damage, momentum, source.source_entity_id,
                        std::move(source.damage_source_kind), std::move(source.status_effects));
      else {
        request->hit_type = hit_type;
        request->knockback_momentum = momentum;
      }
      auto result = dispatch_hit(causing_entity, *target, *request, remaining == 0);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }

  if (!still_in_world(causing_entity, causing_id, world))
    return {};
  auto notifications = causing_entity.self_damage_notifications();
  if (!notifications) return std::unexpected(std::move(notifications.error()));
  for (auto& notification : *notifications) {
    auto result = add_damage_notification(causing_id, std::move(notification));
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> DamageManager::push_remote_hit_request(RemoteHitRequest const& request) {
  if (request.destination_connection() != m_connection_id)
    return std::unexpected(StarException("RemoteHitRequest routed to wrong DamageManager"));
  if (auto entity = m_world.get().entity(request.causing_entity_id)) {
    if constexpr (Star::debug_enabled)
      Star::star_assert_failed(entity->isMaster());
    return entity->hit_other(request.target_entity_id, request.damage_request);
  }
  return {};
}

std::expected<void, StarException> DamageManager::push_remote_damage_request(RemoteDamageRequest const& request) {
  if (request.destination_connection() != m_connection_id)
    return std::unexpected(StarException("RemoteDamageRequest routed to wrong DamageManager"));
  if (auto entity = m_world.get().entity(request.target_entity_id))
    return apply_damage(*entity, request.damage_request);
  return {};
}

std::expected<void, StarException> DamageManager::apply_damage(Entity& target_entity, DamageRequest const& request) {
  if constexpr (Star::debug_enabled)
    Star::star_assert_failed(target_entity.isMaster());
  auto notifications = target_entity.apply_damage(request);
  if (!notifications) return std::unexpected(std::move(notifications.error()));
  for (auto& notification : *notifications) {
    auto result = add_damage_notification(request.source_entity_id, std::move(notification));
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> DamageManager::notify_source(EntityId source_id, DamageNotification const& notification) {
  if (source_id != notification.target_entity_id) {
    if (auto source = m_world.get().entity(source_id); source && source->isMaster())
      return source->damaged_other(notification);
  }
  return {};
}

std::expected<void, StarException> DamageManager::push_remote_damage_notification(RemoteDamageNotification const& notification) {
  auto result = notify_source(notification.source_entity_id, notification.damage_notification);
  if (!result) return result;
  m_pending_notifications.push_back(notification.damage_notification);
  return {};
}

List<RemoteHitRequest> DamageManager::pull_remote_hit_requests() { return std::exchange(m_pending_remote_hit_requests, {}); }

List<RemoteDamageRequest> DamageManager::pull_remote_damage_requests() {
  return std::exchange(m_pending_remote_damage_requests, {});
}

List<RemoteDamageNotification> DamageManager::pull_remote_damage_notifications() {
  return std::exchange(m_pending_remote_notifications, {});
}

List<DamageNotification> DamageManager::pull_pending_notifications() { return std::exchange(m_pending_notifications, {}); }

std::expected<SmallList<DamageManager::PendingHit, 4>, StarException> DamageManager::query_hit(DamageSource const& source, EntityId causing_id) const {
  SmallList<PendingHit, 4> hits;
  auto& world = m_world.get();
  bool const polygon = std::holds_alternative<PolyF>(source.damage_area);
  RectF const bounds = polygon ? std::get<PolyF>(source.damage_area).bounding_box() : RectF();
  auto collect = [&](std::shared_ptr<Entity> const& target) -> std::expected<void, StarException> {
    if (target->entityId() != causing_id)
      hits.emplace_back(target, target->entityId(), HitType::Hit);
    return {};
  };
  if (polygon) {
    auto result = world.forEachEntity(bounds, collect);
    if (!result) return std::unexpected(std::move(result.error()));
  } else if (std::holds_alternative<Line2F>(source.damage_area)) {
    auto const& line = std::get<Line2F>(source.damage_area);
    auto result = world.forEachEntityLine(line.min(), line.max(), collect);
    if (!result) return std::unexpected(std::move(result.error()));
  }

  // Spatial iteration borrows its index entries. Run entity callbacks only
  // after it returns: one candidate can remove a later candidate, even if
  // another entity is immediately assigned the same ID.
  auto query = [&](PendingHit const& candidate) -> std::expected<std::optional<HitType>, StarException> {
    auto const& target = candidate.target;
    if (!still_in_world(*target, candidate.target_id, world) || !source.team.can_damage(target->getTeam(), candidate.target_id == source.source_entity_id))
      return {};
    if (source.ray_check) {
      if (polygon) {
        if (auto origin = world.entity(source.source_entity_id)) {
          auto overlap = world.geometry().rect_overlap(target->meta_bound_box().translated(target->position()), bounds);
          if (!overlap.is_empty()) {
            auto collision = world.lineTileCollision(overlap.center(), origin->position());
            if (!collision) return std::unexpected(std::move(collision.error()));
            if (*collision) return {};
          }
        }
      } else {
        auto const& line = std::get<Line2F>(source.damage_area);
        auto hit_poly = target->hit_poly();
        if (!hit_poly) return std::unexpected(std::move(hit_poly.error()));
        if (!still_in_world(*target, candidate.target_id, world)) return {};
        if (*hit_poly) {
          auto intersection = world.geometry().line_intersects_poly_at(line, **hit_poly);
          if (!intersection) return std::unexpected(std::move(intersection.error()));
          if (*intersection) {
            auto collision = world.lineTileCollision(line.min(), **intersection);
            if (!collision) return std::unexpected(std::move(collision.error()));
            if (*collision) return {};
          }
        }
      }
    }
    return target->query_hit(source);
  };
  auto kept = hits.begin();
  for (auto it = hits.begin(); it != hits.end(); ++it) {
    auto hit = query(*it);
    if (!hit) return std::unexpected(std::move(hit.error()));
    if (*hit) {
      it->hit_type = **hit;
      if (kept != it)
        *kept = std::move(*it);
      ++kept;
    }
  }
  hits.erase(kept, hits.end());
  return hits;
}

bool DamageManager::is_authoritative(Entity const& causing_entity, Entity const& target_entity) const {
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

  if (causing_entity.masterOnly() || target_entity.masterOnly())
    return true;
  auto causing_owner = connection_for_entity(causing_entity.entityId());
  auto target_owner = connection_for_entity(target_entity.entityId());
  if (!causing_owner || !target_owner)
    return false;
  if (*causing_owner == server_connection_id && *target_owner == server_connection_id)
    return m_connection_id == server_connection_id;
  else if (*causing_owner != server_connection_id && *target_owner != server_connection_id && *causing_owner != *target_owner)
    return m_connection_id == server_connection_id;
  else if (*target_owner == server_connection_id)
    return *causing_owner == m_connection_id;
  else
    return *target_owner == m_connection_id;
}

std::expected<void, StarException> DamageManager::dispatch_hit(Entity& causing_entity, Entity& target_entity,
    DamageRequest& request, bool last_hit) {
  auto causing_id = causing_entity.entityId();
  auto target_id = target_entity.entityId();
  auto hit_owner = connection_for_entity(causing_id);
  if (!hit_owner)
    return std::unexpected(StarException("Cannot route hit from an unowned entity"));

  bool const hit_only = request.damage_type == NoDamage;
  if (*hit_owner == m_connection_id) {
    if constexpr (Star::debug_enabled)
      Star::star_assert_failed(causing_entity.isMaster());
    auto result = causing_entity.hit_other(target_id, request);
    if (!result) return result;
    if (!hit_only && !still_in_world(target_entity, target_id, m_world.get()))
      return {};
  } else if (hit_only && last_hit) {
    m_pending_remote_hit_requests.emplace_back(causing_id, target_id, std::move(request));
  } else {
    m_pending_remote_hit_requests.emplace_back(causing_id, target_id, request);
  }

  if (hit_only)
    return {};
  auto damage_owner = connection_for_entity(target_id);
  if (!damage_owner)
    return std::unexpected(StarException("Cannot route damage to an unowned entity"));
  if (*damage_owner == m_connection_id)
    return apply_damage(target_entity, request);
  if (last_hit)
    m_pending_remote_damage_requests.emplace_back(causing_id, target_id, std::move(request));
  else
    m_pending_remote_damage_requests.emplace_back(causing_id, target_id, request);
  return {};
}

std::expected<void, StarException> DamageManager::add_damage_notification(EntityId source_id, DamageNotification notification) {
  auto result = notify_source(source_id, notification);
  if (!result) return result;
  m_pending_notifications.push_back(notification);
  m_pending_remote_notifications.emplace_back(source_id, std::move(notification));
  return {};
}

}
