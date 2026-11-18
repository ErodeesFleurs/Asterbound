module;

import std;
import star.format;
import star.mixer;

namespace Star {

class UniverseClient;
class WorldPainter;
class Root;
class Voice;

class MainMixer {
public:
  MainMixer(Root& root, unsigned sample_rate, unsigned channels);

  void set_universe_client(std::shared_ptr<UniverseClient> universe_client);
  // ClientApplication clears this borrow before releasing its painter.
  void set_world_painter(std::optional<std::reference_wrapper<WorldPainter>> world_painter);
  // Borrows the client voice service; bind after construction and clear before it is destroyed.
  void set_voice(std::optional<std::reference_wrapper<Voice>> voice);

  [[nodiscard]] std::expected<void, StarException> update(float dt, bool mute_sfx = false, bool mute_music = false);

  std::shared_ptr<Mixer> mixer() const;

  void set_speed(float speed);
  void set_volume(float volume, float ramp_time = 0.0f);
  [[nodiscard]] std::expected<void, StarException> read(std::span<std::int16_t> sample_data, std::size_t frame_count,
                                                        Mixer::ExtraMixFunction = {});

private:
  struct VolumeSetting {
    std::optional<float> raw;
    float amplitude = 0.0f;
  };

  std::reference_wrapper<Root> m_root;
  std::optional<std::reference_wrapper<Voice>> m_voice;
  std::shared_ptr<UniverseClient> m_universe_client;
  std::optional<std::reference_wrapper<WorldPainter>> m_world_painter;
  std::shared_ptr<Mixer> m_mixer;
  std::array<bool, 4> m_muted_groups{};
  std::array<std::optional<float>, 4> m_group_volumes{};
  std::array<VolumeSetting, 3> m_volume_settings{};
};

}

export module star.main_mixer;

export namespace Star {
  using ::Star::UniverseClient;
  using ::Star::WorldPainter;
  using ::Star::MainMixer;
}
