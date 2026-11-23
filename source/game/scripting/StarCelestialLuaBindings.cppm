module;

import std;
import star.format;

namespace Star {
class Root;
class LuaCallbacks;
class CelestialDatabase;
class Universe;
}

export module star.celestial_lua_bindings;

export namespace Star::LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_celestial_callbacks(Root& root, std::shared_ptr<CelestialDatabase> database);
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_celestial_callbacks(Root& root, Universe& universe);
}
