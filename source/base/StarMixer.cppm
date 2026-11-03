module;

import std;
import star.format;
import star.audio;
import star.string;
import star.list;
import star.map;
import star.set;
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
inline constexpr auto MixerGroupNames =
  sortedEnumNames(std::to_array<std::pair<MixerGroup, std::string_view>>({{MixerGroup::Effects, "Effects"},
                                                                          {MixerGroup::Music, "Music"},
                                                                          {MixerGroup::Cinematic, "Cinematic"},
                                                                          {MixerGroup::Instruments, "Instruments"}}));

class AudioInstance {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<AudioInstance>, StarException> create(Audio const& audio);

  std::optional<Vec2F> position() const;
  void setPosition(std::optional<Vec2F> position);
  // If the audio has no position, sets the position to zero before translating
  void translate(Vec2F const& distance);

  float rangeMultiplier() const;
  void setRangeMultiplier(float rangeMultiplier);

  void setVolume(float targetValue, float rampTime = 0.0f);
  void setPitchMultiplier(float targetValue, float rampTime = 0.0f);

  // Returns the currently remaining loops
  int loops() const;
  // Sets the remaining loops, set to 0 to stop looping
  void setLoops(int loops);

  // Returns the current audio playing time position
  [[nodiscard]] std::expected<double, StarException> currentTime() const;
  // Total length of time of the audio in seconds
  [[nodiscard]] std::expected<double, StarException> totalTime() const;
  // Seeks the audio to the current time in seconds
  [[nodiscard]] std::expected<void, StarException> seekTime(double time);

  // The MixerGroup defaults to Effects
  MixerGroup mixerGroup() const;
  void setMixerGroup(MixerGroup mixerGroup);

  // If set, uses wall clock time in milliseconds to set precise start and stop
  // times for the AudioInstance
  void setClockStart(std::optional<std::int64_t> clockStartTime);
  void setClockStop(std::optional<std::int64_t> clockStopTime, std::int64_t fadeOutTime = 0);

  void stop(float rampTime = 0.0f);
  bool finished() const;

private:
  friend class Mixer;

  explicit AudioInstance(Audio audio);

  mutable std::mutex m_mutex;

  Audio m_audio;

  MixerGroup m_mixerGroup;

  RampedValue m_volume;

  float m_pitchMultiplier;
  float m_pitchMultiplierTarget;
  float m_pitchMultiplierVelocity;

  int m_loops;
  bool m_stopping;
  bool m_finished;

  std::optional<Vec2F> m_position;
  float m_rangeMultiplier;

  std::optional<std::int64_t> m_clockStart;
  std::optional<std::int64_t> m_clockStop;
  std::int64_t m_clockStopFadeOut;
};

// Thread safe mixer class with basic effects support.
class Mixer {
public:
  typedef std::function<std::expected<void, StarException>(std::span<std::int16_t> buffer, std::size_t frames, unsigned channels)> ExtraMixFunction;
  typedef std::function<std::expected<void, StarException>(std::span<std::int16_t> buffer, std::size_t frames, unsigned channels)> EffectFunction;
  typedef std::function<std::expected<float, StarException>(unsigned, Vec2F, float)> PositionalAttenuationFunction;

  Mixer(Logger& logger, unsigned sampleRate, unsigned channels);

  unsigned sampleRate() const;
  unsigned channels() const;

  // Construct a really crappy low-pass filter based on averaging
  EffectFunction lowpass(std::size_t avgSize) const;
  // Construct a very simple echo filter.
  EffectFunction echo(float time, float dry, float wet) const;

  // Adds / removes effects that affect all playback.
  [[nodiscard]] std::expected<void, StarException> addEffect(String const& effectName, EffectFunction effectFunction, float rampTime);
  void removeEffect(String const& effectName, float rampTime);
  StringList currentEffects();
  bool hasEffect(String const& effectName);

  // Global speed
  void setSpeed(float speed);

  // Global volume
  void setVolume(float volume, float rampTime);

  // per mixer group volume
  void setGroupVolume(MixerGroup group, float targetValue, float rampTime = 0.0f);

  [[nodiscard]] std::expected<void, StarException> play(std::shared_ptr<AudioInstance> sample);

  void stopAll(float rampTime);

  // Reads pending audio data.  This is thread safe with the other Mixer
  // methods, but only one call to read may be active at a time.
  [[nodiscard]] std::expected<void, StarException> read(std::span<std::int16_t> samples, std::size_t frameCount, ExtraMixFunction extraMixFunction = {});

  // Call within the main loop of the program using Mixer, calculates
  // positional attenuation of audio and does cleanup.
  [[nodiscard]] std::expected<void, StarException> update(float dt, PositionalAttenuationFunction positionalAttenuationFunction = {});

private:
  struct EffectInfo {
    EffectFunction effectFunction;
    float amount;
    float velocity;
    bool finished;
  };

  struct AudioState {
    List<float> positionalChannelVolumes;
  };

  Logger& m_logger;
  std::mutex m_mutex;
  unsigned m_sampleRate;
  unsigned m_channels;

  RampedValue m_volume;

  std::mutex m_queueMutex;

  HashMap<std::shared_ptr<AudioInstance>, AudioState> m_audios;

  std::mutex m_effectsMutex;
  StringMap<std::shared_ptr<EffectInfo>> m_effects;

  List<std::int16_t> m_mixBuffer;

  Map<MixerGroup, RampedValue> m_groupVolumes;
  std::atomic<float> m_speed;
};

}

export module star.mixer;

export namespace Star {
  using ::Star::RampedValue;
  using ::Star::MixerGroup;
  using ::Star::MixerGroupNames;
  using ::Star::AudioInstance;
  using ::Star::Mixer;
}
