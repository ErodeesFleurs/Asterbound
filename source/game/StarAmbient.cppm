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
  AmbientTrackGroup();
  AmbientTrackGroup(StringList tracks);
  [[nodiscard]] static std::expected<AmbientTrackGroup, StarException> create(Json const& config, String const& directory = "");

  Json toJson() const;

  StringList tracks;
};

// represents the ambient sounds data for a biome
struct AmbientNoisesDescription {
  AmbientNoisesDescription();
  AmbientNoisesDescription(AmbientTrackGroup day, AmbientTrackGroup night, int loops = -1);
  [[nodiscard]] static std::expected<AmbientNoisesDescription, StarException> create(Json const& config, String const& directory = "");

  Json toJson() const;

  AmbientTrackGroup daySounds;
  AmbientTrackGroup nightSounds;
  int trackLoops = -1;
};

using WeatherNoisesDescription = AmbientTrackGroup;

// manages the running ambient sounds
class AmbientManager {
public:
  explicit AmbientManager(Root& root);

  // Automatically calls cancelAll();
  ~AmbientManager();

  void setTrackSwitchGrace(float grace);
  void setTrackFadeInTime(float fadeInTime);

  // Returns a new AudioInstance if a new ambient sound is to be started.
  [[nodiscard]] std::expected<std::shared_ptr<AudioInstance>, StarException> updateAmbient(std::shared_ptr<AmbientNoisesDescription> current, bool dayTime = false);
  [[nodiscard]] std::expected<std::shared_ptr<AudioInstance>, StarException> updateWeather(std::shared_ptr<WeatherNoisesDescription> current);
  void cancelAll();

  void setVolume(float volume, float delay, float duration);

private:
  Root& m_root;
  std::shared_ptr<AudioInstance> m_currentTrack;
  std::shared_ptr<AudioInstance> m_weatherTrack;
  String m_currentTrackName;
  String m_weatherTrackName;
  float m_trackFadeInTime = 0.0f;
  std::chrono::duration<float> m_trackSwitchGrace{};
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> m_trackGraceTimestamp{};
  Deque<String> m_recentTracks;
  float m_volume = 1.0f;
  float m_delay = 0.0f;
  float m_duration = 0.0f;
  bool m_volumeChanged = false;
};

}

export module star.ambient;

export namespace Star {
  using ::Star::AudioInstance;
  using ::Star::AmbientTrackGroup;
  using ::Star::AmbientNoisesDescription;
  using ::Star::AmbientManager;
  using ::Star::WeatherNoisesDescription;
}
