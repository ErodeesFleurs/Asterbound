module;
import std;
import star.format;
import star.string;
import star.json;

import star.particle;

namespace Star {

class Root;

class ParticleConfig {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  ParticleConfig(ConstructionKey, Root& root, String&& kind, Particle&& particle, Particle&& variance)
    : m_root(root), m_kind(std::move(kind)), m_particle(std::move(particle)), m_variance(std::move(variance)) {}
  [[nodiscard]] static std::expected<std::shared_ptr<ParticleConfig>, StarException> create(Root& root, Json const& config);

  String const& kind() const;
  Particle instance() const;

private:
  Root& m_root;
  String m_kind;
  Particle m_particle;
  Particle m_variance;
};

class ParticleDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  ParticleDatabase(ConstructionKey, Root& root) : m_root(root) {}
  [[nodiscard]] static std::expected<std::shared_ptr<ParticleDatabase>, StarException> create(Root& root);

  [[nodiscard]] std::expected<std::shared_ptr<ParticleConfig>, StarException> config(String const& kind) const;

  // Builds a reusable factory from a registered kind or an inline definition with optional variance.
  [[nodiscard]] std::expected<ParticleVariantCreator, StarException> particle_creator(Json const& kind_or_config, String const& relative_path = "") const;

  // Creates one particle with the same variance as the factory, without building a callable.
  [[nodiscard]] std::expected<Particle, StarException> particle(Json const& kind_or_config, String const& relative_path = "") const;

private:
  [[nodiscard]] std::expected<std::reference_wrapper<std::shared_ptr<ParticleConfig> const>, StarException>
      find_config(String const& kind) const;

  Root& m_root;
  StringMap<std::shared_ptr<ParticleConfig>> m_configs;
};

}

export module star.particle_database;

export namespace Star {
  using ::Star::Particle;
  using ::Star::ParticleVariantCreator;
  using ::Star::ParticleConfig;
  using ::Star::ParticleDatabase;
}
