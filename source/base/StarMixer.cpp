import std;
import star.audio;
import star.string;
import star.format;
import star.list;
import star.map;
import star.vector;
import star.bi_map;
import star.mixer;
import star.interpolation;
import star.time;
import star.logging;

import star.math_common;
import star.algorithm;
namespace Star {

namespace {
float rate_of_change_from_ramp_time(float ramp_time) {
  static constexpr float max_rate = 10000.0f;
  return ramp_time < 1.0f / max_rate ? max_rate : 1.0f / ramp_time;
}

bool valid_mixer_group(MixerGroup group) { return std::to_underlying(group) < 4; }

bool valid_buffer_shape(std::span<std::int16_t> buffer, std::size_t frames, unsigned channels) {
  return channels != 0 && frames <= buffer.size() / channels;
}
}

std::expected<std::shared_ptr<AudioInstance>, StarException> AudioInstance::create(Audio const& audio) {
  auto copy = audio.clone();
  if (!copy)
    return std::unexpected(std::move(copy.error()));
  return std::make_shared<AudioInstance>(ConstructionKey{}, std::move(*copy));
}

AudioInstance::AudioInstance(ConstructionKey, Audio audio)
    : m_audio(std::move(audio)), m_mixer_group(MixerGroup::Effects),
      m_volume{.value = 1.0f, .target = 1.0f, .velocity = 0.0f},
      m_pitch_multiplier{.value = 1.0f, .target = 1.0f, .velocity = 0.0f}, m_loops(0), m_stopping(false),
      m_finished(false), m_range_multiplier(1.0f), m_clock_stop_fade_out(0) {}

std::optional<Vec2F> AudioInstance::position() const {
  std::lock_guard locker(m_mutex);
  return m_position;
}

void AudioInstance::set_position(std::optional<Vec2F> position) {
  std::lock_guard locker(m_mutex);
  m_position = position;
}

void AudioInstance::translate(Vec2F const& distance) {
  std::lock_guard locker(m_mutex);
  if (m_position)
    *m_position += distance;
  else
    m_position = distance;
}

float AudioInstance::range_multiplier() const {
  std::lock_guard locker(m_mutex);
  return m_range_multiplier;
}

void AudioInstance::set_range_multiplier(float range_multiplier) {
  std::lock_guard locker(m_mutex);
  m_range_multiplier = range_multiplier;
}

void AudioInstance::set_volume(float target_value, float ramp_time) {
  if constexpr (Star::debug_enabled)
    Star::star_assert_failed((target_value >= 0));
  std::lock_guard locker(m_mutex);
  if (m_stopping)
    return;
  if (ramp_time <= 0.0f)
    m_volume = {.value = target_value, .target = target_value, .velocity = 0.0f};
  else {
    m_volume.target = target_value;
    m_volume.velocity = rate_of_change_from_ramp_time(ramp_time);
  }
}

void AudioInstance::set_pitch_multiplier(float target_value, float ramp_time) {
  if constexpr (Star::debug_enabled)
    Star::star_assert_failed((target_value >= 0));
  std::lock_guard locker(m_mutex);
  if (m_stopping)
    return;
  if (ramp_time <= 0.0f)
    m_pitch_multiplier = {.value = target_value, .target = target_value, .velocity = 0.0f};
  else {
    m_pitch_multiplier.target = target_value;
    m_pitch_multiplier.velocity = rate_of_change_from_ramp_time(ramp_time);
  }
}

int AudioInstance::loops() const {
  std::lock_guard locker(m_mutex);
  return m_loops;
}

void AudioInstance::set_loops(int loops) {
  std::lock_guard locker(m_mutex);
  m_loops = loops;
}

std::expected<double, StarException> AudioInstance::current_time() const {
  std::lock_guard locker(m_mutex);
  return m_audio.current_time();
}

std::expected<double, StarException> AudioInstance::total_time() const {
  std::lock_guard locker(m_mutex);
  return m_audio.total_time();
}

std::expected<void, StarException> AudioInstance::seek_time(double time) {
  std::lock_guard locker(m_mutex);
  return m_audio.seek_time(time);
}

MixerGroup AudioInstance::mixer_group() const {
  std::lock_guard locker(m_mutex);
  return m_mixer_group;
}

void AudioInstance::set_mixer_group(MixerGroup mixer_group) {
  if (!valid_mixer_group(mixer_group))
    return;
  std::lock_guard locker(m_mutex);
  m_mixer_group = mixer_group;
}

void AudioInstance::set_clock_start(std::optional<std::int64_t> clock_start_time) {
  std::lock_guard locker(m_mutex);
  m_clock_start = clock_start_time;
}

void AudioInstance::set_clock_stop(std::optional<std::int64_t> clock_stop_time, std::int64_t fade_out_time) {
  std::lock_guard locker(m_mutex);
  m_clock_stop = clock_stop_time;
  m_clock_stop_fade_out = fade_out_time;
}

void AudioInstance::stop(float ramp_time) {
  std::lock_guard locker(m_mutex);
  if (ramp_time <= 0.0f) {
    m_volume = {.value = 0.0f, .target = 0.0f, .velocity = 0.0f};
    m_pitch_multiplier.target = 0.0f;
    m_pitch_multiplier.velocity = 0.0f;
  } else {
    m_volume.target = 0.0f;
    m_volume.velocity = rate_of_change_from_ramp_time(ramp_time);
  }
  m_stopping = true;
}

bool AudioInstance::finished() const {
  std::lock_guard locker(m_mutex);
  return m_finished;
}

Mixer::Mixer(Logger& logger, unsigned sample_rate, unsigned channels)
    : m_logger(logger), m_sample_rate(sample_rate), m_channels(channels),
      m_volume{.value = 1.0f, .target = 1.0f, .velocity = 0.0f}, m_speed(1.0f) {
  m_group_volumes.fill({.value = 1.0f, .target = 1.0f, .velocity = 0.0f});
}

unsigned Mixer::sample_rate() const { return m_sample_rate; }

unsigned Mixer::channels() const {
  return m_channels;
}

std::expected<void, StarException> Mixer::add_effect(String const& effect_name, EffectFunction effect_function,
                                                     float ramp_time) {
  if (!effect_function)
    return std::unexpected(AudioException("Cannot add an empty audio effect"));
  auto effect_info = std::make_shared<EffectInfo>(EffectInfo{.effect_function = std::move(effect_function),
                                                             .amount = 0.0f,
                                                             .velocity = rate_of_change_from_ramp_time(ramp_time),
                                                             .finished = false});
  std::lock_guard locker(m_effects_mutex);
  m_effects[effect_name] = std::move(effect_info);
  return {};
}

void Mixer::remove_effect(String const& effect_name, float ramp_time) {
  std::lock_guard locker(m_effects_mutex);
  if (auto effect = m_effects.find(effect_name); effect != m_effects.end())
    effect->second->velocity = -rate_of_change_from_ramp_time(ramp_time);
}

StringList Mixer::current_effects() {
  std::lock_guard locker(m_effects_mutex);
  return m_effects.keys();
}

bool Mixer::has_effect(String const& effect_name) {
  std::lock_guard locker(m_effects_mutex);
  return m_effects.contains(effect_name);
}

void Mixer::set_speed(float speed) { m_speed = speed; }

void Mixer::set_volume(float volume, float ramp_time) {
  std::lock_guard locker(m_mutex);
  m_volume.target = volume;
  m_volume.velocity = rate_of_change_from_ramp_time(ramp_time);
}

std::expected<void, StarException> Mixer::play(std::shared_ptr<AudioInstance> sample) {
  if (!sample)
    return std::unexpected(AudioException("Cannot play an empty audio instance"));
  auto state = std::make_shared<AudioState>(AudioState{sample, std::vector<float>(m_channels, 1.0f)});
  std::lock_guard locker(m_queue_mutex);
  auto added = m_audios.add(std::move(sample), std::move(state));
  if (!added)
    return std::unexpected(std::move(added.error()));
  return {};
}

std::expected<void, StarException> Mixer::read(std::span<std::int16_t> out_buffer, std::size_t frame_count,
                                               ExtraMixFunction extra_mix_function) {
  if (m_sample_rate == 0 || !valid_buffer_shape(out_buffer, frame_count, m_channels))
    return std::unexpected(AudioException("Invalid mixer output buffer shape or sample rate"));
  auto clear_snapshots = make_scope_exit([&] {
    m_read_audios.clear();
    m_read_effects.clear();
  });

  auto const buffer_size = frame_count * m_channels;
  out_buffer = out_buffer.first(buffer_size);
  m_mix_buffer.resize(buffer_size);
  float const time = static_cast<float>(frame_count) / m_sample_rate;
  float const speed = m_speed;
  float begin_volume;
  float end_volume;
  std::array<RampedValue, 4> group_volumes;
  std::array<float, 4> group_end_volumes;
  {
    std::lock_guard locker(m_mutex);
    begin_volume = m_volume.value;
    end_volume = approach(m_volume.target, begin_volume, m_volume.velocity * time);
    m_volume.value = end_volume;
    group_volumes = m_group_volumes;
    for (std::size_t group = 0; group < group_volumes.size(); ++group) {
      auto const& volume = group_volumes[group];
      group_end_volumes[group] = approach(volume.target, volume.value, volume.velocity * time);
      m_group_volumes[group].value = group_end_volumes[group];
    }
  }

  auto const sample_start_time = Time::milliseconds_since_epoch();
  // Wall-clock scheduling and all ramps continue while global speed is zero.
  long double const sample_end_time =
    sample_start_time + std::floor(static_cast<long double>(frame_count) * 1000 / m_sample_rate);
  std::ranges::fill(out_buffer, 0);
  m_read_audios.clear();
  {
    std::lock_guard locker(m_queue_mutex);
    m_read_audios.reserve(m_audios.size());
    for (auto const& entry : m_audios)
      m_read_audios.push_back(entry.second);
  }

  // Preserve the map's voice order and saturate after every voice, not once at the end.
  for (auto const& audio_state : m_read_audios) {
    auto const& audio_instance = audio_state->audio;
    std::lock_guard audio_locker(audio_instance->m_mutex);
    if (audio_instance->m_finished)
      continue;
    if (audio_instance->m_clock_start && *audio_instance->m_clock_start > sample_end_time)
      continue;

    auto const group = std::to_underlying(audio_instance->m_mixer_group);
    float const group_volume = group_volumes[group].value;
    float const group_end_volume = group_end_volumes[group];
    float const audio_volume_begin = audio_instance->m_volume.value;
    float const audio_volume_end =
      approach(audio_instance->m_volume.target, audio_volume_begin, audio_instance->m_volume.velocity * time);
    audio_instance->m_volume.value = audio_volume_end;

    // Persist the instance's unscaled pitch before applying the independent global speed.
    auto& pitch = audio_instance->m_pitch_multiplier;
    pitch.value = approach(pitch.target, pitch.value, pitch.velocity * time);
    float pitch_multiplier = pitch.value;
    if (audio_instance->m_mixer_group == MixerGroup::Effects
        || audio_instance->m_mixer_group == MixerGroup::Instruments)
      pitch_multiplier *= speed;

    bool finished = audio_volume_end == 0.0f && audio_instance->m_stopping;
    bool const clock_finished = audio_instance->m_clock_stop && *audio_instance->m_clock_stop < sample_end_time
      && sample_end_time
        > static_cast<long double>(*audio_instance->m_clock_stop) + audio_instance->m_clock_stop_fade_out;

    // A zero rate is a pause, not EOF or an Audio::resample error. Fades and scheduled
    // stops still retire a paused voice; otherwise its decoder and loops are untouched.
    if (pitch_multiplier == 0.0f) {
      audio_instance->m_finished = finished || clock_finished;
      continue;
    }

    std::size_t read_amount = 0;
    if (audio_instance->m_clock_start && *audio_instance->m_clock_start > sample_start_time) {
      auto const silent_frames = static_cast<std::size_t>(std::min(
        static_cast<long double>(frame_count),
        (static_cast<long double>(*audio_instance->m_clock_start) - sample_start_time) * m_sample_rate / 1000));
      read_amount = silent_frames * m_channels;
      std::fill_n(m_mix_buffer.begin(), read_amount, 0);
    }
    auto mixed = [&]() -> std::expected<void, StarException> {
      if (read_amount < buffer_size) {
        auto resampled = audio_instance->m_audio.resample(
          m_channels, m_sample_rate, std::span(m_mix_buffer).subspan(read_amount), pitch_multiplier);
        if (!resampled)
          return std::unexpected(std::move(resampled.error()));
        read_amount += *resampled;
        while (read_amount < buffer_size && !finished) {
          if (audio_instance->m_loops == 0) {
            finished = true;
            break;
          }
          auto sought = audio_instance->m_audio.seek_sample(0);
          if (!sought)
            return std::unexpected(std::move(sought.error()));
          auto loop_samples = audio_instance->m_audio.resample(
            m_channels, m_sample_rate, std::span(m_mix_buffer).subspan(read_amount), pitch_multiplier);
          if (!loop_samples)
            return std::unexpected(std::move(loop_samples.error()));
          if (audio_instance->m_loops > 0)
            --audio_instance->m_loops;
          if (*loop_samples == 0) {
            // An empty clip cannot make progress, even when looping forever.
            finished = true;
            break;
          }
          read_amount += *loop_samples;
        }
      }

      if (audio_instance->m_clock_stop && *audio_instance->m_clock_stop < sample_end_time) {
        for (std::size_t frame = 0; frame < read_amount / m_channels; ++frame) {
          long double const sample_time =
            sample_start_time + std::floor(static_cast<long double>(frame) * 1000 / m_sample_rate);
          if (sample_time > *audio_instance->m_clock_stop) {
            float volume = 0.0f;
            if (audio_instance->m_clock_stop_fade_out > 0)
              volume = 1.0f
                - static_cast<float>(sample_time - *audio_instance->m_clock_stop)
                  / static_cast<float>(audio_instance->m_clock_stop_fade_out);
            for (std::size_t channel = 0; channel < m_channels; ++channel) {
              auto& sample = m_mix_buffer[frame * m_channels + channel];
              sample = volume <= 0.0f ? 0 : sample * volume;
            }
          }
        }
      }

      for (std::size_t frame = 0; frame < read_amount / m_channels; ++frame) {
        float const volume =
          std::lerp(begin_volume * group_volume * audio_volume_begin, end_volume * group_end_volume * audio_volume_end,
                    static_cast<float>(frame) / frame_count);
        for (std::size_t channel = 0; channel < m_channels; ++channel) {
          float const sample =
            m_mix_buffer[frame * m_channels + channel] * volume * audio_state->positional_channel_volumes[channel];
          auto& out_sample = out_buffer[frame * m_channels + channel];
          out_sample = std::clamp(sample + out_sample, -32767.0f, 32767.0f);
        }
      }
      return {};
    }();
    if (!mixed) {
      if (!mixed.error().is<AudioException>())
        return std::unexpected(std::move(mixed.error()));
      finished = true;
      audio_instance->m_finished = true;
      auto logged =
        m_logger.error("Error reading audio '{}': {}", audio_instance->m_audio.name(), mixed.error().what());
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
    audio_instance->m_finished = finished || clock_finished;
  }
  m_read_audios.clear();

  if (extra_mix_function) {
    auto mixed = extra_mix_function(out_buffer, frame_count, m_channels);
    if (!mixed)
      return std::unexpected(std::move(mixed.error()));
  }

  m_read_effects.clear();
  {
    std::lock_guard locker(m_effects_mutex);
    m_read_effects.reserve(m_effects.size());
    for (auto const& entry : m_effects)
      m_read_effects.push_back(entry.second);
  }
  for (auto const& effect_info : m_read_effects) {
    float effect_begin;
    float effect_end;
    {
      std::lock_guard locker(m_effects_mutex);
      if (effect_info->finished)
        continue;
      effect_begin = effect_info->amount;
      effect_end = effect_info->velocity < 0 ? approach(0.0f, effect_begin, -effect_info->velocity * time)
                                             : approach(1.0f, effect_begin, effect_info->velocity * time);
    }

    // User effects run without registry locks and only publish scratch on success.
    std::ranges::copy(out_buffer, m_mix_buffer.begin());
    auto effected = effect_info->effect_function(std::span(m_mix_buffer), frame_count, m_channels);
    if (!effected)
      return std::unexpected(std::move(effected.error()));
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
      float const amount = std::lerp(effect_begin, effect_end, static_cast<float>(frame) / frame_count);
      for (std::size_t channel = 0; channel < m_channels; ++channel) {
        auto& sample = out_buffer[frame * m_channels + channel];
        sample = lerp(amount, sample, m_mix_buffer[frame * m_channels + channel]);
      }
    }
    {
      std::lock_guard locker(m_effects_mutex);
      effect_info->amount = effect_end;
      effect_info->finished = effect_end <= 0.0f;
    }
  }
  return {};
}

Mixer::EffectFunction Mixer::low_pass(std::size_t average_size) const {
  struct FilterChannel {
    std::vector<std::int16_t> ring;
    std::size_t cursor = 0;
    std::int64_t sum = 0;
  };
  struct LowPass {
    std::size_t average_size;
    std::vector<FilterChannel> filter;

    [[nodiscard]] std::expected<void, StarException> operator()(std::span<std::int16_t> buffer, std::size_t frames, unsigned channels) {
      if (!valid_buffer_shape(buffer, frames, channels) || average_size == 0
          || average_size > std::numeric_limits<std::int64_t>::max() / 32768
          || average_size > std::vector<std::int16_t>().max_size() || channels > filter.max_size())
        return std::unexpected(AudioException("Invalid low-pass window or buffer shape"));
      if (frames == 0)
        return {};
      filter.resize(channels);
      for (auto& channel : filter) {
        if (channel.ring.empty())
          channel.ring.resize(average_size, 0);
      }
      for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
          auto& history = filter[channel];
          auto& sample = buffer[frame * channels + channel];
          history.sum -= history.ring[history.cursor];
          history.sum += sample;
          history.ring[history.cursor] = sample;
          if (++history.cursor == average_size)
            history.cursor = 0;
          // Mathematical box average, with truncation toward zero. Integer state
          // cannot drift; the old normalized-float fold may differ by one int16 LSB.
          sample = history.sum / static_cast<std::int64_t>(average_size);
        }
      }
      return {};
    }
  };
  return LowPass{.average_size = average_size, .filter = {}};
}

Mixer::EffectFunction Mixer::echo(float time, float dry, float wet) const {
  struct FilterChannel {
    std::vector<std::int16_t> ring;
    std::size_t cursor = 0;
  };
  struct Echo {
    unsigned echo_length;
    float dry;
    float wet;
    bool valid_parameters;
    std::vector<FilterChannel> filter;

    [[nodiscard]] std::expected<void, StarException> operator()(std::span<std::int16_t> buffer, std::size_t frames,
                                                                unsigned channels) {
      if (!valid_parameters || !valid_buffer_shape(buffer, frames, channels)
          || echo_length > std::vector<std::int16_t>().max_size() || channels > filter.max_size())
        return std::unexpected(AudioException("Invalid echo parameters or buffer shape"));
      if (echo_length == 0 || frames == 0)
        return {};
      filter.resize(channels);
      for (auto& channel : filter) {
        if (channel.ring.empty())
          channel.ring.resize(echo_length, 0);
      }
      for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
          auto& history = filter[channel];
          auto& sample = buffer[frame * channels + channel];
          sample = sample * dry + history.ring[history.cursor] * wet;
          // Feedback is the quantized processed output, not the dry input.
          history.ring[history.cursor] = sample;
          if (++history.cursor == echo_length)
            history.cursor = 0;
        }
      }
      return {};
    }
  };

  // Preserve float multiplication and truncation of the original unsigned delay.
  float const delay_frames = time * m_sample_rate;
  bool const valid_parameters = is_finite(time) && is_finite(dry) && is_finite(wet) && is_finite(delay_frames)
    && time >= 0.0f
    && static_cast<double>(delay_frames) < static_cast<double>(std::numeric_limits<unsigned>::max()) + 1.0;
  return Echo{.echo_length = valid_parameters ? static_cast<unsigned>(delay_frames) : 0,
              .dry = dry,
              .wet = wet,
              .valid_parameters = valid_parameters,
              .filter = {}};
}

void Mixer::set_group_volume(MixerGroup group, float target_value, float ramp_time) {
  if (!valid_mixer_group(group))
    return;
  std::lock_guard locker(m_mutex);
  auto& volume = m_group_volumes[std::to_underlying(group)];
  if (ramp_time <= 0.0f)
    volume = {.value = target_value, .target = target_value, .velocity = 0.0f};
  else {
    volume.target = target_value;
    volume.velocity = rate_of_change_from_ramp_time(ramp_time);
  }
}

std::expected<void, StarException> Mixer::update(float, PositionalAttenuationFunction positional_attenuation_function) {
  std::lock_guard update_locker(m_update_mutex);
  auto clear_snapshot = make_scope_exit([&] { m_update_audios.clear(); });
  m_update_audios.clear();
  {
    std::lock_guard locker(m_queue_mutex);
    // Queue -> instance is the only nested lock order.
    for (auto entry = m_audios.begin(); entry != m_audios.end();) {
      if (entry->first->finished())
        entry = m_audios.erase(entry);
      else
        ++entry;
    }
    m_update_audios.reserve(m_audios.size());
    for (auto const& entry : m_audios)
      m_update_audios.push_back(entry.second);
  }
  m_attenuation_buffer.resize(m_channels);
  for (auto const& audio_state : m_update_audios) {
    auto const& audio_instance = audio_state->audio;
    std::optional<Vec2F> position;
    float range_multiplier;
    {
      std::lock_guard locker(audio_instance->m_mutex);
      position = audio_instance->m_position;
      range_multiplier = audio_instance->m_range_multiplier;
    }
    std::ranges::fill(m_attenuation_buffer, 0.0f);
    if (positional_attenuation_function && position) {
      // One synchronous batch per source; external geometry code holds no instance
      // or queue lock and may safely inspect or change the audio instance.
      auto attenuated = positional_attenuation_function(*position, range_multiplier, m_attenuation_buffer);
      if (!attenuated)
        return std::unexpected(std::move(attenuated.error()));
    }
    {
      std::lock_guard locker(audio_instance->m_mutex);
      for (std::size_t channel = 0; channel < m_channels; ++channel)
        audio_state->positional_channel_volumes[channel] = 1.0f - m_attenuation_buffer[channel];
    }
  }
  {
    std::lock_guard locker(m_effects_mutex);
    for (auto effect = m_effects.begin(); effect != m_effects.end();) {
      if (effect->second->finished)
        effect = m_effects.erase(effect);
      else
        ++effect;
    }
  }
  return {};
}
}
