import star.json;
import star.json_extra;
import star.id_map;
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
import star.casting;
import star.vector;
import star.game_types;
import star.liquid_types;
import star.strong_typedef;
import star.rpc_promise;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;

import star.lua_root;
import star.animation;
import star.particle;

import star.entity_splash;
import star.damage_types;
import star.world_geometry;
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
import star.force_regions;
import star.world;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;

import star.string;
import star.math_common;
import star.liquids_database;

namespace Star {

std::expected<EntitySplashConfig, StarException> EntitySplashConfig::create(Root& root, Json const& config) {
  auto read_particle = [&](String const& key) -> std::expected<Particle, StarException> {
    auto value = config.find(key);
    if (!value) return std::unexpected(std::move(value.error()));
    if (!*value || !(**value).get().is_type(Json::Type::Object))
      return std::unexpected(JsonException::format("{} must be an object", key));
    return Particle::create(root, (**value).get());
  };
  auto speed = config.get_float("splashSpeedMin");
  if (!speed) return std::unexpected(std::move(speed.error()));
  if (!is_finite(*speed)) return std::unexpected(JsonException("splashSpeedMin must be finite"));
  auto level = config.get_float("splashMinWaterLevel");
  if (!level) return std::unexpected(std::move(level.error()));
  if (!is_finite(*level)) return std::unexpected(JsonException("splashMinWaterLevel must be finite"));
  auto bottom_json = config.get("splashBottomSensor");
  if (!bottom_json) return std::unexpected(std::move(bottom_json.error()));
  auto bottom = json_to_vec2f(*bottom_json);
  if (!bottom) return std::unexpected(std::move(bottom.error()));
  if (!is_finite((*bottom)[0]) || !is_finite((*bottom)[1]))
    return std::unexpected(JsonException("splashBottomSensor must be finite"));
  auto top_json = config.get("splashTopSensor");
  if (!top_json) return std::unexpected(std::move(top_json.error()));
  auto top = json_to_vec2f(*top_json);
  if (!top) return std::unexpected(std::move(top.error()));
  if (!is_finite((*top)[0]) || !is_finite((*top)[1]))
    return std::unexpected(JsonException("splashTopSensor must be finite"));
  auto count = config.get_int("numSplashParticles");
  if (!count) return std::unexpected(std::move(count.error()));
  if (*count < 0 || !std::in_range<int>(*count))
    return std::unexpected(JsonException("numSplashParticles must be a nonnegative int"));
  auto factor = config.get_float("splashYVelocityFactor");
  if (!factor) return std::unexpected(std::move(factor.error()));
  if (!is_finite(*factor)) return std::unexpected(JsonException("splashYVelocityFactor must be finite"));
  auto particle = read_particle("splashParticle");
  if (!particle) return std::unexpected(std::move(particle.error()));
  auto variance = read_particle("splashParticleVariance");
  if (!variance) return std::unexpected(std::move(variance.error()));
  return EntitySplashConfig{
    .splash_speed_min = *speed,
    .splash_bottom_sensor = *bottom,
    .splash_top_sensor = *top,
    .splash_min_water_level = *level,
    .num_splash_particles = static_cast<int>(*count),
    .splash_particle = std::move(*particle),
    .splash_particle_variance = std::move(*variance),
    .splash_y_velocity_factor = *factor
  };
}

std::expected<List<Particle>, StarException> EntitySplashConfig::generate_particles(Vec2F position, Vec2F velocity, World& world) const {
  List<Particle> particles;
  if (num_splash_particles <= 0) return particles;
  auto const speed = std::fabs(velocity[1]);
  if (!(speed >= splash_speed_min)) return particles;

  auto const bottom = Vec2I::floor(position + splash_bottom_sensor);
  auto const top = Vec2I::floor(position + splash_top_sensor);
  auto const bottom_liquid = world.liquidLevel(bottom);
  auto const top_liquid = bottom == top ? bottom_liquid : world.liquidLevel(top);
  if (!(bottom_liquid.level - top_liquid.level >= splash_min_water_level)) return particles;

  auto const liquid = bottom_liquid.level > 0 && bottom_liquid.liquid != EmptyLiquidId
    ? bottom_liquid.liquid : top_liquid.liquid;
  if (liquid == EmptyLiquidId) return particles;
  auto& root = world.root();
  auto database = root.liquids_database();
  if (!database) return std::unexpected(std::move(database.error()));
  auto settings = (*database)->liquid_settings(liquid);
  if (!settings)
    return std::unexpected(LiquidException::format("Unknown splash liquid {} at probes bottom ({}, {}), top ({}, {})",
      liquid, bottom[0], bottom[1], top[0], top[1]));

  auto const color = Color::rgba(settings->get().liquid_color);
  auto const y_velocity = speed * splash_y_velocity_factor;
  particles.reserve(static_cast<std::size_t>(num_splash_particles));
  for (int i = 0; i < num_splash_particles; ++i) {
    auto& particle = particles.emplace_back(splash_particle);
    particle.position = position;
    particle.velocity[1] = y_velocity;
    particle.color = color;
    particle.applyVariance(root, splash_particle_variance);
  }
  return particles;
}

}
