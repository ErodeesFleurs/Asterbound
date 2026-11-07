module;
import std;
import star.format;
import star.list;

import star.random;
import star.poly;
import star.lru_cache;
import star.time;
import star.ttl_cache;

import star.static_random;
import star.vector;
import star.rect;
namespace Star {

// An "infinite" generator of points on a 2d plane, generated cell by cell with
// an upper and lower cell density range.  Each point is generated in a
// predictable way sector by sector, as long as the generator function is
// predictable and uses the RandomSource in a predictable way.  Useful for
// things like starfields, fields of debris, random object placement, etc.

template <typename PointData, typename DataType = float>
class Random2dPointGenerator {
public:
  using Poly = Star::Polygon<DataType>;
  using Point = Star::Vector<DataType, 2>;
  using Rect = Star::Rect<DataType>;
  using PointSet = List<std::pair<Point, PointData>>;
  Random2dPointGenerator(RandomService& random, std::uint64_t seed, float cellSize, Vec2I const& densityRange);

  // Each point will in the area will be generated in a predictable order, and
  // if the callback uses the RandomSource in a predictable way, will generate
  // the same field for every call.
  template <typename PointCallback>
  [[nodiscard]] std::expected<PointSet, StarException> generate(Poly const& area, PointCallback callback);

private:
  HashTtlCache<Point, PointSet> m_cache;

  std::uint64_t m_seed;
  float m_cellSize;
  Vec2I m_densityRange;
};

template <typename PointData, typename DataType>
inline Random2dPointGenerator<PointData, DataType>::Random2dPointGenerator(RandomService& random, std::uint64_t seed, float cellSize, Vec2I const& densityRange)
  : m_cache(random), m_seed(seed), m_cellSize(cellSize), m_densityRange(densityRange) {}

template <typename PointData, typename DataType>
template <typename PointCallback>
auto Random2dPointGenerator<PointData, DataType>::generate(Poly const& area, PointCallback callback) -> std::expected<PointSet, StarException> {
  auto bound = area.boundBox();
  std::int64_t sectorXMin = std::floor(bound.xMin() / m_cellSize);
  std::int64_t sectorYMin = std::floor(bound.yMin() / m_cellSize);
  std::int64_t sectorXMax = std::ceil(bound.xMax() / m_cellSize);
  std::int64_t sectorYMax = std::ceil(bound.yMax() / m_cellSize);
  std::int64_t sectorCount = (sectorXMax - sectorXMin + 1) * (sectorYMax - sectorYMin + 1);

  PointSet finalResult;
  // VT: Reserve space for worst case scenario, this gives ~2x speedup
  finalResult.reserve(sectorCount * m_densityRange[1]); 

  for (std::int64_t x = sectorXMin; x <= sectorXMax; ++x) {
    for (std::int64_t y = sectorYMin; y <= sectorYMax; ++y) {
      auto sector = Rect::withSize({x * m_cellSize, y * m_cellSize}, Point::filled(m_cellSize));
      if (!area.intersects(Poly(sector)))
        continue;

      auto points = m_cache.get(Point(x, y), [&](Point const&) -> std::expected<PointSet, StarException> {
          PointSet sectorResult;
          RandomSource sectorRandomness(static_random_u64(m_seed, x, y));
          auto count = sectorRandomness.randInt(m_densityRange[0], m_densityRange[1]);
          if (!count)
            return std::unexpected(std::move(count.error()));
          unsigned max = *count;
          for (unsigned i = 0; i < max; ++i) {
            Point pointPos = Point(x + (DataType)sectorRandomness.randd(), y + (DataType)sectorRandomness.randd()) * m_cellSize;
            if constexpr (std::is_same_v<std::invoke_result_t<PointCallback&, RandomSource&>, std::expected<PointData, StarException>>) {
              auto data = callback(sectorRandomness);
              if (!data)
                return std::unexpected(std::move(data.error()));
              sectorResult.append(std::pair<Point, PointData>(pointPos, std::move(*data)));
            } else {
              sectorResult.append(std::pair<Point, PointData>(pointPos, callback(sectorRandomness)));
            }
          }

          return sectorResult;
        });
      if (!points)
        return std::unexpected(std::move(points.error()));
      finalResult.appendAll(points->get());
    }
  }

  return finalResult;
}

}

export module star.random_point;

export namespace Star {
  using ::Star::Random2dPointGenerator;
}
