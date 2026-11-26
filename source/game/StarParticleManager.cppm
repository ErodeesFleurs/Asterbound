module;
import star.format;
import star.list;
import star.vector;
import star.rect;
import std;
import star.world_geometry;
import star.particle;
import star.world_tiles;
namespace Star {

class Root;

class ParticleManager {
public:
  ParticleManager(Root& root, WorldGeometry const& world_geometry, std::shared_ptr<ClientTileSectorArray> const& tile_sector_array);
  ParticleManager(ParticleManager const&) = delete;
  ParticleManager& operator=(ParticleManager const&) = delete;

  void add(Particle particle);
  // Consumes elements but retains the caller's allocation for the next tick.
  void add_particles(List<Particle>&& particles);

  std::size_t count() const;
  void clear();

  [[nodiscard]] std::expected<void, StarException> update(float dt, RectF const& cull_region, float wind);

  List<Particle> const& particles() const;
  // Fill a reusable owned display list. Births and deaths are discrete fixed-
  // tick events; only surviving particle state is sampled between endpoints.
  void sample_particles(float interpolation, List<Particle>& samples) const;
  void reset_presentation();

private:
  enum class TileInteraction { None, Foreground, Kill };

  struct PresentationHistory {
    Particle::PresentationState previous;
    float timestep;
    std::uint64_t identity;
  };

  struct PreparedParticle {
    std::size_t index;
    std::optional<Particle::Motion> motion;
    TileInteraction interaction = TileInteraction::None;
    std::optional<Particle::PreparedAnimation> animation;
  };

  void append_particle(Particle&& particle, std::uint64_t identity);
  TileInteraction tile_interaction(Particle const& particle, Vec2F const& position) const;
  void finish_particle(Particle& particle, PresentationHistory&& history, TileInteraction tile);

  Root& m_root;

  List<Particle> m_particles;
  List<Particle> m_next_particles;
  List<PresentationHistory> m_presentation_history;
  List<PresentationHistory> m_next_presentation_history;
  inline static std::atomic_uint64_t s_next_presentation_identity{1};

  std::optional<std::size_t> m_first_uninitialized_animation;
  List<PreparedParticle> m_prepared_particles;

  WorldGeometry m_world_geometry;
  std::shared_ptr<ClientTileSectorArray> m_tile_sector_array;
};

}

export module star.particle_manager;

export namespace Star {
using ::Star::ParticleManager;
}
