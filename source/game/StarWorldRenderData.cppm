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
import star.color;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
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

// Keep last frame's keys available for the whole submission: a new vehicle
// must not steal a still-visible layer's list just because it renders first.
// Reuse nodes absent for a full frame, expiring any unused spares next reset.
template <std::movable T>
class FrameLayers {
public:
  void prepare_frame() {
    m_spares.clear();
    for (auto it = m_layers.begin(); it != m_layers.end();) {
      if (it->second.empty()) {
        m_spares.insert(m_spares.end(), m_layers.extract(it++));
      } else {
        it->second.clear();
        ++it;
      }
    }
  }

  void append(EntityRenderLayer key, T&& value) {
    auto hint = m_layers.lower_bound(key);
    if (hint == m_layers.end() || hint->first != key) {
      if (m_spares.empty()) {
        hint = m_layers.try_emplace(hint, key);
      } else {
        auto node = m_spares.extract(m_spares.begin());
        node.key() = key;
        hint = m_layers.insert(hint, std::move(node));
      }
    }
    hint->second.push_back(std::move(value));
  }

  auto begin() { return m_layers.begin(); }
  auto end() { return m_layers.end(); }
  auto begin() const { return m_layers.begin(); }
  auto end() const { return m_layers.end(); }

private:
  std::map<EntityRenderLayer, List<T>> m_layers;
  std::map<EntityRenderLayer, List<T>> m_spares;
};

struct EntityDrawables {
  EntityHighlightEffect highlight_effect;
  FrameLayers<Drawable> layers;
};

struct EntityDrawableGroup {
  EntityHighlightEffect highlight_effect;
  std::reference_wrapper<List<Drawable> const> drawables;
};

inline void route_entity_drawables(FrameLayers<EntityDrawableGroup>& routes, List<EntityDrawables> const& entities) {
  routes.prepare_frame();
  for (auto const& entity : entities)
    for (auto const& [key, drawables] : entity.layers)
      if (!drawables.empty())
        routes.append(key, {entity.highlight_effect, std::cref(drawables)});
}

struct WorldRenderData {
  // Reset transient frame data while retaining entity and particle sample slots.
  // WorldClient refills/truncates those slots before publishing the frame.
  void prepare_frame();

  WorldGeometry geometry;

  Vec2I tile_min_position;
  RenderTileArray tiles;
  Vec2I light_min_position;
  Lightmap light_map;

  List<EntityDrawables> entity_drawables;
  List<Particle> particles;

  List<OverheadBar> overhead_bars;
  List<Drawable> nametags;

  List<Drawable> background_overlays;
  List<Drawable> foreground_overlays;

  List<ParallaxLayer> parallax_layers;

  SkyRenderData sky_render_data;

  bool is_fullbright = false;
  float dim_level = 0.0f;
  Vec3B dim_color;
};

inline void WorldRenderData::prepare_frame() {
  tiles.clear(); // Retain storage for the next frame.

  for (auto& entity : entity_drawables) {
    entity.highlight_effect = {};
    entity.layers.prepare_frame();
  }
  // ParticleManager overwrites numeric presentation state in retained slots;
  // destroying them here would discard cached immutable payload every frame.
  overhead_bars.clear();
  nametags.clear();
  background_overlays.clear();
  foreground_overlays.clear();
  parallax_layers.clear();
}

}

export module star.world_render_data;

export namespace Star {
  using ::Star::FrameLayers;
  using ::Star::EntityDrawableGroup;
  using ::Star::route_entity_drawables;
  using ::Star::EntityDrawables;
  using ::Star::WorldRenderData;
}
