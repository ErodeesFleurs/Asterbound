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

std::expected<LuaCallbacks, StarException> LuaBindings::make_scripted_animator_callbacks(NetworkedAnimator& networked_animator, std::function<std::expected<Json, StarException>(String const&, Json const&)> get_parameter) {
  LuaCallbacks callbacks;

  if (auto result = callbacks.register_callback("animationParameter", std::move(get_parameter)); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = Detail::register_animator_part_query_callbacks(callbacks, networked_animator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = Detail::register_animator_state_callbacks(callbacks, networked_animator, Detail::AnimatorStateCallbackOrder::Scripted); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback_with_signature<bool, String>(
      "hasTransformationGroup", std::bind_front(&NetworkedAnimator::has_transformation_group, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));

  if (auto result = Detail::register_animator_local_transformation_callbacks(callbacks, networked_animator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = Detail::register_animator_drawable_callbacks(callbacks, networked_animator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = Detail::register_animator_playback_query_callbacks(callbacks, networked_animator); !result) return std::unexpected(std::move(result.error()));


  return callbacks;
}

}
