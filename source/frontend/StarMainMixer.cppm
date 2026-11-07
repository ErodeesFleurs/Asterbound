module;

import std;
import star.format;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.set;
import star.bi_map;
import star.game_types;


import star.mixer;

namespace Star {

class UniverseClient;
class WorldPainter;
class Root;
class Voice;

class MainMixer {
public:
  MainMixer(Root& root, unsigned sampleRate, unsigned channels);

  void setUniverseClient(std::shared_ptr<UniverseClient> universeClient);
  // ClientApplication clears this borrow before releasing its painter.
  void setWorldPainter(std::optional<std::reference_wrapper<WorldPainter>> worldPainter);
  // Borrows the client voice service; bind after construction and clear before it is destroyed.
  void setVoice(std::optional<std::reference_wrapper<Voice>> voice);

  [[nodiscard]] std::expected<void, StarException> update(float dt, bool muteSfx = false, bool muteMusic = false);

  std::shared_ptr<Mixer> mixer() const;

  void setSpeed(float speed);
  void setVolume(float volume, float rampTime = 0.0f);
  [[nodiscard]] std::expected<void, StarException> read(std::span<std::int16_t> sampleData, std::size_t frameCount, Mixer::ExtraMixFunction = {});

private:
  std::reference_wrapper<Root> m_root;
  std::optional<std::reference_wrapper<Voice>> m_voice;
  std::shared_ptr<UniverseClient> m_universeClient;
  std::optional<std::reference_wrapper<WorldPainter>> m_worldPainter;
  std::shared_ptr<Mixer> m_mixer;
  Set<MixerGroup> m_mutedGroups;
  Map<MixerGroup, float> m_groupVolumes;
};

}

export module star.main_mixer;

export namespace Star {
  using ::Star::UniverseClient;
  using ::Star::WorldPainter;
  using ::Star::MainMixer;
}
