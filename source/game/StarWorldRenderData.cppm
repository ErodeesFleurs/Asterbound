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

struct ParallaxLayerSample {
  std::reference_wrapper<ParallaxLayer const> layer;
  float alpha;
  Vec2F parallax_offset;
  Vec2F speed;
  std::uint8_t source_index;
  std::size_t submission_order;
};

class ParallaxRenderLayers {
public:
  // Each source owns immutable layer data. Only presentation values are
  // refreshed per frame; ordering is rebuilt when the source tuple changes.
  std::span<ParallaxLayerSample> sample_sources(std::shared_ptr<Parallax const> const& current,
      std::shared_ptr<Parallax const> const& next, std::shared_ptr<Parallax const> const& weather) {
    if (m_sources[0] != current || m_sources[1] != next || m_sources[2] != weather) {
      m_samples.clear();
      m_sources = {current, next, weather};
      std::size_t layer_count = 0;
      for (auto const& source : m_sources)
        if (source)
          layer_count += source->layers().size();
      m_samples.reserve(layer_count);
      for (std::uint8_t source_index = 0; source_index < m_sources.size(); ++source_index) {
        if (auto const& source = m_sources[source_index]) {
          for (auto const& layer : source->layers())
            m_samples.push_back({std::cref(layer), layer.alpha, layer.parallax_offset, layer.speed, source_index, m_samples.size()});
        }
      }
      // Break equal display keys with submission order so an in-place sort
      // preserves current, next, weather and each source's internal order.
      std::ranges::sort(m_samples, [](ParallaxLayerSample const& a, ParallaxLayerSample const& b) {
        auto key = [](ParallaxLayer const& layer) { return std::tie(layer.z_level, layer.vertical_origin); };
        auto const a_key = key(a.layer.get());
        auto const b_key = key(b.layer.get());
        return a_key == b_key ? a.submission_order < b.submission_order : a_key > b_key;
      });
    }
    m_active = true;
    return m_samples;
  }

  std::span<ParallaxLayerSample const> samples() const {
    return m_active ? std::span<ParallaxLayerSample const>(m_samples) : std::span<ParallaxLayerSample const>();
  }

  void prepare_frame() { m_active = false; }

  void clear() {
    m_samples.clear();
    m_sources = {};
    m_active = false;
  }

private:
  std::array<std::shared_ptr<Parallax const>, 3> m_sources;
  List<ParallaxLayerSample> m_samples;
  bool m_active = false;
};

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

  ParallaxRenderLayers parallax_layers;

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
  parallax_layers.prepare_frame();
}

}

export module star.world_render_data;

export namespace Star {
  using ::Star::FrameLayers;
  using ::Star::EntityDrawableGroup;
  using ::Star::route_entity_drawables;
  using ::Star::EntityDrawables;
  using ::Star::ParallaxLayerSample;
  using ::Star::ParallaxRenderLayers;
  using ::Star::WorldRenderData;
}
