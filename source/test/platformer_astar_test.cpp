#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.actor_movement_controller;
import star.collision_block;
import star.file;
import star.format;
import star.game_test;
import star.game_types;
import star.liquid_types;
import star.lua;
import star.lua_actor_movement_component;
import star.lua_components;
import star.lua_converters;
import star.lua_game_converters;
import star.lua_root;
import star.material_database;
import star.material_types;
import star.movement_controller;
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
                                                                std::optional<unsigned> budget,
                                                                std::optional<std::reference_wrapper<unsigned>> pending = {}) {
  auto created =
    PlatformerAStar::PathFinder::create(world, start, target, std::move(parameters), std::move(search_parameters));
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto finder = std::move(*created);
  for (unsigned iteration = 0; iteration < 20000; ++iteration) {
    auto explored = finder->explore(budget);
    if (!explored)
      return std::unexpected(std::move(explored.error()));
    if (!*explored) {
      if (pending)
        ++pending->get();
      continue;
    }
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

void record_path(std::string const& label, PlatformerAStar::Path const& path, unsigned pending) {
  // Hex float bits retain every coordinate, velocity and cost in edge order.
  std::string trace = std::to_string(pending);
  auto vector = [&](Vec2F const& vec) {
    for (float component : {vec[0], vec[1]})
      std::format_to(std::back_inserter(trace), ",{:08x}", std::bit_cast<std::uint32_t>(component));
  };
  auto node = [&](PlatformerAStar::Node const& value) {
    vector(value.position);
    trace += value.velocity ? ",v" : ",-";
    if (value.velocity)
      vector(*value.velocity);
  };
  for (auto const& edge : path) {
    std::format_to(std::back_inserter(trace), ";{},{:08x}", static_cast<unsigned>(edge.action),
                   std::bit_cast<std::uint32_t>(edge.cost));
    node(edge.source);
    node(edge.target);
    vector(edge.jump_velocity);
  }
  testing::Test::RecordProperty(label, trace);
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
      // Free fall and platform drop enter arc simulation differently; both
      // must eventually preserve the zero-cost Land transition.
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
        unsigned pending = 0;
        auto budgeted = search_path(world, start, target, parameters, search_parameters, 2, std::ref(pending));
        ASSERT_TRUE(budgeted) << budgeted.error().what();
        expect_same_path(*budgeted, *path);
        EXPECT_GT(pending, 0u);
        record_path(drop ? "platform_drop" : "free_fall", *budgeted, pending);
        // The final Arc can cross the radius, but the simultaneous Land starts
        // at its out-of-radius contact, not at the expanded Arc source.
        ASSERT_GE(path->size(), 2u);
        auto const& last_arc = (*path)[path->size() - 2];
        ASSERT_EQ(last_arc.action, PlatformerAStar::Action::Arc);
        float arc_source_distance = world.geometry().diff(start, last_arc.source.position).magnitude();
        float contact_distance = world.geometry().diff(start, last_arc.target.position).magnitude();
        ASSERT_LT(arc_source_distance, contact_distance);
        search_parameters.max_distance = std::midpoint(arc_source_distance, contact_distance);
        auto restricted = PlatformerAStar::PathFinder::create(world, start, target, parameters, search_parameters);
        ASSERT_TRUE(restricted) << restricted.error().what();
        auto done = (*restricted)->explore({});
        ASSERT_TRUE(done) << done.error().what();
        EXPECT_EQ(*done, std::optional<bool>(false));
        done = (*restricted)->explore(1);
        ASSERT_TRUE(done) << done.error().what();
        EXPECT_EQ(*done, std::optional<bool>(false));
        EXPECT_FALSE((*restricted)->result());
      }
    });
  });

const GameTestRegistration platform_slow_arcs(
  "PlatformerAStar", "SlowArcsCanProgressStallOrExhaustSimulationWork", [](GameTest& fixture) {
    with_platform_world(fixture, [](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
      parameters.gravity_enabled = true;
      parameters.air_jump_profile.jump_speed.reset();
      parameters.walk_speed.reset();
      parameters.run_speed.reset();
      auto search_parameters = explicit_bounds();
      search_parameters.must_end_on_ground = false;

      {
        SCOPED_TRACE("a tiny falling step initially leaves position and rounded position unchanged");
        parameters.gravity_multiplier = 2.0e-6f;
        Vec2F start = origin + Vec2F(-3, 8.25f);
        Vec2F target = start - Vec2F(0, 1);
        auto path = search_path(world, start, target, parameters, search_parameters, 1);
        ASSERT_TRUE(path) << path.error().what();
        ASSERT_EQ(path->size(), 1u);
        auto const& edge = path->front();
        EXPECT_EQ(edge.action, PlatformerAStar::Action::Arc);
        EXPECT_EQ(edge.source.position, start);
        EXPECT_FALSE(edge.source.velocity);
        EXPECT_FLOAT_EQ(edge.target.position[0], start[0]);
        EXPECT_LT(edge.target.position[1], start[1] - 0.5f);
        EXPECT_GT(edge.target.position[1], target[1]);
        ASSERT_TRUE(edge.target.velocity);
        EXPECT_GT((*edge.target.velocity)[1], -0.05f);
        EXPECT_LT((*edge.target.velocity)[1], 0.0f);
      }

      {
        SCOPED_TRACE("neither position nor velocity can change in a very small jump");
        parameters.gravity_multiplier = 1.0e-15f;
        parameters.air_jump_profile.jump_speed = 1.0e-4f;
        Vec2F start = origin + Vec2F(-3, 1.25f);
        auto local_search = search_parameters;
        local_search.jump_cost = 0.5f;
        local_search.max_f_score = 5.0;
        auto finder = PlatformerAStar::PathFinder::create(world, start, start + Vec2F(0, 2), parameters, local_search);
        ASSERT_TRUE(finder) << finder.error().what();
        // This score window keeps the vertical Jump nodes, but prunes Walks.
        auto started = (*finder)->explore(1);
        ASSERT_TRUE(started) << started.error().what();
        ASSERT_FALSE(started->has_value());
        auto result = (*finder)->explore();
        ASSERT_TRUE(result) << result.error().what();
        EXPECT_EQ(*result, std::optional<bool>(false));
        EXPECT_FALSE((*finder)->result());
      }

      {
        SCOPED_TRACE("changing velocity does not make a same-node arc unbounded");
        parameters.gravity_multiplier = 1.0e-12f;
        parameters.air_jump_profile.jump_speed.reset();
        Vec2F start = origin + Vec2F(-3, 8.25f);
        auto finder =
          PlatformerAStar::PathFinder::create(world, start, start - Vec2F(0, 1), parameters, search_parameters);
        ASSERT_TRUE(finder) << finder.error().what();
        auto exhausted = (*finder)->explore(1);
        ASSERT_FALSE(exhausted);
        EXPECT_TRUE(exhausted.error().is<MovementControllerException>());
        EXPECT_NE(std::string_view(exhausted.error().what()).find("PathFinder arc simulation exceeded"),
                  std::string_view::npos);
        auto repeated = (*finder)->explore();
        ASSERT_FALSE(repeated);
        EXPECT_EQ(repeated.error().what(), exhausted.error().what());
        EXPECT_FALSE((*finder)->result());
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
      actor->set_position(start);
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
      actor->set_velocity(Vec2F(-8, 0));
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
    actor.set_position(start);
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
    actor.set_position(start);
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
      ASSERT_TRUE(context->set("start_position", origin + Vec2F(-16, 8.25f)));
      ASSERT_TRUE(context->set("target_position", origin + Vec2F(16, 8.25f)));
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
      using PlatformerAStar::Action;
      std::array const action_values{Action::Walk, Action::Jump, Action::Arc, Action::Drop,
                                     Action::Swim, Action::Fly, Action::Land};
      PlatformerAStar::Path actions;
      for (auto action : action_values)
        actions.push_back({2.5f, action, Vec2F(3, 7), {Vec2F(1, 2), {}}, {Vec2F(1, 2), Vec2F(0, 0)}});
      ASSERT_TRUE(context->set("actions", actions));
      auto invalid_actions = actions;
      invalid_actions.back().action = static_cast<Action>(-1);
      auto invalid_name = PlatformerAStar::action_name(invalid_actions.back().action);
      auto invalid_export = context->set("actions", invalid_actions);
      ASSERT_FALSE(invalid_name);
      ASSERT_FALSE(invalid_export);
      auto const& diagnostic = invalid_name.error().diagnostic_summary();
      ASSERT_TRUE(diagnostic);
      EXPECT_EQ(diagnostic->category, "MapException");
      EXPECT_EQ(diagnostic->reason, "Key '-1' not found in Map::get()");
      EXPECT_EQ(invalid_export.error().what(), invalid_name.error().what());
      auto checked = context->eval<bool>(R"LUA(
        local names = {"Walk", "Jump", "Arc", "Drop", "Swim", "Fly", "Land"}
        assert(#actions == #names)
        for i, name in ipairs(names) do
          local edge = actions[i]
          assert(edge.action == name and edge.cost == 2.5 and edge.jump_velocity == nil)
          assert(edge.jumpVelocity[1] == 3 and edge.jumpVelocity[2] == 7)
          assert(edge.source.position[1] == 1 and edge.target.position[2] == 2)
          assert(edge.source.velocity == nil and edge.target.velocity[1] == 0 and edge.target.velocity[2] == 0)
          assert(edge.source.position ~= edge.target.position)
          assert(edge.source ~= edge.target and edge.jumpVelocity ~= edge.target.velocity)
        end
        local options = {maxDistance=40, maxNodesToSearch=2000}
        local synchronous = world.findPlatformerPath(start_position, target_position, movement, options)
        assert(synchronous and #synchronous > 0)
        local finder = world.platformerPathStart(start_position, target_position, movement, options)
        assert(finder:result() == nil)
        local finished
        for i = 1, 20000 do
          finished = finder:explore(2)
          if finished ~= nil then break end
        end
        assert(finished == true and finder:explore(1) == true)
        local first, second = finder:result(), finder:result()
        assert(first ~= second and #first >= 30 and #first == #second and #first == #synchronous)
        for i, edge in ipairs(first) do
          local other = second[i]
          assert(edge ~= other and edge.action == "Fly" and edge.cost == synchronous[i].cost)
          assert(other.action == edge.action and other.cost == edge.cost and edge.jumpVelocity ~= other.jumpVelocity)
          for _, endpoint in ipairs({"source", "target"}) do
            local node, other_node = edge[endpoint], other[endpoint]
            assert(node ~= other_node and node.position ~= other_node.position)
            assert(node.velocity == nil and other_node.velocity == nil)
            for axis = 1, 2 do
              assert(node.position[axis] == other_node.position[axis])
              assert(node.position[axis] == synchronous[i][endpoint].position[axis])
              assert(edge.jumpVelocity[axis] == 0 and other.jumpVelocity[axis] == 0)
            end
          end
        end
        first[1].action = "changed"
        first[1].jumpVelocity[2] = 100
        first[1].source.position[1] = 200
        first[1].target.velocity = {0, 0}
        first[2] = nil
        local third = finder:result()
        assert(third ~= second and #third == #second and third[2] ~= second[2])
        for _, path in ipairs({second, third}) do
          assert(path[1].action == "Fly" and path[1].jumpVelocity[2] == 0)
          assert(path[1].source.position[1] == start_position[1] and path[1].target.velocity == nil)
        end
        actions[1].target.velocity[1] = 100
        actions[1].source.position[2] = 200
        assert(actions[2].target.velocity[1] == 0 and actions[2].source.position[2] == 2)
        return true
      )LUA");
      ASSERT_TRUE(checked) << checked.error().what();
      EXPECT_TRUE(*checked);
    });
  });

const GameTestRegistration platform_semi_submerged_jump(
  "PlatformerAStar", "SemiSubmergedJumpKeepsLiquidAndSwimCost", [](GameTest& fixture) {
    with_platform_world(fixture, [&](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
      Vec2F start = origin + Vec2F(-8, 2.25f);
      Vec2F target = start + Vec2F(0, 1);
      for (int x : {-1, 0})
        ASSERT_TRUE(world.setLiquid(Vec2I(start) + Vec2I(x, 0), LiquidId(1), 0.75f, 1.0f));
      parameters.gravity_enabled = true;
      parameters.minimum_liquid_percentage = 0.5f;
      parameters.air_jump_profile.jump_speed = 24.0f;
      parameters.walk_speed.reset();
      parameters.run_speed.reset();
      auto options = explicit_bounds();
      options.must_end_on_ground = false;
      options.jump_cost = 1.0f;
      options.liquid_jump_cost = 7.0f;
      options.swim_cost = 2.0f;
      auto path = search_path(world, start, target, parameters, options, {});
      ASSERT_TRUE(path) << path.error().what();
      ASSERT_EQ(path->size(), 2u);
      EXPECT_EQ((*path)[0].action, PlatformerAStar::Action::Jump);
      EXPECT_EQ((*path)[0].source.position, start);
      EXPECT_EQ((*path)[0].target.position, start);
      ASSERT_TRUE((*path)[0].target.velocity);
      EXPECT_EQ((*path)[0].jump_velocity, *(*path)[0].target.velocity);
      EXPECT_FLOAT_EQ((*path)[0].cost, 14.0f);
      EXPECT_EQ(path->back().action, PlatformerAStar::Action::Arc);
      EXPECT_EQ(path->back().source, (*path)[0].target);
      // The goal accepts arc samples within one node of the requested point.
      EXPECT_EQ(path->back().target.position, start + Vec2F(0, 0.5f));
      EXPECT_FLOAT_EQ(path->back().cost, 0.5f);
      unsigned pending = 0;
      auto incremental = search_path(world, start, target, parameters, options, 1, std::ref(pending));
      ASSERT_TRUE(incremental) << incremental.error().what();
      expect_same_path(*incremental, *path);
      EXPECT_GT(pending, 0u);
      record_path("semi_submerged_jump", *incremental, pending);
    });
  });

const GameTestRegistration platform_compound_slopes(
  "PlatformerAStar", "CompoundPlatformSlopesPreserveWalkingTransitions", [](GameTest& fixture) {
    with_platform_world(fixture, [&](WorldServer& world, Vec2F origin, ActorMovementParameters parameters) {
      auto materials = fixture.root().material_database();
      ASSERT_TRUE(materials) << materials.error().what();
      auto dirt = (*materials)->material_id("dirt");
      ASSERT_TRUE(dirt) << dirt.error().what();
      // Two connected ramps with opposing directions share the high platform.
      for (auto offset : {Vec2I(-11, 4), Vec2I(-10, 4), Vec2I(-9, 4), Vec2I(-9, 5),
                          Vec2I(-8, 5), Vec2I(-8, 6), Vec2I(-7, 6), Vec2I(-6, 5),
                          Vec2I(-5, 4), Vec2I(-4, 4)}) {
        SCOPED_TRACE(std::format("{}, {}", offset[0], offset[1]));
        auto tile = world.modifyServerTile(Vec2I(origin) + offset, true);
        ASSERT_TRUE(tile) << tile.error().what();
        ASSERT_TRUE(*tile);
        (*tile)->get().background = structure_material_id;
        auto placed = world.forceModifyTile(Vec2I(origin) + offset,
          PlaceMaterial{TileLayer::Foreground, *dirt, MaterialHue(0), TileCollisionOverride::Platform}, true);
        ASSERT_TRUE(placed) << placed.error().what();
        ASSERT_TRUE(*placed);
      }
      parameters.gravity_enabled = true;
      parameters.air_jump_profile.jump_speed.reset();
      parameters.run_speed.reset();
      auto options = explicit_bounds();
      // Place the feet just right of the node so the platform already passed
      // cannot also support the forward probe while descending the right ramp.
      options.bound_box = RectF(0.1f, -0.25f, 0.4f, 0.25f);
      Vec2F start = origin + Vec2F(-9, 5.25f);
      Vec2F target = origin + Vec2F(-6, 6.25f);
      auto path = search_path(world, start, target, parameters, options, {});
      ASSERT_TRUE(path) << path.error().what();
      std::array const ordered_steps{Vec2F(1, 1), Vec2F(1, 1), Vec2F(1, -1)};
      ASSERT_EQ(path->size(), ordered_steps.size());
      PlatformerAStar::Node previous{start, {}};
      for (std::size_t i = 0; i < ordered_steps.size(); ++i) {
        auto const& edge = (*path)[i];
        EXPECT_EQ(edge.action, PlatformerAStar::Action::Walk);
        EXPECT_EQ(edge.source, previous);
        EXPECT_FALSE(edge.target.velocity);
        EXPECT_EQ(world.geometry().diff(edge.target.position, edge.source.position), ordered_steps[i]);
        EXPECT_FLOAT_EQ(edge.cost, ordered_steps[i].magnitude());
        previous = edge.target;
      }
      EXPECT_EQ(previous.position, target);
      unsigned pending = 0;
      auto incremental = search_path(world, start, target, parameters, options, 1, std::ref(pending));
      ASSERT_TRUE(incremental) << incremental.error().what();
      expect_same_path(*incremental, *path);
      EXPECT_GT(pending, 0u);
      record_path("compound_slopes", *incremental, pending);
    });
  });

}// namespace
