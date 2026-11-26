#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.damage;
import star.damage_manager;
import star.damage_types;
import star.byte_array;
import star.data_stream;
import star.data_stream_devices;
import star.status_types;
import star.entity;
import star.file;
import star.format;
import star.game_test;
import star.game_types;
import star.list;
import star.poly;
import star.rect;
import star.root;
import star.string;
import star.vector;
import star.world_server;

using namespace Star;

namespace {

enum class Phase { Query, Hit, Damage, Notify };
using Event = std::tuple<Phase, EntityId, EntityId>;
using Trace = std::vector<Event>;

class TickEntity : public Entity {
public:
  TickEntity(Root& root, Trace& trace, TeamType team) : Entity(root), trace(trace) {
    setTeam(EntityDamageTeam(team));
    setPersistent(false);
  }

  EntityType entity_type() const override { return EntityType::Stagehand; }
  Vec2F position() const override { return {1024, 1024}; }
  RectF meta_bound_box() const override { return RectF(-1, -1, 1, 1); }

  std::expected<List<DamageSource>, StarException> damage_sources() const override {
    return sources;
  }

  std::expected<std::optional<HitType>, StarException> query_hit(DamageSource const& source) const override {
    trace.emplace_back(Phase::Query, entityId(), source.source_entity_id);
    return HitType::StrongHit;
  }

  std::expected<void, StarException> hitOther(EntityId target_id, DamageRequest const& request) override {
    trace.emplace_back(Phase::Hit, entityId(), target_id);
    EXPECT_EQ(request.hit_type, HitType::StrongHit);
    if (on_hit)
      return on_hit(target_id);
    return {};
  }

  std::expected<List<DamageNotification>, StarException> apply_damage(DamageRequest const& request) override {
    trace.emplace_back(Phase::Damage, entityId(), request.source_entity_id);
    return List<DamageNotification>{{request.source_entity_id, entityId(), position(), request.damage, request.damage,
        request.hit_type, request.damage_source_kind, "flesh"}};
  }

  std::expected<void, StarException> damaged_other(DamageNotification const& notification) override {
    trace.emplace_back(Phase::Notify, entityId(), notification.target_entity_id);
    return {};
  }

  List<DamageSource> sources;
  std::function<std::expected<void, StarException>(EntityId)> on_hit;

private:
  Trace& trace;
};

std::expected<std::shared_ptr<WorldServer>, StarException> damage_world(Root& root) {
  auto file = File::ephemeral_file(root.random());
  if (!file) return std::unexpected(std::move(file.error()));
  auto world = WorldServer::create(root, Vec2U(2048, 2048), *file);
  if (world) (*world)->setSpawningEnabled(false);
  return world;
}

DamageSource damage_source(EntityId origin) {
  DamageSource source;
  source.team = EntityDamageTeam(TeamType::Friendly);
  source.damage_area = PolyF(RectF(-1, -1, 1, 1));
  source.track_source_entity = true;
  source.source_entity_id = origin;
  source.damage_type = DamageType::Damage;
  source.damage = 3.5f;
  source.damage_source_kind = "test-hit";
  return source;
}

const GameTestRegistration callback_removal("DamageManagerTest", "DispatchSkipsRemovedTargetEvenWhenIdIsReused", [](GameTest& fixture) {
  auto world = damage_world(fixture.root());
  ASSERT_TRUE(world) << world.error().what();
  Trace trace;
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  DamageManager manager(**world, server_connection_id);

  auto add = [&](EntityId id, TeamType team) {
    auto entity = std::make_shared<TickEntity>(fixture.root(), trace, team);
    EXPECT_TRUE((*world)->addEntity(entity, id));
    return entity;
  };
  auto origin = add(100, TeamType::Friendly);
  auto causing = add(101, TeamType::Friendly);
  auto first = add(102, TeamType::Enemy);
  auto second = add(103, TeamType::Enemy);
  causing->sources.push_back(damage_source(origin->entityId()));
  causing->on_hit = [&](EntityId target_id) -> std::expected<void, StarException> {
    // EntityMap's candidate order is intentionally unspecified. Remove the
    // other candidate, not the one that happens to have a larger ID.
    auto removed_id = target_id == first->entityId() ? second->entityId() : first->entityId();
    auto removed = (*world)->removeEntity(removed_id, false);
    if (!removed) return removed;
    return (*world)->addEntity(std::make_shared<TickEntity>(fixture.root(), trace, TeamType::Enemy), removed_id);
  };
  auto result = manager.update(0);
  ASSERT_TRUE(result) << result.error().what();
  ASSERT_EQ(trace.size(), 5u);
  auto target = std::get<1>(trace[0]);
  auto other = target == 102 ? 103 : 102;
  EXPECT_EQ(trace, (Trace{{Phase::Query, target, 100}, {Phase::Query, other, 100}, {Phase::Hit, 101, target},
                          {Phase::Damage, target, 100}, {Phase::Notify, 100, target}}));
  auto local = manager.pull_pending_notifications();
  auto outbound = manager.pull_remote_damage_notifications();
  ASSERT_EQ(local.size(), 1u);
  ASSERT_EQ(outbound.size(), 1u);
  EXPECT_EQ(outbound.front().source_entity_id, 100);
  DataStreamBuffer legacy;
  ASSERT_TRUE(legacy << EntityId(100) << local.front());
  EXPECT_EQ(DataStreamBuffer::serialize(outbound.front()), legacy.data());
  for (auto const& notification : {std::cref(local.front()), std::cref(outbound.front().damage_notification)}) {
    EXPECT_EQ(notification.get().target_entity_id, target);
    EXPECT_EQ(notification.get().damage_source_kind, "test-hit");
    EXPECT_EQ(notification.get().hit_type, HitType::StrongHit);
    EXPECT_EQ(notification.get().health_lost, 3.5f);
  }

  // Keep one victim. NoDamage still hits; a failed hit never applies damage
  // or publishes a notification. Advance exactly to the cooldown boundary.
  ASSERT_TRUE((*world)->removeEntity(other, false));
  causing->on_hit = {};
  causing->sources.front().damage_type = NoDamage;
  trace.clear();
  ASSERT_TRUE(manager.update(1));
  EXPECT_EQ(trace, (Trace{{Phase::Query, target, 100}, {Phase::Hit, 101, target}}));
  EXPECT_TRUE(manager.pull_pending_notifications().empty());
  trace.clear();
  causing->sources.front().damage_type = DamageType::Damage;
  causing->on_hit = [](EntityId) -> std::expected<void, StarException> {
    return std::unexpected(StarException("intentional hit failure"));
  };
  auto failed = manager.update(1);
  ASSERT_FALSE(failed);
  EXPECT_NE(std::string_view(failed.error().what()).find("intentional hit failure"), std::string_view::npos);
  EXPECT_EQ(trace, (Trace{{Phase::Query, target, 100}, {Phase::Hit, 101, target}}));
  EXPECT_TRUE(manager.pull_pending_notifications().empty());
  EXPECT_TRUE(manager.pull_remote_damage_notifications().empty());
});

const GameTestRegistration cooldown_identity("DamageManagerTest", "ExpiredAndRemovedEntityKeysLeaveLiveStringGroups", [](GameTest& fixture) {
  auto world = damage_world(fixture.root());
  ASSERT_TRUE(world) << world.error().what();
  Trace trace;
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  DamageManager manager(**world, server_connection_id);
  auto target = std::make_shared<TickEntity>(fixture.root(), trace, TeamType::Enemy);
  ASSERT_TRUE((*world)->addEntity(target, 102));
  auto causing = std::make_shared<TickEntity>(fixture.root(), trace, TeamType::Friendly);
  ASSERT_TRUE((*world)->addEntity(causing, 101));

  auto implicit = damage_source(101);
  implicit.damage_repeat_timeout = 10;
  auto long_group = implicit;
  long_group.damage_repeat_group = "long";
  auto short_group = implicit;
  short_group.damage_repeat_group = "short";
  short_group.damage_repeat_timeout = 1;
  implicit.damage_source_kind = "implicit";
  long_group.damage_source_kind = "long";
  short_group.damage_source_kind = "short";
  causing->sources = {implicit, long_group, short_group, long_group};
  auto delivered = [&] {
    std::vector<String> kinds;
    for (auto& event : manager.pull_pending_notifications())
      kinds.emplace_back(std::move(event.damage_source_kind));
    return kinds;
  };
  ASSERT_TRUE(manager.update(0));
  EXPECT_EQ(delivered(), (std::vector<String>{"implicit", "long", "short"}));
  ASSERT_TRUE((*world)->removeEntity(101, false));
  // The implicit key disappears with its causing entity; named groups don't.
  ASSERT_TRUE(manager.update(0.5f));
  ASSERT_TRUE((*world)->addEntity(causing, 101));
  ASSERT_TRUE(manager.update(0));
  EXPECT_EQ(delivered(), (std::vector<String>{"implicit"}));
  ASSERT_TRUE(manager.update(0.5f));
  EXPECT_EQ(delivered(), (std::vector<String>{"short"}));
});

const GameTestRegistration remote_payloads("DamageManagerTest", "RemoteHitAndDamagePreserveOwnersOrderAndPayload", [](GameTest& fixture) {
  auto world = damage_world(fixture.root());
  ASSERT_TRUE(world) << world.error().what();
  Trace trace;
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  DamageManager manager(**world, server_connection_id);
  auto attacking = connection_entity_space(min_client_connection_id);
  auto defending = connection_entity_space(min_client_connection_id + 1);
  ASSERT_TRUE(attacking);
  ASSERT_TRUE(defending);
  EntityId const causing_id = attacking->first;
  EntityId const origin_id = 100;
  auto causing = std::make_shared<TickEntity>(fixture.root(), trace, TeamType::Friendly);
  ASSERT_TRUE((*world)->addEntity(causing, causing_id));
  for (EntityId id : {defending->first, defending->first + 1})
    ASSERT_TRUE((*world)->addEntity(std::make_shared<TickEntity>(fixture.root(), trace, TeamType::Enemy), id));
  auto source = damage_source(origin_id);
  source.status_effects = {{"slow", 2.5f}, {"burning", 5.0f}};
  causing->sources.push_back(source);
  auto tick = manager.update(0);
  ASSERT_TRUE(tick) << tick.error().what();
  auto hits = manager.pull_remote_hit_requests();
  auto damage = manager.pull_remote_damage_requests();
  ASSERT_EQ(hits.size(), 2u);
  ASSERT_EQ(damage.size(), 2u);
  ASSERT_EQ(trace.size(), 2u);
  for (std::size_t i = 0; i < hits.size(); ++i) {
    auto target_id = std::get<1>(trace[i]);
    EXPECT_EQ(trace[i], (Event{Phase::Query, target_id, origin_id}));
    EXPECT_EQ(hits[i].destination_connection(), min_client_connection_id);
    EXPECT_EQ(damage[i].destination_connection(), min_client_connection_id + 1);
    DamageRequest expected{HitType::StrongHit, DamageType::Damage, source.damage, {}, origin_id,
                           source.damage_source_kind, source.status_effects};
    DataStreamBuffer legacy;
    ASSERT_TRUE(legacy << causing_id << target_id << expected);
    EXPECT_EQ(DataStreamBuffer::serialize(hits[i]), legacy.data());
    EXPECT_EQ(DataStreamBuffer::serialize(damage[i]), legacy.data());
    EXPECT_FALSE(manager.push_remote_hit_request(hits[i]));
    EXPECT_FALSE(manager.push_remote_damage_request(damage[i]));
  }
  EXPECT_TRUE(manager.pull_pending_notifications().empty());
  EXPECT_TRUE(manager.pull_remote_damage_notifications().empty());
});

} // namespace
