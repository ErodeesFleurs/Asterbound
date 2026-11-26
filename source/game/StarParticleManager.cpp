import std;
import star.collision_block;
import star.format;
import star.line;
import star.liquid_types;
import star.list;
import star.particle;
import star.particle_manager;
import star.rect;
import star.root;
import star.tile_sector_array;
import star.vector;
import star.world_geometry;
import star.world_tiles;

namespace Star {

ParticleManager::ParticleManager(Root& root, WorldGeometry const& world_geometry,
    std::shared_ptr<ClientTileSectorArray> const& tile_sector_array)
  : m_root(root), m_world_geometry(world_geometry), m_tile_sector_array(tile_sector_array) {}

void ParticleManager::append_particle(Particle&& particle, std::uint64_t identity) {
  if (!m_first_uninitialized_animation && particle.type == Particle::Type::Animated && !particle.animation)
    m_first_uninitialized_animation = m_particles.size();
  m_presentation_history.push_back({particle.presentation_state(), 0.0f, identity});
  m_particles.emplace_back(std::move(particle));
}

void ParticleManager::add(Particle particle) {
  append_particle(std::move(particle), s_next_presentation_identity.fetch_add(1, std::memory_order_relaxed));
}

void ParticleManager::add_particles(List<Particle>&& particles) {
  if (particles.empty())
    return;
  auto identity = s_next_presentation_identity.fetch_add(particles.size(), std::memory_order_relaxed);
  for (auto& particle : particles)
    append_particle(std::move(particle), identity++);
  particles.clear();
}

std::size_t ParticleManager::count() const {
  return m_particles.size();
}

void ParticleManager::clear() {
  m_particles.clear();
  m_next_particles.clear();
  m_presentation_history.clear();
  m_next_presentation_history.clear();
  m_first_uninitialized_animation.reset();
  m_prepared_particles.clear();
}

ParticleManager::TileInteraction ParticleManager::tile_interaction(Particle const& particle, Vec2F const& position) const {
  if (!particle.collides_foreground && !particle.collides_liquid && !particle.underwater_only)
    return TileInteraction::None;

  auto const& tile = m_tile_sector_array->tile(Vec2I(position.floor()));
  if (isSolidColliding(tile.getCollision()))
    return particle.collides_foreground ? TileInteraction::Foreground : TileInteraction::None;
  if (tile.liquid.level > 0.5f)
    return particle.collides_liquid ? TileInteraction::Kill : TileInteraction::None;
  return particle.underwater_only ? TileInteraction::Kill : TileInteraction::None;
}

void ParticleManager::finish_particle(Particle& particle, PresentationHistory&& history, TileInteraction tile) {
  if (tile == TileInteraction::Foreground) {
    RectF collision_rect;
    collision_rect.set_x_max(std::ceil(particle.position[0]));
    collision_rect.set_x_min(std::floor(particle.position[0]));
    collision_rect.set_y_max(std::ceil(particle.position[1]));
    collision_rect.set_y_min(std::floor(particle.position[1]));
    Line2F collision_line(particle.position, particle.position - particle.velocity);
    auto collision_position = collision_rect.edge_intersection(collision_line).point;
    auto center = collision_rect.center();
    for (unsigned axis = 0; axis != 2; ++axis) {
      if (particle.position[axis] > center[axis])
        collision_position[axis] += 0.1f;
      else if (particle.position[axis] < center[axis])
        collision_position[axis] -= 0.1f;
    }
    particle.collide(collision_position);
  } else if (tile == TileInteraction::Kill) {
    particle.destroy(false);
  }

  if (particle.trail && particle.time_to_live >= 0.0f) {
    auto trail = particle;
    trail.trail = false;
    trail.time_to_live = 0;
    trail.velocity = {};
    m_next_presentation_history.push_back(
        {trail.presentation_state(), 0.0f, s_next_presentation_identity.fetch_add(1, std::memory_order_relaxed)});
    m_next_particles.emplace_back(std::move(trail));
  }

  if (!particle.dead()) {
    m_next_presentation_history.emplace_back(std::move(history));
    m_next_particles.emplace_back(std::move(particle));
  }
}

std::expected<void, StarException> ParticleManager::update(float dt, RectF const& cull_region, float wind) {
  if (!m_tile_sector_array)
    return {};

  auto cull_rects = m_world_geometry.split_rect(cull_region);
  if (!cull_rects)
    return std::unexpected(std::move(cull_rects.error()));

  auto is_visible = [&](Particle const& particle) {
    auto world_position = m_world_geometry.xwrap(particle.position);
    for (auto const& rect : *cull_rects)
      if (rect.contains(world_position))
        return true;
    return false;
  };
  auto wind_velocity = Vec2F(wind, 0);

  // Only added particles can need lazy assets. Decide what would happen to
  // them without moving live particles or drawing animation variants.
  if (m_first_uninitialized_animation) {
    for (auto i = *m_first_uninitialized_animation; i != m_particles.size(); ++i) {
      auto const& particle = m_particles[i];
      if (particle.type != Particle::Type::Animated || particle.animation)
        continue;

      auto& prepared = m_prepared_particles.emplace_back(i);
      if (!is_visible(particle))
        continue;

      auto& motion = prepared.motion.emplace(particle.next_motion(dt, wind_velocity));
      prepared.interaction = tile_interaction(particle, motion.position);
      bool foreground_collision = prepared.interaction == TileInteraction::Foreground;
      if (particle.will_use_destruction_image(motion.time_to_live, foreground_collision))
        continue;

      float final_lifetime = motion.time_to_live;
      if (prepared.interaction == TileInteraction::Kill)
        final_lifetime = -particle.destruction_time - 1.0f;
      else if (foreground_collision && final_lifetime >= 0.0f)
        final_lifetime = 0.0f;
      if (final_lifetime < -particle.destruction_time && !(particle.trail && final_lifetime >= 0.0f))
        continue;

      auto animation = particle.prepare_animation(m_root);
      if (!animation) {
        m_prepared_particles.clear();
        return std::unexpected(std::move(animation.error()));
      }
      prepared.animation.emplace(std::move(*animation));
    }
  }

  auto previous_state = [&](std::size_t i) -> PresentationHistory {
    return {m_particles[i].presentation_state(), dt, m_presentation_history[i].identity};
  };
  std::size_t i = 0;
  auto advance_ready = [&](std::size_t end) {
    for (; i != end; ++i) {
      auto& particle = m_particles[i];
      if (!is_visible(particle))
        continue;
      auto history = previous_state(i);
      auto motion = particle.next_motion(dt, wind_velocity);
      particle.advance(dt, motion);
      finish_particle(particle, std::move(history), tile_interaction(particle, motion.position));
    }
  };

  // Everything below can commit in order without another fallible asset read.
  // Ordinary ticks go directly to advance_ready, without a staging pass.
  for (auto& prepared : m_prepared_particles) {
    advance_ready(prepared.index);
    if (prepared.motion) {
      auto& particle = m_particles[i];
      auto history = previous_state(i);
      if (prepared.animation)
        particle.activate_animation(std::move(*prepared.animation));
      particle.advance(dt, *prepared.motion);
      finish_particle(particle, std::move(history), prepared.interaction);
    }
    ++i;
  }
  advance_ready(m_particles.size());

  m_prepared_particles.clear();
  m_first_uninitialized_animation.reset();
  m_particles.clear();
  m_particles.swap(m_next_particles);
  m_presentation_history.clear();
  m_presentation_history.swap(m_next_presentation_history);
  return {};
}

List<Particle> const& ParticleManager::particles() const {
  return m_particles;
}

void ParticleManager::sample_particles(float interpolation, List<Particle>& samples) const {
  samples.resize(m_particles.size());
  for (std::size_t i = 0; i != m_particles.size(); ++i) {
    auto const& history = m_presentation_history[i];
    samples[i].sample_presentation(m_particles[i], history.previous, m_world_geometry,
        interpolation, history.timestep, history.identity);
  }
}

void ParticleManager::reset_presentation() {
  for (std::size_t i = 0; i != m_particles.size(); ++i) {
    m_presentation_history[i].previous = m_particles[i].presentation_state();
    m_presentation_history[i].timestep = 0.0f;
  }
}


}
