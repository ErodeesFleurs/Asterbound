module;

// Match client include order for SIMD intrinsics used by xxhash.
import std;
import star.format;
import star.list;
import star.json;
import star.lua;

namespace Star {
class Root;
class BehaviorState;
}

export module star.behavior_lua_bindings;

export namespace Star::LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_behavior_callbacks(Root& root, List<std::shared_ptr<BehaviorState>>& list);
}

