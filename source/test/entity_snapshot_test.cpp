#include "gtest/gtest.h"

import std;
import star.actor_movement_controller;
import star.algorithm;
import star.byte_array;
import star.casting;
import star.encode;
import star.entity;
import star.entity_factory;
import star.file;
import star.format;
import star.game_test;
import star.game_types;
import star.item;
import star.item_descriptor;
import star.item_drop;
import star.json;
import star.json_extra;
import star.list;
import star.lua;
import star.lua_root;
import star.mobile_entity;
import star.movement_controller;
import star.net_compatibility;
import star.net_packets;
import star.plant;
import star.plant_drop;
import star.player;
import star.player_factory;
import star.projectile;
import star.projectile_database;
import star.root;
import star.set;
import star.string;
import star.vector;
import star.versioning_database;
import star.world_lua_bindings;
import star.world_server;
import star.warping;

using namespace Star;

namespace {
struct SnapshotWorld {
  ~SnapshotWorld() {
    context.reset();
    lua.reset();
    if (world)
      EXPECT_TRUE(world->close());
  }

  std::expected<void, StarException> init(Root& root) {
    auto file = File::ephemeral_file(root.random());
    if (!file)
      return std::unexpected(std::move(file.error()));
    auto created = WorldServer::create(root, Vec2U(2048, 2048), *file);
    if (!created)
      return std::unexpected(std::move(created.error()));
    world = std::move(*created);
    world->setSpawningEnabled(false);
    auto madeLua = LuaRoot::create(root);
    if (!madeLua)
      return std::unexpected(std::move(madeLua.error()));
    lua = std::move(*madeLua);
    auto madeContext = lua->create_context(StringList{});
    if (!madeContext)
      return std::unexpected(std::move(madeContext.error()));
    context = std::move(*madeContext);
    auto callbacks = LuaBindings::makeWorldCallbacks(root, *world);
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    return context->set_callbacks("world", *callbacks);
  }

  std::shared_ptr<WorldServer> world;
  std::shared_ptr<LuaRoot> lua;
  std::optional<LuaContext> context;
};

std::expected<Json, StarException> snapshot(SnapshotWorld& setup, EntityId id) {
  return setup.context->invoke_path<Json>("world.entitySnapshot", id);
}

const GameTestRegistration missingSnapshot("EntitySnapshot", "MissingEntityReturnsNil", [](GameTest& fixture) {
  SnapshotWorld setup;
  auto initialized = setup.init(fixture.root());
  ASSERT_TRUE(initialized) << initialized.error().what();
  auto result = setup.context->eval<bool>("return world.entitySnapshot(2147483647) == nil");
  ASSERT_TRUE(result) << result.error().what();
  EXPECT_TRUE(*result);
});

const GameTestRegistration persistentSnapshot("EntitySnapshot", "VersionedPersistentEntityRoundTrips", [](GameTest& fixture) {
  SnapshotWorld setup;
  auto initialized = setup.init(fixture.root());
  ASSERT_TRUE(initialized) << initialized.error().what();
  auto descriptor = ItemDescriptor::create("dirtmaterial", 7, JsonObject{{"snapshotMarker", "native disk payload"}});
  ASSERT_TRUE(descriptor) << descriptor.error().what();
  auto drop = ItemDrop::createRandomizedDrop(fixture.root(), *descriptor, Vec2F(1024, 1024));
  ASSERT_TRUE(drop) << drop.error().what();
  ASSERT_TRUE(setup.world->addEntity(*drop));
  auto original = (*drop)->diskStore();
  ASSERT_TRUE(original) << original.error().what();
  auto result = snapshot(setup, (*drop)->entityId());
  ASSERT_TRUE(result) << result.error().what();
  EXPECT_EQ(result->get_string("schema"), String("asterbound.entitySnapshot"));
  EXPECT_EQ(result->get_int("schemaVersion"), 1);
  EXPECT_EQ(result->get_string("type"), String("itemDrop"));
  EXPECT_EQ(result->get_int("entityId"), (*drop)->entityId());
  EXPECT_EQ(result->get("position"), json_from_vec2f((*drop)->position()));
  EXPECT_EQ(result->get_string("authority"), String("master"));
  EXPECT_EQ(result->get_bool("complete"), true);
  EXPECT_EQ(result->get_string("format"), String("disk"));
  auto data = result->get("data");
  ASSERT_TRUE(data);
  auto versioned = VersionedJson::fromJson(*data);
  ASSERT_TRUE(versioned) << versioned.error().what();
  EXPECT_EQ(versioned->identifier, String("ItemDropEntity"));
  EXPECT_EQ(versioned->content, *original);
  auto factory = fixture.root().entityFactory();
  ASSERT_TRUE(factory) << factory.error().what();
  auto restored = (*factory)->loadVersionedEntity(*versioned);
  ASSERT_TRUE(restored) << restored.error().what();
  auto restoredDrop = as<ItemDrop>(*restored);
  ASSERT_TRUE(restoredDrop);
  EXPECT_EQ(restoredDrop->item()->descriptor(), *descriptor);
  EXPECT_EQ(restoredDrop->position(), (*drop)->position());
  EXPECT_EQ((*drop)->diskStore(), *original);
  EXPECT_EQ(snapshot(setup, (*drop)->entityId()), *result);
});

const GameTestRegistration replicaPlayerSnapshot("EntitySnapshot", "RemotePlayerDiskSnapshotIsExplicitlyNonAuthoritative", [](GameTest& fixture) {
  SnapshotWorld setup;
  auto initialized = setup.init(fixture.root());
  ASSERT_TRUE(initialized) << initialized.error().what();
  auto factory = fixture.root().playerFactory();
  ASSERT_TRUE(factory) << factory.error().what();
  auto player = (*factory)->create();
  ASSERT_TRUE(player) << player.error().what();
  ASSERT_TRUE((*player)->setName("Snapshot remote player"));
  ASSERT_TRUE((*player)->finalizeCreation());
  (*player)->setGenericProperty("snapshotUnsentSecret", Json("not transmitted"));
  auto originalDisk = (*player)->diskStore();
  ASSERT_TRUE(originalDisk) << originalDisk.error().what();
  NetCompatibilityRules rules;
  auto store = (*player)->netStore(rules);
  ASSERT_TRUE(store) << store.error().what();
  auto state = (*player)->writeNetState(0, rules);
  ASSERT_TRUE(state) << state.error().what();
  auto joined = setup.world->addClient(MinClientConnectionId, SpawnTargetPosition(Vec2F(1024, 1024)), true, false, rules);
  ASSERT_TRUE(joined) << joined.error().what();
  ASSERT_TRUE(*joined);
  auto space = connectionEntitySpace(MinClientConnectionId);
  ASSERT_TRUE(space) << space.error().what();
  EntityId id = space->first;
  auto packet = std::make_shared<EntityCreatePacket>(EntityType::Player, *store, state->first, id);
  ASSERT_TRUE(setup.world->handleIncomingPackets(MinClientConnectionId,
      {std::make_shared<WorldStartAcknowledgePacket>(), packet}));
  auto remote = as<Player>(setup.world->entity(id));
  ASSERT_TRUE(remote) << "real EntityCreate must publish a slave player";
  ASSERT_TRUE(remote->isSlave());
  EXPECT_EQ(remote->getGenericProperty("snapshotUnsentSecret"), Json());
  // Locally present replica-only data must still use the full disk dispatcher.
  remote->setGenericProperty("snapshotLocalReplicaMarker", Json("retained"));
  auto localDisk = remote->diskStore();
  ASSERT_TRUE(localDisk) << localDisk.error().what();
  auto result = snapshot(setup, id);
  ASSERT_TRUE(result) << result.error().what();
  EXPECT_EQ(result->get_string("type"), String("player"));
  EXPECT_EQ(result->get_string("authority"), String("replica"));
  EXPECT_EQ(result->get_bool("complete"), false);
  EXPECT_EQ(result->get_string("format"), String("disk"));
  auto data = result->get("data");
  ASSERT_TRUE(data);
  auto versioned = VersionedJson::fromJson(*data);
  ASSERT_TRUE(versioned) << versioned.error().what();
  EXPECT_EQ(versioned->identifier, String("PlayerEntity"));
  EXPECT_EQ(versioned->content, *localDisk);
  EXPECT_NE(versioned->content, *originalDisk);
  auto inventory = versioned->content.get("inventory");
  ASSERT_TRUE(inventory);
  auto quests = versioned->content.get("quests");
  ASSERT_TRUE(quests);
  auto scripts = versioned->content.get("genericScriptStorage");
  ASSERT_TRUE(scripts);
  auto properties = versioned->content.get("genericProperties");
  ASSERT_TRUE(properties);
  EXPECT_EQ(properties->get_string("snapshotLocalReplicaMarker"), String("retained"));
  auto warnings = result->get_array("warnings");
  ASSERT_TRUE(warnings);
  ASSERT_FALSE(warnings->empty());
  bool unsynchronizedWarning = false;
  for (auto const& warning : *warnings) {
    auto text = warning.to_string();
    ASSERT_TRUE(text);
    auto contains = text->contains("not synchronized");
    ASSERT_TRUE(contains) << contains.error().what();
    unsynchronizedWarning |= *contains;
  }
  EXPECT_TRUE(unsynchronizedWarning);
  EXPECT_EQ(remote->diskStore(), *localDisk);
});

const GameTestRegistration transientSnapshots("EntitySnapshot", "TransientStateRestoresWithoutConsumingReplication", [](GameTest& fixture) {
  SnapshotWorld setup;
  auto initialized = setup.init(fixture.root());
  ASSERT_TRUE(initialized) << initialized.error().what();
  // Projectile::init deliberately clears a source that is absent from its
  // world. Keep a real source alive so the netStore assertion is meaningful.
  auto sourceDescriptor = ItemDescriptor::create("dirtmaterial", 1);
  ASSERT_TRUE(sourceDescriptor) << sourceDescriptor.error().what();
  auto source = ItemDrop::createRandomizedDrop(fixture.root(), *sourceDescriptor, Vec2F(1024, 1024));
  ASSERT_TRUE(source) << source.error().what();
  ASSERT_TRUE(setup.world->addEntity(*source, 4242));
  auto database = fixture.root().projectileDatabase();
  ASSERT_TRUE(database) << database.error().what();
  auto projectile = (*database)->createProjectile("grenade", JsonObject{{"actionOnReap", JsonArray{}}, {"actionOnHit", JsonArray{}}});
  ASSERT_TRUE(projectile) << projectile.error().what();
  (*projectile)->setInitialPosition(Vec2F(1024, 1024));
  (*projectile)->setInitialVelocity(Vec2F(3, 5));
  (*projectile)->setSourceEntity(4242, false);
  Plant::PlantPiece piece;
  piece.image = "/items/materials/dirt.png";
  piece.spaces.add(Vec2I(0, 0));
  piece.structuralSegment = true;
  piece.kind = Plant::PlantPieceKind::Stem;
  auto plantDrop = PlantDrop::create(fixture.root(), {piece}, Vec2F(1028, 1024), Vec2F(1, 0), "Snapshot plant piece", false,
      JsonObject{}, JsonObject{}, JsonObject{}, true, 0.25f);
  ASSERT_TRUE(plantDrop) << plantDrop.error().what();
  auto factory = fixture.root().entityFactory();
  ASSERT_TRUE(factory) << factory.error().what();
  NetCompatibilityRules rules;
  for (std::shared_ptr<Entity> const& entity : {std::shared_ptr<Entity>(*projectile), std::shared_ptr<Entity>(*plantDrop)}) {
    SCOPED_TRACE(static_cast<unsigned>(entity->entityType()));
    ASSERT_TRUE(setup.world->addEntity(entity));
    auto baseline = entity->writeNetState(0, rules);
    ASSERT_TRUE(baseline) << baseline.error().what();
    auto result = snapshot(setup, entity->entityId());
    ASSERT_TRUE(result) << result.error().what();
    EXPECT_EQ(result->get_string("format"), String("net"));
    EXPECT_EQ(result->get_string("authority"), String("master"));
    EXPECT_EQ(result->get_bool("complete"), false);
    auto data = result->get("data");
    ASSERT_TRUE(data);
    EXPECT_EQ(data->get_string("encoding"), String("base64"));
    EXPECT_EQ(data->get_uint("compatibilityVersion"), rules.version());
    EXPECT_EQ(data->get_uint("netStateVersion"), baseline->second);
    auto store = data->get_string("netStore");
    auto state = data->get_string("netState");
    ASSERT_TRUE(store);
    ASSERT_TRUE(state);
    ASSERT_FALSE(store->empty());
    ASSERT_FALSE(state->empty());
    auto decodedState = base64_decode(state->utf8());
    EXPECT_EQ(decodedState, baseline->first);
    auto restored = (*factory)->netLoadEntity(entity->entityType(), base64_decode(store->utf8()), rules);
    ASSERT_TRUE(restored) << restored.error().what();
    ASSERT_TRUE((*restored)->readNetState(decodedState, 0.f, rules));
    EXPECT_EQ((*restored)->position(), entity->position());
    if (auto restoredProjectile = as<Projectile>(*restored))
      EXPECT_EQ(restoredProjectile->sourceEntity(), 4242);
    else
      EXPECT_EQ((*restored)->description(), String("Snapshot plant piece"));
    EXPECT_EQ(snapshot(setup, entity->entityId()), *result);
    auto noChange = entity->writeNetState(baseline->second, rules);
    ASSERT_TRUE(noChange) << noChange.error().what();
    EXPECT_TRUE(noChange->first.empty());
    EXPECT_EQ(noChange->second, baseline->second);

    auto mobile = as<MobileEntity>(entity);
    ASSERT_TRUE(mobile);
    mobile->movementController().setPosition(Vec2F(1035, 1041));
    auto movedSnapshot = snapshot(setup, entity->entityId());
    ASSERT_TRUE(movedSnapshot) << movedSnapshot.error().what();
    auto movedData = movedSnapshot->get("data");
    ASSERT_TRUE(movedData);
    EXPECT_EQ(movedData->get_uint("netStateVersion"), baseline->second);
    auto movedState = movedData->get_string("netState");
    ASSERT_TRUE(movedState);
    ASSERT_TRUE((*restored)->readNetState(base64_decode(movedState->utf8()), 0.f, rules));
    EXPECT_EQ((*restored)->position(), entity->position());
    auto delta = entity->writeNetState(baseline->second, rules);
    ASSERT_TRUE(delta) << delta.error().what();
    EXPECT_FALSE(delta->first.empty());
    EXPECT_EQ(delta->second, baseline->second + 1);
    ASSERT_TRUE((*restored)->readNetState(baseline->first, 0.f, rules));
    ASSERT_TRUE((*restored)->readNetState(delta->first, 0.f, rules));
    EXPECT_EQ((*restored)->position(), entity->position());
  }
});
} // namespace
