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
import star.version;

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

import star.liquids_database;

namespace Star {

EntitySplashConfig::EntitySplashConfig() = default;

std::expected<EntitySplashConfig, StarException> EntitySplashConfig::create(Root& root, Json const& config) {
  EntitySplashConfig splash;
  auto speed = config.getFloat("splashSpeedMin");
  if (!speed) return std::unexpected(std::move(speed.error()));
  splash.splashSpeedMin = *speed;
  auto level = config.getFloat("splashMinWaterLevel");
  if (!level) return std::unexpected(std::move(level.error()));
  splash.splashMinWaterLevel = *level;
  auto bottomJson = config.get("splashBottomSensor");
  if (!bottomJson) return std::unexpected(std::move(bottomJson.error()));
  auto bottom = jsonToVec2F(*bottomJson);
  if (!bottom) return std::unexpected(std::move(bottom.error()));
  splash.splashBottomSensor = *bottom;
  auto topJson = config.get("splashTopSensor");
  if (!topJson) return std::unexpected(std::move(topJson.error()));
  auto top = jsonToVec2F(*topJson);
  if (!top) return std::unexpected(std::move(top.error()));
  splash.splashTopSensor = *top;
  auto count = config.getInt("numSplashParticles");
  if (!count) return std::unexpected(std::move(count.error()));
  splash.numSplashParticles = *count;
  auto factor = config.getFloat("splashYVelocityFactor");
  if (!factor) return std::unexpected(std::move(factor.error()));
  splash.splashYVelocityFactor = *factor;
  auto particleJson = config.get("splashParticle");
  if (!particleJson) return std::unexpected(std::move(particleJson.error()));
  auto particleObject = particleJson->toObject();
  if (!particleObject) return std::unexpected(std::move(particleObject.error()));
  auto particle = Particle::create(root, *particleObject);
  if (!particle) return std::unexpected(std::move(particle.error()));
  splash.splashParticle = std::move(*particle);
  auto varianceJson = config.get("splashParticleVariance");
  if (!varianceJson) return std::unexpected(std::move(varianceJson.error()));
  auto varianceObject = varianceJson->toObject();
  if (!varianceObject) return std::unexpected(std::move(varianceObject.error()));
  auto variance = Particle::create(root, *varianceObject);
  if (!variance) return std::unexpected(std::move(variance.error()));
  splash.splashParticleVariance = std::move(*variance);
  return splash;
}

std::expected<List<Particle>, StarException> EntitySplashConfig::doSplash(Vec2F position, Vec2F velocity, World& world) const {
  List<Particle> particles;
  if (std::fabs(velocity[1]) >= splashSpeedMin) {
    auto liquidDb = world.root().liquidsDatabase();
    if (!liquidDb)
      return std::unexpected(std::move(liquidDb.error()));
    Vec2I bottom = Vec2I::floor(position + splashBottomSensor);
    Vec2I top = Vec2I::floor(position + splashTopSensor);
    if (world.liquidLevel(bottom).level - world.liquidLevel(top).level >= splashMinWaterLevel) {
      LiquidId liquidType;
      auto bottomLiquid = world.liquidLevel(bottom);
      auto topLiquid = world.liquidLevel(top);
      if (bottomLiquid.level > 0 && static_cast<int>(bottomLiquid.liquid))
        liquidType = bottomLiquid.liquid;
      else
        liquidType = topLiquid.liquid;
      Color particleColor = Color::rgba((*liquidDb)->liquidSettings(liquidType)->liquidColor);
      for (int i = 0; i < numSplashParticles; ++i) {
        Particle newSplashParticle = splashParticle;
        newSplashParticle.position = position;
        newSplashParticle.velocity[1] = std::fabs(velocity[1]) * splashYVelocityFactor;
        newSplashParticle.color = particleColor;
        newSplashParticle.applyVariance(world.root(), splashParticleVariance);
        particles.append(newSplashParticle);
      }
    }
  }
  return particles;
}

}
