module;
import star.xx_hash;
import star.json;
import star.id_map;
import star.image;
import star.list;
import star.bi_map;
import star.game_types;
import star.vector;
import star.map;
import star.version;
import star.color;
import star.string;
import star.data_stream;
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
import std;
import star.set;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;


import star.collision_block;
import star.liquid_types;
import star.tile_damage;
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
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.cellular_light_array;
import star.cellular_lighting;

namespace Star {

struct EntityDrawables {
  EntityHighlightEffect highlightEffect;
  Map<EntityRenderLayer, List<Drawable>> layers;
};


struct WorldRenderData {
  // Reset transient frame data while retaining entity and particle sample slots.
  // WorldClient refills/truncates those slots before publishing the frame.
  void prepareFrame();

  WorldGeometry geometry;

  Vec2I tileMinPosition;
  RenderTileArray tiles;
  Vec2I lightMinPosition;
  Lightmap lightMap;

  List<EntityDrawables> entityDrawables;
  List<Particle> particles;

  List<OverheadBar> overheadBars;
  List<Drawable> nametags;

  List<Drawable> backgroundOverlays;
  List<Drawable> foregroundOverlays;

  List<ParallaxLayer> parallaxLayers;

  SkyRenderData skyRenderData;

  bool isFullbright = false;
  float dimLevel = 0.0f;
  Vec3B dimColor;
};

inline void WorldRenderData::prepareFrame() {
  tiles.resize({0, 0}); // keep reserved

  for (auto& entity : entityDrawables) {
    entity.highlightEffect = {};
    for (auto& layer : entity.layers)
      layer.second.clear();
  }
  // ParticleManager overwrites numeric presentation state in retained slots;
  // destroying them here would discard cached immutable payload every frame.
  overheadBars.clear();
  nametags.clear();
  backgroundOverlays.clear();
  foregroundOverlays.clear();
  parallaxLayers.clear();
}

}

export module star.world_render_data;

export namespace Star {
  using ::Star::EntityDrawables;
  using ::Star::WorldRenderData;
}
