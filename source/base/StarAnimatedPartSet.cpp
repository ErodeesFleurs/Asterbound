import std;
import star.string;
import star.format;
import star.json;
import star.json_extra;
import star.ordered_map;
import star.matrix3;
import star.vector;
import star.animated_part_set;
import star.math_common;
import star.interpolation;

namespace Star {

AnimatedPartSet::AnimatedPartSet() {}

std::expected<AnimatedPartSet, StarException> AnimatedPartSet::create(Json config, std::uint8_t animatorVersion) {
  AnimatedPartSet result;
  result.m_animatorVersion = animatorVersion;
  auto stateTypes = config.get("stateTypes", JsonObject());
  if (!stateTypes) return std::unexpected(std::move(stateTypes.error()));
  auto stateTypeEntries = stateTypes->iterateObject();
  if (!stateTypeEntries) return std::unexpected(std::move(stateTypeEntries.error()));
  for (auto const& [stateTypeName, stateTypeConfig] : *stateTypeEntries) {
    if (animatorVersion > 0 && !stateTypeConfig.isType(Json::Type::Object))
      continue;
    StateType newStateType;
    auto priority = stateTypeConfig.getFloat("priority", 0.0f);
    if (!priority) return std::unexpected(std::move(priority.error()));
    newStateType.priority = *priority;
    auto enabled = stateTypeConfig.getBool("enabled", true);
    if (!enabled) return std::unexpected(std::move(enabled.error()));
    newStateType.enabled = *enabled;
    auto defaultState = stateTypeConfig.getString("default", "");
    if (!defaultState) return std::unexpected(std::move(defaultState.error()));
    newStateType.defaultState = std::move(*defaultState);
    auto properties = stateTypeConfig.getObject("properties", {});
    if (!properties) return std::unexpected(std::move(properties.error()));
    newStateType.stateTypeProperties = std::move(*properties);
    auto states = stateTypeConfig.get("states", JsonObject());
    if (!states) return std::unexpected(std::move(states.error()));
    auto stateEntries = states->iterateObject();
    if (!stateEntries) return std::unexpected(std::move(stateEntries.error()));
    for (auto const& [stateName, stateConfig] : *stateEntries) {
      if (animatorVersion > 0 && !stateConfig.isType(Json::Type::Object))
        continue;
      auto newState = std::make_shared<State>();
      auto frames = stateConfig.getInt("frames", 1);
      if (!frames) return std::unexpected(std::move(frames.error()));
      newState->frames = *frames;
      auto cycle = stateConfig.getFloat("cycle", 1.0f);
      if (!cycle) return std::unexpected(std::move(cycle.error()));
      newState->cycle = *cycle;
      auto modeName = stateConfig.getString("mode", "end");
      if (!modeName) return std::unexpected(std::move(modeName.error()));
      auto mode = stringToAnimationMode(*modeName);
      if (!mode) return std::unexpected(std::move(mode.error()));
      newState->animationMode = *mode;
      auto transition = stateConfig.getString("transition", "");
      if (!transition) return std::unexpected(std::move(transition.error()));
      newState->transitionState = std::move(*transition);
      auto stateProperties = stateConfig.getObject("properties", {});
      if (!stateProperties) return std::unexpected(std::move(stateProperties.error()));
      newState->stateProperties = std::move(*stateProperties);
      auto frameProperties = stateConfig.getObject("frameProperties", {});
      if (!frameProperties) return std::unexpected(std::move(frameProperties.error()));
      newState->stateFrameProperties = std::move(*frameProperties);
      newStateType.states[stateName] = std::move(newState);
    }
    newStateType.states.sortByKey();
    newStateType.activeState.stateTypeName = stateTypeName;
    newStateType.activeState.reverse = false;
    newStateType.activeStateDirty = true;
    if (newStateType.defaultState.empty() && !newStateType.states.empty())
      newStateType.defaultState = newStateType.states.begin()->first;
    result.m_stateTypes[stateTypeName] = std::move(newStateType);
  }
  result.m_stateTypes.sort([](auto const& a, auto const& b) {
    return b.second.priority < a.second.priority;
  });
  auto parts = config.get("parts", JsonObject());
  if (!parts) return std::unexpected(std::move(parts.error()));
  auto partEntries = parts->iterateObject();
  if (!partEntries) return std::unexpected(std::move(partEntries.error()));
  for (auto const& [partName, partConfig] : *partEntries) {
    if (animatorVersion > 0 && !partConfig.isType(Json::Type::Object))
      continue;
    Part newPart;
    auto properties = partConfig.getObject("properties", {});
    if (!properties) return std::unexpected(std::move(properties.error()));
    newPart.partProperties = std::move(*properties);
    auto partStates = partConfig.get("partStates", JsonObject());
    if (!partStates) return std::unexpected(std::move(partStates.error()));
    auto partStateTypes = partStates->iterateObject();
    if (!partStateTypes) return std::unexpected(std::move(partStateTypes.error()));
    for (auto const& [stateTypeName, stateTypeConfig] : *partStateTypes) {
      auto states = stateTypeConfig.toObject();
      if (!states) return std::unexpected(std::move(states.error()));
      for (auto const& [stateName, initialStateConfig] : *states) {
        auto stateConfig = initialStateConfig;
        if (animatorVersion > 0 && stateConfig.isType(Json::Type::String)) {
          auto name = stateConfig.toString();
          if (!name) return std::unexpected(std::move(name.error()));
          auto target = stateTypeConfig.get(*name);
          if (!target) return std::unexpected(std::move(target.error()));
          stateConfig = std::move(*target);
        }
        if (animatorVersion > 0 && !stateConfig.isType(Json::Type::Object))
          continue;
        auto properties = stateConfig.getObject("properties", {});
        if (!properties) return std::unexpected(std::move(properties.error()));
        auto frames = stateConfig.getObject("frameProperties", {});
        if (!frames) return std::unexpected(std::move(frames.error()));
        newPart.partStates[stateTypeName][stateName] = PartState{std::move(*properties), std::move(*frames)};
      }
    }
    newPart.activePart.partName = partName;
    newPart.activePart.setAnimationAffineTransform(Mat3F::identity());
    newPart.presentationPart = newPart.activePart;
    newPart.activePartDirty = true;
    result.m_parts[partName] = std::move(newPart);
  }
  for (auto const& pair : result.m_stateTypes) {
    auto active = result.setActiveState(pair.first, pair.second.defaultState, true, false);
    if (!active) return std::unexpected(std::move(active.error()));
  }
  return result;
}

StringList AnimatedPartSet::stateTypes() const {
  return m_stateTypes.keys();
}

std::expected<void, StarException> AnimatedPartSet::setStateTypeEnabled(String const& name, bool enabled) {
  auto stateType = m_stateTypes.get(name);
  if (!stateType) return std::unexpected(std::move(stateType.error()));
  if (stateType->get().enabled != enabled) {
    stateType->get().enabled = enabled;
    for (auto& pair : m_parts)
      pair.second.activePartDirty = true;
  }
  return {};
}

std::expected<void, StarException> AnimatedPartSet::setEnabledStateTypes(StringList const& names) {
  for (auto& pair : m_stateTypes)
    pair.second.enabled = false;
  for (auto const& name : names) {
    auto stateType = m_stateTypes.get(name);
    if (!stateType) return std::unexpected(std::move(stateType.error()));
    stateType->get().enabled = true;
  }
  for (auto& pair : m_parts)
    pair.second.activePartDirty = true;
  return {};
}

std::expected<bool, StarException> AnimatedPartSet::stateTypeEnabled(String const& name) const {
  auto stateType = m_stateTypes.get(name);
  if (!stateType) return std::unexpected(std::move(stateType.error()));
  return stateType->get().enabled;
}

std::expected<StringList, StarException> AnimatedPartSet::states(String const& name) const {
  auto stateType = m_stateTypes.get(name);
  if (!stateType) return std::unexpected(std::move(stateType.error()));
  return stateType->get().states.keys();
}

StringList AnimatedPartSet::partNames() const {
  return m_parts.keys();
}

float AnimatedPartSet::setPresentationInterpolation(float interpolation) const {
  auto previous = m_presentationInterpolation;
  m_presentationInterpolation = interpolation < 0 ? -1 : std::clamp(interpolation, 0.f, 1.f);
  if (m_presentationInterpolation < 0)
    return previous;
  for (auto const& pair : m_stateTypes) {
    pair.second.presentationStateDirty = true;
    pair.second.presentationStateInitialized = false;
  }
  for (auto const& pair : m_parts)
    pair.second.presentationPartDirty = true;
  return previous;
}

std::expected<bool, StarException> AnimatedPartSet::setPresentationState(String const& type, String const& name, bool alwaysStart, bool reverse) {
  auto entry = m_stateTypes.get(type);
  if (!entry) return std::unexpected(std::move(entry.error()));
  auto& stateType = entry->get();
  auto fresh = freshenActiveState(stateType);
  if (!fresh) return std::unexpected(std::move(fresh.error()));
  auto& active = stateType.presentationState;
  if (active.stateName == name && active.reverse == reverse && !alwaysStart)
    return false;
  auto state = stateType.states.get(name);
  if (!state) return std::unexpected(std::move(state.error()));
  active.stateName = name;
  active.timer = 0;
  active.reverse = reverse;
  stateType.presentationStateDirty = true;
  for (auto const& pair : m_parts)
    pair.second.presentationPartDirty = true;
  return true;
}

std::expected<bool, StarException> AnimatedPartSet::setActiveState(String const& stateTypeName, String const& stateName, bool alwaysStart, bool reverse) {
  auto found = m_stateTypes.get(stateTypeName);
  if (!found) return std::unexpected(std::move(found.error()));
  auto& stateType = found->get();
  if (stateType.activeState.stateName != stateName || alwaysStart || stateType.activeState.reverse != reverse) {
    auto state = stateType.states.get(stateName);
    if (!state) return std::unexpected(std::move(state.error()));
    stateType.activeState.stateName = stateName;
    stateType.activeState.timer = 0.0f;
    stateType.previousTimer = stateType.timerAdvance = 0;
    stateType.activeState.frameProgress = 0.0f;
    stateType.activeState.reverse = reverse;
    stateType.activeStateReference = std::cref(*state->get());
    stateType.activeStateDirty = true;
    for (auto& pair : m_parts)
      pair.second.activePartDirty = true;
    return true;
  }
  return false;
}

std::expected<void, StarException> AnimatedPartSet::restartState(String const& name) {
  auto stateType = m_stateTypes.get(name);
  if (!stateType) return std::unexpected(std::move(stateType.error()));
  stateType->get().activeState.timer = 0.0f;
  stateType->get().previousTimer = stateType->get().timerAdvance = 0;
  stateType->get().activeStateDirty = true;
  for (auto& pair : m_parts)
    pair.second.activePartDirty = true;
  return {};
}

std::expected<std::reference_wrapper<AnimatedPartSet::ActiveStateInformation const>, StarException> AnimatedPartSet::activeState(String const& name) const {
  auto stateType = m_stateTypes.get(name);
  if (!stateType) return std::unexpected(std::move(stateType.error()));
  auto fresh = freshenActiveState(stateType->get());
  if (!fresh) return std::unexpected(std::move(fresh.error()));
  return std::cref(m_presentationInterpolation >= 0 ? stateType->get().presentationState : stateType->get().activeState);
}

std::expected<std::reference_wrapper<AnimatedPartSet::ActivePartInformation const>, StarException> AnimatedPartSet::activePart(String const& name) const {
  auto part = m_parts.get(name);
  if (!part) return std::unexpected(std::move(part.error()));
  auto fresh = freshenActivePart(part->get());
  if (!fresh) return std::unexpected(std::move(fresh.error()));
  return std::cref(m_presentationInterpolation >= 0 ? part->get().presentationPart : part->get().activePart);
}

std::expected<std::reference_wrapper<AnimatedPartSet::State const>, StarException> AnimatedPartSet::getState(String const& type, String const& name) const {
  auto stateType = m_stateTypes.get(type);
  if (!stateType) return std::unexpected(std::move(stateType.error()));
  auto state = stateType->get().states.get(name);
  if (!state) return std::unexpected(std::move(state.error()));
  return std::cref(*state->get());
}

StringMap<AnimatedPartSet::Part> const& AnimatedPartSet::constParts() const {
  return m_parts;
}

StringMap<AnimatedPartSet::Part>& AnimatedPartSet::parts() {
  return m_parts;
}

std::expected<void, StarException> AnimatedPartSet::forEachActiveState(std::function<std::expected<void, StarException>(String const&, ActiveStateInformation const&)> callback) const {
  if (!callback) return std::unexpected(AnimatedPartSetException("Empty active-state callback"));
  for (auto const& p : m_stateTypes) {
    auto fresh = freshenActiveState(p.second);
    if (!fresh) return std::unexpected(std::move(fresh.error()));
    auto called = callback(p.first, m_presentationInterpolation >= 0 ? p.second.presentationState : p.second.activeState);
    if (!called) return std::unexpected(std::move(called.error()));
  }
  return {};
}

std::expected<void, StarException> AnimatedPartSet::forEachActivePart(std::function<std::expected<void, StarException>(String const&, ActivePartInformation const&)> callback) const {
  if (!callback) return std::unexpected(AnimatedPartSetException("Empty active-part callback"));
  for (auto const& p : m_parts) {
    auto fresh = freshenActivePart(p.second);
    if (!fresh) return std::unexpected(std::move(fresh.error()));
    auto called = callback(p.first, m_presentationInterpolation >= 0 ? p.second.presentationPart : p.second.activePart);
    if (!called) return std::unexpected(std::move(called.error()));
  }
  return {};
}

std::expected<std::size_t, StarException> AnimatedPartSet::activeStateIndex(String const& name) const {
  auto stateType = m_stateTypes.get(name);
  if (!stateType) return std::unexpected(std::move(stateType.error()));
  auto index = stateType->get().states.indexOf(stateType->get().activeState.stateName);
  if (!index) return std::unexpected(AnimatedPartSetException("Active animation state is missing"));
  return *index;
}

std::expected<bool, StarException> AnimatedPartSet::activeStateReverse(String const& name) const {
  auto stateType = m_stateTypes.get(name);
  if (!stateType) return std::unexpected(std::move(stateType.error()));
  return stateType->get().activeState.reverse;
}

std::expected<bool, StarException> AnimatedPartSet::setActiveStateIndex(String const& name, std::size_t index, bool alwaysStart, bool reverse) {
  auto stateType = m_stateTypes.get(name);
  if (!stateType) return std::unexpected(std::move(stateType.error()));
  auto stateName = stateType->get().states.keyAt(index);
  if (!stateName) return std::unexpected(std::move(stateName.error()));
  return setActiveState(name, stateName->get(), alwaysStart, reverse);
}

std::expected<void, StarException> AnimatedPartSet::update(float dt) {
  for (auto& pair : m_stateTypes) {
    auto& stateType = pair.second;
    auto const& state = stateType.activeStateReference->get();

    stateType.previousTimer = stateType.activeState.timer;
    stateType.timerAdvance = dt;
    stateType.activeState.timer += dt;
    if (stateType.activeState.timer > state.cycle) {
      if (state.animationMode == End) {
        stateType.activeState.timer = state.cycle;
      } else if (state.animationMode == Loop) {
        stateType.activeState.timer = std::fmod(stateType.activeState.timer, state.cycle);
      } else if (state.animationMode == Transition) {
        auto next = stateType.states.get(state.transitionState);
        if (!next) return std::unexpected(std::move(next.error()));
        stateType.activeState.stateName = state.transitionState;
        stateType.activeState.timer = 0.0f;
        stateType.activeStateReference = std::cref(*next->get());
        stateType.previousTimer = stateType.timerAdvance = 0;
      }
    }

    stateType.activeStateDirty = true;
  }

  for (auto& pair : m_parts)
    pair.second.activePartDirty = true;
  return {};
}

std::expected<void, StarException> AnimatedPartSet::finishAnimations() {
  for (auto& pair : m_stateTypes) {
    auto& stateType = pair.second;

    while (true) {
      auto const& state = stateType.activeStateReference->get();

      if (state.animationMode == End) {
        stateType.activeState.timer = state.cycle;
      } else if (state.animationMode == Transition) {
        auto next = stateType.states.get(state.transitionState);
        if (!next) return std::unexpected(std::move(next.error()));
        stateType.activeState.stateName = state.transitionState;
        stateType.activeState.timer = 0.0f;
        stateType.activeStateReference = std::cref(*next->get());
        continue;
      }
      break;
    }

    stateType.previousTimer = stateType.activeState.timer;
    stateType.timerAdvance = 0;
    stateType.activeStateDirty = true;
  }

  for (auto& pair : m_parts)
    pair.second.activePartDirty = true;
  return {};
}

std::expected<AnimatedPartSet::AnimationMode, StarException> AnimatedPartSet::stringToAnimationMode(String const& string) {
  auto end = string.equals("end", String::CaseInsensitive);
  if (!end) return std::unexpected(std::move(end.error()));
  if (*end) return End;
  auto loop = string.equals("loop", String::CaseInsensitive);
  if (!loop) return std::unexpected(std::move(loop.error()));
  if (*loop) return Loop;
  auto transition = string.equals("transition", String::CaseInsensitive);
  if (!transition) return std::unexpected(std::move(transition.error()));
  if (*transition) return Transition;
  return std::unexpected(AnimatedPartSetException(strf("No such AnimationMode '{}'", string)));
}

std::expected<void, StarException> AnimatedPartSet::freshenActiveState(StateType const& stateType) const {
  bool presentation = m_presentationInterpolation >= 0;
  auto& dirty = presentation ? stateType.presentationStateDirty : stateType.activeStateDirty;
  auto& activeState = presentation ? stateType.presentationState : stateType.activeState;
  if (presentation && !stateType.presentationStateInitialized) {
    activeState.stateTypeName = stateType.activeState.stateTypeName;
    activeState.stateName = stateType.activeState.stateName;
    activeState.reverse = stateType.activeState.reverse;
    auto const& state = stateType.activeStateReference->get();
    activeState.timer = stateType.activeState.timer;
    if (m_presentationInterpolation < 1) {
      activeState.timer = stateType.previousTimer + stateType.timerAdvance * m_presentationInterpolation;
      if (state.animationMode == Loop && state.cycle > 0 && activeState.timer > state.cycle)
        activeState.timer = std::fmod(activeState.timer, state.cycle);
      else
        activeState.timer = std::min(activeState.timer, state.cycle);
    }
    stateType.presentationStateInitialized = true;
  }
  if (dirty) {
    auto stateEntry = stateType.states.get(activeState.stateName);
    if (!stateEntry) return std::unexpected(std::move(stateEntry.error()));
    auto const& state = *stateEntry->get();
    double progress = (activeState.timer / state.cycle * state.frames);
    activeState.frameProgress = std::fmod(progress, 1);
    activeState.frame = clamp<int>(progress, 0, state.frames - 1);
    if (activeState.reverse) {
      activeState.frame = (state.frames - 1) - activeState.frame;
      if ((state.animationMode == Loop) && (activeState.frame <= 0)) {
        activeState.nextFrame = state.frames - 1;
      } else {
        activeState.nextFrame = clamp<int>(activeState.frame - 1, 0, state.frames - 1);
      }
    } else {
      if ((state.animationMode == Loop) && (activeState.frame >= (state.frames - 1))) {
        activeState.nextFrame = 0;
      } else {
        activeState.nextFrame = clamp<int>(activeState.frame + 1, 0, state.frames - 1);
      }
    }

    activeState.properties = stateType.stateTypeProperties;
    activeState.properties.merge(state.stateProperties, true);

    activeState.nextProperties = stateType.stateTypeProperties;
    activeState.nextProperties.merge(state.stateProperties, true);

    for (auto const& pair : state.stateFrameProperties) {
      auto size = pair.second.size();
      if (!size) return std::unexpected(std::move(size.error()));
      if (activeState.frame < *size) {
        auto value = pair.second.get(activeState.frame);
        if (!value) return std::unexpected(std::move(value.error()));
        activeState.properties[pair.first] = std::move(*value);
      }
      if (activeState.nextFrame < *size) {
        auto value = pair.second.get(activeState.nextFrame);
        if (!value) return std::unexpected(std::move(value.error()));
        activeState.nextProperties[pair.first] = std::move(*value);
      }
    }

    dirty = false;
  }
  return {};
}

std::expected<void, StarException> AnimatedPartSet::freshenActivePart(Part const& part) const {
  bool presentation = m_presentationInterpolation >= 0;
  auto& dirty = presentation ? part.presentationPartDirty : part.activePartDirty;
  if (dirty) {
    // Reuse a separate derived cache; never overwrite logical frame data.
    auto& activePart = presentation ? part.presentationPart : part.activePart;
    if (presentation)
      activePart.setAnimationAffineTransform(part.activePartDirty ? part.activePart.animationAffineTransform() : part.activePartBaseTransform);
    else
      part.activePartBaseTransform = activePart.animationAffineTransform();
    activePart.activeState = {};
    activePart.properties = part.partProperties;
    activePart.nextProperties = part.partProperties;

    // Then go through each of the state types and states and look for a part
    // state match in order of priority.
    for (auto const& stateTypePair : m_stateTypes) {
      auto const& stateTypeName = stateTypePair.first;
      auto const& stateType = stateTypePair.second;

      // Skip disabled state types
      if (!stateType.enabled)
        continue;

      auto partStateType = part.partStates.maybeRef(stateTypeName);
      if (!partStateType)
        continue;

      auto stateResult = activeState(stateTypeName);
      if (!stateResult) return std::unexpected(std::move(stateResult.error()));
      auto const& sampledState = stateResult->get();
      auto const& stateName = sampledState.stateName;
      auto partState = partStateType->get().maybeRef(stateName);
      if (!partState)
        continue;

      // If we have a partState match, then set the active state information.
      activePart.activeState = sampledState;
      unsigned frame = sampledState.frame;
      unsigned nextFrame = sampledState.nextFrame;

      // Then set the part state data, as well as any part state frame data if
      // the current frame is within the list size.
      activePart.properties.merge(partState->get().partStateProperties, true);

      activePart.nextProperties.merge(partState->get().partStateProperties, true);

      for (auto const& pair : partState->get().partStateFrameProperties) {
        auto size = pair.second.size();
        if (!size) return std::unexpected(std::move(size.error()));
        if (frame < *size) {
          auto value = pair.second.get(frame);
          if (!value) return std::unexpected(std::move(value.error()));
          activePart.properties[pair.first] = std::move(*value);
        }
        if (nextFrame < *size) {
          auto value = pair.second.get(nextFrame);
          if (!value) return std::unexpected(std::move(value.error()));
          activePart.nextProperties[pair.first] = std::move(*value);
        }
      }

      // Each part can only have one state type x state match, so we are done.
      break;
    }
    if (version() > 0) {
      auto processTransforms = [](Mat3F mat, Json const& transforms, JsonObject const& properties) -> std::expected<Mat3F, StarException> {
        auto entries = transforms.iterateArray();
        if (!entries) return std::unexpected(std::move(entries.error()));
        for (auto const& v : *entries) {
          auto action = v.getString(0);
          if (!action) return std::unexpected(std::move(action.error()));
          if (*action == "reset") {
            mat = Mat3F::identity();
          } else if (*action == "translate") {
            auto translation = v.get(1).and_then(jsonToVec2F);
            if (!translation) return std::unexpected(std::move(translation.error()));
            mat.translate(*translation);
          } else if (*action == "rotate" || *action == "rotateDegrees" || *action == "scale") {
            auto centerProperty = properties.maybeRef(*action == "scale" ? std::string_view("scalingCenter")
                                                                         : std::string_view("rotationCenter"));
            // Preserve validation of a supplied default even when an explicit center wins.
            if (centerProperty) {
              auto defaultCenter = centerProperty->get().arrayPtr();
              if (!defaultCenter)
                return std::unexpected(std::move(defaultCenter.error()));
            }
            auto centerValue = v.find(2);
            if (!centerValue)
              return std::unexpected(std::move(centerValue.error()));
            std::expected<Vec2F, StarException> center = Vec2F();
            if (*centerValue)
              center = jsonToVec2F((*centerValue)->get());
            else if (centerProperty)
              center = jsonToVec2F(centerProperty->get());
            if (!center) return std::unexpected(std::move(center.error()));
            if (*action == "scale") {
              auto scale = v.get(1).and_then(jsonToVec2F);
              if (!scale) return std::unexpected(std::move(scale.error()));
              mat.scale(*scale, *center);
            } else {
              auto angle = v.getFloat(1);
              if (!angle) return std::unexpected(std::move(angle.error()));
              mat.rotate(*action == "rotateDegrees" ? *angle * std::numbers::pi / 180 : *angle, *center);
            }
          } else if (*action == "transform") {
            std::array<float, 6> values;
            for (std::size_t i = 0; i < values.size(); ++i) {
              auto value = v.getFloat(i + 1);
              if (!value) return std::unexpected(std::move(value.error()));
              values[i] = *value;
            }
            mat = Mat3F(values[0], values[1], values[2], values[3], values[4], values[5], 0, 0, 1) * mat;
          }
        }
        return mat;
      };


      if (auto transforms = activePart.properties.maybeRef("transforms")) {
        auto mat = processTransforms(activePart.animationAffineTransform(), transforms->get(), activePart.properties);
        if (!mat) return std::unexpected(std::move(mat.error()));
        auto interpolated = activePart.properties.maybe("interpolated").value_or(false).toBool();
        if (!interpolated) return std::unexpected(std::move(interpolated.error()));
        if (*interpolated) {
          if (auto nextTransforms = activePart.nextProperties.maybeRef("transforms")) {
            auto nextMat = processTransforms(activePart.animationAffineTransform(), nextTransforms->get(), activePart.nextProperties);
            if (!nextMat) return std::unexpected(std::move(nextMat.error()));
            activePart.setAnimationAffineTransform(*mat, *nextMat, activePart.activeState ? activePart.activeState->frameProgress : 1);
          } else {
            activePart.setAnimationAffineTransform(*mat);
          }
        } else {
          activePart.setAnimationAffineTransform(*mat);
        }
      }
    }

    dirty = false;
  }
  return {};
}

void AnimatedPartSet::ActivePartInformation::setAnimationAffineTransform(Mat3F const& matrix) {
  xTranslationAnimation = matrix[0][2];
  yTranslationAnimation = matrix[1][2];
  xScaleAnimation = std::sqrt(square(matrix[0][0]) + square(matrix[0][1]));
  yScaleAnimation = std::sqrt(square(matrix[1][0]) + square(matrix[1][1]));
  xShearAnimation = std::atan2(matrix[0][1], matrix[0][0]);
  yShearAnimation = std::atan2(matrix[1][0], matrix[1][1]);
}
void AnimatedPartSet::ActivePartInformation::setAnimationAffineTransform(Mat3F const& mat1, Mat3F const& mat2, float progress) {
  xTranslationAnimation = lerp(progress, mat1[0][2], mat2[0][2]);
  yTranslationAnimation = lerp(progress, mat1[1][2], mat2[1][2]);
  xScaleAnimation = lerp(progress, std::sqrt(square(mat1[0][0]) + square(mat1[0][1])), std::sqrt(square(mat2[0][0]) + square(mat2[0][1])));
  yScaleAnimation = lerp(progress, std::sqrt(square(mat1[1][0]) + square(mat1[1][1])), std::sqrt(square(mat2[1][0]) + square(mat2[1][1])));
  xShearAnimation = angleLerp(progress, std::atan2(mat1[0][1], mat1[0][0]), std::atan2(mat2[0][1], mat2[0][0]));
  yShearAnimation = angleLerp(progress, std::atan2(mat1[1][0], mat1[1][1]), std::atan2(mat2[1][0], mat2[1][1]));
}

Mat3F AnimatedPartSet::ActivePartInformation::animationAffineTransform() const {
  return Mat3F(
      xScaleAnimation * std::cos(xShearAnimation), xScaleAnimation * std::sin(xShearAnimation), xTranslationAnimation,
      yScaleAnimation * std::sin(yShearAnimation), yScaleAnimation * std::cos(yShearAnimation), yTranslationAnimation,
      0, 0, 1
    );
}

std::uint8_t AnimatedPartSet::version() const {
  return m_animatorVersion;
}

std::expected<Json, StarException> AnimatedPartSet::getStateFrameProperty(String const& typeName, String const& propertyName, String stateName, int frame) const {
  auto type = m_stateTypes.get(typeName);
  if (!type) return std::unexpected(std::move(type.error()));
  auto state = type->get().states.get(stateName);
  if (!state) return std::unexpected(std::move(state.error()));
  if (auto property = state->get()->stateFrameProperties.maybeRef(propertyName)) {
    auto size = property->get().size();
    if (!size) return std::unexpected(std::move(size.error()));
    if (frame >= 0 && static_cast<std::size_t>(frame) < *size)
      return property->get().get(frame);
  }
  return state->get()->stateProperties.maybe(propertyName).value_or(type->get().stateTypeProperties.maybe(propertyName).value_or(Json()));
}

std::expected<Json, StarException> AnimatedPartSet::getPartStateFrameProperty(String const& partName, String const& propertyName, String const& typeName, String stateName, int frame) const {
  auto part = m_parts.get(partName);
  if (!part) return std::unexpected(std::move(part.error()));
  auto type = part->get().partStates.get(typeName);
  if (!type) return std::unexpected(std::move(type.error()));
  auto state = type->get().get(stateName);
  if (!state) return std::unexpected(std::move(state.error()));
  if (auto property = state->get().partStateFrameProperties.maybeRef(propertyName)) {
    auto size = property->get().size();
    if (!size) return std::unexpected(std::move(size.error()));
    if (frame >= 0 && static_cast<std::size_t>(frame) < *size)
      return property->get().get(frame);
  }
  return state->get().partStateProperties.maybe(propertyName).value_or(part->get().partProperties.maybe(propertyName).value_or(Json()));
}


}
