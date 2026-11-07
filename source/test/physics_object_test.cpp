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
    auto closeWorld = finally([&] { EXPECT_TRUE(world->close()); });
    EntityMap entities(worldSize, 1, 100);

    // A minimal configuration is input to the real PhysicsObject implementation,
    // not a substitute Entity or World. An anchor-free orientation gives the
    // non-force bounds a known extent without requiring terrain.
    auto config = std::make_shared<ObjectConfig>();
    config->name = "physics-bounds-test";
    config->path = "/";
    config->config = JsonObject();
    config->health = 1;
    auto orientation = std::make_shared<ObjectOrientation>();
    orientation->config = JsonObject();
    orientation->boundBox = RectI(0, 0, 0, 0);
    orientation->metaBoundBox = RectF(-1, -1, 1, 1);
    config->orientations.append(orientation);

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
      auto forces = forceConfig->toObject();
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

      // Reinitialize the same object at two translations, including coordinates
      // that made the old local/world union exceed MaximumEntityBoundBox.
      for (auto const& tilePosition : {Vec2I(512, 256), farPosition}) {
        SCOPED_TRACE(strf("position {}", tilePosition));
        ASSERT_TRUE(object->setTilePosition(tilePosition));
        auto id = entities.reserveEntityId();
        ASSERT_TRUE(id) << id.error().what();
        auto initialized = object->init(*world, *id, EntityMode::Master);
        ASSERT_TRUE(initialized) << initialized.error().what();
        auto uninitObject = finally([&] {
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

        auto added = entities.addEntity(object);
        ASSERT_TRUE(added) << added.error().what();
        auto expectQuery = [&](Vec2F localPoint, bool present) {
          auto found = entities.entityQuery(RectF::withSize(position + localPoint, Vec2F(0.25f, 0.25f)));
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
        ASSERT_TRUE(entities.updateAllEntities());
        expectQuery(Vec2F(-11, -3), true);
        expectQuery(Vec2F(32, 32), false);
      }
    }
  });

}// namespace
