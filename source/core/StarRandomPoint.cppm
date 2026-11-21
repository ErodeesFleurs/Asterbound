module;
import std;
import star.format;
import star.list;

import star.random;
import star.poly;
import star.math_common;
import star.ttl_cache;
import star.static_random;
import star.vector;
import star.rect;
namespace Star {

namespace RandomPointDetail {
  template <typename Value, typename Result>
  struct CallbackResult : std::bool_constant<std::is_constructible_v<Value, Result>> {
    static constexpr bool checked = false;
  };

  template <typename Value, typename Result>
  struct CallbackResult<Value, std::expected<Result, StarException>>
    : std::bool_constant<std::is_constructible_v<Value, std::add_rvalue_reference_t<Result>>> {
    static constexpr bool checked = true;
  };
}

template <typename Callback, typename Value>
concept point_callback = std::invocable<Callback&, RandomSource&>
  && RandomPointDetail::CallbackResult<Value, std::invoke_result_t<Callback&, RandomSource&>>::value;

// A deterministic grid-cell field, not exact polygon clipping: all points from
// every intersecting cell are returned, in x/y/point order. Callbacks must be
// deterministic in their use of the per-cell RandomSource.
// The cache keeps the existing sliding 10s TTL with +/-1s random smear and no
// arbitrary capacity limit. Each generate removes expired cells before lookup;
// revisiting an evicted cell re-executes its callback (including side effects).
// Coordinate types match the fast-math-safe finite classifier (float/double).
template <typename PointData, finite_classifiable DataType = float>
  requires std::is_constructible_v<PointData, PointData const&>
class Random2dPointGenerator {
public:
  using Poly = Star::Polygon<DataType>;
  using Point = Star::Vector<DataType, 2>;
  using Rect = Star::Rect<DataType>;
  using Sector = Star::Vector<std::int64_t, 2>;
  using PointSet = List<std::pair<Point, PointData>>;
  Random2dPointGenerator(RandomService& random, std::uint64_t seed, float cellSize, Vec2I const& densityRange);

  // Immutable configuration is validated once. Invalid configuration is reported
  // here before arithmetic, cache mutation, random draws or callback invocation.
  template <typename PointCallback>
    requires point_callback<PointCallback, PointData>
  [[nodiscard]] std::expected<PointSet, StarException> generate(Poly const& area, PointCallback&& callback);

private:
  HashTtlCache<Sector, PointSet> m_cache;

  std::uint64_t m_seed;
  float m_cellSize;
  Vec2I m_densityRange;
  bool m_cellSizeValid;
  bool m_densityRangeValid;
};

template <typename PointData, finite_classifiable DataType>
  requires std::is_constructible_v<PointData, PointData const&>
inline Random2dPointGenerator<PointData, DataType>::Random2dPointGenerator(RandomService& random, std::uint64_t seed, float cellSize, Vec2I const& densityRange)
  : m_cache(random), m_seed(seed), m_cellSize(cellSize), m_densityRange(densityRange),
    m_cellSizeValid(is_finite(cellSize) && cellSize > 0),
    m_densityRangeValid(densityRange[0] >= 0 && densityRange[1] >= densityRange[0]) {}

template <typename PointData, finite_classifiable DataType>
  requires std::is_constructible_v<PointData, PointData const&>
template <typename PointCallback>
  requires point_callback<PointCallback, PointData>
auto Random2dPointGenerator<PointData, DataType>::generate(Poly const& area, PointCallback&& callback) -> std::expected<PointSet, StarException> {
  if (!m_cellSizeValid)
    return std::unexpected(StarException("Point generator cell size must be finite and positive"));
  if (!m_densityRangeValid)
    return std::unexpected(StarException("Point generator density must be ordered and nonnegative"));
  for (auto const& vertex : area.vertices()) {
    auto finite = classify_finite(vertex[0], vertex[1]);
    if (!finite[0] || !finite[1])
      return std::unexpected(StarException("Point generator area coordinates must be finite"));
  }
  PointSet finalResult;
  if (area.empty()) {
    m_cache.cleanup();
    return finalResult;
  }

  auto bound = area.bounding_box();
  auto sector_coordinate = [&](DataType coordinate, bool upper) -> std::expected<std::int64_t, StarException> {
    auto quotient = coordinate / m_cellSize;
    if (!is_finite(quotient))
      return std::unexpected(StarException("Point generator sector coordinate overflow"));
    auto rounded = upper ? std::ceil(quotient) : std::floor(quotient);
    // The positive int64 endpoint rounds to 2^63 in floating point, so the
    // upper check must be exclusive, not a comparison to converted INT64_MAX.
    if (rounded < -0x1p63 || rounded >= 0x1p63)
      return std::unexpected(StarException("Point generator sector coordinate is outside int64 range"));
    return static_cast<std::int64_t>(rounded);
  };
  auto xMin = sector_coordinate(bound.xMin(), false);
  auto yMin = sector_coordinate(bound.yMin(), false);
  auto xMax = sector_coordinate(bound.xMax(), true);
  auto yMax = sector_coordinate(bound.yMax(), true);
  if (!xMin) return std::unexpected(std::move(xMin.error()));
  if (!yMin) return std::unexpected(std::move(yMin.error()));
  if (!xMax) return std::unexpected(std::move(xMax.error()));
  if (!yMax) return std::unexpected(std::move(yMax.error()));

  auto sector_extent = [](std::int64_t lower, std::int64_t upper) -> std::expected<std::size_t, StarException> {
    auto difference = static_cast<std::uint64_t>(upper) - static_cast<std::uint64_t>(lower);
    if (upper < lower || difference >= std::numeric_limits<std::size_t>::max())
      return std::unexpected(StarException("Point generator sector extent overflow"));
    return static_cast<std::size_t>(difference) + 1;
  };
  auto width = sector_extent(*xMin, *xMax);
  auto height = sector_extent(*yMin, *yMax);
  if (!width) return std::unexpected(std::move(width.error()));
  if (!height) return std::unexpected(std::move(height.error()));
  if (*width > std::numeric_limits<std::size_t>::max() / *height)
    return std::unexpected(StarException("Point generator sector count overflow"));
  auto sectorCount = *width * *height;
  auto maxDensity = static_cast<std::size_t>(m_densityRange[1]);
  if (maxDensity > finalResult.max_size() || (maxDensity && sectorCount > finalResult.max_size() / maxDensity))
    return std::unexpected(StarException("Point generator point reserve overflow"));

  // Check extrema before narrowing or multiplying in the coordinate type.
  // Long double has ample range for int64 * finite float on supported targets.
  auto valid_axis = [&](std::int64_t lower, std::int64_t upper) {
    auto minimum = static_cast<long double>(static_cast<DataType>(lower)) * m_cellSize;
    auto maximum = (static_cast<long double>(static_cast<DataType>(upper)) + 1) * m_cellSize;
    auto limit = static_cast<long double>(std::numeric_limits<DataType>::max());
    if (minimum < -limit || maximum > limit)
      return false;
    auto origin = static_cast<DataType>(upper) * m_cellSize;
    return static_cast<long double>(origin) + m_cellSize <= limit;
  };
  if (!valid_axis(*xMin, *xMax) || !valid_axis(*yMin, *yMax))
    return std::unexpected(StarException("Point generator cell coordinate overflow"));

  finalResult.reserve(sectorCount * maxDensity);
  m_cache.cleanup();
  // Break before incrementing inclusive endpoints, including INT64_MAX.
  for (std::int64_t x = *xMin;;) {
    for (std::int64_t y = *yMin;;) {
      auto sector = Rect::withSize({static_cast<DataType>(x) * m_cellSize, static_cast<DataType>(y) * m_cellSize}, Point::filled(m_cellSize));
      if (area.intersects(Poly(sector))) {
        auto points = m_cache.get(Sector(x, y), [&](Sector const&) -> std::expected<PointSet, StarException> {
          PointSet sectorResult;
          RandomSource sectorRandomness(static_random_u64(m_seed, x, y));
          auto count = sectorRandomness.uniform_int(m_densityRange[0], m_densityRange[1]);
          if (!count)
            return std::unexpected(std::move(count.error()));
          sectorResult.reserve(static_cast<std::size_t>(*count));
          for (std::int64_t i = 0; i < *count; ++i) {
            Point pointPos = Point(x + static_cast<DataType>(sectorRandomness.uniform_double()), y + static_cast<DataType>(sectorRandomness.uniform_double())) * m_cellSize;
            if constexpr (RandomPointDetail::CallbackResult<PointData, std::invoke_result_t<PointCallback&, RandomSource&>>::checked) {
              auto data = std::invoke(callback, sectorRandomness);
              if (!data)
                return std::unexpected(std::move(data.error()));
              sectorResult.emplace_back(pointPos, std::move(*data));
            } else {
              sectorResult.emplace_back(pointPos, std::invoke(callback, sectorRandomness));
            }
          }
          return sectorResult;
        });
        if (!points)
          return std::unexpected(std::move(points.error()));
        finalResult.append_range(std::as_const(points->get()));
      }
      if (y == *yMax)
        break;
      ++y;
    }
    if (x == *xMax)
      break;
    ++x;
  }
  return finalResult;
}

}

export module star.random_point;

export namespace Star {
  using ::Star::point_callback;
  using ::Star::Random2dPointGenerator;
}
