module;

import std;
import star.format;
import star.audio;
import star.string;
import star.list;
import star.map;
import star.vector;
import star.bi_map;

namespace Star {

class Logger;

class Mixer;

struct RampedValue {
  float value;
  float target;
  float velocity;
};

enum class MixerGroup : std::uint8_t {
  Effects,
  Music,
  Cinematic,
  Instruments
};
inline constexpr auto mixer_group_names =
  sortedEnumNames(std::to_array<std::pair<MixerGroup, std::string_view>>({{MixerGroup::Effects, "Effects"},
                                                                          {MixerGroup::Music, "Music"},
                                                                          {MixerGroup::Cinematic, "Cinematic"},
                                                                          {MixerGroup::Instruments, "Instruments"}}));

class AudioInstance {
private:
  struct ConstructionKey {
    explicit ConstructionKey() = default;
  };

public:
  AudioInstance(ConstructionKey, Audio audio);
  [[nodiscard]] static std::expected<std::shared_ptr<AudioInstance>, StarException> create(Audio const& audio);

  std::optional<Vec2F> position() const;
  void set_position(std::optional<Vec2F> position);
  // If the audio has no position, sets the position to zero before translating
  void translate(Vec2F const& distance);

  float range_multiplier() const;
  void set_range_multiplier(float range_multiplier);

  void set_volume(float target_value, float ramp_time = 0.0f);
  void set_pitch_multiplier(float target_value, float ramp_time = 0.0f);

  // Returns the currently remaining loops
  int loops() const;
  // Sets the remaining loops, set to 0 to stop looping
  void set_loops(int loops);

  // Returns the current audio playing time position
  [[nodiscard]] std::expected<double, StarException> current_time() const;
  // Total length of time of the audio in seconds
  [[nodiscard]] std::expected<double, StarException> total_time() const;
  // Seeks the audio to the current time in seconds
  [[nodiscard]] std::expected<void, StarException> seek_time(double time);

  // The MixerGroup defaults to Effects
  MixerGroup mixer_group() const;
  void set_mixer_group(MixerGroup mixer_group);

  // If set, uses wall clock time in milliseconds to set precise start and stop
  // times for the AudioInstance
  void set_clock_start(std::optional<std::int64_t> clock_start_time);
  void set_clock_stop(std::optional<std::int64_t> clock_stop_time, std::int64_t fade_out_time = 0);

  void stop(float ramp_time = 0.0f);
  bool finished() const;

private:
  friend class Mixer;

  mutable std::mutex m_mutex;

  Audio m_audio;

  MixerGroup m_mixer_group;

  RampedValue m_volume;

  RampedValue m_pitch_multiplier;

  int m_loops;
  bool m_stopping;
  bool m_finished;

  std::optional<Vec2F> m_position;
  float m_range_multiplier;

  std::optional<std::int64_t> m_clock_start;
  std::optional<std::int64_t> m_clock_stop;
  std::int64_t m_clock_stop_fade_out;
};

// Thread safe mixer class with basic effects support.
class Mixer {
public:
  using ExtraMixFunction = std::function<std::expected<void, StarException>(std::span<std::int16_t> buffer,
                                                                            std::size_t frames, unsigned channels)>;
  using EffectFunction = std::function<std::expected<void, StarException>(std::span<std::int16_t> buffer,
                                                                          std::size_t frames, unsigned channels)>;
  using PositionalAttenuationFunction = std::function<std::expected<void, StarException>(
    Vec2F position, float range_multiplier, std::span<float> attenuation)>;

  Mixer(Logger& logger, unsigned sample_rate, unsigned channels);

  unsigned sample_rate() const;
  unsigned channels() const;

  // Moving-average low-pass filter with zero-padded history.
  EffectFunction low_pass(std::size_t average_size) const;
  // Construct a very simple echo filter.
  EffectFunction echo(float time, float dry, float wet) const;

  // Adds / removes effects that affect all playback.
  [[nodiscard]] std::expected<void, StarException> add_effect(String const& effect_name, EffectFunction effect_function,
                                                              float ramp_time);
  void remove_effect(String const& effect_name, float ramp_time);
  StringList current_effects();
  bool has_effect(String const& effect_name);

  // Global speed. Zero pauses Effects/Instruments cursors, not fades or wall clocks.
  void set_speed(float speed);

  // Global volume
  void set_volume(float volume, float ramp_time);

  // per mixer group volume
  void set_group_volume(MixerGroup group, float target_value, float ramp_time = 0.0f);

  [[nodiscard]] std::expected<void, StarException> play(std::shared_ptr<AudioInstance> sample);

  // Reads pending audio data.  This is thread safe with the other Mixer
  // methods, but only one call to read may be active at a time.
  [[nodiscard]] std::expected<void, StarException> read(std::span<std::int16_t> samples, std::size_t frame_count,
                                                        ExtraMixFunction extra_mix_function = {});

  // Call within the main loop of the program using Mixer, calculates
  // positional attenuation of audio and does cleanup.
  [[nodiscard]] std::expected<void, StarException>
  update(float dt, PositionalAttenuationFunction positional_attenuation_function = {});

private:
  struct EffectInfo {
    EffectFunction effect_function;
    float amount;
    float velocity;
    bool finished;
  };

  struct AudioState {
    std::shared_ptr<AudioInstance> audio;
    // Protected by audio->m_mutex, together with the corresponding instance state.
    std::vector<float> positional_channel_volumes;
  };

  Logger& m_logger;
  std::mutex m_mutex;
  unsigned m_sample_rate;
  unsigned m_channels;

  RampedValue m_volume;

  std::mutex m_queue_mutex;
  HashMap<std::shared_ptr<AudioInstance>, std::shared_ptr<AudioState>> m_audios;

  std::mutex m_effects_mutex;
  StringMap<std::shared_ptr<EffectInfo>> m_effects;

  List<std::int16_t> m_mix_buffer;
  // Single-reader scratch: retain capacity and owning states, never borrowed map slots.
  std::vector<std::shared_ptr<AudioState>> m_read_audios;
  std::vector<std::shared_ptr<EffectInfo>> m_read_effects;

  std::mutex m_update_mutex;
  std::vector<std::shared_ptr<AudioState>> m_update_audios;
  std::vector<float> m_attenuation_buffer;

  std::array<RampedValue, 4> m_group_volumes;
  std::atomic<float> m_speed;
};

}

export module star.mixer;

export namespace Star {
using ::Star::AudioInstance;
using ::Star::Mixer;
using ::Star::mixer_group_names;
using ::Star::MixerGroup;
using ::Star::RampedValue;
}
