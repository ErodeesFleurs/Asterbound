module;
import star.rect;
import star.multi_array;
import star.list;
import star.format;
import star.set;
import star.vector;
import star.thread;
import star.worker_pool;

import std;
import star.sector_array_2d;

import star.worker_pool;

import star.math_common;
namespace Star {

// Storage container for world tiles that understands the sector based
// non-euclidean nature of the World.
//
// All RectI regions in this class are assumed to be right/top exclusive, so
// each tile covered by a RectI region must be strictly contained within the
// region to be included.
template <typename TileT, unsigned SectorSizeT>
class TileSectorArray {
public:
  using Tile = TileT;
  static unsigned const SectorSize = SectorSizeT;

  using SectorArray = SectorArray2D<Tile, SectorSize>;
  using Sector = typename SectorArray::Sector;
  using Array = typename SectorArray::Array;

  TileSectorArray();
  TileSectorArray(Vec2U const& size, Tile defaultTile = Tile());

  void init(Vec2U const& size, Tile defaultTile = Tile());

  Vec2U size() const;
  Tile defaultTile() const;

  // Returns true if this sector is within the size bounds, regardless of
  // loaded / unloaded status.
  bool sectorValid(Sector const& sector) const;

  Sector sectorFor(Vec2I const& pos) const;

  // Return all valid sectors within a given range, regardless of loaded /
  // unloaded status.
  [[nodiscard]] std::expected<List<Sector>, StarException> validSectorsFor(RectI const& region) const;

  // Returns the region for this sector, which is SectorSize x SectorSize
  // large.
  RectI sectorRegion(Sector const& sector) const;

  // Returns adjacent sectors in any given integral movement, in sectors.
  Sector adjacentSector(Sector const& sector, Vec2I const& sectorMovement);

  // Load a sector into the active sector array.
  void loadSector(Sector const& sector, std::unique_ptr<Array> array);
  // Load with a sector full of the default tile.
  void loadDefaultSector(Sector const& sector);
  // Make a copy of a sector
  std::unique_ptr<Array> copySector(Sector const& sector);
  // Take a sector out of the sector array.
  std::unique_ptr<Array> unloadSector(Sector const& sector);

  bool sectorLoaded(Sector sector) const;
  List<Sector> loadedSectors() const;
  std::size_t loadedSectorCount() const;

  // Empty if the sector is unloaded.
  std::optional<std::reference_wrapper<Array const>> sectorArray(Sector sector) const;
  std::optional<std::reference_wrapper<Array>> sectorArray(Sector sector);

  bool tileLoaded(Vec2I const& pos) const;

  Tile const& tile(Vec2I const& pos) const;

  // Empty if the position is invalid or unloaded.
  std::optional<std::reference_wrapper<Tile>> modifyTile(Vec2I const& pos);

  // Function signature here is (Vec2I const&, Tile const&).  Will be called
  // The callback returns expected<void, StarException>; errors stop traversal.
  // Called for the entire region, valid or not. If tile positions are not valid,
  // they will be called with the defaultTile.
  template <typename Function>
  [[nodiscard]] std::expected<void, StarException> tileEach(RectI const& region, Function&& function) const;

  // Behaves like tileEach, but gathers the results of calling the function into
  // a MultiArray. The callback returns expected<T, StarException>.
  template <typename Function>
  [[nodiscard]] std::expected<MultiArray<typename std::invoke_result_t<Function, Vec2I, Tile>::value_type, 2>, StarException> tileEachResult(WorkerPool& workers, RectI const& region, Function&& function) const;

  // Fastest way to copy data from the tile array to a given target array.
  // Takes a multi-array and a region and a function, resizes the multi-array
  // to be the size of the given region, and then calls the given function on
  // each tile in the region with this signature:
  // function(MultiArray::Element& target, Vec2I const& position, Tile const& source);
  // Called with the defaultTile for out of range positions.
  template <typename MultiArray, typename Function>
  [[nodiscard]] std::expected<void, StarException> tileEachTo(WorkerPool& workers, MultiArray& results, RectI const& region, Function&& function) const;

  // Function signature here is (Vec2I const&, Tile&).  If a tile position
  // within this range is not valid, the function *will not* be called for that
  // position.
  template <typename Function>
  [[nodiscard]] std::expected<void, StarException> tileEval(RectI const& region, Function&& function);

  // Will not be called for parts of the region that are not valid positions.
  template <typename Function>
  [[nodiscard]] std::expected<void, StarException> tileEachColumns(RectI const& region, Function&& function) const;
  template <typename Function>
  [[nodiscard]] std::expected<void, StarException> tileEvalColumns(RectI const& region, Function&& function);
  template <typename Function>
  [[nodiscard]] std::expected<void, StarException> tileEvalColumnsParallel(WorkerPool& workers, RectI const& region, Function&& function);

  // Searches for a tile that satisfies a given condition in a block-area.
  // Returns true on the first instance found.  Passed in function must accept
  // (Vec2I const&, Tile const&).
  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> tileSatisfies(RectI const& region, Function&& function) const;
  // Same, but uses a radius of 'distance', which is inclusive on all sides.
  // In other words, calling tileSatisfies({0, 0}, 1, <func>) should be
  // equivalent to calling tileSatisfies({-1, -1}, {3, 3}, <func>).
  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> tileSatisfies(Vec2I const& pos, unsigned distance, Function&& function) const;

private:
  struct SplitRect {
    RectI rect;
    int xOffset;
  };

  // The callback returns expected<bool, StarException>; false stops iteration.
  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> tileEachAbortable(RectI const& region, Function&& function) const;

  // Splits rects along the world wrap line and wraps the x coordinate for each
  // rect into world space.  Also returns the integral x offset to transform
  // back into the input rect range.
  [[nodiscard]] std::expected<StaticList<SplitRect, 2>, StarException> splitRect(RectI rect) const;

  // Clamp the rect to entirely within valid tile spaces in y dimension
  RectI yClampRect(RectI const& r) const;

  Vec2U m_worldSize;
  Tile m_default;
  SectorArray m_tileSectors;
};

template <typename Tile, unsigned SectorSize>
unsigned const TileSectorArray<Tile, SectorSize>::SectorSize;

template <typename Tile, unsigned SectorSize>
TileSectorArray<Tile, SectorSize>::TileSectorArray() {}

template <typename Tile, unsigned SectorSize>
TileSectorArray<Tile, SectorSize>::TileSectorArray(Vec2U const& size, Tile defaultTile) {
  init(size, std::move(defaultTile));
}

template <typename Tile, unsigned SectorSize>
void TileSectorArray<Tile, SectorSize>::init(Vec2U const& size, Tile defaultTile) {
  m_worldSize = size;
  // Initialize to enough sectors to fit world size at least.
  m_tileSectors.init((size[0] + SectorSize - 1) / SectorSize, (size[1] + SectorSize - 1) / SectorSize);
  m_default = std::move(defaultTile);
}

template <typename Tile, unsigned SectorSize>
Vec2U TileSectorArray<Tile, SectorSize>::size() const {
  return m_worldSize;
}

template <typename Tile, unsigned SectorSize>
Tile TileSectorArray<Tile, SectorSize>::defaultTile() const {
  return m_default;
}

template <typename Tile, unsigned SectorSize>
auto TileSectorArray<Tile, SectorSize>::sectorFor(Vec2I const& pos) const -> Sector {
  return m_tileSectors.sectorFor(static_cast<unsigned>(pmod<int>(pos[0], m_worldSize[0])),
                                 static_cast<unsigned>(pos[1]));
}

template <typename Tile, unsigned SectorSize>
bool TileSectorArray<Tile, SectorSize>::sectorValid(Sector const& sector) const {
  return m_tileSectors.sectorValid(sector);
}

template <typename Tile, unsigned SectorSize>
[[nodiscard]] auto TileSectorArray<Tile, SectorSize>::validSectorsFor(RectI const& region) const -> std::expected<List<Sector>, StarException> {
  List<Sector> sectors;
  auto splits = splitRect(yClampRect(region));
  if (!splits) return std::unexpected(std::move(splits.error()));
  for (auto const& split : *splits) {
    auto sectorRange = m_tileSectors.sectorRange(split.rect.xMin(), split.rect.yMin(), split.rect.width(), split.rect.height());
    sectors.reserve(sectors.size() + (sectorRange.max[0] - sectorRange.min[0]) * (sectorRange.max[1] - sectorRange.min[1]));
    for (std::size_t x = sectorRange.min[0]; x < sectorRange.max[0]; ++x) {
      for (std::size_t y = sectorRange.min[1]; y < sectorRange.max[1]; ++y)
        sectors.append({x, y});
    }
  }
  return sectors;
}

template <typename Tile, unsigned SectorSize>
RectI TileSectorArray<Tile, SectorSize>::sectorRegion(Sector const& sector) const {
  Vec2I sectorCorner(m_tileSectors.sectorCorner(sector));
  return RectI::withSize(sectorCorner, {std::min<int>(SectorSize, m_worldSize[0] - sectorCorner[0]), std::min<int>(SectorSize, m_worldSize[1] - sectorCorner[1])});
}

template <typename Tile, unsigned SectorSize>
auto TileSectorArray<Tile, SectorSize>::adjacentSector(Sector const& sector, Vec2I const& sectorMovement) -> Sector {
  // This works because the only smaller than SectorSize sectors are on the
  // world wrap point, and there is only one vertical line of them, but it's
  // very not-obvious that it works.
  Vec2I corner(m_tileSectors.sectorCorner(sector));
  corner += sectorMovement * SectorSize;
  return sectorFor(corner);
}

template <typename Tile, unsigned SectorSize>
void TileSectorArray<Tile, SectorSize>::loadSector(Sector const& sector, std::unique_ptr<Array> tile) {
  if (sectorValid(sector))
    m_tileSectors.loadSector(sector, std::move(tile));
}

template <typename Tile, unsigned SectorSize>
void TileSectorArray<Tile, SectorSize>::loadDefaultSector(Sector const& sector) {
  if (sectorValid(sector))
    m_tileSectors.loadSector(sector, std::make_unique<Array>(m_default));
}

template <typename Tile, unsigned SectorSize>
auto TileSectorArray<Tile, SectorSize>::copySector(Sector const& sector) -> std::unique_ptr<Array> {
  if (sectorValid(sector))
    return m_tileSectors.copySector(sector);
  else
    return {};
}

template <typename Tile, unsigned SectorSize>
auto TileSectorArray<Tile, SectorSize>::unloadSector(Sector const& sector) -> std::unique_ptr<Array> {
  if (sectorValid(sector))
    return m_tileSectors.takeSector(sector);
  else
    return {};
}

template <typename Tile, unsigned SectorSize>
bool TileSectorArray<Tile, SectorSize>::sectorLoaded(Sector sector) const {
  if (sectorValid(sector))
    return m_tileSectors.sectorLoaded(sector);
  else
    return false;
}

template <typename Tile, unsigned SectorSize>
auto TileSectorArray<Tile, SectorSize>::loadedSectors() const -> List<Sector> {
  return m_tileSectors.loadedSectors();
}

template <typename Tile, unsigned SectorSize>
std::size_t TileSectorArray<Tile, SectorSize>::loadedSectorCount() const {
  return m_tileSectors.loadedSectorCount();
}

template <typename Tile, unsigned SectorSize>
auto TileSectorArray<Tile, SectorSize>::sectorArray(Sector sector) const -> std::optional<std::reference_wrapper<Array const>> {
  if (sectorValid(sector))
    return m_tileSectors.sector(sector);
  return std::nullopt;
}

template <typename Tile, unsigned SectorSize>
auto TileSectorArray<Tile, SectorSize>::sectorArray(Sector sector) -> std::optional<std::reference_wrapper<Array>> {
  if (sectorValid(sector))
    return m_tileSectors.sector(sector);
  return std::nullopt;
}

template <typename Tile, unsigned SectorSize>
bool TileSectorArray<Tile, SectorSize>::tileLoaded(Vec2I const& pos) const {
  if (pos[1] < 0 || pos[1] >= static_cast<int>(m_worldSize[1]))
    return false;

  unsigned xind = static_cast<unsigned>(pmod<int>(pos[0], m_worldSize[0]));
  unsigned yind = static_cast<unsigned>(pos[1]);

  return m_tileSectors.get(xind, yind).has_value();
}

template <typename Tile, unsigned SectorSize>
Tile const& TileSectorArray<Tile, SectorSize>::tile(Vec2I const& pos) const {
  if (pos[1] < 0 || pos[1] >= static_cast<int>(m_worldSize[1]))
    return m_default;

  unsigned xind = static_cast<unsigned>(pmod<int>(pos[0], m_worldSize[0]));
  unsigned yind = static_cast<unsigned>(pos[1]);

  auto tile = m_tileSectors.get(xind, yind);
  if (tile)
    return tile->get();
  return m_default;
}

template <typename Tile, unsigned SectorSize>
auto TileSectorArray<Tile, SectorSize>::modifyTile(Vec2I const& pos) -> std::optional<std::reference_wrapper<Tile>> {
  if (pos[1] < 0 || pos[1] >= static_cast<int>(m_worldSize[1]))
    return std::nullopt;

  unsigned xind = static_cast<unsigned>(pmod<int>(pos[0], m_worldSize[0]));
  unsigned yind = static_cast<unsigned>(pos[1]);

  return m_tileSectors.get(xind, yind);
}

template <typename Tile, unsigned SectorSize>
template <typename Function>
[[nodiscard]] std::expected<void, StarException> TileSectorArray<Tile, SectorSize>::tileEach(RectI const& region, Function&& function) const {
  auto result = tileEachAbortable(region, [&](Vec2I const& pos, Tile const& tile) -> std::expected<bool, StarException> {
    auto r = function(pos, tile);
    if (!r) return std::unexpected(std::move(r.error()));
    return true;
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}

template <typename Tile, unsigned SectorSize>
template <typename Function>
[[nodiscard]] std::expected<MultiArray<typename std::invoke_result_t<Function, Vec2I, Tile>::value_type, 2>, StarException> TileSectorArray<Tile, SectorSize>::tileEachResult(WorkerPool& workers, RectI const& region, Function&& function) const {
  MultiArray<typename std::invoke_result_t<Function, Vec2I, Tile>::value_type, 2> res;
  auto result = tileEachTo(workers, res, region, [&](auto& target, Vec2I const& pos, Tile const& tile) -> std::expected<void, StarException> {
    auto value = function(pos, tile);
    if (!value) return std::unexpected(std::move(value.error()));
    target = std::move(*value);
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return res;
}

template <typename Tile, unsigned SectorSize>
template <typename MultiArray, typename Function>
[[nodiscard]] std::expected<void, StarException> TileSectorArray<Tile, SectorSize>::tileEachTo(WorkerPool& workers, MultiArray& results, RectI const& region, Function&& function) const {
  if (region.isEmpty()) {
    results.setSize({0, 0});
    return {};
  }

  int xArrayOffset = -region.xMin();
  int yArrayOffset = -region.yMin();
  results.setSize(Vec2S(region.size()));

  auto splits = splitRect(region);
  if (!splits) return std::unexpected(std::move(splits.error()));
  for (auto const& split : *splits) {
    auto clampedRect = yClampRect(split.rect);
    if (!clampedRect.isEmpty()) {
      auto evaluated = m_tileSectors.evalColumnsParallel(workers, clampedRect.xMin(), clampedRect.yMin(), clampedRect.width(), clampedRect.height(), [&, function](std::size_t x, std::size_t y, std::optional<std::span<Tile const>> column, std::size_t columnSize) -> std::expected<bool, StarException> {
          std::size_t arrayColumnIndex = (x + split.xOffset + xArrayOffset) * results.size(1) + y + yArrayOffset;
          if (column) {
            for (std::size_t i = 0; i < columnSize; ++i)
              if (auto r = function(results.atIndex(arrayColumnIndex + i),
                                    Vec2I(static_cast<int>(x) + split.xOffset, y + i), (*column)[i]);
                  !r)
                return std::unexpected(std::move(r.error()));
          } else {
            for (std::size_t i = 0; i < columnSize; ++i)
              if (auto r = function(results.atIndex(arrayColumnIndex + i),
                                    Vec2I(static_cast<int>(x) + split.xOffset, y + i), m_default);
                  !r)
                return std::unexpected(std::move(r.error()));
            }
          return true;
        }, true);
      if (!evaluated) return std::unexpected(std::move(evaluated.error()));
    }

    // Call with default tile for tiles outside of the y-range (to ensure that
    // every index in the rect gets called)
    for (int x = split.rect.xMin(); x < split.rect.xMax(); ++x) {
      for (int y = split.rect.yMin(); y < std::min<int>(split.rect.yMax(), 0); ++y)
        if (auto r = function(results(x + split.xOffset + xArrayOffset, y + yArrayOffset), Vec2I(x + split.xOffset, y), m_default); !r) return std::unexpected(std::move(r.error()));
    }

    for (int x = split.rect.xMin(); x < split.rect.xMax(); ++x) {
      for (int y = std::max<int>(split.rect.yMin(), m_worldSize[1]); y < split.rect.yMax(); ++y)
        if (auto r = function(results(x + split.xOffset + xArrayOffset, y + yArrayOffset), Vec2I(x + split.xOffset, y), m_default); !r) return std::unexpected(std::move(r.error()));
    }
  }
  return {};
}

template <typename Tile, unsigned SectorSize>
template <typename Function>
[[nodiscard]] std::expected<void, StarException> TileSectorArray<Tile, SectorSize>::tileEval(RectI const& region, Function&& function) {
  auto splits = splitRect(region);
  if (!splits) return std::unexpected(std::move(splits.error()));
  for (auto const& split : *splits) {
    auto clampedRect = yClampRect(split.rect);
    if (!clampedRect.isEmpty()) {
      // If non-const variant, do not call function if tile not loaded (pass
      // false to evalEmpty in sector array)
      auto fwrapper = [&](unsigned x, unsigned y,
                          std::optional<std::reference_wrapper<Tile>> tile) -> std::expected<bool, StarException> {
        if (auto r = function(Vec2I(static_cast<int>(x) + split.xOffset, static_cast<int>(y)), tile->get()); !r)
          return std::unexpected(std::move(r.error()));
        return true;
      };
      if (auto r = m_tileSectors.eval(clampedRect.xMin(), clampedRect.yMin(), clampedRect.width(), clampedRect.height(), fwrapper, false); !r) return std::unexpected(std::move(r.error()));
    }
  }
  return {};
}

template <typename Tile, unsigned SectorSize>
template <typename Function>
[[nodiscard]] std::expected<void, StarException> TileSectorArray<Tile, SectorSize>::tileEachColumns(RectI const& region, Function&& function) const {
  return const_cast<TileSectorArray&>(*this).tileEvalColumns(
      region, [&](Vec2I const& pos, std::span<Tile> tiles) { return function(pos, std::span<Tile const>(tiles)); });
}

template <typename Tile, unsigned SectorSize>
template <typename Function>
[[nodiscard]] std::expected<void, StarException> TileSectorArray<Tile, SectorSize>::tileEvalColumns(RectI const& region, Function&& function) {
  auto splits = splitRect(region);
  if (!splits) return std::unexpected(std::move(splits.error()));
  for (auto const& split : *splits) {
    auto clampedRect = yClampRect(split.rect);
    if (!clampedRect.isEmpty()) {
      auto fwrapper = [&](std::size_t x, std::size_t y, std::optional<std::span<Tile>> column,
                          std::size_t) -> std::expected<bool, StarException> {
        if (auto r = function(Vec2I(static_cast<int>(x) + split.xOffset, static_cast<int>(y)), *column); !r)
          return std::unexpected(std::move(r.error()));
        return true;
      };
      if (auto r = m_tileSectors.evalColumns(clampedRect.xMin(), clampedRect.yMin(), clampedRect.width(), clampedRect.height(), fwrapper, false); !r) return std::unexpected(std::move(r.error()));
    }
  }
  return {};
}

template <typename Tile, unsigned SectorSize>
template <typename Function>
[[nodiscard]] std::expected<void, StarException> TileSectorArray<Tile, SectorSize>::tileEvalColumnsParallel(WorkerPool& workers, RectI const& region, Function&& function) {
  auto splits = splitRect(region);
  if (!splits) return std::unexpected(std::move(splits.error()));
  for (auto const& split : *splits) {
    auto clampedRect = yClampRect(split.rect);
    if (!clampedRect.isEmpty()) {
      auto fwrapper = [&](std::size_t x, std::size_t y, std::optional<std::span<Tile>> column,
                          std::size_t) -> std::expected<bool, StarException> {
        if (auto r = function(Vec2I(static_cast<int>(x) + split.xOffset, static_cast<int>(y)), *column); !r)
          return std::unexpected(std::move(r.error()));
        return true;
      };
      if (auto r = m_tileSectors.evalColumnsParallel(workers, clampedRect.xMin(), clampedRect.yMin(), clampedRect.width(), clampedRect.height(), fwrapper, false); !r) return std::unexpected(std::move(r.error()));
    }
  }
  return {};
}

template <typename Tile, unsigned SectorSize>
template <typename Function>
[[nodiscard]] std::expected<bool, StarException> TileSectorArray<Tile, SectorSize>::tileSatisfies(Vec2I const& pos, unsigned distance, Function&& function) const {
  return tileSatisfies(RectI::withSize(pos - Vec2I::filled(distance), Vec2I::filled(distance * 2 + 1)), function);
}

template <typename Tile, unsigned SectorSize>
template <typename Function>
[[nodiscard]] std::expected<bool, StarException> TileSectorArray<Tile, SectorSize>::tileSatisfies(RectI const& region, Function&& function) const {
  auto result = tileEachAbortable(region, [&](Vec2I const& pos, Tile const& tile) -> std::expected<bool, StarException> {
    auto r = function(pos, tile);
    if (!r) return std::unexpected(std::move(r.error()));
    return !*r;
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return !*result;
}

template <typename Tile, unsigned SectorSize>
template <typename Function>
[[nodiscard]] std::expected<bool, StarException> TileSectorArray<Tile, SectorSize>::tileEachAbortable(RectI const& region, Function&& function) const {
  auto splits = splitRect(region);
  if (!splits) return std::unexpected(std::move(splits.error()));
  for (auto const& split : *splits) {
    auto clampedRect = yClampRect(split.rect);
    if (!clampedRect.isEmpty()) {
      // If const variant, call function with default tile if not loaded.
      auto fwrapper = [&](unsigned x, unsigned y, std::optional<std::reference_wrapper<Tile const>> tile) {
        return function(Vec2I(static_cast<int>(x) + split.xOffset, y), tile ? tile->get() : m_default);
      };
      auto result = m_tileSectors.eval(clampedRect.xMin(), clampedRect.yMin(), clampedRect.width(), clampedRect.height(), fwrapper, true);
      if (!result) return std::unexpected(std::move(result.error()));
      if (!*result) return false;
    }

    // Call with default tile for tiles outside of the y-range (to ensure
    // that every index in the rect gets called)
    for (int x = split.rect.xMin(); x < split.rect.xMax(); ++x) {
      for (int y = split.rect.yMin(); y < std::min<int>(split.rect.yMax(), 0); ++y) {
        auto result = function(Vec2I(x + split.xOffset, y), m_default);
        if (!result) return std::unexpected(std::move(result.error()));
        if (!*result) return false;
      }
    }

    for (int x = split.rect.xMin(); x < split.rect.xMax(); ++x) {
      for (int y = std::max<int>(split.rect.yMin(), m_worldSize[1]); y < split.rect.yMax(); ++y) {
        auto result = function(Vec2I(x + split.xOffset, y), m_default);
        if (!result) return std::unexpected(std::move(result.error()));
        if (!*result) return false;
      }
    }
  }
  return true;
}

template <typename Tile, unsigned SectorSize>
[[nodiscard]] auto TileSectorArray<Tile, SectorSize>::splitRect(RectI rect) const -> std::expected<StaticList<SplitRect, 2>, StarException> {
  // TODO: Offset here does not support rects outside of -m_worldSize[0] to 2 * m_worldSize[0]!
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed(
      (rect.xMin() >= -static_cast<int>(m_worldSize[0]) && rect.xMax() <= 2 * static_cast<int>(m_worldSize[0])));
  }

  // any rect at least the width of the world is equivalent to a rect that spans the width of the world exactly
  if (rect.width() >= static_cast<int>(m_worldSize[0]))
    return StaticList<SplitRect, 2>::create({SplitRect{RectI(0, rect.yMin(), m_worldSize[0], rect.yMax()), 0}});

  if (rect.isEmpty())
    return StaticList<SplitRect, 2>();

  int width = rect.width();
  int xMin = pmod<int>(rect.xMin(), m_worldSize[0]);
  int xOffset = rect.xMin() - xMin;
  rect.setXMin(xMin);
  rect.setXMax(xMin + width);

  if (rect.xMin() < static_cast<int>(m_worldSize[0]) && rect.xMax() > static_cast<int>(m_worldSize[0])) {
    return StaticList<SplitRect, 2>::create(
      {SplitRect{RectI(rect.xMin(), rect.yMin(), m_worldSize[0], rect.yMax()), xOffset},
       SplitRect{RectI(0, rect.yMin(), rect.xMax() - m_worldSize[0], rect.yMax()),
                 xOffset + static_cast<int>(m_worldSize[0])}});
  } else {
    return StaticList<SplitRect, 2>::create({SplitRect{rect, xOffset}});
  }
}

template <typename Tile, unsigned SectorSize>
RectI TileSectorArray<Tile, SectorSize>::yClampRect(RectI const& r) const {
  return RectI(r.xMin(), clamp<int>(r.yMin(), 0, m_worldSize[1]), r.xMax(), clamp<int>(r.yMax(), 0, m_worldSize[1]));
}

}

export module star.tile_sector_array;

export namespace Star {
  using ::Star::TileSectorArray;
}
