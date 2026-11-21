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
      m_movement_params(std::move(movement_parameters)), m_search_params(std::move(search_parameters)) {}

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
      result.filter([&self](Edge const& edge) {
        return self.distance(edge.source.position, self.m_search_from)
          <= self.m_search_params.max_distance.value_or(default_max_distance);
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
    auto liquid = in_liquid(node.position);
    if (!liquid) return std::unexpected(std::move(liquid.error()));
    if (*liquid)
      return get_swimming_neighbors(node, neighbors);
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
      auto drop = get_drop_neighbors(node, neighbors);
      if (!drop) return std::unexpected(std::move(drop.error()));
    }
    return get_jumping_neighbors(node, neighbors);
  }

  std::expected<void, StarException> PathFinder::get_drop_neighbors(Node const& node, List<Edge>& neighbors) const {
    auto drop_position = node.position + Vec2F(0, -1);
    // Platforms resting directly on solid surfaces cannot be dropped through.
    auto grounded = on_solid_ground(drop_position);
    if (!grounded) return std::unexpected(std::move(grounded.error()));
    if (!*grounded) {
      float drop_cost = m_search_params.drop_cost.value_or(default_drop_cost);
      auto acceleration_value = acceleration(node.position);
      if (!acceleration_value)
        return std::unexpected(std::move(acceleration_value.error()));
      float acc = (*acceleration_value)[1];
      float drop_speed = acc * std::sqrt(2.0 / std::abs(acc));
      neighbors.push_back(Edge{drop_cost, Action::Drop, Vec2F(0, 0), node, Node{drop_position, Vec2F(0, drop_speed)}});
    }
    return {};
  }

  std::expected<void, StarException>
  PathFinder::get_walking_neighbors_in_direction(Node const& node, List<Edge>& neighbors, float direction) const {
    auto add_node = [&self = *this, &node, &neighbors](Node const& target) {
      neighbors.push_back(self.default_cost_edge(Action::Walk, node, target));
    };

    Vec2F forward = node.position + Vec2F(direction, 0);
    Vec2F forward_and_up = node.position + Vec2F(direction, 1);
    Vec2F forward_and_down = node.position + Vec2F(direction, -1);

    auto bounds_result = bound_box(node.position);
    if (!bounds_result)
      return std::unexpected(std::move(bounds_result.error()));
    RectF bounds = *bounds_result;

    bool slope_down = false;
    bool slope_up = false;
    Vec2F forward_ground_pos =
      direction > 0 ? Vec2F(bounds.x_max(), bounds.y_min()) : Vec2F(bounds.x_min(), bounds.y_min());
    Vec2F back_ground_pos = direction < 0 ? Vec2F(bounds.x_max(), bounds.y_min()) : Vec2F(bounds.x_min(), bounds.y_min());
    RectI tile_bounds = RectI::integral(bounds);
    RectI ground_rect(Vec2I(tile_bounds.x_min(), tile_bounds.y_min() - 1), Vec2I(tile_bounds.x_max(), tile_bounds.y_min()));
    auto collision_scan = m_world.get().forEachCollisionBlock(
      ground_rect.padded(1),
      [&, &world = m_world.get()](CollisionBlock const& block) -> std::expected<void, StarException> {
        if (slope_up || slope_down)
          return {};
        for (std::size_t i = 0; i < block.poly.sides(); ++i) {
          auto side = block.poly.side(i);
          auto side_dir = side.direction();

          auto lower = side.min()[1] < side.max()[1] ? side.min() : side.max();
          auto upper = side.min()[1] > side.max()[1] ? side.min() : side.max();
          if (side_dir[0] != 0 && side_dir[1] != 0
              && (lower[1] == std::round(forward_ground_pos[1]) || upper[1] == std::round(forward_ground_pos[1]))) {
            float y_dir = (side_dir[1] / side_dir[0]) * direction;
            if (std::abs(world.geometry().diff(forward_ground_pos, lower)[0]) < 0.5 && y_dir > 0)
              slope_up = true;
            else if (std::abs(world.geometry().diff(back_ground_pos, upper)[0]) < 0.5 && y_dir < 0)
              slope_down = true;

            if (slope_up || slope_down)
              break;
          }
        }
        return {};
      });
    if (!collision_scan)
      return std::unexpected(std::move(collision_scan.error()));

    std::optional<float> walk_speed = m_movement_params.walkSpeed;
    std::optional<float> run_speed = m_movement_params.runSpeed;

    // Try walking up the ramp first, preserving the original predicate order.
    if (slope_up) {
      auto grounded = on_ground(forward_and_up);
      if (!grounded) return std::unexpected(std::move(grounded.error()));
      if (*grounded) {
        auto valid = valid_position(forward_and_up);
        if (!valid) return std::unexpected(std::move(valid.error()));
        if (*valid) {
          add_node(Node{forward_and_up, {}});
          return {};
        }
      }
    }
    auto forward_valid = valid_position(forward);
    if (!forward_valid)
      return std::unexpected(std::move(forward_valid.error()));
    if (*forward_valid) {
      auto grounded = on_ground(forward);
      if (!grounded) return std::unexpected(std::move(grounded.error()));
      if (*grounded) {
        add_node(Node{forward, {}});
        return {};
      }
    }
    if (slope_down) {
      if (*forward_valid) {
        auto down_valid = valid_position(forward_and_down);
        if (!down_valid)
          return std::unexpected(std::move(down_valid.error()));
        if (*down_valid) {
          auto grounded = on_ground(forward_and_down);
          if (!grounded) return std::unexpected(std::move(grounded.error()));
          if (*grounded) {
            add_node(Node{forward_and_down, {}});
            return {};
          }
        }
      }
    }
    if (*forward_valid) {
      if (!m_movement_params.standingPoly)
        return std::unexpected(MovementControllerException("PathFinder requires standingPoly for walking"));
      bounds = standing_bounds();
      float back = direction > 0 ? bounds.x_min() : bounds.x_max();
      forward[0] -= (1 - std::fmod(std::abs(back), 1.0f)) * direction;
      if (walk_speed)
        add_node(Node{forward, Vec2F{std::copysign(*walk_speed, direction), 0.0f}});
      if (run_speed)
        add_node(Node{forward, Vec2F{std::copysign(*run_speed, direction), 0.0f}});
    }
    return {};
  }

  std::expected<void, StarException> PathFinder::get_walking_neighbors(Node const& node, List<Edge>& neighbors) const {
    auto right = get_walking_neighbors_in_direction(node, neighbors, node_granularity);
    if (!right) return std::unexpected(std::move(right.error()));
    return get_walking_neighbors_in_direction(node, neighbors, -node_granularity);
  }

  std::expected<void, StarException> PathFinder::get_falling_neighbors(Node const& node, List<Edge>& neighbors) const {
    return for_each_arc_neighbor(
      node, 0.0f,
      [&self = *this, &node, &neighbors](Node const& target, bool landed) -> std::expected<void, StarException> {
        neighbors.push_back(self.default_cost_edge(Action::Arc, node, target));
        if (landed)
          neighbors.push_back(self.default_cost_edge(Action::Land, target, Node{target.position, {}}));
        return {};
      });
  }

  std::expected<void, StarException> PathFinder::get_jumping_neighbors(Node const& node, List<Edge>& neighbors) const {
    if (auto jump_speed = m_movement_params.airJumpProfile.jumpSpeed) {
      float jump_cost = m_search_params.jump_cost.value_or(default_jump_cost);
      auto liquid = in_liquid(node.position);
      if (!liquid) return std::unexpected(std::move(liquid.error()));
      if (*liquid)
        jump_cost = m_search_params.liquid_jump_cost.value_or(default_liquid_jump_cost);
      auto add_vel = [jump_cost, &node, &neighbors](Vec2F const& vel) -> std::expected<void, StarException> {
        neighbors.push_back(Edge{jump_cost, Action::Jump, vel, node, node.with_velocity(vel)});
        return {};
      };
      auto full_jump = for_each_arc_velocity(*jump_speed, add_vel);
      if (!full_jump)
        return std::unexpected(std::move(full_jump.error()));
      return for_each_arc_velocity(
        *jump_speed * m_search_params.small_jump_multiplier.value_or(default_small_jump_multiplier), add_vel);
    }
    return {};
  }

  std::expected<void, StarException> PathFinder::get_swimming_neighbors(Node const& node, List<Edge>& neighbors) const {
    // We assume when swimming that movement can be freely against gravity.
    auto flying = get_flying_neighbors(node, neighbors);
    if (!flying) return std::unexpected(std::move(flying.error()));
    auto bounds = bound_box(node.position);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    auto acc = acceleration(node.position);
    if (!acc) return std::unexpected(std::move(acc.error()));
    if ((*acc)[1] != 0.0f) {
      auto liquid = m_world.get().liquidLevel(*bounds);
      if (!liquid) return std::unexpected(std::move(liquid.error()));
      if (liquid->level < 1.0f) {
        auto jumping = get_jumping_neighbors(node, neighbors);
        if (!jumping) return std::unexpected(std::move(jumping.error()));
      }
    }
    // Filtering is stable and fallible predicates are checked before each erase.
    for (auto it = neighbors.begin(); it != neighbors.end();) {
      auto liquid = in_liquid(it->target.position);
      if (!liquid) return std::unexpected(std::move(liquid.error()));
      if (!*liquid) it = neighbors.erase(it);
      else ++it;
    }
    for (auto& edge : neighbors) {
      if (edge.action == Action::Fly) edge.action = Action::Swim;
      edge.cost *= m_search_params.swim_cost.value_or(default_swim_cost);
    }
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
    return simulate_arc(
      node, [&self = *this, &node, &neighbors](Node const& target, bool landed) -> std::expected<void, StarException> {
        neighbors.push_back(self.default_cost_edge(Action::Arc, node, target));
        if (landed)
          neighbors.push_back(self.default_cost_edge(Action::Land, target, Node{target.position, {}}));
        return {};
      });
  }

  std::expected<void, StarException>
  PathFinder::for_each_arc_velocity(float y_velocity,
                                    std::function<std::expected<void, StarException>(Vec2F const&)> const& func) const {
    auto walk_speed = m_movement_params.walkSpeed;
    auto run_speed = m_movement_params.runSpeed;
    auto vertical = func(Vec2F(0, y_velocity));
    if (!vertical) return std::unexpected(std::move(vertical.error()));
    if (m_search_params.enable_walk_speed_jumps && walk_speed) {
      auto right = func(Vec2F(*walk_speed, y_velocity));
      if (!right) return std::unexpected(std::move(right.error()));
      auto left = func(Vec2F(-*walk_speed, y_velocity));
      if (!left) return std::unexpected(std::move(left.error()));
    }
    if (run_speed) {
      auto right = func(Vec2F(*run_speed, y_velocity));
      if (!right) return std::unexpected(std::move(right.error()));
      auto left = func(Vec2F(-*run_speed, y_velocity));
      if (!left) return std::unexpected(std::move(left.error()));
    }
    return {};
  }

  std::expected<void, StarException> PathFinder::for_each_arc_neighbor(
    Node const& node, float y_velocity,
    std::function<std::expected<void, StarException>(Node const&, bool)> const& func) const {
    auto position = round_to_node(node.position);
    if (!position) return std::unexpected(std::move(position.error()));
    return for_each_arc_velocity(
      y_velocity, [&self = *this, &position, &func](Vec2F const& vel) -> std::expected<void, StarException> {
        return self.simulate_arc(Node{*position, vel}, func);
      });
  }

  std::expected<Vec2F, StarException> PathFinder::acceleration(Vec2F pos) const {
    auto const& parameters = m_movement_params;
    auto world_gravity = m_world.get().gravity(pos);
    if (!world_gravity)
      return std::unexpected(std::move(world_gravity.error()));
    float gravity = *world_gravity * parameters.gravityMultiplier.value_or(1.0f);
    if (!parameters.gravityEnabled.value_or(true) || parameters.mass.value_or(0.0f) == 0.0f) gravity = 0.0f;
    float buoyancy = parameters.airBuoyancy.value_or(0.0f);
    return Vec2F(0, -gravity * (1.0f - buoyancy));
  }

  std::expected<Vec2F, StarException> PathFinder::simulate_arc_collision(Vec2F position, Vec2F velocity, float dt,
                                                                         bool& collided_x, bool& collided_y) const {
    // Use tile bounds, rather than expensive full collision resolution.
    Vec2F new_position = position + velocity * dt;
    auto valid = valid_position(new_position);
    if (!valid) return std::unexpected(std::move(valid.error()));
    if (*valid) {
      collided_x = collided_y = false;
      return new_position;
    }
    collided_x = collided_y = true;
    auto horizontal = valid_position(Vec2F(new_position[0], position[1]));
    if (!horizontal) return std::unexpected(std::move(horizontal.error()));
    if (*horizontal) {
      collided_x = false;
      position[0] = new_position[0];
    } else {
      auto vertical = valid_position(Vec2F(position[0], new_position[1]), BoundBoxKind::Stand);
      if (!vertical) return std::unexpected(std::move(vertical.error()));
      if (*vertical) {
        collided_y = false;
        position[1] = new_position[1];
      }
    }
    return position;
  }

  std::expected<void, StarException>
  PathFinder::simulate_arc(Node const& node,
                           std::function<std::expected<void, StarException>(Node const&, bool)> const& func) const {
    Vec2F position = node.position;
    if (!node.velocity) return std::unexpected(MovementControllerException("PathFinder arc requires velocity"));
    Vec2F velocity = *node.velocity;
    bool jumping = velocity[1] > 0.0f;
    float max_landing_velocity = m_search_params.max_landing_velocity.value_or(default_max_landing_velocity);

    auto acc_result = acceleration(position);
    if (!acc_result)
      return std::unexpected(std::move(acc_result.error()));
    Vec2F acc = *acc_result;
    if (acc[1] == 0.0f)
      return {};

    // Simulate until we're roughly node_granularity distance from the previous
    // node
    auto start_result = round_to_node(node.position);
    if (!start_result)
      return std::unexpected(std::move(start_result.error()));
    Vec2F start = *start_result;
    Vec2F rounded = start;
    while (rounded == start) {
      float speed = velocity.magnitude();
      float dt = std::fmin(0.2f,
                           speed != 0.0f ? simulate_arc_granularity / speed
                                         : std::sqrt(simulate_arc_granularity * 2.0 * std::abs(acc[1])));

      bool collided_x = false, collided_y = false;
      auto simulated = simulate_arc_collision(position, velocity, dt, collided_x, collided_y);
      if (!simulated) return std::unexpected(std::move(simulated.error()));
      position = *simulated;
      auto rounded_result = round_to_node(position);
      if (!rounded_result)
        return std::unexpected(std::move(rounded_result.error()));
      rounded = *rounded_result;

      if (collided_y) {
        // We've either landed or hit our head on the ceiling
        if (!jumping) {
          // Landed
          if (velocity[1] < max_landing_velocity)
            if (auto result = func(Node{rounded, velocity}, true); !result) return std::unexpected(std::move(result.error()));
          return {};
        } else {
          auto grounded = on_ground(rounded, BoundBoxKind::Stand);
          if (!grounded) return std::unexpected(std::move(grounded.error()));
          if (*grounded) {
          // Simultaneously hit head and landed -- this is a gap we can *just*
          // fit
          // through. No checking of the maxLandingVelocity, since the tiles'
          // polygons are rounded, making this an easier target to hit than it
          // seems.
          if (auto result = func(Node{rounded, velocity}, true); !result) return std::unexpected(std::move(result.error()));
          return {};
          }
        }
        // Hit ceiling. Remove y velocity
        velocity[1] = 0.0f;

      } else if (collided_x) {
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
        std::optional<float> run_speed = m_movement_params.runSpeed;
        std::optional<float> walk_speed = m_movement_params.walkSpeed;
        float crawl_multiplier = m_search_params.jump_drop_x_multiplier.value_or(default_jump_drop_x_multiplier);

        if ((*node.velocity)[0] != 0.0f || m_search_params.enable_vertical_jump_air_control) {
          if (run_speed.has_value())
            if (auto result = func(Node{position, Vec2F{std::copysign(*run_speed, velocity[0]), 0.0f}}, false); !result)
              return std::unexpected(std::move(result.error()));
          if (m_search_params.enable_walk_speed_jumps && walk_speed.has_value()) {
            if (auto result = func(Node{position, Vec2F{std::copysign(*walk_speed, velocity[0]), 0.0f}}, false);
                !result)
              return std::unexpected(std::move(result.error()));
            if (auto result =
                  func(Node{position, Vec2F{std::copysign(*walk_speed * crawl_multiplier, velocity[0]), 0.0f}}, false);
                !result)
              return std::unexpected(std::move(result.error()));
          }
        }
        // Only fall straight down if we were going straight up originally.
        // Going from an arc to falling straight down looks unnatural.
        if ((*node.velocity)[0] == 0.0f) {
          if (auto result = func(Node{position, Vec2F(0.0f, 0.0f)}, false); !result) return std::unexpected(std::move(result.error()));
        }
        return {};
      }
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
          if (auto result = func(Node{rounded, velocity}, true); !result) return std::unexpected(std::move(result.error()));
          return {};
        }
      }
    }

    if constexpr (Star::DebugEnabled) {
      Star::star_assert_failed((velocity[1] != 0.0f));
    }
    if (auto result = func(Node{position, velocity}, false); !result) return std::unexpected(std::move(result.error()));
    return {};
  }

  std::expected<bool, StarException> PathFinder::valid_position(Vec2F pos, BoundBoxKind bound_kind) const {
    auto bounds = bound_box(pos, bound_kind);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    auto collision = m_world.get().rectTileCollision(RectI::integral(*bounds), collision_solid);
    if (!collision) return std::unexpected(std::move(collision.error()));
    return !*collision;
  }

  std::expected<bool, StarException> PathFinder::on_ground(Vec2F pos, BoundBoxKind bound_kind) const {
    auto bounds = bound_box(pos, bound_kind);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    RectI integral_bounds = RectI::integral(*bounds);
    RectI ground_rect(Vec2I(integral_bounds.x_min(), integral_bounds.y_min() - 1),
                      Vec2I(integral_bounds.x_max(), integral_bounds.y_min()));
    auto inside_object = m_world.get().rectTileCollision(integral_bounds, collision_dynamic);
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
    auto ground_rect = ground_collision_rect(pos, BoundBoxKind::Drop);
    if (!ground_rect)
      return std::unexpected(std::move(ground_rect.error()));
    return m_world.get().rectTileCollision(*ground_rect, collision_solid);
  }

  std::expected<bool, StarException> PathFinder::in_liquid(Vec2F pos) const {
    auto bounds = bound_box(pos);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    auto liquid = m_world.get().liquidLevel(*bounds);
    if (!liquid) return std::unexpected(std::move(liquid.error()));
    return liquid->level >= m_movement_params.minimumLiquidPercentage.value_or(0.5f);
  }

  std::expected<RectF, StarException> PathFinder::bound_box(Vec2F pos, BoundBoxKind bound_kind) const {
    RectF bounds;
    if (bound_kind == BoundBoxKind::Drop && m_search_params.dropping_bound_box)
      bounds = *m_search_params.dropping_bound_box;
    else if (bound_kind == BoundBoxKind::Stand && m_search_params.standing_bound_box)
      bounds = *m_search_params.standing_bound_box;
    else if (m_search_params.bound_box)
      bounds = *m_search_params.bound_box;
    else {
      if (!m_movement_params.standingPoly)
        return std::unexpected(MovementControllerException("PathFinder requires standingPoly or boundBox"));
      bounds = standing_bounds();
    }
    bounds.scale(bound_box_rounding_error_scaling);
    bounds.translate(pos);
    return bounds;
  }

  std::expected<RectI, StarException> PathFinder::ground_collision_rect(Vec2F pos, BoundBoxKind bound_kind) const {
    auto box = bound_box(pos, bound_kind);
    if (!box) return std::unexpected(std::move(box.error()));
    RectI bounds = RectI::integral(*box);
    return RectI(Vec2I(bounds.x_min(), bounds.y_min() - 1), Vec2I(bounds.x_max(), bounds.y_min()));
  }

  RectF PathFinder::standing_bounds() const {
    if (!m_standing_bounds)
      m_standing_bounds = m_movement_params.standingPoly->bounding_box();
    return *m_standing_bounds;
  }

  std::expected<Vec2F, StarException> PathFinder::round_to_node(Vec2F pos) const {
    // Round Y relative to the feet so exact-height gaps retain their nodes.
    RectF bounds;
    if (m_search_params.bound_box)
      bounds = *m_search_params.bound_box;
    else {
      if (!m_movement_params.standingPoly)
        return std::unexpected(MovementControllerException("PathFinder requires standingPoly or boundBox"));
      bounds = standing_bounds();
    }
    float bottom = bounds.y_min();
    float x = std::round(pos[0] / node_granularity) * node_granularity;
    float y = std::round((pos[1] + bottom) / node_granularity) * node_granularity - bottom;
    return Vec2F(x, y);
  }

  float PathFinder::distance(Vec2F a, Vec2F b) const {
    return m_world.get().geometry().diff(a, b).magnitude();
  }
}

}
