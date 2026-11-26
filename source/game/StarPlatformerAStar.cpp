import std;
import star.format;
import star.a_star;
import star.list;
import star.vector;
import star.rect;
import star.poly;
import star.math_common;
import star.world_geometry;
import star.collision_block;
import star.world;
import star.movement_controller;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.liquid_types;
import star.platformer_astar;

namespace Star {

namespace PlatformerAStar {

  // The desired spacing between nodes:
float const node_granularity = 1.f;
float const simulate_arc_granularity = 0.5f;
// Per arc, independent of the A* node budget: bound changing states that
// never leave the rounded node without treating them as blocked.
unsigned constexpr max_arc_steps = 4096;

float const default_max_distance = 50.0f;
float const default_small_jump_multiplier = 0.75f;
float const default_jump_drop_x_multiplier = 0.125f;

float const default_swim_cost = 40.0f;
float const default_jump_cost = 3.0f;
float const default_liquid_jump_cost = 10.0f;
float const default_drop_cost = 3.0f;

float const default_max_landing_velocity = -5.0f;

// Bounding boxes are shrunk slightly to work around floating point rounding
// errors.
float const bound_box_rounding_error_scaling = 0.99f;

CollisionSet const collision_solid{CollisionKind::Null, CollisionKind::Slippery, CollisionKind::Block};

CollisionSet const collision_floor_only{CollisionKind::Null, CollisionKind::Block, CollisionKind::Slippery,
                                        CollisionKind::Platform};

CollisionSet const collision_dynamic{CollisionKind::Dynamic};

CollisionSet const collision_any{CollisionKind::Null, CollisionKind::Platform, CollisionKind::Dynamic,
                                 CollisionKind::Slippery, CollisionKind::Block};

PathFinder::PathFinder(ConstructionKey, World& world, Vec2F search_from, Vec2F search_to,
                       ActorMovementParameters movement_parameters, Parameters search_parameters)
    : m_world(world), m_search_from(search_from), m_search_to(search_to),
      m_movement_params(std::move(movement_parameters)), m_search_params(std::move(search_parameters)) {
  if (!m_search_params.bound_box && !m_movement_params.standing_poly)
    return;

  RectF const& full = m_search_params.bound_box ? *m_search_params.bound_box : standing_bounds();
  RectF const scaled_full = full.scaled(bound_box_rounding_error_scaling);
  auto const scaled_variant = [&scaled_full](std::optional<RectF> const& variant) {
    return variant ? variant->scaled(bound_box_rounding_error_scaling) : scaled_full;
  };
  m_local_bounds.emplace(LocalBounds{
      .full = scaled_full,
      .drop = scaled_variant(m_search_params.dropping_bound_box),
      .stand = scaled_variant(m_search_params.standing_bound_box),
      .foot_offset = full.y_min(),
  });
}

std::expected<std::shared_ptr<PathFinder>, StarException>
PathFinder::create(World& world, Vec2F search_from, Vec2F search_to, ActorMovementParameters movement_parameters,
                   Parameters search_parameters) {
  auto finder = std::make_shared<PathFinder>(ConstructionKey{}, world, search_from, search_to,
                                             std::move(movement_parameters), std::move(search_parameters));
  auto initialized = finder->init_a_star();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return finder;
}

std::expected<std::optional<bool>, StarException> PathFinder::explore(std::optional<unsigned> max_explore_nodes) {
  return (*m_astar).explore(max_explore_nodes);
}

  std::optional<Path> const& PathFinder::result() const {
    return (*m_astar).result();
  }

  std::optional<Path> PathFinder::extract_result() { return m_astar->extract_result(); }

  std::expected<void, StarException> PathFinder::init_a_star() {
    auto heuristic_cost_fn = [&self = *this](Node const& from_node,
                                             Node const& to_node) -> std::expected<double, StarException> {
      return self.heuristic_cost(from_node.position, to_node.position);
    };
    auto goal_reached_fn = [&self = *this](Node const& node) -> std::expected<bool, StarException> {
      if (self.m_search_params.must_end_on_ground) {
        auto grounded = self.on_ground(node.position);
        if (!grounded)
          return std::unexpected(std::move(grounded.error()));
        if (!*grounded || node.velocity.has_value())
          return false;
      }
      return self.distance(node.position, self.m_search_to) < node_granularity;
    };
    auto neighbors_fn = [&self = *this](Node const& node, List<Edge>& result) -> std::expected<void, StarException> {
      auto generated = self.neighbors(node, result);
      if (!generated)
        return std::unexpected(std::move(generated.error()));
      if (result.empty())
        return {};
      float const limit = self.m_search_params.max_distance.value_or(default_max_distance);
      bool const within_limit = self.distance(node.position, self.m_search_from) <= limit;
      result.filter([&self, within_limit, limit](Edge const& edge) {
        // Land starts at the simulated contact, not the node being expanded.
        return edge.action == Action::Land ? self.distance(edge.source.position, self.m_search_from) <= limit
                                           : within_limit;
      });
      return {};
    };
    auto validate_end_fn = [&self = *this](Edge const& edge) -> std::expected<bool, StarException> {
      if (!self.m_search_params.must_end_on_ground)
        return true;
      auto grounded = self.on_ground(edge.target.position);
      if (!grounded)
        return std::unexpected(std::move(grounded.error()));
      return *grounded && edge.action != Action::Jump;
    };
    auto rounded_from = round_to_node(m_search_from);
    if (!rounded_from)
      return std::unexpected(std::move(rounded_from.error()));
    auto rounded_to = round_to_node(m_search_to);
    if (!rounded_to)
      return std::unexpected(std::move(rounded_to.error()));
    m_astar.emplace(heuristic_cost_fn, neighbors_fn, goal_reached_fn, m_search_params.return_best,
                    std::optional<AStar::Search<Edge, Node>::ValidateEndFunction>{validate_end_fn},
                    m_search_params.max_f_score, m_search_params.max_nodes_to_search);
    return m_astar->start(Node{*rounded_from, {}}, Node{*rounded_to, {}});
  }

  float PathFinder::heuristic_cost(Vec2F const& from_position, Vec2F const& to_position) const {
    // This function is used to estimate the cost of travel between two nodes.
    // Underestimating the actual cost results in A* giving the optimal path.
    // Overestimating results in A* finding a non-optimal path, but terminating
    // more quickly when there is a route to the target.
    // We don't really care all that much about getting the optimal path as long
    // as we get one that looks feasible, so we deliberately overestimate here.
    Vec2F diff = m_world.get().geometry().diff(from_position, to_position);
    // Manhattan distance * 2:
    return 2.0f * (std::abs(diff[0]) + std::abs(diff[1]));
  }

  Edge PathFinder::default_cost_edge(Action action, Node const& source, Node const& target) const {
    return Edge{distance(source.position, target.position), action, Vec2F(0, 0), source, target};
  }

  std::expected<void, StarException> PathFinder::neighbors(Node const& node, List<Edge>& neighbors) const {
    if (node.velocity)
      return get_arc_neighbors(node, neighbors);
    auto level = liquid_level(node.position);
    if (!level) return std::unexpected(std::move(level.error()));
    if (in_liquid(*level))
      return get_swimming_neighbors(node, neighbors, *level);
    auto acc = acceleration(node.position);
    if (!acc) return std::unexpected(std::move(acc.error()));
    if ((*acc)[1] == 0.0f)
      return get_flying_neighbors(node, neighbors);
    auto grounded = on_ground(node.position);
    if (!grounded) return std::unexpected(std::move(grounded.error()));
    if (!*grounded)
      return get_falling_neighbors(node, neighbors);
    auto walking = get_walking_neighbors(node, neighbors);
    if (!walking) return std::unexpected(std::move(walking.error()));
    auto solid_ground = on_solid_ground(node.position);
    if (!solid_ground)
      return std::unexpected(std::move(solid_ground.error()));
    if (!*solid_ground) {
      auto drop = get_drop_neighbors(node, neighbors, (*acc)[1]);
      if (!drop) return std::unexpected(std::move(drop.error()));
    }
    return get_jumping_neighbors(node, neighbors, m_search_params.jump_cost.value_or(default_jump_cost));
  }

  std::expected<void, StarException>
  PathFinder::get_drop_neighbors(Node const& node, List<Edge>& neighbors, float vertical_acceleration) const {
    auto drop_position = node.position + Vec2F(0, -1);
    // Platforms resting directly on solid surfaces cannot be dropped through.
    auto grounded = on_solid_ground(drop_position);
    if (!grounded) return std::unexpected(std::move(grounded.error()));
    if (!*grounded) {
      float drop_cost = m_search_params.drop_cost.value_or(default_drop_cost);
      float drop_speed = vertical_acceleration * std::sqrt(2.0 / std::abs(vertical_acceleration));
      neighbors.push_back(Edge{drop_cost, Action::Drop, Vec2F(0, 0), node, Node{drop_position, Vec2F(0, drop_speed)}});
    }
    return {};
  }

  std::expected<void, StarException>
  PathFinder::get_walking_neighbors_in_direction(Node const& node, List<Edge>& neighbors, RectF const& local_bounds,
                                                 float direction, Slope slope) const {
    auto const add_node = [&self = *this, &node, &neighbors](Node const& target) {
      neighbors.push_back(self.default_cost_edge(Action::Walk, node, target));
    };
    auto const tile_bounds_at = [&local_bounds](Vec2F const& position) {
      return RectI::integral(local_bounds.translated(position));
    };

    // Uphill tests ground before clearance; flat and downhill do the reverse.
    if (slope == Slope::Up) {
      Vec2F const uphill = node.position + Vec2F(direction, 1);
      RectI const uphill_bounds = tile_bounds_at(uphill);
      auto grounded = on_ground(uphill_bounds);
      if (!grounded) return std::unexpected(std::move(grounded.error()));
      if (*grounded) {
        auto valid = valid_position(uphill_bounds);
        if (!valid) return std::unexpected(std::move(valid.error()));
        if (*valid) {
          add_node(Node{uphill, {}});
          return {};
        }
      }
    }

    Vec2F forward = node.position + Vec2F(direction, 0);
    RectI const forward_bounds = tile_bounds_at(forward);
    auto forward_valid = valid_position(forward_bounds);
    if (!forward_valid)
      return std::unexpected(std::move(forward_valid.error()));
    if (!*forward_valid)
      return {};
    auto grounded = on_ground(forward_bounds);
    if (!grounded) return std::unexpected(std::move(grounded.error()));
    if (*grounded) {
      add_node(Node{forward, {}});
      return {};
    }

    if (slope == Slope::Down) {
      Vec2F const downhill = node.position + Vec2F(direction, -1);
      RectI const downhill_bounds = tile_bounds_at(downhill);
      auto valid = valid_position(downhill_bounds);
      if (!valid) return std::unexpected(std::move(valid.error()));
      if (*valid) {
        auto downhill_grounded = on_ground(downhill_bounds);
        if (!downhill_grounded) return std::unexpected(std::move(downhill_grounded.error()));
        if (*downhill_grounded) {
          add_node(Node{downhill, {}});
          return {};
        }
      }
    }

    if (!m_movement_params.standing_poly)
      return std::unexpected(MovementControllerException("PathFinder requires standingPoly for walking"));
    RectF const& raw_bounds = standing_bounds();
    float const back = direction > 0 ? raw_bounds.x_min() : raw_bounds.x_max();
    forward[0] -= (1 - std::fmod(std::abs(back), 1.0f)) * direction;
    if (m_movement_params.walk_speed)
      add_node(Node{forward, Vec2F{std::copysign(*m_movement_params.walk_speed, direction), 0.0f}});
    if (m_movement_params.run_speed)
      add_node(Node{forward, Vec2F{std::copysign(*m_movement_params.run_speed, direction), 0.0f}});
    return {};
  }

  std::expected<void, StarException> PathFinder::get_walking_neighbors(Node const& node, List<Edge>& neighbors) const {
    auto bounds = bound_box(node.position);
    if (!bounds)
      return std::unexpected(std::move(bounds.error()));
    Vec2F const right_foot(bounds->x_max(), bounds->y_min());
    Vec2F const left_foot(bounds->x_min(), bounds->y_min());
    float const foot_height = std::round(bounds->y_min());
    Slope right_slope = Slope::None;
    Slope left_slope = Slope::None;

    auto const& world = m_world.get();
    auto scanned = world.for_each_collision_block(
      ground_collision_rect(RectI::integral(*bounds)).padded(1),
      [&](CollisionBlock const& block) -> std::expected<void, StarException> {
        for (std::size_t i = 0; i < block.poly.sides(); ++i) {
          if (right_slope != Slope::None && left_slope != Slope::None)
            break;
          auto const side = block.poly.side(i);
          auto const side_dir = side.direction();
          if (side_dir[0] == 0 || side_dir[1] == 0)
            continue;
          auto const& lower = side.min()[1] < side.max()[1] ? side.min() : side.max();
          auto const& upper = side.min()[1] > side.max()[1] ? side.min() : side.max();
          if (lower[1] != foot_height && upper[1] != foot_height)
            continue;

          // Each direction locks in its own first match. The same foot is
          // used for uphill in one direction and downhill in the other.
          auto const match_slope = [&](Vec2F const& foot, Slope& uphill, Slope& downhill) {
            auto const& geometry = world.geometry();
            if (uphill == Slope::None && std::abs(geometry.diff(foot, lower)[0]) < 0.5f)
              uphill = Slope::Up;
            if (downhill == Slope::None && std::abs(geometry.diff(foot, upper)[0]) < 0.5f)
              downhill = Slope::Down;
          };
          float const rise = (side_dir[1] / side_dir[0]) * node_granularity;
          if (rise > 0)
            match_slope(right_foot, right_slope, left_slope);
          else if (rise < 0)
            match_slope(left_foot, left_slope, right_slope);
        }
        return {};
      });
    if (!scanned)
      return std::unexpected(std::move(scanned.error()));

    RectF const& local_bounds = m_local_bounds->full;
    auto right = get_walking_neighbors_in_direction(node, neighbors, local_bounds, node_granularity, right_slope);
    if (!right) return std::unexpected(std::move(right.error()));
    return get_walking_neighbors_in_direction(node, neighbors, local_bounds, -node_granularity, left_slope);
  }

  namespace {
    template <typename Callback>
    concept arc_velocity_callback = requires(Callback& callback, Vec2F velocity) {
      { callback(std::move(velocity)) } -> std::same_as<std::expected<void, StarException>>;
    };

    template <arc_velocity_callback Callback>
    std::expected<void, StarException> for_each_arc_velocity(float y_velocity, std::optional<float> walk_speed,
                                                            std::optional<float> run_speed, Callback&& callback) {
      auto vertical = callback(Vec2F(0, y_velocity));
      if (!vertical) return std::unexpected(std::move(vertical.error()));
      if (walk_speed) {
        auto right = callback(Vec2F(*walk_speed, y_velocity));
        if (!right) return std::unexpected(std::move(right.error()));
        auto left = callback(Vec2F(-*walk_speed, y_velocity));
        if (!left) return std::unexpected(std::move(left.error()));
      }
      if (run_speed) {
        auto right = callback(Vec2F(*run_speed, y_velocity));
        if (!right) return std::unexpected(std::move(right.error()));
        auto left = callback(Vec2F(-*run_speed, y_velocity));
        if (!left) return std::unexpected(std::move(left.error()));
      }
      return {};
    }
  }

  std::expected<void, StarException> PathFinder::get_falling_neighbors(Node const& node, List<Edge>& neighbors) const {
    auto position = round_to_node(node.position);
    if (!position) return std::unexpected(std::move(position.error()));
    return for_each_arc_velocity(
      0.0f, m_search_params.enable_walk_speed_jumps ? m_movement_params.walk_speed : std::nullopt,
      m_movement_params.run_speed, [&](Vec2F velocity) { return simulate_arc(node, *position, velocity, neighbors); });
  }

  std::expected<void, StarException>
  PathFinder::get_jumping_neighbors(Node const& node, List<Edge>& neighbors, float jump_cost) const {
    if (auto jump_speed = m_movement_params.air_jump_profile.jump_speed) {
      auto add_velocity = [jump_cost, &node, &neighbors](Vec2F velocity) -> std::expected<void, StarException> {
        neighbors.push_back(Edge{jump_cost, Action::Jump, velocity, node, node.with_velocity(velocity)});
        return {};
      };
      auto walk_speed = m_search_params.enable_walk_speed_jumps ? m_movement_params.walk_speed : std::nullopt;
      auto run_speed = m_movement_params.run_speed;
      auto full_jump = for_each_arc_velocity(*jump_speed, walk_speed, run_speed, add_velocity);
      if (!full_jump)
        return std::unexpected(std::move(full_jump.error()));
      return for_each_arc_velocity(
        *jump_speed * m_search_params.small_jump_multiplier.value_or(default_small_jump_multiplier),
        walk_speed, run_speed, add_velocity);
    }
    return {};
  }

  std::expected<void, StarException>
  PathFinder::get_swimming_neighbors(Node const& node, List<Edge>& neighbors, float source_liquid_level) const {
    // We assume when swimming that movement can be freely against gravity.
    auto flying = get_flying_neighbors(node, neighbors);
    if (!flying) return std::unexpected(std::move(flying.error()));
    auto acc = acceleration(node.position);
    if (!acc) return std::unexpected(std::move(acc.error()));
    if ((*acc)[1] != 0.0f && source_liquid_level < 1.0f) {
      auto jumping = get_jumping_neighbors(node, neighbors,
                                          m_search_params.liquid_jump_cost.value_or(default_liquid_jump_cost));
      if (!jumping) return std::unexpected(std::move(jumping.error()));
    }

    // Preserve generation order with a single write cursor; source-position
    // targets (including jumps) already passed the liquid check.
    float swim_cost = m_search_params.swim_cost.value_or(default_swim_cost);
    auto kept = neighbors.begin();
    for (auto it = neighbors.begin(); it != neighbors.end(); ++it) {
      if (it->target.position != node.position) {
        auto liquid = in_liquid(it->target.position);
        if (!liquid) return std::unexpected(std::move(liquid.error()));
        if (!*liquid) continue;
      }
      if (it->action == Action::Fly) it->action = Action::Swim;
      it->cost *= swim_cost;
      if (kept != it)
        *kept = std::move(*it);
      ++kept;
    }
    neighbors.erase(kept, neighbors.end());
    return {};
  }

  std::expected<void, StarException> PathFinder::get_flying_neighbors(Node const& node, List<Edge>& neighbors) const {
    auto rounded_position = round_to_node(node.position);
    if (!rounded_position)
      return std::unexpected(std::move(rounded_position.error()));
    for (int dx = -1; dx < 2; ++dx) {
      for (int dy = -1; dy < 2; ++dy) {
        Vec2F new_position = *rounded_position + Vec2F(dx, dy) * node_granularity;
        auto valid = valid_position(new_position);
        if (!valid) return std::unexpected(std::move(valid.error()));
        if (*valid)
          neighbors.push_back(default_cost_edge(Action::Fly, node, Node{new_position, {}}));
      }
    }
    return {};
  }

  std::expected<void, StarException> PathFinder::get_arc_neighbors(Node const& node, List<Edge>& neighbors) const {
    if (!node.velocity) return std::unexpected(MovementControllerException("PathFinder arc requires velocity"));
    return simulate_arc(node, node.position, *node.velocity, neighbors);
  }

  std::expected<Vec2F, StarException> PathFinder::acceleration(Vec2F pos) const {
    auto const& parameters = m_movement_params;
    auto world_gravity = m_world.get().gravity(pos);
    if (!world_gravity)
      return std::unexpected(std::move(world_gravity.error()));
    float gravity = *world_gravity * parameters.gravity_multiplier.value_or(1.0f);
    if (!parameters.gravity_enabled.value_or(true) || parameters.mass.value_or(0.0f) == 0.0f) gravity = 0.0f;
    float buoyancy = parameters.air_buoyancy.value_or(0.0f);
    return Vec2F(0, -gravity * (1.0f - buoyancy));
  }

  std::expected<PathFinder::ArcCollision, StarException>
  PathFinder::simulate_arc_collision(Vec2F position, Vec2F velocity, float dt) const {
    // Use tile bounds, rather than expensive full collision resolution.
    Vec2F new_position = position + velocity * dt;
    auto valid = valid_position(new_position);
    if (!valid) return std::unexpected(std::move(valid.error()));
    if (*valid)
      return ArcCollision{new_position, false, false};
    auto horizontal = valid_position(Vec2F(new_position[0], position[1]));
    if (!horizontal) return std::unexpected(std::move(horizontal.error()));
    if (*horizontal)
      return ArcCollision{Vec2F(new_position[0], position[1]), false, true};
    auto vertical = valid_position(Vec2F(position[0], new_position[1]), BoundBoxKind::Stand);
    if (!vertical) return std::unexpected(std::move(vertical.error()));
    if (*vertical)
      return ArcCollision{Vec2F(position[0], new_position[1]), true, false};
    return ArcCollision{position, true, true};
  }

  std::expected<void, StarException>
  PathFinder::simulate_arc(Node const& source, Vec2F position, Vec2F velocity, List<Edge>& neighbors) const {
    auto emit_arc = [&](Vec2F const& target_position, Vec2F const& target_velocity, bool landed = false) {
      Node target{target_position, target_velocity};
      neighbors.push_back(default_cost_edge(Action::Arc, source, target));
      if (landed)
        neighbors.push_back(default_cost_edge(Action::Land, target, Node{target_position, {}}));
    };

    bool initially_vertical = velocity[0] == 0.0f;
    bool jumping = velocity[1] > 0.0f;
    float max_landing_velocity = m_search_params.max_landing_velocity.value_or(default_max_landing_velocity);

    auto acc_result = acceleration(position);
    if (!acc_result)
      return std::unexpected(std::move(acc_result.error()));
    Vec2F acc = *acc_result;
    if (acc[1] == 0.0f)
      return {};

    // Simulate until the position rounds to a different node.
    auto start_result = round_to_node(position);
    if (!start_result)
      return std::unexpected(std::move(start_result.error()));
    Vec2F start = *start_result;
    Vec2F rounded = start;
    unsigned steps = 0;
    while (rounded == start) {
      if (steps == max_arc_steps)
        return std::unexpected(
          MovementControllerException::format("PathFinder arc simulation exceeded its {}-step limit", max_arc_steps));
      ++steps;

      Vec2F previous_position = position;
      Vec2F previous_velocity = velocity;
      bool was_jumping = jumping;
      float speed = velocity.magnitude();
      float dt = std::fmin(0.2f,
                           speed != 0.0f ? simulate_arc_granularity / speed
                                         : std::sqrt(simulate_arc_granularity * 2.0 * std::abs(acc[1])));

      auto simulated = simulate_arc_collision(position, velocity, dt);
      if (!simulated) return std::unexpected(std::move(simulated.error()));
      position = simulated->position;
      auto rounded_result = round_to_node(position);
      if (!rounded_result)
        return std::unexpected(std::move(rounded_result.error()));
      rounded = *rounded_result;

      if (simulated->collided_y) {
        if (!jumping) {
          // Landed.
          if (velocity[1] < max_landing_velocity)
            emit_arc(rounded, velocity, true);
          return {};
        }
        auto grounded = on_ground(rounded, BoundBoxKind::Stand);
        if (!grounded) return std::unexpected(std::move(grounded.error()));
        if (*grounded) {
          // Hit the ceiling and landed simultaneously. Allow tight gaps
          // without checking landing speed because tile polygons are rounded.
          emit_arc(rounded, velocity, true);
          return {};
        }
        // Hit the ceiling.
        velocity[1] = 0.0f;
      } else if (simulated->collided_x) {
        // Hit a wall, just fall down
        velocity[0] = 0.0f;
        if (jumping) {
          velocity[1] = 0.0f;
          jumping = false;
        }
      }

      velocity += acc * dt;
      if (jumping && velocity[1] <= 0.0f) {
        // We've reached a peak in the jump and the entity can now choose to
        // change direction.
        std::optional<float> run_speed = m_movement_params.run_speed;
        std::optional<float> walk_speed = m_movement_params.walk_speed;
        float crawl_multiplier = m_search_params.jump_drop_x_multiplier.value_or(default_jump_drop_x_multiplier);

        if (!initially_vertical || m_search_params.enable_vertical_jump_air_control) {
          if (run_speed)
            emit_arc(position, Vec2F(std::copysign(*run_speed, velocity[0]), 0.0f));
          if (m_search_params.enable_walk_speed_jumps && walk_speed) {
            emit_arc(position, Vec2F(std::copysign(*walk_speed, velocity[0]), 0.0f));
            emit_arc(position, Vec2F(std::copysign(*walk_speed * crawl_multiplier, velocity[0]), 0.0f));
          }
        }
        // Only fall straight down if we were going straight up originally.
        if (initially_vertical)
          emit_arc(position, Vec2F(0.0f, 0.0f));
        return {};
      }

      // The rounded node can stay the same while an arc is still gaining speed.
      // Only a fixed point of the full simulation state rules out future motion.
      if (position == previous_position && velocity == previous_velocity && jumping == was_jumping)
        return {};
    }

    if (!jumping) {
      if (velocity[1] < max_landing_velocity) {
        auto grounded = on_ground(rounded, BoundBoxKind::Stand);
        if (!grounded) return std::unexpected(std::move(grounded.error()));
        bool landed = *grounded;
        if (!landed) {
          auto liquid = in_liquid(rounded);
          if (!liquid) return std::unexpected(std::move(liquid.error()));
          landed = *liquid;
        }
        if (landed) {
          // Collision with platform
          emit_arc(rounded, velocity, true);
          return {};
        }
      }
    }

    if constexpr (Star::debug_enabled) {
      Star::star_assert_failed((velocity[1] != 0.0f));
    }
    emit_arc(position, velocity);
    return {};
  }

  std::expected<bool, StarException> PathFinder::valid_position(Vec2F pos, BoundBoxKind bound_kind) const {
    auto bounds = bound_box(pos, bound_kind);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    return valid_position(RectI::integral(*bounds));
  }

  std::expected<bool, StarException> PathFinder::valid_position(RectI const& tile_bounds) const {
    auto collision = m_world.get().rectTileCollision(tile_bounds, collision_solid);
    if (!collision) return std::unexpected(std::move(collision.error()));
    return !*collision;
  }

  std::expected<bool, StarException> PathFinder::on_ground(Vec2F pos, BoundBoxKind bound_kind) const {
    auto bounds = bound_box(pos, bound_kind);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    return on_ground(RectI::integral(*bounds));
  }

  std::expected<bool, StarException> PathFinder::on_ground(RectI const& tile_bounds) const {
    RectI const ground_rect = ground_collision_rect(tile_bounds);
    auto inside_object = m_world.get().rectTileCollision(tile_bounds, collision_dynamic);
    if (!inside_object)
      return std::unexpected(std::move(inside_object.error()));
    if (*inside_object)
      return m_world.get().rectTileCollision(ground_rect, collision_floor_only);
    auto grounded = m_world.get().rectTileCollision(ground_rect, collision_any);
    if (!grounded) return std::unexpected(std::move(grounded.error()));
    if (*grounded) return true;
    return m_world.get().rectTileCollision(ground_rect.translated(Vec2I(0, 1)), collision_solid);
  }

  std::expected<bool, StarException> PathFinder::on_solid_ground(Vec2F pos) const {
    auto bounds = bound_box(pos, BoundBoxKind::Drop);
    if (!bounds)
      return std::unexpected(std::move(bounds.error()));
    return m_world.get().rectTileCollision(ground_collision_rect(RectI::integral(*bounds)), collision_solid);
  }

  std::expected<float, StarException> PathFinder::liquid_level(Vec2F pos) const {
    auto bounds = bound_box(pos);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    auto liquid = m_world.get().liquidLevel(*bounds);
    if (!liquid) return std::unexpected(std::move(liquid.error()));
    return liquid->level;
  }

  bool PathFinder::in_liquid(float level) const {
    return level >= m_movement_params.minimum_liquid_percentage.value_or(0.5f);
  }

  std::expected<bool, StarException> PathFinder::in_liquid(Vec2F pos) const {
    auto level = liquid_level(pos);
    if (!level) return std::unexpected(std::move(level.error()));
    return in_liquid(*level);
  }

  std::expected<RectF, StarException> PathFinder::bound_box(Vec2F pos, BoundBoxKind bound_kind) const {
    if (!m_local_bounds)
      return std::unexpected(MovementControllerException("PathFinder requires standingPoly or boundBox"));
    if (bound_kind == BoundBoxKind::Drop)
      return m_local_bounds->drop.translated(pos);
    if (bound_kind == BoundBoxKind::Stand)
      return m_local_bounds->stand.translated(pos);
    return m_local_bounds->full.translated(pos);
  }

  RectI PathFinder::ground_collision_rect(RectI const& tile_bounds) {
    return RectI(Vec2I(tile_bounds.x_min(), tile_bounds.y_min() - 1), Vec2I(tile_bounds.x_max(), tile_bounds.y_min()));
  }

  RectF const& PathFinder::standing_bounds() const {
    if (!m_standing_bounds)
      m_standing_bounds = m_movement_params.standing_poly->bounding_box();
    return *m_standing_bounds;
  }

  std::expected<Vec2F, StarException> PathFinder::round_to_node(Vec2F pos) const {
    if (!m_local_bounds)
      return std::unexpected(MovementControllerException("PathFinder requires standingPoly or boundBox"));
    // Round Y relative to the unscaled feet so exact-height gaps retain their nodes.
    float const foot_offset = m_local_bounds->foot_offset;
    float const x = std::round(pos[0] / node_granularity) * node_granularity;
    float const y = std::round((pos[1] + foot_offset) / node_granularity) * node_granularity - foot_offset;
    return Vec2F(x, y);
  }

  float PathFinder::distance(Vec2F a, Vec2F b) const {
    return m_world.get().geometry().diff(a, b).magnitude();
  }
}

}
