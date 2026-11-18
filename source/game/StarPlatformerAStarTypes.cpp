import star.vector;
import std;
import star.format;
import star.string;
import star.bi_map;
import star.platformer_astar_types;

namespace Star {
namespace PlatformerAStar {

extern std::expected<EnumMap<Action>, StarException> const action_names =
  EnumMap<Action>::create({{Action::Walk, "Walk"},
                           {Action::Jump, "Jump"},
                           {Action::Arc, "Arc"},
                           {Action::Drop, "Drop"},
                           {Action::Swim, "Swim"},
                           {Action::Fly, "Fly"},
                           {Action::Land, "Land"}});

Node Node::with_velocity(Vec2F velocity) const { return {position, velocity}; }

std::expected<std::reference_wrapper<String const>, StarException> action_name(Action action) {
  if (!action_names)
    return std::unexpected(action_names.error());
  return action_names->get_right(action);
}

std::expected<String, StarException> print_edge(Edge const& edge) {
  auto name = action_name(edge.action);
  if (!name)
    return std::unexpected(std::move(name.error()));
  return Star::format("Edge{{cost = {}, action = {}, jumpVelocity = {}, source = {}, target = {}}}", edge.cost,
                      name->get(), edge.jump_velocity, edge.source, edge.target);
}
}
}
