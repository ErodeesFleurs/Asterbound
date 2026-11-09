import std;
import star.format;
import star.json;
import star.json_extra;
import star.id_map;
import star.logging;
import star.random;
import star.bi_map;
import star.a_star;
import star.casting;
import star.data_stream;
import star.strong_typedef;
import star.list;
import star.rpc_promise;
import star.game_types;
import star.vector;
import star.rect;
import star.math_common;
import star.lua_root;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;

import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.liquid_types;

import star.platformer_astar;

namespace Star {

namespace PlatformerAStar {

  // The desired spacing between nodes:
  float const NodeGranularity = 1.f;
  float const SimulateArcGranularity = 0.5f;

  float const DefaultMaxDistance = 50.0f;
  float const DefaultSmallJumpMultiplier = 0.75f;
  float const DefaultJumpDropXMultiplier = 0.125f;

  float const DefaultSwimCost = 40.0f;
  float const DefaultJumpCost = 3.0f;
  float const DefaultLiquidJumpCost = 10.0f;
  float const DefaultDropCost = 3.0f;

  float const DefaultMaxLandingVelocity = -5.0f;

  // Bounding boxes are shrunk slightly to work around floating point rounding
  // errors.
  float const BoundBoxRoundingErrorScaling = 0.99f;

  CollisionSet const CollisionSolid{CollisionKind::Null, CollisionKind::Slippery, CollisionKind::Block, CollisionKind::Slippery};

  CollisionSet const CollisionFloorOnly{CollisionKind::Null, CollisionKind::Block, CollisionKind::Slippery, CollisionKind::Platform};

  CollisionSet const CollisionDynamic{CollisionKind::Dynamic};

  CollisionSet const CollisionAny{CollisionKind::Null,
      CollisionKind::Platform,
      CollisionKind::Dynamic,
      CollisionKind::Slippery,
      CollisionKind::Block};

  PathFinder::PathFinder(World& world, Vec2F searchFrom, Vec2F searchTo, ActorMovementParameters movementParameters, Parameters searchParameters)
    : m_world(world), m_searchFrom(searchFrom), m_searchTo(searchTo), m_movementParams(std::move(movementParameters)), m_searchParams(std::move(searchParameters)) {}

  std::expected<std::shared_ptr<PathFinder>, StarException> PathFinder::create(World& world, Vec2F searchFrom, Vec2F searchTo, ActorMovementParameters movementParameters, Parameters searchParameters) {
    auto finder = std::shared_ptr<PathFinder>(new PathFinder(world, searchFrom, searchTo, std::move(movementParameters), std::move(searchParameters)));
    auto initialized = finder->initAStar();
    if (!initialized) return std::unexpected(std::move(initialized.error()));
    return finder;
  }

  std::expected<std::optional<bool>, StarException> PathFinder::explore(std::optional<unsigned> maxExploreNodes) {
    return (*m_astar).explore(maxExploreNodes);
  }

  std::optional<Path> const& PathFinder::result() const {
    return (*m_astar).result();
  }

  std::expected<void, StarException> PathFinder::initAStar() {
    auto heuristicCostFn = [&self = *this](Node const& fromNode, Node const& toNode) -> std::expected<double, StarException> {
      return self.heuristicCost(fromNode.position, toNode.position);
    };
    auto goalReachedFn = [&self = *this](Node const& node) -> std::expected<bool, StarException> {
      if (self.m_searchParams.mustEndOnGround) {
        auto grounded = self.onGround(node.position);
        if (!grounded) return std::unexpected(std::move(grounded.error()));
        if (!*grounded || node.velocity.has_value()) return false;
      }
      return self.distance(node.position, self.m_searchTo) < NodeGranularity;
    };
    auto neighborsFn = [&self = *this](Node const& node, List<Edge>& result) -> std::expected<void, StarException> {
      auto generated = self.neighbors(node, result);
      if (!generated) return std::unexpected(std::move(generated.error()));
      result.filter([&self](Edge const& edge) {
        return self.distance(edge.source.position, self.m_searchFrom) <= self.m_searchParams.maxDistance.value_or(DefaultMaxDistance);
      });
      return {};
    };
    auto validateEndFn = [&self = *this](Edge const& edge) -> std::expected<bool, StarException> {
      if (!self.m_searchParams.mustEndOnGround) return true;
      auto grounded = self.onGround(edge.target.position);
      if (!grounded) return std::unexpected(std::move(grounded.error()));
      return *grounded && edge.action != Action::Jump;
    };
    auto roundedFrom = roundToNode(m_searchFrom);
    if (!roundedFrom) return std::unexpected(std::move(roundedFrom.error()));
    auto roundedTo = roundToNode(m_searchTo);
    if (!roundedTo) return std::unexpected(std::move(roundedTo.error()));
    m_astar.emplace(heuristicCostFn, neighborsFn, goalReachedFn, m_searchParams.returnBest, std::optional<AStar::Search<Edge, Node>::ValidateEndFunction>{validateEndFn}, m_searchParams.maxFScore, m_searchParams.maxNodesToSearch);
    return m_astar->start(Node{*roundedFrom, {}}, Node{*roundedTo, {}});
  }

  float PathFinder::heuristicCost(Vec2F const& fromPosition, Vec2F const& toPosition) const {
    // This function is used to estimate the cost of travel between two nodes.
    // Underestimating the actual cost results in A* giving the optimal path.
    // Overestimating results in A* finding a non-optimal path, but terminating
    // more quickly when there is a route to the target.
    // We don't really care all that much about getting the optimal path as long
    // as we get one that looks feasible, so we deliberately overestimate here.
    Vec2F diff = m_world.get().geometry().diff(fromPosition, toPosition);
    // Manhattan distance * 2:
    return 2.0f * (std::abs(diff[0]) + std::abs(diff[1]));
  }

  Edge PathFinder::defaultCostEdge(Action action, Node const& source, Node const& target) const {
    return Edge{distance(source.position, target.position), action, Vec2F(0, 0), source, target};
  }

  std::expected<void, StarException> PathFinder::neighbors(Node const& node, List<Edge>& neighbors) const {
    if (node.velocity) return getArcNeighbors(node, neighbors);
    auto liquid = inLiquid(node.position);
    if (!liquid) return std::unexpected(std::move(liquid.error()));
    if (*liquid) return getSwimmingNeighbors(node, neighbors);
    auto acc = acceleration(node.position);
    if (!acc) return std::unexpected(std::move(acc.error()));
    if ((*acc)[1] == 0.0f) return getFlyingNeighbors(node, neighbors);
    auto grounded = onGround(node.position);
    if (!grounded) return std::unexpected(std::move(grounded.error()));
    if (!*grounded) return getFallingNeighbors(node, neighbors);
    auto walking = getWalkingNeighbors(node, neighbors);
    if (!walking) return std::unexpected(std::move(walking.error()));
    auto solidGround = onSolidGround(node.position);
    if (!solidGround) return std::unexpected(std::move(solidGround.error()));
    if (!*solidGround) {
      auto drop = getDropNeighbors(node, neighbors);
      if (!drop) return std::unexpected(std::move(drop.error()));
    }
    return getJumpingNeighbors(node, neighbors);
  }

  std::expected<void, StarException> PathFinder::getDropNeighbors(Node const& node, List<Edge>& neighbors) const {
    auto dropPosition = node.position + Vec2F(0, -1);
    // Platforms resting directly on solid surfaces cannot be dropped through.
    auto grounded = onSolidGround(dropPosition);
    if (!grounded) return std::unexpected(std::move(grounded.error()));
    if (!*grounded) {
      float dropCost = m_searchParams.dropCost.value_or(DefaultDropCost);
      auto accelerationValue = acceleration(node.position);
      if (!accelerationValue) return std::unexpected(std::move(accelerationValue.error()));
      float acc = (*accelerationValue)[1];
      float dropSpeed = acc * std::sqrt(2.0 / std::abs(acc));
      neighbors.push_back(Edge{dropCost, Action::Drop, Vec2F(0, 0), node, Node{dropPosition, Vec2F(0, dropSpeed)}});
    }
    return {};
  }

  std::expected<void, StarException> PathFinder::getWalkingNeighborsInDirection(Node const& node, List<Edge>& neighbors, float direction) const {
    auto addNode = [&self = *this, &node, &neighbors](Node const& target) {
      neighbors.push_back(self.defaultCostEdge(Action::Walk, node, target));
    };

    Vec2F forward = node.position + Vec2F(direction, 0);
    Vec2F forwardAndUp = node.position + Vec2F(direction, 1);
    Vec2F forwardAndDown = node.position + Vec2F(direction, -1);

    auto boundsResult = boundBox(node.position);
    if (!boundsResult) return std::unexpected(std::move(boundsResult.error()));
    RectF bounds = *boundsResult;

    bool slopeDown = false;
    bool slopeUp = false;
    Vec2F forwardGroundPos = direction > 0 ? Vec2F(bounds.xMax(), bounds.yMin()) : Vec2F(bounds.xMin(), bounds.yMin());
    Vec2F backGroundPos = direction < 0 ? Vec2F(bounds.xMax(), bounds.yMin()) : Vec2F(bounds.xMin(), bounds.yMin());
    auto groundRect = groundCollisionRect(node.position, BoundBoxKind::Full);
    if (!groundRect) return std::unexpected(std::move(groundRect.error()));
    auto collisionScan = m_world.get().forEachCollisionBlock(groundRect->padded(1), [&, &world = m_world.get()](CollisionBlock const& block) -> std::expected<void, StarException> {
      if (slopeUp || slopeDown) return {};
      for (std::size_t i = 0; i < block.poly.sides(); ++i) {
        auto side = block.poly.side(i);
        auto sideDir = side.direction();

        auto lower = side.min()[1] < side.max()[1] ? side.min() : side.max();
        auto upper = side.min()[1] > side.max()[1] ? side.min() : side.max();
        if (sideDir[0] != 0 && sideDir[1] != 0 && (lower[1] == std::round(forwardGroundPos[1]) || upper[1] == std::round(forwardGroundPos[1]))) {
          float yDir = (sideDir[1] / sideDir[0]) * direction;
          if (std::abs(world.geometry().diff(forwardGroundPos, lower)[0]) < 0.5 && yDir > 0)
            slopeUp = true;
          else if (std::abs(world.geometry().diff(backGroundPos, upper)[0]) < 0.5 && yDir < 0)
            slopeDown = true;

          if (slopeUp || slopeDown) break;
        }
      }
      return {};
    });
    if (!collisionScan) return std::unexpected(std::move(collisionScan.error()));

    std::optional<float> walkSpeed = m_movementParams.walkSpeed;
    std::optional<float> runSpeed = m_movementParams.runSpeed;

    // Try walking up the ramp first, preserving the original predicate order.
    if (slopeUp) {
      auto grounded = onGround(forwardAndUp);
      if (!grounded) return std::unexpected(std::move(grounded.error()));
      if (*grounded) {
        auto valid = validPosition(forwardAndUp);
        if (!valid) return std::unexpected(std::move(valid.error()));
        if (*valid) { addNode(Node{forwardAndUp, {}}); return {}; }
      }
    }
    auto forwardValid = validPosition(forward);
    if (!forwardValid) return std::unexpected(std::move(forwardValid.error()));
    if (*forwardValid) {
      auto grounded = onGround(forward);
      if (!grounded) return std::unexpected(std::move(grounded.error()));
      if (*grounded) { addNode(Node{forward, {}}); return {}; }
    }
    if (slopeDown) {
      auto valid = validPosition(forward);
      if (!valid) return std::unexpected(std::move(valid.error()));
      if (*valid) {
        auto downValid = validPosition(forwardAndDown);
        if (!downValid) return std::unexpected(std::move(downValid.error()));
        if (*downValid) {
          auto grounded = onGround(forwardAndDown);
          if (!grounded) return std::unexpected(std::move(grounded.error()));
          if (*grounded) { addNode(Node{forwardAndDown, {}}); return {}; }
        }
      }
    }
    auto ledgeValid = validPosition(forward);
    if (!ledgeValid) return std::unexpected(std::move(ledgeValid.error()));
    if (*ledgeValid) {
      if (!m_movementParams.standingPoly) return std::unexpected(MovementControllerException("PathFinder requires standingPoly for walking"));
      bounds = m_movementParams.standingPoly->boundBox();
      float back = direction > 0 ? bounds.xMin() : bounds.xMax();
      forward[0] -= (1 - std::fmod(std::abs(back), 1.0f)) * direction;
      if (walkSpeed) addNode(Node{forward, Vec2F{std::copysign(*walkSpeed, direction), 0.0f}});
      if (runSpeed) addNode(Node{forward, Vec2F{std::copysign(*runSpeed, direction), 0.0f}});
    }
    return {};
  }

  std::expected<void, StarException> PathFinder::getWalkingNeighbors(Node const& node, List<Edge>& neighbors) const {
    auto right = getWalkingNeighborsInDirection(node, neighbors, NodeGranularity);
    if (!right) return std::unexpected(std::move(right.error()));
    return getWalkingNeighborsInDirection(node, neighbors, -NodeGranularity);
  }

  std::expected<void, StarException> PathFinder::getFallingNeighbors(Node const& node, List<Edge>& neighbors) const {
    return forEachArcNeighbor(
      node, 0.0f,
      [&self = *this, &node, &neighbors](Node const& target, bool landed) -> std::expected<void, StarException> {
        neighbors.push_back(self.defaultCostEdge(Action::Arc, node, target));
        if (landed)
          neighbors.push_back(self.defaultCostEdge(Action::Land, target, Node{target.position, {}}));
        return {};
      });
  }

  std::expected<void, StarException> PathFinder::getJumpingNeighbors(Node const& node, List<Edge>& neighbors) const {
    if (auto jumpSpeed = m_movementParams.airJumpProfile.jumpSpeed) {
      float jumpCost = m_searchParams.jumpCost.value_or(DefaultJumpCost);
      auto liquid = inLiquid(node.position);
      if (!liquid) return std::unexpected(std::move(liquid.error()));
      if (*liquid) jumpCost = m_searchParams.liquidJumpCost.value_or(DefaultLiquidJumpCost);
      auto addVel = [jumpCost, &node, &neighbors](Vec2F const& vel) -> std::expected<void, StarException> {
        neighbors.push_back(Edge{jumpCost, Action::Jump, vel, node, node.withVelocity(vel)});
        return {};
      };
      auto fullJump = forEachArcVelocity(*jumpSpeed, addVel);
      if (!fullJump) return std::unexpected(std::move(fullJump.error()));
      return forEachArcVelocity(*jumpSpeed * m_searchParams.smallJumpMultiplier.value_or(DefaultSmallJumpMultiplier), addVel);
    }
    return {};
  }

  std::expected<void, StarException> PathFinder::getSwimmingNeighbors(Node const& node, List<Edge>& neighbors) const {
    // We assume when swimming that movement can be freely against gravity.
    auto flying = getFlyingNeighbors(node, neighbors);
    if (!flying) return std::unexpected(std::move(flying.error()));
    auto bounds = boundBox(node.position);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    auto acc = acceleration(node.position);
    if (!acc) return std::unexpected(std::move(acc.error()));
    if ((*acc)[1] != 0.0f) {
      auto liquid = m_world.get().liquidLevel(*bounds);
      if (!liquid) return std::unexpected(std::move(liquid.error()));
      if (liquid->level < 1.0f) {
        auto jumping = getJumpingNeighbors(node, neighbors);
        if (!jumping) return std::unexpected(std::move(jumping.error()));
      }
    }
    // Filtering is stable and fallible predicates are checked before each erase.
    for (auto it = neighbors.begin(); it != neighbors.end();) {
      auto liquid = inLiquid(it->target.position);
      if (!liquid) return std::unexpected(std::move(liquid.error()));
      if (!*liquid) it = neighbors.erase(it);
      else ++it;
    }
    for (auto& edge : neighbors) {
      if (edge.action == Action::Fly) edge.action = Action::Swim;
      edge.cost *= m_searchParams.swimCost.value_or(DefaultSwimCost);
    }
    return {};
  }

  std::expected<void, StarException> PathFinder::getFlyingNeighbors(Node const& node, List<Edge>& neighbors) const {
    auto roundedPosition = roundToNode(node.position);
    if (!roundedPosition) return std::unexpected(std::move(roundedPosition.error()));
    for (int dx = -1; dx < 2; ++dx) {
      for (int dy = -1; dy < 2; ++dy) {
        Vec2F newPosition = *roundedPosition + Vec2F(dx, dy) * NodeGranularity;
        auto valid = validPosition(newPosition);
        if (!valid) return std::unexpected(std::move(valid.error()));
        if (*valid)
          neighbors.push_back(defaultCostEdge(Action::Fly, node, Node{newPosition, {}}));
      }
    }
    return {};
  }

  std::expected<void, StarException> PathFinder::getArcNeighbors(Node const& node, List<Edge>& neighbors) const {
    return simulateArc(
      node, [&self = *this, &node, &neighbors](Node const& target, bool landed) -> std::expected<void, StarException> {
        neighbors.push_back(self.defaultCostEdge(Action::Arc, node, target));
        if (landed)
          neighbors.push_back(self.defaultCostEdge(Action::Land, target, Node{target.position, {}}));
        return {};
      });
  }

  std::expected<void, StarException> PathFinder::forEachArcVelocity(float yVelocity, std::function<std::expected<void, StarException>(Vec2F)> func) const {
    auto walkSpeed = m_movementParams.walkSpeed;
    auto runSpeed = m_movementParams.runSpeed;
    auto vertical = func(Vec2F(0, yVelocity));
    if (!vertical) return std::unexpected(std::move(vertical.error()));
    if (m_searchParams.enableWalkSpeedJumps && walkSpeed) {
      auto right = func(Vec2F(*walkSpeed, yVelocity));
      if (!right) return std::unexpected(std::move(right.error()));
      auto left = func(Vec2F(-*walkSpeed, yVelocity));
      if (!left) return std::unexpected(std::move(left.error()));
    }
    if (runSpeed) {
      auto right = func(Vec2F(*runSpeed, yVelocity));
      if (!right) return std::unexpected(std::move(right.error()));
      auto left = func(Vec2F(-*runSpeed, yVelocity));
      if (!left) return std::unexpected(std::move(left.error()));
    }
    return {};
  }

  std::expected<void, StarException> PathFinder::forEachArcNeighbor(Node const& node, float yVelocity, std::function<std::expected<void, StarException>(Node, bool)> func) const {
    auto position = roundToNode(node.position);
    if (!position) return std::unexpected(std::move(position.error()));
    return forEachArcVelocity(yVelocity, [&self = *this, &position, &func](Vec2F const& vel) -> std::expected<void, StarException> {
      return self.simulateArc(Node{*position, vel}, func);
    });
  }

  std::expected<Vec2F, StarException> PathFinder::acceleration(Vec2F pos) const {
    auto const& parameters = m_movementParams;
    auto worldGravity = m_world.get().gravity(pos);
    if (!worldGravity) return std::unexpected(std::move(worldGravity.error()));
    float gravity = *worldGravity * parameters.gravityMultiplier.value_or(1.0f);
    if (!parameters.gravityEnabled.value_or(true) || parameters.mass.value_or(0.0f) == 0.0f) gravity = 0.0f;
    float buoyancy = parameters.airBuoyancy.value_or(0.0f);
    return Vec2F(0, -gravity * (1.0f - buoyancy));
  }

  std::expected<Vec2F, StarException> PathFinder::simulateArcCollision(Vec2F position, Vec2F velocity, float dt, bool& collidedX, bool& collidedY) const {
    // Use tile bounds, rather than expensive full collision resolution.
    Vec2F newPosition = position + velocity * dt;
    auto valid = validPosition(newPosition);
    if (!valid) return std::unexpected(std::move(valid.error()));
    if (*valid) {
      collidedX = collidedY = false;
      return newPosition;
    }
    collidedX = collidedY = true;
    auto horizontal = validPosition(Vec2F(newPosition[0], position[1]));
    if (!horizontal) return std::unexpected(std::move(horizontal.error()));
    if (*horizontal) {
      collidedX = false;
      position[0] = newPosition[0];
    } else {
      auto vertical = validPosition(Vec2F(position[0], newPosition[1]), BoundBoxKind::Stand);
      if (!vertical) return std::unexpected(std::move(vertical.error()));
      if (*vertical) {
        collidedY = false;
        position[1] = newPosition[1];
      }
    }
    return position;
  }

  std::expected<void, StarException> PathFinder::simulateArc(Node const& node, std::function<std::expected<void, StarException>(Node, bool)> func) const {
    Vec2F position = node.position;
    if (!node.velocity) return std::unexpected(MovementControllerException("PathFinder arc requires velocity"));
    Vec2F velocity = *node.velocity;
    bool jumping = velocity[1] > 0.0f;
    float maxLandingVelocity = m_searchParams.maxLandingVelocity.value_or(DefaultMaxLandingVelocity);
    
    auto accResult = acceleration(position);
    if (!accResult) return std::unexpected(std::move(accResult.error()));
    Vec2F acc = *accResult;
    if (acc[1] == 0.0f)
      return {};

    // Simulate until we're roughly NodeGranularity distance from the previous
    // node
    auto startResult = roundToNode(node.position);
    if (!startResult) return std::unexpected(std::move(startResult.error()));
    Vec2F start = *startResult;
    Vec2F rounded = start;
    while (rounded == start) {
      float speed = velocity.magnitude();
      float dt = std::fmin(0.2f, speed != 0.0f ? SimulateArcGranularity / speed : std::sqrt(SimulateArcGranularity * 2.0 * std::abs(acc[1])));

      bool collidedX = false, collidedY = false;
      auto simulated = simulateArcCollision(position, velocity, dt, collidedX, collidedY);
      if (!simulated) return std::unexpected(std::move(simulated.error()));
      position = *simulated;
      auto roundedResult = roundToNode(position);
      if (!roundedResult) return std::unexpected(std::move(roundedResult.error()));
      rounded = *roundedResult;

      if (collidedY) {
        // We've either landed or hit our head on the ceiling
        if (!jumping) {
          // Landed
          if (velocity[1] < maxLandingVelocity)
            if (auto result = func(Node{rounded, velocity}, true); !result) return std::unexpected(std::move(result.error()));
          return {};
        } else {
          auto grounded = onGround(rounded, BoundBoxKind::Stand);
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

      } else if (collidedX) {
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
        std::optional<float> runSpeed = m_movementParams.runSpeed;
        std::optional<float> walkSpeed = m_movementParams.walkSpeed;
        float crawlMultiplier = m_searchParams.jumpDropXMultiplier.value_or(DefaultJumpDropXMultiplier);

        if ((*node.velocity)[0] != 0.0f || m_searchParams.enableVerticalJumpAirControl) {
          if (runSpeed.has_value())
            if (auto result = func(Node{position, Vec2F{std::copysign(*runSpeed, velocity[0]), 0.0f}}, false); !result) return std::unexpected(std::move(result.error()));
          if (m_searchParams.enableWalkSpeedJumps && walkSpeed.has_value()) {
            if (auto result = func(Node{position, Vec2F{std::copysign(*walkSpeed, velocity[0]), 0.0f}}, false); !result) return std::unexpected(std::move(result.error()));
            if (auto result = func(Node{position, Vec2F{std::copysign(*walkSpeed * crawlMultiplier, velocity[0]), 0.0f}}, false); !result) return std::unexpected(std::move(result.error()));
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
      if (velocity[1] < maxLandingVelocity) {
        auto grounded = onGround(rounded, BoundBoxKind::Stand);
        if (!grounded) return std::unexpected(std::move(grounded.error()));
        bool landed = *grounded;
        if (!landed) {
          auto liquid = inLiquid(rounded);
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

  std::expected<bool, StarException> PathFinder::validPosition(Vec2F pos, BoundBoxKind boundKind) const {
    auto bounds = boundBox(pos, boundKind);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    auto collision = m_world.get().rectTileCollision(RectI::integral(*bounds), CollisionSolid);
    if (!collision) return std::unexpected(std::move(collision.error()));
    return !*collision;
  }

  std::expected<bool, StarException> PathFinder::onGround(Vec2F pos, BoundBoxKind boundKind) const {
    auto groundRect = groundCollisionRect(pos, boundKind);
    if (!groundRect) return std::unexpected(std::move(groundRect.error()));
    auto bounds = boundBox(pos, boundKind);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    auto insideObject = m_world.get().rectTileCollision(RectI::integral(*bounds), CollisionDynamic);
    if (!insideObject) return std::unexpected(std::move(insideObject.error()));
    if (*insideObject) return m_world.get().rectTileCollision(*groundRect, CollisionFloorOnly);
    auto grounded = m_world.get().rectTileCollision(*groundRect, CollisionAny);
    if (!grounded) return std::unexpected(std::move(grounded.error()));
    if (*grounded) return true;
    return m_world.get().rectTileCollision(groundRect->translated(Vec2I(0, 1)), CollisionSolid);
  }

  std::expected<bool, StarException> PathFinder::onSolidGround(Vec2F pos) const {
    auto groundRect = groundCollisionRect(pos, BoundBoxKind::Drop);
    if (!groundRect) return std::unexpected(std::move(groundRect.error()));
    return m_world.get().rectTileCollision(*groundRect, CollisionSolid);
  }

  std::expected<bool, StarException> PathFinder::inLiquid(Vec2F pos) const {
    auto bounds = boundBox(pos);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    auto liquid = m_world.get().liquidLevel(*bounds);
    if (!liquid) return std::unexpected(std::move(liquid.error()));
    return liquid->level >= m_movementParams.minimumLiquidPercentage.value_or(0.5f);
  }

  std::expected<RectF, StarException> PathFinder::boundBox(Vec2F pos, BoundBoxKind boundKind) const {
    RectF bounds;
    if (boundKind == BoundBoxKind::Drop && m_searchParams.droppingBoundBox)
      bounds = *m_searchParams.droppingBoundBox;
    else if (boundKind == BoundBoxKind::Stand && m_searchParams.standingBoundBox)
      bounds = *m_searchParams.standingBoundBox;
    else if (m_searchParams.boundBox)
      bounds = *m_searchParams.boundBox;
    else {
      if (!m_movementParams.standingPoly) return std::unexpected(MovementControllerException("PathFinder requires standingPoly or boundBox"));
      bounds = m_movementParams.standingPoly->boundBox();
    }
    bounds.scale(BoundBoxRoundingErrorScaling);
    bounds.translate(pos);
    return bounds;
  }

  std::expected<RectI, StarException> PathFinder::groundCollisionRect(Vec2F pos, BoundBoxKind boundKind) const {
    auto box = boundBox(pos, boundKind);
    if (!box) return std::unexpected(std::move(box.error()));
    RectI bounds = RectI::integral(*box);
    return RectI(Vec2I(bounds.xMin(), bounds.yMin() - 1), Vec2I(bounds.xMax(), bounds.yMin()));
  }

  std::expected<Vec2I, StarException> PathFinder::groundNodePosition(Vec2F pos) const {
    auto box = boundBox(pos, BoundBoxKind::Full);
    if (!box) return std::unexpected(std::move(box.error()));
    return Vec2I(std::floor(pos[0]), RectI::integral(*box).yMin() - 1);
  }

  std::expected<Vec2F, StarException> PathFinder::roundToNode(Vec2F pos) const {
    // Round Y relative to the feet so exact-height gaps retain their nodes.
    RectF bounds;
    if (m_searchParams.boundBox) bounds = *m_searchParams.boundBox;
    else {
      if (!m_movementParams.standingPoly) return std::unexpected(MovementControllerException("PathFinder requires standingPoly or boundBox"));
      bounds = m_movementParams.standingPoly->boundBox();
    }
    float bottom = bounds.yMin();
    float x = std::round(pos[0] / NodeGranularity) * NodeGranularity;
    float y = std::round((pos[1] + bottom) / NodeGranularity) * NodeGranularity - bottom;
    return Vec2F(x, y);
  }

  float PathFinder::distance(Vec2F a, Vec2F b) const {
    return m_world.get().geometry().diff(a, b).magnitude();
  }
}

}
