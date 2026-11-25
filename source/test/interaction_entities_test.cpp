#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.entity;
import star.file;
import star.game_test;
import star.game_types;
import star.interaction_types;
import star.interactive_entity;
import star.json;
import star.lua;
import star.monster;
import star.monster_database;
import star.npc;
import star.npc_database;
import star.object;
import star.object_database;
import star.root;
import star.scripted_entity;
import star.string;
import star.list;
import star.vector;
import star.vehicle;
import star.world_server;

using namespace Star;

namespace {

void expect_action(InteractiveEntity& entity, InteractRequest const& request, InteractActionType type, Json data = {}) {
  auto action = entity.interact(request);
  ASSERT_TRUE(action) << action.error().what();
  EXPECT_EQ(action->type, type);
  EXPECT_EQ(action->data, data);
  if (type != InteractActionType::None)
    EXPECT_EQ(action->entity_id, request.target_id);
}

void expect_actor_script(InteractiveEntity& entity, ScriptedEntity& script, EntityId id) {
  InteractRequest request{37, {13, 19}, id, {0, 0}};
  expect_action(entity, request, InteractActionType::None);
  ASSERT_TRUE(script.evalScript("function interact() return nil end"));
  expect_action(entity, request, InteractActionType::None);
  ASSERT_TRUE(script.evalScript("function interact(r) return {'Message', {r.sourceId, r.sourcePosition}} end"));
  expect_action(entity, request, InteractActionType::Message, JsonArray{37, JsonArray{13, 19}});
}

const GameTestRegistration entity_scripts("InteractionEntities", "ObjectNpcAndMonsterFallbacks", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto file = File::ephemeral_file(root.random());
  ASSERT_TRUE(file);
  auto world = WorldServer::create(root, {128, 128}, *file);
  ASSERT_TRUE(world) << world.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });

  auto objects = root.objectDatabase();
  ASSERT_TRUE(objects);
  auto object = (*objects)->createObject("stoneteleporter", JsonObject{{"scripts", JsonArray{}},
      {"interactAction", "ShowPopup"}, {"interactData", "configured"}});
  ASSERT_TRUE(object) << object.error().what();
  ASSERT_TRUE((*object)->setTilePosition({126, 53}));
  ASSERT_TRUE((*object)->init(**world, 1, EntityMode::Master));
  auto release_object = make_scope_exit([&] { EXPECT_TRUE((*object)->uninit()); });
  InteractRequest request{37, {1, 57}, 1, {0, 0}};
  expect_action(**object, request, InteractActionType::ShowPopup, "configured");
  ASSERT_TRUE((*object)->evalScript("function onInteraction() return nil end"));
  expect_action(**object, request, InteractActionType::None);
  ASSERT_TRUE((*object)->evalScript("function onInteraction(r) return {'Message', {r.sourceId, r.source}} end"));
  expect_action(**object, request, InteractActionType::Message, JsonArray{37, JsonArray{3, 4}});

  auto npcs = root.npcDatabase();
  ASSERT_TRUE(npcs);
  auto villager = (*npcs)->generateNpcVariant("human", "villager", 1, 1234, JsonObject{});
  ASSERT_TRUE(villager) << villager.error().what();
  villager->scripts.clear();
  auto npc = (*npcs)->createNpc(*villager);
  ASSERT_TRUE(npc) << npc.error().what();
  ASSERT_TRUE((*npc)->init(**world, 2, EntityMode::Master));
  auto release_npc = make_scope_exit([&] { EXPECT_TRUE((*npc)->uninit()); });
  expect_actor_script(**npc, **npc, 2);

  auto monsters = root.monsterDatabase();
  ASSERT_TRUE(monsters);
  auto poptop = (*monsters)->monsterVariant("poptop", 1234, JsonObject{{"scripts", JsonArray{}}});
  ASSERT_TRUE(poptop) << poptop.error().what();
  auto monster = (*monsters)->createMonster(std::move(*poptop), 1);
  ASSERT_TRUE(monster) << monster.error().what();
  ASSERT_TRUE((*monster)->init(**world, 3, EntityMode::Master));
  auto release_monster = make_scope_exit([&] { EXPECT_TRUE((*monster)->uninit()); });
  expect_actor_script(**monster, **monster, 3);
});

const GameTestRegistration vehicle_seats("InteractionEntities", "VehicleEnabledSeatsAndAnchorOrder", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto file = File::ephemeral_file(root.random());
  ASSERT_TRUE(file);
  auto world = WorldServer::create(root, {128, 128}, *file);
  ASSERT_TRUE(world) << world.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  auto config = Json::parse(R"({
    "name":"interaction-seat-test", "script":"/scripts/util.lua", "boundBox":[-1,-1,1,1],
    "slaveControlTimeout":1, "slaveControlHeartbeat":1, "movementSettings":{},
    "animation":{"animatedParts":{"stateTypes":{},"parts":{"body":{"properties":{"left":[-1,0],"right":[1,0]}}}}}
  })");
  ASSERT_TRUE(config);
  auto seat = [](String anchor, bool enabled) -> Json {
    return JsonObject{{"part", "body"}, {"partAnchor", std::move(anchor)}, {"enabled", enabled}};
  };
  InteractRequest request{37, {12, 31}, 1, {20, 20}};
  auto exercise = [&](JsonObject seats, InteractActionType type, Json data) {
    auto vehicle = Vehicle::create(root, *config, "/", JsonObject{{"loungePositions", std::move(seats)}});
    ASSERT_TRUE(vehicle) << vehicle.error().what();
    (*vehicle)->setPosition({20, 20});
    ASSERT_TRUE((*vehicle)->init(**world, 1, EntityMode::Master));
    auto release = make_scope_exit([&] { EXPECT_TRUE((*vehicle)->uninit()); });
    expect_action(**vehicle, request, type, data);
    ASSERT_TRUE((*vehicle)->evalScript("function onInteraction() return nil end"));
    expect_action(**vehicle, request, type, data);
    ASSERT_TRUE((*vehicle)->evalScript("function onInteraction(r) return {'Message', {r.sourceId,r.sourcePosition,r.interactPosition}} end"));
    expect_action(**vehicle, request, InteractActionType::Message, JsonArray{37, JsonArray{12, 31}, JsonArray{20, 20}});
  };
  exercise({}, InteractActionType::None, {});
  exercise({{"a", seat("missing", false)}}, InteractActionType::None, {});
  exercise({{"a", seat("missing", false)}, {"b", seat("missing", false)}}, InteractActionType::None, {});
  exercise({{"a", seat("missing", false)}, {"b", seat("missing", true)}}, InteractActionType::SitDown, 1);
  exercise({{"a", seat("left", true)}, {"b", seat("right", true)}}, InteractActionType::SitDown, 0);
  request.interact_position = {21, 20};
  auto contested = Vehicle::create(root, *config, "/", JsonObject{{"loungePositions", JsonObject{
      {"a", seat("left", true)}, {"b", seat("right", true)}, {"c", seat("right", true)}}}});
  ASSERT_TRUE(contested) << contested.error().what();
  (*contested)->setPosition({20, 20});
  ASSERT_TRUE((*contested)->init(**world, 1, EntityMode::Master));
  auto release = make_scope_exit([&] { EXPECT_TRUE((*contested)->uninit()); });
  expect_action(**contested, request, InteractActionType::SitDown, 1);

  auto broken = Vehicle::create(root, *config, "/", JsonObject{{"loungePositions", JsonObject{
      {"a", seat("missing-winner", true)}, {"b", seat("missing-candidate", true)}}}});
  ASSERT_TRUE(broken) << broken.error().what();
  request.target_id = 2;
  ASSERT_TRUE((*broken)->init(**world, 2, EntityMode::Master));
  auto release_broken = make_scope_exit([&] { EXPECT_TRUE((*broken)->uninit()); });
  auto error = (*broken)->interact(request);
  ASSERT_FALSE(error);
  EXPECT_NE(std::string_view(error.error().what()).find("missing-candidate"), std::string_view::npos);
  ASSERT_TRUE((*broken)->evalScript("function onInteraction() return 'ShowPopup' end"));
  expect_action(**broken, request, InteractActionType::ShowPopup);
});

}
