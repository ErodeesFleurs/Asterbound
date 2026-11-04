module;
import star.format;
import std;
import star.string;
import star.list;
import star.json;

import star.vector;

namespace Star {
class Root;

class AudioInstance;
struct Particle;

class EffectSource {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<EffectSource>, StarException> create(Root& root, String const& kind, String suggestedSpawnLocation, Json const& definition);
  String const& kind() const;
  void tick(float dt);
  bool expired() const;
  void stop();
  [[nodiscard]] std::expected<List<String>, StarException> particles();
  [[nodiscard]] std::expected<List<std::shared_ptr<AudioInstance>>, StarException> sounds(Vec2F offset);
  // Acknowledge the current tick's particle/audio stages after collection.
  void finishEventCollection();
  String effectSpawnLocation() const;
  String suggestedSpawnLocation() const;

private:
  explicit EffectSource(Root& root) : m_root(root) {}
  struct ParticleChoices {
    std::shared_ptr<JsonArray const> options;
    List<std::optional<StringList>> decoded;
  };

  Root& m_root;
  String m_kind;
  Json m_config;
  bool m_loops;
  float m_loopDuration;
  float m_durationVariance;
  String m_effectSpawnLocation;
  String m_suggestedSpawnLocation;

  bool m_initialTick;
  bool m_loopTick;
  bool m_finalTick;
  float m_timer;
  bool m_expired;
  bool m_stop;

  List<std::shared_ptr<AudioInstance>> m_mainSounds;
  // Each immutable stage is acquired lazily; only selected successful decodes are retained.
  std::array<std::unique_ptr<ParticleChoices>, 3> m_particleChoices;
};

class EffectSourceConfig {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<EffectSourceConfig>, StarException> create(Root& root, Json const& config);
  String const& kind();
  [[nodiscard]] std::expected<std::shared_ptr<EffectSource>, StarException> instance(String const& suggestedSpawnLocation);

private:
  explicit EffectSourceConfig(Root& root) : m_root(root) {}
  Root& m_root;
  String m_kind;
  Json m_config;
};

class EffectSourceDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<EffectSourceDatabase>, StarException> create(Root& root);

  [[nodiscard]] std::expected<std::shared_ptr<EffectSourceConfig>, StarException> effectSourceConfig(String const& kind) const;

private:
  EffectSourceDatabase() = default;
  StringMap<std::shared_ptr<EffectSourceConfig>> m_sourceConfigs;
};

[[nodiscard]] std::expected<List<Particle>, StarException> particlesFromDefinition(Root& root, Json const& config, Vec2F const& position = Vec2F());
[[nodiscard]] std::expected<List<std::shared_ptr<AudioInstance>>, StarException> soundsFromDefinition(Root& root, Json const& config, Vec2F const& position = Vec2F());

}

export module star.effect_source_database;

export namespace Star {
  using ::Star::AudioInstance;
  using ::Star::Particle;
  using ::Star::EffectSource;
  using ::Star::EffectSourceConfig;
  using ::Star::EffectSourceDatabase;
  using ::Star::particlesFromDefinition;
  using ::Star::soundsFromDefinition;
}
