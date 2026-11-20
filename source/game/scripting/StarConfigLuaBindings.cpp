module;

// Match client include order for SIMD intrinsics used by xxhash.
import std;
import star.format;
import star.string;
import star.json;
import star.lua;
import star.json_extra;
import star.lua_game_converters;

module star.config_lua_bindings;

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeConfigCallbacks(std::function<std::expected<Json, StarException>(String const&, Json const&)> getParameter) {
  LuaCallbacks callbacks;

  if (auto result = callbacks.register_callback("getParameter", std::move(getParameter)); !result)
    return std::unexpected(std::move(result.error()));

  return callbacks;
}

}
