import star.json;
import star.json_extra;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.random;
import star.ordered_map;
import star.bi_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.game_types;
import star.lexical_cast;

import star.drawable;
import star.asset_source;
import star.assets;
import star.image_metadata_database;
import star.root_base;
import star.configuration;
import star.root;

import star.animation;

import star.math_common;
namespace Star {

std::expected<Animation, StarException> Animation::create(Root& root, Json config, String const& directory) {
  Animation animation(root);
  auto initialized = animation.initialize(std::move(config), directory);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return animation;
}

std::expected<void, StarException> Animation::initialize(Json config, String const& directory) {
  m_directory = directory;
  if (m_directory.empty()) {
    if (config.isType(Json::Type::String)) {
      auto path = config.toString();
      if (!path)
        return std::unexpected(std::move(path.error()));
      m_directory = AssetPath::directory(*path);
    } else {
      m_directory = "/";
    }
  }
  if (config.isNull())
    config = JsonObject();
  auto assets = m_root.get().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto fetched = (*assets)->fetchJson(config);
  if (!fetched)
    return std::unexpected(std::move(fetched.error()));
  config = std::move(*fetched);
  auto modeName = config.getString("mode", "endAndDisappear");
  if (!modeName)
    return std::unexpected(std::move(modeName.error()));
  if (!AnimationModeNames)
    return std::unexpected(AnimationModeNames.error());
  auto mode = AnimationModeNames->getLeft(*modeName);
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  m_mode = mode->get();
  auto base = config.getString("frames", "");
  if (!base)
    return std::unexpected(std::move(base.error()));
  m_base = std::move(*base);
  auto hasIndex = m_base.contains("<index>");
  if (!hasIndex)
    return std::unexpected(std::move(hasIndex.error()));
  m_appendFrame = !*hasIndex;
  auto frameNumber = config.getInt("frameNumber", 1);
  if (!frameNumber)
    return std::unexpected(std::move(frameNumber.error()));
  m_frameNumber = *frameNumber;
  auto cycle = config.getFloat("animationCycle", 1.0);
  if (!cycle)
    return std::unexpected(std::move(cycle.error()));
  m_animationCycle = *cycle;
  auto loops = config.getFloat("loops", 1.0);
  if (!loops)
    return std::unexpected(std::move(loops.error()));
  m_animationTime = m_animationCycle * *loops;
  auto angle = config.getFloat("angle", 0.0f);
  if (!angle)
    return std::unexpected(std::move(angle.error()));
  m_angle = *angle;
  auto offsetConfig = config.get("offset", JsonArray{0.0f, 0.0f});
  if (!offsetConfig)
    return std::unexpected(std::move(offsetConfig.error()));
  auto offset = jsonToVec2F(*offsetConfig);
  if (!offset)
    return std::unexpected(std::move(offset.error()));
  m_offset = *offset;
  auto centered = config.getBool("centered", true);
  if (!centered)
    return std::unexpected(std::move(centered.error()));
  m_centered = *centered;
  auto processing = config.getString("processing", "");
  if (!processing)
    return std::unexpected(std::move(processing.error()));
  m_processing = std::move(*processing);
  auto colorConfig = config.opt("color");
  if (!colorConfig)
    return std::unexpected(std::move(colorConfig.error()));
  m_color = Color::White;
  if (*colorConfig) {
    auto color = jsonToColor(**colorConfig);
    if (!color)
      return std::unexpected(std::move(color.error()));
    m_color = *color;
  }
  auto variants = config.getInt("variants", 1);
  if (!variants)
    return std::unexpected(std::move(variants.error()));
  m_variantOffset = m_root.get().random().randInt(*variants - 1) * m_frameNumber;
  reset();
  return {};
}

void Animation::setAngle(float angle) {
  m_angle = angle;
}

void Animation::setProcessing(DirectivesGroup processing) {
  m_processing = std::move(processing);
}

void Animation::setColor(Color color) {
  m_color = color;
}

void Animation::setTag(String tagName, String tagValue) {
  m_tagValues[std::move(tagName)] = std::move(tagValue);
}

void Animation::clearTags() {
  m_tagValues.clear();
}

std::expected<Drawable, StarException> Animation::drawable(float pixelSize) const {
  if (m_base.empty() || m_frame < 0)
    return Drawable();
  String baseFrame = AssetPath::relativeTo(m_directory, m_base).replaceTags(m_tagValues);
  if (m_appendFrame)
    baseFrame += ":" + toString(m_frame);
  auto imageMetadata = m_root.get().imageMetadataDatabase();
  if (!imageMetadata)
    return std::unexpected(std::move(imageMetadata.error()));
  auto drawable = Drawable::makeImage(**imageMetadata, std::move(baseFrame), pixelSize, m_centered, m_offset);
  if (!drawable)
    return std::unexpected(std::move(drawable.error()));
  auto directives = drawable->imagePart().addDirectivesGroup(**imageMetadata, m_processing, true);
  if (!directives)
    return std::unexpected(std::move(directives.error()));
  drawable->rotate(m_angle);
  drawable->color = m_color;
  return drawable;
}

void Animation::update(float dt) {
  if (m_completed)
    return;

  float time_within_cycle = std::fmod(m_animationTimer, m_animationCycle);
  float time_per_frame = m_animationCycle / m_frameNumber;
  float frame_number = time_within_cycle / time_per_frame;
  m_frame = m_variantOffset + clamp<int>(frame_number, 0, m_frameNumber - 1);
  m_animationTimer += dt;

  if (m_mode == Animation::LoopForever) {
    // to prevent floating point jitter and seizing after a very long time
    m_animationTimer = std::fmod(m_animationTimer, m_animationCycle);
  } else if (m_animationTimer >= m_timeToLive) {
    if (m_mode == Animation::EndAndDisappear)
      m_frame = -1;
    m_completed = true;
  }

  m_tagValues["index"] = toString(m_frame);
}

Animation::PlaybackState Animation::playbackState() const {
  return {m_animationTimer, m_completed};
}

void Animation::samplePlayback(PlaybackState const& previous, float elapsed) {
  int previousFrame = m_frame;
  m_animationTimer = previous.timer + elapsed;
  m_completed = previous.completed;
  if (m_mode == Animation::LoopForever) {
    m_animationTimer = std::fmod(m_animationTimer, m_animationCycle);
  } else if (m_animationTimer >= m_timeToLive) {
    m_completed = true;
  }

  if (m_completed && m_mode == Animation::EndAndDisappear) {
    m_frame = -1;
  } else if (m_completed) {
    m_frame = m_variantOffset + m_frameNumber - 1;
  } else {
    float withinCycle = std::fmod(m_animationTimer, m_animationCycle);
    m_frame = m_variantOffset + clamp<int>(withinCycle / (m_animationCycle / m_frameNumber), 0, m_frameNumber - 1);
  }
  if (m_frame != previousFrame)
    m_tagValues["index"] = toString(m_frame);
}

bool Animation::isComplete() const {
  return m_completed;
}

void Animation::reset() {
  m_frame = 0;
  m_animationTimer = 0;
  m_timeToLive = m_animationTime;
  m_completed = false;
  m_tagValues["index"] = "0";
}

std::expected<EnumMap<Animation::AnimationMode>, StarException> const Animation::AnimationModeNames = EnumMap<AnimationMode>::create({{AnimationMode::Stop, "Stop"},
    {AnimationMode::EndAndDisappear, "EndAndDisappear"},
    {AnimationMode::LoopForever, "LoopForever"}});
}
