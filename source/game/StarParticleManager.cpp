import star.format;
import star.json;
import star.xx_hash;
import star.color;
import star.bi_map;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import std;
import star.game_types;
import star.vector;
import star.set;
import star.worker_pool;

import star.sector_array_2d;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.version;
import star.logging;

import star.world_geometry;
import star.animation;
import star.particle;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;

import star.particle_manager;
import star.root;

import star.line;
namespace Star {

ParticleManager::ParticleManager(Root& root, WorldGeometry const& worldGeometry, std::shared_ptr<ClientTileSectorArray> const& tileSectorArray)
  : m_root(root), m_worldGeometry(worldGeometry), m_undergroundLevel(0.0f), m_tileSectorArray(tileSectorArray) {}

void ParticleManager::add(Particle particle) {
  m_presentationHistory.push_back(
    {particle.presentationState(), 0.0f, s_nextPresentationIdentity.fetch_add(1, std::memory_order_relaxed)});
  m_particles.push_back(std::move(particle));
}

void ParticleManager::addParticles(List<Particle>&& particles) {
  for (auto& particle : particles)
    add(std::move(particle));
  particles.clear();
}

std::size_t ParticleManager::count() const {
  return m_particles.size();
}

void ParticleManager::clear() {
  m_particles.clear();
  m_nextParticles.clear();
  m_presentationHistory.clear();
  m_nextPresentationHistory.clear();
}

void ParticleManager::setUndergroundLevel(float undergroundLevel) {
  m_undergroundLevel = undergroundLevel;
}

std::expected<void, StarException> ParticleManager::update(float dt, RectF const& cullRegion, float wind) {
  if (!m_tileSectorArray)
    return {};

  auto cullRects = m_worldGeometry.splitRect(cullRegion);
  if (!cullRects)
    return std::unexpected(std::move(cullRects.error()));

  for (std::size_t i = 0; i != m_particles.size(); ++i) {
    auto& particle = m_particles[i];
    bool inRegion = false;
    Vec2F worldPos = m_worldGeometry.xwrap(particle.position);
    for (auto const& cullRect : *cullRects) {
      if (cullRect.contains(worldPos)) {
        inRegion = true;
        break;
      }
    }
    if (!inRegion)
      continue;

    PresentationHistory history{particle.presentationState(), dt, m_presentationHistory[i].identity};
    auto updated = particle.update(m_root, dt, Vec2F(wind, 0));
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    Vec2I pos(particle.position.floor());
    TileType tiletype;
    auto const& tile = m_tileSectorArray->tile(pos);
    if (isSolidColliding(tile.getCollision()))
      tiletype = TileType::Colliding;
    else if (tile.liquid.level > 0.5f)
      tiletype = TileType::Water;
    else
      tiletype = TileType::Empty;

    if (particle.collidesForeground && tiletype == TileType::Colliding) {
      RectF colRect;
      colRect.setXMax(std::ceil(particle.position[0]));
      colRect.setXMin(std::floor(particle.position[0]));
      colRect.setYMax(std::ceil(particle.position[1]));
      colRect.setYMin(std::floor(particle.position[1]));
      Line2F colLine(particle.position, particle.position - particle.velocity);

      auto collisionPosition = colRect.edgeIntersection(colLine).point;
      if (particle.position[0] > colRect.center()[0])
        collisionPosition[0] += 0.1f;
      else if (particle.position[0] < colRect.center()[0])
        collisionPosition[0] -= 0.1f;
      if (particle.position[1] > colRect.center()[1])
        collisionPosition[1] += 0.1f;
      else if (particle.position[1] < colRect.center()[1])
        collisionPosition[1] -= 0.1f;

      particle.collide(collisionPosition);
    }

    if (particle.underwaterOnly && tiletype == TileType::Empty)
      particle.destroy(false);

    if (particle.collidesLiquid && tiletype == TileType::Water)
      particle.destroy(false);

    if (particle.trail && particle.timeToLive >= 0.0f) {
      auto trail = particle;
      trail.trail = false;
      trail.timeToLive = 0;
      trail.velocity = {};
      m_nextPresentationHistory.push_back(
        {trail.presentationState(), 0.0f, s_nextPresentationIdentity.fetch_add(1, std::memory_order_relaxed)});
      m_nextParticles.push_back(std::move(trail));
    }

    if (!particle.dead()) {
      m_nextPresentationHistory.push_back(std::move(history));
      m_nextParticles.push_back(std::move(particle));
    }
  }

  m_particles.clear();
  std::swap(m_particles, m_nextParticles);
  m_presentationHistory.clear();
  std::swap(m_presentationHistory, m_nextPresentationHistory);
  return {};
}

List<Particle> const& ParticleManager::particles() const {
  return m_particles;
}

void ParticleManager::sampleParticles(float interpolation, List<Particle>& samples) const {
  samples.resize(m_particles.size());
  for (std::size_t i = 0; i != m_particles.size(); ++i) {
    auto const& history = m_presentationHistory[i];
    samples[i].samplePresentation(m_particles[i], history.previous, m_worldGeometry,
        interpolation, history.timestep, history.identity);
  }
}

void ParticleManager::resetPresentation() {
  for (std::size_t i = 0; i != m_particles.size(); ++i) {
    m_presentationHistory[i].previous = m_particles[i].presentationState();
    m_presentationHistory[i].timestep = 0.0f;
  }
}


}
