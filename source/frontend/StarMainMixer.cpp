import std;
import star.audio;
import star.assets;
import star.configuration;
import star.format;
import star.json;
import star.json_extra;
import star.list;
import star.main_mixer;
import star.mixer;
import star.player;
import star.root;
import star.string;
import star.universe_client;
import star.vector;
import star.voice;
import star.world_camera;
import star.world_client;
import star.world_geometry;
import star.world_painter;

namespace Star {

MainMixer::MainMixer(Root& root, unsigned sample_rate, unsigned channels) : m_root(root) {
  m_mixer = std::make_shared<Mixer>(root.logger(), sample_rate, channels);
}

void MainMixer::set_universe_client(std::shared_ptr<UniverseClient> universe_client) {
  m_universe_client = std::move(universe_client);
}

void MainMixer::set_world_painter(std::optional<std::reference_wrapper<WorldPainter>> world_painter) {
  m_world_painter = world_painter;
}

void MainMixer::set_voice(std::optional<std::reference_wrapper<Voice>> voice) { m_voice = voice; }

std::expected<void, StarException> MainMixer::update(float dt, bool mute_sfx, bool mute_music) {
  auto assets = m_root.get().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto configuration = m_root.get().configuration();
  if (!configuration) return std::unexpected(std::move(configuration.error()));

  std::array<std::optional<float>, 3> current_settings{};
  auto read_volume_setting = [&](std::size_t setting_index,
                                 String const& setting_name) -> std::expected<float, StarException> {
    if (current_settings[setting_index])
      return *current_settings[setting_index];
    auto setting = (*configuration)->get(setting_name);
    if (!setting)
      return std::unexpected(std::move(setting.error()));
    auto raw_volume = setting->to_float();
    if (!raw_volume)
      return std::unexpected(std::move(raw_volume.error()));

    auto& cached_setting = m_volume_settings[setting_index];
    if (!cached_setting.raw || *cached_setting.raw != *raw_volume) {
      cached_setting.amplitude = perceptual_to_amplitude(*raw_volume / 100.0f);
      cached_setting.raw = *raw_volume;
    }
    current_settings[setting_index] = cached_setting.amplitude;
    return cached_setting.amplitude;
  };

  auto update_group_volume = [&](MixerGroup group, bool muted, std::size_t setting_index,
                                 String const& setting_name) -> std::expected<void, StarException> {
    auto group_index = static_cast<std::size_t>(group);
    if (muted) {
      if (!m_muted_groups[group_index])
        m_mixer->set_group_volume(group, 0.0f, 1.0f);
      m_muted_groups[group_index] = true;
      return {};
    }
    auto volume = read_volume_setting(setting_index, setting_name);
    if (!volume)
      return std::unexpected(std::move(volume.error()));
    auto& group_volume = m_group_volumes[group_index];
    if (m_muted_groups[group_index]) {
      m_mixer->set_group_volume(group, *volume, 1.0f);
      m_muted_groups[group_index] = false;
    } else if (!group_volume || *group_volume != *volume) {
      m_mixer->set_group_volume(group, *volume);
    }
    group_volume = *volume;
    return {};
  };

  if (auto result = update_group_volume(MixerGroup::Effects, mute_sfx, 0, "sfxVol"); !result)
    return result;
  if (auto result = update_group_volume(MixerGroup::Music, mute_music, 1, "musicVol"); !result)
    return result;
  if (auto result = update_group_volume(MixerGroup::Cinematic, false, 0, "sfxVol"); !result)
    return result;
  if (auto result = update_group_volume(MixerGroup::Instruments, mute_sfx, 2, "instrumentVol"); !result)
    return result;

  std::shared_ptr<WorldClient> current_world;
  if (m_universe_client)
    current_world = m_universe_client->worldClient();

  if (current_world) {
    for (auto const& audio_instance : current_world->pullPendingAudio()) {
      auto played = m_mixer->play(audio_instance);
      if (!played)
        return std::unexpected(std::move(played.error()));
    }

    for (auto const& audio_instance : current_world->pullPendingMusic()) {
      audio_instance->set_mixer_group(MixerGroup::Music);
      auto played = m_mixer->play(audio_instance);
      if (!played)
        return std::unexpected(std::move(played.error()));
    }

    if (m_universe_client && m_universe_client->mainPlayer()->underwater()) {
      if (!m_mixer->has_effect("lowpass")) {
        auto effect = m_mixer->add_effect("lowpass", m_mixer->low_pass(32), 0.50f);
        if (!effect)
          return std::unexpected(std::move(effect.error()));
      }
      if (!m_mixer->has_effect("echo")) {
        auto effect = m_mixer->add_effect("echo", m_mixer->echo(0.2f, 0.6f, 0.4f), 0.50f);
        if (!effect)
          return std::unexpected(std::move(effect.error()));
      }
    } else {
      if (m_mixer->has_effect("lowpass"))
        m_mixer->remove_effect("lowpass", 0.5f);
      if (m_mixer->has_effect("echo"))
        m_mixer->remove_effect("echo", 0.5f);
    }

    auto sfx_config = (*assets)->json("/sfx.config");
    if (!sfx_config)
      return std::unexpected(std::move(sfx_config.error()));
    auto distance = sfx_config->get_float("baseMaxDistance");
    if (!distance) return std::unexpected(std::move(distance.error()));
    float base_max_distance = *distance;
    auto stereo_json = sfx_config->get("stereoAdjustmentRange");
    if (!stereo_json)
      return std::unexpected(std::move(stereo_json.error()));
    auto stereo = json_to_vec2f(*stereo_json);
    if (!stereo) return std::unexpected(std::move(stereo.error()));
    Vec2F stereo_adjustment_range = *stereo;
    auto gamma = sfx_config->get_float("attenuationGamma");
    if (!gamma) return std::unexpected(std::move(gamma.error()));
    float inverse_attenuation_gamma = 1.0f / *gamma;
    auto player_position = m_universe_client->mainPlayer()->position();
    auto camera_position = m_world_painter->get().camera().centerWorldPosition();
    auto world_geometry = current_world->geometry();

    Mixer::PositionalAttenuationFunction attenuation_function =
      [&](Vec2F position, float range_multiplier, std::span<float> attenuation) -> std::expected<void, StarException> {
      if (attenuation.empty())
        return {};

      Vec2F player_diff = world_geometry.diff(position, player_position);
      Vec2F camera_diff = world_geometry.diff(position, camera_position);
      float player_magnitude_squared = player_diff.magnitude_squared();
      float camera_magnitude_squared = camera_diff.magnitude_squared();

      Vec2F diff;
      float diff_magnitude;
      if (player_magnitude_squared < camera_magnitude_squared) {
        diff = player_diff;
        diff_magnitude = std::sqrt(player_magnitude_squared);
      } else {
        diff = camera_diff;
        diff_magnitude = std::sqrt(camera_magnitude_squared);
      }

      if (diff_magnitude == 0.0f) {
        std::fill(attenuation.begin(), attenuation.end(), 0.0f);
        return {};
      }

      Vec2F diff_normal = diff / diff_magnitude;
      float source_max_distance = base_max_distance * range_multiplier;
      auto channel_attenuation = [&](float stereo_incidence) {
        float max_distance = source_max_distance
          * std::lerp(stereo_adjustment_range[0], stereo_adjustment_range[1], (stereo_incidence + 1.0f) / 2.0f);
        return std::pow(std::clamp(diff_magnitude / max_distance, 0.0f, 1.0f), inverse_attenuation_gamma);
      };

      attenuation.front() = channel_attenuation(-diff_normal[0]);
      if (attenuation.size() > 1)
        std::fill(attenuation.begin() + 1, attenuation.end(), channel_attenuation(diff_normal[0]));
      return {};
    };

    if (m_voice) {
      auto voice = m_voice->get().update(dt, attenuation_function);
      if (!voice)
        return std::unexpected(std::move(voice.error()));
    }

    auto mixer = m_mixer->update(dt, attenuation_function);
    if (!mixer)
      return std::unexpected(std::move(mixer.error()));

  } else {
    if (m_mixer->has_effect("lowpass"))
      m_mixer->remove_effect("lowpass", 0);
    if (m_mixer->has_effect("echo"))
      m_mixer->remove_effect("echo", 0);

    if (m_voice) {
      auto voice = m_voice->get().update(dt);
      if (!voice)
        return std::unexpected(std::move(voice.error()));
    }

    auto mixer = m_mixer->update(dt);
    if (!mixer)
      return std::unexpected(std::move(mixer.error()));
  }
  return {};
}

std::shared_ptr<Mixer> MainMixer::mixer() const {
  return m_mixer;
}

void MainMixer::set_speed(float speed) { m_mixer->set_speed(std::max(speed, 0.0f)); }

void MainMixer::set_volume(float volume, float ramp_time) { m_mixer->set_volume(volume, ramp_time); }

std::expected<void, StarException> MainMixer::read(std::span<std::int16_t> sample_data, std::size_t frame_count,
                                                   Mixer::ExtraMixFunction extra_mix_function) {
  return m_mixer->read(sample_data, frame_count, std::move(extra_mix_function));
}
}