module;
import std;
import star.format;
import star.xx_hash;
import star.json;
import star.id_map;
import star.game_types;
import star.image;
import star.list;
import star.map;
import star.bi_map;
import star.vector;
import star.version;
import star.color;
import star.string;
import star.data_stream;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.either;
import star.random;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
import star.font;
import star.text;
import star.listener;
import star.set;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;


namespace Star {
class Assets;
class EnvironmentPainter;
class TilePainter;
class TileDrawer;
class Root;
class ImageMetadataDatabase;
}
import star.world_geometry;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.worker_pool;
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
import star.plant_database;
import star.parallax;
import star.animation;
import star.particle;
import star.weather_types;

import star.damage_types;
import star.status_types;
import star.damage;

import star.light_source;

import star.entity;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.ref_ptr;
import star.renderer;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.asset_texture_group;
import star.drawable_painter;

import star.world_camera;


namespace Star {

// Will update client rendering window internally
class WorldPainter {
public:
  // Root outlives this painter; resource providers are refreshed for each frame.
  [[nodiscard]] static std::expected<std::shared_ptr<WorldPainter>, StarException> create(Root& root);

  [[nodiscard]] std::expected<void, StarException> renderInit(std::shared_ptr<Renderer> renderer);

  void setCameraPosition(WorldGeometry const& worldGeometry, Vec2F const& position);

  WorldCamera& camera();
  // Borrowed preview provider, unavailable until renderInit creates the renderer.
  std::optional<std::reference_wrapper<TileDrawer>> tileDrawer() const;

  void update(float dt);
  [[nodiscard]] std::expected<void, StarException> render(WorldRenderData& renderData, std::function<std::expected<bool, StarException>()> lightWaiter);
  [[nodiscard]] std::expected<void, StarException> adjustLighting(WorldRenderData& renderData);

private:
  explicit WorldPainter(Root& root);
  [[nodiscard]] std::expected<void, StarException> initialize();
  [[nodiscard]] std::expected<void, StarException> renderParticles(WorldRenderData& renderData, Particle::Layer layer);
  [[nodiscard]] std::expected<void, StarException> renderBars(WorldRenderData& renderData);

  [[nodiscard]] std::expected<void, StarException> drawEntityLayer(List<Drawable> const& drawables, EntityHighlightEffect highlightEffect = EntityHighlightEffect());

  [[nodiscard]] std::expected<void, StarException> drawDrawable(Drawable const& drawable);
  [[nodiscard]] std::expected<void, StarException> drawDrawableSet(List<Drawable> const& drawable);

  Root& m_root;
  WorldCamera m_camera;

  std::shared_ptr<Renderer> m_renderer;

  std::shared_ptr<TextPainter> m_textPainter;
  std::shared_ptr<DrawablePainter> m_drawablePainter;
  std::shared_ptr<EnvironmentPainter> m_environmentPainter;
  std::shared_ptr<TilePainter> m_tilePainter;

  Json m_highlightConfig;
  Map<EntityHighlightEffectType, std::pair<Directives, Directives>> m_highlightDirectives;
  Map<EntityRenderLayer, List<std::pair<EntityHighlightEffect, std::reference_wrapper<List<Drawable> const>>>> m_entityDrawableLayers;

  Vec2F m_entityBarOffset;
  Vec2F m_entityBarSpacing;
  Vec2F m_entityBarSize;
  Vec2F m_entityBarIconOffset;

  // Updated every frame

  std::shared_ptr<Assets const> m_assets;
  std::shared_ptr<ImageMetadataDatabase const> m_imageMetadata;
  RectF m_worldScreenRect;

  Vec2F m_previousCameraCenter;
  Vec2F m_parallaxWorldPosition;

  RandomSource m_preloadRandom{0};
  float m_preloadTextureChance;
};

}

export module star.world_painter;

export namespace Star {
  using ::Star::Assets;
  using ::Star::WorldPainter;
}
