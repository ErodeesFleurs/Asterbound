module;
import std;
import star.format;
import star.lua;

namespace Star {
class Root;
class Player;
}

export module star.player_lua_bindings;

export namespace Star::LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_player_callbacks(Root& root, Player& player);
}
