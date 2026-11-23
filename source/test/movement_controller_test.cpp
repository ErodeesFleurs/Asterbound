#include "gtest/gtest.h"

import std;
import star.format;
import star.game_test;
import star.root;
import star.file;
import star.algorithm;
import star.movement_controller;
import star.world_server;
import star.world_tiles;
import star.material_database;
import star.material_types;
import star.tile_modification;
import star.collision_block;
import star.game_types;
import star.poly;
import star.rect;
import star.vector;
import star.actor_movement_controller;
import star.data_stream_devices;
import star.byte_array;

using namespace Star;

TEST(ActorMovementWire, LegacyParametersAndNestedJumpProfiles) {
  // Presence bytes and big-endian payloads, independent of the current writer.
  constexpr char legacy[] =
    "\x01\x3f\x80\x00\x00"              // mass = 1
    "\0\0\0\0\0\0\0\0\0\0"             // gravityMultiplier through speedLimit
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // standingPoly through liquidForce
    "\0\0\0\0\0\x01\x00\0\0\0"         // air: multiJump = false
    "\0\0\0\0\0\0\0\0\x01\x01"         // liquid: collisionCancelled = true
    "\0\0\0\0\0"                        // fallStatusSpeedMin through physicsEffectCategories
    "\x01\x00\0\0\x01\x40\x00\x00\x00"; // collisionEnabled = false, pathExploreRate = 2
  ByteArray bytes({legacy, sizeof(legacy) - 1});
  DataStreamBuffer input(bytes);
  ActorMovementParameters p;
  p.ground_movement_minimum_sustain = 3.0f;
  p.ground_movement_maximum_sustain = 4.0f;
  p.ground_movement_check_distance = 5.0f;
  ASSERT_TRUE(input >> p);
  EXPECT_EQ(p.mass, 1.0f);
  EXPECT_EQ(p.air_jump_profile.multi_jump, false);
  EXPECT_EQ(p.liquid_jump_profile.collision_cancelled, true);
  EXPECT_EQ(p.collision_enabled, false);
  EXPECT_EQ(p.path_explore_rate, 2.0f);
  EXPECT_EQ(p.ground_movement_minimum_sustain, 3.0f);
  EXPECT_EQ(p.ground_movement_maximum_sustain, 4.0f);
  EXPECT_EQ(p.ground_movement_check_distance, 5.0f);
  DataStreamBuffer output;
  ASSERT_TRUE(output << p);
  EXPECT_EQ(output.data(), bytes);
}

TEST(ActorMovementWire, ModifierFlagOrderAndTruncatedPrefix) {
  constexpr char legacy[] =
    "\x3f\x80\0\0\x40\0\0\0\x40\x40\0\0\x40\x80\0\0\x40\xa0\0\0\x01\x00\x01\x00";
  ByteArray bytes({legacy, sizeof(legacy) - 1});
  DataStreamBuffer input(bytes);
  ActorMovementModifiers m;
  ASSERT_TRUE(input >> m);
  EXPECT_EQ(m.ground_movement_modifier, 1.0f);
  EXPECT_EQ(m.liquid_movement_modifier, 2.0f);
  EXPECT_EQ(m.speed_modifier, 3.0f);
  EXPECT_EQ(m.air_jump_modifier, 4.0f);
  EXPECT_EQ(m.liquid_jump_modifier, 5.0f);
  EXPECT_TRUE(m.running_suppressed);
  EXPECT_FALSE(m.jumping_suppressed);
  EXPECT_TRUE(m.facing_suppressed);
  EXPECT_FALSE(m.movement_suppressed);
  DataStreamBuffer output;
  ASSERT_TRUE(output << m);
  EXPECT_EQ(output.data(), bytes);

  DataStreamBuffer truncated(ByteArray({legacy, 8}));
  m.liquid_movement_modifier = m.liquid_jump_modifier = -1.0f;
  EXPECT_FALSE(truncated >> m);
  EXPECT_EQ(m.liquid_movement_modifier, 2.0f);
  EXPECT_EQ(m.liquid_jump_modifier, -1.0f);
}

// Exercise the real WorldServer collision query and tickMaster consumer rather
// than a local solver or a fake World. The ephemeral storage cannot touch saves.
const GameTestRegistration movementTerrainContactPreservesPositionAndVelocity(
  "MovementController", "TerrainContactPreservesPositionAndVelocity", [](GameTest& fixture) {
    auto& root = fixture.root();
    auto storage = File::ephemeral_file(root.random());
    ASSERT_TRUE(storage) << storage.error().what();
    auto created = WorldServer::create(root, Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto closeWorld = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
    world->setSpawningEnabled(false);
    world->setTileProtectionEnabled(false);
    Vec2I origin(1024, 1024);
    ASSERT_TRUE(world->generateRegion(RectI::with_size(origin - Vec2I(16, 16), Vec2I(40, 40))));
    ASSERT_TRUE(world->setDungeonGravity(no_dungeon_id, 60.0f));
    // Material placement marks constructed tiles with construction_dungeon_id.
    // gravity() rounds the final position, so the corner can sample the wall
    // rather than the neighboring empty tile. Keep both domain gravities fixed.
    ASSERT_TRUE(world->setDungeonGravity(construction_dungeon_id, 60.0f));
    auto materials = root.materialDatabase();
    ASSERT_TRUE(materials) << materials.error().what();
    auto dirt = (*materials)->materialId("dirt");
    ASSERT_TRUE(dirt) << dirt.error().what();

    auto place = [&](Vec2I position, TileCollisionOverride collision) -> std::expected<void, StarException> {
      auto tile = world->modifyServerTile(position, true);
      if (!tile)
        return std::unexpected(std::move(tile.error()));
      if (!*tile)
        return std::unexpected(StarException("Movement fixture tile is unavailable"));
      (*tile)->get().background = StructureMaterialId;
      auto placed =
        world->forceModifyTile(position, PlaceMaterial{TileLayer::Foreground, *dirt, MaterialHue(0), collision}, true);
      if (!placed)
        return std::unexpected(std::move(placed.error()));
      if (!*placed)
        return std::unexpected(StarException("Movement fixture material placement was rejected"));
      return {};
    };
    for (int x = 0; x <= 8; ++x)
      ASSERT_TRUE(place(origin + Vec2I(x, 0), TileCollisionOverride::Block));
    for (int y = 1; y <= 8; ++y)
      ASSERT_TRUE(place(origin + Vec2I(8, y), TileCollisionOverride::Block));
    ASSERT_TRUE(place(origin + Vec2I(2, 3), TileCollisionOverride::Platform));

    MovementParameters parameters;
    parameters.collisionPoly = PolyF(RectF(-0.25f, -0.25f, 0.25f, 0.25f));
    parameters.maxMovementPerStep = 0.0f;
    parameters.maximumCorrection = 8.0f;
    parameters.maximumPlatformCorrection = 1.0f;
    parameters.maximumPlatformCorrectionVelocityFactor = 0.0f;
    parameters.gravityEnabled = true;
    parameters.gravityMultiplier = 1.0f;
    parameters.airBuoyancy = 0.0f;
    parameters.liquidBuoyancy = 0.0f;
    parameters.frictionEnabled = false;
    parameters.stickyCollision = false;
    parameters.bounceFactor = 0.0f;
    parameters.slopeSlidingFactor = 0.0f;
    parameters.ignorePlatformCollision = false;
    parameters.speedLimit = 1000.0f;

    struct Contact {
      Vec2F position;
      Vec2F velocity;
      Vec2F expectedPosition;
      Vec2F expectedVelocity;
      bool ignorePlatforms;
      bool onGround;
    };
    for (auto const& contact : std::array{Contact{{4, 2}, {0, -60}, {4, 1.25f}, {0, -46}, false, true},
                                          Contact{{7.5f, 2}, {37.5f, -60}, {7.75f, 1.25f}, {15, -46}, false, true},
                                          Contact{{2.5f, 5}, {0, -60}, {2.5f, 4.25f}, {0, -46}, false, true},
                                          Contact{{2.5f, 5}, {0, -60}, {2.5f, 4}, {0, -61}, true, false}}) {
      parameters.ignorePlatformCollision = contact.ignorePlatforms;
      auto made = MovementController::create(root, parameters);
      ASSERT_TRUE(made) << made.error().what();
      auto controller = *made;
      controller->init(*world);
      auto uninit = make_scope_exit([&] { controller->uninit(); });
      controller->setPosition(Vec2F(origin) + contact.position);
      controller->setVelocity(contact.velocity);
      ASSERT_TRUE(controller->tickMaster(1.0f / 60.0f));
      auto position = controller->position() - Vec2F(origin);
      EXPECT_NEAR(position[0], contact.expectedPosition[0], 0.0002f);
      EXPECT_NEAR(position[1], contact.expectedPosition[1], 0.0002f);
      EXPECT_NEAR(controller->xVelocity(), contact.expectedVelocity[0], 0.02f);
      EXPECT_NEAR(controller->yVelocity(), contact.expectedVelocity[1], 0.02f);
      EXPECT_EQ(controller->onGround(), contact.onGround);
      EXPECT_FALSE(controller->isCollisionStuck());
      EXPECT_FALSE(controller->isNullColliding());
    }

    // Real high-speed movement must keep its substeps; a benchmark win must not
    // come from skipping collision work and tunnelling through the same floor.
    parameters.ignorePlatformCollision = false;
    parameters.maxMovementPerStep = 0.125f;
    auto made = MovementController::create(root, parameters);
    ASSERT_TRUE(made) << made.error().what();
    auto controller = *made;
    controller->init(*world);
    auto uninit = make_scope_exit([&] { controller->uninit(); });
    controller->setPosition(Vec2F(origin) + Vec2F(4, 6));
    controller->setVelocity(Vec2F(0, -600));
    ASSERT_TRUE(controller->tickMaster(1.0f / 60.0f));
    EXPECT_GE(controller->yPosition(), origin[1] + 1.25f - 0.0002f);
    EXPECT_LE(controller->yPosition(), origin[1] + 1.25f + 0.0002f);
    EXPECT_TRUE(controller->onGround());
    EXPECT_FALSE(controller->isCollisionStuck());
    EXPECT_FALSE(controller->isNullColliding());
  });
