module;
import std;
import star.format;
import star.list;
import star.multi_array;
import star.rect;
import star.sector_array_2d;
import star.vector;
import star.worker_pool;

namespace Star {

template <typename Function, typename Argument, typename Result = void>
concept tile_array_callback = requires(Function& function, Vec2I const& position, Argument&& argument) {
  { std::invoke(function, position, std::forward<Argument>(argument)) } -> std::same_as<std::expected<Result, StarException>>;
};

template <typename Target, typename Function, typename Tile>
concept tile_array_writer = std::is_copy_constructible_v<std::decay_t<Function>>
    && std::is_constructible_v<std::decay_t<Function>, Function&>
    && requires(Target& target, std::decay_t<Function> const& split_function, Function& function,
                Vec2I const& position, Tile const& tile, Vec2S const& shape, std::size_t index) {
      { target.set_size(shape) } -> std::same_as<std::expected<void, StarException>>;
      { target.extent(index) } -> std::convertible_to<std::size_t>;
      { std::invoke(split_function, target[index], position, tile) } -> std::same_as<std::expected<void, StarException>>;
      { std::invoke(function, target(index, index), position, tile) } -> std::same_as<std::expected<void, StarException>>;
    };

// A custom target's references can alias even if neither is a proxy. Only
// the actual, non-packed MultiArray storage promises disjoint elements.
template <typename Target>
concept independent_tile_array = std::same_as<Target, MultiArray<typename Target::Element, 2>>
    && std::same_as<typename Target::reference, typename Target::Element&>;

template <typename Function, typename Tile>
concept tile_array_result = std::invocable<Function&, Vec2I const&, Tile const&>
    && requires { typename std::invoke_result_t<Function&, Vec2I const&, Tile const&>::value_type; }
    && std::is_object_v<typename std::invoke_result_t<Function&, Vec2I const&, Tile const&>::value_type>
    && tile_array_callback<Function, Tile const&, typename std::invoke_result_t<Function&, Vec2I const&, Tile const&>::value_type>
    && requires(MultiArray<typename std::invoke_result_t<Function&, Vec2I const&, Tile const&>::value_type, 2>& target,
                typename std::invoke_result_t<Function&, Vec2I const&, Tile const&>::value_type value) {
      target.set_size(Vec2S{});
      target[0] = std::move(value);
    };

// World tiles wrap horizontally. RectI bounds are right/top exclusive.
// Sector replacement, unloading and reinitialization invalidate tile/sector borrows.
template <typename TileT, unsigned sector_size_v> requires valid_sector_size<sector_size_v>
class TileSectorArray {
public:
  using Tile = TileT;
  static constexpr unsigned sector_size = sector_size_v;
  using SectorArray = SectorArray2D<Tile, sector_size>;
  using Sector = typename SectorArray::Sector;
  using Array = typename SectorArray::Array;

  TileSectorArray() = default;
  TileSectorArray(Vec2U const& size, Tile default_tile = Tile()) {
    init(size, std::move(default_tile));
  }

  void init(Vec2U const& size, Tile default_tile = Tile()) {
    // Wrapped positions and exclusive sector edges must fit the public RectI.
    if (!std::in_range<int>(size[0]) || !std::in_range<int>(size[1]))
      Star::fatal_error("TileSectorArray world dimensions exceed RectI bounds", false);
    auto sector_count = [](unsigned n) { return n / sector_size + (n % sector_size != 0); };
    m_tile_sectors.init(sector_count(size[0]), sector_count(size[1]));
    m_world_size = size;
    m_default = std::move(default_tile);
  }

  Vec2U size() const { return m_world_size; }
  Tile default_tile() const requires std::is_copy_constructible_v<Tile> { return Tile(m_default); }

  bool sector_valid(Sector const& sector) const { return m_tile_sectors.sector_valid(sector); }

  Sector sector_for(Vec2I const& pos) const {
    return m_tile_sectors.sector_for(wrapped_x(pos[0]), static_cast<unsigned>(pos[1]));
  }

  [[nodiscard]] std::expected<List<Sector>, StarException> valid_sectors_for(RectI const& region) const {
    List<Sector> sectors;
    for (auto const split : split_rects(y_clamp_rect(region))) {
      auto range = m_tile_sectors.sector_range(split.rect.x_min(), split.rect.y_min(), split.rect.width(), split.rect.height());
      if (!range) return std::unexpected(std::move(range.error()));
      auto width = range->max[0] - range->min[0];
      auto height = range->max[1] - range->min[1];
      if (height && width > (sectors.max_size() - sectors.size()) / height)
        return std::unexpected(MultiArrayException("TileSectorArray sector list exceeds max_size"));
      sectors.reserve(sectors.size() + width * height);
      for (std::size_t x = range->min[0]; x < range->max[0]; ++x) {
        for (std::size_t y = range->min[1]; y < range->max[1]; ++y)
          sectors.push_back({x, y});
      }
    }
    return sectors;
  }

  RectI sector_region(Sector const& sector) const {
    if (!sector_valid(sector)) return RectI::null();
    auto corner = m_tile_sectors.sector_corner(sector);
    return RectI::with_size(Vec2I(corner), {static_cast<int>(std::min<std::size_t>(sector_size, m_world_size[0] - corner[0])),
                                            static_cast<int>(std::min<std::size_t>(sector_size, m_world_size[1] - corner[1]))});
  }

  Sector adjacent_sector(Sector const& sector, Vec2I const& movement) const {
    if (!sector_valid(sector)) return sector;
    auto corner = m_tile_sectors.sector_corner(sector);
    auto x = static_cast<std::int64_t>(corner[0]) + std::int64_t(movement[0]) * sector_size;
    auto y = static_cast<std::int64_t>(corner[1]) + std::int64_t(movement[1]) * sector_size;
    return m_tile_sectors.sector_for(wrapped_x(x), std::in_range<unsigned>(y) ? static_cast<unsigned>(y) : std::numeric_limits<unsigned>::max());
  }

  void load_sector(Sector const& sector, std::unique_ptr<Array> array) {
    if (sector_valid(sector)) m_tile_sectors.load_sector(sector, std::move(array));
  }

  void load_default_sector(Sector const& sector) requires std::constructible_from<Array, Tile const&> {
    if (sector_valid(sector)) m_tile_sectors.load_sector(sector, std::make_unique<Array>(m_default));
  }

  std::unique_ptr<Array> copy_sector(Sector const& sector) const requires std::is_copy_constructible_v<Array> {
    if (sector_valid(sector)) return m_tile_sectors.copy_sector(sector);
    return {};
  }

  std::unique_ptr<Array> unload_sector(Sector const& sector) {
    if (sector_valid(sector)) return m_tile_sectors.take_sector(sector);
    return {};
  }

  bool is_sector_loaded(Sector const& sector) const { return m_tile_sectors.is_sector_loaded(sector); }
  List<Sector> loaded_sectors() const { return m_tile_sectors.loaded_sectors(); }
  std::size_t loaded_sector_count() const { return m_tile_sectors.loaded_sector_count(); }

  template <typename Self> requires (std::same_as<std::remove_cvref_t<Self>, TileSectorArray> && std::is_lvalue_reference_v<Self>)
  auto sector_array(this Self&& self, Sector const& sector) {
    using Borrow = decltype(self.m_tile_sectors.sector(sector));
    return self.sector_valid(sector) ? self.m_tile_sectors.sector(sector) : Borrow{};
  }

  bool tile_loaded(Vec2I const& pos) const {
    return valid_row(pos[1]) && m_tile_sectors.is_sector_loaded(sector_for(pos));
  }

  Tile const& tile(Vec2I const& pos) const & {
    if (valid_row(pos[1])) {
      if (auto value = m_tile_sectors.get(wrapped_x(pos[0]), static_cast<unsigned>(pos[1]))) return value->get();
    }
    return m_default;
  }
  Tile const& tile(Vec2I const&) const && = delete;

  std::optional<std::reference_wrapper<Tile>> modify_tile(Vec2I const& pos) & {
    if (!valid_row(pos[1])) return std::nullopt;
    return m_tile_sectors.get(wrapped_x(pos[0]), static_cast<unsigned>(pos[1]));
  }

  // Missing tiles and out-of-world rows are read as the default tile.
  template <typename Function> requires tile_array_callback<Function, Tile const&>
  [[nodiscard]] std::expected<void, StarException> tile_each(RectI const& region, Function&& function) const {
    auto r = tile_each_abortable(region, [&](Vec2I const& pos, Tile const& tile) -> std::expected<bool, StarException> {
      if (auto result = std::invoke(function, pos, tile); !result) return std::unexpected(std::move(result.error()));
      return true;
    });
    if (!r) return std::unexpected(std::move(r.error()));
    return {};
  }

  template <typename Function> requires tile_array_result<Function, Tile>
  [[nodiscard]] auto tile_each_result(WorkerPool& workers, RectI const& region, Function&& function) const
      -> std::expected<MultiArray<typename std::invoke_result_t<Function&, Vec2I const&, Tile const&>::value_type, 2>, StarException> {
    MultiArray<typename std::invoke_result_t<Function&, Vec2I const&, Tile const&>::value_type, 2> results;
    auto r = tile_each_to(workers, results, region, [&](auto&& target, Vec2I const& pos, Tile const& tile) -> std::expected<void, StarException> {
      auto value = std::invoke(function, pos, tile);
      if (!value) return std::unexpected(std::move(value.error()));
      target = std::move(*value);
      return {};
    });
    if (!r) return std::unexpected(std::move(r.error()));
    return results;
  }

  template <typename Target, typename Function> requires tile_array_writer<Target, Function, Tile>
  [[nodiscard]] std::expected<void, StarException> tile_each_to(WorkerPool& workers, Target& results, RectI const& region, Function&& function) const {
    if (region.is_empty()) return results.set_size(Vec2S{0, 0});
    auto width = std::int64_t(region.x_max()) - region.x_min();
    auto height = std::int64_t(region.y_max()) - region.y_min();
    auto resized = results.set_size(Vec2S(static_cast<std::size_t>(width), static_cast<std::size_t>(height)));
    if (!resized) return std::unexpected(std::move(resized.error()));
    for (auto const split : split_rects(region, false)) {
      auto rect = y_clamp_rect(split.rect);
      if (!rect.is_empty()) {
        // The original callable handles out-of-world rows. Each loaded-world
        // split uses its own const copy (including the serial proxy path).
        auto visit_column = [&, split_function = function](std::size_t x, std::size_t y, std::optional<std::span<Tile const>> column,
                                         std::size_t count) -> std::expected<bool, StarException> {
          int source_x = split.source_x(x);
          std::size_t base = static_cast<std::size_t>(std::int64_t(source_x) - region.x_min()) * results.extent(1)
              + static_cast<std::size_t>(static_cast<std::int64_t>(y) - region.y_min());
          for (std::size_t i = 0; i < count; ++i) {
            Vec2I const pos(source_x, static_cast<int>(y + i));
            if (auto r = std::invoke(split_function, results[base + i], pos, column ? (*column)[i] : m_default); !r)
              return std::unexpected(std::move(r.error()));
          }
          return true;
        };
        auto visited = [&] {
          if constexpr (independent_tile_array<Target>)
            return m_tile_sectors.eval_columns_parallel(workers, rect.x_min(), rect.y_min(), rect.width(), rect.height(), visit_column, true);
          else
            return m_tile_sectors.eval_columns(rect.x_min(), rect.y_min(), rect.width(), rect.height(), visit_column, true);
        }();
        if (!visited) return std::unexpected(std::move(visited.error()));
      }
      auto defaults = visit_default_rows(split, [&](Vec2I const& pos, Tile const& tile) -> std::expected<bool, StarException> {
        auto x = static_cast<std::size_t>(std::int64_t(pos[0]) - region.x_min());
        auto y = static_cast<std::size_t>(std::int64_t(pos[1]) - region.y_min());
        if (auto r = std::invoke(function, results(x, y), pos, tile); !r) return std::unexpected(std::move(r.error()));
        return true;
      });
      if (!defaults) return std::unexpected(std::move(defaults.error()));
    }
    return {};
  }

  // Visit each loaded sector window once. Local bounds index the borrowed
  // sector; source_min retains query-space coordinates across the seam.
  template <typename Function>
    requires std::invocable<Function&, Array&, RectI const&, Vec2I const&>
        && (std::same_as<std::invoke_result_t<Function&, Array&, RectI const&, Vec2I const&>, void>
            || std::same_as<std::invoke_result_t<Function&, Array&, RectI const&, Vec2I const&>, std::expected<void, StarException>>)
  auto visit_loaded_sectors(RectI const& region, Function&& function)
      -> std::invoke_result_t<Function&, Array&, RectI const&, Vec2I const&> {
    using Result = std::invoke_result_t<Function&, Array&, RectI const&, Vec2I const&>;
    for (auto const split : split_rects(y_clamp_rect(region))) {
      auto const& rect = split.rect;
      if (rect.is_empty()) continue;
      auto first = SectorArray::sector_for(rect.x_min(), rect.y_min());
      auto last = SectorArray::sector_for(rect.x_max() - 1, rect.y_max() - 1);
      for (std::size_t x = first[0]; x <= last[0]; ++x) {
        for (std::size_t y = first[1]; y <= last[1]; ++y) {
          auto sector = m_tile_sectors.sector({x, y});
          if (!sector) continue;
          auto corner = Vec2I(SectorArray::sector_corner({x, y}));
          auto local = rect.translated(-corner);
          RectI const window(std::max(local.x_min(), 0), std::max(local.y_min(), 0),
              std::min(local.x_max(), static_cast<int>(sector_size)), std::min(local.y_max(), static_cast<int>(sector_size)));
          Vec2I const source_min(split.source_x(corner[0] + window.x_min()), corner[1] + window.y_min());
          if constexpr (std::is_void_v<Result>)
            std::invoke(function, sector->get(), window, source_min);
          else if (auto result = std::invoke(function, sector->get(), window, source_min); !result)
            return result;
        }
      }
    }
    if constexpr (!std::is_void_v<Result>) return {};
  }

  template <typename Function> requires tile_array_callback<Function, Tile&>
  [[nodiscard]] std::expected<void, StarException> tile_eval(RectI const& region, Function&& function) {
    for (auto const split : split_rects(region)) {
      auto rect = y_clamp_rect(split.rect);
      if (rect.is_empty()) continue;
      auto visited = m_tile_sectors.eval(rect.x_min(), rect.y_min(), rect.width(), rect.height(),
          [&](std::size_t x, std::size_t y, std::optional<std::reference_wrapper<Tile>> tile) -> std::expected<bool, StarException> {
            Vec2I const pos(split.source_x(x), static_cast<int>(y));
            if (auto r = std::invoke(function, pos, tile->get()); !r) return std::unexpected(std::move(r.error()));
            return true;
          });
      if (!visited) return std::unexpected(std::move(visited.error()));
    }
    return {};
  }

  template <typename Function> requires tile_array_callback<Function, std::span<Tile const>>
  [[nodiscard]] std::expected<void, StarException> tile_each_columns(RectI const& region, Function&& function) const {
    return visit_columns(region, function, std::nullopt);
  }

  template <typename Function> requires tile_array_callback<Function, std::span<Tile>&>
  [[nodiscard]] std::expected<void, StarException> tile_eval_columns(RectI const& region, Function&& function) {
    return visit_columns(region, function, std::nullopt);
  }

  template <typename Function> requires tile_array_callback<Function, std::span<Tile>&>
  [[nodiscard]] std::expected<void, StarException> tile_eval_columns_parallel(WorkerPool& workers, RectI const& region, Function&& function) {
    return visit_columns(region, function, std::ref(workers));
  }

  template <typename Function> requires tile_array_callback<Function, Tile const&, bool>
  [[nodiscard]] std::expected<bool, StarException> tile_satisfies(RectI const& region, Function&& function) const {
    auto result = tile_each_abortable(region, [&](Vec2I const& pos, Tile const& tile) -> std::expected<bool, StarException> {
      auto r = std::invoke(function, pos, tile);
      if (!r) return std::unexpected(std::move(r.error()));
      return !*r;
    });
    if (!result) return std::unexpected(std::move(result.error()));
    return !*result;
  }

  template <typename Function> requires tile_array_callback<Function, Tile const&, bool>
  [[nodiscard]] std::expected<bool, StarException> tile_satisfies(Vec2I const& pos, unsigned distance, Function&& function) const {
    auto radius = std::int64_t(distance);
    auto x_min = pos[0] - radius, y_min = pos[1] - radius;
    auto x_max = pos[0] + radius + 1, y_max = pos[1] + radius + 1;
    if (!std::in_range<int>(x_min) || !std::in_range<int>(x_max) || !std::in_range<int>(y_min) || !std::in_range<int>(y_max))
      return std::unexpected(MultiArrayException("TileSectorArray search bounds exceed RectI"));
    return tile_satisfies(RectI(static_cast<int>(x_min), static_cast<int>(y_min), static_cast<int>(x_max), static_cast<int>(y_max)), function);
  }

private:
  struct SplitRect {
    RectI rect;
    std::int64_t x_offset;
    int source_x(std::int64_t x) const { return static_cast<int>(x_offset + x); }
  };

  unsigned wrapped_x(std::int64_t x) const {
    if (!m_world_size[0]) return 0;
    auto remainder = x % m_world_size[0];
    return static_cast<unsigned>(remainder < 0 ? remainder + m_world_size[0] : remainder);
  }

  int row_limit() const { return m_world_size[0] ? static_cast<int>(m_world_size[1]) : 0; }
  bool valid_row(int y) const { return y >= 0 && y < row_limit(); }

  RectI y_clamp_rect(RectI const& rect) const {
    auto limit = row_limit();
    return {rect.x_min(), std::clamp(rect.y_min(), 0, limit), rect.x_max(), std::clamp(rect.y_max(), 0, limit)};
  }

  // Full-world queries visit the canonical world once. Output arrays instead
  // cover every requested coordinate, repeating the world as often as needed.
  auto split_rects(RectI const& rect, bool canonical = true) const {
    std::int64_t world_width = m_world_size[0];
    std::int64_t begin = rect.x_min(), end = rect.x_max();
    if (world_width && canonical && end - begin >= world_width) {
      begin = 0;
      end = world_width;
    }
    auto offset = begin - wrapped_x(begin);
    std::int64_t count = rect.is_empty() ? 0 : world_width ? 1 + (end - offset - 1) / world_width : 1;
    return std::views::iota(std::int64_t{0}, count) | std::views::transform([=](std::int64_t i) -> SplitRect {
      if (!world_width) return {rect, 0};
      auto origin = offset + i * world_width;
      return {RectI(static_cast<int>(std::max<std::int64_t>(0, begin - origin)), rect.y_min(),
                    static_cast<int>(std::min(world_width, end - origin)), rect.y_max()), origin};
    });
  }

  template <typename Function> requires tile_array_callback<Function, Tile const&, bool>
  std::expected<bool, StarException> visit_default_rows(SplitRect const& split, Function&& function) const {
    auto const& rect = split.rect;
    for (auto [begin, end] : {std::pair{rect.y_min(), std::min(rect.y_max(), 0)},
                              std::pair{std::max(rect.y_min(), row_limit()), rect.y_max()}}) {
      if (begin >= end) continue;
      for (int x = rect.x_min(); x < rect.x_max(); ++x) {
        for (int y = begin; y < end; ++y) {
          Vec2I const pos(split.source_x(x), y);
          auto r = std::invoke(function, pos, m_default);
          if (!r || !*r) return r;
        }
      }
    }
    return true;
  }

  template <typename Function> requires tile_array_callback<Function, Tile const&, bool>
  std::expected<bool, StarException> tile_each_abortable(RectI const& region, Function&& function) const {
    for (auto const split : split_rects(region)) {
      auto rect = y_clamp_rect(split.rect);
      if (!rect.is_empty()) {
        auto visited = m_tile_sectors.eval(rect.x_min(), rect.y_min(), rect.width(), rect.height(),
            [&](std::size_t x, std::size_t y, std::optional<std::reference_wrapper<Tile const>> tile) {
              Vec2I const pos(split.source_x(x), static_cast<int>(y));
              return std::invoke(function, pos, tile ? tile->get() : m_default);
            }, true);
        if (!visited || !*visited) return visited;
      }
      auto defaults = visit_default_rows(split, function);
      if (!defaults || !*defaults) return defaults;
    }
    return true;
  }

  template <typename Self, typename Function>
    requires std::same_as<std::remove_const_t<Self>, TileSectorArray>
        && tile_array_callback<Function, std::conditional_t<std::is_const_v<Self>, std::span<Tile const>, std::span<Tile>&>>
  std::expected<void, StarException> visit_columns(this Self& self, RectI const& region, Function& function,
                                                   std::optional<std::reference_wrapper<WorkerPool>> workers) {
    using Value = std::conditional_t<std::is_const_v<Self>, Tile const, Tile>;
    for (auto const split : self.split_rects(region)) {
      auto rect = self.y_clamp_rect(split.rect);
      if (rect.is_empty()) continue;
      auto column_visitor = [&](std::size_t x, std::size_t y, std::optional<std::span<Value>> column, std::size_t) -> std::expected<bool, StarException> {
        Vec2I const pos(split.source_x(x), static_cast<int>(y));
        auto r = [&] {
          if constexpr (std::is_const_v<Self>) return std::invoke(function, pos, std::span<Value>(*column));
          else return std::invoke(function, pos, *column);
        }();
        if (!r) return std::unexpected(std::move(r.error()));
        return true;
      };
      auto visited = workers
          ? self.m_tile_sectors.eval_columns_parallel(workers->get(), rect.x_min(), rect.y_min(), rect.width(), rect.height(), column_visitor)
          : self.m_tile_sectors.eval_columns(rect.x_min(), rect.y_min(), rect.width(), rect.height(), column_visitor);
      if (!visited) return std::unexpected(std::move(visited.error()));
    }
    return {};
  }

  Vec2U m_world_size{};
  Tile m_default{};
  SectorArray m_tile_sectors;
};

}

export module star.tile_sector_array;

export namespace Star {
  using ::Star::tile_array_callback;
  using ::Star::tile_array_writer;
  using ::Star::tile_array_result;
  using ::Star::TileSectorArray;
}
