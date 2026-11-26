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
import star.lexical_cast;
import star.data_stream_extra;
import star.game_types;
import star.net_element_containers;

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
Mat3F sample_affine(float alpha, Mat3F const& previous, Mat3F const& current) {
  if (alpha <= 0)
    return previous;
  if (alpha >= 1)
    return current;
  float xs = lerp(alpha, std::hypot(previous[0][0], previous[0][1]), std::hypot(current[0][0], current[0][1]));
  float ys = lerp(alpha, std::hypot(previous[1][0], previous[1][1]), std::hypot(current[1][0], current[1][1]));
  float xa = angle_lerp(alpha, std::atan2(previous[0][1], previous[0][0]), std::atan2(current[0][1], current[0][0]));
  float ya = angle_lerp(alpha, std::atan2(previous[1][0], previous[1][1]), std::atan2(current[1][0], current[1][1]));
  return Mat3F(xs * std::cos(xa), xs * std::sin(xa), lerp(alpha, previous[0][2], current[0][2]),
      ys * std::sin(ya), ys * std::cos(ya), lerp(alpha, previous[1][2], current[1][2]), 0, 0, 1);
}
auto process_transforms = [](Mat3F mat, Json const& transforms,
                            JsonObject const& properties) -> std::expected<Mat3F, StarException> {
  auto entries = transforms.iterate_array();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& v : *entries) {
    auto action = v.get_string(0);
    if (!action)
      return std::unexpected(std::move(action.error()));
    if (*action == "reset") {
      mat = Mat3F::identity();
    } else if (*action == "translate") {
      auto offset_json = v.get_array(1);
      if (!offset_json)
        return std::unexpected(std::move(offset_json.error()));
      auto offset = json_to_vec2f(*offset_json);
      if (!offset)
        return std::unexpected(std::move(offset.error()));
      mat.translate(*offset);
    } else if (*action == "rotate" || *action == "rotateDegrees") {
      auto angle = v.get_float(1);
      if (!angle)
        return std::unexpected(std::move(angle.error()));
      auto configured_center = properties.maybe("rotationCenter");
      auto default_center =
        configured_center ? configured_center->to_array() : std::expected<JsonArray, StarException>(JsonArray{0, 0});
      if (!default_center)
        return std::unexpected(std::move(default_center.error()));
      auto center_json = v.get_array(2, *default_center);
      if (!center_json)
        return std::unexpected(std::move(center_json.error()));
      auto center = json_to_vec2f(*center_json);
      if (!center)
        return std::unexpected(std::move(center.error()));
      mat.rotate(*action == "rotate" ? *angle : *angle * std::numbers::pi / 180, *center);
    } else if (*action == "scale") {
      auto scale_json = v.get_array(1);
      if (!scale_json)
        return std::unexpected(std::move(scale_json.error()));
      auto scale = json_to_vec2f(*scale_json);
      if (!scale)
        return std::unexpected(std::move(scale.error()));
      auto configured_center = properties.maybe("scalingCenter");
      auto default_center =
        configured_center ? configured_center->to_array() : std::expected<JsonArray, StarException>(JsonArray{0, 0});
      if (!default_center)
        return std::unexpected(std::move(default_center.error()));
      auto center_json = v.get_array(2, *default_center);
      if (!center_json)
        return std::unexpected(std::move(center_json.error()));
      auto center = json_to_vec2f(*center_json);
      if (!center)
        return std::unexpected(std::move(center.error()));
      mat.scale(*scale, *center);
    } else if (*action == "transform") {
      auto v1 = v.get_float(1);
      if (!v1)
        return std::unexpected(std::move(v1.error()));
      auto v2 = v.get_float(2);
      if (!v2)
        return std::unexpected(std::move(v2.error()));
      auto v3 = v.get_float(3);
      if (!v3)
        return std::unexpected(std::move(v3.error()));
      auto v4 = v.get_float(4);
      if (!v4)
        return std::unexpected(std::move(v4.error()));
      auto v5 = v.get_float(5);
      if (!v5)
        return std::unexpected(std::move(v5.error()));
      auto v6 = v.get_float(6);
      if (!v6)
        return std::unexpected(std::move(v6.error()));
      mat = Mat3F(*v1, *v2, *v3, *v4, *v5, *v6, 0, 0, 1) * mat;
    }
  }
  return mat;
};
}

void NetworkedAnimator::Presentation::invalidate_part_transforms() {
  // Group and state overrides can change any anchored descendant.
  if (++pose_generation == 0) {
    for (auto& part : part_transforms)
      part.pose_generation = 0;
    ++pose_generation;
  }
}

void NetworkedAnimator::Presentation::invalidate_state() {
  invalidate_part_transforms();
  if (++state_generation == 0) {
    for (auto& group : groups)
      group.state_generation = 0;
    ++state_generation;
  }
}

NetworkedAnimator::Presentation& NetworkedAnimator::acquire_presentation() const {
  for (auto& slot : m_presentations) {
    if (!slot.users)
      return slot;
  }
  return m_presentations.emplace_back();
}

NetworkedAnimator::Sample::Sample(NetworkedAnimator const& animator, float interpolation)
  : m_presentation(animator.acquire_presentation()), m_animator(animator), m_interpolation(std::clamp(interpolation, 0.f, 1.f)) {
  auto& presentation = m_presentation.get();
  animator.m_animated_parts.prepare_sample(presentation.parts, m_interpolation);
  presentation.part_transforms.resize(presentation.parts.parts.size());
  presentation.groups.resize(animator.m_transformation_groups.size());
  presentation.invalidate_state();
  for (auto&& [name, tag] : presentation.tags)
    tag.active = false;
  for (auto const& [name, group] : animator.m_transformation_groups) {
    auto& cache = presentation.groups[group.presentation_index];
    cache.local_transform = group.local_transform;
    cache.transform = group.affine_transform();
    if (group.presentation_history) {
      cache.transform = sample_affine(m_interpolation, group.previous_transform, cache.transform);
      cache.local_transform = sample_affine(m_interpolation, group.previous_local_transform, group.local_transform);
    }
  }
  presentation.users = 1;
}

NetworkedAnimator::Sample::Sample(Sample const& sample)
  : m_presentation(sample.m_presentation), m_animator(sample.m_animator), m_interpolation(sample.m_interpolation) {
  ++m_presentation.get().users;
}

NetworkedAnimator::Sample::~Sample() {
  auto& presentation = m_presentation.get();
  if (--presentation.users == 0) {
    for (auto&& [name, attachment] : presentation.drawables)
      attachment = {};
  }
}

NetworkedAnimator::Sample NetworkedAnimator::sample(float interpolation) const {
  return Sample(*this, interpolation);
}

std::expected<String, StarException> NetworkedAnimator::state(String const& state_type) const {
  return query_state(state_type, std::nullopt);
}

std::expected<String, StarException> NetworkedAnimator::Sample::state(String const& state_type) const {
  return m_animator.query_state(state_type, std::cref(*this));
}

std::expected<int, StarException> NetworkedAnimator::state_frame(String const& state_type) const {
  return query_state_frame(state_type, std::nullopt);
}

std::expected<int, StarException> NetworkedAnimator::Sample::state_frame(String const& state_type) const {
  return m_animator.query_state_frame(state_type, std::cref(*this));
}

std::expected<int, StarException> NetworkedAnimator::state_next_frame(String const& state_type) const {
  return query_state_next_frame(state_type, std::nullopt);
}

std::expected<int, StarException> NetworkedAnimator::Sample::state_next_frame(String const& state_type) const {
  return m_animator.query_state_next_frame(state_type, std::cref(*this));
}

std::expected<float, StarException> NetworkedAnimator::state_frame_progress(String const& state_type) const {
  return query_state_frame_progress(state_type, std::nullopt);
}

std::expected<float, StarException> NetworkedAnimator::Sample::state_frame_progress(String const& state_type) const {
  return m_animator.query_state_frame_progress(state_type, std::cref(*this));
}

std::expected<float, StarException> NetworkedAnimator::state_timer(String const& state_type) const {
  return query_state_timer(state_type, std::nullopt);
}

std::expected<float, StarException> NetworkedAnimator::Sample::state_timer(String const& state_type) const {
  return m_animator.query_state_timer(state_type, std::cref(*this));
}

std::expected<bool, StarException> NetworkedAnimator::state_reverse(String const& state_type) const {
  return query_state_reverse(state_type, std::nullopt);
}

std::expected<bool, StarException> NetworkedAnimator::Sample::state_reverse(String const& state_type) const {
  return m_animator.query_state_reverse(state_type, std::cref(*this));
}

std::expected<float, StarException> NetworkedAnimator::state_cycle(String const& state_type, std::optional<String> state) const {
  return query_state_cycle(state_type, std::move(state), std::nullopt);
}

std::expected<float, StarException> NetworkedAnimator::Sample::state_cycle(String const& state_type, std::optional<String> state) const {
  return m_animator.query_state_cycle(state_type, std::move(state), std::cref(*this));
}

std::expected<int, StarException> NetworkedAnimator::state_frames(String const& state_type, std::optional<String> state) const {
  return query_state_frames(state_type, std::move(state), std::nullopt);
}

std::expected<int, StarException> NetworkedAnimator::Sample::state_frames(String const& state_type, std::optional<String> state) const {
  return m_animator.query_state_frames(state_type, std::move(state), std::cref(*this));
}

std::expected<Json, StarException> NetworkedAnimator::state_property(String const& state_type, String const& property_name, std::optional<String> state, std::optional<int> frame) const {
  return query_state_property(state_type, property_name, std::move(state), std::move(frame), std::nullopt);
}

std::expected<Json, StarException> NetworkedAnimator::Sample::state_property(String const& state_type, String const& property_name, std::optional<String> state, std::optional<int> frame) const {
  return m_animator.query_state_property(state_type, property_name, std::move(state), std::move(frame), std::cref(*this));
}

std::expected<Json, StarException> NetworkedAnimator::state_next_property(String const& state_type, String const& property_name) const {
  return query_state_next_property(state_type, property_name, std::nullopt);
}

std::expected<Json, StarException> NetworkedAnimator::Sample::state_next_property(String const& state_type, String const& property_name) const {
  return m_animator.query_state_next_property(state_type, property_name, std::cref(*this));
}

std::expected<Json, StarException> NetworkedAnimator::part_property(String const& part_name, String const& property_name, std::optional<String> state_type, std::optional<String> state, std::optional<int> frame) const {
  return query_part_property(part_name, property_name, std::move(state_type), std::move(state), std::move(frame), std::nullopt);
}

std::expected<Json, StarException> NetworkedAnimator::Sample::part_property(String const& part_name, String const& property_name, std::optional<String> state_type, std::optional<String> state, std::optional<int> frame) const {
  return m_animator.query_part_property(part_name, property_name, std::move(state_type), std::move(state), std::move(frame), std::cref(*this));
}

std::expected<Json, StarException> NetworkedAnimator::part_next_property(String const& part_name, String const& property_name) const {
  return query_part_next_property(part_name, property_name, std::nullopt);
}

std::expected<Json, StarException> NetworkedAnimator::Sample::part_next_property(String const& part_name, String const& property_name) const {
  return m_animator.query_part_next_property(part_name, property_name, std::cref(*this));
}

std::expected<Mat3F, StarException> NetworkedAnimator::group_transformation(std::span<String const> groups) const {
  return query_group_transformation(groups, std::nullopt);
}

std::expected<Mat3F, StarException> NetworkedAnimator::Sample::group_transformation(std::span<String const> groups) const {
  return m_animator.query_group_transformation(groups, std::cref(*this));
}

std::expected<Mat3F, StarException> NetworkedAnimator::part_transformation(String const& part_name) const {
  return query_part_transformation(part_name, std::nullopt, std::nullopt);
}

std::expected<Mat3F, StarException> NetworkedAnimator::Sample::part_transformation(String const& part_name) const {
  return m_animator.query_part_transformation(part_name, std::cref(*this), std::nullopt);
}

std::expected<Mat3F, StarException> NetworkedAnimator::final_part_transformation(String const& part_name) const {
  return query_final_part_transformation(part_name, std::nullopt);
}

std::expected<Mat3F, StarException> NetworkedAnimator::Sample::final_part_transformation(String const& part_name) const {
  return m_animator.query_final_part_transformation(part_name, std::cref(*this));
}

std::expected<std::optional<Vec2F>, StarException> NetworkedAnimator::part_point(String const& part_name, String const& property_name) const {
  return query_part_point(part_name, property_name, std::nullopt);
}

std::expected<std::optional<Vec2F>, StarException> NetworkedAnimator::Sample::part_point(String const& part_name, String const& property_name) const {
  return m_animator.query_part_point(part_name, property_name, std::cref(*this));
}

std::expected<std::optional<PolyF>, StarException> NetworkedAnimator::part_poly(String const& part_name, String const& property_name) const {
  return query_part_poly(part_name, property_name, std::nullopt);
}

std::expected<std::optional<PolyF>, StarException> NetworkedAnimator::Sample::part_poly(String const& part_name, String const& property_name) const {
  return m_animator.query_part_poly(part_name, property_name, std::cref(*this));
}

std::expected<String, StarException> NetworkedAnimator::apply_part_tags(String const& part_name, String apply) const {
  return query_apply_part_tags(part_name, std::move(apply), std::nullopt);
}

std::expected<String, StarException> NetworkedAnimator::Sample::apply_part_tags(String const& part_name, String apply) const {
  return m_animator.query_apply_part_tags(part_name, std::move(apply), std::cref(*this));
}

std::expected<float, StarException> NetworkedAnimator::current_rotation_angle(String const& rotation_group) const {
  return query_current_rotation_angle(rotation_group, std::nullopt);
}

std::expected<float, StarException> NetworkedAnimator::Sample::current_rotation_angle(String const& rotation_group) const {
  return m_animator.query_current_rotation_angle(rotation_group, std::cref(*this));
}

std::expected<List<Drawable>, StarException> NetworkedAnimator::drawables(Vec2F const& translate) const {
  return query_drawables(translate, std::nullopt);
}

std::expected<List<Drawable>, StarException> NetworkedAnimator::Sample::drawables(Vec2F const& translate) const {
  return m_animator.query_drawables(translate, std::cref(*this));
}

std::expected<List<std::pair<Drawable, float>>, StarException> NetworkedAnimator::drawables_with_z_level(Vec2F const& translate) const {
  return query_drawables_with_z_level(translate, std::nullopt);
}

std::expected<List<std::pair<Drawable, float>>, StarException> NetworkedAnimator::Sample::drawables_with_z_level(Vec2F const& translate) const {
  return m_animator.query_drawables_with_z_level(translate, std::cref(*this));
}

std::expected<List<LightSource>, StarException> NetworkedAnimator::light_sources(Vec2F const& translate) const {
  List<LightSource> lights;
  if (auto appended = append_light_sources(lights, translate); !appended)
    return std::unexpected(std::move(appended.error()));
  return lights;
}

std::expected<void, StarException> NetworkedAnimator::append_light_sources(List<LightSource>& lights, Vec2F const& translate) const {
  return append_light_sources(lights, translate, std::nullopt);
}

std::expected<List<LightSource>, StarException> NetworkedAnimator::Sample::light_sources(Vec2F const& translate) const {
  List<LightSource> lights;
  if (auto appended = append_light_sources(lights, translate); !appended)
    return std::unexpected(std::move(appended.error()));
  return lights;
}

std::expected<void, StarException> NetworkedAnimator::Sample::append_light_sources(List<LightSource>& lights, Vec2F const& translate) const {
  return m_animator.append_light_sources(lights, translate, std::cref(*this));
}

std::expected<bool, StarException> NetworkedAnimator::Sample::has_state(String const& type, std::optional<String> const& state) const {
  return m_animator.has_state(type, state);
}

std::expected<bool, StarException> NetworkedAnimator::Sample::set_local_state(String const& type, String const& state, bool start_new, bool reverse) {
  auto& presentation = m_presentation.get();
  auto changed = m_animator.m_animated_parts.set_presentation_state(type, state, start_new, reverse, presentation.parts);
  if (changed && *changed)
    presentation.invalidate_state();
  return changed;
}

void NetworkedAnimator::Sample::set_local_tag(String name, std::optional<String> value) {
  m_presentation.get().tags.set(std::move(name), {true, std::move(value)});
}

void NetworkedAnimator::Sample::set_part_drawables(String const& name, std::optional<std::reference_wrapper<List<Drawable> const>> drawables) {
  m_presentation.get().drawables.set(name, {true, drawables});
}

std::expected<void, StarException> NetworkedAnimator::Sample::reset_local_transformation_group(String const& name) {
  auto group = m_animator.m_transformation_groups.get(name);
  if (!group) return std::unexpected(std::move(group.error()));
  m_presentation.get().groups[group->get().presentation_index].local_transform = Mat3F::identity();
  m_presentation.get().invalidate_part_transforms();
  return {};
}

std::expected<void, StarException> NetworkedAnimator::Sample::rotate_local_transformation_group(String const& name, float rotation, Vec2F const& center) {
  auto group = m_animator.m_transformation_groups.get(name);
  if (!group) return std::unexpected(std::move(group.error()));
  auto& matrix = m_presentation.get().groups[group->get().presentation_index].local_transform;
  matrix = Mat3F::rotation(rotation, center) * matrix;
  m_presentation.get().invalidate_part_transforms();
  return {};
}

std::expected<void, StarException> NetworkedAnimator::Sample::translate_local_transformation_group(String const& name, Vec2F const& translation) {
  auto group = m_animator.m_transformation_groups.get(name);
  if (!group) return std::unexpected(std::move(group.error()));
  auto& matrix = m_presentation.get().groups[group->get().presentation_index].local_transform;
  matrix = Mat3F::translation(translation) * matrix;
  m_presentation.get().invalidate_part_transforms();
  return {};
}

std::expected<Mat3F, StarException> NetworkedAnimator::Sample::get_local_transformation_group(String const& name) const {
  auto group = m_animator.m_transformation_groups.get(name);
  if (!group) return std::unexpected(std::move(group.error()));
  return m_presentation.get().groups[group->get().presentation_index].local_transform;
}

NetworkedAnimator::DynamicTarget::~DynamicTarget() {
  stop_audio();
}

List<std::shared_ptr<AudioInstance>> NetworkedAnimator::DynamicTarget::pull_new_audios() {
  // Position updates normally prune the registries. Event-only consumers can
  // skip them, so also prune when publishing audio without a position update.
  if (!std::exchange(positioned_since_audio_pull, false) && !pending_audios.empty() && !current_audio_base_positions.empty())
    clear_finished_audio();
  for (auto const& audio : pending_audios) {
    Vec2F base_position = audio->position().value_or(Vec2F{});
    current_audio_base_positions[audio] = base_position;
    audio->set_position(position + base_position);
  }
  return std::exchange(pending_audios, {});
}

List<Particle> NetworkedAnimator::DynamicTarget::pull_new_particles() {
  for (auto& particle : pending_particles)
    particle.position += position;
  return std::exchange(pending_particles, {});
}

void NetworkedAnimator::DynamicTarget::stop_audio() {
  for (auto const& [audio, base_position] : current_audio_base_positions) {
    if (audio->loops() != 0)
      audio->stop();
  }
}

void NetworkedAnimator::DynamicTarget::update_position(Vec2F const& p) {
  clear_finished_audio();
  positioned_since_audio_pull = true;
  position = p;
  for (auto const& [audio, base_position] : current_audio_base_positions)
    audio->set_position(base_position + p);
}

void NetworkedAnimator::DynamicTarget::clear_finished_audio() {
  for (auto&& [name, sound] : state_persistent_sounds) {
    if (sound.audio && sound.audio->finished())
      sound.audio.reset();
  }

  auto finished = [](std::shared_ptr<AudioInstance> const& audio) { return audio->finished(); };
  Star::erase_if(independent_sounds, [&](std::ranges::range_reference_t<decltype(independent_sounds)> pair) {
    auto& sounds = pair.second;
    sounds.erase(std::remove_if(sounds.begin(), sounds.end(), finished), sounds.end());
    return sounds.empty();
  });
  Star::erase_if(current_audio_base_positions, [&](std::ranges::range_reference_t<decltype(current_audio_base_positions)> pair) {
    return finished(pair.first);
  });
}

NetworkedAnimator::NetworkedAnimator(ConstructionKey, Root& root) : m_root(root) {
  m_zoom.set(1.0f);
  m_flipped.set(false);
  m_flipped_relative_center_line.set(0.0f);
  m_animation_rate.set(1.0f);
  m_animator_version = 0;
}

std::expected<std::shared_ptr<NetworkedAnimator>, StarException> NetworkedAnimator::create(Root& root, Json config, String relative_path) {
  auto animator = std::make_shared<NetworkedAnimator>(ConstructionKey{}, root);
  auto loaded = animator->load_config(std::move(config), std::move(relative_path));
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  return animator;
}

std::expected<void, StarException> NetworkedAnimator::load_config(Json config, String relative_path) {
  auto& root = m_root.get();
  if (config.is_null()) return setup_net_states();
  if (config.type() == Json::Type::String) {
    auto path = config.to_string();
    if (!path) return std::unexpected(std::move(path.error()));
    if (relative_path.empty()) relative_path = *path;
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto loaded = (*assets)->json(AssetPath::relative_to(relative_path, *path));
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    config = std::move(*loaded);
  } else if (relative_path.empty()) {
    relative_path = "/";
  }
  auto version_value = config.get_uint("version", 0);
  if (!version_value) return std::unexpected(std::move(version_value.error()));
  m_animator_version = *version_value;
  if (version() > 0) {
    auto has_includes = config.contains("includes");
    if (!has_includes) return std::unexpected(std::move(has_includes.error()));
    if (*has_includes) {
      auto includes = config.get("includes");
      if (!includes) return std::unexpected(std::move(includes.error()));
      StringList include_chain;
      auto merged = merge_includes(root, config, *includes, relative_path, include_chain);
      if (!merged) return std::unexpected(std::move(merged.error()));
      config = std::move(*merged);
    }
  }
  auto parts_config = config.get("animatedParts", JsonObject());
  if (!parts_config) return std::unexpected(std::move(parts_config.error()));
  auto parts = AnimatedPartSet::create(*parts_config, version());
  if (!parts) return std::unexpected(std::move(parts.error()));
  m_animated_parts = std::move(*parts);
  m_animation_state_types = m_animated_parts.state_types();
  m_relative_path = AssetPath::directory(relative_path);
  auto global_tag_defaults_config = config.get("globalTagDefaults", JsonObject());
  if (!global_tag_defaults_config) return std::unexpected(std::move(global_tag_defaults_config.error()));
  auto global_tag_defaults_entries = global_tag_defaults_config->iterate_object();
  if (!global_tag_defaults_entries) return std::unexpected(std::move(global_tag_defaults_entries.error()));
  for (auto const& pair : *global_tag_defaults_entries) {
    auto value = pair.second.to_string();
    if (!value) return std::unexpected(std::move(value.error()));
    set_global_tag(pair.first, *value);
  }
  auto part_tag_defaults_config = config.get("partTagDefaults", JsonObject());
  if (!part_tag_defaults_config) return std::unexpected(std::move(part_tag_defaults_config.error()));
  auto part_tag_defaults_entries = part_tag_defaults_config->iterate_object();
  if (!part_tag_defaults_entries) return std::unexpected(std::move(part_tag_defaults_entries.error()));
  for (auto const& pair : *part_tag_defaults_entries) {
    auto tags = pair.second.iterate_object();
    if (!tags) return std::unexpected(std::move(tags.error()));
    for (auto const& tag : *tags) {
      auto value = tag.second.to_string();
      if (!value) return std::unexpected(std::move(value.error()));
      set_part_tag(pair.first, tag.first, *value);
    }
  }
  auto transformation_groups_config = config.get("transformationGroups", JsonObject());
  if (!transformation_groups_config) return std::unexpected(std::move(transformation_groups_config.error()));
  auto transformation_groups_entries = transformation_groups_config->iterate_object();
  if (!transformation_groups_entries) return std::unexpected(std::move(transformation_groups_entries.error()));
  std::size_t group_index = 0;
  for (auto const& pair : *transformation_groups_entries) {
    auto& tg = m_transformation_groups[pair.first];
    tg.presentation_index = group_index++;
    auto interpolated = pair.second.get_bool("interpolated", false);
    if (!interpolated) return std::unexpected(std::move(interpolated.error()));
    tg.interpolated = *interpolated;
    tg.set_affine_transform(Mat3F::identity());
    tg.set_animation_affine_transform(Mat3F::identity());
    tg.set_local_affine_transform(Mat3F::identity());
  }
  auto rotation_groups_config = config.get("rotationGroups", JsonObject());
  if (!rotation_groups_config) return std::unexpected(std::move(rotation_groups_config.error()));
  auto rotation_groups_entries = rotation_groups_config->iterate_object();
  if (!rotation_groups_entries) return std::unexpected(std::move(rotation_groups_entries.error()));
  for (auto const& pair : *rotation_groups_entries) {
    auto& rotation_group = m_rotation_groups[pair.first];
    auto velocity = pair.second.get_float("angularVelocity", 0.0f);
    if (!velocity) return std::unexpected(std::move(velocity.error()));
    rotation_group.angular_velocity = std::move(*velocity);
    auto center_json = pair.second.get("rotationCenter", JsonArray{0, 0});
    if (!center_json) return std::unexpected(std::move(center_json.error()));
    auto center = json_to_vec2f(*center_json);
    if (!center) return std::unexpected(std::move(center.error()));
    rotation_group.rotation_center = std::move(*center);
  }
  auto particle_emitters_config = config.get("particleEmitters", JsonObject());
  if (!particle_emitters_config) return std::unexpected(std::move(particle_emitters_config.error()));
  auto particle_emitters_entries = particle_emitters_config->iterate_object();
  if (!particle_emitters_entries) return std::unexpected(std::move(particle_emitters_entries.error()));
  for (auto const& pair : *particle_emitters_entries) {
    auto& emitter = m_particle_emitters[pair.first];
    auto const& particle_emitter_config = pair.second;
    auto rate = particle_emitter_config.get_float("emissionRate", 1.0f);
    if (!rate) return std::unexpected(std::move(rate.error()));
    emitter.emission_rate.set(*rate);
    auto variance = particle_emitter_config.get_float("emissionRateVariance", 0.0f);
    if (!variance) return std::unexpected(std::move(variance.error()));
    emitter.emission_rate_variance = std::move(*variance);
    RectF offset_region = RectF::null();
    auto region = particle_emitter_config.opt("offsetRegion");
    if (!region) return std::unexpected(std::move(region.error()));
    if (*region) {
      auto region_value = json_to_rectf(**region);
      if (!region_value) return std::unexpected(std::move(region_value.error()));
      offset_region = *region_value;
    }
    emitter.offset_region.set(offset_region);
    auto anchor = particle_emitter_config.opt_string("anchorPart");
    if (!anchor) return std::unexpected(std::move(anchor.error()));
    emitter.anchor_part = std::move(*anchor);
    auto groups_json = particle_emitter_config.get("transformationGroups", JsonArray());
    if (!groups_json) return std::unexpected(std::move(groups_json.error()));
    auto groups = json_to_string_list(*groups_json);
    if (!groups) return std::unexpected(std::move(groups.error()));
    emitter.transformation_groups = std::move(*groups);
    auto rotation = particle_emitter_config.opt_string("rotationGroup");
    if (!rotation) return std::unexpected(std::move(rotation.error()));
    emitter.rotation_group = std::move(*rotation);
    auto center = particle_emitter_config.opt("rotationCenter");
    if (!center) return std::unexpected(std::move(center.error()));
    if (*center) {
      auto center_value = json_to_vec2f(**center);
      if (!center_value) return std::unexpected(std::move(center_value.error()));
      emitter.rotation_center = *center_value;
    }
    auto particles_json = particle_emitter_config.get("particles");
    if (!particles_json) return std::unexpected(std::move(particles_json.error()));
    auto particles = particles_json->iterate_array();
    if (!particles) return std::unexpected(std::move(particles.error()));
    for (auto const& particle_config : *particles) {
      auto database = root.particleDatabase();
      if (!database) return std::unexpected(std::move(database.error()));
      auto particle = particle_config.get("particle");
      if (!particle) return std::unexpected(std::move(particle.error()));
      auto creator = (*database)->particleCreator(*particle, relative_path);
      if (!creator) return std::unexpected(std::move(creator.error()));
      auto count = particle_config.get_uint("count", 1);
      if (!count) return std::unexpected(std::move(count.error()));
      auto offset_json = particle_config.get("offset", JsonArray{0, 0});
      if (!offset_json) return std::unexpected(std::move(offset_json.error()));
      auto offset = json_to_vec2f(*offset_json);
      if (!offset) return std::unexpected(std::move(offset.error()));
      auto flip = particle_config.get_bool("flip", false);
      if (!flip) return std::unexpected(std::move(flip.error()));
      emitter.particle_list.push_back({*creator, static_cast<unsigned>(*count), *offset, *flip});
    }
    auto burst_count = particle_emitter_config.get_uint("burstCount", 1);
    if (!burst_count) return std::unexpected(std::move(burst_count.error()));
    emitter.burst_count.set(*burst_count);
    auto random_count = particle_emitter_config.get_uint("randomSelectCount", emitter.particle_list.size());
    if (!random_count) return std::unexpected(std::move(random_count.error()));
    emitter.random_select_count.set(*random_count);
    auto active = particle_emitter_config.get_bool("active", false);
    if (!active) return std::unexpected(std::move(active.error()));
    emitter.active.set(*active);
  }
  auto lights_config = config.get("lights", JsonObject());
  if (!lights_config) return std::unexpected(std::move(lights_config.error()));
  auto lights_entries = lights_config->iterate_object();
  if (!lights_entries) return std::unexpected(std::move(lights_entries.error()));
  for (auto const& pair : *lights_entries) {
    auto& light = m_lights[pair.first];
    auto const& light_config = pair.second;
    auto active = light_config.get_bool("active", true);
    if (!active) return std::unexpected(std::move(active.error()));
    light.active.set(*active);
    Vec2F light_position{};
    auto position = light_config.opt("position");
    if (!position) return std::unexpected(std::move(position.error()));
    if (*position) {
      auto position_value = json_to_vec2f(**position);
      if (!position_value) return std::unexpected(std::move(position_value.error()));
      light_position = *position_value;
    }
    light.x_position.set(light_position[0]);
    light.y_position.set(light_position[1]);
    Color color = Color::palette_white;
    auto color_config = light_config.opt("color");
    if (!color_config) return std::unexpected(std::move(color_config.error()));
    if (*color_config) {
      auto color_config_value = json_to_color(**color_config);
      if (!color_config_value) return std::unexpected(std::move(color_config_value.error()));
      color = *color_config_value;
    }
    light.color.set(color);
    auto anchor = light_config.opt_string("anchorPart");
    if (!anchor) return std::unexpected(std::move(anchor.error()));
    light.anchor_part = std::move(*anchor);
    auto groups_json = light_config.get("transformationGroups", JsonArray());
    if (!groups_json) return std::unexpected(std::move(groups_json.error()));
    auto groups = json_to_string_list(*groups_json);
    if (!groups) return std::unexpected(std::move(groups.error()));
    light.transformation_groups = std::move(*groups);
    auto rotation = light_config.opt_string("rotationGroup");
    if (!rotation) return std::unexpected(std::move(rotation.error()));
    light.rotation_group = std::move(*rotation);
    auto center = light_config.opt("rotationCenter");
    if (!center) return std::unexpected(std::move(center.error()));
    if (*center) {
      auto center_value = json_to_vec2f(**center);
      if (!center_value) return std::unexpected(std::move(center_value.error()));
      light.rotation_center = *center_value;
    }
    auto has_flicker = light_config.contains("flickerPeriod");
    if (!has_flicker) return std::unexpected(std::move(has_flicker.error()));
    if (*has_flicker) {
      auto period = light_config.get_float("flickerPeriod");
      if (!period) return std::unexpected(std::move(period.error()));
      auto minimum = light_config.get_float("flickerMinIntensity", 0.0);
      if (!minimum) return std::unexpected(std::move(minimum.error()));
      auto maximum = light_config.get_float("flickerMaxIntensity", 0.0);
      if (!maximum) return std::unexpected(std::move(maximum.error()));
      auto period_variance = light_config.get_float("flickerPeriodVariance", 0.0);
      if (!period_variance) return std::unexpected(std::move(period_variance.error()));
      auto intensity_variance = light_config.get_float("flickerIntensityVariance", 0.0);
      if (!intensity_variance) return std::unexpected(std::move(intensity_variance.error()));
      auto flicker = PeriodicFunction<float>::create(root.random(), *period, *minimum, *maximum, *period_variance, *intensity_variance);
      if (!flicker)
        return std::unexpected(NetworkedAnimatorException(Star::format("Invalid flicker settings for animator light '{}'", pair.first),
            std::move(flicker.error())));
      light.flicker = std::move(*flicker);
    }
    auto angle = light_config.get_float("pointAngle", 0.0f);
    if (!angle) return std::unexpected(std::move(angle.error()));
    light.point_angle.set(*angle * Constants::degrees_to_radians);
    auto point = light_config.get_bool("pointLight", false);
    if (!point) return std::unexpected(std::move(point.error()));
    light.point_light = std::move(*point);
    auto beam = light_config.get_float("pointBeam", 0.0f);
    if (!beam) return std::unexpected(std::move(beam.error()));
    light.point_beam = std::move(*beam);
    auto ambience = light_config.get_float("beamAmbience", 0.0f);
    if (!ambience) return std::unexpected(std::move(ambience.error()));
    light.beam_ambience = std::move(*ambience);
  }
  auto sounds_config = config.get("sounds", JsonObject());
  if (!sounds_config) return std::unexpected(std::move(sounds_config.error()));
  auto sounds_entries = sounds_config->iterate_object();
  if (!sounds_entries) return std::unexpected(std::move(sounds_entries.error()));
  for (auto const& pair : *sounds_entries) {
    auto& sound = m_sounds[pair.first];
    auto const& sound_config = pair.second;
    if (sound_config.is_type(Json::Type::Array)) {
      sound.range_multiplier = 1.0f;
      auto pool = json_to_string_list(sound_config);
      if (!pool) return std::unexpected(std::move(pool.error()));
      sound.sound_pool.set(pool->transformed(
        [relative_path = m_relative_path](String const& path) { return AssetPath::relative_to(relative_path, path); }));
      sound.volume_target.set(1.0f);
      sound.volume_ramp_time.set(0.0f);
      sound.pitch_multiplier_target.set(1.0f);
      sound.pitch_multiplier_ramp_time.set(0.0f);
    } else {
      auto range = sound_config.get_float("rangeMultiplier", 1.0f);
      if (!range) return std::unexpected(std::move(range.error()));
      sound.range_multiplier = std::move(*range);
      Vec2F sound_position{};
      auto position = sound_config.opt("position");
      if (!position) return std::unexpected(std::move(position.error()));
      if (*position) {
        auto position_value = json_to_vec2f(**position);
        if (!position_value) return std::unexpected(std::move(position_value.error()));
        sound_position = *position_value;
      }
      sound.x_position.set(sound_position[0]);
      sound.y_position.set(sound_position[1]);
      auto volume = sound_config.get_float("volume", 1.0f);
      if (!volume) return std::unexpected(std::move(volume.error()));
      sound.volume_target.set(*volume);
      auto volume_ramp = sound_config.get_float("volumeRampTime", 0.0f);
      if (!volume_ramp) return std::unexpected(std::move(volume_ramp.error()));
      sound.volume_ramp_time.set(*volume_ramp);
      auto pitch = sound_config.get_float("pitchMultiplier", 1.0f);
      if (!pitch) return std::unexpected(std::move(pitch.error()));
      sound.pitch_multiplier_target.set(*pitch);
      auto pitch_ramp = sound_config.get_float("pitchMultiplierRampTime", 0.0f);
      if (!pitch_ramp) return std::unexpected(std::move(pitch_ramp.error()));
      sound.pitch_multiplier_ramp_time.set(*pitch_ramp);
      auto pool_json = sound_config.get("pool", JsonArray());
      if (!pool_json) return std::unexpected(std::move(pool_json.error()));
      auto pool = json_to_string_list(*pool_json);
      if (!pool) return std::unexpected(std::move(pool.error()));
      sound.sound_pool.set(pool->transformed(
        [relative_path = m_relative_path](String const& path) { return AssetPath::relative_to(relative_path, path); }));
    }
  }
  auto effects_config = config.get("effects", JsonObject());
  if (!effects_config) return std::unexpected(std::move(effects_config.error()));
  auto effects_entries = effects_config->iterate_object();
  if (!effects_entries) return std::unexpected(std::move(effects_entries.error()));
  for (auto const& pair : *effects_entries) {
    auto& effect = m_effects[pair.first];
    auto type = pair.second.get_string("type");
    if (!type) return std::unexpected(std::move(type.error()));
    effect.type = std::move(*type);
    auto time = pair.second.get_float("time", 0.0f);
    if (!time) return std::unexpected(std::move(time.error()));
    effect.time = std::move(*time);
    auto directives = pair.second.get_string("directives");
    if (!directives) return std::unexpected(std::move(directives.error()));
    effect.directives = *directives;
  }
  m_transformation_groups.sort_by_key();
  m_rotation_groups.sort_by_key();
  m_particle_emitters.sort_by_key();
  m_lights.sort_by_key();
  m_sounds.sort_by_key();
  m_effects.sort_by_key();
  for (auto const& state_type : m_animated_parts.state_types())
    m_state_info[state_type].was_updated = true;
  m_state_info.sort_by_key();
  return setup_net_states();
}

std::expected<std::shared_ptr<NetworkedAnimator>, StarException> NetworkedAnimator::create(Root& root) {
  auto result = std::make_shared<NetworkedAnimator>(ConstructionKey{}, root);
  auto registered = result->setup_net_states();
  if (!registered) return std::unexpected(std::move(registered.error()));
  return result;
}

std::expected<std::shared_ptr<NetworkedAnimator>, StarException> NetworkedAnimator::create(NetworkedAnimator const& animator) {
  auto result = std::make_shared<NetworkedAnimator>(ConstructionKey{}, animator.m_root.get());
  result->m_relative_path = animator.m_relative_path;
  result->m_animated_parts = animator.m_animated_parts;
  result->m_animation_state_types = animator.m_animation_state_types;
  result->m_state_info = animator.m_state_info;
  result->m_transformation_groups = animator.m_transformation_groups;
  result->m_rotation_groups = animator.m_rotation_groups;
  result->m_particle_emitters = animator.m_particle_emitters;
  result->m_lights = animator.m_lights;
  result->m_sounds = animator.m_sounds;
  result->m_effects = animator.m_effects;
  result->m_processing_directives = animator.m_processing_directives;
  result->m_zoom = animator.m_zoom;
  result->m_flipped = animator.m_flipped;
  result->m_flipped_relative_center_line = animator.m_flipped_relative_center_line;
  result->m_animation_rate = animator.m_animation_rate;
  result->m_global_tags = animator.m_global_tags;
  result->m_part_tags = animator.m_part_tags;
  result->m_cached_part_drawables = animator.m_cached_part_drawables;
  result->m_part_drawables = animator.m_part_drawables;
  result->m_local_tags = animator.m_local_tags;
  result->m_animator_version = animator.m_animator_version;
  auto registered = result->setup_net_states();
  if (!registered) return std::unexpected(std::move(registered.error()));

  return result;
}

StringList NetworkedAnimator::state_types() const {
  return m_animated_parts.state_types();
}

std::expected<StringList, StarException> NetworkedAnimator::states(String const& state_type) const {
  return m_animated_parts.states(state_type);
}

std::expected<bool, StarException> NetworkedAnimator::set_state(String const& state_type, String const& state, bool start_new, bool reverse) {
  auto changed = m_animated_parts.set_active_state(state_type, state, start_new, reverse);
  if (!changed) return std::unexpected(std::move(changed.error()));
  if (*changed) {
    m_state_info[state_type].was_updated = true;
    m_state_info[state_type].started_event.trigger();
    return true;
  }
  return false;
}

std::expected<bool, StarException> NetworkedAnimator::set_local_state(String const& state_type, String const& state, bool start_new, bool reverse) {
  return m_animated_parts.set_active_state(state_type, state, start_new, reverse);
}

std::expected<void, StarException> NetworkedAnimator::validate_local_state(String const& type, String const& state) const {
  auto target = m_animated_parts.get_state(type, state);
  if (!target) return std::unexpected(std::move(target.error()));
  return {};
}

std::expected<String, StarException> NetworkedAnimator::query_state(String const& state_type, SampleView sample) const {
  auto state = sample ? m_animated_parts.active_state(state_type, sample->get().m_presentation.get().parts) : m_animated_parts.active_state(state_type);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().state_name;
}
std::expected<int, StarException> NetworkedAnimator::query_state_frame(String const& state_type, SampleView sample) const {
  auto state = sample ? m_animated_parts.active_state(state_type, sample->get().m_presentation.get().parts) : m_animated_parts.active_state(state_type);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().frame;
}
std::expected<int, StarException> NetworkedAnimator::query_state_next_frame(String const& state_type, SampleView sample) const {
  auto state = sample ? m_animated_parts.active_state(state_type, sample->get().m_presentation.get().parts) : m_animated_parts.active_state(state_type);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().next_frame;
}
std::expected<float, StarException> NetworkedAnimator::query_state_frame_progress(String const& state_type, SampleView sample) const {
  auto state = sample ? m_animated_parts.active_state(state_type, sample->get().m_presentation.get().parts) : m_animated_parts.active_state(state_type);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().frame_progress;
}
std::expected<float, StarException> NetworkedAnimator::query_state_timer(String const& state_type, SampleView sample) const {
  auto state = sample ? m_animated_parts.active_state(state_type, sample->get().m_presentation.get().parts) : m_animated_parts.active_state(state_type);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().timer;
}
std::expected<bool, StarException> NetworkedAnimator::query_state_reverse(String const& state_type, SampleView sample) const {
  auto state = sample ? m_animated_parts.active_state(state_type, sample->get().m_presentation.get().parts) : m_animated_parts.active_state(state_type);
  if (!state) return std::unexpected(std::move(state.error()));
  return state->get().reverse;
}

std::expected<float, StarException> NetworkedAnimator::query_state_cycle(String const& state_type, std::optional<String> state, SampleView sample) const {
  auto active = sample ? m_animated_parts.active_state(state_type, sample->get().m_presentation.get().parts) : m_animated_parts.active_state(state_type);
  if (!active) return std::unexpected(std::move(active.error()));
  auto selected = m_animated_parts.get_state(state_type, state ? *state : active->get().state_name);
  if (!selected) return std::unexpected(std::move(selected.error()));
  return selected->get().cycle;
}
std::expected<int, StarException> NetworkedAnimator::query_state_frames(String const& state_type, std::optional<String> state, SampleView sample) const {
  auto active = sample ? m_animated_parts.active_state(state_type, sample->get().m_presentation.get().parts) : m_animated_parts.active_state(state_type);
  if (!active) return std::unexpected(std::move(active.error()));
  auto selected = m_animated_parts.get_state(state_type, state ? *state : active->get().state_name);
  if (!selected) return std::unexpected(std::move(selected.error()));
  return selected->get().frames;
}

std::expected<bool, StarException> NetworkedAnimator::has_state(String const& state_type, std::optional<String> const& state) const {
  auto contains = m_animated_parts.state_types().contains(state_type);
  if (!contains) return std::unexpected(std::move(contains.error()));
  if (!*contains) return false;
  if (state) {
    auto states = m_animated_parts.states(state_type);
    if (!states) return std::unexpected(std::move(states.error()));
    return states->contains(*state);
  }
  return true;
}

StringMap<AnimatedPartSet::Part> const& NetworkedAnimator::const_parts() const {
  return m_animated_parts.const_parts();
}

StringMap<AnimatedPartSet::Part>& NetworkedAnimator::parts() {
  return m_animated_parts.parts();
}

StringList NetworkedAnimator::part_names() const {
  return m_animated_parts.part_names();
}

std::expected<Json, StarException> NetworkedAnimator::query_state_property(String const& state_type, String const& property_name, std::optional<String> state, std::optional<int> frame, SampleView sample) const {
  if (state) {
    if (!frame) return std::unexpected(NetworkedAnimatorException("State property requires a frame"));
    return m_animated_parts.get_state_frame_property(state_type, property_name, *state, *frame);
  }
  auto active = sample ? m_animated_parts.active_state(state_type, sample->get().m_presentation.get().parts) : m_animated_parts.active_state(state_type);
  if (!active) return std::unexpected(std::move(active.error()));
  return active->get().properties.value(property_name);
}
std::expected<Json, StarException> NetworkedAnimator::query_state_next_property(String const& state_type, String const& property_name, SampleView sample) const {
  auto active = sample ? m_animated_parts.active_state(state_type, sample->get().m_presentation.get().parts) : m_animated_parts.active_state(state_type);
  if (!active) return std::unexpected(std::move(active.error()));
  return active->get().next_properties.value(property_name);
}

std::expected<Json, StarException> NetworkedAnimator::query_part_property(String const& part_name, String const& property_name, std::optional<String> state_type, std::optional<String> state, std::optional<int> frame, SampleView sample) const {
  if (state_type) {
    if (!state || !frame) return std::unexpected(NetworkedAnimatorException("Part state property requires a state and frame"));
    return m_animated_parts.get_part_state_frame_property(part_name, property_name, *state_type, *state, *frame);
  }
  auto active = sample ? m_animated_parts.active_part(part_name, sample->get().m_presentation.get().parts) : m_animated_parts.active_part(part_name);
  if (!active) return std::unexpected(std::move(active.error()));
  return active->get().properties.value(property_name);
}
std::expected<Json, StarException> NetworkedAnimator::query_part_next_property(String const& part_name, String const& property_name, SampleView sample) const {
  auto active = sample ? m_animated_parts.active_part(part_name, sample->get().m_presentation.get().parts) : m_animated_parts.active_part(part_name);
  if (!active) return std::unexpected(std::move(active.error()));
  return active->get().next_properties.value(property_name);
}

Mat3F NetworkedAnimator::global_transformation() const {
  Mat3F transformation = Mat3F::scaling(m_zoom.get());
  if (m_flipped.get())
    transformation = Mat3F::scaling(Vec2F(-1, 1), Vec2F(m_flipped_relative_center_line.get(), 0)) * transformation;
  return transformation;
}

std::expected<Mat3F, StarException> NetworkedAnimator::animation_group_transformation(String const& name, TransformationGroup const& group, Mat3F const& base, SampleView sample) const {
  for (auto const& state_type : m_animation_state_types) {
    auto state = sample ? m_animated_parts.active_state(state_type, sample->get().m_presentation.get().parts) : m_animated_parts.active_state(state_type);
    if (!state) return std::unexpected(std::move(state.error()));
    auto const& active = state->get();
    if (auto transforms = active.properties.find_ref(name)) {
      auto matrix = process_transforms(base, transforms->get(), active.properties);
      if (!matrix) return std::unexpected(std::move(matrix.error()));
      if (group.interpolated) {
        if (auto next = active.next_properties.find_ref(name)) {
          auto next_matrix = process_transforms(base, next->get(), active.next_properties);
          if (!next_matrix) return std::unexpected(std::move(next_matrix.error()));
          return sample_affine(active.frame_progress, *matrix, *next_matrix);
        }
      }
      return matrix;
    }
  }
  return base;
}

std::expected<Mat3F, StarException> NetworkedAnimator::query_group_transformation(std::span<String const> groups, SampleView sample) const {
  auto mat = Mat3F::identity();
  for (auto const& name : groups) {
    auto group = m_transformation_groups.get(name);
    if (!group) return std::unexpected(std::move(group.error()));
    auto const& value = group->get();
    if (sample) {
      auto& presentation = sample->get().m_presentation.get();
      auto& cache = presentation.groups[value.presentation_index];
      if (presentation.parts.logical_reset) {
        mat = cache.transform * cache.local_transform * value.animation_affine_transform() * mat;
      } else {
        if (cache.state_generation != presentation.state_generation) {
          auto sampled = animation_group_transformation(name, value, value.animation_base_transform, sample);
          if (!sampled) return std::unexpected(std::move(sampled.error()));
          cache.animation_transform = *sampled;
          cache.state_generation = presentation.state_generation;
        }
        mat = cache.transform * cache.local_transform * cache.animation_transform * mat;
      }
    } else {
      mat = value.affine_transform() * value.local_affine_transform() * value.animation_affine_transform() * mat;
    }
  }
  return mat;
}

std::expected<Mat3F, StarException> NetworkedAnimator::query_part_transformation(String const& part_name, SampleView sample,
    std::optional<std::reference_wrapper<AnchorPath const>> path) const {
  auto definition = m_animated_parts.const_parts().get(part_name);
  if (!definition) return std::unexpected(std::move(definition.error()));
  auto const& entry = definition->get();
  std::optional<std::reference_wrapper<Presentation::PartTransform>> cache;
  if (sample) {
    auto& presentation = sample->get().m_presentation.get();
    if (entry.presentation_index >= presentation.part_transforms.size())
      return std::unexpected(AnimatedPartSetException("Invalid animation part index"));
    auto& cached = presentation.part_transforms[entry.presentation_index];
    if (cached.pose_generation == presentation.pose_generation)
      return cached.transform;
    cache = std::ref(cached);
  }
  for (auto ancestor = path; ancestor; ancestor = ancestor->get().parent) {
    if (ancestor->get().name == part_name) {
      StringList names;
      for (; path; path = path->get().parent)
        names.push_back(path->get().name);
      std::reverse(names.begin(), names.end());
      return std::unexpected(NetworkedAnimatorException::format("Animation anchorPart cycle: {} -> {}", names.join(" -> "), part_name));
    }
  }

  auto active = sample ? m_animated_parts.active_part(entry, sample->get().m_presentation.get().parts) : m_animated_parts.active_part(entry);
  if (!active) return std::unexpected(std::move(active.error()));
  auto const& part = active->get();
  Mat3F transformation = Mat3F::identity();
  if (auto offset_json = part.properties.find_ref("offset"); offset_json && !offset_json->get().is_null()) {
    auto offset = json_to_vec2f(offset_json->get());
    if (!offset) return std::unexpected(std::move(offset.error()));
    transformation = Mat3F::translation(*offset) * transformation;
  }
  transformation = part.animation_affine_transform() * transformation;
  auto group_transform = Mat3F::identity();
  if (auto groups_json = part.properties.find_ref("transformationGroups")) {
    auto groups = json_to_string_list(groups_json->get());
    if (!groups) return std::unexpected(std::move(groups.error()));
    auto group = query_group_transformation(*groups, sample);
    if (!group) return std::unexpected(std::move(group.error()));
    group_transform = *group;
  }
  transformation = group_transform * transformation;
  if (auto rotation_json = part.properties.find_ref("rotationGroup"); rotation_json && !rotation_json->get().is_null()) {
    auto rotation_name = rotation_json->get().to_string();
    if (!rotation_name) return std::unexpected(std::move(rotation_name.error()));
    auto rotation = m_rotation_groups.get(*rotation_name);
    if (!rotation) return std::unexpected(std::move(rotation.error()));
    auto const& value = rotation->get();
    auto center = value.rotation_center;
    if (auto center_json = part.properties.find_ref("rotationCenter"); center_json && !center_json->get().is_null()) {
      auto configured_center = json_to_vec2f(center_json->get());
      if (!configured_center) return std::unexpected(std::move(configured_center.error()));
      center = *configured_center;
    }
    float angle = sample ? angle_lerp(sample->get().interpolation(), value.previous_angle, value.current_angle) : value.current_angle;
    transformation = Mat3F::rotation(angle, center) * transformation;
  }
  if (auto anchor_json = part.properties.find_ref("anchorPart")) {
    auto anchor_name = anchor_json->get().to_string();
    if (!anchor_name) return std::unexpected(std::move(anchor_name.error()));
    // A stack-local link is unwound automatically on both success and failure.
    AnchorPath current{part_name, path};
    auto anchor = query_part_transformation(*anchor_name, sample, std::cref(current));
    if (!anchor) return std::unexpected(std::move(anchor.error()));
    transformation = *anchor * transformation;
  }
  if (cache) {
    cache->get().transform = transformation;
    cache->get().pose_generation = sample->get().m_presentation.get().pose_generation;
  }
  return transformation;
}

std::expected<Mat3F, StarException> NetworkedAnimator::query_final_part_transformation(String const& part_name, SampleView sample) const {
  auto part = query_part_transformation(part_name, sample, std::nullopt);
  if (!part) return std::unexpected(std::move(part.error()));
  return global_transformation() * *part;
}

std::expected<std::optional<Vec2F>, StarException> NetworkedAnimator::query_part_point(String const& part_name, String const& property_name, SampleView sample) const {
  auto active = sample ? m_animated_parts.active_part(part_name, sample->get().m_presentation.get().parts) : m_animated_parts.active_part(part_name);
  if (!active) return std::unexpected(std::move(active.error()));
  auto property = active->get().properties.value(property_name);
  if (!property) return std::optional<Vec2F>{};
  auto value = json_to_vec2f(property);
  if (!value) return std::unexpected(std::move(value.error()));
  auto transform = query_final_part_transformation(part_name, sample);
  if (!transform) return std::unexpected(std::move(transform.error()));
  return std::optional<Vec2F>(transform->transform_point(*value));
}

std::expected<std::optional<PolyF>, StarException> NetworkedAnimator::query_part_poly(String const& part_name, String const& property_name, SampleView sample) const {
  auto active = sample ? m_animated_parts.active_part(part_name, sample->get().m_presentation.get().parts) : m_animated_parts.active_part(part_name);
  if (!active) return std::unexpected(std::move(active.error()));
  auto property = active->get().properties.value(property_name);
  if (!property) return std::optional<PolyF>{};
  auto value = json_to_polyf(property);
  if (!value) return std::unexpected(std::move(value.error()));
  auto transform = query_final_part_transformation(part_name, sample);
  if (!transform) return std::unexpected(std::move(transform.error()));
  value->transform(*transform);
  return std::optional<PolyF>(std::move(*value));
}

void NetworkedAnimator::set_global_tag(String tag_name, std::optional<String> tag_value) {
  if (tag_value)
    m_global_tags.set(std::move(tag_name), std::move(*tag_value));
  else
    m_global_tags.remove(tag_name);
}

void NetworkedAnimator::remove_global_tag(String const& tag_name) {
  m_global_tags.remove(tag_name);
}

std::optional<std::reference_wrapper<String const>> NetworkedAnimator::global_tag(String const& tag_name) const {
  return m_global_tags.find_ref(tag_name);
}


void NetworkedAnimator::set_part_tag(String const& part_type, String tag_name, std::optional<String> tag_value) {
  if (tag_value)
    m_part_tags[part_type].set(std::move(tag_name), std::move(*tag_value));
  else
    m_part_tags[part_type].remove(tag_name);
}

void NetworkedAnimator::set_local_tag(String tag_name, std::optional<String> tag_value) {
  if (tag_value)
    m_local_tags.set(tag_name, *tag_value);
  else
    m_local_tags.remove(tag_name);
}

void NetworkedAnimator::set_part_drawables(String const& part_name, List<Drawable> drawables) {
  m_part_drawables.set(part_name, std::move(drawables));
}
void NetworkedAnimator::add_part_drawables(String const& part_name, List<Drawable> drawables) {
  m_part_drawables[part_name].append_range(drawables);
}
std::expected<HashMap<String, String>, StarException> NetworkedAnimator::animation_tags(SampleView sample) const {
  auto tags = m_local_tags;
  if (sample) {
    for (auto const& [name, tag] : sample->get().m_presentation.get().tags) {
      if (!tag.active)
        continue;
      if (tag.value)
        tags.set(name, *tag.value);
      else
        tags.remove(name);
    }
  }
  if (version() > 0) {
    tags.set("relativePath", m_relative_path);
    for (auto const& name : m_animation_state_types) {
      auto active = sample ? m_animated_parts.active_state(name, sample->get().m_presentation.get().parts)
                           : m_animated_parts.active_state(name);
      if (!active) return std::unexpected(std::move(active.error()));
      auto const& state = active->get();
      tags.set(name + "_frame", static_cast<String>(Star::to_string(state.frame + 1)));
      tags.set(name + "_frameIndex", static_cast<String>(Star::to_string(state.frame)));
      tags.set(name + "_state", state.state_name);
      if (auto property = state.properties.find_ref("animationTags")) {
        auto authored = property->get().iterate_object();
        if (!authored) return std::unexpected(std::move(authored.error()));
        for (auto const& [tag, entry] : *authored) {
          if (!tags.contains(tag)) {
            auto value = entry.to_string();
            if (!value) return std::unexpected(std::move(value.error()));
            tags.set(tag, std::move(*value));
          }
        }
      }
    }
  }
  return tags;
}

namespace {
class PartTagContext {
public:
  PartTagContext(HashMap<String, String> const& animation, NetElementHashMap<String, String> const& part,
      NetElementHashMap<String, String> const& global, std::optional<unsigned> frame)
    : m_animation(animation), m_part(part), m_global(global), m_frame(frame) {}

  StringView lookup(StringView tag) const {
    if (auto value = m_animation.get().find_ref(tag)) return StringView(value->get());
    if (auto value = m_part.get().find_ref(tag)) return StringView(value->get());
    if (auto value = m_global.get().find_ref(tag)) return StringView(value->get());
    return StringView("default");
  }

  StringView lookup_image(StringView tag) {
    if (tag == "frame")
      return frame_tag(m_frame_text, 1);
    if (tag == "frameIndex")
      return frame_tag(m_frame_index_text, 0);
    return lookup(tag);
  }

private:
  StringView frame_tag(std::optional<String>& text, unsigned offset) const {
    if (!m_frame)
      return StringView("default");
    if (!text)
      text.emplace(static_cast<String>(Star::to_string(*m_frame + offset)));
    return StringView(*text);
  }

  std::reference_wrapper<HashMap<String, String> const> m_animation;
  std::reference_wrapper<NetElementHashMap<String, String> const> m_part;
  std::reference_wrapper<NetElementHashMap<String, String> const> m_global;
  std::optional<unsigned> m_frame;
  std::optional<String> m_frame_text;
  std::optional<String> m_frame_index_text;
};
}

std::expected<String, StarException> NetworkedAnimator::query_apply_part_tags(String const& part_name, String apply, SampleView sample) const {
  auto active = sample ? m_animated_parts.active_part(part_name, sample->get().m_presentation.get().parts)
                       : m_animated_parts.active_part(part_name);
  if (!active) return std::unexpected(std::move(active.error()));
  auto part_tags = m_part_tags.get(part_name);
  if (!part_tags) return std::unexpected(std::move(part_tags.error()));
  auto tags = animation_tags(sample);
  if (!tags) return std::unexpected(std::move(tags.error()));
  auto const& state = active->get().active_state;
  PartTagContext context(*tags, part_tags->get(), m_global_tags, state ? std::optional<unsigned>{state->frame} : std::nullopt);
  auto applied = apply.maybe_lookup_tags_view([&](StringView tag) { return context.lookup_image(tag); });
  return applied ? std::move(*applied) : std::move(apply);
}


void NetworkedAnimator::set_processing_directives(Directives const& directives) {
  m_processing_directives.set(directives);
}

void NetworkedAnimator::set_zoom(float zoom) {
  m_zoom.set(zoom);
}

bool NetworkedAnimator::flipped() const {
  return m_flipped.get();
}

float NetworkedAnimator::flipped_relative_center_line() const {
  return m_flipped_relative_center_line.get();
}

void NetworkedAnimator::set_flipped(bool flipped, float relative_center_line) {
  m_flipped.set(flipped);
  m_flipped_relative_center_line.set(relative_center_line);
}

void NetworkedAnimator::set_animation_rate(float rate) {
  m_animation_rate.set(rate);
}

float NetworkedAnimator::animation_rate() {
  return m_animation_rate.get();
}

bool NetworkedAnimator::has_rotation_group(String const& rotation_group) const {
  return m_rotation_groups.contains(rotation_group);
}

std::expected<void, StarException> NetworkedAnimator::rotate_group(String const& rotation_group, float target_angle, bool immediate) {
  auto group_result = m_rotation_groups.get(rotation_group);
  if (!group_result) return std::unexpected(std::move(group_result.error()));
  auto& group = group_result->get();
  group.target_angle.set(target_angle);

  if (immediate) {
    group.current_angle = target_angle;
    group.previous_angle = target_angle;
    group.net_immediate_event.trigger();
  }
  return {};
}

std::expected<float, StarException> NetworkedAnimator::query_current_rotation_angle(String const& rotation_group, SampleView sample) const {
  auto entry = m_rotation_groups.get(rotation_group);
  if (!entry) return std::unexpected(std::move(entry.error()));
  auto const& group = entry->get();
  return sample ? angle_lerp(sample->get().interpolation(), group.previous_angle, group.current_angle) : group.current_angle;
}

bool NetworkedAnimator::has_transformation_group(String const& transformation_group) const {
  return m_transformation_groups.contains(transformation_group);
}

std::expected<void, StarException> NetworkedAnimator::translate_transformation_group(String const& transformation_group, Vec2F const& translation) {
  auto group_result = m_transformation_groups.get(transformation_group);
  if (!group_result) return std::unexpected(std::move(group_result.error()));
  auto& group = group_result->get();
  group.set_affine_transform(Mat3F::translation(translation) * group.affine_transform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::rotate_transformation_group(
    String const& transformation_group, float rotation, Vec2F const& rotation_center) {
  auto group_result = m_transformation_groups.get(transformation_group);
  if (!group_result) return std::unexpected(std::move(group_result.error()));
  auto& group = group_result->get();
  group.set_affine_transform(Mat3F::rotation(rotation, rotation_center) * group.affine_transform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::scale_transformation_group(
    String const& transformation_group, float scale, Vec2F const& scale_center) {
  auto group_result = m_transformation_groups.get(transformation_group);
  if (!group_result) return std::unexpected(std::move(group_result.error()));
  auto& group = group_result->get();
  group.set_affine_transform(Mat3F::scaling(scale, scale_center) * group.affine_transform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::scale_transformation_group(
    String const& transformation_group, Vec2F const& scale, Vec2F const& scale_center) {
  auto group_result = m_transformation_groups.get(transformation_group);
  if (!group_result) return std::unexpected(std::move(group_result.error()));
  auto& group = group_result->get();
  group.set_affine_transform(Mat3F::scaling(scale, scale_center) * group.affine_transform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::transform_transformation_group(
    String const& transformation_group, float a, float b, float c, float d, float tx, float ty) {
  auto group_result = m_transformation_groups.get(transformation_group);
  if (!group_result) return std::unexpected(std::move(group_result.error()));
  auto& group = group_result->get();
  Mat3F transform = Mat3F(a, b, tx, c, d, ty, 0, 0, 1);
  group.set_affine_transform(transform * group.affine_transform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::reset_transformation_group(String const& transformation_group) {
  auto entry = m_transformation_groups.get(transformation_group);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().set_affine_transform(Mat3F::identity());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_transformation_group(String const& transformation_group, Mat3F transform) {
  auto entry = m_transformation_groups.get(transformation_group);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().set_affine_transform(transform);
  return {};
}

std::expected<Mat3F, StarException> NetworkedAnimator::get_transformation_group(String const& transformation_group) {
  auto entry = m_transformation_groups.get(transformation_group);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return entry->get().affine_transform();
}
std::expected<void, StarException> NetworkedAnimator::translate_local_transformation_group(String const& transformation_group, Vec2F const& translation) {
  auto group_result = m_transformation_groups.get(transformation_group);
  if (!group_result) return std::unexpected(std::move(group_result.error()));
  auto& group = group_result->get();
  group.set_local_affine_transform(Mat3F::translation(translation) * group.local_affine_transform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::rotate_local_transformation_group(
    String const& transformation_group, float rotation, Vec2F const& rotation_center) {
  auto group_result = m_transformation_groups.get(transformation_group);
  if (!group_result) return std::unexpected(std::move(group_result.error()));
  auto& group = group_result->get();
  group.set_local_affine_transform(Mat3F::rotation(rotation, rotation_center) * group.local_affine_transform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::scale_local_transformation_group(
    String const& transformation_group, float scale, Vec2F const& scale_center) {
  auto group_result = m_transformation_groups.get(transformation_group);
  if (!group_result) return std::unexpected(std::move(group_result.error()));
  auto& group = group_result->get();
  group.set_local_affine_transform(Mat3F::scaling(scale, scale_center) * group.local_affine_transform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::scale_local_transformation_group(
    String const& transformation_group, Vec2F const& scale, Vec2F const& scale_center) {
  auto group_result = m_transformation_groups.get(transformation_group);
  if (!group_result) return std::unexpected(std::move(group_result.error()));
  auto& group = group_result->get();
  group.set_local_affine_transform(Mat3F::scaling(scale, scale_center) * group.local_affine_transform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::transform_local_transformation_group(
    String const& transformation_group, float a, float b, float c, float d, float tx, float ty) {
  auto group_result = m_transformation_groups.get(transformation_group);
  if (!group_result) return std::unexpected(std::move(group_result.error()));
  auto& group = group_result->get();
  Mat3F transform = Mat3F(a, b, tx, c, d, ty, 0, 0, 1);
  group.set_local_affine_transform(transform * group.local_affine_transform());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::reset_local_transformation_group(String const& transformation_group) {
  auto entry = m_transformation_groups.get(transformation_group);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().set_local_affine_transform(Mat3F::identity());
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_local_transformation_group(String const& transformation_group, Mat3F transform) {
  auto entry = m_transformation_groups.get(transformation_group);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().set_local_affine_transform(transform);
  return {};
}

std::expected<Mat3F, StarException> NetworkedAnimator::get_local_transformation_group(String const& transformation_group) {
  auto entry = m_transformation_groups.get(transformation_group);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return entry->get().local_affine_transform();
}

bool NetworkedAnimator::has_particle_emitter(String const& emitter_name) const {
  return m_particle_emitters.contains(emitter_name);
}

std::expected<void, StarException> NetworkedAnimator::set_particle_emitter_active(String const& emitter_name, bool active) {
  auto entry = m_particle_emitters.get(emitter_name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().active.set(active);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_particle_emitter_emission_rate(String const& emitter_name, float emission_rate) {
  auto entry = m_particle_emitters.get(emitter_name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().emission_rate.set(emission_rate);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_particle_emitter_offset_region(String const& emitter_name, RectF const& offset_region) {
  auto entry = m_particle_emitters.get(emitter_name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().offset_region.set(offset_region);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_particle_emitter_burst_count(String const& emitter_name, unsigned burst_count) {
  auto entry = m_particle_emitters.get(emitter_name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().burst_count.set(burst_count);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::burst_particle_emitter(String const& emitter_name) {
  auto entry = m_particle_emitters.get(emitter_name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().burst_event.trigger();
  return {};
}

bool NetworkedAnimator::has_light(String const& light_name) const {
  return m_lights.contains(light_name);
}

std::expected<void, StarException> NetworkedAnimator::set_light_active(String const& light_name, bool active) {
  auto entry = m_lights.get(light_name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().active.set(active);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_light_position(String const& light_name, Vec2F position) {
  auto light_result = m_lights.get(light_name);
  if (!light_result) return std::unexpected(std::move(light_result.error()));
  auto& light = light_result->get();
  light.x_position.set(position[0]);
  light.y_position.set(position[1]);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_light_color(String const& light_name, Color color) {
  auto entry = m_lights.get(light_name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().color.set(color);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_light_point_angle(String const& light_name, float angle) {
  auto entry = m_lights.get(light_name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().point_angle.set(angle * Constants::degrees_to_radians);
  return {};
}

bool NetworkedAnimator::has_sound(String const& sound_name) const {
  return m_sounds.contains(sound_name);
}

std::expected<void, StarException> NetworkedAnimator::set_sound_pool(String const& sound_name, StringList sound_pool) {
  auto entry = m_sounds.get(sound_name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().sound_pool.set(std::move(sound_pool));
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_sound_position(String const& sound_name, Vec2F const& position) {
  auto sound_result = m_sounds.get(sound_name);
  if (!sound_result) return std::unexpected(std::move(sound_result.error()));
  auto& sound = sound_result->get();
  sound.x_position.set(position[0]);
  sound.y_position.set(position[1]);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_sound_volume(String const& sound_name, float volume, float ramp_time) {
  auto sound_result = m_sounds.get(sound_name);
  if (!sound_result) return std::unexpected(std::move(sound_result.error()));
  auto& sound = sound_result->get();
  sound.volume_target.set(volume);
  sound.volume_ramp_time.set(ramp_time);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_sound_pitch_multiplier(String const& sound_name, float pitch_multiplier, float ramp_time) {
  auto sound_result = m_sounds.get(sound_name);
  if (!sound_result) return std::unexpected(std::move(sound_result.error()));
  auto& sound = sound_result->get();
  sound.pitch_multiplier_target.set(pitch_multiplier);
  sound.pitch_multiplier_ramp_time.set(ramp_time);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::play_sound(String const& sound_name, int loops) {
  auto sound_result = m_sounds.get(sound_name);
  if (!sound_result) return std::unexpected(std::move(sound_result.error()));
  auto& sound = sound_result->get();
  sound.loops.set(loops);
  sound.signals.send(SoundSignal::Play);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::stop_all_sounds(String const& sound_name, float ramp_time) {
  auto sound_result = m_sounds.get(sound_name);
  if (!sound_result) return std::unexpected(std::move(sound_result.error()));
  auto& sound = sound_result->get();
  sound.volume_ramp_time.set(ramp_time);
  sound.signals.send(SoundSignal::StopAll);
  return {};
}

std::expected<void, StarException> NetworkedAnimator::set_effect_enabled(String const& effect, bool enabled) {
  auto entry = m_effects.get(effect);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().enabled.set(enabled);
  return {};
}

std::expected<List<Drawable>, StarException> NetworkedAnimator::query_drawables(Vec2F const& position, SampleView sample) const {
  return collect_drawables<Drawable>(position, sample);
}

std::expected<List<std::pair<Drawable, float>>, StarException> NetworkedAnimator::query_drawables_with_z_level(Vec2F const& position, SampleView sample) const {
  return collect_drawables<std::pair<Drawable, float>>(position, sample);
}

template <typename Output>
  requires (std::same_as<Output, Drawable> || std::same_as<Output, std::pair<Drawable, float>>)
std::expected<List<Output>, StarException> NetworkedAnimator::collect_drawables(Vec2F const& position, SampleView sample) const {
  List<Output> drawables;
  std::size_t const part_count = m_animated_parts.const_parts().size();
  if (!part_count)
    return drawables;

  List<std::reference_wrapper<Directives const>> effect_directives;
  for (auto const& [name, effect] : m_effects) {
    if (!effect.enabled.get())
      continue;
    if (effect.type == "flash") {
      if (!(effect.timer > effect.time / 2))
        continue;
    } else if (effect.type != "directive") {
      return std::unexpected(NetworkedAnimatorException(Star::format("No such NetworkedAnimator effect type '{}'", effect.type)));
    }
    if (!effect.directives.empty())
      effect_directives.push_back(std::cref(effect.directives));
  }
  auto tags = animation_tags(sample);
  if (!tags) return std::unexpected(std::move(tags.error()));

  struct Part {
    std::reference_wrapper<AnimatedPartSet::ActivePartInformation const> active;
    std::reference_wrapper<String const> name;
    std::span<Drawable const> attachments;
    float z;
  };
  List<Part> parts;
  parts.reserve(part_count);
  std::size_t drawable_count = part_count;
  bool const flipped = m_flipped.get();
  bool const has_animation_tags = version() > 0;
  auto attachments_for = [&](String const& name) -> std::span<Drawable const> {
    if (sample) {
      auto override = sample->get().m_presentation.get().drawables.find_ref(name);
      if (override && override->get().active) {
        auto const& drawables = override->get().drawables;
        return drawables ? std::span<Drawable const>(drawables->get()) : std::span<Drawable const>{};
      }
    }
    if (auto attached = m_part_drawables.find_ref(name))
      return attached->get();
    return {};
  };
  auto collect_part = [&](String const& name, AnimatedPartSet::ActivePartInformation const& active) -> std::expected<void, StarException> {
    std::optional<float> z;
    if (flipped) {
      auto flipped_z = active.properties.value("flippedZLevel").opt_float();
      if (!flipped_z) return std::unexpected(std::move(flipped_z.error()));
      z = *flipped_z;
    }
    if (!z) {
      auto normal_z = active.properties.value("zLevel").opt_float();
      if (!normal_z) return std::unexpected(std::move(normal_z.error()));
      z = *normal_z;
    }
    auto attachments = attachments_for(name);
    drawable_count += attachments.size();
    parts.push_back({std::cref(active), std::cref(name), attachments, z.value_or(0.0f)});
    return {};
  };
  auto collected = sample ? m_animated_parts.for_each_active_part(collect_part, sample->get().m_presentation.get().parts)
                          : m_animated_parts.for_each_active_part(collect_part);
  if (!collected) return std::unexpected(std::move(collected.error()));
  std::sort(parts.begin(), parts.end(), [](Part const& a, Part const& b) { return a.z < b.z; });

  drawables.reserve(drawable_count);
  auto emit = [&](Drawable drawable, Mat3F const& transform, float z) {
    drawable.transform(transform);
    if constexpr (std::same_as<Output, Drawable>)
      drawables.push_back(std::move(drawable));
    else
      drawables.emplace_back(std::move(drawable), z);
  };
  std::shared_ptr<ImageMetadataDatabase const> metadata;
  auto ensure_metadata = [&]() -> std::expected<void, StarException> {
    if (!metadata) {
      auto loaded = m_root.get().image_metadata_database();
      if (!loaded) return std::unexpected(std::move(loaded.error()));
      metadata = std::move(*loaded);
    }
    return {};
  };
  Mat3F const global_transform = global_transformation();
  for (auto const& entry : parts) {
    auto const& active = entry.active.get();
    auto const& name = entry.name.get();
    auto flipped_image = has_animation_tags && flipped ? active.properties.find_ref("flippedImage") : std::nullopt;
    bool const use_flipped_image = flipped_image && !flipped_image->get().is_null();
    auto image_property = use_flipped_image ? flipped_image : active.properties.find_ref("image");
    std::shared_ptr<String const> image;
    if (image_property && (use_flipped_image || image_property->get().is_type(Json::Type::String))) {
      auto checked = image_property->get().string_ptr();
      if (!checked) return std::unexpected(std::move(checked.error()));
      image = std::move(*checked);
    }
    auto centered_value = active.properties.value("centered").opt_bool();
    if (!centered_value) return std::unexpected(std::move(centered_value.error()));
    auto fullbright = active.properties.value("fullbright").opt_bool();
    if (!fullbright) return std::unexpected(std::move(fullbright.error()));
    bool const centered = centered_value->value_or(true);

    auto part_tags = m_part_tags.get(name);
    if (!part_tags) return std::unexpected(std::move(part_tags.error()));
    auto const& state = active.active_state;
    PartTagContext context(*tags, part_tags->get(), m_global_tags, state ? std::optional<unsigned>{state->frame} : std::nullopt);
    auto processing = [&](JsonObject const& properties) -> std::expected<Directives, StarException> {
      auto property = properties.find_ref("processingDirectives");
      if (!property || property->get().is_null()) return Directives{};
      auto text = property->get().string_ptr();
      if (!text) return std::unexpected(std::move(text.error()));
      if ((**text).empty()) return Directives{};
      if (has_animation_tags) {
        auto applied = (**text).maybe_lookup_tags_view([&](StringView tag) { return context.lookup(tag); });
        if (applied) return Directives(std::move(*applied));
      }
      return Directives(**text);
    };
    auto part_directives = processing(active.properties);
    if (!part_directives) return std::unexpected(std::move(part_directives.error()));
    auto state_directives = state ? processing(state->properties) : std::expected<Directives, StarException>{};
    if (!state_directives) return std::unexpected(std::move(state_directives.error()));

    auto part_transform = query_part_transformation(name, sample, std::nullopt);
    if (!part_transform) return std::unexpected(std::move(part_transform.error()));
    auto transform = global_transform * *part_transform;
    transform.translate(position);

    if (image) {
      auto processed = image->maybe_lookup_tags_view([&](StringView tag) { return context.lookup_image(tag); });
      String const& used_image = processed ? *processed : *image;
      if (!used_image.empty()) {
        auto initial = used_image.at(0);
        if (!initial) return std::unexpected(std::move(initial.error()));
        if (*initial != ':' && *initial != '?') {
          auto cached = m_cached_part_drawables.find(name);
          if (cached == m_cached_part_drawables.end() || cached->second.path != used_image || cached->second.centered != centered) {
            auto loaded = ensure_metadata();
            if (!loaded) return std::unexpected(std::move(loaded.error()));
            String resolved;
            if (*initial != '/')
              resolved = AssetPath::relative_to(m_relative_path, used_image);
            auto created = Drawable::make_image(*metadata, *initial == '/' ? used_image : resolved, 1.0f / tile_pixels, centered, Vec2F());
            if (!created) return std::unexpected(std::move(created.error()));
            CachedPartDrawable next{processed ? std::move(*processed) : String(used_image), centered, std::move(*created)};
            if (cached == m_cached_part_drawables.end())
              cached = m_cached_part_drawables.emplace(name, std::move(next)).first;
            else
              cached->second = std::move(next);
          }

          Drawable drawable = cached->second.drawable;
          auto& image_part = drawable.image_part();
          auto append = [&](Directives const& directives) -> std::expected<void, StarException> {
            if (directives.empty()) return {};
            if (!centered) {
              image_part.image.directives += directives;
              return {};
            }
            auto loaded = ensure_metadata();
            if (!loaded) return std::unexpected(std::move(loaded.error()));
            auto added = image_part.add_directives(*metadata, directives, true);
            if (!added) return std::unexpected(std::move(added.error()));
            return {};
          };
          auto added_base = append(m_processing_directives.get());
          if (!added_base) return std::unexpected(std::move(added_base.error()));
          for (Directives const& directives : effect_directives) {
            auto added = append(directives);
            if (!added) return std::unexpected(std::move(added.error()));
          }
          auto added_part = append(*part_directives);
          if (!added_part) return std::unexpected(std::move(added_part.error()));
          auto added_state = append(*state_directives);
          if (!added_state) return std::unexpected(std::move(added_state.error()));
          drawable.fullbright = fullbright->value_or(false);
          emit(std::move(drawable), transform, entry.z);
        }
      }
    }
    for (auto const& attached : entry.attachments)
      emit(attached, transform, entry.z);
  }
  return drawables;
}

std::expected<void, StarException> NetworkedAnimator::append_light_sources(List<LightSource>& lights, Vec2F const& translate, SampleView sample) const {
  auto initial_size = lights.size();
  auto rollback = [&](StarException&& error) {
    lights.resize(initial_size);
    return std::unexpected(std::move(error));
  };
  std::optional<Mat3F> global;
  bool const flipped = m_flipped.get();
  for (auto const& [name, light] : m_lights) {
    if (!light.active.get())
      continue;

    Vec2F position = {light.x_position.get(), light.y_position.get()};
    float point_angle = normalize_angle(light.point_angle.get());
    Mat3F transformation = Mat3F::identity();
    if (light.anchor_part) {
      auto anchor = query_part_transformation(*light.anchor_part, sample, std::nullopt);
      if (!anchor) return rollback(std::move(anchor.error()));
      transformation = *anchor;
    }
    auto group = query_group_transformation(light.transformation_groups, sample);
    if (!group) return rollback(std::move(group.error()));
    transformation = *group * transformation;
    position = transformation.transform_point(position);
    point_angle = transformation.transform_angle(point_angle);
    if (light.rotation_group) {
      auto rotation = m_rotation_groups.get(*light.rotation_group);
      if (!rotation) return rollback(std::move(rotation.error()));
      auto const& rg = rotation->get();
      float angle = sample ? angle_lerp(sample->get().interpolation(), rg.previous_angle, rg.current_angle) : rg.current_angle;
      Vec2F center = light.rotation_center.value_or(rg.rotation_center);
      position = (position - center).rotate(angle) + center;
      point_angle += angle;
    }
    if (!global)
      global.emplace(global_transformation());
    position = global->transform_point(position);
    if (flipped) {
      if (point_angle > 0)
        point_angle = std::numbers::pi / 2 + normalize_angle(std::numbers::pi / 2 - point_angle);
      else
        point_angle = -std::numbers::pi / 2 - normalize_angle(point_angle + std::numbers::pi / 2);
    }

    Color color = light.color.get();
    if (light.flicker)
      color.set_value(std::clamp(color.value() * light.flicker->value(SinWeightOperator<float>()), 0.0f, 1.0f));

    lights.push_back(LightSource{position + translate, color.to_rgb_f(),
                                 light.point_light ? LightType::Point : LightType::Spread,
                                 light.point_beam, point_angle, light.beam_ambience});
  }
  return {};
}

std::expected<std::shared_ptr<AudioInstance>, StarException> NetworkedAnimator::load_sound(String const& path) const {
  auto assets = m_root.get().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto audio = (*assets)->audio(path);
  if (!audio) return std::unexpected(std::move(audio.error()));
  return AudioInstance::create(**audio);
}

std::expected<void, StarException> NetworkedAnimator::update_state_sounds(DynamicTarget& target, String const& name,
    AnimatedPartSet::ActiveStateInformation const& state) const {
  using SoundFile = std::shared_ptr<String const>;
  auto select_file = [this](Json const& sound) -> std::expected<SoundFile, StarException> {
    if (sound.is_type(Json::Type::String))
      return sound.string_ptr();
    if (sound.is_type(Json::Type::Array)) {
      auto pool = sound.array_ptr();
      if (!pool) return std::unexpected(std::move(pool.error()));
      if (!(**pool).empty()) {
        auto selected = m_root.get().random().sample_ref(**pool);
        if (!selected) return std::unexpected(std::move(selected.error()));
        return selected->get().string_ptr();
      }
    }
    return SoundFile{};
  };

  // Selection precedes the identity check, even for an unchanged pool.
  auto persistent = state.properties.find_ref("persistentSound");
  auto persistent_file = persistent ? select_file(persistent->get()) : std::expected<SoundFile, StarException>{};
  if (!persistent_file) return std::unexpected(std::move(persistent_file.error()));
  auto active = target.state_persistent_sounds.find_ref(name);
  if (active && active->get().audio && active->get().audio->finished())
    active->get().audio.reset();
  if (!active || !persistent || active->get().sound != persistent->get() || !active->get().audio) {
    if (*persistent_file && !(*persistent_file)->empty()) {
      auto range = state.properties.value("persistentSoundRangeMultiplier", 1.f).to_float();
      if (!range) return std::unexpected(std::move(range.error()));
      auto stop_time = state.properties.value("persistentSoundStopTime", 0.f).to_float();
      if (!stop_time) return std::unexpected(std::move(stop_time.error()));
      auto next = load_sound(AssetPath::relative_to(m_relative_path, **persistent_file));
      if (!next) return std::unexpected(std::move(next.error()));
      (*next)->set_range_multiplier(*range);
      (*next)->set_loops(-1);
      (*next)->set_position(global_transformation().transform_point(Vec2F{}));

      if (active && active->get().audio)
        active->get().audio->stop(active->get().stop_ramp_time);
      target.pending_audios.push_back(*next);
      target.state_persistent_sounds[name] = {persistent->get(), std::move(*next), *stop_time};
    } else if (active) {
      if (active->get().audio)
        active->get().audio->stop(active->get().stop_ramp_time);
      target.state_persistent_sounds.remove(name);
    }
  }

  auto immediate = state.properties.find_ref("immediateSound");
  if (!immediate) {
    target.state_immediate_sounds.remove(name);
    return {};
  }
  auto immediate_file = select_file(immediate->get());
  if (!immediate_file) return std::unexpected(std::move(immediate_file.error()));
  if (auto last = target.state_immediate_sounds.find_ref(name); !last || last->get() != immediate->get()) {
    if (*immediate_file && !(*immediate_file)->empty()) {
      auto range = state.properties.value("immediateSoundRangeMultiplier", 1.f).to_float();
      if (!range) return std::unexpected(std::move(range.error()));
      auto next = load_sound(AssetPath::relative_to(m_relative_path, **immediate_file));
      if (!next) return std::unexpected(std::move(next.error()));
      (*next)->set_range_multiplier(*range);
      (*next)->set_position(global_transformation().transform_point(Vec2F{}));
      target.pending_audios.push_back(std::move(*next));
    }
    target.state_immediate_sounds[name] = immediate->get();
  }
  return {};
}

std::expected<void, StarException> NetworkedAnimator::update_independent_sounds(DynamicTarget& target) {
  for (auto& [name, entry] : m_sounds) {
    auto& active_sounds = target.independent_sounds;
    std::optional<Vec2F> position;
    auto base_position = [&]() -> Vec2F const& {
      if (!position)
        position = global_transformation().transform_point(Vec2F(entry.x_position.get(), entry.y_position.get()));
      return *position;
    };
    auto update_levels = [&](AudioInstance& audio) {
      audio.set_volume(entry.volume_target.get(), entry.volume_ramp_time.get());
      audio.set_pitch_multiplier(entry.pitch_multiplier_target.get(), entry.pitch_multiplier_ramp_time.get());
    };
    auto deliver = [&](List<SoundSignal> const& signals, std::size_t& next_signal) -> std::expected<void, StarException> {
      for (; next_signal < signals.size(); ++next_signal) {
        if (signals[next_signal] == SoundSignal::StopAll) {
          if (auto active = active_sounds.find_ref(name)) {
            for (auto const& audio : active->get())
              audio->stop(entry.volume_ramp_time.get());
            active_sounds.remove(name);
          }
          continue;
        }
        if (signals[next_signal] != SoundSignal::Play)
          continue;
        auto const& pool = entry.sound_pool.get();
        if (pool.empty())
          continue;
        auto selected = m_root.get().random().sample_ref(pool);
        if (!selected) return std::unexpected(std::move(selected.error()));
        if (!selected->get().empty()) {
          auto audio = load_sound(selected->get());
          if (!audio) return std::unexpected(std::move(audio.error()));
          (*audio)->set_range_multiplier(entry.range_multiplier);
          (*audio)->set_loops(entry.loops.get());
          (*audio)->set_position(base_position());
          update_levels(**audio);
          active_sounds[name].push_back(*audio);
          target.pending_audios.push_back(std::move(*audio));
        }
      }
      return {};
    };

    // receive() acknowledges the whole batch. Retain its uncommitted suffix
    // across a failed asset load and deliver it before reading newer signals.
    if (!entry.retry_signals.empty()) {
      auto delivered = deliver(entry.retry_signals, entry.next_retry_signal);
      if (!delivered) return delivered;
      entry.retry_signals = {};
    }
    auto signals = entry.signals.receive();
    std::size_t next_signal = 0;
    if (auto delivered = deliver(signals, next_signal); !delivered) {
      entry.retry_signals = std::move(signals);
      entry.next_retry_signal = next_signal;
      return delivered;
    }

    if (auto active = active_sounds.find_ref(name)) {
      for (auto const& audio : active->get()) {
        if (auto base = target.current_audio_base_positions.find_ref(audio))
          base->get() = base_position();
        update_levels(*audio);
      }
    }
  }
  return {};
}

std::expected<void, StarException> NetworkedAnimator::update(float dt, std::optional<std::reference_wrapper<DynamicTarget>> dynamic_target) {
  dt *= m_animation_rate.get();

  auto animated = m_animated_parts.update(dt);
  if (!animated) return std::unexpected(std::move(animated.error()));

  auto states_updated = m_animated_parts.for_each_active_state([&](String const& state_type,
      AnimatedPartSet::ActiveStateInformation const& active_state) -> std::expected<void, StarException> {
      if (dynamic_target) {
        if (auto sounds = update_state_sounds(dynamic_target->get(), state_type, active_state); !sounds)
          return sounds;
      }

      if (auto lights_on = active_state.properties.find_ref("lightsOn")) {
        auto entries = lights_on->get().iterate_array();
        if (!entries) return std::unexpected(std::move(entries.error()));
        for (auto const& name_json : *entries) {
          auto name = name_json.to_string();
          if (!name) return std::unexpected(std::move(name.error()));
          auto entry = m_lights.get(*name);
          if (!entry) return std::unexpected(std::move(entry.error()));
          entry->get().active.set(true);
        }
      }
      if (auto lights_off = active_state.properties.find_ref("lightsOff")) {
        auto entries = lights_off->get().iterate_array();
        if (!entries) return std::unexpected(std::move(entries.error()));
        for (auto const& name_json : *entries) {
          auto name = name_json.to_string();
          if (!name) return std::unexpected(std::move(name.error()));
          auto entry = m_lights.get(*name);
          if (!entry) return std::unexpected(std::move(entry.error()));
          entry->get().active.set(false);
        }
      }

      if (auto emitters_on = active_state.properties.find_ref("particleEmittersOn")) {
        auto entries = emitters_on->get().iterate_array();
        if (!entries) return std::unexpected(std::move(entries.error()));
        for (auto const& name_json : *entries) {
          auto name = name_json.to_string();
          if (!name) return std::unexpected(std::move(name.error()));
          auto entry = m_particle_emitters.get(*name);
          if (!entry) return std::unexpected(std::move(entry.error()));
          entry->get().active.set(true);
        }
      }
      if (auto emitters_off = active_state.properties.find_ref("particleEmittersOff")) {
        auto entries = emitters_off->get().iterate_array();
        if (!entries) return std::unexpected(std::move(entries.error()));
        for (auto const& name_json : *entries) {
          auto name = name_json.to_string();
          if (!name) return std::unexpected(std::move(name.error()));
          auto entry = m_particle_emitters.get(*name);
          if (!entry) return std::unexpected(std::move(entry.error()));
          entry->get().active.set(false);
        }
      }

      return {};
    });
  if (!states_updated) return states_updated;
  if (version() > 0) {
    for (auto& [name, group] : m_transformation_groups) {
      group.animation_base_transform = group.animation_affine_transform();
      auto transform = animation_group_transformation(name, group, group.animation_base_transform, std::nullopt);
      if (!transform) return std::unexpected(std::move(transform.error()));
      group.set_animation_affine_transform(*transform);
    }
  }

  for (auto& [name, rotation] : m_rotation_groups) {
    rotation.previous_angle = rotation.current_angle;
    if (rotation.angular_velocity == 0.0f)
      rotation.current_angle = rotation.target_angle.get();
    else
      rotation.current_angle = approach_angle(rotation.target_angle.get(), rotation.current_angle, rotation.angular_velocity * dt);
  }

  if (dynamic_target) {
    auto& target = dynamic_target->get();
    auto& random = m_root.get().random();
    for (auto& [name, emitter] : m_particle_emitters) {
      Mat3F transformation = Mat3F::identity();
      if (emitter.anchor_part) {
        auto anchor = part_transformation(*emitter.anchor_part);
        if (!anchor) return std::unexpected(std::move(anchor.error()));
        transformation = *anchor;
      }
      auto group = group_transformation(emitter.transformation_groups);
      if (!group) return std::unexpected(std::move(group.error()));
      transformation = *group * transformation;

      if (emitter.rotation_group) {
        auto rotation = m_rotation_groups.get(*emitter.rotation_group);
        if (!rotation) return std::unexpected(std::move(rotation.error()));
        auto const& rg = rotation->get();
        Vec2F rotation_center = emitter.rotation_center.value_or(rg.rotation_center);
        transformation = Mat3F::rotation(rg.current_angle, rotation_center) * transformation;
      }

      unsigned emission_cycles = 0;
      if (emitter.active.get()) {
        auto rate = emitter.emission_rate.get();
        emitter.timer = std::min(emitter.timer, 1.0f / (rate + emitter.emission_rate_variance));
        if (emitter.timer <= 0.0f) {
          ++emission_cycles;
          auto variance = random.uniform_float(-emitter.emission_rate_variance, emitter.emission_rate_variance);
          if (!variance) return std::unexpected(std::move(variance.error()));
          emitter.timer = 1.0f / (rate + *variance);
        } else {
          emitter.timer -= dt;
        }
      }

      auto bursts = emitter.burst_event.pull_occurrences();
      auto count = emitter.burst_count.get();
      if (count != 0 && bursts > (std::numeric_limits<unsigned>::max() - emission_cycles) / count)
        return std::unexpected(NetworkedAnimatorException(Star::format("Particle emitter '{}' burst cycle count overflow", name)));
      emission_cycles += static_cast<unsigned>(bursts * count);
      if (emission_cycles == 0 || emitter.particle_list.empty())
        continue;

      transformation = global_transformation() * transformation;
      auto const& offset_region = emitter.offset_region.get();
      bool has_offset = !offset_region.is_null();
      bool flipped = transformation.determinant() < 0;
      auto zoom = m_zoom.get();
      auto emit = [&](ParticleEmitter::ParticleConfig const& config) {
        for (unsigned i = 0; i < config.count; ++i) {
          Particle particle = config.creator();
          particle.position += config.offset;
          if (has_offset) {
            particle.position[0] += random.uniform_float() * offset_region.width() + offset_region.x_min();
            particle.position[1] += random.uniform_float() * offset_region.height() + offset_region.y_min();
          }

          float speed = particle.velocity.magnitude();
          particle.velocity = Vec2F::with_angle(transformation.transform_angle(particle.velocity.angle())) * speed;
          particle.position = transformation.transform_point(particle.position);
          particle.rotation = transformation.transform_angle(particle.rotation);
          particle.size *= zoom;
          if (config.flip != flipped)
            particle.flip = !particle.flip;
          if (flipped)
            particle.rotation += std::numbers::pi;
          target.pending_particles.push_back(std::move(particle));
        }
      };

      auto selected_count = static_cast<unsigned>(emitter.random_select_count.get());
      if (selected_count >= emitter.particle_list.size()) {
        for (unsigned cycle = 0; cycle < emission_cycles; ++cycle) {
          for (auto const& config : emitter.particle_list)
            emit(config);
        }
      } else {
        m_particle_selection.resize(emitter.particle_list.size());
        for (unsigned cycle = 0; cycle < emission_cycles; ++cycle) {
          // A fresh authored order per cycle matches the old shuffled copy,
          // including RNG consumption when selected_count is zero.
          std::iota(m_particle_selection.begin(), m_particle_selection.end(), std::size_t{0});
          random.shuffle(m_particle_selection);
          for (std::size_t i = 0; i < selected_count; ++i)
            emit(emitter.particle_list[m_particle_selection[i]]);
        }
      }
    }

    if (auto sounds = update_independent_sounds(target); !sounds)
      return sounds;
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
  for (auto& [name, group] : m_transformation_groups) {
    auto transform = group.affine_transform();
    group.previous_transform = group.presentation_history ? group.current_transform : transform;
    group.previous_local_transform = group.presentation_history ? group.current_local_transform : group.local_transform;
    group.current_local_transform = group.local_transform;
    group.current_transform = transform;
    group.presentation_history = true;
  }
  return {};
}

std::expected<void, StarException> NetworkedAnimator::finish_animations() {
  return m_animated_parts.finish_animations();
}

void NetworkedAnimator::commit_local_reset(Sample const& staged) {
  auto const& presentation = staged.m_presentation.get();
  m_animated_parts.commit_finished_states(presentation.parts);
  for (auto& [name, group] : m_transformation_groups)
    group.set_local_affine_transform(presentation.groups[group.presentation_index].local_transform);
  for (auto const& [name, tag] : presentation.tags)
    if (tag.active)
      set_local_tag(name, tag.value);
}

Mat3F NetworkedAnimator::TransformationGroup::affine_transform() const {
  return Mat3F(
      x_scale.get() * std::cos(x_shear.get()), x_scale.get() * std::sin(x_shear.get()), x_translation.get(),
      y_scale.get() * std::sin(y_shear.get()), y_scale.get() * std::cos(y_shear.get()), y_translation.get(),
      0, 0, 1
    );
}

void NetworkedAnimator::TransformationGroup::set_affine_transform(Mat3F const& matrix) {
  x_translation.set(matrix[0][2]);
  y_translation.set(matrix[1][2]);
  x_scale.set(std::sqrt(square(matrix[0][0]) + square(matrix[0][1])));
  y_scale.set(std::sqrt(square(matrix[1][0]) + square(matrix[1][1])));
  x_shear.set(std::atan2(matrix[0][1], matrix[0][0]));
  y_shear.set(std::atan2(matrix[1][0], matrix[1][1]));
}

void NetworkedAnimator::TransformationGroup::set_local_affine_transform(Mat3F const& matrix) {
  local_transform = matrix;
}

Mat3F NetworkedAnimator::TransformationGroup::local_affine_transform() const {
  return local_transform;
}

void NetworkedAnimator::TransformationGroup::set_animation_affine_transform(Mat3F const& matrix) {
  x_translation_animation = matrix[0][2];
  y_translation_animation = matrix[1][2];
  x_scale_animation = std::sqrt(square(matrix[0][0]) + square(matrix[0][1]));
  y_scale_animation = std::sqrt(square(matrix[1][0]) + square(matrix[1][1]));
  x_shear_animation = std::atan2(matrix[0][1], matrix[0][0]);
  y_shear_animation = std::atan2(matrix[1][0], matrix[1][1]);
}

Mat3F NetworkedAnimator::TransformationGroup::animation_affine_transform() const {
  return Mat3F(
      x_scale_animation * std::cos(x_shear_animation), x_scale_animation * std::sin(x_shear_animation), x_translation_animation,
      y_scale_animation * std::sin(y_shear_animation), y_scale_animation * std::cos(y_shear_animation), y_translation_animation,
      0, 0, 1
    );
}

std::expected<void, StarException> NetworkedAnimator::setup_net_states() {
  clear_net_elements();

  if (auto registered = add_net_element(m_processing_directives); !registered) return std::unexpected(std::move(registered.error()));
  if (auto registered = add_net_element(m_zoom); !registered) return std::unexpected(std::move(registered.error()));
  if (auto registered = add_net_element(m_flipped); !registered) return std::unexpected(std::move(registered.error()));
  if (auto registered = add_net_element(m_flipped_relative_center_line); !registered) return std::unexpected(std::move(registered.error()));

  if (auto registered = add_net_element(m_animation_rate); !registered) return std::unexpected(std::move(registered.error()));
  m_animation_rate.set_interpolator(lerp<float, float>);

  if (auto registered = add_net_element(m_global_tags); !registered) return std::unexpected(std::move(registered.error()));

  auto part_names = m_animated_parts.part_names();
  part_names.sort();
  for (auto const& part : part_names)
    if (auto registered = add_net_element(m_part_tags[part]); !registered) return std::unexpected(std::move(registered.error()));

  for (auto& pair : m_state_info) {
    pair.second.reverse.set_compatibility_version(10);
    if (auto registered = add_net_element(pair.second.reverse); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.state_index); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.started_event); !registered) return std::unexpected(std::move(registered.error()));
  }

  for (auto& pair : m_transformation_groups) {
    if (auto registered = add_net_element(pair.second.x_translation); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.y_translation); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.x_scale); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.y_scale); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.x_shear); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.y_shear); !registered) return std::unexpected(std::move(registered.error()));

    if (pair.second.interpolated) {
      pair.second.x_translation.set_interpolator(lerp<float, float>);
      pair.second.y_translation.set_interpolator(lerp<float, float>);
      pair.second.x_scale.set_interpolator(lerp<float, float>);
      pair.second.y_scale.set_interpolator(lerp<float, float>);
      pair.second.x_shear.set_interpolator(angle_lerp<float, float>);
      pair.second.y_shear.set_interpolator(angle_lerp<float, float>);
    }
  }

  for (auto& pair : m_rotation_groups) {
    if (auto registered = add_net_element(pair.second.target_angle); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.net_immediate_event); !registered) return std::unexpected(std::move(registered.error()));
  }

  for (auto& pair : m_particle_emitters) {
    if (auto registered = add_net_element(pair.second.emission_rate); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.burst_count); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.random_select_count); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.offset_region); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.active); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.burst_event); !registered) return std::unexpected(std::move(registered.error()));

    pair.second.burst_event.set_ignore_occurrences_on_net_load(true);
  }

  for (auto& pair : m_lights) {
    if (auto registered = add_net_element(pair.second.active); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.x_position); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.y_position); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.color); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.point_angle); !registered) return std::unexpected(std::move(registered.error()));

    pair.second.x_position.set_fixed_point_base(0.0125f);
    pair.second.y_position.set_fixed_point_base(0.0125f);
    pair.second.point_angle.set_fixed_point_base(0.01f);

    pair.second.x_position.set_interpolator(lerp<float, float>);
    pair.second.y_position.set_interpolator(lerp<float, float>);
    pair.second.point_angle.set_interpolator(angle_lerp<float, float>);
  }

  for (auto& pair : m_sounds) {
    if (auto registered = add_net_element(pair.second.sound_pool); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.x_position); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.y_position); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.volume_target); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.volume_ramp_time); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.pitch_multiplier_target); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.pitch_multiplier_ramp_time); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.loops); !registered) return std::unexpected(std::move(registered.error()));
    if (auto registered = add_net_element(pair.second.signals); !registered) return std::unexpected(std::move(registered.error()));
    pair.second.retry_signals = {};
    pair.second.next_retry_signal = 0;

    pair.second.x_position.set_fixed_point_base(0.0125f);
    pair.second.y_position.set_fixed_point_base(0.0125f);

    pair.second.x_position.set_interpolator(lerp<float, float>);
    pair.second.y_position.set_interpolator(lerp<float, float>);
  }

  for (auto& pair : m_effects)
    if (auto registered = add_net_element(pair.second.enabled); !registered) return std::unexpected(std::move(registered.error()));

  return {};
}

std::expected<void, StarException> NetworkedAnimator::net_elements_need_load(bool initial) {
  for (auto& pair : m_state_info) {
    if (pair.second.started_event.pull_occurred() || initial) {
      auto changed = m_animated_parts.set_active_state_index(pair.first, pair.second.state_index.get(), true, pair.second.reverse.get());
      if (!changed) return std::unexpected(std::move(changed.error()));
    }
  }

  for (auto& pair : m_rotation_groups) {
    if (pair.second.net_immediate_event.pull_occurred() || initial) {
      pair.second.current_angle = pair.second.target_angle.get();
      pair.second.previous_angle = pair.second.current_angle;
    }
  }
  return {};
}

std::expected<void, StarException> NetworkedAnimator::net_elements_need_store() {
  for (auto& pair : m_state_info) {
    if (pair.second.was_updated || (version() < 1)) {
      pair.second.was_updated = false;
      auto index = m_animated_parts.active_state_index(pair.first);
      if (!index) return std::unexpected(std::move(index.error()));
      pair.second.state_index.set(*index);
      auto reverse = m_animated_parts.active_state_reverse(pair.first);
      if (!reverse) return std::unexpected(std::move(reverse.error()));
      pair.second.reverse.set(*reverse);
    }
  }
  return {};
}

std::uint8_t NetworkedAnimator::version() const {
  return m_animator_version;
}

std::expected<Json, StarException> NetworkedAnimator::merge_includes(Root& root, Json const& config, Json const& includes,
    String const& relative_path, StringList& include_chain) {
  Json included_configs;
  auto paths = includes.iterate_array();
  if (!paths) return std::unexpected(std::move(paths.error()));
  for (auto const& path : *paths) {
    auto name = path.to_string();
    if (!name) return std::unexpected(std::move(name.error()));
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto resolved = AssetPath::relative_to(relative_path, *name);
    if (std::ranges::find(include_chain, resolved) != include_chain.end())
      return std::unexpected(NetworkedAnimatorException::format("Animation include cycle: {} -> {}", include_chain.join(" -> "), resolved));
    auto loaded = (*assets)->json(resolved);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    auto nested = loaded->find("includes");
    if (!nested) return std::unexpected(std::move(nested.error()));
    if (*nested) {
      include_chain.push_back(std::move(resolved));
      auto merged = merge_includes(root, *loaded, (*nested)->get(), relative_path, include_chain);
      include_chain.pop_back();
      if (!merged) return std::unexpected(std::move(merged.error()));
      *loaded = std::move(*merged);
    }
    included_configs = json_merge(included_configs, *loaded);
  }
  return json_merge(included_configs, config);
}

}
