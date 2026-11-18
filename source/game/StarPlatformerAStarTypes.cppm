module;
import std;
import star.optional;
import star.format;
import star.string;
import star.text_formatter;

import star.vector;
import star.rect;
import star.bi_map;
import star.a_star;

namespace Star {
namespace PlatformerAStar {

class PathFinder;

struct Node {
  friend bool operator==(Node const& a, Node const& b) = default;
  friend void write_text(TextBuffer& buffer, Node const& node);

  Node with_velocity(Vec2F velocity) const;

  bool operator<(Node const& other) const;

  Vec2F position;
  std::optional<Vec2F> velocity; // Only valid when jumping/falling
};

enum class Action { Walk, Jump, Arc, Drop, Swim, Fly, Land };
extern std::expected<EnumMap<Action>, StarException> const action_names;

struct Edge {
  float cost;
  Action action;
  Vec2F jump_velocity;
  Node source;
  Node target;
};

using Path = AStar::Path<Edge>;

struct Parameters {
  friend bool operator==(Parameters const& lhs, Parameters const& rhs) = default;

  // Maximum distance from the start node to search for a path to the target
  // node
  std::optional<float> max_distance;
  // If true, returns the path to the closest node to the target found, if a
  // path to the target itself could not be found.
  // Otherwise, find_path will return a None value.
  bool return_best{false};
  // If true, end the path only on ground
  bool must_end_on_ground{false};
  // If true, allows jumps to have the entity's walk speed as horizontal
  // velocity
  bool enable_walk_speed_jumps{false};
  // if true, allows perfectly vertical jumps to change horizontal velocity at
  // the peak
  bool enable_vertical_jump_air_control{false};
  // Multiplies the cost of edges going through liquids. Can be used to
  // penalize or promote paths involving swiming.
  std::optional<float> swim_cost;
  // The cost of jump edges.
  std::optional<float> jump_cost;
  // The cost of jump edges that start in liquids.
  std::optional<float> liquid_jump_cost;
  // The cost of dropping through a platform.
  std::optional<float> drop_cost;
  // If set, will be the default bounding box, otherwise will use
  // movementParameters.standingPoly.
  std::optional<RectF> bound_box;
  // The bound box used for checking if the entity can stand at a position
  // Should be thinner than the full bound box
  std::optional<RectF> standing_bound_box;
  // The bound box used for checking if the entity can drop at a position
  // Should be wider than the full bound box
  std::optional<RectF> dropping_bound_box;
  // Pathing simulates jump arcs for two Y velocities: 1.0 * jumpSpeed and
  // small_jump_multiplier * jumpSpeed. This value should be in the range
  // 0 < small_jump_multiplier < 1.0
  std::optional<float> small_jump_multiplier;
  // Mid-jump, at the peak, entities can choose to change horizontal velocity.
  // The velocities they can switch to are runSpeed, walkSpeed, and
  // (walkSpeed * jump_drop_x_multiplier). The purpose of the latter option is to
  // make a vertical drop (if 0) or disable dropping (if 1). Inbetween values
  // can be used to make less angular-looking arcs.
  std::optional<float> jump_drop_x_multiplier;
  // If provided, the following fields can be supplied to put a limit on how
  // long find_path calls can take:
  std::optional<double> max_f_score;
  std::optional<unsigned> max_nodes_to_search;
  // Upper bound on the (negative) velocity that entities can land on
  // platforms
  // and ledges with. This is used to ensure there is a small amount of
  // clearance
  // over ledges to improve the scripts' chances of landing the same way we
  // simulated the jump.
  std::optional<float> max_landing_velocity;
};

inline void write_text(TextBuffer& buffer, Node const& node);
[[nodiscard]] std::expected<std::reference_wrapper<String const>, StarException> action_name(Action action);
[[nodiscard]] std::expected<String, StarException> print_edge(Edge const& edge);
}

}

// The formatter specialization must be visible before formatting Node below.
template <> struct std::formatter<Star::PlatformerAStar::Node> : Star::TextFormatter {};

namespace Star {
namespace PlatformerAStar {

inline void write_text(TextBuffer& buffer, Node const& node) {
  std::format_to(std::back_inserter(buffer), "Node{{position = {}, velocity = {}}}", node.position, Star::output_optional(node.velocity));
}

inline bool Node::operator<(Node const& other) const {
  if (position == other.position)
    return velocity < other.velocity;
  return position < other.position;
}

}
}

export module star.platformer_astar_types;

export template <> struct std::formatter<Star::PlatformerAStar::Node>;

export namespace Star::PlatformerAStar {
using ::Star::PlatformerAStar::Action;
using ::Star::PlatformerAStar::action_name;
using ::Star::PlatformerAStar::action_names;
using ::Star::PlatformerAStar::Edge;
using ::Star::PlatformerAStar::Node;
using ::Star::PlatformerAStar::Parameters;
using ::Star::PlatformerAStar::Path;
using ::Star::PlatformerAStar::PathFinder;
using ::Star::PlatformerAStar::print_edge;
using ::Star::PlatformerAStar::write_text;
}
