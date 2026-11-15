module;
import std;
import star.format;
import star.json;
import star.xx_hash;
import star.id_map;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.vector;
import star.bi_map;
import star.image;
import star.game_types;
import star.version;
import star.string;
import star.asset_path;
import star.either;
import star.random;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
import star.listener;
import star.perlin;
import star.random_point;
import star.set;
import star.worker_pool;

import star.sector_array_2d;


import star.tile_damage;
import star.plant_database;
import star.parallax;
import star.collision_block;
import star.liquid_types;
import star.tile_sector_array;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.drawable;
import star.entity_rendering_types;
import star.sky_types;
import star.celestial_coordinate;
import star.sky_parameters;
import star.sky_render_data;
import star.animation;
import star.particle;
import star.weather_types;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;

import star.light_source;

import star.entity;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.ref_ptr;
import star.renderer;
import star.asset_texture_group;

namespace Star {

class WorldCamera;
class Root;

class EnvironmentPainter {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<EnvironmentPainter>, StarException> create(Root& root, std::shared_ptr<Renderer> renderer);

  void update(float dt);

  [[nodiscard]] std::expected<void, StarException> renderStars(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky);
  [[nodiscard]] std::expected<void, StarException> renderDebrisFields(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky);
  [[nodiscard]] std::expected<void, StarException> renderBackOrbiters(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky);
  [[nodiscard]] std::expected<void, StarException> renderPlanetHorizon(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky);
  [[nodiscard]] std::expected<void, StarException> renderFrontOrbiters(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky);
  [[nodiscard]] std::expected<void, StarException> renderSky(Vec2F const& screenSize, SkyRenderData const& sky);

  [[nodiscard]] std::expected<void, StarException> renderParallaxLayers(Vec2F parallaxWorldPosition, WorldCamera const& camera, ParallaxLayers const& layers, SkyRenderData const& sky);

  void cleanup(std::int64_t textureTimeout);

private:
  EnvironmentPainter(Root& root, std::shared_ptr<Renderer> renderer);
  static float const SunriseTime;
  static float const SunsetTime;
  static float const SunFadeRate;
  static float const MaxFade;
  static float const RayPerlinFrequency;
  static float const RayPerlinAmplitude;
  static int const RayCount;
  static float const RayMinWidth;
  static float const RayWidthVariance;
  static float const RayAngleVariance;
  static float const SunRadius;
  static float const RayColorDependenceLevel;
  static float const RayColorDependenceScale;
  static float const RayUnscaledAlphaVariance;
  static float const RayMinUnscaledAlpha;
  static Vec3B const RayColor;

  struct RayStyle {
    Vec3B color;
    float intensity;
    float sun_scale;
  };

  [[nodiscard]] std::expected<void, StarException> draw_rays(float pixelRatio, SkyRenderData const& sky, Vec2F start, float length, double time, float alpha);
  [[nodiscard]] std::expected<void, StarException> draw_ray(float pixelRatio,
      Vec2F start,
      float width,
      float length,
      float angle,
      double time,
      RayStyle const& style,
      float alpha);
  [[nodiscard]] std::expected<void, StarException> drawOrbiter(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky, SkyOrbiter const& orbiter);

  std::uint64_t starsHash(SkyRenderData const& sky, Vec2F const& viewSize) const;
  [[nodiscard]] std::expected<void, StarException> setupStars(SkyRenderData const& sky);

  Root& m_root;
  std::shared_ptr<Renderer> m_renderer;
  std::shared_ptr<AssetTextureGroup> m_textureGroup;

  double m_timer;
  PerlinF m_rayPerlin;

  std::uint64_t m_starsHash{};
  List<TexturePtr> m_starTextures;
  std::shared_ptr<Random2dPointGenerator<std::pair<std::size_t, float>>> m_starGenerator;
  List<std::shared_ptr<Random2dPointGenerator<std::pair<String, float>, double>>> m_debrisGenerators;
};

}

export module star.environment_painter;

export namespace Star {
  using ::Star::WorldCamera;
  using ::Star::EnvironmentPainter;
}
