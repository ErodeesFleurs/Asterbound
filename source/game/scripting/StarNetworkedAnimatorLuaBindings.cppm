module;

// Match client include order for SIMD intrinsics used by xxhash.
import std;
import star.format;
import star.json;
import star.lua;

namespace Star {
class NetworkedAnimator;
}

export module star.networked_animator_lua_bindings;

export namespace Star {
  using ::Star::NetworkedAnimator;
}

export namespace Star::LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_networked_animator_callbacks(NetworkedAnimator& networked_animator);

  namespace Detail {
    enum class AnimatorStateCallbackOrder { Networked, Scripted };
    [[nodiscard]] std::expected<void, StarException> register_animator_state_callbacks(LuaCallbacks& callbacks, NetworkedAnimator& networked_animator, AnimatorStateCallbackOrder order);
    [[nodiscard]] std::expected<void, StarException> register_animator_local_transformation_callbacks(LuaCallbacks& callbacks, NetworkedAnimator& networked_animator);
    [[nodiscard]] std::expected<void, StarException> register_animator_part_query_callbacks(LuaCallbacks& callbacks, NetworkedAnimator& networked_animator);
    [[nodiscard]] std::expected<void, StarException> register_animator_playback_query_callbacks(LuaCallbacks& callbacks, NetworkedAnimator& networked_animator);
    [[nodiscard]] std::expected<void, StarException> register_animator_drawable_callbacks(LuaCallbacks& callbacks, NetworkedAnimator& networked_animator);
  }
}
