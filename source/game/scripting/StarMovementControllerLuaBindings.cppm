module;
import std;
import star.format;
import star.lua;

namespace Star {
class LuaCallbacks;
class MovementController;
}

export module star.movement_controller_lua_bindings;

export namespace Star::LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_movement_controller_callbacks(MovementController& movement_controller);
}
