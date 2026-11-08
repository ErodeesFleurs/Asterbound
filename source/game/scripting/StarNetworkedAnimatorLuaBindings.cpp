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
std::expected<void, StarException> applyAnimatorJsonDrawables(NetworkedAnimator& networkedAnimator,
    void (NetworkedAnimator::*apply)(String const&, List<Drawable>), LuaEngine& engine,
    String const& part, JsonArray const& drawablesConfig) {
  // Keep this execution scope alive through the mutation, including empty lists.
  auto root = luaExecutionRoot(engine);
  if (!root) return std::unexpected(std::move(root.error()));
  auto metadata = root->get().imageMetadataDatabase();
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  List<Drawable> drawables;
  for (auto const& config : drawablesConfig) {
    auto drawable = Drawable::create(**metadata, config);
    if (!drawable) return std::unexpected(std::move(drawable.error()));
    drawables.push_back(std::move(*drawable));
  }
  (networkedAnimator.*apply)(part, std::move(drawables));
  return {};
}
}

std::expected<void, StarException> LuaBindings::Detail::registerAnimatorStateCallbacks(LuaCallbacks& callbacks, NetworkedAnimator& networkedAnimator, AnimatorStateCallbackOrder order) {
  // Preserve each table's historical order for these shared callbacks.
  if (auto result = callbacks.registerCallbackWithSignature<bool, String, String, bool, bool>(
      "setLocalAnimationState", std::bind_front(&NetworkedAnimator::setLocalState, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (order == AnimatorStateCallbackOrder::Networked) {
    if (auto result = callbacks.registerCallbackWithSignature<String, String>(
        "animationState", std::bind_front(&NetworkedAnimator::state, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  }
  if (auto result = callbacks.registerCallbackWithSignature<Json, String, String, std::optional<String>, std::optional<int>>(
      "animationStateProperty", std::bind_front(&NetworkedAnimator::stateProperty, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<Json, String, String>(
      "animationStateNextProperty", std::bind_front(&NetworkedAnimator::stateNextProperty, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (order == AnimatorStateCallbackOrder::Scripted) {
    if (auto result = callbacks.registerCallbackWithSignature<String, String>(
        "animationState", std::bind_front(&NetworkedAnimator::state, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  }
  if (auto result = callbacks.registerCallbackWithSignature<int, String>(
      "animationStateFrame", std::bind_front(&NetworkedAnimator::stateFrame, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<int, String>(
      "animationStateNextFrame", std::bind_front(&NetworkedAnimator::stateNextFrame, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<float, String>(
      "animationStateFrameProgress", std::bind_front(&NetworkedAnimator::stateFrameProgress, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<float, String>(
      "animationStateTimer", std::bind_front(&NetworkedAnimator::stateTimer, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<bool, String>(
      "animationStateReverse", std::bind_front(&NetworkedAnimator::stateReverse, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (order == AnimatorStateCallbackOrder::Networked) {
    if (auto result = callbacks.registerCallbackWithSignature<bool, String, std::optional<String>>(
        "hasState", std::bind_front(&NetworkedAnimator::hasState, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  }
  if (auto result = callbacks.registerCallbackWithSignature<float, String, std::optional<String>>(
      "stateCycle", std::bind_front(&NetworkedAnimator::stateCycle, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<int, String, std::optional<String>>(
      "stateFrames", std::bind_front(&NetworkedAnimator::stateFrames, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (order == AnimatorStateCallbackOrder::Scripted) {
    if (auto result = callbacks.registerCallbackWithSignature<bool, String, std::optional<String>>(
        "hasState", std::bind_front(&NetworkedAnimator::hasState, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> LuaBindings::Detail::registerAnimatorLocalTransformationCallbacks(LuaCallbacks& callbacks, NetworkedAnimator& networkedAnimator) {
  if (auto result = callbacks.registerCallbackWithSignature<void, String, Vec2F>("translateLocalTransformationGroup",
      std::bind_front(&NetworkedAnimator::translateLocalTransformationGroup, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("rotateLocalTransformationGroup",
      [&networkedAnimator](String const& transformationGroup, float rotation, std::optional<Vec2F> const& rotationCenter) {
        return networkedAnimator.rotateLocalTransformationGroup(transformationGroup, rotation, rotationCenter.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("rotateDegreesLocalTransformationGroup",
      [&networkedAnimator](String const& transformationGroup, float rotation, std::optional<Vec2F> const& rotationCenter) {
        return networkedAnimator.rotateLocalTransformationGroup(transformationGroup, rotation * std::numbers::pi / 180, rotationCenter.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("scaleLocalTransformationGroup",
      [&networkedAnimator](LuaEngine& engine, String const& transformationGroup, LuaValue scale, std::optional<Vec2F> const& scaleCenter) -> std::expected<void, StarException> {
        auto cs = engine.luaMaybeTo<Vec2F>(scale);
        if (!cs) return std::unexpected(std::move(cs.error()));
        if (*cs) return networkedAnimator.scaleLocalTransformationGroup(transformationGroup, **cs, scaleCenter.value_or(Vec2F{}));
        auto scalar = engine.luaTo<float>(scale);
        if (!scalar) return std::unexpected(std::move(scalar.error()));
        return networkedAnimator.scaleLocalTransformationGroup(transformationGroup, *scalar, scaleCenter.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, float, float, float, float, float, float>(
      "transformLocalTransformationGroup",
      std::bind_front(&NetworkedAnimator::transformLocalTransformationGroup, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String>(
      "resetLocalTransformationGroup", std::bind_front(&NetworkedAnimator::resetLocalTransformationGroup, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, Mat3F>(
      "setLocalTransformationGroup", std::bind_front(&NetworkedAnimator::setLocalTransformationGroup, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<Mat3F, String>(
      "getLocalTransformationGroup", std::bind_front(&NetworkedAnimator::getLocalTransformationGroup, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> LuaBindings::Detail::registerAnimatorPartQueryCallbacks(LuaCallbacks& callbacks, NetworkedAnimator& networkedAnimator) {
  if (auto result = callbacks.registerCallbackWithSignature<std::optional<Vec2F>, String, String>("partPoint", std::bind_front(&NetworkedAnimator::partPoint, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<std::optional<PolyF>, String, String>("partPoly", std::bind_front(&NetworkedAnimator::partPoly, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<Json, String, String, std::optional<String>, std::optional<String>, std::optional<int>>("partProperty", std::bind_front(&NetworkedAnimator::partProperty, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<Json, String, String>("partNextProperty", std::bind_front(&NetworkedAnimator::partNextProperty, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallback("transformPoint", [&networkedAnimator] (Vec2F point, String const& part) -> std::expected<Vec2F, StarException> {
      auto transformation = networkedAnimator.partTransformation(part);
      if (!transformation) return std::unexpected(std::move(transformation.error()));
      return transformation->transformVec2(point);
    }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("transformPoly", [&networkedAnimator] (PolyF poly, String const& part) -> std::expected<PolyF, StarException> {
      auto transformation = networkedAnimator.partTransformation(part);
      if (!transformation) return std::unexpected(std::move(transformation.error()));
      poly.transform(*transformation);
      return poly;
    }); !result) return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> LuaBindings::Detail::registerAnimatorPlaybackQueryCallbacks(LuaCallbacks& callbacks, NetworkedAnimator& networkedAnimator) {
  if (auto result = callbacks.registerCallback("flipped", [&networkedAnimator]() -> bool {
        return networkedAnimator.flipped();
    }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("flippedRelativeCenterLine", [&networkedAnimator]() -> float {
        return networkedAnimator.flippedRelativeCenterLine();
    }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("animationRate", [&networkedAnimator]() -> float {
        return networkedAnimator.animationRate();
    }); !result) return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> LuaBindings::Detail::registerAnimatorDrawableCallbacks(LuaCallbacks& callbacks, NetworkedAnimator& networkedAnimator) {
  if (auto result = callbacks.registerCallbackWithSignature<void, String, List<Drawable>>(
      "addPartDrawables", std::bind_front(&NetworkedAnimator::addPartDrawables, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, List<Drawable>>(
      "setPartDrawables", std::bind_front(&NetworkedAnimator::setPartDrawables, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("addPartJsonDrawables",
    [&networkedAnimator](LuaEngine& engine, String const& part, JsonArray drawablesConfig) -> std::expected<void, StarException> {
      return applyAnimatorJsonDrawables(networkedAnimator, &NetworkedAnimator::addPartDrawables, engine, part, drawablesConfig);
    }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setPartJsonDrawables",
    [&networkedAnimator](LuaEngine& engine, String const& part, JsonArray drawablesConfig) -> std::expected<void, StarException> {
      return applyAnimatorJsonDrawables(networkedAnimator, &NetworkedAnimator::setPartDrawables, engine, part, drawablesConfig);
    }); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<String, String, String>(
      "applyPartTags", std::bind_front(&NetworkedAnimator::applyPartTags, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, std::optional<String>>(
      "setLocalTag", std::bind_front(&NetworkedAnimator::setLocalTag, std::ref(networkedAnimator))); !result) return std::unexpected(std::move(result.error()));
  return {};
}


std::expected<LuaCallbacks, StarException> LuaBindings::makeNetworkedAnimatorCallbacks(NetworkedAnimator& networkedAnimator) {
  LuaCallbacks callbacks;

  if (auto result = callbacks.registerCallbackWithSignature<bool, String, String, bool, bool>(
        "setAnimationState",
        [&networkedAnimator](String const& type, String const& state, bool startNew, bool reverse) {
          return networkedAnimator.setState(type, state, startNew, reverse);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = Detail::registerAnimatorStateCallbacks(callbacks, networkedAnimator, Detail::AnimatorStateCallbackOrder::Networked); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<void, String, std::optional<String>>(
        "setGlobalTag",
        [&networkedAnimator](String&& name, std::optional<String>&& value) {
          return networkedAnimator.setGlobalTag(std::move(name), std::move(value));
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, String, std::optional<String>>(
        "setPartTag",
        [&networkedAnimator](String const& part, String&& name, std::optional<String>&& value) {
          return networkedAnimator.setPartTag(part, std::move(name), std::move(value));
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setFlipped",
      [&networkedAnimator](bool flipped, std::optional<float> relativeCenterLine) {
        networkedAnimator.setFlipped(flipped, relativeCenterLine.value_or(0.0f));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = Detail::registerAnimatorPlaybackQueryCallbacks(callbacks, networkedAnimator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<void, float>(
        "setAnimationRate", [&networkedAnimator](float rate) { return networkedAnimator.setAnimationRate(rate); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, float, bool>(
        "rotateGroup",
        [&networkedAnimator](String const& group, float angle, bool immediate) {
          return networkedAnimator.rotateGroup(group, angle, immediate);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<float, String>(
        "currentRotationAngle",
        [&networkedAnimator](String const& group) { return networkedAnimator.currentRotationAngle(group); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<bool, String>(
        "hasTransformationGroup",
        [&networkedAnimator](String const& group) { return networkedAnimator.hasTransformationGroup(group); });
      !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<void, String, Vec2F>(
        "translateTransformationGroup",
        [&networkedAnimator](String const& group, Vec2F const& translation) {
          return networkedAnimator.translateTransformationGroup(group, translation);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("rotateTransformationGroup",
      [&networkedAnimator](String const& transformationGroup, float rotation, std::optional<Vec2F> const& rotationCenter) {
        return networkedAnimator.rotateTransformationGroup(transformationGroup, rotation, rotationCenter.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("rotateDegreesTransformationGroup",
      [&networkedAnimator](String const& transformationGroup, float rotation, std::optional<Vec2F> const& rotationCenter) {
        return networkedAnimator.rotateTransformationGroup(transformationGroup, rotation * std::numbers::pi / 180, rotationCenter.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("scaleTransformationGroup",
      [&networkedAnimator](LuaEngine& engine, String const& transformationGroup, LuaValue scale, std::optional<Vec2F> const& scaleCenter) -> std::expected<void, StarException> {
        auto cs = engine.luaMaybeTo<Vec2F>(scale);
        if (!cs) return std::unexpected(std::move(cs.error()));
        if (*cs) return networkedAnimator.scaleTransformationGroup(transformationGroup, **cs, scaleCenter.value_or(Vec2F{}));
        auto scalar = engine.luaTo<float>(scale);
        if (!scalar) return std::unexpected(std::move(scalar.error()));
        return networkedAnimator.scaleTransformationGroup(transformationGroup, *scalar, scaleCenter.value_or(Vec2F{}));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, float, float, float, float, float, float>(
        "transformTransformationGroup",
        [&networkedAnimator](String const& group, float a, float b, float c, float d, float tx, float ty) {
          return networkedAnimator.transformTransformationGroup(group, a, b, c, d, tx, ty);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String>(
        "resetTransformationGroup",
        [&networkedAnimator](String const& group) { return networkedAnimator.resetTransformationGroup(group); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, Mat3F>(
        "setTransformationGroup",
        [&networkedAnimator](String const& group, Mat3F transform) {
          return networkedAnimator.setTransformationGroup(group, transform);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<Mat3F, String>(
        "getTransformationGroup",
        [&networkedAnimator](String const& group) { return networkedAnimator.getTransformationGroup(group); });
      !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = Detail::registerAnimatorLocalTransformationCallbacks(callbacks, networkedAnimator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<void, String, bool>(
        "setParticleEmitterActive",
        [&networkedAnimator](String const& emitter, bool active) {
          return networkedAnimator.setParticleEmitterActive(emitter, active);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, float>(
        "setParticleEmitterEmissionRate",
        [&networkedAnimator](String const& emitter, float rate) {
          return networkedAnimator.setParticleEmitterEmissionRate(emitter, rate);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, unsigned>(
        "setParticleEmitterBurstCount",
        [&networkedAnimator](String const& emitter, unsigned count) {
          return networkedAnimator.setParticleEmitterBurstCount(emitter, count);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, RectF>(
        "setParticleEmitterOffsetRegion",
        [&networkedAnimator](String const& emitter, RectF const& region) {
          return networkedAnimator.setParticleEmitterOffsetRegion(emitter, region);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String>(
        "burstParticleEmitter",
        [&networkedAnimator](String const& emitter) { return networkedAnimator.burstParticleEmitter(emitter); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, bool>(
        "setLightActive",
        [&networkedAnimator](String const& light, bool active) {
          return networkedAnimator.setLightActive(light, active);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, Vec2F>(
        "setLightPosition",
        [&networkedAnimator](String const& light, Vec2F position) {
          return networkedAnimator.setLightPosition(light, position);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, Color>(
        "setLightColor",
        [&networkedAnimator](String const& light, Color color) {
          return networkedAnimator.setLightColor(light, color);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, float>(
        "setLightPointAngle",
        [&networkedAnimator](String const& light, float angle) {
          return networkedAnimator.setLightPointAngle(light, angle);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<bool, String>(
        "hasSound", [&networkedAnimator](String const& sound) { return networkedAnimator.hasSound(sound); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, StringList>(
        "setSoundPool",
        [&networkedAnimator](String const& sound, StringList&& pool) {
          return networkedAnimator.setSoundPool(sound, std::move(pool));
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, Vec2F>(
        "setSoundPosition",
        [&networkedAnimator](String const& sound, Vec2F position) {
          return networkedAnimator.setSoundPosition(sound, position);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("playSound",
      [&networkedAnimator](String const& sound, std::optional<int> loops) {
        return networkedAnimator.playSound(sound, loops.value_or(0));
      }); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallback("setSoundVolume",
      [&networkedAnimator](String const& sound, float targetVolume, std::optional<float> rampTime) {
        return networkedAnimator.setSoundVolume(sound, targetVolume, rampTime.value_or(0));
      }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setSoundPitch",
      [&networkedAnimator](String const& sound, float targetPitch, std::optional<float> rampTime) {
        return networkedAnimator.setSoundPitchMultiplier(sound, targetPitch, rampTime.value_or(0));
      }); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallback("stopAllSounds",
      [&networkedAnimator](String const& sound, std::optional<float> rampTime) {
        return networkedAnimator.stopAllSounds(sound, rampTime.value_or(0.0f));
      }); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<void, String, bool>(
        "setEffectActive",
        [&networkedAnimator](String const& effect, bool active) {
          return networkedAnimator.setEffectEnabled(effect, active);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = Detail::registerAnimatorPartQueryCallbacks(callbacks, networkedAnimator); !result) return std::unexpected(std::move(result.error()));

  if (auto result = Detail::registerAnimatorDrawableCallbacks(callbacks, networkedAnimator); !result) return std::unexpected(std::move(result.error()));

  return callbacks;
}

}
