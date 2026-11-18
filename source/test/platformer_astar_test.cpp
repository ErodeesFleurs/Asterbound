#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.actor_movement_controller;
import star.collision_block;
import star.file;
import star.format;
import star.game_test;
import star.game_types;
import star.lua;
import star.lua_converters;
import star.lua_game_converters;
import star.lua_root;
import star.material_database;
import star.material_types;
import star.platformer_astar;
import star.platformer_astar_types;
import star.poly;
import star.rect;
import star.root;
import star.tile_modification;
import star.vector;
import star.world_lua_bindings;
import star.world_server;
import star.world_tiles;

using namespace Star;

namespace {

template <typename Callback>
concept platform_world_callback = std::invocable<Callback&, WorldServer&, Vec2F, ActorMovementParameters&>
  && std::same_as<std::invoke_result_t<Callback&, WorldServer&, Vec2F, ActorMovementParameters&>, void>;

template <platform_world_callback Callback> void with_platform_world(GameTest& fixture, Callback callback) {
  auto& root = fixture.root();
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto created = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(created) << created.error().what();
  auto world = *created;
  auto close_world = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
  world->setSpawningEnabled(false);
  world->setTileProtectionEnabled(false);
  Vec2I origin(1024, 1024);
  ASSERT_TRUE(world->generateRegion(RectI::withSize(origin - Vec2I(20, 16), Vec2I(40, 40))));
  ASSERT_TRUE(world->setDungeonGravity(NoDungeonId, 60.0f));
  ASSERT_TRUE(world->setDungeonGravity(ConstructionDungeonId, 60.0f));
  auto materials = root.materialDatabase();
  ASSERT_TRUE(materials) << materials.error().what();
  auto dirt = (*materials)->materialId("dirt");
  ASSERT_TRUE(dirt) << dirt.error().what();
  auto place = [&](Vec2I position) -> std::expected<void, StarException> {
    auto tile = world->modifyServerTile(position, true);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    if (!*tile)
      return std::unexpected(StarException("Platform fixture tile is unavailable"));
    (*tile)->get().background = StructureMaterialId;
    auto placed = world->forceModifyTile(
      position, PlaceMaterial{TileLayer::Foreground, *dirt, MaterialHue(0), TileCollisionOverride::Block}, true);
    if (!placed)
      return std::unexpected(std::move(placed.error()));
    if (!*placed)
      return std::unexpected(StarException("Platform fixture material placement was rejected"));
    return {};
  };
  for (int x = -12; x <= 12; ++x)
    ASSERT_TRUE(place(origin + Vec2I(x, 0)));
  for (int y = 1; y <= 5; ++y)
    ASSERT_TRUE(place(origin + Vec2I(2, y)));

  auto defaults = ActorMovementParameters::sensibleDefaults(root);
  ASSERT_TRUE(defaults) << defaults.error().what();
  auto parameters = std::move(*defaults);
  parameters.standingPoly = PolyF(RectF(-0.25f, -0.25f, 0.25f, 0.25f));
  parameters.crouchingPoly = parameters.standingPoly;
  parameters.mass = 1.0f;
  parameters.airForce = 128.0f;
  parameters.flySpeed = 8.0f;
  parameters.runSpeed = 8.0f;
  parameters.walkSpeed = 4.0f;
  parameters.gravityMultiplier = 1.0f;
  parameters.airBuoyancy = 0.0f;
  parameters.liquidBuoyancy = 0.0f;
  parameters.pathExploreRate = 1000.0f;
  callback(*world, Vec2F(origin), parameters);
}

std::expected<PlatformerAStar::Path, StarException> search_path(WorldServer& world, Vec2F start, Vec2F target,
                                                                ActorMovementParameters parameters,
                                                                PlatformerAStar::Parameters search_parameters,
                                                                std::optional<unsigned> budget) {
  auto created =
    PlatformerAStar::PathFinder::create(world, start, target, std::move(parameters), std::move(search_parameters));
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto finder = std::move(*created);
  for (unsigned iteration = 0; iteration < 20000; ++iteration) {
    auto explored = finder->explore(budget);
    if (!explored)
      return std::unexpected(std::move(explored.error()));
    if (!*explored)
      continue;
    if (!**explored || !finder->result())
      return std::unexpected(StarException("Platform fixture search failed to reach its target"));
    auto repeated = finder->explore(budget);
    if (!repeated)
      return std::unexpected(std::move(repeated.error()));
    if (*repeated != std::optional<bool>(true))
      return std::unexpected(StarException("Platform search changed terminal outcome"));
    auto path = finder->extract_result();
    if (finder->result())
      return std::unexpected(StarException("Platform extraction retained its result"));
    repeated = finder->explore(budget);
    if (!repeated)
      return std::unexpected(std::move(repeated.error()));
    if (*repeated != std::optional<bool>(true))
      return std::unexpected(StarException("Platform extraction changed terminal outcome"));
    return std::move(*path);
  }
  return std::unexpected(StarException("Platform fixture exceeded its deterministic search budget"));
}

void expect_same_path(PlatformerAStar::Path const& actual, PlatformerAStar::Path const& expected) {
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i) {
    EXPECT_EQ(actual[i].action, expected[i].action);
    EXPECT_EQ(actual[i].source, expected[i].source);
    EXPECT_EQ(actual[i].target, expected[i].target);
    EXPECT_EQ(actual[i].jump_velocity, expected[i].jump_velocity);
    EXPECT_EQ(actual[i].cost, expected[i].cost);
  }
}

std::expected<void, StarException> place_ledge(Root& root, WorldServer& world, Vec2F origin,
                                               TileCollisionOverride collision) {
  auto materials = root.materialDatabase();
  if (!materials)
    return std::unexpected(std::move(materials.error()));
  auto dirt = (*materials)->materialId("dirt");
  if (!dirt)
    return std::unexpected(std::move(dirt.error()));
  for (int x = -8; x <= -6; ++x) {
    Vec2I position = Vec2I(origin) + Vec2I(x, 5);
    auto tile = world.modifyServerTile(position, true);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    if (!*tile)
      return std::unexpected(StarException("Platform fixture ledge tile is unavailable"));
    (*tile)->get().background = StructureMaterialId;
    auto placed =
      world.forceModifyTile(position, PlaceMaterial{TileLayer::Foreground, *dirt, MaterialHue(0), collision}, true);
    if (!placed)
      return std::unexpected(std::move(placed.error()));
    if (!*placed)
      return std::unexpected(StarException("Platform fixture ledge placement was rejected"));
  }
  return {};
}

PlatformerAStar::Parameters explicit_bounds() {
  PlatformerAStar::Parameters parameters;
  parameters.bound_box = RectF(-0.4f, -0.25f, 0.4f, 0.25f);
  parameters.standing_bound_box = RectF(-0.125f, -0.25f, 0.125f, 0.25f);
  parameters.dropping_bound_box = RectF(-0.75f, -0.25f, 0.75f, 0.25f);
  parameters.must_end_on_ground = true;
  parameters.max_distance = 16.0f;
  parameters.max_nodes_to_search = 1000;
  return parameters;
}

void expect_simulated_landing(WorldServer& world, PlatformerAStar::Path const& path, Vec2F start, Vec2F target,
                              RectF const& full_bounds) {
  ASSERT_FALSE(path.empty());
  EXPECT_EQ(path[0].source.position, start);
  EXPECT_EQ(path.back().target.position, target);
  EXPECT_FALSE(path.back().target.velocity);
  bool has_arc = false;
  bool has_land = false;
  for (std::size_t i = 0; i < path.size(); ++i) {
    auto const& edge = path[i];
    if (i > 0)
      EXPECT_EQ(edge.source, path[i - 1].target);
    auto collided = world.polyCollision(PolyF(full_bounds.translated(edge.target.position)));
    ASSERT_TRUE(collided) << collided.error().what();
    EXPECT_FALSE(*collided);
    if (edge.action == PlatformerAStar::Action::Arc) {
      has_arc = true;
      ASSERT_TRUE(edge.target.velocity);
      EXPECT_LT((*edge.target.velocity)[1], 0.0f);
      EXPECT_GT(edge.cost, 0.0f);
      EXPECT_NEAR(edge.cost, (edge.target.position - edge.source.position).magnitude(), 0.0001f);
    } else if (edge.action == PlatformerAStar::Action::Land) {
      has_land = true;
      EXPECT_FLOAT_EQ(edge.cost, 0.0f);
      EXPECT_EQ(edge.source.position, edge.target.position);
      ASSERT_TRUE(edge.source.velocity);
      EXPECT_LT((*edge.source.velocity)[1], -5.0f);
      EXPECT_FALSE(edge.target.velocity);
    } else {
      EXPECT_TRUE(edge.action == PlatformerAStar::Action::Walk || edge.action == PlatformerAStar::Action::Drop);
    }
  }
  EXPECT_TRUE(has_arc);
  EXPECT_TRUE(has_land);
}

const GameTestRegistration platform_simulated_landings(
  "PlatformerAStar", "FallingAndPlatformDropProduceArcLandTransitions", [](GameTest& fixture) {
    with_platform_world(fixture, [&](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
      ASSERT_TRUE(place_ledge(fixture.root(), world, origin, TileCollisionOverride::Platform));
      parameters.gravityEnabled = true;
      parameters.airJumpProfile.jumpSpeed.reset();
      parameters.walkSpeed.reset();
      parameters.runSpeed.reset();
      auto search_parameters = explicit_bounds();
      search_parameters.drop_cost = 0.25f;
      // The free fall and platform drop enter different synchronous callback
      // chains; both must eventually preserve the zero-cost Land transition.
      for (bool drop : {false, true}) {
        SCOPED_TRACE(drop ? "platform drop" : "free fall");
        Vec2F start = origin + (drop ? Vec2F(-7, 6.25f) : Vec2F(-3, 8.25f));
        Vec2F target = origin + (drop ? Vec2F(-7, 1.25f) : Vec2F(-3, 1.25f));
        auto path = search_path(world, start, target, parameters, search_parameters, {});
        ASSERT_TRUE(path) << path.error().what();
        expect_simulated_landing(world, *path, start, target, *search_parameters.bound_box);
        ASSERT_FALSE(path->empty());
        EXPECT_EQ(path->back().action, PlatformerAStar::Action::Land);
        EXPECT_EQ((*path)[0].action, drop ? PlatformerAStar::Action::Drop : PlatformerAStar::Action::Arc);
        if (drop) {
          EXPECT_FLOAT_EQ((*path)[0].cost, 0.25f);
          EXPECT_EQ((*path)[0].target.position, start - Vec2F(0, 1));
          ASSERT_TRUE((*path)[0].target.velocity);
          EXPECT_LT((*(*path)[0].target.velocity)[1], -5.0f);
        }
        for (auto const& edge : *path) {
          EXPECT_FLOAT_EQ(edge.target.position[0], start[0]);
          EXPECT_LE(edge.target.position[1], edge.source.position[1]);
        }
        auto budgeted = search_path(world, start, target, parameters, search_parameters, 2);
        ASSERT_TRUE(budgeted) << budgeted.error().what();
        expect_same_path(*budgeted, *path);
      }
    });
  });

const GameTestRegistration
  platform_standing_bounds("PlatformerAStar", "WalkOffLedgeUsesStandingPolyWithBoundsOverrides", [](GameTest& fixture) {
    with_platform_world(fixture, [&](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
      ASSERT_TRUE(place_ledge(fixture.root(), world, origin, TileCollisionOverride::Block));
      parameters.gravityEnabled = true;
      parameters.airJumpProfile.jumpSpeed.reset();
      parameters.walkSpeed = 4.0f;
      parameters.runSpeed.reset();
      auto search_parameters = explicit_bounds();
      Vec2F start = origin + Vec2F(-6, 6.25f);
      Vec2F target = origin + Vec2F(-2, 1.25f);
      auto path = search_path(world, start, target, parameters, search_parameters, {});
      ASSERT_TRUE(path) << path.error().what();
      expect_simulated_landing(world, *path, start, target, *search_parameters.bound_box);
      bool walked_off = false;
      for (auto const& edge : *path) {
        if (edge.action != PlatformerAStar::Action::Walk || !edge.target.velocity)
          continue;
        walked_off = true;
        EXPECT_EQ(edge.source.position, origin + Vec2F(-5, 6.25f));
        // This quarter-tile offset comes from standingPoly, not any of the
        // three deliberately distinct overridden query-box widths.
        EXPECT_EQ(edge.target.position, edge.source.position + Vec2F(0.25f, 0));
        EXPECT_EQ(*edge.target.velocity, Vec2F(4, 0));
      }
      EXPECT_TRUE(walked_off);
      auto budgeted = search_path(world, start, target, parameters, search_parameters, 1);
      ASSERT_TRUE(budgeted) << budgeted.error().what();
      expect_same_path(*budgeted, *path);
    });
  });

const GameTestRegistration platform_missing_standing_poly(
  "PlatformerAStar", "BoundsOverridesPreserveMissingStandingPolyErrorTiming", [](GameTest& fixture) {
    with_platform_world(fixture, [&](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
      ASSERT_TRUE(place_ledge(fixture.root(), world, origin, TileCollisionOverride::Block));
      parameters.gravityEnabled = true;
      parameters.airJumpProfile.jumpSpeed.reset();
      parameters.standingPoly.reset();
      auto search_parameters = explicit_bounds();
      // Stand/Drop overrides alone cannot supply the Full box used to round
      // the endpoints: this error must occur during construction.
      search_parameters.bound_box.reset();
      auto missing_full = PlatformerAStar::PathFinder::create(world, origin + Vec2F(-10, 1.25f),
                                                              origin + Vec2F(-8, 1.25f), parameters, search_parameters);
      ASSERT_FALSE(missing_full);
      EXPECT_NE(std::string_view(missing_full.error().what()).find("requires standingPoly or boundBox"),
                std::string_view::npos);
      search_parameters = explicit_bounds();
      // A Full override is sufficient on continuous ground, without eagerly
      // requiring the standing polygon needed only by walking off a ledge.
      auto flat_path =
        search_path(world, origin + Vec2F(-10, 1.25f), origin + Vec2F(-8, 1.25f), parameters, search_parameters, {});
      ASSERT_TRUE(flat_path) << flat_path.error().what();
      ASSERT_EQ(flat_path->size(), 2u);
      for (auto const& edge : *flat_path)
        EXPECT_EQ(edge.action, PlatformerAStar::Action::Walk);
      auto finder = PlatformerAStar::PathFinder::create(world, origin + Vec2F(-5, 6.25f), origin + Vec2F(-2, 1.25f),
                                                        parameters, search_parameters);
      ASSERT_TRUE(finder) << finder.error().what();
      auto untouched = (*finder)->explore(0);
      ASSERT_TRUE(untouched) << untouched.error().what();
      EXPECT_FALSE(untouched->has_value());
      auto walked_off = (*finder)->explore(1);
      ASSERT_FALSE(walked_off);
      EXPECT_NE(std::string_view(walked_off.error().what()).find("requires standingPoly for walking"),
                std::string_view::npos);
    });
  });

const GameTestRegistration
  platform_search_budgets("PlatformerAStar", "BudgetsPreserveTerrainAndFlightPaths", [](GameTest& fixture) {
    with_platform_world(fixture, [](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
      for (bool flying : {false, true}) {
        SCOPED_TRACE(flying ? "flight over real wall" : "walk on real floor");
        parameters.gravityEnabled = !flying;
        Vec2F start = origin + (flying ? Vec2F(-6, 4.25f) : Vec2F(-8, 1.25f));
        Vec2F target = origin + (flying ? Vec2F(6, 4.25f) : Vec2F(-3, 1.25f));
        PlatformerAStar::Parameters search_parameters;
        search_parameters.must_end_on_ground = !flying;
        search_parameters.max_distance = 20.0f;
        search_parameters.max_nodes_to_search = 2000;
        auto oracle = search_path(world, start, target, parameters, search_parameters, {});
        ASSERT_TRUE(oracle) << oracle.error().what();
        ASSERT_FALSE(oracle->empty());
        EXPECT_EQ((*oracle)[0].source.position, start);
        EXPECT_EQ(oracle->back().target.position, target);
        bool detoured = false;
        for (auto const& edge : *oracle) {
          EXPECT_EQ(edge.action, flying ? PlatformerAStar::Action::Fly : PlatformerAStar::Action::Walk);
          detoured |= edge.target.position[1] >= origin[1] + 6.25f;
          auto collision_poly = *parameters.standingPoly;
          collision_poly.translate(edge.target.position);
          auto collided = world.polyCollision(collision_poly);
          ASSERT_TRUE(collided) << collided.error().what();
          EXPECT_FALSE(*collided);
        }
        if (flying)
          EXPECT_TRUE(detoured);
        for (unsigned budget : {1u, 2u, 7u}) {
          SCOPED_TRACE(budget);
          auto actual = search_path(world, start, target, parameters, search_parameters, budget);
          ASSERT_TRUE(actual) << actual.error().what();
          expect_same_path(*actual, *oracle);
        }
      }
    });
  });

const GameTestRegistration
  platform_replan_target("PathController", "OppositeTargetReplanDoesNotReuseEdgeTimer", [](GameTest& fixture) {
    with_platform_world(fixture, [&](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
      parameters.gravityEnabled = false;
      auto created = ActorMovementController::create(fixture.root(), parameters);
      ASSERT_TRUE(created) << created.error().what();
      auto actor = *created;
      actor->init(world);
      auto uninit = make_scope_exit([&] { actor->uninit(); });
      Vec2F start = origin + Vec2F(0, 8.25f);
      actor->setPosition(start);
      PathController controller(world);
      auto initial = controller.find_path(*actor, start + Vec2F(6, 0));
      ASSERT_TRUE(initial) << initial.error().what();
      EXPECT_FALSE(initial->has_value());
      ASSERT_FALSE(controller.pathfinding());
      EXPECT_EQ(controller.current_action(), PlatformerAStar::Action::Fly);
      // move() first evaluates t=0, then accumulates dt. Keep the shared source
      // position while retaining a nonzero timer on the original rightward edge.
      ASSERT_TRUE(controller.move(*actor, parameters, actor->baseModifiers(), false, 0.0625f));
      EXPECT_EQ(actor->position(), start);
      auto replanned = controller.find_path(*actor, start - Vec2F(6, 0));
      ASSERT_TRUE(replanned) << replanned.error().what();
      ASSERT_FALSE(controller.pathfinding());
      EXPECT_EQ(controller.target_position(), start - Vec2F(6, 0));
      EXPECT_EQ(controller.current_action(), PlatformerAStar::Action::Fly);
      actor->setVelocity(Vec2F(-8, 0));
      ASSERT_TRUE(controller.move(*actor, parameters, actor->baseModifiers(), false, 0.0f));
      // Same Fly action and source, different target: retaining the old timer
      // teleports half a tile left here. A new edge must begin exactly at t=0.
      EXPECT_EQ(actor->position(), start);
      ASSERT_TRUE(controller.move(*actor, parameters, actor->baseModifiers(), false, 0.0625f));
      ASSERT_TRUE(controller.move(*actor, parameters, actor->baseModifiers(), false, 0.0f));
      EXPECT_EQ(actor->position(), start - Vec2F(0.5f, 0));
      bool reached = false;
      for (unsigned tick = 0; tick < 100 && !reached; ++tick) {
        auto moved = controller.move(*actor, parameters, actor->baseModifiers(), false, 0.0625f);
        ASSERT_TRUE(moved) << moved.error().what();
        reached = moved->value_or(false);
      }
      EXPECT_TRUE(reached);
      EXPECT_EQ(actor->position(), start - Vec2F(6, 0));
    });
  });

const GameTestRegistration
  platform_lua_contract("PlatformerAStar", "LuaNamesAndRepeatableResults", [](GameTest& fixture) {
    with_platform_world(fixture, [&](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
      parameters.gravityEnabled = false;
      auto lua_root = LuaRoot::create(fixture.root());
      ASSERT_TRUE(lua_root) << lua_root.error().what();
      auto context = (*lua_root)->luaEngine().createContext();
      ASSERT_TRUE(context) << context.error().what();
      auto callbacks = LuaBindings::makeWorldCallbacks(fixture.root(), world);
      ASSERT_TRUE(callbacks) << callbacks.error().what();
      ASSERT_TRUE(context->setCallbacks("world", *callbacks));
      ASSERT_TRUE(context->set("movement", parameters));
      ASSERT_TRUE(context->set("start_position", origin + Vec2F(-6, 4.25f)));
      ASSERT_TRUE(context->set("target_position", origin + Vec2F(6, 4.25f)));
      auto decoded = context->eval<PlatformerAStar::Parameters>(R"LUA(
        return {maxDistance=21, returnBest=true, mustEndOnGround=true,
          enableWalkSpeedJumps=true, enableVerticalJumpAirControl=true,
          swimCost=11, jumpCost=12, liquidJumpCost=13, dropCost=14,
          boundBox={-1,-2,1,2}, standingBoundBox={-0.5,-2,0.5,2},
          droppingBoundBox={-2,-2,2,2}, smallJumpMultiplier=0.5,
          jumpDropXMultiplier=0.25, maxFScore=123, maxNodesToSearch=456,
          maxLandingVelocity=-9}
      )LUA");
      ASSERT_TRUE(decoded) << decoded.error().what();
      EXPECT_EQ(decoded->max_distance, 21);
      EXPECT_TRUE(decoded->return_best);
      EXPECT_TRUE(decoded->must_end_on_ground);
      EXPECT_TRUE(decoded->enable_walk_speed_jumps);
      EXPECT_TRUE(decoded->enable_vertical_jump_air_control);
      EXPECT_EQ(decoded->swim_cost, 11);
      EXPECT_EQ(decoded->jump_cost, 12);
      EXPECT_EQ(decoded->liquid_jump_cost, 13);
      EXPECT_EQ(decoded->drop_cost, 14);
      EXPECT_EQ(decoded->bound_box, RectF(-1, -2, 1, 2));
      EXPECT_EQ(decoded->standing_bound_box, RectF(-0.5f, -2, 0.5f, 2));
      EXPECT_EQ(decoded->dropping_bound_box, RectF(-2, -2, 2, 2));
      EXPECT_EQ(decoded->small_jump_multiplier, 0.5f);
      EXPECT_EQ(decoded->jump_drop_x_multiplier, 0.25f);
      EXPECT_EQ(decoded->max_f_score, 123);
      EXPECT_EQ(decoded->max_nodes_to_search, 456u);
      EXPECT_EQ(decoded->max_landing_velocity, -9);
      auto defaults = context->eval<PlatformerAStar::Parameters>("return nil");
      ASSERT_TRUE(defaults) << defaults.error().what();
      EXPECT_EQ(*defaults, PlatformerAStar::Parameters());
      PlatformerAStar::Path jump_path{PlatformerAStar::Edge{2.5f,
                                                            PlatformerAStar::Action::Jump,
                                                            Vec2F(3, 7),
                                                            {Vec2F(1, 2), {}},
                                                            {Vec2F(1, 2), Vec2F(3, 7)}}};
      ASSERT_TRUE(context->set("jump_path", jump_path));
      auto checked = context->eval<bool>(R"LUA(
        assert(jump_path[1].action == "Jump")
        assert(jump_path[1].jumpVelocity[1] == 3 and jump_path[1].jumpVelocity[2] == 7)
        assert(jump_path[1].jump_velocity == nil and jump_path[1].cost == 2.5)
        assert(jump_path[1].source.position[1] == 1 and jump_path[1].target.velocity[2] == 7)
        local options = {maxDistance=20, maxNodesToSearch=2000}
        local synchronous = world.findPlatformerPath(start_position, target_position, movement, options)
        assert(synchronous and #synchronous > 0)
        local finder = world.platformerPathStart(start_position, target_position, movement, options)
        local finished
        for i = 1, 20000 do
          finished = finder:explore(2)
          if finished ~= nil then break end
        end
        assert(finished == true and finder:explore(1) == true)
        local first, second = finder:result(), finder:result()
        assert(#first == #second and #first == #synchronous)
        for i, edge in ipairs(first) do
          assert(edge.action == "Fly" and edge.cost == synchronous[i].cost)
          assert(second[i].action == edge.action and type(edge.jumpVelocity) == "table")
          for axis = 1, 2 do
            assert(edge.source.position[axis] == second[i].source.position[axis])
            assert(edge.target.position[axis] == second[i].target.position[axis])
            assert(edge.target.position[axis] == synchronous[i].target.position[axis])
          end
        end
        return true
      )LUA");
      ASSERT_TRUE(checked) << checked.error().what();
      EXPECT_TRUE(*checked);
    });
  });

}// namespace
