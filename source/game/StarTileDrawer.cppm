module;
import std;
import star.array;
import star.drawable;
import star.format;
import star.game_types;
import star.list;
import star.map;
import star.material_render_profile;
import star.material_types;
import star.rect;
import star.vector;
import star.world_geometry;
import star.world_render_data;
import star.world_tiles;

namespace Star {

class MaterialDatabase;
class TilePainter;
class Root;

class TileDrawer {
public:
  using QuadZLevel = std::uint64_t;
  using Drawables = HashMap<QuadZLevel, List<Drawable>>;

  using MaterialRenderPieceIndex = std::size_t;
  // Pieces borrow the profile that owns them.
  using MaterialPieceResultList = List<std::pair<std::reference_wrapper<MaterialRenderPiece const>, Vec2F>>;

  enum class TerrainLayer { Background, Midground, Foreground };

  static RenderTile const default_render_tile;

  // Root must outlive this drawer. Runtime consumers borrow this instance
  // explicitly; an absent renderer does not require constructing a drawer.
  [[nodiscard]] static std::expected<std::shared_ptr<TileDrawer>, StarException> create(Root& root);

  [[nodiscard]] std::expected<bool, StarException> produce_terrain_drawables(Drawables& drawables, TerrainLayer terrain_layer, Vec2I const& pos,
      WorldRenderData const& render_data, float scale = 1.0f, Vec2I variant_offset = {}, std::optional<TerrainLayer> variant_layer = {});

  WorldRenderData& render_data();
  std::unique_lock<std::mutex> lock_render_data();

  // Appends in draw order and returns whether any match succeeded. Keep the
  // profile alive and unchanged while using the resulting piece references.
  [[nodiscard]] static std::expected<bool, StarException> determine_matching_pieces(
      MaterialPieceResultList& result_list, std::optional<std::reference_wrapper<bool>> occlude, MaterialDatabase const& material_database,
      MaterialRenderProfile const& profile, WorldRenderData const& render_data, Vec2I const& base_pos, TileLayer layer, bool is_mod);

  template <typename Function>
    requires std::same_as<std::invoke_result_t<Function&, Vec2I, RenderTile const&>, std::expected<void, StarException>>
  [[nodiscard]] static std::expected<void, StarException> for_each_render_tile(WorldRenderData const& render_data, RectI const& world_coord_range,
      Function&& function);
protected:
  explicit TileDrawer(Root& root) : m_root(root) {}
  [[nodiscard]] std::expected<void, StarException> initialize_tile_drawer();
private:
  friend class TilePainter;

  struct MatchEvaluator;

  static RenderTile const& get_render_tile(WorldRenderData const& render_data, Vec2I const& world_pos);

  static QuadZLevel material_z_level(std::uint32_t z_level, MaterialId material, MaterialHue hue, MaterialColorVariant color_variant);
  static QuadZLevel mod_z_level(std::uint32_t z_level, ModId mod, MaterialHue hue, MaterialColorVariant color_variant);
  static QuadZLevel damage_z_level();

  Root& m_root;
  Vec4B m_background_layer_color;
  Vec4B m_foreground_layer_color;
  Vec2F m_liquid_draw_levels;

  WorldRenderData m_temp_render_data;
  std::mutex m_temp_render_data_mutex;
};

template <typename Function>
  requires std::same_as<std::invoke_result_t<Function&, Vec2I, RenderTile const&>, std::expected<void, StarException>>
std::expected<void, StarException> TileDrawer::for_each_render_tile(WorldRenderData const& render_data, RectI const& world_coord_range,
    Function&& function) {
  auto const& range_min = world_coord_range.min();
  auto const& tile_min = render_data.tile_min_position;
  auto index_rect = RectI::with_size(Vec2I(render_data.geometry.pdiff(range_min[0], tile_min[0]), range_min[1] - tile_min[1]), world_coord_range.size());
  index_rect.limit(RectI::with_size(Vec2I(0, 0), Vec2I(render_data.tiles.extents())));
  if (!index_rect.is_empty()) {
    auto const& clipped_min = index_rect.min();
    for (int x = index_rect.x_min(); x < index_rect.x_max(); ++x) {
      for (int y = index_rect.y_min(); y < index_rect.y_max(); ++y) {
        auto applied = std::invoke(function, range_min + (Vec2I(x, y) - clipped_min), render_data.tiles(Array2S(x, y)));
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
  using ::Star::MaterialDatabase;
  using ::Star::TilePainter;
  using ::Star::TileDrawer;
}
