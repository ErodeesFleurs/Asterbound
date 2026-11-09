import star.memory;
import star.json;
import star.data_stream_extra;
import star.ordered_map;
import star.rect;
import star.thread;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.string;
import star.version;
import star.time;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.list;
import star.rpc_promise;
import star.lua;
import star.random;
import star.weighted_pool;
import star.color;
import star.bi_map;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;

import star.asset_source;
import star.assets;
import star.configuration;
import star.root;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.mobile_entity;
import star.scripted_entity;
import star.status_effect_entity;
import star.movement_controller;
import star.lua_components;
import star.effect_emitter;
import star.projectile;
import star.animation;
import star.particle;
import star.weather_types;
import star.world_geometry;

import star.weather;

import star.projectile_database;
import star.biome_database;


import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
import star.math_common;
import star.algorithm;
namespace Star {

ServerWeather::ServerWeather(Root& root) : m_root(root) {
  m_undergroundLevel = 0.0f;
  m_currentWeatherIndex = NPos;
  m_currentWeatherIntensity = 0.0f;
  m_currentWind = 0.0f;
  m_forceWeather = false;

  m_currentTime = 0.0;
  m_lastWeatherChangeTime = 0.0;
  m_nextWeatherChangeTime = 0.0;

}

std::expected<std::shared_ptr<ServerWeather>, StarException> ServerWeather::create(Root& root) {
  auto result = std::shared_ptr<ServerWeather>(new ServerWeather(root));
  {
    auto added = result->m_netGroup.addNetElement(result->m_weatherPoolNetState);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = result->m_netGroup.addNetElement(result->m_undergroundLevelNetState);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = result->m_netGroup.addNetElement(result->m_currentWeatherIndexNetState);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = result->m_netGroup.addNetElement(result->m_currentWeatherIntensityNetState);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = result->m_netGroup.addNetElement(result->m_currentWindNetState);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  return result;
}

void ServerWeather::setup(WeatherPool weatherPool, float undergroundLevel, WorldGeometry worldGeometry,
    WeatherEffectsActiveQuery weatherEffectsActiveQuery) {
  m_weatherPool = std::move(weatherPool);
  m_undergroundLevel = undergroundLevel;

  m_worldGeometry = worldGeometry;
  m_weatherEffectsActiveQuery = std::move(weatherEffectsActiveQuery);

  m_currentWeatherIndex = NPos;
  m_currentWeatherType = {};

  m_currentTime = 0.0;
  m_lastWeatherChangeTime = 0.0;
  m_nextWeatherChangeTime = 0.0;
}

void ServerWeather::setReferenceClock(std::shared_ptr<Clock const> referenceClock) {
  m_referenceClock = std::move(referenceClock);
  if (m_referenceClock)
    m_clockTrackingTime = m_referenceClock->time();
  else
    m_clockTrackingTime = {};
}

void ServerWeather::setClientVisibleRegions(List<RectI> regions) {
  m_clientVisibleRegions = std::move(regions);
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> ServerWeather::writeUpdate(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  auto states = setNetStates();
  if (!states) return std::unexpected(std::move(states.error()));
  return m_netGroup.writeNetState(fromVersion, rules);
}

std::expected<void, StarException> ServerWeather::update(double dt) {
  auto spawned = spawnWeatherProjectiles(dt);
  if (!spawned) return std::unexpected(std::move(spawned.error()));
  if (m_referenceClock) {
    double clockTime = m_referenceClock->time();
    if (!m_clockTrackingTime) m_clockTrackingTime = clockTime;
    else {
      dt = clockTime - *m_clockTrackingTime;
      m_clockTrackingTime = clockTime;
    }
  }
  m_currentTime += dt;
  if (m_forceWeather) {
    m_currentWeatherIntensity = m_currentWeatherType ? 1.0f : 0.0f;
  } else if (!m_weatherPool.empty()) {
    auto assets = m_root.get().assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto cooldownConfig = (*assets)->json("/weather.config:weatherCooldownTime");
    if (!cooldownConfig) return std::unexpected(std::move(cooldownConfig.error()));
    auto cooldown = cooldownConfig->toDouble();
    if (!cooldown) return std::unexpected(std::move(cooldown.error()));
    auto warmupConfig = (*assets)->json("/weather.config:weatherWarmupTime");
    if (!warmupConfig) return std::unexpected(std::move(warmupConfig.error()));
    auto warmup = warmupConfig->toDouble();
    if (!warmup) return std::unexpected(std::move(warmup.error()));
    if (m_currentTime >= m_nextWeatherChangeTime) {
      m_currentWeatherIndex = m_weatherPool.selectIndex(m_root.get().random());
      if (m_currentWeatherIndex == NPos) m_currentWeatherType = {};
      else {
        auto biomes = m_root.get().biomeDatabase();
        if (!biomes) return std::unexpected(std::move(biomes.error()));
        auto name = m_weatherPool.item(m_currentWeatherIndex);
        if (!name) return std::unexpected(std::move(name.error()));
        {
          auto value = (*biomes)->weatherType(name->get());
          if (!value) return std::unexpected(std::move(value.error()));
          m_currentWeatherType = std::move(*value);
        }
      }
      m_lastWeatherChangeTime = m_nextWeatherChangeTime;
      if (!m_currentWeatherType) return std::unexpected(StarException("No selected weather type in a nonempty weather pool"));
      auto duration = m_root.get().random().randd(m_currentWeatherType->duration[0], m_currentWeatherType->duration[1]);
      if (!duration) return std::unexpected(std::move(duration.error()));
      m_nextWeatherChangeTime = m_currentTime + *duration;
      m_currentWind = m_currentWeatherType->maximumWind * (m_root.get().random().randb() ? 1 : -1);
    }
    m_currentWeatherIntensity = std::min(std::clamp((m_currentTime - m_lastWeatherChangeTime) / *warmup, 0.0, 1.0),
                                         std::clamp((m_nextWeatherChangeTime - m_currentTime) / *cooldown, 0.0, 1.0));
  } else {
    m_currentWeatherIndex = NPos;
    m_currentWeatherType = {};
  }
  return {};
}

float ServerWeather::wind() const {
  return m_currentWind * m_currentWeatherIntensity;
}

float ServerWeather::weatherIntensity() const {
  return m_currentWeatherIntensity;
}

StringList ServerWeather::statusEffects() const {
  if (m_currentWeatherType && m_currentWeatherIntensity == 1.0)
    return m_currentWeatherType->statusEffects;
  return {};
}

List<std::shared_ptr<Projectile>> ServerWeather::pullNewProjectiles() { return std::exchange(m_newProjectiles, {}); }

std::expected<std::optional<String>, StarException> ServerWeather::activeWeather() const {
  if (m_currentWeatherIndex == NPos) return std::optional<String>();
  auto name = m_weatherPool.item(m_currentWeatherIndex);
  if (!name) return std::unexpected(std::move(name.error()));
  return name->get();
}

StringList ServerWeather::weatherList() const {
  StringList weatherList;
  for (std::size_t i = 0; i < m_weatherPool.size(); ++i)
    weatherList.push_back(m_weatherPool.items()[i].second);
  return weatherList;
}

std::expected<void, StarException> ServerWeather::setWeather(String const& weatherName, bool force) {
  std::size_t index = NPos;
  for (std::size_t i = 0; i < m_weatherPool.size(); ++i) {
    if (m_weatherPool.items()[i].second == weatherName) { index = i; break; }
  }
  return setWeatherIndex(index, force);
}

std::expected<void, StarException> ServerWeather::setWeatherIndex(std::size_t weatherIndex, bool force) {
  m_forceWeather = force;
  if (weatherIndex == NPos || weatherIndex >= m_weatherPool.size()) {
    m_currentWeatherIndex = NPos;
    m_currentWeatherType = {};
    m_currentWeatherIntensity = 0.0f;
    m_currentWind = 0.0f;
  } else {
    m_currentWeatherIndex = weatherIndex;
    auto biomes = m_root.get().biomeDatabase();
    if (!biomes) return std::unexpected(std::move(biomes.error()));
    auto name = m_weatherPool.item(m_currentWeatherIndex);
    if (!name) return std::unexpected(std::move(name.error()));
    {
      auto value = (*biomes)->weatherType(name->get());
      if (!value) return std::unexpected(std::move(value.error()));
      m_currentWeatherType = std::move(*value);
    }
    m_currentWeatherIntensity = 1.0f;
    m_currentWind = m_currentWeatherType->maximumWind * (m_root.get().random().randb() ? 1 : -1);
  }
  m_lastWeatherChangeTime = m_currentTime;
  if (m_forceWeather) m_nextWeatherChangeTime = std::numeric_limits<float>::infinity();
  else if (m_currentWeatherType) {
    auto duration = m_root.get().random().randd(m_currentWeatherType->duration[0], m_currentWeatherType->duration[1]);
    if (!duration) return std::unexpected(std::move(duration.error()));
    m_nextWeatherChangeTime = m_currentTime + *duration;
  } else m_nextWeatherChangeTime = m_currentTime;
  return setNetStates();
}

void ServerWeather::forceWeather(bool force) {
  m_forceWeather = force;
  if (force)
    m_nextWeatherChangeTime = std::numeric_limits<float>::infinity();
}

std::expected<void, StarException> ServerWeather::setNetStates() {
  auto serialized = DataStreamBuffer::serializeContainer(m_weatherPool.items());
  if (!serialized) return std::unexpected(std::move(serialized.error()));
  m_weatherPoolNetState.set(std::move(*serialized));
  m_undergroundLevelNetState.set(m_undergroundLevel);
  m_currentWeatherIndexNetState.set(m_currentWeatherIndex);
  m_currentWeatherIntensityNetState.set(m_currentWeatherIntensity);
  m_currentWindNetState.set(m_currentWind);
  return {};
}

std::expected<void, StarException> ServerWeather::spawnWeatherProjectiles(float dt) {
  if (!m_currentWeatherType || m_clientVisibleRegions.empty()) return {};
  auto projectileDatabase = m_root.get().projectileDatabase();
  if (!projectileDatabase) return std::unexpected(std::move(projectileDatabase.error()));

  // TODO: The complexity of this method is TERRIBLE, if this becomes a problem
  // for any reason there are large numbers of ways to make this much better,
  // but this was the lazy, simple-ish, and clear (hah) way.

  for (auto const& projectileConfig : m_currentWeatherType->projectiles) {
    // Gather all the tops of the client regions together with the proper
    // padding, splitting at the world wrap boundary.
    List<std::pair<Vec2I, int>> baseSpawnRegions;
    for (auto const& clientRegion : m_clientVisibleRegions) {
      Vec2I baseRegion = {clientRegion.xMin() - projectileConfig.spawnHorizontalPad, clientRegion.xMax() + projectileConfig.spawnHorizontalPad};
      int height = clientRegion.yMax();
      auto regions = m_worldGeometry.splitXRegion(baseRegion);
      if (!regions) return std::unexpected(std::move(regions.error()));
      for (auto const& region : *regions)
        baseSpawnRegions.push_back({region, height});
    }

    // We are going to have to eliminate vertically redundant sections of
    // spawning regions, so gather up every left and right edge of a spawn
    // region is a "split point"
    List<int> splitPoints;
    for (auto const& baseSpawnRegion : baseSpawnRegions) {
      splitPoints.push_back(baseSpawnRegion.first[0]);
      splitPoints.push_back(baseSpawnRegion.first[1]);
    }

    // Split every spawn region on every split point.
    List<std::pair<Vec2I, int>> splitSpawnRegions;
    for (auto const& baseSpawnRegion : baseSpawnRegions) {
      List<Vec2I> regions = {baseSpawnRegion.first};
      for (auto splitPoint : splitPoints) {
        auto prevRegions = std::exchange(regions, {});
        for (auto const& region : prevRegions) {
          if (splitPoint > region[0] && splitPoint < region[1]) {
            regions.push_back({region[0], splitPoint});
            regions.push_back({splitPoint, region[1]});
          } else {
            regions.push_back(region);
          }
        }
      }
      for (auto const& region : regions)
        splitSpawnRegions.push_back({region, baseSpawnRegion.second});
    }

    // Sort the split spawn regions by leftmost point then height, preparing to
    // remove the lower overlapping sections.
    std::sort(splitSpawnRegions.begin(), splitSpawnRegions.end(),
        [](std::pair<Vec2I, int> const& lhs, std::pair<Vec2I, int> rhs) {
          return std::tie(lhs.first[0], lhs.second) < std::tie(rhs.first[0], rhs.second);
        });

    // For each region, at this point, if the region to the right shares the
    // same starting X, because we've split up each region on each possible
    // overlapping point, then they totally overlap.  The lower region (which
    // should come before in the list) is totally redundant and should be
    // removed.
    for (auto sit = splitSpawnRegions.begin(); sit != splitSpawnRegions.end();) {
      auto right = std::next(sit);
      if (right != splitSpawnRegions.end() && sit->first[0] == right->first[0])
        sit = splitSpawnRegions.erase(sit);
      else
        ++sit;
    }

    for (auto const& spawnRegion : splitSpawnRegions) {
      RectF spawnRect = RectF(spawnRegion.first[0],
          spawnRegion.second,
          spawnRegion.first[1],
          spawnRegion.second + projectileConfig.spawnAboveRegion);

      // Figure out a good target value based on the rate per x tile, making
      // sure to handle very low count values appropriately on average.
      float count = projectileConfig.ratePerX * spawnRect.width() * dt * m_currentWeatherIntensity;
      if (m_root.get().random().randf() > fpart(count))
        count = std::floor(count);
      else
        count = std::ceil(count);

      for (int i = 0; i < count; ++i) {
        Vec2F position = {m_root.get().random().randf() * spawnRect.width() + spawnRect.xMin(), m_root.get().random().randf() * spawnRect.height() + spawnRect.yMin()};

        if (position[1] > m_undergroundLevel) {
          if (m_weatherEffectsActiveQuery) {
            auto active = m_weatherEffectsActiveQuery(Vec2I::floor(position));
            if (!active) return std::unexpected(std::move(active.error()));
            if (!*active) continue;
          }
          // Make sure not to spawn projectiles if they intersect any client
          // visible region.
          bool intersectsVisibleRegion = false;
          for (auto const& visibleRegion : m_clientVisibleRegions) {
            if (RectF(visibleRegion).contains(position)) {
              intersectsVisibleRegion = true;
              break;
            }
          }

          if (!intersectsVisibleRegion) {
            auto newProjectile = (*projectileDatabase)->createProjectile(projectileConfig.projectile, projectileConfig.parameters);
            if (!newProjectile) return std::unexpected(std::move(newProjectile.error()));
            (*newProjectile)->setInitialPosition(position);
            (*newProjectile)->setInitialVelocity(projectileConfig.velocity + Vec2F(projectileConfig.windAffectAmount * wind(), 0));
            (*newProjectile)->setTeam(EntityDamageTeam(TeamType::Environment));
            m_newProjectiles.push_back(std::move(*newProjectile));
          }
        }
      }
    }
  }
  return {};
}

ClientWeather::ClientWeather(Root& root) : m_root(root) {
  m_undergroundLevel = 0.0f;
  m_currentWeatherIndex = NPos;
  m_currentWeatherIntensity = 0.0f;
  m_currentWind = 0.0f;
  m_currentTime = 0.0;

}

std::expected<std::shared_ptr<ClientWeather>, StarException> ClientWeather::create(Root& root) {
  auto result = std::shared_ptr<ClientWeather>(new ClientWeather(root));
  {
    auto added = result->m_netGroup.addNetElement(result->m_weatherPoolNetState);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = result->m_netGroup.addNetElement(result->m_undergroundLevelNetState);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = result->m_netGroup.addNetElement(result->m_currentWeatherIndexNetState);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = result->m_netGroup.addNetElement(result->m_currentWeatherIntensityNetState);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = result->m_netGroup.addNetElement(result->m_currentWindNetState);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  return result;
}

void ClientWeather::setup(WorldGeometry worldGeometry, WeatherEffectsActiveQuery weatherEffectsActiveQuery) {
  m_worldGeometry = worldGeometry;
  m_weatherEffectsActiveQuery = std::move(weatherEffectsActiveQuery);
  m_currentTime = 0.0;
}

std::expected<void, StarException> ClientWeather::readUpdate(ByteArray data, NetCompatibilityRules rules) {
  if (!data.empty()) {
    auto read = m_netGroup.readNetState(std::move(data), 0.0f, rules);
    if (!read) return std::unexpected(std::move(read.error()));
    return getNetStates();
  }
  return {};
}

std::expected<void, StarException> ClientWeather::clear() {
  m_currentWeatherIndex = NPos;
  m_currentWeatherType.reset();
  m_currentWeatherIntensity = 0.0f;
  m_currentWind = 0.0f;
  m_visibleRegion = {};
  m_particles.clear();
  m_lastParticleVisibleRegion = {};
  return {};
}

void ClientWeather::setVisibleRegion(RectI visibleRegion) {
  m_visibleRegion = visibleRegion;
}

std::expected<void, StarException> ClientWeather::update(double dt) {
  m_currentTime += dt;
  if (m_currentWeatherIndex == NPos) m_currentWeatherType = {};
  else if (m_visibleRegion == RectI() || m_visibleRegion.yMax() > m_undergroundLevel) {
    auto biomes = m_root.get().biomeDatabase();
    if (!biomes) return std::unexpected(std::move(biomes.error()));
    auto name = m_weatherPool.item(m_currentWeatherIndex);
    if (!name) return std::unexpected(std::move(name.error()));
    {
      auto value = (*biomes)->weatherType(name->get());
      if (!value) return std::unexpected(std::move(value.error()));
      m_currentWeatherType = std::move(*value);
    }
  } else m_currentWeatherType = {};
  if (m_currentWeatherType && m_visibleRegion != RectI()) return spawnWeatherParticles(RectF(m_visibleRegion), dt);
  return {};
}

float ClientWeather::wind() const {
  return m_currentWind * m_currentWeatherIntensity;
}

float ClientWeather::weatherIntensity() const {
  return m_currentWeatherIntensity;
}

StringList ClientWeather::statusEffects() const {
  if (m_currentWeatherIntensity == 1.0 && m_currentWeatherType)
    return m_currentWeatherType->statusEffects;
  return {};
}

List<Particle> ClientWeather::pullNewParticles() { return std::exchange(m_particles, {}); }

std::expected<std::optional<String>, StarException> ClientWeather::activeWeather() const {
  if (m_currentWeatherIndex == NPos) return std::optional<String>();
  auto name = m_weatherPool.item(m_currentWeatherIndex);
  if (!name) return std::unexpected(std::move(name.error()));
  return name->get();
}

StringList ClientWeather::weatherTrackOptions() const {
  if (m_currentWeatherType)
    return m_currentWeatherType->weatherNoises;
  return {};
}

std::optional<String> ClientWeather::weatherParallax() const {
  if (m_currentWeatherType)
    return m_currentWeatherType->parallax;
  return {};
}

std::expected<void, StarException> ClientWeather::getNetStates() {
  if (m_weatherPoolNetState.pullUpdated()) {
    auto items = DataStreamBuffer::deserializeContainer<WeatherPool::ItemsList>(m_weatherPoolNetState.get());
    if (!items) return std::unexpected(std::move(items.error()));
    m_weatherPool = WeatherPool(std::move(*items));
  }
  m_undergroundLevel = m_undergroundLevelNetState.get();
  m_currentWeatherIndex = m_currentWeatherIndexNetState.get();
  m_currentWeatherIntensity = m_currentWeatherIntensityNetState.get();
  m_currentWind = m_currentWindNetState.get();
  return {};
}

std::expected<void, StarException> ClientWeather::spawnWeatherParticles(RectF newClientRegion, float dt) {
  if (!m_currentWeatherType) return {};

  for (auto const& particleConfig : m_currentWeatherType->particles) {
    // Move client region to same wrap region as newClientRegion
    RectF visibleRegion(m_worldGeometry.nearestTo(newClientRegion.min(), m_lastParticleVisibleRegion.min()),
        m_worldGeometry.nearestTo(newClientRegion.max(), m_lastParticleVisibleRegion.max()));

    Vec2F targetVelocity = particleConfig.particle.velocity + Vec2F(wind(), 0);
    float angleChange = Vec2F::angle_between_2(Vec2F(0, 1), targetVelocity);
    visibleRegion.translate(targetVelocity * dt);

    for (auto const& renderZone : newClientRegion.subtract(visibleRegion)) {
      float count = particleConfig.density * renderZone.width() * renderZone.height() * m_currentWeatherIntensity;
      if (m_root.get().random().randf() > fpart(count))
        count = std::floor(count);
      else
        count = std::ceil(count);

      for (int i = 0; i < count; ++i) {
        auto newParticle = particleConfig.particle;
        float x = m_root.get().random().randf() * renderZone.width() + renderZone.xMin();
        float y = m_root.get().random().randf() * renderZone.height() + renderZone.yMin();
        newParticle.position += m_worldGeometry.xwrap(Vec2F(x, y));
        newParticle.velocity = targetVelocity;
        if (y > m_undergroundLevel) {
          if (m_weatherEffectsActiveQuery) {
            auto active = m_weatherEffectsActiveQuery(Vec2I::floor(newParticle.position));
            if (!active) return std::unexpected(std::move(active.error()));
            if (!*active) continue;
          }
          if (particleConfig.autoRotate)
            newParticle.rotation += angleChange;
          m_particles.push_back(std::move(newParticle));
        }
      }
    }
  }

  m_lastParticleVisibleRegion = newClientRegion;
  return {};
}

}
