import star.json;
import star.json_extra;
import star.interpolation;
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
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.lexical_cast;
import star.data_stream_extra;
import star.game_types;

import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.asset_source;
import star.assets;
import star.image_metadata_database;
import star.root_base;
import star.configuration;
import star.root;

import star.particle_database;

import star.math_common;
import star.algorithm;
namespace Star {

namespace {
Mat3F sampleAffine(float alpha, Mat3F const& previous, Mat3F const& current) {
  if (alpha <= 0)
    return previous;
  if (alpha >= 1)
    return current;
  float xs = lerp(alpha, std::hypot(previous[0][0], previous[0][1]), std::hypot(current[0][0], current[0][1]));
  float ys = lerp(alpha, std::hypot(previous[1][0], previous[1][1]), std::hypot(current[1][0], current[1][1]));
  float xa = angleLerp(alpha, std::atan2(previous[0][1], previous[0][0]), std::atan2(current[0][1], current[0][0]));
  float ya = angleLerp(alpha, std::atan2(previous[1][0], previous[1][1]), std::atan2(current[1][0], current[1][1]));
  return Mat3F(xs * std::cos(xa), xs * std::sin(xa), lerp(alpha, previous[0][2], current[0][2]),
      ys * std::sin(ya), ys * std::cos(ya), lerp(alpha, previous[1][2], current[1][2]), 0, 0, 1);
}
    auto processTransforms = [](Mat3F mat, Json const& transforms, JsonObject const& properties) -> std::expected<Mat3F, StarException> {
      auto entries = transforms.iterateArray();
      if (!entries) return std::unexpected(std::move(entries.error()));
      for (auto const& v : *entries) {
        auto action = v.getString(0);
        if (!action) return std::unexpected(std::move(action.error()));
        if (*action == "reset") {
          mat = Mat3F::identity();
        } else if (*action == "translate") {
          auto offsetJson = v.getArray(1);
          if (!offsetJson) return std::unexpected(std::move(offsetJson.error()));
          auto offset = jsonToVec2F(*offsetJson);
          if (!offset) return std::unexpected(std::move(offset.error()));
          mat.translate(*offset);
        } else if (*action == "rotate" || *action == "rotateDegrees") {
          auto angle = v.getFloat(1);
          if (!angle) return std::unexpected(std::move(angle.error()));
          auto defaultCenter = properties.maybe("rotationCenter").value_or(JsonArray{0, 0}).toArray();
          if (!defaultCenter) return std::unexpected(std::move(defaultCenter.error()));
          auto centerJson = v.getArray(2, *defaultCenter);
          if (!centerJson) return std::unexpected(std::move(centerJson.error()));
          auto center = jsonToVec2F(*centerJson);
          if (!center) return std::unexpected(std::move(center.error()));
          mat.rotate(*action == "rotate" ? *angle : *angle * std::numbers::pi / 180, *center);
        } else if (*action == "scale") {
          auto scaleJson = v.getArray(1);
          if (!scaleJson) return std::unexpected(std::move(scaleJson.error()));
          auto scale = jsonToVec2F(*scaleJson);
          if (!scale) return std::unexpected(std::move(scale.error()));
          auto defaultCenter = properties.maybe("scalingCenter").value_or(JsonArray{0, 0}).toArray();
          if (!defaultCenter) return std::unexpected(std::move(defaultCenter.error()));
          auto centerJson = v.getArray(2, *defaultCenter);
          if (!centerJson) return std::unexpected(std::move(centerJson.error()));
          auto center = jsonToVec2F(*centerJson);
          if (!center) return std::unexpected(std::move(center.error()));
          mat.scale(*scale, *center);
        } else if (*action == "transform") {
          auto v1 = v.getFloat(1);
          if (!v1) return std::unexpected(std::move(v1.error()));
          auto v2 = v.getFloat(2);
          if (!v2) return std::unexpected(std::move(v2.error()));
          auto v3 = v.getFloat(3);
          if (!v3) return std::unexpected(std::move(v3.error()));
          auto v4 = v.getFloat(4);
          if (!v4) return std::unexpected(std::move(v4.error()));
          auto v5 = v.getFloat(5);
          if (!v5) return std::unexpected(std::move(v5.error()));
          auto v6 = v.getFloat(6);
          if (!v6) return std::unexpected(std::move(v6.error()));
          mat = Mat3F(*v1, *v2, *v3, *v4, *v5, *v6, 0, 0, 1) * mat;
        }
      }
      return mat;
    };
}

NetworkedAnimator::Sample::Sample(NetworkedAnimator const& animator, float interpolation)
  : m_presentation(nullptr), m_animator(animator), m_interpolation(std::clamp(interpolation, 0.f, 1.f)) {
  for (auto const& slot : animator.m_presentations) {
    if (!slot->users) {
      m_presentation = slot.get();
      break;
    }
  }
  if (!m_presentation) {
    animator.m_presentations.push_back(std::make_unique<Presentation>());
    m_presentation = animator.m_presentations.back().get();
  }
  auto& presentation = *m_presentation;
  animator.m_animatedParts.prepareSample(presentation.parts, m_interpolation);
  presentation.partTransforms.resize(presentation.parts.parts.size());
  presentation.invalidatePartTransforms();
  presentation.groups.resize(animator.m_transformationGroups.size());
  for (auto&& tag : presentation.tags)
    tag.second.active = false;
  for (auto&& drawables : presentation.drawables)
    drawables.second.active = false;
  std::size_t index = 0;
  for (auto const& pair : animator.m_transformationGroups) {
    auto const& group = pair.second;
    group.presentationIndex = index;
    auto& cache = presentation.groups[index++];
    cache.localTransform = group.localTransform;
    cache.transform = group.affineTransform();
    cache.animationDirty = true;
    if (group.presentationHistory) {
      cache.transform = sampleAffine(m_interpolation, group.previousTransform, cache.transform);
      cache.localTransform = sampleAffine(m_interpolation, group.previousLocalTransform, group.localTransform);
    }
  }
  presentation.users = 1;
}

NetworkedAnimator::Sample::Sample(Sample const& sample)
  : m_presentation(sample.m_presentation), m_animator(sample.m_animator), m_interpolation(sample.m_interpolation) {
  ++m_presentation->users;
}

NetworkedAnimator::Sample::~Sample() {
  --m_presentation->users;
}

NetworkedAnimator::Sample NetworkedAnimator::sample(float interpolation) const {
  return Sample(*this, interpolation);
}

std::expected<String, StarException> NetworkedAnimator::state(String const& stateType) const {
  return queryState(stateType, nullptr);
}

std::expected<String, StarException> NetworkedAnimator::Sample::state(String const& stateType) const {
  return m_animator.queryState(stateType, this);
}

std::expected<int, StarException> NetworkedAnimator::stateFrame(String const& stateType) const {
  return queryStateFrame(stateType, nullptr);
}

std::expected<int, StarException> NetworkedAnimator::Sample::stateFrame(String const& stateType) const {
  return m_animator.queryStateFrame(stateType, this);
}

std::expected<int, StarException> NetworkedAnimator::stateNextFrame(String const& stateType) const {
  return queryStateNextFrame(stateType, nullptr);
}

std::expected<int, StarException> NetworkedAnimator::Sample::stateNextFrame(String const& stateType) const {
  return m_animator.queryStateNextFrame(stateType, this);
}

std::expected<float, StarException> NetworkedAnimator::stateFrameProgress(String const& stateType) const {
  return queryStateFrameProgress(stateType, nullptr);
}

std::expected<float, StarException> NetworkedAnimator::Sample::stateFrameProgress(String const& stateType) const {
  return m_animator.queryStateFrameProgress(stateType, this);
}

std::expected<float, StarException> NetworkedAnimator::stateTimer(String const& stateType) const {
  return queryStateTimer(stateType, nullptr);
}

std::expected<float, StarException> NetworkedAnimator::Sample::stateTimer(String const& stateType) const {
  return m_animator.queryStateTimer(stateType, this);
}

std::expected<bool, StarException> NetworkedAnimator::stateReverse(String const& stateType) const {
  return queryStateReverse(stateType, nullptr);
}

std::expected<bool, StarException> NetworkedAnimator::Sample::stateReverse(String const& stateType) const {
  return m_animator.queryStateReverse(stateType, this);
}

std::expected<float, StarException> NetworkedAnimator::stateCycle(String const& stateType, std::optional<String> state) const {
  return queryStateCycle(stateType, std::move(state), nullptr);
}

std::expected<float, StarException> NetworkedAnimator::Sample::stateCycle(String const& stateType, std::optional<String> state) const {
  return m_animator.queryStateCycle(stateType, std::move(state), this);
}

std::expected<int, StarException> NetworkedAnimator::stateFrames(String const& stateType, std::optional<String> state) const {
  return queryStateFrames(stateType, std::move(state), nullptr);
}

std::expected<int, StarException> NetworkedAnimator::Sample::stateFrames(String const& stateType, std::optional<String> state) const {
  return m_animator.queryStateFrames(stateType, std::move(state), this);
}

std::expected<Json, StarException> NetworkedAnimator::stateProperty(String const& stateType, String const& propertyName, std::optional<String> state, std::optional<int> frame) const {
  return queryStateProperty(stateType, propertyName, std::move(state), std::move(frame), nullptr);
}

std::expected<Json, StarException> NetworkedAnimator::Sample::stateProperty(String const& stateType, String const& propertyName, std::optional<String> state, std::optional<int> frame) const {
  return m_animator.queryStateProperty(stateType, propertyName, std::move(state), std::move(frame), this);
}

std::expected<Json, StarException> NetworkedAnimator::stateNextProperty(String const& stateType, String const& propertyName) const {
  return queryStateNextProperty(stateType, propertyName, nullptr);
}

std::expected<Json, StarException> NetworkedAnimator::Sample::stateNextProperty(String const& stateType, String const& propertyName) const {
  return m_animator.queryStateNextProperty(stateType, propertyName, this);
}

std::expected<Json, StarException> NetworkedAnimator::partProperty(String const& partName, String const& propertyName, std::optional<String> stateType, std::optional<String> state, std::optional<int> frame) const {
  return queryPartProperty(partName, propertyName, std::move(stateType), std::move(state), std::move(frame), nullptr);
}

std::expected<Json, StarException> NetworkedAnimator::Sample::partProperty(String const& partName, String const& propertyName, std::optional<String> stateType, std::optional<String> state, std::optional<int> frame) const {
  return m_animator.queryPartProperty(partName, propertyName, std::move(stateType), std::move(state), std::move(frame), this);
}

std::expected<Json, StarException> NetworkedAnimator::partNextProperty(String const & partName, String const & propertyName) const {
  return queryPartNextProperty(partName, propertyName, nullptr);
}

std::expected<Json, StarException> NetworkedAnimator::Sample::partNextProperty(String const & partName, String const & propertyName) const {
  return m_animator.queryPartNextProperty(partName, propertyName, this);
}

std::expected<Mat3F, StarException> NetworkedAnimator::groupTransformation(StringList const& transformationGroups) const {
  return queryGroupTransformation(transformationGroups, nullptr);
}

std::expected<Mat3F, StarException> NetworkedAnimator::Sample::groupTransformation(StringList const& transformationGroups) const {
  return m_animator.queryGroupTransformation(transformationGroups, this);
}

std::expected<Mat3F, StarException> NetworkedAnimator::partTransformation(String const& partName) const {
  return queryPartTransformation(partName, nullptr);
}

std::expected<Mat3F, StarException> NetworkedAnimator::Sample::partTransformation(String const& partName) const {
  return m_animator.queryPartTransformation(partName, this);
}

std::expected<Mat3F, StarException> NetworkedAnimator::finalPartTransformation(String const& partName) const {
  return queryFinalPartTransformation(partName, nullptr);
}

std::expected<Mat3F, StarException> NetworkedAnimator::Sample::finalPartTransformation(String const& partName) const {
  return m_animator.queryFinalPartTransformation(partName, this);
}

std::expected<std::optional<Vec2F>, StarException> NetworkedAnimator::partPoint(String const& partName, String const& propertyName) const {
  return queryPartPoint(partName, propertyName, nullptr);
}

std::expected<std::optional<Vec2F>, StarException> NetworkedAnimator::Sample::partPoint(String const& partName, String const& propertyName) const {
  return m_animator.queryPartPoint(partName, propertyName, this);
}

std::expected<std::optional<PolyF>, StarException> NetworkedAnimator::partPoly(String const& partName, String const& propertyName) const {
  return queryPartPoly(partName, propertyName, nullptr);
}

std::expected<std::optional<PolyF>, StarException> NetworkedAnimator::Sample::partPoly(String const& partName, String const& propertyName) const {
  return m_animator.queryPartPoly(partName, propertyName, this);
}

std::expected<String, StarException> NetworkedAnimator::applyPartTags(String const& partName, String apply) const {
  return queryApplyPartTags(partName, std::move(apply), nullptr);
}

std::expected<String, StarException> NetworkedAnimator::Sample::applyPartTags(String const& partName, String apply) const {
  return m_animator.queryApplyPartTags(partName, std::move(apply), this);
}

std::expected<float, StarException> NetworkedAnimator::currentRotationAngle(String const& rotationGroup) const {
  return queryCurrentRotationAngle(rotationGroup, nullptr);
}

std::expected<float, StarException> NetworkedAnimator::Sample::currentRotationAngle(String const& rotationGroup) const {
  return m_animator.queryCurrentRotationAngle(rotationGroup, this);
}

std::expected<List<Drawable>, StarException> NetworkedAnimator::drawables(Vec2F const& translate) const {
  return queryDrawables(translate, nullptr);
}

std::expected<List<Drawable>, StarException> NetworkedAnimator::Sample::drawables(Vec2F const& translate) const {
  return m_animator.queryDrawables(translate, this);
}

std::expected<List<std::pair<Drawable, float>>, StarException> NetworkedAnimator::drawablesWithZLevel(Vec2F const& translate) const {
  return queryDrawablesWithZLevel(translate, nullptr);
}

std::expected<List<std::pair<Drawable, float>>, StarException> NetworkedAnimator::Sample::drawablesWithZLevel(Vec2F const& translate) const {
  return m_animator.queryDrawablesWithZLevel(translate, this);
}

std::expected<List<LightSource>, StarException> NetworkedAnimator::lightSources(Vec2F const& translate) const {
  return queryLightSources(translate, nullptr);
}

std::expected<List<LightSource>, StarException> NetworkedAnimator::Sample::lightSources(Vec2F const& translate) const {
  return m_animator.queryLightSources(translate, this);
}

std::expected<bool, StarException> NetworkedAnimator::Sample::hasState(String const& type, std::optional<String> const& state) const {
  return m_animator.hasState(type, state);
}

std::expected<bool, StarException> NetworkedAnimator::Sample::setLocalState(String const& type, String const& state, bool startNew, bool reverse) {
  auto changed = m_animator.m_animatedParts.setPresentationState(type, state, startNew, reverse, m_presentation->parts);
  if (changed && *changed) {
    m_presentation->invalidatePartTransforms();
    for (auto& group : m_presentation->groups)
      group.animationDirty = true;
  }
  return changed;
}

void NetworkedAnimator::Sample::setLocalTag(String name, std::optional<String> value) {
  m_presentation->tags.set(std::move(name), {true, std::move(value)});
}

void NetworkedAnimator::Sample::setPartDrawables(String const& name, List<Drawable> const* drawables) {
  m_presentation->drawables.set(name, {true, drawables});
}

std::expected<void, StarException> NetworkedAnimator::Sample::resetLocalTransformationGroup(String const& name) {
  auto group = m_animator.m_transformationGroups.get(name);
  if (!group) return std::unexpected(std::move(group.error()));
  m_presentation->groups[group->get().presentationIndex].localTransform = Mat3F::identity();
  m_presentation->invalidatePartTransforms();
  return {};
}

std::expected<void, StarException> NetworkedAnimator::Sample::rotateLocalTransformationGroup(String const& name, float rotation, Vec2F const& center) {
  auto group = m_animator.m_transformationGroups.get(name);
  if (!group) return std::unexpected(std::move(group.error()));
  auto& matrix = m_presentation->groups[group->get().presentationIndex].localTransform;
  matrix = Mat3F::rotation(rotation, center) * matrix;
  m_presentation->invalidatePartTransforms();
  return {};
}

std::expected<void, StarException> NetworkedAnimator::Sample::translateLocalTransformationGroup(String const& name, Vec2F const& translation) {
  auto group = m_animator.m_transformationGroups.get(name);
  if (!group) return std::unexpected(std::move(group.error()));
  auto& matrix = m_presentation->groups[group->get().presentationIndex].localTransform;
  matrix = Mat3F::translation(translation) * matrix;
  m_presentation->invalidatePartTransforms();
  return {};
}

std::expected<Mat3F, StarException> NetworkedAnimator::Sample::getLocalTransformationGroup(String const& name) const {
  auto group = m_animator.m_transformationGroups.get(name);
  if (!group) return std::unexpected(std::move(group.error()));
  return m_presentation->groups[group->get().presentationIndex].localTransform;
}

List<Drawable> const* NetworkedAnimator::partDrawables(String const& name, Sample const* sample) const {
  if (sample) {
    if (auto override = sample->m_presentation->drawables.find_ref(name); override && override->get().active)
      return override->get().drawables;
  }
  if (auto drawables = m_partDrawables.find_ref(name))
    return &drawables->get();
  return nullptr;
}

NetworkedAnimator::DynamicTarget::~DynamicTarget() {
  stopAudio();
}

List<std::shared_ptr<AudioInstance>> NetworkedAnimator::DynamicTarget::pullNewAudios() {
  pendingAudios.exec([this](std::shared_ptr<AudioInstance> const& ptr) {
    Vec2F audioBasePosition = ptr->position().value_or(Vec2F{});
    currentAudioBasePositions[ptr] = audioBasePosition;
    ptr->setPosition(position + audioBasePosition);
  });
  return std::exchange(pendingAudios, {});
}

List<Particle> NetworkedAnimator::DynamicTarget::pullNewParticles() {
  pendingParticles.exec([this](Particle& particle) {
    particle.position += position;
    return particle;
  });
  return std::exchange(pendingParticles, {});
}

void NetworkedAnimator::DynamicTarget::stopAudio() {
  for (auto const& pair : currentAudioBasePositions) {
    if (pair.first->loops() != 0)
      pair.first->stop();
  }
}

void NetworkedAnimator::DynamicTarget::updatePosition(Vec2F const& p) {
  clearFinishedAudio();
  position = p;
  for (auto&& audioPair : currentAudioBasePositions)
    audioPair.first->setPosition(audioPair.second + p);
}

void NetworkedAnimator::DynamicTarget::clearFinishedAudio() {
  for (auto&& p : statePersistentSounds) {
    if (p.second.audio && p.second.audio->finished())
      p.second.audio.reset();
  }

  for (auto&& p : stateImmediateSounds) {
    if (p.second.audio && p.second.audio->finished())
      p.second.audio.reset();
  }

  for (auto&& p : independentSounds)
    p.second.filter([](std::shared_ptr<AudioInstance> const& audio) { return !audio->finished(); });

  eraseWhere(currentAudioBasePositions, [](std::pair<std::shared_ptr<AudioInstance>, Vec2F> const& pair) {
      return pair.first->finished();
    });
}

NetworkedAnimator::NetworkedAnimator(Root& root) : m_root(root) {
  m_zoom.set(1.0f);
  m_flipped.set(false);
  m_flippedRelativeCenterLine.set(0.0f);
  m_animationRate.set(1.0f);
  m_animatorVersion = 0;
}

std::expected<std::shared_ptr<NetworkedAnimator>, StarException> NetworkedAnimator::create(Root& root, Json config, String relativePath) {
  auto animator = std::shared_ptr<NetworkedAnimator>(new NetworkedAnimator(root));
  auto loaded = animator->loadConfig(std::move(config), std::move(relativePath));
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  return animator;
}

std::expected<void, StarException> NetworkedAnimator::loadConfig(Json config, String relativePath) {
  auto& root = m_root.get();
  if (config.isNull()) return setupNetStates();
  if (config.type() == Json::Type::String) {
    auto path = config.toString();
    if (!path) return std::unexpected(std::move(path.error()));
    if (relativePath.empty()) relativePath = *path;
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto loaded = (*assets)->json(AssetPath::relative_to(relativePath, *path));
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    config = std::move(*loaded);
  } else if (relativePath.empty()) {
    relativePath = "/";
  }
  auto versionValue = config.getUInt("version", 0);
  if (!versionValue) return std::unexpected(std::move(versionValue.error()));
  m_animatorVersion = *versionValue;
  if (version() > 0) {
    auto hasIncludes = config.contains("includes");
    if (!hasIncludes) return std::unexpected(std::move(hasIncludes.error()));
    if (*hasIncludes) {
      auto includes = config.get("includes");
      if (!includes) return std::unexpected(std::move(includes.error()));
      auto merged = mergeIncludes(root, config, *includes, relativePath);
      if (!merged) return std::unexpected(std::move(merged.error()));
      config = std::move(*merged);
    }
  }
  auto partsConfig = config.get("animatedParts", JsonObject());
  if (!partsConfig) return std::unexpected(std::move(partsConfig.error()));
  auto parts = AnimatedPartSet::create(*partsConfig, version());
  if (!parts) return std::unexpected(std::move(parts.error()));
  m_animatedParts = std::move(*parts);
  m_animationStateTypes = m_animatedParts.stateTypes();
  m_relativePath = AssetPath::directory(relativePath);
  auto globalTagDefaultsConfig = config.get("globalTagDefaults", JsonObject());
  if (!globalTagDefaultsConfig) return std::unexpected(std::move(globalTagDefaultsConfig.error()));
  auto globalTagDefaultsEntries = globalTagDefaultsConfig->iterateObject();
  if (!globalTagDefaultsEntries) return std::unexpected(std::move(globalTagDefaultsEntries.error()));
  for (auto const& pair : *globalTagDefaultsEntries) {
    auto value = pair.second.toString();
    if (!value) return std::unexpected(std::move(value.error()));
    setGlobalTag(pair.first, *value);
  }
  auto partTagDefaultsConfig = config.get("partTagDefaults", JsonObject());
  if (!partTagDefaultsConfig) return std::unexpected(std::move(partTagDefaultsConfig.error()));
  auto partTagDefaultsEntries = partTagDefaultsConfig->iterateObject();
  if (!partTagDefaultsEntries) return std::unexpected(std::move(partTagDefaultsEntries.error()));
  for (auto const& pair : *partTagDefaultsEntries) {
    auto tags = pair.second.iterateObject();
    if (!tags) return std::unexpected(std::move(tags.error()));
    for (auto const& tag : *tags) {
      auto value = tag.second.toString();
      if (!value) return std::unexpected(std::move(value.error()));
      setPartTag(pair.first, tag.first, *value);
    }
  }
  auto transformationGroupsConfig = config.get("transformationGroups", JsonObject());
  if (!transformationGroupsConfig) return std::unexpected(std::move(transformationGroupsConfig.error()));
  auto transformationGroupsEntries = transformationGroupsConfig->iterateObject();
  if (!transformationGroupsEntries) return std::unexpected(std::move(transformationGroupsEntries.error()));
  for (auto const& pair : *transformationGroupsEntries) {
    auto& tg = m_transformationGroups[pair.first];
    auto interpolated = pair.second.getBool("interpolated", false);
    if (!interpolated) return std::unexpected(std::move(interpolated.error()));
    tg.interpolated = *interpolated;
    tg.setAffineTransform(Mat3F::identity());
    tg.setAnimationAffineTransform(Mat3F::identity());
    tg.setLocalAffineTransform(Mat3F::identity());
  }
  auto rotationGroupsConfig = config.get("rotationGroups", JsonObject());
  if (!rotationGroupsConfig) return std::unexpected(std::move(rotationGroupsConfig.error()));
  auto rotationGroupsEntries = rotationGroupsConfig->iterateObject();
  if (!rotationGroupsEntries) return std::unexpected(std::move(rotationGroupsEntries.error()));
  for (auto const& pair : *rotationGroupsEntries) {
    auto& rotationGroup = m_rotationGroups[pair.first];
    auto velocity = pair.second.getFloat("angularVelocity", 0.0f);
    if (!velocity) return std::unexpected(std::move(velocity.error()));
    rotationGroup.angularVelocity = std::move(*velocity);
    auto centerJson = pair.second.get("rotationCenter", JsonArray{0, 0});
    if (!centerJson) return std::unexpected(std::move(centerJson.error()));
    auto center = jsonToVec2F(*centerJson);
    if (!center) return std::unexpected(std::move(center.error()));
    rotationGroup.rotationCenter = std::move(*center);
  }
  auto particleEmittersConfig = config.get("particleEmitters", JsonObject());
  if (!particleEmittersConfig) return std::unexpected(std::move(particleEmittersConfig.error()));
  auto particleEmittersEntries = particleEmittersConfig->iterateObject();
  if (!particleEmittersEntries) return std::unexpected(std::move(particleEmittersEntries.error()));
  for (auto const& pair : *particleEmittersEntries) {
    auto& emitter = m_particleEmitters[pair.first];
    auto const& particleEmitterConfig = pair.second;
    auto rate = particleEmitterConfig.getFloat("emissionRate", 1.0f);
    if (!rate) return std::unexpected(std::move(rate.error()));
    emitter.emissionRate.set(*rate);
    auto variance = particleEmitterConfig.getFloat("emissionRateVariance", 0.0f);
    if (!variance) return std::unexpected(std::move(variance.error()));
    emitter.emissionRateVariance = std::move(*variance);
    RectF offsetRegion = RectF::null();
    auto region = particleEmitterConfig.opt("offsetRegion");
    if (!region) return std::unexpected(std::move(region.error()));
    if (*region) {
      auto regionValue = jsonToRectF(**region);
      if (!regionValue) return std::unexpected(std::move(regionValue.error()));
      offsetRegion = *regionValue;
    }
    emitter.offsetRegion.set(offsetRegion);
    auto anchor = particleEmitterConfig.optString("anchorPart");
    if (!anchor) return std::unexpected(std::move(anchor.error()));
    emitter.anchorPart = std::move(*anchor);
    auto groupsJson = particleEmitterConfig.get("transformationGroups", JsonArray());
    if (!groupsJson) return std::unexpected(std::move(groupsJson.error()));
    auto groups = jsonToStringList(*groupsJson);
    if (!groups) return std::unexpected(std::move(groups.error()));
    emitter.transformationGroups = std::move(*groups);
    auto rotation = particleEmitterConfig.optString("rotationGroup");
    if (!rotation) return std::unexpected(std::move(rotation.error()));
    emitter.rotationGroup = std::move(*rotation);
    auto center = particleEmitterConfig.opt("rotationCenter");
    if (!center) return std::unexpected(std::move(center.error()));
    if (*center) {
      auto centerValue = jsonToVec2F(**center);
      if (!centerValue) return std::unexpected(std::move(centerValue.error()));
      emitter.rotationCenter = *centerValue;
    }
    auto particlesJson = particleEmitterConfig.get("particles");
    if (!particlesJson) return std::unexpected(std::move(particlesJson.error()));
    auto particles = particlesJson->iterateArray();
    if (!particles) return std::unexpected(std::move(particles.error()));
    for (auto const& particleConfig : *particles) {
      auto database = root.particleDatabase();
      if (!database) return std::unexpected(std::move(database.error()));
      auto particle = particleConfig.get("particle");
      if (!particle) return std::unexpected(std::move(particle.error()));
      auto creator = (*database)->particleCreator(*particle, relativePath);
      if (!creator) return std::unexpected(std::move(creator.error()));
      auto count = particleConfig.getUInt("count", 1);
      if (!count) return std::unexpected(std::move(count.error()));
      auto offsetJson = particleConfig.get("offset", JsonArray{0, 0});
      if (!offsetJson) return std::unexpected(std::move(offsetJson.error()));
      auto offset = jsonToVec2F(*offsetJson);
      if (!offset) return std::unexpected(std::move(offset.error()));
      auto flip = particleConfig.getBool("flip", false);
      if (!flip) return std::unexpected(std::move(flip.error()));
      emitter.particleList.append({*creator, static_cast<unsigned>(*count), *offset, *flip});
    }
    auto burstCount = particleEmitterConfig.getUInt("burstCount", 1);
    if (!burstCount) return std::unexpected(std::move(burstCount.error()));
    emitter.burstCount.set(*burstCount);
    auto randomCount = particleEmitterConfig.getUInt("randomSelectCount", emitter.particleList.size());
    if (!randomCount) return std::unexpected(std::move(randomCount.error()));
    emitter.randomSelectCount.set(*randomCount);
    auto active = particleEmitterConfig.getBool("active", false);
    if (!active) return std::unexpected(std::move(active.error()));
    emitter.active.set(*active);
  }
  auto lightsConfig = config.get("lights", JsonObject());
  if (!lightsConfig) return std::unexpected(std::move(lightsConfig.error()));
  auto lightsEntries = lightsConfig->iterateObject();
  if (!lightsEntries) return std::unexpected(std::move(lightsEntries.error()));
  for (auto const& pair : *lightsEntries) {
    auto& light = m_lights[pair.first];
    auto const& lightConfig = pair.second;
    auto active = lightConfig.getBool("active", true);
    if (!active) return std::unexpected(std::move(active.error()));
    light.active.set(*active);
    Vec2F lightPosition{};
    auto position = lightConfig.opt("position");
    if (!position) return std::unexpected(std::move(position.error()));
    if (*position) {
      auto positionValue = jsonToVec2F(**position);
      if (!positionValue) return std::unexpected(std::move(positionValue.error()));
      lightPosition = *positionValue;
    }
    light.xPosition.set(lightPosition[0]);
    light.yPosition.set(lightPosition[1]);
    Color color = Color::White;
    auto colorConfig = lightConfig.opt("color");
    if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
    if (*colorConfig) {
      auto colorConfigValue = jsonToColor(**colorConfig);
      if (!colorConfigValue) return std::unexpected(std::move(colorConfigValue.error()));
      color = *colorConfigValue;
    }
    light.color.set(color);
    auto anchor = lightConfig.optString("anchorPart");
    if (!anchor) return std::unexpected(std::move(anchor.error()));
    light.anchorPart = std::move(*anchor);
    auto groupsJson = lightConfig.get("transformationGroups", JsonArray());
    if (!groupsJson) return std::unexpected(std::move(groupsJson.error()));
    auto groups = jsonToStringList(*groupsJson);
    if (!groups) return std::unexpected(std::move(groups.error()));
    light.transformationGroups = std::move(*groups);
    auto rotation = lightConfig.optString("rotationGroup");
    if (!rotation) return std::unexpected(std::move(rotation.error()));
    light.rotationGroup = std::move(*rotation);
    auto center = lightConfig.opt("rotationCenter");
    if (!center) return std::unexpected(std::move(center.error()));
    if (*center) {
      auto centerValue = jsonToVec2F(**center);
      if (!centerValue) return std::unexpected(std::move(centerValue.error()));
      light.rotationCenter = *centerValue;
    }
    auto hasFlicker = lightConfig.contains("flickerPeriod");
    if (!hasFlicker) return std::unexpected(std::move(hasFlicker.error()));
    if (*hasFlicker) {
      auto period = lightConfig.getFloat("flickerPeriod");
      if (!period) return std::unexpected(std::move(period.error()));
      auto minimum = lightConfig.getFloat("flickerMinIntensity", 0.0);
      if (!minimum) return std::unexpected(std::move(minimum.error()));
      auto maximum = lightConfig.getFloat("flickerMaxIntensity", 0.0);
      if (!maximum) return std::unexpected(std::move(maximum.error()));
      auto periodVariance = lightConfig.getFloat("flickerPeriodVariance", 0.0);
      if (!periodVariance) return std::unexpected(std::move(periodVariance.error()));
      auto intensityVariance = lightConfig.getFloat("flickerIntensityVariance", 0.0);
      if (!intensityVariance) return std::unexpected(std::move(intensityVariance.error()));
      light.flicker = PeriodicFunction<float>(root.random(), *period, *minimum, *maximum, *periodVariance, *intensityVariance);
    }
    auto angle = lightConfig.getFloat("pointAngle", 0.0f);
    if (!angle) return std::unexpected(std::move(angle.error()));
    light.pointAngle.set(*angle * Constants::deg2rad);
    auto point = lightConfig.getBool("pointLight", false);
    if (!point) return std::unexpected(std::move(point.error()));
    light.pointLight = std::move(*point);
    auto beam = lightConfig.getFloat("pointBeam", 0.0f);
    if (!beam) return std::unexpected(std::move(beam.error()));
    light.pointBeam = std::move(*beam);
    auto ambience = lightConfig.getFloat("beamAmbience", 0.0f);
    if (!ambience) return std::unexpected(std::move(ambience.error()));
    light.beamAmbience = std::move(*ambience);
  }
  auto soundsConfig = config.get("sounds", JsonObject());
  if (!soundsConfig) return std::unexpected(std::move(soundsConfig.error()));
  auto soundsEntries = soundsConfig->iterateObject();
  if (!soundsEntries) return std::unexpected(std::move(soundsEntries.error()));
  for (auto const& pair : *soundsEntries) {
    auto& sound = m_sounds[pair.first];
    auto const& soundConfig = pair.second;
    if (soundConfig.isType(Json::Type::Array)) {
      sound.rangeMultiplier = 1.0f;
      auto pool = jsonToStringList(soundConfig);
      if (!pool) return std::unexpected(std::move(pool.error()));
      sound.soundPool.set(pool->transformed(
        [relativePath = m_relativePath](String const& path) { return AssetPath::relative_to(relativePath, path); }));
      sound.volumeTarget.set(1.0f);
      sound.volumeRampTime.set(0.0f);
      sound.pitchMultiplierTarget.set(1.0f);
      sound.pitchMultiplierRampTime.set(0.0f);
    } else {
      auto range = soundConfig.getFloat("rangeMultiplier", 1.0f);
      if (!range) return std::unexpected(std::move(range.error()));
      sound.rangeMultiplier = std::move(*range);
      Vec2F soundPosition{};
      auto position = soundConfig.opt("position");
      if (!position) return std::unexpected(std::move(position.error()));
      if (*position) {
        auto positionValue = jsonToVec2F(**position);
        if (!positionValue) return std::unexpected(std::move(positionValue.error()));
        soundPosition = *positionValue;
      }
      sound.xPosition.set(soundPosition[0]);
      sound.yPosition.set(soundPosition[1]);
      auto volume = soundConfig.getFloat("volume", 1.0f);
      if (!volume) return std::unexpected(std::move(volume.error()));
      sound.volumeTarget.set(*volume);
      auto volumeRamp = soundConfig.getFloat("volumeRampTime", 0.0f);
      if (!volumeRamp) return std::unexpected(std::move(volumeRamp.error()));
      sound.volumeRampTime.set(*volumeRamp);
      auto pitch = soundConfig.getFloat("pitchMultiplier", 1.0f);
      if (!pitch) return std::unexpected(std::move(pitch.error()));
      sound.pitchMultiplierTarget.set(*pitch);
      auto pitchRamp = soundConfig.getFloat("pitchMultiplierRampTime", 0.0f);
      if (!pitchRamp) return std::unexpected(std::move(pitchRamp.error()));
      sound.pitchMultiplierRampTime.set(*pitchRamp);
      auto poolJson = soundConfig.get("pool", JsonArray());
      if (!poolJson) return std::unexpected(std::move(poolJson.error()));
      auto pool = jsonToStringList(*poolJson);
      if (!pool) return std::unexpected(std::move(pool.error()));
      sound.soundPool.set(pool->transformed(
        [relativePath = m_relativePath](String const& path) { return AssetPath::relative_to(relativePath, path); }));
    }
  }
  auto effectsConfig = config.get("effects", JsonObject());
  if (!effectsConfig) return std::unexpected(std::move(effectsConfig.error()));
  auto effectsEntries = effectsConfig->iterateObject();
  if (!effectsEntries) return std::unexpected(std::move(effectsEntries.error()));
  for (auto const& pair : *effectsEntries) {
    auto& effect = m_effects[pair.first];
    auto type = pair.second.getString("type");
    if (!type) return std::unexpected(std::move(type.error()));
    effect.type = std::move(*type);
    auto time = pair.second.getFloat("time", 0.0f);
    if (!time) return std::unexpected(std::move(time.error()));
    effect.time = std::move(*time);
    auto directives = pair.second.getString("directives");
    if (!directives) return std::unexpected(std::move(directives.error()));
    effect.directives = *directives;
  }
  m_transformationGroups.sort_by_key();
  m_rotationGroups.sort_by_key();
  m_particleEmitters.sort_by_key();
  m_lights.sort_by_key();
  m_sounds.sort_by_key();
  m_effects.sort_by_key();
  for (auto const& stateType : m_animatedParts.stateTypes())
    m_stateInfo[stateType].wasUpdated = true;
  m_stateInfo.sort_by_key();
  return setupNetStates();
}

std::expected<std::shared_ptr<NetworkedAnimator>, StarException> NetworkedAnimator::create(Root& root) {
  auto result = std::shared_ptr<NetworkedAnimator>(new NetworkedAnimator(root));
  auto registered = result->setupNetStates();
  if (!registered) return std::unexpected(std::move(registered.error()));
  return result;
}

std::expected<std::shared_ptr<NetworkedAnimator>, StarException> NetworkedAnimator::create(NetworkedAnimator const& animator) {
  auto result = std::shared_ptr<NetworkedAnimator>(new NetworkedAnimator(animator.m_root.get()));
  result->m_relativePath = animator.m_relativePath;
  result->m_animatedParts = animator.m_animatedParts;
  result->m_animationStateTypes = animator.m_animationStateTypes;
  result->m_stateInfo = animator.m_stateInfo;
  result->m_transformationGroups = animator.m_transformationGroups;
  result->m_rotationGroups = animator.m_rotationGroups;
  result->m_particleEmitters = animator.m_particleEmitters;
  result->m_lights = animator.m_lights;
  result->m_sounds = animator.m_sounds;
  result->m_effects = animator.m_effects;
  result->m_processingDirectives = animator.m_processingDirectives;
  result->m_zoom = animator.m_zoom;
  result->m_flipped = animator.m_flipped;
  result->m_flippedRelativeCenterLine = animator.m_flippedRelativeCenterLine;
  result->m_animationRate = animator.m_animationRate;
  result->m_globalTags = animator.m_globalTags;
  result->m_partTags = animator.m_partTags;
  result->m_cachedPartDrawables = animator.m_cachedPartDrawables;
  result->m_partDrawables = animator.m_partDrawables;
  result->m_localTags = animator.m_localTags;
  result->m_animatorVersion = animator.m_animatorVersion;
  auto registered = result->setupNetStates();
  if (!registered) return std::unexpected(std::move(registered.error()));

  return result;
}

StringList NetworkedAnimator::stateTypes() const {
  return m_animatedParts.stateTypes();
}

std::expected<StringList, StarException> NetworkedAnimator::states(String const& stateType) const {
  return m_animatedParts.states(stateType);
}

std::expected<bool, StarException> NetworkedAnimator::setState(String const& stateType, String const& state, bool startNew, bool reverse) {
  auto changed = m_animatedParts.setActiveState(stateType, state, startNew, reverse);
  if (!changed) return std::unexpected(std::move(changed.error()));
  if (*changed) {
    m_stateInfo[stateType].wasUpdated = true;
    m_stateInfo[stateType].startedEvent.trigger();
    return true;
  }
  return false;
}

std::expected<bool, StarException> NetworkedAnimator::setLocalState(String const& stateType, String const& state, bool startNew, bool reverse) {
  return m_animatedParts.setActiveState(stateType, state, startNew, reverse);
}

std::expected<String, StarException> NetworkedAnimator::queryState(String const& stateType, Sample const* sample) const {
  auto state = m_animatedParts.activeState(stateType, sample ? &sample->m_presentation->parts : nullptr);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().stateName;
}
std::expected<int, StarException> NetworkedAnimator::queryStateFrame(String const& stateType, Sample const* sample) const {
  auto state = m_animatedParts.activeState(stateType, sample ? &sample->m_presentation->parts : nullptr);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().frame;
}
std::expected<int, StarException> NetworkedAnimator::queryStateNextFrame(String const& stateType, Sample const* sample) const {
  auto state = m_animatedParts.activeState(stateType, sample ? &sample->m_presentation->parts : nullptr);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().nextFrame;
}
std::expected<float, StarException> NetworkedAnimator::queryStateFrameProgress(String const& stateType, Sample const* sample) const {
  auto state = m_animatedParts.activeState(stateType, sample ? &sample->m_presentation->parts : nullptr);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().frameProgress;
}
std::expected<float, StarException> NetworkedAnimator::queryStateTimer(String const& stateType, Sample const* sample) const {
  auto state = m_animatedParts.activeState(stateType, sample ? &sample->m_presentation->parts : nullptr);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().timer;
}
std::expected<bool, StarException> NetworkedAnimator::queryStateReverse(String const& stateType, Sample const* sample) const {
  auto state = m_animatedParts.activeState(stateType, sample ? &sample->m_presentation->parts : nullptr);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().reverse;
}

std::expected<float, StarException> NetworkedAnimator::queryStateCycle(String const& stateType, std::optional<String> state, Sample const* sample) const {
  auto active = m_animatedParts.activeState(stateType, sample ? &sample->m_presentation->parts : nullptr);
  if (!active) return std::unexpected(std::move(active.error()));
  auto selected = m_animatedParts.getState(stateType, state.value_or(active->get().stateName));
  if (!selected) return std::unexpected(std::move(selected.error()));
  return selected->get().cycle;
}
std::expected<int, StarException> NetworkedAnimator::queryStateFrames(String const& stateType, std::optional<String> state, Sample const* sample) const {
  auto active = m_animatedParts.activeState(stateType, sample ? &sample->m_presentation->parts : nullptr);
  if (!active) return std::unexpected(std::move(active.error()));
  auto selected = m_animatedParts.getState(stateType, state.value_or(active->get().stateName));
  if (!selected) return std::unexpected(std::move(selected.error()));
  return selected->get().frames;
}

std::expected<bool, StarException> NetworkedAnimator::hasState(String const& stateType, std::optional<String> const& state) const {
  auto contains = m_animatedParts.stateTypes().contains(stateType);
  if (!contains) return std::unexpected(std::move(contains.error()));
  if (!*contains) return false;
  if (state) {
    auto states = m_animatedParts.states(stateType);
    if (!states) return std::unexpected(std::move(states.error()));
    return states->contains(*state);
  }
  return true;
}

StringMap<AnimatedPartSet::Part> const& NetworkedAnimator::constParts() const {
  return m_animatedParts.constParts();
}

StringMap<AnimatedPartSet::Part>& NetworkedAnimator::parts() {
  return m_animatedParts.parts();
}

StringList NetworkedAnimator::partNames() const {
  return m_animatedParts.partNames();
}

std::expected<Json, StarException> NetworkedAnimator::queryStateProperty(String const& stateType, String const& propertyName, std::optional<String> state, std::optional<int> frame, Sample const* sample) const {
  if (state) {
    if (!frame) return std::unexpected(NetworkedAnimatorException("State property requires a frame"));
    return m_animatedParts.getStateFrameProperty(stateType, propertyName, *state, *frame);
  }
  auto active = m_animatedParts.activeState(stateType, sample ? &sample->m_presentation->parts : nullptr);
  if (!active) return std::unexpected(std::move(active.error()));
  return active->get().properties.value(propertyName);
}
std::expected<Json, StarException> NetworkedAnimator::queryStateNextProperty(String const& stateType, String const& propertyName, Sample const* sample) const {
  auto active = m_animatedParts.activeState(stateType, sample ? &sample->m_presentation->parts : nullptr);
  if (!active) return std::unexpected(std::move(active.error()));
  return active->get().nextProperties.value(propertyName);
}

std::expected<Json, StarException> NetworkedAnimator::queryPartProperty(String const& partName, String const& propertyName, std::optional<String> stateType, std::optional<String> state, std::optional<int> frame, Sample const* sample) const {
  if (stateType) {
    if (!state || !frame) return std::unexpected(NetworkedAnimatorException("Part state property requires a state and frame"));
    return m_animatedParts.getPartStateFrameProperty(partName, propertyName, *stateType, *state, *frame);
  }
  auto active = m_animatedParts.activePart(partName, sample ? &sample->m_presentation->parts : nullptr);
  if (!active) return std::unexpected(std::move(active.error()));
  return active->get().properties.value(propertyName);
}
std::expected<Json, StarException> NetworkedAnimator::queryPartNextProperty(String const& partName, String const& propertyName, Sample const* sample) const {
  auto active = m_animatedParts.activePart(partName, sample ? &sample->m_presentation->parts : nullptr);
  if (!active) return std::unexpected(std::move(active.error()));
  return active->get().nextProperties.value(propertyName);
}

Mat3F NetworkedAnimator::globalTransformation() const {
  Mat3F transformation = Mat3F::scaling(m_zoom.get());
  if (m_flipped.get())
    transformation = Mat3F::scaling(Vec2F(-1, 1), Vec2F(m_flippedRelativeCenterLine.get(), 0)) * transformation;
  return transformation;
}

std::expected<Mat3F, StarException> NetworkedAnimator::animationGroupTransformation(String const& name, TransformationGroup const& group, Mat3F const& base, Sample const* sample) const {
  for (auto const& stateType : m_animationStateTypes) {
    auto state = m_animatedParts.activeState(stateType, sample ? &sample->m_presentation->parts : nullptr);
    if (!state) return std::unexpected(std::move(state.error()));
    auto const& active = state->get();
    if (auto transforms = active.properties.find_ref(name)) {
      auto matrix = processTransforms(base, transforms->get(), active.properties);
      if (!matrix) return std::unexpected(std::move(matrix.error()));
      if (group.interpolated) {
        if (auto next = active.nextProperties.find_ref(name)) {
          auto nextMatrix = processTransforms(base, next->get(), active.nextProperties);
          if (!nextMatrix) return std::unexpected(std::move(nextMatrix.error()));
          return sampleAffine(active.frameProgress, *matrix, *nextMatrix);
        }
      }
      return matrix;
    }
  }
  return base;
}

std::expected<Mat3F, StarException> NetworkedAnimator::queryGroupTransformation(StringList const& transformationGroups, Sample const* sample) const {
  auto mat = Mat3F::identity();
  for (auto const& tg : transformationGroups) {
    auto group = m_transformationGroups.get(tg);
    if (!group) return std::unexpected(std::move(group.error()));
    auto const& value = group->get();
    auto cache = sample ? &sample->m_presentation->groups[value.presentationIndex] : nullptr;
    if (cache && cache->animationDirty) {
      auto sampled = animationGroupTransformation(tg, value, value.animationBaseTransform, sample);
      if (!sampled) return std::unexpected(std::move(sampled.error()));
      cache->animationTransform = *sampled;
      cache->animationDirty = false;
    }
    auto base = sample ? cache->transform : value.affineTransform();
    auto animation = sample ? cache->animationTransform : value.animationAffineTransform();
    mat = base * (sample ? cache->localTransform : value.localAffineTransform()) * animation * mat;
  }
  return mat;
}

std::expected<Mat3F, StarException> NetworkedAnimator::queryPartTransformation(String const& partName, Sample const* sample) const {
  auto definition = m_animatedParts.constParts().get(partName);
  if (!definition) return std::unexpected(std::move(definition.error()));
  Presentation::PartTransform* cache = nullptr;
  if (sample) {
    cache = &sample->m_presentation->partTransforms[definition->get().presentationIndex];
    if (cache->valid)
      return cache->transform;
  }
  auto active = m_animatedParts.activePart(definition->get(), sample ? &sample->m_presentation->parts : nullptr);
  if (!active) return std::unexpected(std::move(active.error()));
  auto const& part = active->get();
  Mat3F transformation = Mat3F::identity();
  if (auto offsetJson = part.properties.value("offset").opt()) {
    auto offset = jsonToVec2F(*offsetJson);
    if (!offset) return std::unexpected(std::move(offset.error()));
    transformation = Mat3F::translation(*offset) * transformation;
  }
  transformation = part.animationAffineTransform() * transformation;
  auto groups = jsonToStringList(part.properties.value("transformationGroups", JsonArray()));
  if (!groups) return std::unexpected(std::move(groups.error()));
  auto group = queryGroupTransformation(*groups, sample);
  if (!group) return std::unexpected(std::move(group.error()));
  transformation = *group * transformation;
  auto rotationName = part.properties.value("rotationGroup").optString();
  if (!rotationName) return std::unexpected(std::move(rotationName.error()));
  if (*rotationName) {
    auto rotation = m_rotationGroups.get(**rotationName);
    if (!rotation) return std::unexpected(std::move(rotation.error()));
    Vec2F center = rotation->get().rotationCenter;
    if (auto centerJson = part.properties.value("rotationCenter").opt()) {
      auto configuredCenter = jsonToVec2F(*centerJson);
      if (!configuredCenter) return std::unexpected(std::move(configuredCenter.error()));
      center = *configuredCenter;
    }
    auto const& value = rotation->get();
    float angle = sample ? angleLerp(sample->interpolation(), value.previousAngle, value.currentAngle) : value.currentAngle;
    transformation = Mat3F::rotation(angle, center) * transformation;
  }
  if (auto anchorJson = part.properties.find_ref("anchorPart")) {
    auto anchorName = anchorJson->get().toString();
    if (!anchorName) return std::unexpected(std::move(anchorName.error()));
    auto anchor = queryPartTransformation(*anchorName, sample);
    if (!anchor) return std::unexpected(std::move(anchor.error()));
    transformation = *anchor * transformation;
  }
  if (cache) {
    cache->transform = transformation;
    cache->valid = true;
  }
  return transformation;
}

std::expected<Mat3F, StarException> NetworkedAnimator::queryFinalPartTransformation(String const& partName, Sample const* sample) const {
  auto part = queryPartTransformation(partName, sample);
  if (!part) return std::unexpected(std::move(part.error()));
  return globalTransformation() * *part;
}

std::expected<std::optional<Vec2F>, StarException> NetworkedAnimator::queryPartPoint(String const& partName, String const& propertyName, Sample const* sample) const {
  auto active = m_animatedParts.activePart(partName, sample ? &sample->m_presentation->parts : nullptr);
  if (!active) return std::unexpected(std::move(active.error()));
  auto property = active->get().properties.value(propertyName);
  if (!property) return std::optional<Vec2F>{};
  auto value = jsonToVec2F(property);
  if (!value) return std::unexpected(std::move(value.error()));
  auto transform = queryFinalPartTransformation(partName, sample);
  if (!transform) return std::unexpected(std::move(transform.error()));
  return std::optional<Vec2F>(transform->transformVec2(*value));
}

std::expected<std::optional<PolyF>, StarException> NetworkedAnimator::queryPartPoly(String const& partName, String const& propertyName, Sample const* sample) const {
  auto active = m_animatedParts.activePart(partName, sample ? &sample->m_presentation->parts : nullptr);
  if (!active) return std::unexpected(std::move(active.error()));
  auto property = active->get().properties.value(propertyName);
  if (!property) return std::optional<PolyF>{};
  auto value = jsonToPolyF(property);
  if (!value) return std::unexpected(std::move(value.error()));
  auto transform = queryFinalPartTransformation(partName, sample);
  if (!transform) return std::unexpected(std::move(transform.error()));
  value->transform(*transform);
  return std::optional<PolyF>(std::move(*value));
}

void NetworkedAnimator::setGlobalTag(String tagName, std::optional<String> tagValue) {
  if (tagValue)
    m_globalTags.set(std::move(tagName), std::move(*tagValue));
  else
    m_globalTags.remove(tagName);
}

void NetworkedAnimator::removeGlobalTag(String const& tagName) {
  m_globalTags.remove(tagName);
}

std::optional<std::reference_wrapper<String const>> NetworkedAnimator::globalTag(String const& tagName) const {
  return m_globalTags.find_ref(tagName);
}


void NetworkedAnimator::setPartTag(String const& partType, String tagName, std::optional<String> tagValue) {
  if (tagValue)
    m_partTags[partType].set(std::move(tagName), std::move(*tagValue));
  else
    m_partTags[partType].remove(tagName);
}

void NetworkedAnimator::setLocalTag(String tagName, std::optional<String> tagValue) {
  if (tagValue)
    m_localTags.set(tagName, *tagValue);
  else
    m_localTags.remove(tagName);
}

void NetworkedAnimator::setPartDrawables(String const& partName, List<Drawable> drawables) {
  m_partDrawables.set(partName, std::move(drawables));
}
void NetworkedAnimator::addPartDrawables(String const& partName, List<Drawable> drawables) {
  m_partDrawables[partName].appendAll(drawables);
}
std::expected<String, StarException> NetworkedAnimator::queryApplyPartTags(String const& partName, String apply, Sample const* sample) const {
  HashMap<String, String> animationTags = m_localTags;
  if (sample) {
    for (auto const& tag : sample->m_presentation->tags) {
      if (!tag.second.active)
        continue;
      if (tag.second.value)
        animationTags.set(tag.first, *tag.second.value);
      else
        animationTags.remove(tag.first);
    }
  }
  std::optional<unsigned> frame;
  String frameStr;
  String frameIndexStr;
  auto activePartResult = m_animatedParts.activePart(partName, sample ? &sample->m_presentation->parts : nullptr);
  if (!activePartResult) return std::unexpected(std::move(activePartResult.error()));
  auto const& activePart = activePartResult->get();
  auto partTagsResult = m_partTags.get(partName);
  if (!partTagsResult) return std::unexpected(std::move(partTagsResult.error()));
  auto const& partTags = partTagsResult->get();
  if (activePart.activeState) {
    unsigned stateFrame = activePart.activeState->frame;
    frame = stateFrame;
    frameStr = static_cast<String>(toString(stateFrame + 1));
    frameIndexStr = static_cast<String>(toString(stateFrame));
  }
  if (version() > 0) {
    animationTags.set("relativePath", m_relativePath);
    for (auto& stateTypeName : m_animatedParts.stateTypes()) {
      auto activeStateResult = m_animatedParts.activeState(stateTypeName, sample ? &sample->m_presentation->parts : nullptr);
      if (!activeStateResult) return std::unexpected(std::move(activeStateResult.error()));
      auto const& activeState = activeStateResult->get();
      unsigned stateFrame = activeState.frame;
      std::optional<unsigned> frame;
      String frameStr;
      String frameIndexStr;

      frame = stateFrame;
      frameStr = static_cast<String>(toString(stateFrame + 1));
      frameIndexStr = static_cast<String>(toString(stateFrame));
      if (frame) {
        animationTags.set(stateTypeName + "_frame", frameStr);
        animationTags.set(stateTypeName + "_frameIndex", frameIndexStr);
      }
      animationTags.set(stateTypeName + "_state", activeState.stateName);

      if (auto p = activeState.properties.find_ref("animationTags")) {
        auto tags = p->get().iterateObject();
        if (!tags) return std::unexpected(std::move(tags.error()));
        for (auto const& tag : *tags) {
          if (!animationTags.contains(tag.first)) {
            auto value = tag.second.toString();
            if (!value) return std::unexpected(std::move(value.error()));
            animationTags.set(tag.first, *value);
          }
        }
      }
    }
  }


  auto applied = apply.maybeLookupTagsView([&](StringView tag) -> StringView {
    if (tag == "frame") {
      if (frame)
        return frameStr;
    } else if (tag == "frameIndex") {
      if (frame)
        return frameIndexStr;
    } else if (auto p = animationTags.find_ref(tag)) {
      return StringView(p->get());
    } else if (auto p = partTags.find_ref(tag)) {
      return StringView(p->get());
    } else if (auto p = m_globalTags.find_ref(tag)) {
      return StringView(p->get());
    }

    return StringView("default");
  });
  return applied ? *applied : apply;
}


void NetworkedAnimator::setProcessingDirectives(Directives const& directives) {
  m_processingDirectives.set(directives);
}

void NetworkedAnimator::setZoom(float zoom) {
  m_zoom.set(zoom);
}

bool NetworkedAnimator::flipped() const {
  return m_flipped.get();
}

float NetworkedAnimator::flippedRelativeCenterLine() const {
  return m_flippedRelativeCenterLine.get();
}

void NetworkedAnimator::setFlipped(bool flipped, float relativeCenterLine) {
  m_flipped.set(flipped);
  m_flippedRelativeCenterLine.set(relativeCenterLine);
}

void NetworkedAnimator::setAnimationRate(float rate) {
  m_animationRate.set(rate);
}

float NetworkedAnimator::animationRate() {
  return m_animationRate.get();
}

bool NetworkedAnimator::hasRotationGroup(String const& rotationGroup) const {
  return m_rotationGroups.contains(rotationGroup);
}

std::expected<void, StarException> NetworkedAnimator::rotateGroup(String const& rotationGroup, float targetAngle, bool immediate) {
  auto groupResult = m_rotationGroups.get(rotationGroup);
  if (!groupResult) return std::unexpected(std::move(groupResult.error()));
  auto& group = groupResult->get();
  group.targetAngle.set(targetAngle);

  if (immediate) {
    group.currentAngle = targetAngle;
    group.previousAngle = targetAngle;
    group.netImmediateEvent.trigger();
  }
  return {};
}

std::expected<float, StarException> NetworkedAnimator::queryCurrentRotationAngle(String const& rotationGroup, Sample const* sample) const {
  auto entry = m_rotationGroups.get(rotationGroup);
  if (!entry) return std::unexpected(std::move(entry.error()));
  auto const& group = entry->get();
  return sample ? angleLerp(sample->interpolation(), group.previousAngle, group.currentAngle) : group.currentAngle;
}

bool NetworkedAnimator::hasTransformationGroup(String const& transformationGroup) const {
  return m_transformationGroups.contains(transformationGroup);
}

std::expected<void, StarException> NetworkedAnimator::translateTransformationGroup(String const& transformationGroup, Vec2F const& translation) {
  auto groupResult = m_transformationGroups.get(transformationGroup);
  if (!groupResult) return std::unexpected(std::move(groupResult.error()));
  auto& group = groupResult->get();
  group.setAffineTransform(Mat3F::translation(translation) * group.affineTransform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::rotateTransformationGroup(
    String const& transformationGroup, float rotation, Vec2F const& rotationCenter) {
  auto groupResult = m_transformationGroups.get(transformationGroup);
  if (!groupResult) return std::unexpected(std::move(groupResult.error()));
  auto& group = groupResult->get();
  group.setAffineTransform(Mat3F::rotation(rotation, rotationCenter) * group.affineTransform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::scaleTransformationGroup(
    String const& transformationGroup, float scale, Vec2F const& scaleCenter) {
  auto groupResult = m_transformationGroups.get(transformationGroup);
  if (!groupResult) return std::unexpected(std::move(groupResult.error()));
  auto& group = groupResult->get();
  group.setAffineTransform(Mat3F::scaling(scale, scaleCenter) * group.affineTransform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::scaleTransformationGroup(
    String const& transformationGroup, Vec2F const& scale, Vec2F const& scaleCenter) {
  auto groupResult = m_transformationGroups.get(transformationGroup);
  if (!groupResult) return std::unexpected(std::move(groupResult.error()));
  auto& group = groupResult->get();
  group.setAffineTransform(Mat3F::scaling(scale, scaleCenter) * group.affineTransform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::transformTransformationGroup(
    String const& transformationGroup, float a, float b, float c, float d, float tx, float ty) {
  auto groupResult = m_transformationGroups.get(transformationGroup);
  if (!groupResult) return std::unexpected(std::move(groupResult.error()));
  auto& group = groupResult->get();
  Mat3F transform = Mat3F(a, b, tx, c, d, ty, 0, 0, 1);
  group.setAffineTransform(transform * group.affineTransform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::resetTransformationGroup(String const& transformationGroup) {
  auto entry = m_transformationGroups.get(transformationGroup);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().setAffineTransform(Mat3F::identity());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setTransformationGroup(String const& transformationGroup, Mat3F transform) {
  auto entry = m_transformationGroups.get(transformationGroup);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().setAffineTransform(transform);
  return {};
}

std::expected<Mat3F, StarException> NetworkedAnimator::getTransformationGroup(String const& transformationGroup) {
  auto entry = m_transformationGroups.get(transformationGroup);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return entry->get().affineTransform();
}
std::expected<void, StarException> NetworkedAnimator::translateLocalTransformationGroup(String const& transformationGroup, Vec2F const& translation) {
  auto groupResult = m_transformationGroups.get(transformationGroup);
  if (!groupResult) return std::unexpected(std::move(groupResult.error()));
  auto& group = groupResult->get();
  group.setLocalAffineTransform(Mat3F::translation(translation) * group.localAffineTransform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::rotateLocalTransformationGroup(
    String const& transformationGroup, float rotation, Vec2F const& rotationCenter) {
  auto groupResult = m_transformationGroups.get(transformationGroup);
  if (!groupResult) return std::unexpected(std::move(groupResult.error()));
  auto& group = groupResult->get();
  group.setLocalAffineTransform(Mat3F::rotation(rotation, rotationCenter) * group.localAffineTransform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::scaleLocalTransformationGroup(
    String const& transformationGroup, float scale, Vec2F const& scaleCenter) {
  auto groupResult = m_transformationGroups.get(transformationGroup);
  if (!groupResult) return std::unexpected(std::move(groupResult.error()));
  auto& group = groupResult->get();
  group.setLocalAffineTransform(Mat3F::scaling(scale, scaleCenter) * group.localAffineTransform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::scaleLocalTransformationGroup(
    String const& transformationGroup, Vec2F const& scale, Vec2F const& scaleCenter) {
  auto groupResult = m_transformationGroups.get(transformationGroup);
  if (!groupResult) return std::unexpected(std::move(groupResult.error()));
  auto& group = groupResult->get();
  group.setLocalAffineTransform(Mat3F::scaling(scale, scaleCenter) * group.localAffineTransform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::transformLocalTransformationGroup(
    String const& transformationGroup, float a, float b, float c, float d, float tx, float ty) {
  auto groupResult = m_transformationGroups.get(transformationGroup);
  if (!groupResult) return std::unexpected(std::move(groupResult.error()));
  auto& group = groupResult->get();
  Mat3F transform = Mat3F(a, b, tx, c, d, ty, 0, 0, 1);
  group.setLocalAffineTransform(transform * group.localAffineTransform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::resetLocalTransformationGroup(String const& transformationGroup) {
  auto entry = m_transformationGroups.get(transformationGroup);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().setLocalAffineTransform(Mat3F::identity());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setLocalTransformationGroup(String const& transformationGroup, Mat3F transform) {
  auto entry = m_transformationGroups.get(transformationGroup);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().setLocalAffineTransform(transform);
  return {};
}

std::expected<Mat3F, StarException> NetworkedAnimator::getLocalTransformationGroup(String const& transformationGroup) {
  auto entry = m_transformationGroups.get(transformationGroup);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return entry->get().localAffineTransform();
}

bool NetworkedAnimator::hasParticleEmitter(String const& emitterName) const {
  return m_particleEmitters.contains(emitterName);
}

std::expected<void, StarException> NetworkedAnimator::setParticleEmitterActive(String const& emitterName, bool active) {
  auto entry = m_particleEmitters.get(emitterName);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().active.set(active);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setParticleEmitterEmissionRate(String const& emitterName, float emissionRate) {
  auto entry = m_particleEmitters.get(emitterName);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().emissionRate.set(emissionRate);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setParticleEmitterOffsetRegion(String const& emitterName, RectF const& offsetRegion) {
  auto entry = m_particleEmitters.get(emitterName);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().offsetRegion.set(offsetRegion);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setParticleEmitterBurstCount(String const& emitterName, unsigned burstCount) {
  auto entry = m_particleEmitters.get(emitterName);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().burstCount.set(burstCount);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::burstParticleEmitter(String const& emitterName) {
  auto entry = m_particleEmitters.get(emitterName);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().burstEvent.trigger();
  return {};
}

bool NetworkedAnimator::hasLight(String const& lightName) const {
  return m_lights.contains(lightName);
}

std::expected<void, StarException> NetworkedAnimator::setLightActive(String const& lightName, bool active) {
  auto entry = m_lights.get(lightName);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().active.set(active);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setLightPosition(String const& lightName, Vec2F position) {
  auto lightResult = m_lights.get(lightName);
  if (!lightResult) return std::unexpected(std::move(lightResult.error()));
  auto& light = lightResult->get();
  light.xPosition.set(position[0]);
  light.yPosition.set(position[1]);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setLightColor(String const& lightName, Color color) {
  auto entry = m_lights.get(lightName);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().color.set(color);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setLightPointAngle(String const& lightName, float angle) {
  auto entry = m_lights.get(lightName);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().pointAngle.set(angle * Constants::deg2rad);
  return {};
}

bool NetworkedAnimator::hasSound(String const& soundName) const {
  return m_sounds.contains(soundName);
}

std::expected<void, StarException> NetworkedAnimator::setSoundPool(String const& soundName, StringList soundPool) {
  auto entry = m_sounds.get(soundName);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().soundPool.set(std::move(soundPool));
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setSoundPosition(String const& soundName, Vec2F const& position) {
  auto soundResult = m_sounds.get(soundName);
  if (!soundResult) return std::unexpected(std::move(soundResult.error()));
  auto& sound = soundResult->get();
  sound.xPosition.set(position[0]);
  sound.yPosition.set(position[1]);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setSoundVolume(String const& soundName, float volume, float rampTime) {
  auto soundResult = m_sounds.get(soundName);
  if (!soundResult) return std::unexpected(std::move(soundResult.error()));
  auto& sound = soundResult->get();
  sound.volumeTarget.set(volume);
  sound.volumeRampTime.set(rampTime);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setSoundPitchMultiplier(String const& soundName, float pitchMultiplier, float rampTime) {
  auto soundResult = m_sounds.get(soundName);
  if (!soundResult) return std::unexpected(std::move(soundResult.error()));
  auto& sound = soundResult->get();
  sound.pitchMultiplierTarget.set(pitchMultiplier);
  sound.pitchMultiplierRampTime.set(rampTime);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::playSound(String const& soundName, int loops) {
  auto soundResult = m_sounds.get(soundName);
  if (!soundResult) return std::unexpected(std::move(soundResult.error()));
  auto& sound = soundResult->get();
  sound.loops.set(loops);
  sound.signals.send(SoundSignal::Play);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::stopAllSounds(String const& soundName, float rampTime) {
  auto soundResult = m_sounds.get(soundName);
  if (!soundResult) return std::unexpected(std::move(soundResult.error()));
  auto& sound = soundResult->get();
  sound.volumeRampTime.set(rampTime);
  sound.signals.send(SoundSignal::StopAll);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::setEffectEnabled(String const& effect, bool enabled) {
  auto entry = m_effects.get(effect);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().enabled.set(enabled);
  return {};
}

std::expected<List<Drawable>, StarException> NetworkedAnimator::queryDrawables(Vec2F const& position, Sample const* sample) const {
  List<Drawable> drawables;
  auto rendered = queryDrawablesWithZLevel(position, sample);
  if (!rendered) return std::unexpected(std::move(rendered.error()));
  for (auto& p : *rendered)
    drawables.append(std::move(p.first));
  return drawables;
}

std::expected<List<std::pair<Drawable, float>>, StarException> NetworkedAnimator::queryDrawablesWithZLevel(Vec2F const& position, Sample const* sample) const {
  std::size_t partCount = m_animatedParts.constParts().size();
  if (!partCount)
    return List<std::pair<Drawable, float>>{};

  List<Directives> baseProcessingDirectives = { m_processingDirectives.get() };
  for (auto& pair : m_effects) {
    auto const& effectState = pair.second;

    if (effectState.enabled.get()) {
      auto const& effect = pair.second;
      if (effect.type == "flash") {
        if (effectState.timer > effect.time / 2) {
          baseProcessingDirectives.append(effect.directives);
        }
      } else if (effect.type == "directive") {
        baseProcessingDirectives.append(effect.directives);
      } else {
        return std::unexpected(NetworkedAnimatorException(strf("No such NetworkedAnimator effect type '{}'", effect.type)));
      }
    }
  }
  HashMap<String, String> animationTags = m_localTags;
  if (sample) {
    for (auto const& tag : sample->m_presentation->tags) {
      if (!tag.second.active)
        continue;
      if (tag.second.value)
        animationTags.set(tag.first, *tag.second.value);
      else
        animationTags.remove(tag.first);
    }
  }
  if (version() > 0) {
    animationTags.set("relativePath", m_relativePath);
    for (auto& stateTypeName : m_animatedParts.stateTypes()) {
      auto activeStateResult = m_animatedParts.activeState(stateTypeName, sample ? &sample->m_presentation->parts : nullptr);
      if (!activeStateResult) return std::unexpected(std::move(activeStateResult.error()));
      auto const& activeState = activeStateResult->get();
      unsigned stateFrame = activeState.frame;
      std::optional<unsigned> frame;
      String frameStr;
      String frameIndexStr;

      frame = stateFrame;
      frameStr = static_cast<String>(toString(stateFrame + 1));
      frameIndexStr = static_cast<String>(toString(stateFrame));
      if (frame) {
        animationTags.set(stateTypeName + "_frame", frameStr);
        animationTags.set(stateTypeName + "_frameIndex", frameIndexStr);
      }
      animationTags.set(stateTypeName + "_state", activeState.stateName);

      if (auto p = activeState.properties.find_ref("animationTags")) {
        auto tags = p->get().iterateObject();
        if (!tags) return std::unexpected(std::move(tags.error()));
        for (auto const& tag : *tags) {
          if (!animationTags.contains(tag.first)) {
            auto value = tag.second.toString();
            if (!value) return std::unexpected(std::move(value.error()));
            animationTags.set(tag.first, *value);
          }
        }
      }
    }
  }

  List<std::tuple<std::reference_wrapper<AnimatedPartSet::ActivePartInformation const>, std::reference_wrapper<String const>, float>> parts;
  parts.reserve(partCount);
  int drawableCount = 0;
  auto collected = m_animatedParts.forEachActivePart([&](String const& partName, AnimatedPartSet::ActivePartInformation const& activePart) -> std::expected<void, StarException> {
    std::optional<float> maybeZLevel;
    if (m_flipped.get()) {
      auto maybeFlipped = activePart.properties.value("flippedZLevel").optFloat();
      if (!maybeFlipped) return std::unexpected(std::move(maybeFlipped.error()));
      if (*maybeFlipped) maybeZLevel = **maybeFlipped;
    }
    if (!maybeZLevel) {
      auto z = activePart.properties.value("zLevel").optFloat();
      if (!z) return std::unexpected(std::move(z.error()));
      maybeZLevel = *z;
    }

    if (auto drawables = partDrawables(partName, sample))
      drawableCount += drawables->size();
    parts.append(std::make_tuple(std::cref(activePart), std::cref(partName), maybeZLevel.value_or(0.0f)));
    return {};
  }, sample ? &sample->m_presentation->parts : nullptr);
  if (!collected) return std::unexpected(std::move(collected.error()));

  std::sort(parts.begin(), parts.end(), [](auto const& a, auto const& b) { return std::get<2>(a) < std::get<2>(b); });

  List<std::pair<Drawable, float>> drawables;
  drawables.reserve(partCount + drawableCount);
  for (auto& entry : parts) {
    auto& activePart = std::get<0>(entry).get();
    auto& partName = std::get<1>(entry).get();
    // Make sure we don't copy the original image
    String fallback = "";
    Json jImage = activePart.properties.value("image", {});
    if (version() > 0 && m_flipped.get()) {
      auto maybeFlipped = activePart.properties.value("flippedImage").optString();
      if (!maybeFlipped) return std::unexpected(std::move(maybeFlipped.error()));
      if (*maybeFlipped) jImage = **maybeFlipped;
    }

    std::shared_ptr<String const> imageString;
    if (jImage.isType(Json::Type::String)) {
      auto checkedImage = jImage.stringPtr();
      if (!checkedImage)
        return std::unexpected(std::move(checkedImage.error()));
      imageString = std::move(*checkedImage);
    }
    String const& image = imageString ? *imageString : fallback;

    auto centeredValue = activePart.properties.value("centered").optBool();
    if (!centeredValue) return std::unexpected(std::move(centeredValue.error()));
    auto fullbrightValue = activePart.properties.value("fullbright").optBool();
    if (!fullbrightValue) return std::unexpected(std::move(fullbrightValue.error()));
    bool centered = centeredValue->value_or(true);
    bool fullbright = fullbrightValue->value_or(false);

    std::size_t originalDirectivesSize = baseProcessingDirectives.size();

    auto partTagsResult = m_partTags.get(partName);
    if (!partTagsResult) return std::unexpected(std::move(partTagsResult.error()));
    auto const& partTags = partTagsResult->get();

    auto partDirectives = activePart.properties.value("processingDirectives").optString();
    if (!partDirectives) return std::unexpected(std::move(partDirectives.error()));
    if (auto directives = *partDirectives) {
      if (version() > 0){
        directives = directives->maybeLookupTagsView([&](StringView tag) -> StringView {
          if (auto p = animationTags.find_ref(tag)) {
            return StringView(p->get());
          } else if (auto p = partTags.find_ref(tag)) {
            return StringView(p->get());
          } else if (auto p = m_globalTags.find_ref(tag)) {
            return StringView(p->get());
          }
          return StringView("default");
        });
      }
      if (!directives) return std::unexpected(NetworkedAnimatorException("Processing directives tag lookup produced no value"));
      baseProcessingDirectives.append(*directives);
    }

    std::optional<unsigned> frame;
    String frameStr;
    String frameIndexStr;
    if (activePart.activeState) {
      unsigned stateFrame = activePart.activeState->frame;
      frame = stateFrame;
      frameStr = static_cast<String>(toString(stateFrame + 1));
      frameIndexStr = static_cast<String>(toString(stateFrame));

      auto stateDirectives = activePart.activeState->properties.value("processingDirectives").optString();
      if (!stateDirectives) return std::unexpected(std::move(stateDirectives.error()));
      if (auto directives = *stateDirectives) {
        if (version() > 0){
          directives = directives->maybeLookupTagsView([&](StringView tag) -> StringView {
            if (auto p = animationTags.find_ref(tag)) {
              return StringView(p->get());
            } else if (auto p = partTags.find_ref(tag)) {
              return StringView(p->get());
            } else if (auto p = m_globalTags.find_ref(tag)) {
              return StringView(p->get());
            }
            return StringView("default");
          });
        }
        if (!directives) return std::unexpected(NetworkedAnimatorException("Processing directives tag lookup produced no value"));
        baseProcessingDirectives.append(*directives);
      }
    }

    std::optional<String> processedImage = image.maybeLookupTagsView([&](StringView tag) -> StringView {
      if (tag == "frame") {
        if (frame)
          return frameStr;
      } else if (tag == "frameIndex") {
        if (frame)
          return frameIndexStr;
      } else if (auto p = animationTags.find_ref(tag)) {
        return StringView(p->get());
      } else if (auto p = partTags.find_ref(tag)) {
        return StringView(p->get());
      } else if (auto p = m_globalTags.find_ref(tag)) {
        return StringView(p->get());
      }

      return StringView("default");
    });
    String const& usedImage = processedImage ? *processedImage : image;

    auto partTransform = queryPartTransformation(partName, sample);
    if (!partTransform) return std::unexpected(std::move(partTransform.error()));
    auto transformation = globalTransformation() * *partTransform;
    transformation.translate(position);

    std::optional<String::Char> initial;
    if (!usedImage.empty()) {
      auto character = usedImage.at(0);
      if (!character) return std::unexpected(std::move(character.error()));
      initial = *character;
    }
    if (initial && *initial != ':' && *initial != '?') {
      std::size_t hash = hash_of(usedImage);
      auto find = m_cachedPartDrawables.find(partName);
      if (find == m_cachedPartDrawables.end() || find->second.first != hash) {
        String relativeImage;
        if (*initial != '/')
          relativeImage = AssetPath::relative_to(m_relativePath, usedImage);

        auto metadata = m_root.get().imageMetadataDatabase();
        if (!metadata) return std::unexpected(std::move(metadata.error()));
        auto created = Drawable::makeImage(**metadata, !relativeImage.empty() ? relativeImage : usedImage, 1.0f / TilePixels, centered, Vec2F());
        if (!created) return std::unexpected(std::move(created.error()));
        Drawable drawable = std::move(*created);
        if (find == m_cachedPartDrawables.end())
          find = m_cachedPartDrawables.emplace(partName, std::pair{ hash, std::move(drawable) }).first;
        else {
          find->second.first = hash;
          find->second.second = std::move(drawable);
        }
      }

      Drawable drawable = find->second.second;
      auto& imagePart = drawable.imagePart();
      for (Directives const& directives : baseProcessingDirectives) {
        auto metadata = m_root.get().imageMetadataDatabase();
        if (!metadata) return std::unexpected(std::move(metadata.error()));
        auto added = imagePart.addDirectives(**metadata, directives, centered);
        if (!added) return std::unexpected(std::move(added.error()));
      }
      drawable.fullbright = fullbright;
      drawable.transform(transformation);
      drawables.append({std::move(drawable), std::get<2>(entry)});
    }

    if (auto attached = partDrawables(partName, sample)) {
      for (auto const& attachedDrawable : *attached) {
        Drawable drawable = attachedDrawable;
        drawable.transform(transformation);
        drawables.append({std::move(drawable), std::get<2>(entry)});
      }
    }

    baseProcessingDirectives.resize(originalDirectivesSize);
  }

  return drawables;
}

std::expected<List<LightSource>, StarException> NetworkedAnimator::queryLightSources(Vec2F const& translate, Sample const* sample) const {
  List<LightSource> lightSources;
  for (auto const& pair : m_lights) {
    if (!pair.second.active.get())
      continue;

    Vec2F position = {pair.second.xPosition.get(), pair.second.yPosition.get()};
    float pointAngle = constrainAngle(pair.second.pointAngle.get());
    Mat3F transformation = Mat3F::identity();
    if (pair.second.anchorPart) {
      auto anchor = queryPartTransformation(*pair.second.anchorPart, sample);
      if (!anchor) return std::unexpected(std::move(anchor.error()));
      transformation = *anchor;
    }
    auto group = queryGroupTransformation(pair.second.transformationGroups, sample);
    if (!group) return std::unexpected(std::move(group.error()));
    transformation = *group * transformation;
    position = transformation.transformVec2(position);
    pointAngle = transformation.transformAngle(pointAngle);
    if (pair.second.rotationGroup) {
      auto rotation = m_rotationGroups.get(*pair.second.rotationGroup);
      if (!rotation) return std::unexpected(std::move(rotation.error()));
      auto const& rg = rotation->get();
      float angle = sample ? angleLerp(sample->interpolation(), rg.previousAngle, rg.currentAngle) : rg.currentAngle;
      position = (position - pair.second.rotationCenter.value_or(rg.rotationCenter)).rotate(angle)
          + pair.second.rotationCenter.value_or(rg.rotationCenter);
      pointAngle += angle;
    }
    position = globalTransformation().transformVec2(position);
    if (m_flipped.get()) {
      if (pointAngle > 0)
        pointAngle = std::numbers::pi / 2 + constrainAngle(std::numbers::pi / 2 - pointAngle);
      else
        pointAngle = -std::numbers::pi / 2 - constrainAngle(pointAngle + std::numbers::pi / 2);
    }

    Color color = pair.second.color.get();
    if (pair.second.flicker)
      color.setValue(std::clamp(color.value() * pair.second.flicker->value(SinWeightOperator<float>()), 0.0f, 1.0f));

    lightSources.append(LightSource{
      position + translate,
      color.toRgbF(),
      pair.second.pointLight ? LightType::Point : LightType::Spread,
      pair.second.pointBeam,
      pointAngle,
      pair.second.beamAmbience
    });
  }
  return lightSources;
}

std::expected<void, StarException> NetworkedAnimator::update(float dt, std::optional<std::reference_wrapper<DynamicTarget>> dynamicTarget) {
  dt *= m_animationRate.get();

  auto animated = m_animatedParts.update(dt);
  if (!animated) return std::unexpected(std::move(animated.error()));

  auto statesUpdated = m_animatedParts.forEachActiveState([&](String const& stateTypeName, AnimatedPartSet::ActiveStateInformation const& activeState) -> std::expected<void, StarException> {
      if (dynamicTarget) {
        dynamicTarget->get().clearFinishedAudio();

        Json persistentSound = activeState.properties.value("persistentSound", "");
        String persistentSoundFile;

        if (persistentSound.isType(Json::Type::String)) {
          auto file = persistentSound.toString();
          if (!file) return std::unexpected(std::move(file.error()));
          persistentSoundFile = *file;
        } else if (persistentSound.isType(Json::Type::Array)) {
          auto pool = persistentSound.toArray();
          if (!pool) return std::unexpected(std::move(pool.error()));
          auto file = m_root.get().random().randValueFrom(*pool, "").toString();
          if (!file) return std::unexpected(std::move(file.error()));
          persistentSoundFile = *file;
        }

        if (!persistentSoundFile.empty())
          persistentSoundFile = AssetPath::relative_to(m_relativePath, persistentSoundFile);

        auto& activePersistentSound = dynamicTarget->get().statePersistentSounds[stateTypeName];

        bool changedPersistentSound = persistentSound != activePersistentSound.sound;
        if (changedPersistentSound || !activePersistentSound.audio) {
          activePersistentSound.sound = std::move(persistentSound);
          if (activePersistentSound.audio)
            activePersistentSound.audio->stop(activePersistentSound.stopRampTime);

          if (!persistentSoundFile.empty()) {
            auto assets = m_root.get().assets();
            if (!assets) return std::unexpected(std::move(assets.error()));
            auto audio = (*assets)->audio(persistentSoundFile);
            if (!audio) return std::unexpected(std::move(audio.error()));
            auto instance = AudioInstance::create(**audio);
            if (!instance) return std::unexpected(std::move(instance.error()));
            activePersistentSound.audio = std::move(*instance);
            auto range = activeState.properties.value("persistentSoundRangeMultiplier", 1.0f).toFloat();
            if (!range) return std::unexpected(std::move(range.error()));
            activePersistentSound.audio->setRangeMultiplier(*range);
            activePersistentSound.audio->setLoops(-1);
            activePersistentSound.audio->setPosition(globalTransformation().transformVec2(Vec2F()));
            auto stopTime = activeState.properties.value("persistentSoundStopTime", 0.0f).toFloat();
            if (!stopTime) return std::unexpected(std::move(stopTime.error()));
            activePersistentSound.stopRampTime = *stopTime;
            dynamicTarget->get().pendingAudios.append(activePersistentSound.audio);
          } else {
            dynamicTarget->get().statePersistentSounds.remove(stateTypeName);
          }
        }

        Json immediateSound = activeState.properties.value("immediateSound", "");
        String immediateSoundFile = "";

        if (immediateSound.isType(Json::Type::String)) {
          auto file = immediateSound.toString();
          if (!file) return std::unexpected(std::move(file.error()));
          immediateSoundFile = *file;
        } else if (immediateSound.isType(Json::Type::Array)) {
          auto pool = immediateSound.toArray();
          if (!pool) return std::unexpected(std::move(pool.error()));
          auto file = m_root.get().random().randValueFrom(*pool, "").toString();
          if (!file) return std::unexpected(std::move(file.error()));
          immediateSoundFile = *file;
        }

        if (!immediateSoundFile.empty())
          immediateSoundFile = AssetPath::relative_to(m_relativePath, immediateSoundFile);

        auto& activeImmediateSound = dynamicTarget->get().stateImmediateSounds[stateTypeName];

        bool changedImmediateSound = immediateSound != activeImmediateSound.sound;
        if (changedImmediateSound) {
          activeImmediateSound.sound = std::move(immediateSound);
          if (!immediateSoundFile.empty()) {
            auto assets = m_root.get().assets();
            if (!assets) return std::unexpected(std::move(assets.error()));
            auto audio = (*assets)->audio(immediateSoundFile);
            if (!audio) return std::unexpected(std::move(audio.error()));
            auto instance = AudioInstance::create(**audio);
            if (!instance) return std::unexpected(std::move(instance.error()));
            activeImmediateSound.audio = std::move(*instance);
            auto range = activeState.properties.value("immediateSoundRangeMultiplier", 1.0f).toFloat();
            if (!range) return std::unexpected(std::move(range.error()));
            activeImmediateSound.audio->setRangeMultiplier(*range);
            activeImmediateSound.audio->setPosition(globalTransformation().transformVec2(Vec2F()));
            dynamicTarget->get().pendingAudios.append(activeImmediateSound.audio);
          }
        }
      }

      if (auto lightsOn = activeState.properties.find_ref("lightsOn")) {
        auto entries = lightsOn->get().iterateArray();
        if (!entries) return std::unexpected(std::move(entries.error()));
        for (auto const& nameJson : *entries) {
          auto name = nameJson.toString();
          if (!name) return std::unexpected(std::move(name.error()));
          auto entry = m_lights.get(*name);
          if (!entry) return std::unexpected(std::move(entry.error()));
          entry->get().active.set(true);
        }
      }
      if (auto lightsOff = activeState.properties.find_ref("lightsOff")) {
        auto entries = lightsOff->get().iterateArray();
        if (!entries) return std::unexpected(std::move(entries.error()));
        for (auto const& nameJson : *entries) {
          auto name = nameJson.toString();
          if (!name) return std::unexpected(std::move(name.error()));
          auto entry = m_lights.get(*name);
          if (!entry) return std::unexpected(std::move(entry.error()));
          entry->get().active.set(false);
        }
      }

      if (auto particleEmittersOn = activeState.properties.find_ref("particleEmittersOn")) {
        auto entries = particleEmittersOn->get().iterateArray();
        if (!entries) return std::unexpected(std::move(entries.error()));
        for (auto const& nameJson : *entries) {
          auto name = nameJson.toString();
          if (!name) return std::unexpected(std::move(name.error()));
          auto entry = m_particleEmitters.get(*name);
          if (!entry) return std::unexpected(std::move(entry.error()));
          entry->get().active.set(true);
        }
      }
      if (auto particleEmittersOff = activeState.properties.find_ref("particleEmittersOff")) {
        auto entries = particleEmittersOff->get().iterateArray();
        if (!entries) return std::unexpected(std::move(entries.error()));
        for (auto const& nameJson : *entries) {
          auto name = nameJson.toString();
          if (!name) return std::unexpected(std::move(name.error()));
          auto entry = m_particleEmitters.get(*name);
          if (!entry) return std::unexpected(std::move(entry.error()));
          entry->get().active.set(false);
        }
      }

      return {};
    });
  if (!statesUpdated) return std::unexpected(std::move(statesUpdated.error()));
  if (version() > 0) {
    for (auto& pair : m_transformationGroups) {
      auto& group = pair.second;
      group.animationBaseTransform = group.animationAffineTransform();
      auto transform = animationGroupTransformation(pair.first, group, group.animationBaseTransform, nullptr);
      if (!transform) return std::unexpected(std::move(transform.error()));
      group.setAnimationAffineTransform(*transform);
    }
  }

  for (auto& pair : m_rotationGroups) {
    auto& rotationGroup = pair.second;
    rotationGroup.previousAngle = rotationGroup.currentAngle;
    if (rotationGroup.angularVelocity == 0.0f)
      rotationGroup.currentAngle = rotationGroup.targetAngle.get();
    else
      rotationGroup.currentAngle = approachAngle(rotationGroup.targetAngle.get(), rotationGroup.currentAngle, rotationGroup.angularVelocity * dt);
  }

  if (dynamicTarget) {
    auto addParticles = [this, dynamicTarget](ParticleEmitter::ParticleConfig const& config, RectF const& offsetRegion, Mat3F const& transformation) {
      for (unsigned i = 0; i < config.count; ++i) {
        Particle particle = config.creator();
        particle.position += config.offset;

        if (!offsetRegion.isNull()) {
          particle.position[0] += m_root.get().random().randf() * offsetRegion.width() + offsetRegion.xMin();
          particle.position[1] += m_root.get().random().randf() * offsetRegion.height() + offsetRegion.yMin();
        }

        float speed = particle.velocity.magnitude();
        particle.velocity = Vec2F::withAngle(transformation.transformAngle(particle.velocity.angle())) * speed;
        particle.position = transformation.transformVec2(particle.position);
        particle.rotation = transformation.transformAngle(particle.rotation);

        particle.size *= m_zoom.get();
        if (config.flip)
          particle.flip = !particle.flip;

        if (transformation.determinant() < 0) {
          particle.flip = !particle.flip;
          particle.rotation += std::numbers::pi;
        }

        dynamicTarget->get().pendingParticles.append(std::move(particle));
      }
    };

    for (auto& pair : m_particleEmitters) {
      Mat3F transformation = Mat3F::identity();
      if (pair.second.anchorPart) {
        auto anchor = partTransformation(*pair.second.anchorPart);
        if (!anchor) return std::unexpected(std::move(anchor.error()));
        transformation = *anchor;
      }
      auto group = groupTransformation(pair.second.transformationGroups);
      if (!group) return std::unexpected(std::move(group.error()));
      transformation = *group * transformation;

      if (pair.second.rotationGroup) {
        auto rotation = m_rotationGroups.get(*pair.second.rotationGroup);
        if (!rotation) return std::unexpected(std::move(rotation.error()));
        auto const& rg = rotation->get();
        Vec2F rotationCenter = pair.second.rotationCenter.value_or(rg.rotationCenter);
        transformation = Mat3F::rotation(rg.currentAngle, rotationCenter) * transformation;
      }

      transformation = globalTransformation() * transformation;

      // assume we emit no particles
      unsigned numEmissionCycles = 0;

      if (pair.second.active.get()) {
        pair.second.timer = std::min(pair.second.timer, 1.0f / (pair.second.emissionRate.get() + pair.second.emissionRateVariance));
        if (pair.second.timer <= 0.0f) {
          // timer causes us to emit one set
          ++numEmissionCycles;
          auto variance = m_root.get().random().randf(-pair.second.emissionRateVariance, pair.second.emissionRateVariance);
          if (!variance) return std::unexpected(std::move(variance.error()));
          pair.second.timer = 1.0f / (pair.second.emissionRate.get() + *variance);
        } else {
          pair.second.timer -= dt;
        }
      }

      auto bursts = pair.second.burstEvent.pullOccurrences();
      for (std::uint64_t i = 0; i < bursts; ++i)
        numEmissionCycles += pair.second.burstCount.get();

      if (numEmissionCycles > 0) {
        RectF rect = pair.second.offsetRegion.get();
        unsigned numToSelect = pair.second.randomSelectCount.get();

        for (unsigned i = 0; i < numEmissionCycles; ++i) {
          if (numToSelect >= pair.second.particleList.size()) {
            for (auto const& particleConfig : pair.second.particleList)
              addParticles(particleConfig, rect, transformation);
          } else {
            List<ParticleEmitter::ParticleConfig> shuffledList = pair.second.particleList;
            m_root.get().random().shuffle(shuffledList);

            for (unsigned i = 0; i < numToSelect; ++i)
              addParticles(shuffledList[i], rect, transformation);
          }
        }
      }
    }

    for (auto& pair : m_sounds) {
      auto const& soundName = pair.first;
      auto& soundEntry = pair.second;

      for (auto signal : soundEntry.signals.receive()) {
        if (signal == SoundSignal::StopAll) {
          for (auto& sound : std::exchange(dynamicTarget->get().independentSounds[soundName], {}))
            sound->stop(soundEntry.volumeRampTime.get());
        } else if (signal == SoundSignal::Play) {
          String soundFile = m_root.get().random().randValueFrom(soundEntry.soundPool.get());
          if (!soundFile.empty()) {
            auto assets = m_root.get().assets();
            if (!assets) return std::unexpected(std::move(assets.error()));
            auto audio = (*assets)->audio(soundFile);
            if (!audio) return std::unexpected(std::move(audio.error()));
            auto instance = AudioInstance::create(**audio);
            if (!instance) return std::unexpected(std::move(instance.error()));
            auto sound = std::move(*instance);
            sound->setRangeMultiplier(soundEntry.rangeMultiplier);
            sound->setLoops(soundEntry.loops.get());
            sound->setPosition(globalTransformation().transformVec2(Vec2F(soundEntry.xPosition.get(), soundEntry.yPosition.get())));
            sound->setVolume(soundEntry.volumeTarget.get(), soundEntry.volumeRampTime.get());
            sound->setPitchMultiplier(soundEntry.pitchMultiplierTarget.get(), soundEntry.pitchMultiplierRampTime.get());
            dynamicTarget->get().independentSounds[soundName].append(sound);
            dynamicTarget->get().pendingAudios.append(std::move(sound));
          }
        }
      }

      // Update all still active independent sounds position, volume, and speed
      for (auto const& activeIndependentSound : dynamicTarget->get().independentSounds.value(soundName)) {
        if (auto basePosition = dynamicTarget->get().currentAudioBasePositions.find_ref(activeIndependentSound))
          basePosition->get() = globalTransformation().transformVec2(Vec2F(soundEntry.xPosition.get(), soundEntry.yPosition.get()));
        activeIndependentSound->setVolume(soundEntry.volumeTarget.get(), soundEntry.volumeRampTime.get());
        activeIndependentSound->setPitchMultiplier(soundEntry.pitchMultiplierTarget.get(), soundEntry.pitchMultiplierRampTime.get());
      }
    }
  }

  for (auto& pair : m_lights) {
    if (pair.second.flicker)
      pair.second.flicker->update(dt);
  }

  for (auto& pair : m_effects) {
    if (pair.second.enabled.get()) {
      auto& effect = pair.second;
      if (effect.timer <= 0.0f)
        effect.timer = effect.time;
      else
        effect.timer -= dt;
    }
  }
  for (auto& pair : m_transformationGroups) {
    auto& group = pair.second;
    auto transform = group.affineTransform();
    group.previousTransform = group.presentationHistory ? group.currentTransform : transform;
    group.previousLocalTransform = group.presentationHistory ? group.currentLocalTransform : group.localTransform;
    group.currentLocalTransform = group.localTransform;
    group.currentTransform = transform;
    group.presentationHistory = true;
  }
  return {};
}

std::expected<void, StarException> NetworkedAnimator::finishAnimations() {
  return m_animatedParts.finishAnimations();
}

Mat3F NetworkedAnimator::TransformationGroup::affineTransform() const {
  return Mat3F(
      xScale.get() * std::cos(xShear.get()), xScale.get() * std::sin(xShear.get()), xTranslation.get(),
      yScale.get() * std::sin(yShear.get()), yScale.get() * std::cos(yShear.get()), yTranslation.get(),
      0, 0, 1
    );
}

void NetworkedAnimator::TransformationGroup::setAffineTransform(Mat3F const& matrix) {
  xTranslation.set(matrix[0][2]);
  yTranslation.set(matrix[1][2]);
  xScale.set(std::sqrt(square(matrix[0][0]) + square(matrix[0][1])));
  yScale.set(std::sqrt(square(matrix[1][0]) + square(matrix[1][1])));
  xShear.set(std::atan2(matrix[0][1], matrix[0][0]));
  yShear.set(std::atan2(matrix[1][0], matrix[1][1]));
}

void NetworkedAnimator::TransformationGroup::setLocalAffineTransform(Mat3F const& matrix) {
  localTransform = matrix;
}

Mat3F NetworkedAnimator::TransformationGroup::localAffineTransform() const {
  return localTransform;
}

void NetworkedAnimator::TransformationGroup::setAnimationAffineTransform(Mat3F const& matrix) {
  xTranslationAnimation = matrix[0][2];
  yTranslationAnimation = matrix[1][2];
  xScaleAnimation = std::sqrt(square(matrix[0][0]) + square(matrix[0][1]));
  yScaleAnimation = std::sqrt(square(matrix[1][0]) + square(matrix[1][1]));
  xShearAnimation = std::atan2(matrix[0][1], matrix[0][0]);
  yShearAnimation = std::atan2(matrix[1][0], matrix[1][1]);
}

Mat3F NetworkedAnimator::TransformationGroup::animationAffineTransform() const {
  return Mat3F(
      xScaleAnimation * std::cos(xShearAnimation), xScaleAnimation * std::sin(xShearAnimation), xTranslationAnimation,
      yScaleAnimation * std::sin(yShearAnimation), yScaleAnimation * std::cos(yShearAnimation), yTranslationAnimation,
      0, 0, 1
    );
}

std::expected<void, StarException> NetworkedAnimator::setupNetStates() {
  clearNetElements();

  if (auto registered = addNetElement(m_processingDirectives); !registered) return std::unexpected(std::move(registered.error()));
  if (auto registered = addNetElement(m_zoom); !registered) return std::unexpected(std::move(registered.error()));
  if (auto registered = addNetElement(m_flipped); !registered) return std::unexpected(std::move(registered.error()));
  if (auto registered = addNetElement(m_flippedRelativeCenterLine); !registered) return std::unexpected(std::move(registered.error()));

  if (auto registered = addNetElement(m_animationRate); !registered) return std::unexpected(std::move(registered.error()));
  m_animationRate.setInterpolator(lerp<float, float>);

  if (auto registered = addNetElement(m_globalTags); !registered) return std::unexpected(std::move(registered.error()));

  for (auto const& part : sorted(m_animatedParts.partNames()))
    if (auto registered = addNetElement(m_partTags[part]); !registered) return std::unexpected(std::move(registered.error()));

  for (auto& pair : m_stateInfo) {
    pair.second.reverse.setCompatibilityVersion(10);
    if (auto registered = addNetElement(pair.second.reverse); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.stateIndex); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.startedEvent); !registered) return std::unexpected(std::move(registered.error()));
  }

  for (auto& pair : m_transformationGroups) {
    if (auto registered = addNetElement(pair.second.xTranslation); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.yTranslation); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.xScale); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.yScale); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.xShear); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.yShear); !registered) return std::unexpected(std::move(registered.error()));

    if (pair.second.interpolated) {
      pair.second.xTranslation.setInterpolator(lerp<float, float>);
      pair.second.yTranslation.setInterpolator(lerp<float, float>);
      pair.second.xScale.setInterpolator(lerp<float, float>);
      pair.second.yScale.setInterpolator(lerp<float, float>);
      pair.second.xShear.setInterpolator(angleLerp<float, float>);
      pair.second.yShear.setInterpolator(angleLerp<float, float>);
    }
  }

  for (auto& pair : m_rotationGroups) {
    if (auto registered = addNetElement(pair.second.targetAngle); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.netImmediateEvent); !registered) return std::unexpected(std::move(registered.error()));
  }

  for (auto& pair : m_particleEmitters) {
    if (auto registered = addNetElement(pair.second.emissionRate); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.burstCount); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.randomSelectCount); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.offsetRegion); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.active); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.burstEvent); !registered) return std::unexpected(std::move(registered.error()));

    pair.second.burstEvent.setIgnoreOccurrencesOnNetLoad(true);
  }

  for (auto& pair : m_lights) {
    if (auto registered = addNetElement(pair.second.active); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.xPosition); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.yPosition); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.color); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.pointAngle); !registered) return std::unexpected(std::move(registered.error()));

    pair.second.xPosition.setFixedPointBase(0.0125f);
    pair.second.yPosition.setFixedPointBase(0.0125f);
    pair.second.pointAngle.setFixedPointBase(0.01f);

    pair.second.xPosition.setInterpolator(lerp<float, float>);
    pair.second.yPosition.setInterpolator(lerp<float, float>);
    pair.second.pointAngle.setInterpolator(angleLerp<float, float>);
  }

  for (auto& pair : m_sounds) {
    if (auto registered = addNetElement(pair.second.soundPool); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.xPosition); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.yPosition); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.volumeTarget); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.volumeRampTime); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.pitchMultiplierTarget); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.pitchMultiplierRampTime); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.loops); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = addNetElement(pair.second.signals); !registered) return std::unexpected(std::move(registered.error()));

    pair.second.xPosition.setFixedPointBase(0.0125f);
    pair.second.yPosition.setFixedPointBase(0.0125f);

    pair.second.xPosition.setInterpolator(lerp<float, float>);
    pair.second.yPosition.setInterpolator(lerp<float, float>);
  }

  for (auto& pair : m_effects)
    if (auto registered = addNetElement(pair.second.enabled); !registered) return std::unexpected(std::move(registered.error()));

  return {};
}

std::expected<void, StarException> NetworkedAnimator::netElementsNeedLoad(bool initial) {
  for (auto& pair : m_stateInfo) {
    if (pair.second.startedEvent.pullOccurred() || initial) {
      auto changed = m_animatedParts.setActiveStateIndex(pair.first, pair.second.stateIndex.get(), true, pair.second.reverse.get());
      if (!changed) return std::unexpected(std::move(changed.error()));
    }
  }

  for (auto& pair : m_rotationGroups) {
    if (pair.second.netImmediateEvent.pullOccurred() || initial) {
      pair.second.currentAngle = pair.second.targetAngle.get();
      pair.second.previousAngle = pair.second.currentAngle;
    }
  }
  return {};
}

std::expected<void, StarException> NetworkedAnimator::netElementsNeedStore() {
  for (auto& pair : m_stateInfo) {
    if (pair.second.wasUpdated || (version() < 1)) {
      pair.second.wasUpdated = false;
      auto index = m_animatedParts.activeStateIndex(pair.first);
      if (!index) return std::unexpected(std::move(index.error()));
      pair.second.stateIndex.set(*index);
      auto reverse = m_animatedParts.activeStateReverse(pair.first);
      if (!reverse) return std::unexpected(std::move(reverse.error()));
      pair.second.reverse.set(*reverse);
    }
  }
  return {};
}

std::uint8_t NetworkedAnimator::version() const {
  return m_animatorVersion;
}

std::expected<Json, StarException> NetworkedAnimator::mergeIncludes(Root& root, Json config, Json includes, String relativePath) {
  Json includedConfigs;
  auto paths = includes.iterateArray();
  if (!paths) return std::unexpected(std::move(paths.error()));
  for (auto const& path : *paths) {
    auto name = path.toString();
    if (!name) return std::unexpected(std::move(name.error()));
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto loaded = (*assets)->json(AssetPath::relative_to(relativePath, *name));
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    Json includeConfig = std::move(*loaded);
    auto hasIncludes = includeConfig.contains("includes");
    if (!hasIncludes) return std::unexpected(std::move(hasIncludes.error()));
    if (*hasIncludes) {
      auto nested = includeConfig.get("includes");
      if (!nested) return std::unexpected(std::move(nested.error()));
      auto merged = mergeIncludes(root, includeConfig, *nested, relativePath);
      if (!merged) return std::unexpected(std::move(merged.error()));
      includeConfig = std::move(*merged);
    }
    includedConfigs = jsonMerge(includedConfigs, includeConfig);
  }
  return jsonMerge(includedConfigs, config);
}

}
