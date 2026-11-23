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

#pragma float_control(precise, on, push)
std::expected<Animation, StarException> Animation::create(Root& root, Json config, String const& directory) {
  Animation animation(root);
  String source_directory;
  if (directory.empty()) {
    if (config.is_type(Json::Type::String)) {
      auto path = config.to_string();
      if (!path)
        return std::unexpected(std::move(path.error()));
      source_directory = AssetPath::directory(*path);
    } else {
      source_directory = "/";
    }
  }
  if (config.is_null())
    config = JsonObject();
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto fetched = (*assets)->fetch_json(config);
  if (!fetched)
    return std::unexpected(std::move(fetched.error()));
  config = std::move(*fetched);
  auto mode_name = config.get_string("mode", "endAndDisappear");
  if (!mode_name)
    return std::unexpected(std::move(mode_name.error()));
  auto mode = enum_value<mode_names>(*mode_name);
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  animation.m_mode = *mode;
  auto base = config.get_string("frames", "");
  if (!base)
    return std::unexpected(std::move(base.error()));
  if (!base->empty()) {
    animation.m_append_frame = !base->utf8().contains("<index>");
    auto resolved = AssetPath::relative_to(directory.empty() ? source_directory : directory, *base);
    if (resolved.utf8().find_first_of("<:?") == std::string::npos)
      animation.m_image.base_path = std::move(resolved);
    else
      animation.m_frame_template = std::move(resolved);
  }

  auto positive_count = [&](String const& field, std::int64_t maximum) -> std::expected<std::int64_t, StarException> {
    auto value = config.get_double(field, 1.0);
    if (!value)
      return std::unexpected(std::move(value.error()).with_context("Reading Animation " + field.utf8()));
    if (!is_finite(*value) || *value < 1 || *value > maximum || *value != std::trunc(*value))
      return std::unexpected(StarException::format("Animation {} must be an integer between 1 and {}", field, maximum));
    return static_cast<std::int64_t>(*value);
  };
  auto frame_count = positive_count("frameNumber", std::numeric_limits<int>::max());
  if (!frame_count)
    return std::unexpected(std::move(frame_count.error()));
  animation.m_frame_count = static_cast<int>(*frame_count);
  auto cycle = config.get_float("animationCycle", 1.0);
  if (!cycle)
    return std::unexpected(std::move(cycle.error()).with_context("Reading Animation animationCycle"));
  if (!is_finite(*cycle) || *cycle <= 0)
    return std::unexpected(StarException("Animation animationCycle must be finite and positive"));
  animation.m_cycle = *cycle;
  animation.m_frame_duration = *cycle / animation.m_frame_count;
  if (animation.m_frame_duration == 0)
    return std::unexpected(StarException("Animation animationCycle / frameNumber underflows"));
  auto loops = config.get_float("loops", 1.0);
  if (!loops)
    return std::unexpected(std::move(loops.error()).with_context("Reading Animation loops"));
  if (!is_finite(*loops) || *loops < 0)
    return std::unexpected(StarException("Animation loops must be finite and nonnegative"));
  animation.m_duration = *cycle * *loops;
  if (!is_finite(animation.m_duration))
    return std::unexpected(StarException("Animation animationCycle * loops overflows"));
  auto angle = config.get_float("angle", 0.0f);
  if (!angle)
    return std::unexpected(std::move(angle.error()));
  if (!is_finite(*angle))
    return std::unexpected(StarException("Animation angle must be finite"));
  animation.m_angle = *angle;
  auto offset_config = config.find("offset");
  if (!offset_config)
    return std::unexpected(std::move(offset_config.error()));
  if (*offset_config) {
    auto offset = json_to_vec2f((*offset_config)->get());
    if (!offset)
      return std::unexpected(std::move(offset.error()).with_context("Reading Animation offset"));
    if (!is_finite((*offset)[0]) || !is_finite((*offset)[1]))
      return std::unexpected(StarException("Animation offset components must fit in finite floats"));
    animation.m_offset = *offset;
  }
  auto centered = config.get_bool("centered", true);
  if (!centered)
    return std::unexpected(std::move(centered.error()));
  animation.m_centered = *centered;
  auto processing = config.get_string("processing", "");
  if (!processing)
    return std::unexpected(std::move(processing.error()));
  animation.m_processing = std::move(*processing);
  auto color_config = config.opt("color");
  if (!color_config)
    return std::unexpected(std::move(color_config.error()));
  animation.m_color = Color::palette_white;
  if (*color_config) {
    auto color = json_to_color(**color_config);
    if (!color)
      return std::unexpected(std::move(color.error()));
    animation.m_color = *color;
  }
  // The largest representable frame index is variants * frame_count - 1.
  constexpr auto frame_limit = std::int64_t{std::numeric_limits<int>::max()} + 1;
  auto variants = positive_count("variants", frame_limit / *frame_count);
  if (!variants)
    return std::unexpected(std::move(variants.error()));
  auto variant = root.random().uniform_int(*variants - 1);
  if (!variant)
    return std::unexpected(std::move(variant.error()));
  animation.m_variant_offset = static_cast<int>(*variant * *frame_count);
  animation.reset();
  return animation;
}
#pragma float_control(pop)

void Animation::set_angle(float angle) {
  m_angle = angle;
}

void Animation::set_processing(DirectivesGroup processing) {
  m_processing = std::move(processing);
}

void Animation::set_color(Color color) {
  m_color = color;
}

void Animation::set_tag(String name, String value) {
  if (m_frame_template.empty())
    return;
  if (name == "index")
    m_automatic_index = false;
  m_tags[std::move(name)] = std::move(value);
  refresh_image();
}

void Animation::clear_tags() {
  if (m_frame_template.empty())
    return;
  m_tags.clear();
  m_automatic_index = false;
  refresh_image();
}

std::expected<Drawable, StarException> Animation::drawable(float pixel_size) const {
  if (m_frame < 0 || (!m_append_frame && m_frame_template.empty()))
    return Drawable();
  auto image_metadata = m_root.get().imageMetadataDatabase();
  if (!image_metadata)
    return std::unexpected(std::move(image_metadata.error()));
  auto drawable = Drawable::make_animation_image(**image_metadata, m_image, pixel_size, m_centered,
      m_offset, m_processing, m_color);
  if (drawable && m_angle != 0)
    drawable->rotate(m_angle);
  return drawable;
}

void Animation::refresh_image() {
  if (m_frame < 0)
    return;
  if (m_frame_template.empty()) {
    if (m_append_frame)
      m_image.sub_path = std::to_string(m_frame);
    return;
  }

  auto index = m_automatic_index || m_append_frame ? String(std::to_string(m_frame)) : String();
  auto path = m_frame_template.lookup_tags([&](String const& key) -> String {
    if (m_automatic_index && key == "index")
      return index;
    auto it = m_tags.find(key);
    if (it != m_tags.end())
      return it->second;
    return "<" + key + ">";
  });
  if (m_append_frame) {
    path.append(":");
    path.append(index);
  }
  m_image = AssetPath(path);
}

void Animation::set_frame(int frame) {
  if (frame == m_frame && m_automatic_index)
    return;
  if (!m_automatic_index) {
    m_tags.erase("index");
    m_automatic_index = true;
  }
  m_frame = frame;
  refresh_image();
}

#pragma float_control(precise, on, push)
int Animation::frame_at(float timer) const {
  float within_cycle = timer < m_cycle ? timer : std::fmod(timer, m_cycle);
  float fraction = within_cycle / m_frame_duration;
  // Compare before converting: even a valid sheet can have a quotient that
  // rounds to 2^31. Widen the comparison, not the legacy frame arithmetic.
  int last = m_frame_count - 1;
  if (!(fraction > 0))
    return m_variant_offset;
  if (static_cast<double>(fraction) >= last)
    return m_variant_offset + last;
  return m_variant_offset + static_cast<int>(fraction);
}

float Animation::advance_timer(float timer, float elapsed) const {
  float next = timer + elapsed;
  if (m_mode != Mode::LoopForever || (next >= 0 && next < m_cycle))
    return next;
  // Avoid losing the phase when two finite clock values overflow their sum.
  if (!is_finite(next))
    return static_cast<float>(std::fmod(static_cast<double>(timer) + elapsed, m_cycle));
  return std::fmod(next, m_cycle);
}
#pragma float_control(pop)

void Animation::update(float dt) {
  if (m_completed)
    return;
  int frame = frame_at(m_timer);
  m_timer = advance_timer(m_timer, dt);
  m_completed = m_mode != Mode::LoopForever && m_timer >= m_duration;
  set_frame(m_completed && m_mode == Mode::EndAndDisappear ? -1 : frame);
}

Animation::PlaybackState Animation::playback_state() const {
  return {m_timer, m_completed};
}

void Animation::sample_playback(PlaybackState const& previous, float elapsed) {
  m_timer = advance_timer(previous.timer, elapsed);
  m_completed = previous.completed || (m_mode != Mode::LoopForever && m_timer >= m_duration);
  int frame = !m_completed ? frame_at(m_timer)
      : m_mode == Mode::EndAndDisappear ? -1 : m_variant_offset + (m_frame_count - 1);
  // Unlike a logical update, a same-frame sample preserves caller tag overrides.
  if (frame != m_frame)
    set_frame(frame);
}

bool Animation::is_complete() const {
  return m_completed;
}

void Animation::reset() {
  m_timer = 0;
  m_completed = false;
  set_frame(0);
}

}
