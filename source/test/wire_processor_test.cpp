#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.file;
import star.format;
import star.game_test;
import star.game_types;
import star.json;
import star.list;
import star.material_types;
import star.object;
import star.object_database;
import star.rect;
import star.root;
import star.string;
import star.vector;
import star.wiring;
import star.world_server;
import star.world_tiles;

using namespace Star;

namespace {

std::expected<String, StarException> wiredObjectName(ObjectDatabase const& database, WorldServer const& world, Vec2I const& position) {
  // Use a real installed object with both node directions, without replacing its
  // scripts, node configuration, or entity implementation with a test double.
  for (auto const& name : database.allObjects()) {
    auto config = database.getConfig(name);
    if (!config) return std::unexpected(std::move(config.error()));
    auto inputs = (*config)->config.getArray("inputNodes", {});
    if (!inputs) return std::unexpected(std::move(inputs.error()));
    if (inputs->empty())
      continue;
    auto outputs = (*config)->config.getArray("outputNodes", {});
    if (!outputs) return std::unexpected(std::move(outputs.error()));
    if (outputs->empty())
      continue;
    auto placeable = database.canPlaceObject(world, position, name);
    if (!placeable) return std::unexpected(std::move(placeable.error()));
    if (*placeable)
      return name;
  }
  return std::unexpected(StarException("Real assets contain no placeable object with both input and output wire nodes"));
}

void expectConnection(Object const& object, WireDirection direction, WireConnection const& connection, bool present) {
  auto connections = object.connectionsForNode({direction, 0});
  ASSERT_TRUE(connections) << connections.error().what();
  EXPECT_EQ(std::ranges::find(*connections, connection) != connections->end(), present);
}

void exerciseNetwork(GameTest& fixture, bool cycle) {
  auto storage = File::ephemeral_file(fixture.root().random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto created = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(created) << created.error().what();
  auto world = *created;
  auto close = finally([&] { EXPECT_TRUE(world->close()); });
  world->setSpawningEnabled(false);
  ASSERT_TRUE(world->setFidelity(WorldServerFidelity::High));
  ASSERT_TRUE(world->generateRegion(RectI::withSize(Vec2I(992, 992), Vec2I(128, 64))));
  world->setTileProtectionEnabled(false);
  // A real background wall supplies attachment surfaces for the installed
  // wiring object. Both factory validation and Object::init choose orientation.
  for (int y = 992; y < 1056; ++y) {
    for (int x = 992; x < 1120; ++x) {
      auto tile = world->modifyServerTile(Vec2I(x, y), true);
      ASSERT_TRUE(tile) << tile.error().what();
      ASSERT_TRUE(*tile);
      (*tile)->get().background = StructureMaterialId;
    }
  }
  auto database = fixture.root().objectDatabase();
  ASSERT_TRUE(database) << database.error().what();
  auto name = wiredObjectName(**database, *world, Vec2I(1000, 1024));
  ASSERT_TRUE(name) << name.error().what();
  SCOPED_TRACE(name->utf8());
  std::array<std::shared_ptr<Object>, 3> objects;
  for (std::size_t i = 0; i < objects.size(); ++i) {
    auto object = (*database)->createForPlacement(*world, *name,
        Vec2I(1000 + 40 * i, 1024), Direction::Left, JsonObject{{"unbreakable", true}});
    ASSERT_TRUE(object) << object.error().what();
    ASSERT_NE(*object, nullptr);
    objects[i] = *object;
    ASSERT_GT(objects[i]->nodeCount(WireDirection::Input), 0u);
    ASSERT_GT(objects[i]->nodeCount(WireDirection::Output), 0u);
    ASSERT_TRUE(world->addEntity(objects[i]));
  }
  auto connect = [&](std::size_t from, std::size_t to) -> std::expected<void, StarException> {
    auto output = objects[from]->addNodeConnection({WireDirection::Output, 0}, {objects[to]->tilePosition(), 0});
    if (!output) return std::unexpected(std::move(output.error()));
    return objects[to]->addNodeConnection({WireDirection::Input, 0}, {objects[from]->tilePosition(), 0});
  };
  ASSERT_TRUE(connect(0, 1));
  ASSERT_TRUE(connect(1, 2));
  if (cycle)
    ASSERT_TRUE(connect(2, 0));

  // Distinct missing endpoints exercise both recursive direction loops. Their
  // removal is observable proof that update reached loadNetwork, not merely
  // Object::evaluate or an isolated recursive-lambda/helper probe.
  WireConnection missingInput{Vec2I(1100, 1024), 0};
  WireConnection missingOutput{Vec2I(1104, 1024), 0};
  ASSERT_TRUE(objects[1]->addNodeConnection({WireDirection::Input, 0}, missingInput));
  ASSERT_TRUE(objects[2]->addNodeConnection({WireDirection::Output, 0}, missingOutput));
  expectConnection(*objects[1], WireDirection::Input, missingInput, true);
  expectConnection(*objects[2], WireDirection::Output, missingOutput, true);

  bool removed = false;
  for (unsigned step = 0; step < 128; ++step) {
    auto updated = world->update(0.0f);
    ASSERT_TRUE(updated) << updated.error().what();
    auto inputs = objects[1]->connectionsForNode({WireDirection::Input, 0});
    ASSERT_TRUE(inputs) << inputs.error().what();
    auto outputs = objects[2]->connectionsForNode({WireDirection::Output, 0});
    ASSERT_TRUE(outputs) << outputs.error().what();
    removed = std::ranges::find(*inputs, missingInput) == inputs->end()
        && std::ranges::find(*outputs, missingOutput) == outputs->end();
    if (removed)
      break;
  }
  ASSERT_TRUE(removed);
  for (auto const& object : objects)
    EXPECT_EQ(world->entity(object->entityId()).get(), object.get());
  expectConnection(*objects[0], WireDirection::Output, {objects[1]->tilePosition(), 0}, true);
  expectConnection(*objects[1], WireDirection::Input, {objects[0]->tilePosition(), 0}, true);
  expectConnection(*objects[1], WireDirection::Output, {objects[2]->tilePosition(), 0}, true);
  expectConnection(*objects[2], WireDirection::Input, {objects[1]->tilePosition(), 0}, true);
  if (cycle) {
    expectConnection(*objects[2], WireDirection::Output, {objects[0]->tilePosition(), 0}, true);
    expectConnection(*objects[0], WireDirection::Input, {objects[2]->tilePosition(), 0}, true);
  }
}

const GameTestRegistration wireChain("WireProcessor", "RealWorldChainRetainsLinksAndPrunesMissingEndpoints", [](GameTest& fixture) {
  exerciseNetwork(fixture, false);
});

const GameTestRegistration wireCycle("WireProcessor", "RealWorldCycleTerminatesAndPrunesMissingEndpoints", [](GameTest& fixture) {
  exerciseNetwork(fixture, true);
});

}
