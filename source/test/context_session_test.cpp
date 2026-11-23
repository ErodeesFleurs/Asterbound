import std;
import star.byte_array;
import star.celestial_coordinate;
import star.damage_types;
import star.file;
import star.format;
import star.game_test;
import star.game_types;
import star.host_address;
import star.json;
import star.list;
import star.map;
import star.net_compatibility;
import star.root;
import star.server_client_context;
import star.set;
import star.string;
import star.system_world;
import star.uuid;
import star.vector;
import star.warping;
import star.world_server;
import star.world_server_thread;
#include "gtest/gtest.h"

using namespace Star;

namespace {
auto create_context(GameTest& fixture) {
  return ServerClientContext::create(fixture.root(), 1, {}, NetCompatibilityRules(15u), Uuid::nil(), "player", "human", true, {}, {}, {});
}

const GameTestRegistration persisted_state("ContextSession", "LateLoadErrorLeavesAllPersistedFieldsUnchanged", [](GameTest& fixture) {
  auto created = create_context(fixture);
  ASSERT_TRUE(created) << created.error().what();
  auto& context = **created;
  auto original = context.store_server_data();
  ASSERT_TRUE(original) << original.error().what();

  CelestialCoordinate planet(Vec3I(11, -7, 3), 2);
  context.set_ship_coordinate(planet);
  context.set_ship_location(SystemLocation(planet));
  context.set_admin(true);
  context.set_team(EntityDamageTeam(TeamType::PVP, 12));
  context.set_player_revive_warp(WarpToWorld(CelestialWorldId(planet)));
  context.set_player_return_warp(WarpToWorld(ClientShipWorldId(Uuid::nil())));
  auto updated = context.store_server_data();
  ASSERT_TRUE(updated) << updated.error().what();
  ASSERT_NE(*original, *updated);

  ASSERT_TRUE(context.load_server_data(*original));
  auto malformed = updated->set("returnWarp", JsonObject{{"world", 7}});
  ASSERT_TRUE(malformed);
  EXPECT_FALSE(context.load_server_data(*malformed));
  auto retained = context.store_server_data();
  ASSERT_TRUE(retained);
  EXPECT_EQ(*retained, *original);

  ASSERT_TRUE(context.load_server_data(*updated));
  auto restored = context.store_server_data();
  ASSERT_TRUE(restored);
  EXPECT_EQ(*restored, *updated);
});

const GameTestRegistration sub_world_handles("ContextSession", "NullAndClearEraseOnlyTheSelectedSubWorld", [](GameTest& fixture) {
  auto created = create_context(fixture);
  ASSERT_TRUE(created) << created.error().what();
  auto& context = **created;
  context.set_sub_world(17, {});
  EXPECT_FALSE(context.has_sub_world(17));
  EXPECT_FALSE(context.sub_world(17));
  EXPECT_TRUE(context.sub_worlds().empty());

  auto storage = File::ephemeral_file(fixture.root().random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto world = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto thread = WorldServerThread::create(*world, ClientShipWorldId(Uuid::nil()));
  ASSERT_TRUE(thread) << thread.error().what();
  context.set_sub_world(17, *thread);
  context.set_sub_world(18, *thread);
  EXPECT_EQ(context.sub_world(17), *thread);
  context.set_sub_world(17, {});
  EXPECT_FALSE(context.has_sub_world(17));
  EXPECT_EQ(context.sub_worlds(), List<ClientSubWorldId>{18});
  context.clear_sub_world(18);
  context.clear_sub_world(18);
  EXPECT_FALSE(context.sub_world(18));
  EXPECT_TRUE(context.sub_worlds().empty());
  EXPECT_TRUE((*thread)->close());
});
}
