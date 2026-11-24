#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.entity;
import star.entity_map;
import star.file;
import star.force_regions;
import star.format;
import star.game_test;
import star.game_types;
import star.json;
import star.line;
import star.list;
import star.movement_controller;
import star.collision_block;
import star.physics_entity;
import star.object_database;
import star.physics_object;
import star.poly;
import star.rect;
import star.root;
import star.string;
import star.vector;
import star.world_server;

using namespace Star;

namespace {

std::shared_ptr<ObjectConfig> unanchored_physics_config() {
  auto config = std::make_shared<ObjectConfig>();
  config->name = "physics-bounds-test";
  config->path = "/";
  config->config = JsonObject();
  config->health = 1;
  auto orientation = std::make_shared<ObjectOrientation>();
  orientation->config = JsonObject();
  orientation->boundBox = RectI(0, 0, 0, 0);
  orientation->metaBoundBox = RectF(-1, -1, 1, 1);
  config->orientations.push_back(std::move(orientation));
  return config;
}

const GameTestRegistration legacy_orientation_order("ObjectDatabase", "LegacyOrientationExpansion", [](GameTest& fixture) {
  auto parse = [&](Json const& entries) {
    return ObjectDatabase::parseOrientations(fixture.root(), "/test.object", entries, JsonObject());
  };
  auto entries = Json::parse(R"([
    {"imageLayers":[]},
    {"imageLayers":[],"dualImage":null,"leftImage":"ignored"},
    {"imageLayers":[],"leftImage":"left"},
    {"imageLayers":[],"leftImage":"a","rightImage":"b"},
    {"imageLayers":[],"spaceScan":0,"imageKeys":{"color":"custom"}}
  ])");
  ASSERT_TRUE(entries);
  auto orientations = parse(*entries);
  ASSERT_TRUE(orientations) << orientations.error().what();
  JsonArray actual;
  for (auto const& orientation : *orientations)
    actual.emplace_back(orientation->config);
  auto expected = Json::parse(R"([
    {"imageLayers":[]},
    {"imageLayers":[],"dualImage":null,"leftImage":"ignored","image":null,"flipImages":true,"direction":"left"},
    {"imageLayers":[],"dualImage":null,"leftImage":"ignored","image":null,"direction":"right"},
    {"imageLayers":[],"leftImage":"left","image":"left","direction":"left"},
    {"imageLayers":[],"leftImage":"left","image":null,"direction":"right"},
    {"imageLayers":[],"leftImage":"a","rightImage":"b","image":"a","direction":"left"},
    {"imageLayers":[],"leftImage":"a","rightImage":"b","image":"b","direction":"right"},
    {"imageLayers":[],"spaceScan":0,"imageKeys":{"color":"custom"}}
  ])");
  ASSERT_TRUE(expected);
  EXPECT_EQ(Json(std::move(actual)), *expected);

  // The later non-object must fail before the earlier missing image is parsed.
  auto malformed = parse(JsonArray{JsonObject(), Json()});
  ASSERT_FALSE(malformed);
  EXPECT_NE(std::string_view(malformed.error().what()).find("Improper conversion to JsonObject"), std::string_view::npos);
});

const GameTestRegistration
  localForceBounds("PhysicsObject", "LocalBoundsAndWorldForcesAtLargeCoordinates", [](GameTest& fixture) {
    // The empty world is sparse: no sectors or terrain need to be generated to
    // initialize an anchor-free object and index its physics bounds.
    Vec2U worldSize(32768, 32768);
    auto storage = File::ephemeral_file(fixture.root().random());
    ASSERT_TRUE(storage) << storage.error().what();
    auto created = WorldServer::create(fixture.root(), worldSize, *storage);
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto closeWorld = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
    EntityMap entities(worldSize, 1, 100);

    // A minimal configuration is input to the real PhysicsObject implementation,
    // not a substitute Entity or World. An anchor-free orientation gives the
    // non-force bounds a known extent without requiring terrain.
    auto config = unanchored_physics_config();

    auto forceConfig = Json::parse(R"({
    "directional": {
      "type":"DirectionalForceRegion", "rectRegion":[-12,-4,-8,2],
      "xTargetVelocity":3, "controlForce":10
    },
    "gradient": {
      "type":"GradientForceRegion", "polyRegion":[[7,-9],[13,-9],[10,-5]],
      "gradient":[[7,-9],[13,-5]], "baseTargetVelocity":4, "baseControlForce":20
    },
    "radial": {
      "type":"RadialForceRegion", "center":[3,4], "outerRadius":4,
      "innerRadius":1, "targetRadialVelocity":5, "controlForce":30
    }
  })");
    ASSERT_TRUE(forceConfig) << forceConfig.error().what();
    RectF expectedBounds(-12, -9, 13, 6);
    Vec2I farPosition(EntityMap::MaximumEntityBoundBox + 1024, EntityMap::MaximumEntityBoundBox + 2048);
    ASSERT_LT(farPosition[0] + 13, static_cast<int>(worldSize[0]));
    ASSERT_LT(farPosition[1] + 6, static_cast<int>(worldSize[1]));

    for (bool enabled : {false, true}) {
      SCOPED_TRACE(enabled ? "enabled" : "disabled");
      auto forces = forceConfig->to_object();
      ASSERT_TRUE(forces) << forces.error().what();
      for (auto&& pair : *forces) {
        auto changed = pair.second.set("enabled", enabled);
        ASSERT_TRUE(changed) << changed.error().what();
        pair.second = std::move(*changed);
      }
      auto objectResult =
        PhysicsObject::create(fixture.root(), config, JsonObject{{"physicsForces", std::move(*forces)}});
      ASSERT_TRUE(objectResult) << objectResult.error().what();
      auto object = *objectResult;
      std::optional<RectF> firstBounds;

      // Exercise ordinary, large-coordinate and horizontally wrapped bounds.
      int const world_width = static_cast<int>(worldSize[0]);
      for (auto const& tilePosition : {Vec2I(512, 256), farPosition, Vec2I(0, 256),
                                       Vec2I(world_width - 13, 256), Vec2I(12, 256)}) {
        SCOPED_TRACE(Star::format("position {}", tilePosition));
        ASSERT_TRUE(object->setTilePosition(tilePosition));
        auto id = entities.reserveEntityId();
        ASSERT_TRUE(id) << id.error().what();
        auto initialized = object->init(*world, *id, EntityMode::Master);
        ASSERT_TRUE(initialized) << initialized.error().what();
        auto uninitObject = make_scope_exit([&] {
          entities.removeEntity(*id);
          EXPECT_TRUE(object->uninit());
        });
        Vec2F position(tilePosition);
        EXPECT_EQ(object->position(), position);
        EXPECT_EQ(object->metaBoundBox(), expectedBounds);
        if (firstBounds)
          EXPECT_EQ(object->metaBoundBox(), *firstBounds);
        else
          firstBounds = object->metaBoundBox();
        EXPECT_LT(object->metaBoundBox().width(), EntityMap::MaximumEntityBoundBox);
        EXPECT_LT(object->metaBoundBox().height(), EntityMap::MaximumEntityBoundBox);

        auto output = object->forceRegions();
        ASSERT_TRUE(output) << output.error().what();
        if (enabled) {
          ASSERT_EQ(output->size(), 3u);
          ASSERT_TRUE(std::holds_alternative<DirectionalForceRegion>((*output)[0]));
          ASSERT_TRUE(std::holds_alternative<GradientForceRegion>((*output)[1]));
          ASSERT_TRUE(std::holds_alternative<RadialForceRegion>((*output)[2]));
          auto const& directional = std::get<DirectionalForceRegion>((*output)[0]);
          auto const& gradient = std::get<GradientForceRegion>((*output)[1]);
          auto const& radial = std::get<RadialForceRegion>((*output)[2]);
          EXPECT_EQ(directional.boundBox(), RectF(-12, -4, -8, 2).translated(position));
          EXPECT_EQ(gradient.boundBox(), RectF(7, -9, 13, -5).translated(position));
          EXPECT_EQ(gradient.gradient.min(), position + Vec2F(7, -9));
          EXPECT_EQ(gradient.gradient.max(), position + Vec2F(13, -5));
          EXPECT_EQ(radial.center, position + Vec2F(3, 4));
          EXPECT_EQ(radial.boundBox(), RectF(1, 2, 5, 6).translated(position));
        } else {
          EXPECT_TRUE(output->empty());
        }

        object->setUniqueId(String("indexed-object"));
        auto added = entities.addEntity(object);
        ASSERT_TRUE(added) << added.error().what();
        auto expectQuery = [&](Vec2F localPoint, bool present) {
          auto found = entities.entityQuery(RectF::with_size(position + localPoint, Vec2F(0.25f, 0.25f)));
          ASSERT_TRUE(found) << found.error().what();
          EXPECT_EQ(found->size(), present ? 1u : 0u);
          if (present && !found->empty())
            EXPECT_EQ((*found)[0].get(), object.get());
        };
        // Force-only areas must be indexed even while forces are disabled. The
        // old translated meta box missed the negative-side area and produced a
        // false hit far outside the local bounds at the first position.
        expectQuery(Vec2F(-11, -3), true);
        expectQuery(Vec2F(10, -8), true);
        expectQuery(Vec2F(3, 4), true);
        expectQuery(Vec2F(32, 32), false);
        expectQuery(-position, false);
        if (tilePosition[0] == 0) {
          float const width = static_cast<float>(worldSize[0]);
          float const y = position[1];
          for (auto const& query : {RectF(width - 11, y, width - 10, y + 1),
                                    RectF(9, y, 10, y + 1), RectF(-11, y, 10, y + 1)}) {
            auto hits = entities.entityQuery(query);
            ASSERT_TRUE(hits) << hits.error().what();
            ASSERT_EQ(hits->size(), 1u);
            EXPECT_EQ((*hits)[0], object);
          }
          int filter_calls = 0;
          auto rejected = entities.entityQuery(RectF(-11, y, 10, y + 1),
              [&](std::shared_ptr<Entity> const&) -> std::expected<bool, StarException> {
                ++filter_calls;
                return std::unexpected(EntityMapException("seam filter"));
              });
          ASSERT_FALSE(rejected);
          EXPECT_TRUE(rejected.error().is<EntityMapException>());
          EXPECT_EQ(filter_calls, 1);
          auto other = PhysicsObject::create(fixture.root(), config, JsonObject{{"physicsForces", JsonObject()}});
          ASSERT_TRUE(other) << other.error().what();
          ASSERT_TRUE((*other)->setTilePosition(Vec2I(128, 256)));
          auto other_id = entities.reserveEntityId();
          ASSERT_TRUE(other_id);
          ASSERT_TRUE((*other)->init(*world, *other_id, EntityMode::Master));
          auto release_other = make_scope_exit([&] {
            entities.removeEntity(*other_id);
            EXPECT_TRUE((*other)->uninit());
          });
          (*other)->setUniqueId(String("second-object"));
          ASSERT_TRUE(entities.addEntity(*other));
          ASSERT_TRUE((*other)->setTilePosition(Vec2I(512, 256)));
          (*other)->setUniqueId(String("indexed-object"));
          auto conflict = entities.updateAllEntities();
          ASSERT_FALSE(conflict);
          EXPECT_TRUE(conflict.error().is<EntityMapException>());
          EXPECT_EQ(entities.uniqueEntityId("indexed-object"), *id);
          EXPECT_EQ(entities.uniqueEntityId("second-object"), *other_id);
          auto old_hits = entities.entityQuery(RectF(128, 256, 128, 256));
          ASSERT_TRUE(old_hits);
          ASSERT_EQ(old_hits->size(), 1u);
          EXPECT_EQ((*old_hits)[0], *other);
          auto new_hits = entities.entityQuery(RectF(512, 256, 512, 256));
          ASSERT_TRUE(new_hits);
          EXPECT_TRUE(new_hits->empty());
          (*other)->setUniqueId(String("second-object"));
          ASSERT_TRUE(entities.updateAllEntities());
          new_hits = entities.entityQuery(RectF(512, 256, 512, 256));
          ASSERT_TRUE(new_hits);
          ASSERT_EQ(new_hits->size(), 1u);
          EXPECT_EQ((*new_hits)[0], *other);
        }
        if (tilePosition[0] == 12 || tilePosition[0] == world_width - 13) {
          // Exact periodic contact, not a box straddling the seam.
          bool const at_left = tilePosition[0] == 12;
          float const seam = at_left ? static_cast<float>(world_width) : 0.f;
          Vec2F const edge(seam, position[1]);
          Vec2F const outside(seam + (at_left ? -1.f : 1.f), position[1]);
          auto point_hits = entities.entityQuery(RectF(edge, edge));
          ASSERT_TRUE(point_hits) << point_hits.error().what();
          ASSERT_EQ(point_hits->size(), 1u);
          EXPECT_EQ((*point_hits)[0], object);
          auto line_hits = entities.entityLineQuery(outside, edge);
          ASSERT_TRUE(line_hits) << line_hits.error().what();
          ASSERT_EQ(line_hits->size(), 1u);
          EXPECT_EQ((*line_hits)[0], object);
          auto diagonal_miss = entities.entityLineQuery(outside, edge + Vec2F(0, 20));
          ASSERT_TRUE(diagonal_miss) << diagonal_miss.error().what();
          EXPECT_TRUE(diagonal_miss->empty());
        }
        ASSERT_TRUE(entities.updateAllEntities());
        expectQuery(Vec2F(-11, -3), true);
        expectQuery(Vec2F(32, 32), false);
      }
    }
  });

const GameTestRegistration moving_collision_filter("MovementController", "MovingPlatformAcrossSeam", [](GameTest& fixture) {
  auto storage = File::ephemeral_file(fixture.root().random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto created = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(created) << created.error().what();
  auto world = *created;
  auto close_world = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
  // Indices follow key order: reject 0 by category, accept 1, stop before 2.
  auto shapes = Json::parse(R"({
    "a":{"collision":[[-1,0],[1,0],[1,1],[-1,1]],"categoryWhitelist":["vehicle"]},
    "b":{"collision":[[-1,0],[1,0],[1,1],[-1,1]],"collisionKind":"platform","categoryWhitelist":["actor"]},
    "c":{"collision":[[-1,0],[1,0],[1,1],[-1,1]]}
  })");
  ASSERT_TRUE(shapes) << shapes.error().what();
  auto object = PhysicsObject::create(fixture.root(), unanchored_physics_config(), JsonObject{{"physicsCollisions", *shapes}});
  ASSERT_TRUE(object) << object.error().what();
  ASSERT_TRUE((*object)->setTilePosition({0, 256}));
  auto added = world->addEntity(*object);
  ASSERT_TRUE(added) << added.error().what();
  auto remove = make_scope_exit([&] { EXPECT_TRUE(world->removeEntity((*object)->entityId(), false)); });
  MovementParameters parameters;
  parameters.physicsEffectCategories = StringSet{"actor"};
  auto controller = MovementController::create(fixture.root(), parameters);
  ASSERT_TRUE(controller) << controller.error().what();
  (*controller)->init(*world);
  auto release = make_scope_exit([&] { (*controller)->uninit(); });
  std::size_t visits = 0;
  PolyF retained;
  auto scan = (*controller)->for_each_moving_collision(RectF(2047.25f, 256, 2047.75f, 257),
      [&](MovingCollisionId id, CollisionKind kind, PolyF&& poly, RectF const& bounds) -> std::expected<bool, StarException> {
        ++visits;
        EXPECT_EQ(id.physicsEntityId, (*object)->entityId());
        EXPECT_EQ(id.collisionIndex, 1u);
        EXPECT_EQ(kind, CollisionKind::Platform);
        EXPECT_EQ(bounds, RectF(2047, 256, 2049, 257));
        retained = std::move(poly);
        return false;
      });
  ASSERT_TRUE(scan) << scan.error().what();
  EXPECT_EQ(visits, 1u);
  EXPECT_EQ(retained, (PolyF{{2047,256},{2049,256},{2049,257},{2047,257}}));
});

}// namespace
