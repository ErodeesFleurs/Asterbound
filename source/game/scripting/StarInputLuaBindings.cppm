module;
import std;
import star.format;
import star.lua;

namespace Star {
class LuaCallbacks;
class Input;
}

export module star.input_lua_bindings;

export namespace Star::LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_input_callbacks(Input& input);
}
