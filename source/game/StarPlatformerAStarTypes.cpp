import star.vector;
import std;
import star.format;
import star.string;
import star.bi_map;
import star.a_star;
import star.platformer_astar_types;

namespace Star {
namespace PlatformerAStar {

extern std::expected<EnumMap<Action>, StarException> const ActionNames = EnumMap<Action>::create({
  {Action::Walk, "Walk"},
  {Action::Jump, "Jump"},
  {Action::Arc, "Arc"},
  {Action::Drop, "Drop"},
  {Action::Swim, "Swim"},
  {Action::Fly, "Fly"},
  {Action::Land, "Land"}
});

Node Node::withVelocity(Vec2F velocity) const {
  return {position, velocity};
}

std::expected<std::reference_wrapper<String const>, StarException> actionName(Action action) {
  if (!ActionNames)
    return std::unexpected(ActionNames.error());
  return ActionNames->get_right(action);
}

std::expected<String, StarException> printEdge(Edge const& edge) {
  auto name = actionName(edge.action);
  if (!name)
    return std::unexpected(std::move(name.error()));
  return strf("Edge{{cost = {}, action = {}, jumpVelocity = {}, source = {}, target = {}}}",
      edge.cost, name->get(), edge.jumpVelocity, edge.source, edge.target);
}

}
}
