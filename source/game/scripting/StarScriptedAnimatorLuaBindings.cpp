module;
import std;
import star.format;
import star.json;
import star.json_extra;
import star.lua;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.matrix3;
import star.string;
import star.data_stream;
import star.poly;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.vector;
import star.audio;
import star.map;
import star.set;


// Match client include order for SIMD intrinsics used by xxhash.
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.lua_game_converters;

module star.scripted_animator_lua_bindings;

import star.networked_animator_lua_bindings;

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeScriptedAnimatorCallbacks(NetworkedAnimator& networkedAnimator, std::function<std::expected<Json, StarException>(String const&, Json const&)> getParameter) {
  LuaCallbacks callbacks;

  if (auto result = callbacks.registerCallback("animationParameter", std::move(getParameter)); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = Detail::registerAnimatorPartQueryCallbacks(callbacks, networkedAnimator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = Detail::registerAnimatorStateCallbacks(callbacks, networkedAnimator, Detail::AnimatorStateCallbackOrder::Scripted); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<bool, String>(
      "hasTransformationGroup", std::bind_front(&NetworkedAnimator::hasTransformationGroup, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));

  if (auto result = Detail::registerAnimatorLocalTransformationCallbacks(callbacks, networkedAnimator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = Detail::registerAnimatorDrawableCallbacks(callbacks, networkedAnimator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = Detail::registerAnimatorPlaybackQueryCallbacks(callbacks, networkedAnimator); !result) return std::unexpected(std::move(result.error()));


  return callbacks;
}

}
