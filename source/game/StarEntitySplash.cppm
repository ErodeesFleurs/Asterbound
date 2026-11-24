module;
import std;
import star.json;
import star.format;

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
import star.animation;
import star.particle;

import star.vector;
namespace Star {

class World;
class Root;

struct EntitySplashConfig {
  [[nodiscard]] static std::expected<EntitySplashConfig, StarException> create(Root& root, Json const& config);
  float splash_speed_min = 0;
  Vec2F splash_bottom_sensor{};
  Vec2F splash_top_sensor{};
  float splash_min_water_level = 0;
  int num_splash_particles = 0;
  Particle splash_particle;
  Particle splash_particle_variance;
  float splash_y_velocity_factor = 0;

  [[nodiscard]] std::expected<List<Particle>, StarException> generate_particles(Vec2F position, Vec2F velocity, World& world) const;
};

}

export module star.entity_splash;

export namespace Star {
  using ::Star::World;
  using ::Star::EntitySplashConfig;
}
