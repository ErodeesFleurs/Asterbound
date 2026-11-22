module;
import std;
import star.string;
import star.lua;
import star.random;
import star.logging;
import star.format;

namespace Star {

namespace LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_utility_callbacks(RandomService& random, Logger& logger, LogMap& log_map);

  [[nodiscard]] std::expected<String, StarException> format_lua(String const& format, std::span<LuaValue const> args);
}
}

export module star.utility_lua_bindings;

export namespace Star::LuaBindings {
  using ::Star::LuaBindings::make_utility_callbacks;
  using ::Star::LuaBindings::format_lua;
}
