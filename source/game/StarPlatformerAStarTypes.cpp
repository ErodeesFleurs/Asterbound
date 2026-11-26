import std;
import star.format;
import star.map;
import star.platformer_astar_types;

namespace Star {
namespace PlatformerAStar {

std::expected<std::string_view, StarException> action_name(Action action) {
  using namespace std::string_view_literals;
  switch (action) {
    case Action::Walk: return "Walk"sv;
    case Action::Jump: return "Jump"sv;
    case Action::Arc: return "Arc"sv;
    case Action::Drop: return "Drop"sv;
    case Action::Swim: return "Swim"sv;
    case Action::Fly: return "Fly"sv;
    case Action::Land: return "Land"sv;
  }
  return std::unexpected(MapException::format("Key '{}' not found in Map::get()", std::to_underlying(action)));
}
}
}
