module;
import star.format;

import std;
import star.string;
import star.list;
import star.json;

namespace Star {

class AudioInstance;
class Root;

struct AmbientTrackGroup {
  AmbientTrackGroup() = default;
  AmbientTrackGroup(StringList tracks);
  [[nodiscard]] static std::expected<AmbientTrackGroup, StarException> create(Json const& config, String const& directory = "");

  Json to_json() const;

  StringList tracks;
};

// represents the ambient sounds data for a biome
struct AmbientNoisesDescription {
  AmbientNoisesDescription() = default;
  AmbientNoisesDescription(AmbientTrackGroup day, AmbientTrackGroup night, int loops = -1);
  [[nodiscard]] static std::expected<AmbientNoisesDescription, StarException> create(Json const& config, String const& directory = "");

  Json to_json() const;

  AmbientTrackGroup day_sounds;
  AmbientTrackGroup night_sounds;
  int track_loops = -1;
};

// manages the running ambient sounds
class AmbientManager {
public:
  explicit AmbientManager(Root& root);

  ~AmbientManager();

  void set_track_switch_grace(float grace);
  void set_track_fade_in_time(float fade_in_time);

  // Returns a new AudioInstance if a new ambient sound is to be started.
  [[nodiscard]] std::expected<std::shared_ptr<AudioInstance>, StarException> update_ambient(std::shared_ptr<AmbientNoisesDescription const> const& current, bool day_time = false);
  [[nodiscard]] std::expected<std::shared_ptr<AudioInstance>, StarException> update_weather(std::span<String const> tracks);
  void stop_ambient(float fade_time);
  void cancel_all();

  void set_volume(float volume, float delay, float duration);

private:
  Root& m_root;
  std::shared_ptr<AudioInstance> m_current_track;
  std::shared_ptr<AudioInstance> m_weather_track;
  String m_current_track_name;
  String m_weather_track_name;
  float m_track_fade_in_time = 0.0f;
  std::chrono::duration<float> m_track_switch_grace{};
  std::chrono::steady_clock::time_point m_track_grace_timestamp{};
  Deque<String> m_recent_tracks;
  float m_volume = 1.0f;
  float m_delay = 0.0f;
  float m_duration = 0.0f;
  bool m_volume_changed = false;
};

}

export module star.ambient;

export namespace Star {
  using ::Star::AudioInstance;
  using ::Star::AmbientTrackGroup;
  using ::Star::AmbientNoisesDescription;
  using ::Star::AmbientManager;
}
