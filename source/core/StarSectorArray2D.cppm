module;

import star.multi_array;
import star.list;
import star.format;
import star.set;
import star.vector;
import star.thread;
import star.worker_pool;

import std;

namespace Star {

template <std::size_t sector_size>
concept valid_sector_size = sector_size > 0 && sector_size <= std::numeric_limits<std::size_t>::max() / sector_size;

template <typename Function, typename Element>
concept sector_element_visitor = requires(Function& function, std::size_t x, std::optional<std::reference_wrapper<Element>> element) {
  { std::invoke(function, x, std::size_t{}, element) } -> std::same_as<std::expected<bool, StarException>>;
};

template <typename Function, typename Element>
concept sector_column_visitor = requires(Function& function) {
  { std::invoke(function, std::size_t{}, std::size_t{}, std::optional<std::span<Element>>{}, std::size_t{}) }
      -> std::same_as<std::expected<bool, StarException>>;
};

// Dense slots provide coordinate lookup. Only loaded blocks own elements.
template <typename ElementT, std::size_t sector_size> requires valid_sector_size<sector_size>
class SectorArray2D {
public:
  using Element = ElementT;
  using Sector = Vec2S;

  struct SectorRange {
    // Lower left sector
    Vec2S min;
    // Upper right sector *non-inclusive*
    Vec2S max;
  };

  struct Array {
    Array() requires std::default_initializable<Element> : elements() {}

    explicit Array(Element const& value)
        requires (std::default_initializable<Element> && std::is_copy_assignable_v<Element>) {
      elements.fill(value);
    }

    template <typename Self> requires std::is_lvalue_reference_v<Self&&>
    decltype(auto) operator()(this Self&& self, std::size_t x, std::size_t y) {
      if constexpr (Star::DebugEnabled)
        Star::star_assert_failed(x < sector_size && y < sector_size);
      return self.elements[x * sector_size + y];
    }

    std::array<Element, sector_size * sector_size> elements;
  };

  SectorArray2D() = default;
  SectorArray2D(std::size_t sectors_wide, std::size_t sectors_high) {
    init(sectors_wide, sectors_high);
  }

  void init(std::size_t sectors_wide, std::size_t sectors_high) {
    constexpr auto max_sectors = std::numeric_limits<std::size_t>::max() / sector_size;
    if (sectors_wide > max_sectors || sectors_high > max_sectors)
      Star::fatal_error("SectorArray2D element extent overflows", false);
    m_sectors.clear();
    auto resized = m_sectors.set_size(sectors_wide, sectors_high);
    if (!resized)
      Star::fatal_error(resized.error().what(), false);
    m_loaded_sectors.clear();
  }

  std::size_t width() const { return m_sectors.extent(0) * sector_size; }
  std::size_t height() const { return m_sectors.extent(1) * sector_size; }

  bool sector_valid(Sector const& id) const {
    return id[0] < m_sectors.extent(0) && id[1] < m_sectors.extent(1);
  }

  static Sector sector_for(std::size_t x, std::size_t y) {
    return {x / sector_size, y / sector_size};
  }

  // Empty rectangles visit nothing, regardless of origin. Nonempty rectangles
  // must fit entirely in this grid; do not wrap or clip at this layer.
  [[nodiscard]] std::expected<SectorRange, StarException> sector_range(
      std::size_t min_x, std::size_t min_y, std::size_t width, std::size_t height) const {
    auto first = sector_for(min_x, min_y);
    if (!width || !height)
      return SectorRange{first, first};
    if (min_x > this->width() || width > this->width() - min_x
        || min_y > this->height() || height > this->height() - min_y)
      return std::unexpected(OutOfRangeException("SectorArray2D rectangle outside grid"));
    auto last = sector_for(min_x + width - 1, min_y + height - 1);
    return SectorRange{first, {last[0] + 1, last[1] + 1}};
  }

  static Vec2S sector_corner(Sector const& id) {
    return {id[0] * sector_size, id[1] * sector_size};
  }

  bool is_sector_loaded(Sector const& id) const {
    return sector_valid(id) && m_sectors(id[0], id[1]);
  }

  // Owning snapshot: safe to iterate while unloading sectors.
  List<Sector> loaded_sectors() const { return m_loaded_sectors.values(); }
  std::size_t loaded_sector_count() const { return m_loaded_sectors.size(); }

  // All borrows expire when the block is destroyed, replaced, or reinitialized.
  template <typename Self> requires std::is_lvalue_reference_v<Self&&>
  auto sector(this Self&& self, Sector const& id) {
    using Block = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>, Array const, Array>;
    using Borrow = std::optional<std::reference_wrapper<Block>>;
    if (self.sector_valid(id)) {
      if (auto const& slot = self.m_sectors(id[0], id[1]))
        return Borrow(*slot);
    }
    return Borrow{};
  }

  // Mutators require a valid sector id; passing an empty block unloads it.
  void load_sector(Sector const& id, std::unique_ptr<Array> array) {
    exchange_sector(id, std::move(array));
  }

  std::unique_ptr<Array> copy_sector(Sector const& id) const requires std::is_copy_constructible_v<Array> {
    if (auto block = sector(id))
      return std::make_unique<Array>(block->get());
    return {};
  }

  std::unique_ptr<Array> take_sector(Sector const& id) {
    return exchange_sector(id, {});
  }

  void discard_sector(Sector const& id) { take_sector(id); }

  template <typename Self> requires std::is_lvalue_reference_v<Self&&>
  auto get(this Self&& self, std::size_t x, std::size_t y) {
    auto block = self.sector(sector_for(x, y));
    using Cell = decltype(block->get()(x, y));
    using Borrow = std::optional<std::reference_wrapper<std::remove_reference_t<Cell>>>;
    if (block)
      return Borrow(std::in_place, block->get()(x % sector_size, y % sector_size));
    return Borrow{};
  }

private:
  template <typename Self>
  using VisitElement = std::conditional_t<std::is_const_v<Self>, Element const, Element>;

public:
  // Visits sector x, sector y, local column, then row. An unloaded cell is
  // skipped unless eval_empty is set. The visitor borrows storage only for
  // the call; do not replace sectors or resize the grid during traversal.
  template <typename Self, typename Function>
    requires (std::same_as<std::remove_const_t<Self>, SectorArray2D> && sector_element_visitor<Function, VisitElement<Self>>)
  [[nodiscard]] std::expected<bool, StarException> eval(this Self& self, std::size_t min_x, std::size_t min_y,
      std::size_t width, std::size_t height, Function&& function, bool eval_empty = false) {
    return self.eval_columns(min_x, min_y, width, height, [&](std::size_t x, std::size_t y,
        std::optional<std::span<VisitElement<Self>>> column, std::size_t count) -> std::expected<bool, StarException> {
      for (std::size_t i = 0; i < count; ++i) {
        std::optional<std::reference_wrapper<VisitElement<Self>>> element;
        if (column)
          element.emplace((*column)[i]);
        auto result = std::invoke(function, x, y + i, element);
        if (!result || !*result)
          return result;
      }
      return true;
    }, eval_empty);
  }

  // Each span is a contiguous column clipped to one sector. Empty columns
  // carry the same clipped length. False or an error stops serial traversal.
  template <typename Self, typename Function>
    requires (std::same_as<std::remove_const_t<Self>, SectorArray2D> && sector_column_visitor<Function, VisitElement<Self>>)
  [[nodiscard]] std::expected<bool, StarException> eval_columns(this Self& self, std::size_t min_x, std::size_t min_y,
      std::size_t width, std::size_t height, Function&& function, bool eval_empty = false) {
    return self.eval_columns_impl({}, min_x, min_y, width, height, function, eval_empty);
  }

  // Parallel jobs share the named visitor: concurrent invocations must be
  // safe. False stops only its x-sector strip; all submitted strips finish
  // before return. The first error in submission order takes precedence.
  template <typename Self, typename Function>
    requires (std::same_as<std::remove_const_t<Self>, SectorArray2D> && sector_column_visitor<Function, VisitElement<Self>>)
  [[nodiscard]] std::expected<bool, StarException> eval_columns_parallel(this Self& self, WorkerPool& workers,
      std::size_t min_x, std::size_t min_y, std::size_t width, std::size_t height, Function&& function, bool eval_empty = false) {
    return self.eval_columns_impl(std::ref(workers), min_x, min_y, width, height, function, eval_empty);
  }

private:
  using SectorArray = MultiArray<std::unique_ptr<Array>, 2>;

  template <typename Self, typename Function>
    requires (std::same_as<std::remove_const_t<Self>, SectorArray2D> && sector_column_visitor<Function, VisitElement<Self>>)
  [[nodiscard]] std::expected<bool, StarException> eval_columns_impl(this Self& self,
      std::optional<std::reference_wrapper<WorkerPool>> workers, std::size_t min_x, std::size_t min_y,
      std::size_t width, std::size_t height, Function& function, bool eval_empty) {
    auto range = self.sector_range(min_x, min_y, width, height);
    if (!range)
      return std::unexpected(std::move(range.error()));
    if (range->min[0] == range->max[0])
      return true;

    auto const max_x = min_x + width;
    auto const max_y = min_y + height;
    auto visit_strip = [&](std::size_t sector_x) -> std::expected<bool, StarException> {
      auto const x_origin = sector_x * sector_size;
      auto const first_x = std::max(min_x, x_origin) - x_origin;
      auto const end_x = std::min(max_x - x_origin, sector_size);
      for (auto sector_y = range->min[1]; sector_y != range->max[1]; ++sector_y) {
        auto const& array = self.m_sectors(sector_x, sector_y);
        if (!array && !eval_empty)
          continue;

        auto const y_origin = sector_y * sector_size;
        auto const first_y = std::max(min_y, y_origin) - y_origin;
        auto const end_y = std::min(max_y - y_origin, sector_size);
        auto const count = end_y - first_y;
        using Column = std::optional<std::span<VisitElement<Self>>>;
        auto visit_columns = [&](std::invocable<std::size_t&> auto&& column_at) -> std::expected<bool, StarException> {
          for (auto x = first_x; x != end_x; ++x) {
            auto result = std::invoke(function, x_origin + x, y_origin + first_y, column_at(x), std::size_t(count));
            if (!result || !*result)
              return result;
          }
          return true;
        };
        // Choose loaded/unloaded once per sector, outside the hot column loop.
        auto result = array ? visit_columns([&](std::size_t x) -> Column {
          return std::span<VisitElement<Self>>(array->elements).subspan(x * sector_size + first_y, count);
        }) : visit_columns([](std::size_t) -> Column { return {}; });
        if (!result || !*result)
          return result;
      }
      return true;
    };

    if (!workers || !workers->get().can_parallelize()) {
      for (auto x = range->min[0]; x != range->max[0]; ++x) {
        auto result = visit_strip(x);
        if (!result || !*result)
          return result;
      }
      return true;
    }

    std::vector<WorkerPoolPromise<bool>> jobs;
    jobs.reserve(range->max[0] - range->min[0]);
    for (auto x = range->min[0]; x != range->max[0]; ++x)
      jobs.push_back(workers->get().add_producer<bool>([&visit_strip, x] { return visit_strip(x); }));

    // Every job borrows the visitor and region. Drain even rejected jobs and
    // callback failures before any of those borrows leave scope.
    std::expected<bool, StarException> result = true;
    for (auto const& job : jobs) {
      auto completion = job.get();
      if (result) {
        if (!completion)
          result = std::unexpected(std::move(completion.error()));
        else if (!completion->get())
          *result = false;
      }
    }
    return result;
  }

  // A slot is indexed exactly when occupied. Replacing an occupied slot
  // does not touch the sparse index.
  std::unique_ptr<Array> exchange_sector(Sector const& id, std::unique_ptr<Array> replacement) {
    auto& slot = m_sectors(id[0], id[1]);
    if (replacement && !slot)
      m_loaded_sectors.add(id);
    else if (!replacement && slot)
      m_loaded_sectors.remove(id);
    return std::exchange(slot, std::move(replacement));
  }

  SectorArray m_sectors;
  HashSet<Sector> m_loaded_sectors;
};

}

export module star.sector_array_2d;

export namespace Star {
  using ::Star::valid_sector_size;
  using ::Star::sector_column_visitor;
  using ::Star::sector_element_visitor;
  using ::Star::SectorArray2D;
}
