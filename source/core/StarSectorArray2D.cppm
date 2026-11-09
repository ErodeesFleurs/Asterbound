module;

import star.multi_array;
import star.list;
import star.format;
import star.set;
import star.vector;
import star.thread;
import star.worker_pool;
import star.optional;

import std;

namespace Star {

// Holds a sparse 2d array of data based on sector size.  Meant to be used as a
// fast-as-possible sparse array.  Memory requiremenets are equal to the size
// of all loaded sectors PLUS pointer size * sectors wide * sectors high
template <typename ElementT, std::size_t SectorSize>
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
    Array();
    Array(Element const& def);

    Element const& operator()(std::size_t x, std::size_t y) const;
    Element& operator()(std::size_t x, std::size_t y);

    Element elements[SectorSize * SectorSize];
  };

  using DynamicArray = MultiArray<Element, 2>;

  SectorArray2D();
  SectorArray2D(std::size_t numSectorsWide, std::size_t numSectorsHigh);

  void init(std::size_t numSectorsWide, std::size_t numSectorsHigh);

  // Total size of array elements
  std::size_t width() const;
  std::size_t height() const;

  // Is sector within width() and heigh()
  bool sectorValid(Sector const& sector) const;

  // Returns the sector that contains the given point
  Sector sectorFor(std::size_t x, std::size_t y) const;
  // Returns the sector range that contains the given rectangle
  SectorRange sectorRange(std::size_t minX, std::size_t minY, std::size_t width, std::size_t height) const;

  Vec2S sectorCorner(Sector const& id) const;
  bool hasSector(Sector const& id) const;

  List<Sector> loadedSectors() const;
  std::size_t loadedSectorCount() const;
  bool sectorLoaded(Sector const& id) const;

  // Empty when the sector is not loaded.
  std::optional<std::reference_wrapper<Array>> sector(Sector const& id);
  std::optional<std::reference_wrapper<Array const>> sector(Sector const& id) const;

  void loadSector(Sector const& id, std::unique_ptr<Array> array);
  std::unique_ptr<Array> copySector(Sector const& id);
  std::unique_ptr<Array> takeSector(Sector const& id);
  void discardSector(Sector const& id);

  // Empty when the sector is not loaded.
  std::optional<std::reference_wrapper<Element const>> get(std::size_t x, std::size_t y) const;
  std::optional<std::reference_wrapper<Element>> get(std::size_t x, std::size_t y);

  // Fast evaluate of elements in the given range. If evalEmpty is true, then
  // function is also called for unloaded sectors with an empty optional.
  // Function receives (x, y, optional<reference_wrapper<Element [const]>>).
  // Given function returns expected<bool, StarException>: true to continue,
  // false to stop, or an error to propagate.
  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> eval(std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty = false) const;
  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> eval(std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty = false);

  // Individual sectors are stored column-major, so for speed, use this method
  // to get whole columns at a time. If evalEmpty is true, unloaded columns
  // receive an empty optional plus the correct columnSize.
  // Function receives (x, y, optional<span<Element [const]>>, columnSize).
  // columnSize is guaranteed never to be greater than SectorSize. Given
  // function returns expected<bool, StarException>: false stops traversal,
  // and an error propagates after all parallel jobs have been drained.
  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> evalColumns(
      std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty = false) const;
  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> evalColumns(std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty = false);
  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> evalColumnsParallel(WorkerPool& workers, std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty = false) const;
  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> evalColumnsParallel(WorkerPool& workers, std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty = false);

private:
  using SectorArray = MultiArray<std::unique_ptr<Array>, 2>;

  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> evalPriv(std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty);
  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> evalColumnsPriv(std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty);
  template <typename Function>
  [[nodiscard]] std::expected<bool, StarException> evalColumnsPrivPar(WorkerPool& workers, std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty);

  SectorArray m_sectors;
  HashSet<Sector> m_loadedSectors;
};

template <typename ElementT, std::size_t SectorSize>
SectorArray2D<ElementT, SectorSize>::Array::Array()
  : elements() {}

template <typename ElementT, std::size_t SectorSize>
SectorArray2D<ElementT, SectorSize>::Array::Array(Element const& def) {
  for (std::size_t i = 0; i < SectorSize * SectorSize; ++i)
    elements[i] = def;
}

template <typename ElementT, std::size_t SectorSize>
ElementT const& SectorArray2D<ElementT, SectorSize>::Array::operator()(std::size_t x, std::size_t y) const {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((x < SectorSize && y < SectorSize));
  }
  return elements[x * SectorSize + y];
}

template <typename ElementT, std::size_t SectorSize>
ElementT& SectorArray2D<ElementT, SectorSize>::Array::operator()(std::size_t x, std::size_t y) {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((x < SectorSize && y < SectorSize));
  }
  return elements[x * SectorSize + y];
}

template <typename ElementT, std::size_t SectorSize> SectorArray2D<ElementT, SectorSize>::SectorArray2D() = default;

template <typename ElementT, std::size_t SectorSize>
SectorArray2D<ElementT, SectorSize>::SectorArray2D(std::size_t numSectorsWide, std::size_t numSectorsHigh) {
  init(numSectorsWide, numSectorsHigh);
}

template <typename ElementT, std::size_t SectorSize>
void SectorArray2D<ElementT, SectorSize>::init(std::size_t numSectorsWide, std::size_t numSectorsHigh) {
  m_sectors.clear();
  m_sectors.setSize(numSectorsWide, numSectorsHigh);
  m_loadedSectors.clear();
}

template <typename ElementT, std::size_t SectorSize>
std::size_t SectorArray2D<ElementT, SectorSize>::width() const {
  return m_sectors.size(0) * SectorSize;
}

template <typename ElementT, std::size_t SectorSize>
std::size_t SectorArray2D<ElementT, SectorSize>::height() const {
  return m_sectors.size(1) * SectorSize;
}

template <typename ElementT, std::size_t SectorSize>
bool SectorArray2D<ElementT, SectorSize>::sectorValid(Sector const& sector) const {
  return sector[0] < m_sectors.size(0) && sector[1] < m_sectors.size(1);
}

template <typename ElementT, std::size_t SectorSize>
auto SectorArray2D<ElementT, SectorSize>::sectorFor(std::size_t x, std::size_t y) const -> Sector {
  return {x / SectorSize, y / SectorSize};
}

template <typename ElementT, std::size_t SectorSize>
auto SectorArray2D<ElementT, SectorSize>::sectorRange(std::size_t minX, std::size_t minY, std::size_t width, std::size_t height) const -> SectorRange {
  return {
    {minX / SectorSize, minY / SectorSize},
    {(minX + width + SectorSize - 1) / SectorSize, (minY + height + SectorSize - 1) / SectorSize}
  };
}

template <typename ElementT, std::size_t SectorSize>
Vec2S SectorArray2D<ElementT, SectorSize>::sectorCorner(Sector const& id) const {
  return Vec2S(id[0] * SectorSize, id[1] * SectorSize);
}

template <typename ElementT, std::size_t SectorSize>
bool SectorArray2D<ElementT, SectorSize>::hasSector(Sector const& id) const {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((id[0] < m_sectors.size(0) && id[1] < m_sectors.size(1)));
  }
  return (bool)m_sectors(id[0], id[1]);
}

template <typename ElementT, std::size_t SectorSize>
auto SectorArray2D<ElementT, SectorSize>::loadedSectors() const -> List<Sector> {
  return m_loadedSectors.values();
}

template <typename ElementT, std::size_t SectorSize>
std::size_t SectorArray2D<ElementT, SectorSize>::loadedSectorCount() const {
  return m_loadedSectors.size();
}

template <typename ElementT, std::size_t SectorSize>
bool SectorArray2D<ElementT, SectorSize>::sectorLoaded(Sector const& id) const {
  return m_loadedSectors.contains(id);
}

template <typename ElementT, std::size_t SectorSize>
auto SectorArray2D<ElementT, SectorSize>::sector(Sector const& id) -> std::optional<std::reference_wrapper<Array>> {
  return optionalRef(m_sectors(id[0], id[1]));
}

template <typename ElementT, std::size_t SectorSize>
auto SectorArray2D<ElementT, SectorSize>::sector(Sector const& id) const -> std::optional<std::reference_wrapper<Array const>> {
  if (auto const& array = m_sectors(id[0], id[1]))
    return std::cref(*array);
  return std::nullopt;
}

template <typename ElementT, std::size_t SectorSize>
void SectorArray2D<ElementT, SectorSize>::loadSector(Sector const& id, std::unique_ptr<Array> array) {
  auto& data = m_sectors(id[0], id[1]);
  data = std::move(array);
  if (data)
    m_loadedSectors.add(id);
  else
    m_loadedSectors.remove(id);
}

template <typename ElementT, std::size_t SectorSize>
std::unique_ptr<typename SectorArray2D<ElementT, SectorSize>::Array> SectorArray2D<ElementT, SectorSize>::copySector(
    Sector const& id) {
  if (auto const& array = m_sectors(id))
    return std::make_unique<Array>(*array);
  else
    return {};
}

template <typename ElementT, std::size_t SectorSize>
std::unique_ptr<typename SectorArray2D<ElementT, SectorSize>::Array> SectorArray2D<ElementT, SectorSize>::takeSector(
    Sector const& id) {
  std::unique_ptr<Array> ret;
  m_loadedSectors.remove(id);
  std::swap(m_sectors(id[0], id[1]), ret);
  return ret;
}

template <typename ElementT, std::size_t SectorSize>
void SectorArray2D<ElementT, SectorSize>::discardSector(Sector const& id) {
  m_loadedSectors.remove(id);
  m_sectors(id[0], id[1]).reset();
}

template <typename ElementT, std::size_t SectorSize>
auto SectorArray2D<ElementT, SectorSize>::get(
    std::size_t x, std::size_t y) const -> std::optional<std::reference_wrapper<Element const>> {
  if (auto const& array = m_sectors(x / SectorSize, y / SectorSize))
    return std::reference_wrapper<Element const>((*array)(x % SectorSize, y % SectorSize));
  return std::nullopt;
}

template <typename ElementT, std::size_t SectorSize>
auto SectorArray2D<ElementT, SectorSize>::get(std::size_t x, std::size_t y) -> std::optional<std::reference_wrapper<Element>> {
  if (auto& array = m_sectors(x / SectorSize, y / SectorSize))
    return std::reference_wrapper<Element>((*array)(x % SectorSize, y % SectorSize));
  return std::nullopt;
}

template <typename ElementT, std::size_t SectorSize>
template <typename Function>
std::expected<bool, StarException> SectorArray2D<ElementT, SectorSize>::eval(
    std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty) const {
  return const_cast<SectorArray2D&>(*this).evalPriv(minX, minY, width, height,
      [&function](std::size_t x, std::size_t y, std::optional<std::reference_wrapper<Element>> element) {
        std::optional<std::reference_wrapper<Element const>> value;
        if (element) value = std::reference_wrapper<Element const>(element->get());
        return function(x, y, value);
      }, evalEmpty);
}

template <typename ElementT, std::size_t SectorSize>
template <typename Function>
std::expected<bool, StarException> SectorArray2D<ElementT, SectorSize>::eval(
    std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty) {
  return evalPriv(minX, minY, width, height, std::forward<Function>(function), evalEmpty);
}

template <typename ElementT, std::size_t SectorSize>
template <typename Function>
std::expected<bool, StarException> SectorArray2D<ElementT, SectorSize>::evalColumns(
    std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty) const {
  return const_cast<SectorArray2D&>(*this).evalColumnsPriv(minX, minY, width, height,
      [&function](std::size_t x, std::size_t y, std::optional<std::span<Element>> column, std::size_t size) {
        std::optional<std::span<Element const>> values;
        if (column) values = *column;
        return function(x, y, values, size);
      }, evalEmpty);
}

template <typename ElementT, std::size_t SectorSize>
template <typename Function>
std::expected<bool, StarException> SectorArray2D<ElementT, SectorSize>::evalColumns(
    std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty) {
  return evalColumnsPriv(minX, minY, width, height, std::forward<Function>(function), evalEmpty);
}

template <typename ElementT, std::size_t SectorSize>
template <typename Function>
std::expected<bool, StarException> SectorArray2D<ElementT, SectorSize>::evalColumnsParallel(
    WorkerPool& workers, std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty) const {
  return const_cast<SectorArray2D&>(*this).evalColumnsPrivPar(workers, minX, minY, width, height,
      [&function](std::size_t x, std::size_t y, std::optional<std::span<Element>> column, std::size_t size) {
        std::optional<std::span<Element const>> values;
        if (column) values = *column;
        return function(x, y, values, size);
      }, evalEmpty);
}

template <typename ElementT, std::size_t SectorSize>
template <typename Function>
std::expected<bool, StarException> SectorArray2D<ElementT, SectorSize>::evalColumnsParallel(
    WorkerPool& workers, std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty) {
  return evalColumnsPrivPar(workers, minX, minY, width, height, std::forward<Function>(function), evalEmpty);
}

template <typename ElementT, std::size_t SectorSize>
template <typename Function>
std::expected<bool, StarException> SectorArray2D<ElementT, SectorSize>::evalPriv(
    std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty) {
  return evalColumnsPriv(minX,
      minY,
      width,
      height,
      [&function](std::size_t x, std::size_t y, std::optional<std::span<Element>> column, std::size_t columnSize) -> std::expected<bool, StarException> {
        for (std::size_t i = 0; i < columnSize; ++i) {
          std::optional<std::reference_wrapper<Element>> element;
          if (column) element = std::reference_wrapper<Element>((*column)[i]);
          auto result = function(x, y + i, element);
          if (!result)
            return std::unexpected(std::move(result.error()));
          if (!*result)
            return false;
        }
        return true;
      },
      evalEmpty);
}

template <typename ElementT, std::size_t SectorSize>
template <typename Function>
std::expected<bool, StarException> SectorArray2D<ElementT, SectorSize>::evalColumnsPriv(
    std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty) {
  if (width == 0 || height == 0)
    return true;

  std::size_t maxX = minX + width;
  std::size_t maxY = minY + height;
  std::size_t minXSector = minX / SectorSize;
  std::size_t maxXSector = (maxX - 1) / SectorSize;

  std::size_t minYSector = minY / SectorSize;
  std::size_t maxYSector = (maxY - 1) / SectorSize;

  for (std::size_t xSector = minXSector; xSector <= maxXSector; ++xSector) {
    std::size_t minXi = 0;
    if (xSector == minXSector)
      minXi = minX % SectorSize;

    std::size_t maxXi = SectorSize - 1;
    if (xSector == maxXSector)
      maxXi = (maxX - 1) % SectorSize;

    std::size_t x_ = xSector * SectorSize;

    for (std::size_t ySector = minYSector; ySector <= maxYSector; ++ySector) {
      auto& array = m_sectors(xSector, ySector);

      if (!array && !evalEmpty)
        continue;

      std::size_t minYi = 0;
      if (ySector == minYSector)
        minYi = minY % SectorSize;

      std::size_t maxYi = SectorSize - 1;
      if (ySector == maxYSector)
        maxYi = (maxY - 1) % SectorSize;

      std::size_t y_ = ySector * SectorSize;

      if (!array) {
        for (std::size_t xi = minXi; xi <= maxXi; ++xi) {
          auto result = function(xi + x_, minYi + y_, std::optional<std::span<Element>>{}, maxYi - minYi + 1);
          if (!result)
            return std::unexpected(std::move(result.error()));
          if (!*result)
            return false;
        }
      } else {
        for (std::size_t xi = minXi; xi <= maxXi; ++xi) {
          auto result = function(xi + x_, minYi + y_, std::optional{std::span(array->elements).subspan(xi * SectorSize + minYi, maxYi - minYi + 1)}, maxYi - minYi + 1);
          if (!result)
            return std::unexpected(std::move(result.error()));
          if (!*result)
            return false;
        }
      }
    }
  }

  return true;
}

// VT: This is about as good as I can get it, I think it's bound by memory bandwidth when it's called with
// huge width and height in the renderer (tiles, lighting etc.), should probably use compute shader and GPU
// pipeline in the future.
template <typename ElementT, std::size_t SectorSize>
template <typename Function>
std::expected<bool, StarException> SectorArray2D<ElementT, SectorSize>::evalColumnsPrivPar(
    WorkerPool& workers, std::size_t minX, std::size_t minY, std::size_t width, std::size_t height, Function&& function, bool evalEmpty) {
  if (width == 0 || height == 0)
    return true;
  if (workers.isWorkerThread() || workers.getWorkerCount() == 0)
    return evalColumnsPriv(minX, minY, width, height, std::forward<Function>(function), evalEmpty);

  std::size_t maxX = minX + width;
  std::size_t maxY = minY + height;
  std::size_t minXSector = minX / SectorSize;
  std::size_t maxXSector = (maxX - 1) / SectorSize;

  std::size_t minYSector = minY / SectorSize;
  std::size_t maxYSector = (maxY - 1) / SectorSize;

  std::size_t sectorCount = maxXSector - minXSector + 1;

  std::vector<WorkerPoolPromise<bool>> futures;
  futures.reserve(sectorCount);

  for (std::size_t xSector = minXSector; xSector <= maxXSector; ++xSector) {
    std::size_t minXi = 0;
    if (xSector == minXSector)
      minXi = minX % SectorSize;

    std::size_t maxXi = SectorSize - 1;
    if (xSector == maxXSector)
      maxXi = (maxX - 1) % SectorSize;

    std::size_t x_ = xSector * SectorSize;

    futures.push_back(workers.addProducer<bool>([=, &function, this]() -> std::expected<bool, StarException> {
      for (std::size_t ySector = minYSector; ySector <= maxYSector; ++ySector) {
        auto& array = m_sectors(xSector, ySector);

        if (!array && !evalEmpty)
          continue;

        std::size_t minYi = 0;
        if (ySector == minYSector)
          minYi = minY % SectorSize;

        std::size_t maxYi = SectorSize - 1;
        if (ySector == maxYSector)
          maxYi = (maxY - 1) % SectorSize;

        std::size_t y_ = ySector * SectorSize;

        if (!array) {
          for (std::size_t xi = minXi; xi <= maxXi; ++xi) {
            auto result = function(xi + x_, minYi + y_, std::optional<std::span<Element>>{}, maxYi - minYi + 1);
            if (!result)
              return std::unexpected(std::move(result.error()));
            if (!*result)
              return false;
          }
        } else {
          for (std::size_t xi = minXi; xi <= maxXi; ++xi) {
            auto result = function(xi + x_, minYi + y_, std::optional{std::span(array->elements).subspan(xi * SectorSize + minYi, maxYi - minYi + 1)}, maxYi - minYi + 1);
            if (!result)
              return std::unexpected(std::move(result.error()));
            if (!*result)
              return false;
          }
        }
      }
      return true;
      }));
  }
  bool result = true;
  std::optional<StarException> firstError;
  // Every worker borrows this array and function. Drain all jobs even after
  // enqueue or callback failure before either borrowed object can disappear.
  for (auto const& future : futures) {
    auto completion = future.get();
    if (!completion) {
      if (!firstError)
        firstError.emplace(std::move(completion.error()));
    } else {
      result = completion->get() && result;
    }
  }
  if (firstError)
    return std::unexpected(std::move(*firstError));
  return result;
}


}

export module star.sector_array_2d;

export namespace Star {
  using ::Star::SectorArray2D;
}
