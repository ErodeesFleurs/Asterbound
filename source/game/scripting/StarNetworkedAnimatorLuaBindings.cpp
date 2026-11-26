module;
import star.lua_root;
import star.root;
import star.image_metadata_database;
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
import star.rect;
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

module star.networked_animator_lua_bindings;

import star.math_common;
namespace Star {

namespace {
std::expected<void, StarException> apply_animator_json_drawables(LuaEngine& engine, JsonArray const& drawables_config,
    std::invocable<List<Drawable>&&> auto apply) {
  // Keep this execution scope alive through the mutation, including empty lists.
  auto root = lua_execution_root(engine);
  if (!root) return std::unexpected(std::move(root.error()));
  auto metadata = root->get().image_metadata_database();
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  List<Drawable> drawables;
  for (auto const& config : drawables_config) {
    auto drawable = Drawable::create(**metadata, config);
    if (!drawable) return std::unexpected(std::move(drawable.error()));
    drawables.push_back(std::move(*drawable));
  }
  apply(std::move(drawables));
  return {};
}
}

std::expected<void, StarException> LuaBindings::Detail::register_animator_state_callbacks(LuaCallbacks& callbacks, NetworkedAnimator& networked_animator, AnimatorStateCallbackOrder order) {
  // Preserve each table's historical order for these shared callbacks.
  if (auto result = callbacks.register_callback_with_signature<bool, String, String, bool, bool>(
      "setLocalAnimationState", std::bind_front(&NetworkedAnimator::set_local_state, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (order == AnimatorStateCallbackOrder::Networked) {
    if (auto result = callbacks.register_callback_with_signature<String, String>(
        "animationState", std::bind_front(&NetworkedAnimator::state, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  }
  if (auto result = callbacks.register_callback_with_signature<Json, String, String, std::optional<String>, std::optional<int>>(
      "animationStateProperty", std::bind_front(&NetworkedAnimator::state_property, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<Json, String, String>(
      "animationStateNextProperty", std::bind_front(&NetworkedAnimator::state_next_property, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (order == AnimatorStateCallbackOrder::Scripted) {
    if (auto result = callbacks.register_callback_with_signature<String, String>(
        "animationState", std::bind_front(&NetworkedAnimator::state, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  }
  if (auto result = callbacks.register_callback_with_signature<int, String>(
      "animationStateFrame", std::bind_front(&NetworkedAnimator::state_frame, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<int, String>(
      "animationStateNextFrame", std::bind_front(&NetworkedAnimator::state_next_frame, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<float, String>(
      "animationStateFrameProgress", std::bind_front(&NetworkedAnimator::state_frame_progress, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<float, String>(
      "animationStateTimer", std::bind_front(&NetworkedAnimator::state_timer, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<bool, String>(
      "animationStateReverse", std::bind_front(&NetworkedAnimator::state_reverse, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (order == AnimatorStateCallbackOrder::Networked) {
    if (auto result = callbacks.register_callback_with_signature<bool, String, std::optional<String>>(
        "hasState", std::bind_front(&NetworkedAnimator::has_state, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  }
  if (auto result = callbacks.register_callback_with_signature<float, String, std::optional<String>>(
      "stateCycle", std::bind_front(&NetworkedAnimator::state_cycle, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<int, String, std::optional<String>>(
      "stateFrames", std::bind_front(&NetworkedAnimator::state_frames, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (order == AnimatorStateCallbackOrder::Scripted) {
    if (auto result = callbacks.register_callback_with_signature<bool, String, std::optional<String>>(
        "hasState", std::bind_front(&NetworkedAnimator::has_state, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> LuaBindings::Detail::register_animator_local_transformation_callbacks(LuaCallbacks& callbacks, NetworkedAnimator& networked_animator) {
  if (auto result = callbacks.register_callback_with_signature<void, String, Vec2F>("translateLocalTransformationGroup",
      std::bind_front(&NetworkedAnimator::translate_local_transformation_group, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("rotateLocalTransformationGroup",
      [&networked_animator](String const& transformation_group, float rotation, std::optional<Vec2F> const& rotation_center) {
        return networked_animator.rotate_local_transformation_group(transformation_group, rotation, rotation_center.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("rotateDegreesLocalTransformationGroup",
      [&networked_animator](String const& transformation_group, float rotation, std::optional<Vec2F> const& rotation_center) {
        return networked_animator.rotate_local_transformation_group(transformation_group, rotation * std::numbers::pi / 180, rotation_center.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("scaleLocalTransformationGroup",
      [&networked_animator](LuaEngine& engine, String const& transformation_group, LuaValue scale, std::optional<Vec2F> const& scale_center) -> std::expected<void, StarException> {
        auto cs = engine.lua_maybe_to<Vec2F>(scale);
        if (!cs) return std::unexpected(std::move(cs.error()));
        if (*cs) return networked_animator.scale_local_transformation_group(transformation_group, **cs, scale_center.value_or(Vec2F{}));
        auto scalar = engine.lua_to<float>(scale);
        if (!scalar) return std::unexpected(std::move(scalar.error()));
        return networked_animator.scale_local_transformation_group(transformation_group, *scalar, scale_center.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, float, float, float, float, float, float>(
      "transformLocalTransformationGroup",
      std::bind_front(&NetworkedAnimator::transform_local_transformation_group, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String>(
      "resetLocalTransformationGroup", std::bind_front(&NetworkedAnimator::reset_local_transformation_group, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, Mat3F>(
      "setLocalTransformationGroup", std::bind_front(&NetworkedAnimator::set_local_transformation_group, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<Mat3F, String>(
      "getLocalTransformationGroup", std::bind_front(&NetworkedAnimator::get_local_transformation_group, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> LuaBindings::Detail::register_animator_part_query_callbacks(LuaCallbacks& callbacks, NetworkedAnimator& networked_animator) {
  if (auto result = callbacks.register_callback_with_signature<std::optional<Vec2F>, String, String>("partPoint", std::bind_front(&NetworkedAnimator::part_point, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<std::optional<PolyF>, String, String>("partPoly", std::bind_front(&NetworkedAnimator::part_poly, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<Json, String, String, std::optional<String>, std::optional<String>, std::optional<int>>("partProperty", std::bind_front(&NetworkedAnimator::part_property, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<Json, String, String>("partNextProperty", std::bind_front(&NetworkedAnimator::part_next_property, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("transformPoint", [&networked_animator] (Vec2F point, String const& part) -> std::expected<Vec2F, StarException> {
      auto transformation = networked_animator.part_transformation(part);
      if (!transformation) return std::unexpected(std::move(transformation.error()));
      return transformation->transform_point(point);
    }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("transformPoly", [&networked_animator] (PolyF poly, String const& part) -> std::expected<PolyF, StarException> {
      auto transformation = networked_animator.part_transformation(part);
      if (!transformation) return std::unexpected(std::move(transformation.error()));
      poly.transform(*transformation);
      return poly;
    }); !result) return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> LuaBindings::Detail::register_animator_playback_query_callbacks(LuaCallbacks& callbacks, NetworkedAnimator& networked_animator) {
  if (auto result = callbacks.register_callback("flipped", [&networked_animator]() -> bool {
        return networked_animator.flipped();
    }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("flippedRelativeCenterLine", [&networked_animator]() -> float {
        return networked_animator.flipped_relative_center_line();
    }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("animationRate", [&networked_animator]() -> float {
        return networked_animator.animation_rate();
    }); !result) return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> LuaBindings::Detail::register_animator_drawable_callbacks(LuaCallbacks& callbacks, NetworkedAnimator& networked_animator) {
  if (auto result = callbacks.register_callback_with_signature<void, String, List<Drawable>>(
      "addPartDrawables", std::bind_front(&NetworkedAnimator::add_part_drawables, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, List<Drawable>>(
      "setPartDrawables", std::bind_front(&NetworkedAnimator::set_part_drawables, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("addPartJsonDrawables",
    [&networked_animator](LuaEngine& engine, String const& part, JsonArray drawables_config) -> std::expected<void, StarException> {
      return apply_animator_json_drawables(engine, drawables_config, [&](List<Drawable>&& drawables) {
        networked_animator.add_part_drawables(part, std::move(drawables));
      });
    }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("setPartJsonDrawables",
    [&networked_animator](LuaEngine& engine, String const& part, JsonArray drawables_config) -> std::expected<void, StarException> {
      return apply_animator_json_drawables(engine, drawables_config, [&](List<Drawable>&& drawables) {
        networked_animator.set_part_drawables(part, std::move(drawables));
      });
    }); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback_with_signature<String, String, String>(
      "applyPartTags", std::bind_front(&NetworkedAnimator::apply_part_tags, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, std::optional<String>>(
      "setLocalTag", std::bind_front(&NetworkedAnimator::set_local_tag, std::ref(networked_animator))); !result) return std::unexpected(std::move(result.error()));
  return {};
}


std::expected<LuaCallbacks, StarException> LuaBindings::make_networked_animator_callbacks(NetworkedAnimator& networked_animator) {
  LuaCallbacks callbacks;

  if (auto result = callbacks.register_callback_with_signature<bool, String, String, bool, bool>(
        "setAnimationState",
        [&networked_animator](String const& type, String const& state, bool start_new, bool reverse) {
          return networked_animator.set_state(type, state, start_new, reverse);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = Detail::register_animator_state_callbacks(callbacks, networked_animator, Detail::AnimatorStateCallbackOrder::Networked); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback_with_signature<void, String, std::optional<String>>(
        "setGlobalTag",
        [&networked_animator](String&& name, std::optional<String>&& value) {
          return networked_animator.set_global_tag(std::move(name), std::move(value));
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, String, std::optional<String>>(
        "setPartTag",
        [&networked_animator](String const& part, String&& name, std::optional<String>&& value) {
          return networked_animator.set_part_tag(part, std::move(name), std::move(value));
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("setFlipped",
      [&networked_animator](bool flipped, std::optional<float> relative_center_line) {
        networked_animator.set_flipped(flipped, relative_center_line.value_or(0.0f));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = Detail::register_animator_playback_query_callbacks(callbacks, networked_animator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback_with_signature<void, float>(
        "setAnimationRate", [&networked_animator](float rate) { return networked_animator.set_animation_rate(rate); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, float, bool>(
        "rotateGroup",
        [&networked_animator](String const& group, float angle, bool immediate) {
          return networked_animator.rotate_group(group, angle, immediate);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<float, String>(
        "currentRotationAngle",
        [&networked_animator](String const& group) { return networked_animator.current_rotation_angle(group); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<bool, String>(
        "hasTransformationGroup",
        [&networked_animator](String const& group) { return networked_animator.has_transformation_group(group); });
      !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback_with_signature<void, String, Vec2F>(
        "translateTransformationGroup",
        [&networked_animator](String const& group, Vec2F const& translation) {
          return networked_animator.translate_transformation_group(group, translation);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("rotateTransformationGroup",
      [&networked_animator](String const& transformation_group, float rotation, std::optional<Vec2F> const& rotation_center) {
        return networked_animator.rotate_transformation_group(transformation_group, rotation, rotation_center.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("rotateDegreesTransformationGroup",
      [&networked_animator](String const& transformation_group, float rotation, std::optional<Vec2F> const& rotation_center) {
        return networked_animator.rotate_transformation_group(transformation_group, rotation * std::numbers::pi / 180, rotation_center.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("scaleTransformationGroup",
      [&networked_animator](LuaEngine& engine, String const& transformation_group, LuaValue scale, std::optional<Vec2F> const& scale_center) -> std::expected<void, StarException> {
        auto cs = engine.lua_maybe_to<Vec2F>(scale);
        if (!cs) return std::unexpected(std::move(cs.error()));
        if (*cs) return networked_animator.scale_transformation_group(transformation_group, **cs, scale_center.value_or(Vec2F{}));
        auto scalar = engine.lua_to<float>(scale);
        if (!scalar) return std::unexpected(std::move(scalar.error()));
        return networked_animator.scale_transformation_group(transformation_group, *scalar, scale_center.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, float, float, float, float, float, float>(
        "transformTransformationGroup",
        [&networked_animator](String const& group, float a, float b, float c, float d, float tx, float ty) {
          return networked_animator.transform_transformation_group(group, a, b, c, d, tx, ty);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String>(
        "resetTransformationGroup",
        [&networked_animator](String const& group) { return networked_animator.reset_transformation_group(group); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, Mat3F>(
        "setTransformationGroup",
        [&networked_animator](String const& group, Mat3F transform) {
          return networked_animator.set_transformation_group(group, transform);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<Mat3F, String>(
        "getTransformationGroup",
        [&networked_animator](String const& group) { return networked_animator.get_transformation_group(group); });
      !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = Detail::register_animator_local_transformation_callbacks(callbacks, networked_animator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback_with_signature<void, String, bool>(
        "setParticleEmitterActive",
        [&networked_animator](String const& emitter, bool active) {
          return networked_animator.set_particle_emitter_active(emitter, active);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, float>(
        "setParticleEmitterEmissionRate",
        [&networked_animator](String const& emitter, float rate) {
          return networked_animator.set_particle_emitter_emission_rate(emitter, rate);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, unsigned>(
        "setParticleEmitterBurstCount",
        [&networked_animator](String const& emitter, unsigned count) {
          return networked_animator.set_particle_emitter_burst_count(emitter, count);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, RectF>(
        "setParticleEmitterOffsetRegion",
        [&networked_animator](String const& emitter, RectF const& region) {
          return networked_animator.set_particle_emitter_offset_region(emitter, region);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String>(
        "burstParticleEmitter",
        [&networked_animator](String const& emitter) { return networked_animator.burst_particle_emitter(emitter); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, bool>(
        "setLightActive",
        [&networked_animator](String const& light, bool active) {
          return networked_animator.set_light_active(light, active);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, Vec2F>(
        "setLightPosition",
        [&networked_animator](String const& light, Vec2F position) {
          return networked_animator.set_light_position(light, position);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, Color>(
        "setLightColor",
        [&networked_animator](String const& light, Color color) {
          return networked_animator.set_light_color(light, color);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, float>(
        "setLightPointAngle",
        [&networked_animator](String const& light, float angle) {
          return networked_animator.set_light_point_angle(light, angle);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<bool, String>(
        "hasSound", [&networked_animator](String const& sound) { return networked_animator.has_sound(sound); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, StringList>(
        "setSoundPool",
        [&networked_animator](String const& sound, StringList&& pool) {
          return networked_animator.set_sound_pool(sound, std::move(pool));
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, String, Vec2F>(
        "setSoundPosition",
        [&networked_animator](String const& sound, Vec2F position) {
          return networked_animator.set_sound_position(sound, position);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("playSound",
      [&networked_animator](String const& sound, std::optional<int> loops) {
        return networked_animator.play_sound(sound, loops.value_or(0));
      }); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("setSoundVolume",
      [&networked_animator](String const& sound, float target_volume, std::optional<float> ramp_time) {
        return networked_animator.set_sound_volume(sound, target_volume, ramp_time.value_or(0));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("setSoundPitch",
      [&networked_animator](String const& sound, float target_pitch, std::optional<float> ramp_time) {
        return networked_animator.set_sound_pitch_multiplier(sound, target_pitch, ramp_time.value_or(0));
      }); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("stopAllSounds",
      [&networked_animator](String const& sound, std::optional<float> ramp_time) {
        return networked_animator.stop_all_sounds(sound, ramp_time.value_or(0.0f));
      }); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback_with_signature<void, String, bool>(
        "setEffectActive",
        [&networked_animator](String const& effect, bool active) {
          return networked_animator.set_effect_enabled(effect, active);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = Detail::register_animator_part_query_callbacks(callbacks, networked_animator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = Detail::register_animator_drawable_callbacks(callbacks, networked_animator); !result) return std::unexpected(std::move(result.error()));

  return callbacks;
}

}
