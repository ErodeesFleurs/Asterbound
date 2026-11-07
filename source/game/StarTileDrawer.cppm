module;
import star.format;
import star.json;
import star.xx_hash;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.image;
import star.list;
import star.bi_map;
import star.game_types;
import star.material_types;
import star.vector;
import star.map;
import star.version;
import star.color;
import star.string;
import star.data_stream;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.either;
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
import star.world_render_data;
import star.material_render_profile;

import star.array;
namespace Star {

class Assets;
class MaterialDatabase;
class TilePainter;
class Root;

class TileDrawer {
public:
  using QuadZLevel = std::uint64_t;
  using Drawables = HashMap<QuadZLevel, List<Drawable>>;

  using MaterialRenderPieceIndex = std::size_t;
  using MaterialPieceResultList = List<std::pair<std::shared_ptr<MaterialRenderPiece const>, Vec2F>>;

  enum class TerrainLayer { Background, Midground, Foreground };

  static RenderTile const DefaultRenderTile;

  // Root must outlive this drawer. Runtime consumers borrow this instance
  // explicitly; absent renderers pass nullptr rather than constructing a drawer.
  [[nodiscard]] static std::expected<std::shared_ptr<TileDrawer>, StarException> create(Root& root);

  [[nodiscard]] std::expected<bool, StarException> produceTerrainDrawables(Drawables& drawables, TerrainLayer terrainLayer, Vec2I const& pos,
    WorldRenderData const& renderData, float scale = 1.0f, Vec2I variantOffset = {}, std::optional<TerrainLayer> variantLayer = {});

  WorldRenderData& renderData();
  std::unique_lock<std::mutex> lockRenderData();

  template <typename Function>
  [[nodiscard]] static std::expected<void, StarException> forEachRenderTile(WorldRenderData const& renderData, RectI const& worldCoordRange, Function&& function);
protected:
  explicit TileDrawer(Root& root) : m_root(root) {}
  [[nodiscard]] std::expected<void, StarException> initializeTileDrawer();
private:
  friend class TilePainter;

  static RenderTile const& getRenderTile(WorldRenderData const& renderData, Vec2I const& worldPos);

  static QuadZLevel materialZLevel(std::uint32_t zLevel, MaterialId material, MaterialHue hue, MaterialColorVariant colorVariant);
  static QuadZLevel modZLevel(std::uint32_t zLevel, ModId mod, MaterialHue hue, MaterialColorVariant colorVariant);
  static QuadZLevel damageZLevel();

  [[nodiscard]] static std::expected<bool, StarException> determineMatchingPieces(MaterialPieceResultList& resultList, std::optional<std::reference_wrapper<bool>> occlude, std::shared_ptr<MaterialDatabase const> const& materialDb, MaterialRenderMatchList const& matchList,
    WorldRenderData const& renderData, Vec2I const& basePos, TileLayer layer, bool isMod);

  Root& m_root;
  Vec4B m_backgroundLayerColor;
  Vec4B m_foregroundLayerColor;
  Vec2F m_liquidDrawLevels;

  WorldRenderData m_tempRenderData;
  std::mutex m_tempRenderDataMutex;
};

template <typename Function>
[[nodiscard]] std::expected<void, StarException> TileDrawer::forEachRenderTile(WorldRenderData const& renderData, RectI const& worldCoordRange, Function&& function) {
  RectI indexRect = RectI::withSize(Vec2I(renderData.geometry.pdiff(worldCoordRange.min()[0], renderData.tileMinPosition[0]), worldCoordRange.min()[1] - renderData.tileMinPosition[1]), worldCoordRange.size());
  indexRect.limit(RectI::withSize(Vec2I(0, 0), Vec2I(renderData.tiles.size())));
  if (!indexRect.isEmpty()) {
    for (int x = indexRect.xMin(); x < indexRect.xMax(); ++x) {
      for (int y = indexRect.yMin(); y < indexRect.yMax(); ++y) {
        Array2S index(x, y);
        auto applied = function(worldCoordRange.min() + (Vec2I(index) - indexRect.min()), renderData.tiles(index));
        if (!applied)
          return std::unexpected(std::move(applied.error()));
      }
    }
  }
  return {};
}

}

export module star.tile_drawer;

export namespace Star {
  using ::Star::Assets;
  using ::Star::MaterialDatabase;
  using ::Star::TilePainter;
  using ::Star::TileDrawer;
}
