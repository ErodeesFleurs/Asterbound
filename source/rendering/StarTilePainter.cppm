module;
import std;
import star.format;
import star.xx_hash;
import star.byte_array;
import star.json;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.image;
import star.list;
import star.map;
import star.bi_map;
import star.game_types;
import star.material_types;
import star.vector;
import star.version;
import star.color;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.either;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
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
import star.ref_ptr;
import star.renderer;

import star.tile_drawer;

namespace Star {

class WorldCamera;

class Assets;
class MaterialDatabase;
class Root;

class TilePainter : public TileDrawer {
public:
  // The rendered tiles are split and cached in chunks of RenderChunkSize x
  // RenderChunkSize.  This means that, around the border, there may be as many
  // as RenderChunkSize - 1 tiles rendered outside of the viewing area from
  // chunk alignment.  In addition to this, there is also a region around each
  // tile that is used for neighbor based rendering rules which has a max of
  // MaterialRenderProfileMaxNeighborDistance.  If the given tile data does not
  // extend RenderChunkSize + MaterialRenderProfileMaxNeighborDistance - 1
  // around the viewing area, then border chunks can continuously change hash,
  // and will be recomputed too often.
  static unsigned const RenderChunkSize = 16;
  static unsigned const BorderTileSize = RenderChunkSize + MaterialRenderProfileMaxNeighborDistance - 1;

  [[nodiscard]] static std::expected<std::shared_ptr<TilePainter>, StarException> create(Root& root, std::shared_ptr<Renderer> renderer);

  // Adjusts lighting levels for liquids.
  [[nodiscard]] std::expected<void, StarException> adjustLighting(WorldRenderData& renderData) const;

  // Sets up chunk data for every chunk that intersects the rendering region
  // and prepares it for rendering.  Do not call cleanup in between calling
  // setup and each render method.
  [[nodiscard]] std::expected<void, StarException> setup(WorldCamera const& camera, WorldRenderData& renderData);

  [[nodiscard]] std::expected<void, StarException> renderBackground(WorldCamera const& camera);
  [[nodiscard]] std::expected<void, StarException> renderMidground(WorldCamera const& camera);
  [[nodiscard]] std::expected<void, StarException> renderLiquid(WorldCamera const& camera);
  [[nodiscard]] std::expected<void, StarException> renderForeground(WorldCamera const& camera);

  // Clears any render data, as well as cleaning up old cached textures and
  // chunks.
  void cleanup();

private:
  friend struct TilePainterPerformanceAccess;

  TilePainter(Root& root, std::shared_ptr<Renderer> renderer);

  typedef std::uint64_t QuadZLevel;
  typedef std::uint64_t ChunkHash;

  enum class TerrainLayer { Background, Midground, Foreground };

  struct LiquidInfo {
    TexturePtr texture;
    Vec4B color;
    Vec3F bottomLightMix;
    float textureMovementFactor;
  };

  typedef HashMap<TerrainLayer, HashMap<QuadZLevel, std::shared_ptr<RenderBuffer>>> TerrainChunk;
  typedef HashMap<LiquidId, std::shared_ptr<RenderBuffer>> LiquidChunk;

  typedef std::tuple<MaterialId, MaterialRenderPieceIndex, MaterialHue, bool> MaterialPieceTextureKey;
  typedef String AssetTextureKey;
  typedef std::variant<MaterialPieceTextureKey, AssetTextureKey> TextureKey;

  struct TextureKeyHash {
    std::size_t operator()(TextureKey const& key) const;
  };

  // chunkIndex here is the index of the render chunk such that chunkIndex *
  // RenderChunkSize results in the coordinate of the lower left most tile in
  // the render chunk.

  [[nodiscard]] std::expected<std::pair<ChunkHash, ChunkHash>, StarException>
  chunkHashes(WorldRenderData const& renderData, Vec2I chunkIndex);

  [[nodiscard]] std::expected<void, StarException> renderTerrainChunks(WorldCamera const& camera, TerrainLayer terrainLayer);

  [[nodiscard]] std::expected<std::shared_ptr<TerrainChunk const>, StarException>
  getTerrainChunk(WorldRenderData& renderData, Vec2I chunkIndex, ChunkHash hash);
  [[nodiscard]] std::expected<std::shared_ptr<LiquidChunk const>, StarException>
  getLiquidChunk(WorldRenderData& renderData, Vec2I chunkIndex, ChunkHash hash);

  [[nodiscard]] std::expected<bool, StarException> produceTerrainPrimitives(HashMap<QuadZLevel, List<RenderPrimitive>>& primitives,
      MaterialPieceResultList& pieces, TerrainLayer terrainLayer, Vec2I const& pos, WorldRenderData const& renderData,
      std::shared_ptr<Assets const> const& assets, std::shared_ptr<MaterialDatabase const> const& materialDatabase);
  [[nodiscard]] std::expected<void, StarException> produceLiquidPrimitives(HashMap<LiquidId, List<RenderPrimitive>>& primitives, Vec2I const& pos, WorldRenderData const& renderData);
  [[nodiscard]] std::expected<TexturePtr, StarException> setupTexture(std::shared_ptr<Image const> const& image);

  float liquidDrawLevel(float liquidLevel) const;

  ByteArray m_terrainHashBuffer;
  ByteArray m_liquidHashBuffer;

  List<LiquidInfo> m_liquids;

  std::shared_ptr<Renderer> m_renderer;
  std::shared_ptr<TextureGroup> m_textureGroup;

  HashTtlCache<TextureKey, TexturePtr, TextureKeyHash> m_textureCache;
  HashTtlCache<std::pair<Vec2I, ChunkHash>, std::shared_ptr<TerrainChunk const>> m_terrainChunkCache;
  HashTtlCache<std::pair<Vec2I, ChunkHash>, std::shared_ptr<LiquidChunk const>> m_liquidChunkCache;

  List<std::shared_ptr<TerrainChunk const>> m_pendingTerrainChunks;
  List<std::shared_ptr<LiquidChunk const>> m_pendingLiquidChunks;
  List<std::shared_ptr<TerrainChunk const>> m_setupTerrainChunks;
  List<std::shared_ptr<LiquidChunk const>> m_setupLiquidChunks;
  // Only deduplicates uploads within setup; never retains an extra frame owner.
  HashMap<std::shared_ptr<Image const>, TexturePtr> m_setupTextures;

  std::optional<Vec2F> m_lastCameraCenter;
  Vec2F m_cameraPan;
};

}

export module star.tile_painter;

export namespace Star {
  using ::Star::WorldCamera;
  using ::Star::Assets;
  using ::Star::MaterialDatabase;
  using ::Star::TilePainter;
}
