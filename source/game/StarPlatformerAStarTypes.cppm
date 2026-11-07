module;
import std;
import star.optional;
import star.format;
import star.string;
import star.stream_formatter;

import star.vector;
import star.rect;
import star.bi_map;
import star.a_star;

namespace Star {
namespace PlatformerAStar {

class PathFinder;

struct Node {
  friend bool operator==(Node const& a, Node const& b);
  friend std::ostream& operator<<(std::ostream& os, Node const& node);

  Node withVelocity(Vec2F velocity) const;

  bool operator<(Node const& other) const;

  Vec2F position;
  std::optional<Vec2F> velocity; // Only valid when jumping/falling
};

enum class Action { Walk, Jump, Arc, Drop, Swim, Fly, Land };
extern std::expected<EnumMap<Action>, StarException> const ActionNames;

struct Edge {
  float cost;
  Action action;
  Vec2F jumpVelocity;
  Node source;
  Node target;
};

using Path = AStar::Path<Edge>;

struct Parameters {
  friend bool operator==(Parameters const& lhs, Parameters const& rhs);
  friend bool operator!=(Parameters const& lhs, Parameters const& rhs);

  // Maximum distance from the start node to search for a path to the target
  // node
  std::optional<float> maxDistance;
  // If true, returns the path to the closest node to the target found, if a
  // path to the target itself could not be found.
  // Otherwise, findPath will return a None value.
  bool returnBest{false};
  // If true, end the path only on ground
  bool mustEndOnGround{false};
  // If true, allows jumps to have the entity's walk speed as horizontal
  // velocity
  bool enableWalkSpeedJumps{false};
  // if true, allows perfectly vertical jumps to change horizontal velocity at
  // the peak
  bool enableVerticalJumpAirControl{false};
  // Multiplies the cost of edges going through liquids. Can be used to
  // penalize or promote paths involving swiming.
  std::optional<float> swimCost;
  // The cost of jump edges.
  std::optional<float> jumpCost;
  // The cost of jump edges that start in liquids.
  std::optional<float> liquidJumpCost;
  // The cost of dropping through a platform.
  std::optional<float> dropCost;
  // If set, will be the default bounding box, otherwise will use
  // movementParameters.standingPoly.
  std::optional<RectF> boundBox;
  // The bound box used for checking if the entity can stand at a position
  // Should be thinner than the full bound box
  std::optional<RectF> standingBoundBox;
  // The bound box used for checking if the entity can drop at a position
  // Should be wider than the full bound box
  std::optional<RectF> droppingBoundBox;
  // Pathing simulates jump arcs for two Y velocities: 1.0 * jumpSpeed and
  // smallJumpMultiplier * jumpSpeed. This value should be in the range
  // 0 < smallJumpMultiplier < 1.0
  std::optional<float> smallJumpMultiplier;
  // Mid-jump, at the peak, entities can choose to change horizontal velocity.
  // The velocities they can switch to are runSpeed, walkSpeed, and
  // (walkSpeed * jumpDropXMultiplier). The purpose of the latter option is to
  // make a vertical drop (if 0) or disable dropping (if 1). Inbetween values
  // can be used to make less angular-looking arcs.
  std::optional<float> jumpDropXMultiplier;
  // If provided, the following fields can be supplied to put a limit on how
  // long findPath calls can take:
  std::optional<double> maxFScore;
  std::optional<unsigned> maxNodesToSearch;
  // Upper bound on the (negative) velocity that entities can land on
  // platforms
  // and ledges with. This is used to ensure there is a small amount of
  // clearance
  // over ledges to improve the scripts' chances of landing the same way we
  // simulated the jump.
  std::optional<float> maxLandingVelocity;
};

bool operator==(Parameters const& lhs, Parameters const& rhs);
bool operator!=(Parameters const& lhs, Parameters const& rhs);

inline bool operator==(Node const& a, Node const& b) {
  return a.position == b.position && a.velocity == b.velocity;
}

inline std::ostream& operator<<(std::ostream& os, Node const& node);
[[nodiscard]] std::expected<std::reference_wrapper<String const>, StarException> actionName(Action action);
[[nodiscard]] std::expected<String, StarException> printEdge(Edge const& edge);

}

}

// The formatter specializations have to be visible before the stream operators
// that use them are defined below, otherwise the instantiations come first.
template <> struct std::formatter<Star::PlatformerAStar::Node> : Star::StreamFormatter {};

namespace Star {
namespace PlatformerAStar {

inline std::ostream& operator<<(std::ostream& os, Node const& node) {
  return os << strf("Node{{position = {}, velocity = {}}}", node.position, outputOptional(node.velocity));
}

inline bool Node::operator<(Node const& other) const {
  if (position == other.position)
    return velocity < other.velocity;
  return position < other.position;
}

inline bool operator==(Parameters const& lhs, Parameters const& rhs) {
  return lhs.maxDistance == rhs.maxDistance
    && lhs.returnBest == rhs.returnBest
    && lhs.mustEndOnGround == rhs.mustEndOnGround
    && lhs.enableWalkSpeedJumps == rhs.enableWalkSpeedJumps
    && lhs.enableVerticalJumpAirControl == rhs.enableVerticalJumpAirControl
    && lhs.swimCost == rhs.swimCost
    && lhs.jumpCost == rhs.jumpCost
    && lhs.liquidJumpCost == rhs.liquidJumpCost
    && lhs.dropCost == rhs.dropCost
    && lhs.boundBox == rhs.boundBox
    && lhs.standingBoundBox == rhs.standingBoundBox
    && lhs.droppingBoundBox == rhs.droppingBoundBox
    && lhs.smallJumpMultiplier == rhs.smallJumpMultiplier
    && lhs.jumpDropXMultiplier == rhs.jumpDropXMultiplier
    && lhs.maxFScore == rhs.maxFScore
    && lhs.maxNodesToSearch == rhs.maxNodesToSearch
    && lhs.maxLandingVelocity == rhs.maxLandingVelocity;
}

inline bool operator!=(Parameters const& lhs, Parameters const& rhs) {
  return !(lhs == rhs);
}

}
}

export module star.platformer_astar_types;

export template <> struct std::formatter<Star::PlatformerAStar::Node>;

export namespace Star::PlatformerAStar {
  using ::Star::PlatformerAStar::PathFinder;
  using ::Star::PlatformerAStar::Node;
  using ::Star::PlatformerAStar::Action;
  using ::Star::PlatformerAStar::ActionNames;
  using ::Star::PlatformerAStar::Edge;
  using ::Star::PlatformerAStar::Path;
  using ::Star::PlatformerAStar::Parameters;
  using ::Star::PlatformerAStar::actionName;
  using ::Star::PlatformerAStar::printEdge;
  using ::Star::PlatformerAStar::operator<<;
}
