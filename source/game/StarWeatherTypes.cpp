import star.json;
import star.json_extra;
import star.random;
import star.weighted_pool;
import star.color;
import star.bi_map;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.data_stream_extra;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.string;
import star.version;

import star.animation;
import star.particle;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;
import star.weather_types;


import star.biome_database;

namespace Star {

WeatherType::WeatherType() {
  maximumWind = 0;
}

std::expected<WeatherType, StarException> WeatherType::create(Root& root, Json config, String path) {
  WeatherType result;
  if (config.isType(Json::Type::String)) {
    auto reference = config.toString();
    if (!reference) return std::unexpected(std::move(reference.error()));
    path = std::move(*reference);
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto loaded = (*assets)->json(path);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    config = std::move(*loaded);
  }
  auto name = config.getString("name");
  if (!name) return std::unexpected(std::move(name.error()));
  result.name = std::move(*name);
  auto particles = config.getArray("particles", JsonArray());
  if (!particles) return std::unexpected(std::move(particles.error()));
  for (auto const& v : *particles) {
    ParticleConfig pc;
    auto particleConfig = v.get("particle");
    if (!particleConfig) return std::unexpected(std::move(particleConfig.error()));
    auto particle = Particle::create(root, *particleConfig, path);
    if (!particle) return std::unexpected(std::move(particle.error()));
    pc.particle = std::move(*particle);
    auto density = v.getFloat("density");
    if (!density) return std::unexpected(std::move(density.error()));
    pc.density = *density;
    auto autoRotate = v.getBool("autoRotate", false);
    if (!autoRotate) return std::unexpected(std::move(autoRotate.error()));
    pc.autoRotate = *autoRotate;
    result.particles.push_back(std::move(pc));
  }
  auto projectiles = config.getArray("projectiles", JsonArray());
  if (!projectiles) return std::unexpected(std::move(projectiles.error()));
  for (auto const& v : *projectiles) {
    ProjectileConfig pc;
    auto projectile = v.getString("projectile");
    if (!projectile) return std::unexpected(std::move(projectile.error()));
    pc.projectile = std::move(*projectile);
    auto parameters = v.get("parameters", {});
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    pc.parameters = std::move(*parameters);
    auto velocityConfig = v.get("velocity");
    if (!velocityConfig) return std::unexpected(std::move(velocityConfig.error()));
    auto velocity = jsonToVec2F(*velocityConfig);
    if (!velocity) return std::unexpected(std::move(velocity.error()));
    pc.velocity = *velocity;
    auto rate = v.getFloat("ratePerX");
    if (!rate) return std::unexpected(std::move(rate.error()));
    pc.ratePerX = *rate;
    auto above = v.getInt("spawnAboveRegion");
    if (!above) return std::unexpected(std::move(above.error()));
    pc.spawnAboveRegion = *above;
    auto pad = v.getInt("spawnHorizontalPad");
    if (!pad) return std::unexpected(std::move(pad.error()));
    pc.spawnHorizontalPad = *pad;
    auto wind = v.getFloat("windAffectAmount", 0.0f);
    if (!wind) return std::unexpected(std::move(wind.error()));
    pc.windAffectAmount = *wind;
    result.projectiles.push_back(std::move(pc));
  }
  auto maximumWind = config.getFloat("maximumWind", 0.0f);
  if (!maximumWind) return std::unexpected(std::move(maximumWind.error()));
  result.maximumWind = *maximumWind;
  auto durationConfig = config.get("duration");
  if (!durationConfig) return std::unexpected(std::move(durationConfig.error()));
  auto duration = jsonToVec2F(*durationConfig);
  if (!duration) return std::unexpected(std::move(duration.error()));
  result.duration = *duration;
  auto noisesConfig = config.get("weatherNoises", JsonArray());
  if (!noisesConfig) return std::unexpected(std::move(noisesConfig.error()));
  auto noises = jsonToStringList(*noisesConfig);
  if (!noises) return std::unexpected(std::move(noises.error()));
  result.weatherNoises = std::move(*noises);
  auto effectsConfig = config.get("statusEffects", JsonArray());
  if (!effectsConfig) return std::unexpected(std::move(effectsConfig.error()));
  auto effects = jsonToStringList(*effectsConfig);
  if (!effects) return std::unexpected(std::move(effects.error()));
  result.statusEffects = std::move(*effects);
  auto parallax = config.optString("parallax");
  if (!parallax) return std::unexpected(std::move(parallax.error()));
  if (*parallax && !(*parallax)->empty()) {
    auto absolute = (*parallax)->starts_with("/");
    if (!absolute) return std::unexpected(std::move(absolute.error()));
    result.parallax = *absolute ? **parallax : "/parallax/weather/" + **parallax + ".parallax";
  }
  return result;
}

std::expected<Json, StarException> WeatherType::toJson() const {
  JsonArray particleConfigs;
  for (auto const& pc : particles) {
    auto particle = pc.particle.toJson();
    if (!particle) return std::unexpected(std::move(particle.error()));
    particleConfigs.push_back(
      JsonObject{{"particle", std::move(*particle)}, {"density", pc.density}, {"autoRotate", pc.autoRotate}});
  }
  JsonArray projectileConfigs;
  for (auto const& pc : projectiles)
    projectileConfigs.push_back(JsonObject{{"projectile", pc.projectile},
                                           {"parameters", pc.parameters},
                                           {"velocity", jsonFromVec2F(pc.velocity)},
                                           {"ratePerX", pc.ratePerX},
                                           {"spawnAboveRegion", pc.spawnAboveRegion},
                                           {"spawnHorizontalPad", pc.spawnHorizontalPad},
                                           {"windAffectAmount", pc.windAffectAmount}});
  return JsonObject{{"name", name}, {"particles", std::move(particleConfigs)}, {"projectiles", std::move(projectileConfigs)},
      {"maximumWind", maximumWind}, {"duration", jsonFromVec2F(duration)}, {"weatherNoises", jsonFromStringList(weatherNoises)},
      {"statusEffects", jsonFromStringList(statusEffects)}, {"parallax", parallax ? Json(*parallax) : Json()}};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WeatherType& weatherType) {
  auto name = ds.read(weatherType.name);
  if (!name) return std::unexpected(std::move(name.error()));
  auto particles =
    ds.read_container(weatherType.particles,
                      [](DataStream& ds, WeatherType::ParticleConfig& config) -> std::expected<void, StarException> {
                        auto result = ds >> config.particle >> config.density >> config.autoRotate;
                        if (!result)
                          return std::unexpected(std::move(result.error()));
                        return {};
                      });
  if (!particles) return std::unexpected(std::move(particles.error()));
  auto projectiles =
    ds.read_container(weatherType.projectiles,
                      [](DataStream& ds, WeatherType::ProjectileConfig& config) -> std::expected<void, StarException> {
                        auto result = ds >> config.projectile >> config.parameters >> config.velocity >> config.ratePerX
                          >> config.spawnAboveRegion >> config.spawnHorizontalPad >> config.windAffectAmount;
                        if (!result)
                          return std::unexpected(std::move(result.error()));
                        return {};
                      });
  if (!projectiles) return std::unexpected(std::move(projectiles.error()));
  auto result = ds >> weatherType.maximumWind >> weatherType.duration >> weatherType.weatherNoises >> weatherType.statusEffects;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WeatherType const& weatherType) {
  auto name = ds.write(weatherType.name);
  if (!name) return std::unexpected(std::move(name.error()));
  auto particles = ds.write_container(
    weatherType.particles,
    [](DataStream& ds, WeatherType::ParticleConfig const& config) -> std::expected<void, StarException> {
      auto result = ds << config.particle << config.density << config.autoRotate;
      if (!result)
        return std::unexpected(std::move(result.error()));
      return {};
    });
  if (!particles) return std::unexpected(std::move(particles.error()));
  auto projectiles = ds.write_container(
    weatherType.projectiles,
    [](DataStream& ds, WeatherType::ProjectileConfig const& config) -> std::expected<void, StarException> {
      auto result = ds << config.projectile << config.parameters << config.velocity << config.ratePerX
                       << config.spawnAboveRegion << config.spawnHorizontalPad << config.windAffectAmount;
      if (!result)
        return std::unexpected(std::move(result.error()));
      return {};
    });
  if (!projectiles) return std::unexpected(std::move(projectiles.error()));
  auto result = ds << weatherType.maximumWind << weatherType.duration << weatherType.weatherNoises << weatherType.statusEffects;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

}
