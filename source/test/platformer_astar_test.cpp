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
import star.lua_actor_movement_component;
import star.lua_components;
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
  ASSERT_TRUE(world->generateRegion(RectI::with_size(origin - Vec2I(20, 16), Vec2I(40, 40))));
  ASSERT_TRUE(world->setDungeonGravity(no_dungeon_id, 60.0f));
  ASSERT_TRUE(world->setDungeonGravity(construction_dungeon_id, 60.0f));
  auto materials = root.material_database();
  ASSERT_TRUE(materials) << materials.error().what();
  auto dirt = (*materials)->material_id("dirt");
  ASSERT_TRUE(dirt) << dirt.error().what();
  auto place = [&](Vec2I position) -> std::expected<void, StarException> {
    auto tile = world->modifyServerTile(position, true);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    if (!*tile)
      return std::unexpected(StarException("Platform fixture tile is unavailable"));
    (*tile)->get().background = structure_material_id;
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

  auto defaults = ActorMovementParameters::sensible_defaults(root);
  ASSERT_TRUE(defaults) << defaults.error().what();
  auto parameters = std::move(*defaults);
  parameters.standing_poly = PolyF(RectF(-0.25f, -0.25f, 0.25f, 0.25f));
  parameters.crouching_poly = parameters.standing_poly;
  parameters.mass = 1.0f;
  parameters.air_force = 128.0f;
  parameters.fly_speed = 8.0f;
  parameters.run_speed = 8.0f;
  parameters.walk_speed = 4.0f;
  parameters.gravity_multiplier = 1.0f;
  parameters.air_buoyancy = 0.0f;
  parameters.liquid_buoyancy = 0.0f;
  parameters.path_explore_rate = 1000.0f;
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
  auto materials = root.material_database();
  if (!materials)
    return std::unexpected(std::move(materials.error()));
  auto dirt = (*materials)->material_id("dirt");
  if (!dirt)
    return std::unexpected(std::move(dirt.error()));
  for (int x = -8; x <= -6; ++x) {
    Vec2I position = Vec2I(origin) + Vec2I(x, 5);
    auto tile = world.modifyServerTile(position, true);
    if (!tile)
      return std::unexpected(std::move(tile.error()));
    if (!*tile)
      return std::unexpected(StarException("Platform fixture ledge tile is unavailable"));
    (*tile)->get().background = structure_material_id;
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
    auto collided = world.poly_collision(PolyF(full_bounds.translated(edge.target.position)));
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
      parameters.gravity_enabled = true;
      parameters.air_jump_profile.jump_speed.reset();
      parameters.walk_speed.reset();
      parameters.run_speed.reset();
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
      parameters.gravity_enabled = true;
      parameters.air_jump_profile.jump_speed.reset();
      parameters.walk_speed = 4.0f;
      parameters.run_speed.reset();
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
      parameters.gravity_enabled = true;
      parameters.air_jump_profile.jump_speed.reset();
      parameters.standing_poly.reset();
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
        parameters.gravity_enabled = !flying;
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
          auto collision_poly = *parameters.standing_poly;
          collision_poly.translate(edge.target.position);
          auto collided = world.poly_collision(collision_poly);
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
      parameters.gravity_enabled = false;
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
      ASSERT_TRUE(controller.move(*actor, parameters, actor->base_modifiers(), false, 0.0625f));
      EXPECT_EQ(actor->position(), start);
      auto replanned = controller.find_path(*actor, start - Vec2F(6, 0));
      ASSERT_TRUE(replanned) << replanned.error().what();
      ASSERT_FALSE(controller.pathfinding());
      EXPECT_EQ(controller.target_position(), start - Vec2F(6, 0));
      EXPECT_EQ(controller.current_action(), PlatformerAStar::Action::Fly);
      actor->setVelocity(Vec2F(-8, 0));
      ASSERT_TRUE(controller.move(*actor, parameters, actor->base_modifiers(), false, 0.0f));
      // Same Fly action and source, different target: retaining the old timer
      // teleports half a tile left here. A new edge must begin exactly at t=0.
      EXPECT_EQ(actor->position(), start);
      ASSERT_TRUE(controller.move(*actor, parameters, actor->base_modifiers(), false, 0.0625f));
      ASSERT_TRUE(controller.move(*actor, parameters, actor->base_modifiers(), false, 0.0f));
      EXPECT_EQ(actor->position(), start - Vec2F(0.5f, 0));
      bool reached = false;
      for (unsigned tick = 0; tick < 100 && !reached; ++tick) {
        auto moved = controller.move(*actor, parameters, actor->base_modifiers(), false, 0.0625f);
        ASSERT_TRUE(moved) << moved.error().what();
        reached = moved->value_or(false);
      }
      EXPECT_TRUE(reached);
      EXPECT_EQ(actor->position(), start - Vec2F(6, 0));
    });
  });

const GameTestRegistration actor_path_budget("PathController", "ActorCollectsOneStepAndInvalidatesChangedSearch", [](GameTest& fixture) {
  with_platform_world(fixture, [&](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
    parameters.gravity_enabled = false;
    parameters.path_explore_rate = 1.0f;
    auto created = ActorMovementController::create(fixture.root(), parameters);
    ASSERT_TRUE(created) << created.error().what();
    auto& actor = **created;
    actor.init(world);
    auto uninit = make_scope_exit([&] { actor.uninit(); });
    Vec2F start = origin + Vec2F(0, 8.25f);
    Vec2F blocked = origin + Vec2F(2, 3.25f); // Inside the fixture's wall.
    actor.setPosition(start);
    auto expect_step = [](auto result, std::optional<bool> expected) {
      ASSERT_TRUE(result) << result.error().what();
      EXPECT_EQ(*result, expected);
    };
    PlatformerAStar::Parameters limit;
    limit.max_nodes_to_search = 2;
    expect_step(actor.path_move(blocked, limit), {});
    expect_step(actor.path_move(blocked, limit), {});
    expect_step(actor.path_move(blocked, limit), false);
    expect_step(actor.path_move(blocked, limit), {});
    limit.max_nodes_to_search = 0;
    expect_step(actor.path_move(blocked, limit), false); // Same target, different in-flight policy.

    limit.max_nodes_to_search = 1;
    expect_step(actor.control_path_move(blocked, false, limit), {});
    ASSERT_TRUE(actor.tick_master(1.0f / 60.0f));
    expect_step(actor.path_move(blocked), false); // Tick exhausted the search; don't restart it to poll.
    expect_step(actor.control_path_move(blocked, false, limit), {});
    ASSERT_TRUE(actor.tick_master(1.0f / 60.0f));
    limit.max_nodes_to_search = 2;
    expect_step(actor.path_move(blocked, limit), {}); // Old tick result does not match the new policy.

    // Finish a short flight search, then change the policy before taking its first edge.
    Vec2F nearby = start + Vec2F(1, 0);
    expect_step(actor.path_move(nearby), {});
    expect_step(actor.path_move(nearby), {});
    ASSERT_FALSE(actor.pathfinding());
    limit.max_nodes_to_search = 0;
    expect_step(actor.path_move(nearby, limit), false);

    // A background replan can also terminate inside move(), while an old edge is still active.
    PathController controller(world);
    limit.max_nodes_to_search = 2;
    controller.set_parameters(limit);
    expect_step(controller.find_path(actor, nearby), {});
    expect_step(controller.find_path(actor, nearby), {});
    ASSERT_TRUE(controller.current_action());
    expect_step(controller.find_path(actor, blocked), {});
    expect_step(controller.move(actor, parameters, actor.base_modifiers(), false, 0.0f), {});
    expect_step(controller.move(actor, parameters, actor.base_modifiers(), false, 0.0f), false);
  });
});

const GameTestRegistration lua_path_timing("PathController", "LuaObservesImmediateBufferedAndTickResults", [](GameTest& fixture) {
  with_platform_world(fixture, [&](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
    parameters.gravity_enabled = false;
    parameters.path_explore_rate = 1.0f;
    auto created = ActorMovementController::create(fixture.root(), parameters);
    ASSERT_TRUE(created) << created.error().what();
    auto& actor = **created;
    actor.init(world);
    auto uninit = make_scope_exit([&] { actor.uninit(); });
    Vec2F start = origin + Vec2F(0, 8.25f);
    Vec2F blocked = origin + Vec2F(2, 3.25f);
    actor.setPosition(start);
    LuaActorScript script(fixture.root());
    ASSERT_TRUE(script.add_actor_movement_callbacks(actor));
    ASSERT_TRUE(script.init(world));
    auto stop = make_scope_exit([&] { EXPECT_TRUE(script.uninit()); });
    auto defined = script.eval<bool>(R"LUA(
      function update(target, limit)
        local reached = mcontroller.controlPathMove(target, false, {maxNodesToSearch = limit})
        if reached == nil then return -1 end
        return reached and 1 or 0
      end
      return true
    )LUA");
    ASSERT_TRUE(defined) << defined.error().what();
    auto step = [&](Vec2F target, int limit, int expected) {
      auto observed = script.update<int>(target, limit);
      ASSERT_TRUE(observed) << observed.error().what();
      EXPECT_EQ(*observed, std::optional<int>(expected));
      ASSERT_TRUE(actor.tick_master(1.0f / 60.0f));
    };
    step(start, 1, 1);    // Immediate success, not delayed to the next Lua update.
    step(blocked, 1, -1); // Callback pending, perform_controls exhausts the search.
    step(blocked, 1, 0);  // The buffered false is collected once.
    step(blocked, 1, -1);
    step(blocked, 2, -1); // Changed options invalidate the buffered result for this same position.
    step(blocked, 2, 0);  // This time tickMaster exhausted it, after both script phases.
  });
});

const GameTestRegistration
  platform_lua_contract("PlatformerAStar", "LuaNamesAndRepeatableResults", [](GameTest& fixture) {
    with_platform_world(fixture, [&](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
      parameters.gravity_enabled = false;
      auto lua_root = LuaRoot::create(fixture.root());
      ASSERT_TRUE(lua_root) << lua_root.error().what();
      auto context = (*lua_root)->lua_engine().create_context();
      ASSERT_TRUE(context) << context.error().what();
      auto callbacks = LuaBindings::makeWorldCallbacks(fixture.root(), world);
      ASSERT_TRUE(callbacks) << callbacks.error().what();
      ASSERT_TRUE(context->set_callbacks("world", *callbacks));
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
