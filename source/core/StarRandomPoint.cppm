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
  Random2dPointGenerator(RandomService& random, std::uint64_t seed, float cell_size, Vec2I const& density_range);

  // Immutable configuration is validated once. Invalid configuration is reported
  // here before arithmetic, cache mutation, random draws or callback invocation.
  template <typename PointCallback>
    requires point_callback<PointCallback, PointData>
  [[nodiscard]] std::expected<PointSet, StarException> generate(Poly const& area, PointCallback&& callback);

private:
  HashTtlCache<Sector, PointSet> cache_;

  std::uint64_t seed_;
  float cell_size_;
  Vec2I density_range_;
  bool cell_size_valid_;
  bool density_range_valid_;
};

template <typename PointData, finite_classifiable DataType>
  requires std::is_constructible_v<PointData, PointData const&>
inline Random2dPointGenerator<PointData, DataType>::Random2dPointGenerator(RandomService& random, std::uint64_t seed, float cell_size, Vec2I const& density_range)
  : cache_(random), seed_(seed), cell_size_(cell_size), density_range_(density_range),
    cell_size_valid_(is_finite(cell_size) && cell_size > 0),
    density_range_valid_(density_range[0] >= 0 && density_range[1] >= density_range[0]) {}

template <typename PointData, finite_classifiable DataType>
  requires std::is_constructible_v<PointData, PointData const&>
template <typename PointCallback>
  requires point_callback<PointCallback, PointData>
auto Random2dPointGenerator<PointData, DataType>::generate(Poly const& area, PointCallback&& callback) -> std::expected<PointSet, StarException> {
  if (!cell_size_valid_)
    return std::unexpected(StarException("Point generator cell size must be finite and positive"));
  if (!density_range_valid_)
    return std::unexpected(StarException("Point generator density must be ordered and nonnegative"));
  for (auto const& vertex : area.vertices()) {
    auto finite = classify_finite(vertex[0], vertex[1]);
    if (!finite[0] || !finite[1])
      return std::unexpected(StarException("Point generator area coordinates must be finite"));
  }
  PointSet result;
  if (area.empty()) {
    cache_.cleanup();
    return result;
  }

  auto bound = area.bounding_box();
  auto sector_coordinate = [&](DataType coordinate, bool upper) -> std::expected<std::int64_t, StarException> {
    auto quotient = coordinate / cell_size_;
    if (!is_finite(quotient))
      return std::unexpected(StarException("Point generator sector coordinate overflow"));
    auto rounded = upper ? std::ceil(quotient) : std::floor(quotient);
    // The positive int64 endpoint rounds to 2^63 in floating point, so the
    // upper check must be exclusive, not a comparison to converted INT64_MAX.
    if (rounded < -0x1p63 || rounded >= 0x1p63)
      return std::unexpected(StarException("Point generator sector coordinate is outside int64 range"));
    return static_cast<std::int64_t>(rounded);
  };
  auto x_min = sector_coordinate(bound.x_min(), false);
  auto y_min = sector_coordinate(bound.y_min(), false);
  auto x_max = sector_coordinate(bound.x_max(), true);
  auto y_max = sector_coordinate(bound.y_max(), true);
  if (!x_min) return std::unexpected(std::move(x_min.error()));
  if (!y_min) return std::unexpected(std::move(y_min.error()));
  if (!x_max) return std::unexpected(std::move(x_max.error()));
  if (!y_max) return std::unexpected(std::move(y_max.error()));

  auto sector_extent = [](std::int64_t lower, std::int64_t upper) -> std::expected<std::size_t, StarException> {
    auto difference = static_cast<std::uint64_t>(upper) - static_cast<std::uint64_t>(lower);
    if (upper < lower || difference >= std::numeric_limits<std::size_t>::max())
      return std::unexpected(StarException("Point generator sector extent overflow"));
    return static_cast<std::size_t>(difference) + 1;
  };
  auto width = sector_extent(*x_min, *x_max);
  auto height = sector_extent(*y_min, *y_max);
  if (!width) return std::unexpected(std::move(width.error()));
  if (!height) return std::unexpected(std::move(height.error()));
  if (*width > std::numeric_limits<std::size_t>::max() / *height)
    return std::unexpected(StarException("Point generator sector count overflow"));
  auto sector_count = *width * *height;
  auto max_density = static_cast<std::size_t>(density_range_[1]);
  if (max_density > result.max_size() || (max_density && sector_count > result.max_size() / max_density))
    return std::unexpected(StarException("Point generator point reserve overflow"));

  // Check extrema before narrowing or multiplying in the coordinate type.
  // Long double has ample range for int64 * finite float on supported targets.
  auto valid_axis = [&](std::int64_t lower, std::int64_t upper) {
    auto minimum = static_cast<long double>(static_cast<DataType>(lower)) * cell_size_;
    auto maximum = (static_cast<long double>(static_cast<DataType>(upper)) + 1) * cell_size_;
    auto limit = static_cast<long double>(std::numeric_limits<DataType>::max());
    if (minimum < -limit || maximum > limit)
      return false;
    auto origin = static_cast<DataType>(upper) * cell_size_;
    return static_cast<long double>(origin) + cell_size_ <= limit;
  };
  if (!valid_axis(*x_min, *x_max) || !valid_axis(*y_min, *y_max))
    return std::unexpected(StarException("Point generator cell coordinate overflow"));

  // Eager reservation avoids growth for small dense queries. A thin diagonal can
  // intersect O(n) cells inside an O(n²) box: never speculate unbounded storage.
  constexpr std::size_t eager_reserve_bytes = 4096;
  auto maximum_points = sector_count * max_density;
  if (maximum_points <= eager_reserve_bytes / sizeof(typename PointSet::value_type))
    result.reserve(maximum_points);
  cache_.cleanup();
  // Break before incrementing inclusive endpoints, including INT64_MAX.
  for (std::int64_t x = *x_min;;) {
    for (std::int64_t y = *y_min;;) {
      auto sector = Rect::with_size({static_cast<DataType>(x) * cell_size_, static_cast<DataType>(y) * cell_size_}, Point::filled(cell_size_));
      if (area.intersects_rect(sector)) {
        auto points = cache_.get(Sector(x, y), [&](Sector const&) -> std::expected<PointSet, StarException> {
          PointSet sector_points;
          RandomSource sector_random(static_random_u64(seed_, x, y));
          auto count = sector_random.uniform_int(density_range_[0], density_range_[1]);
          if (!count)
            return std::unexpected(std::move(count.error()));
          sector_points.reserve(static_cast<std::size_t>(*count));
          for (std::int64_t i = 0; i < *count; ++i) {
            // Pinned native order: x, y, then the callback on the same stream.
            auto x_offset = static_cast<DataType>(sector_random.uniform_double());
            auto y_offset = static_cast<DataType>(sector_random.uniform_double());
            auto point = Point(x + x_offset, y + y_offset) * cell_size_;
            if constexpr (RandomPointDetail::CallbackResult<PointData, std::invoke_result_t<PointCallback&, RandomSource&>>::checked) {
              auto data = std::invoke(callback, sector_random);
              if (!data)
                return std::unexpected(std::move(data.error()));
              sector_points.emplace_back(point, std::move(*data));
            } else {
              sector_points.emplace_back(point, std::invoke(callback, sector_random));
            }
          }
          return sector_points;
        });
        if (!points)
          return std::unexpected(std::move(points.error()));
        result.append_range(std::as_const(points->get()));
      }
      if (y == *y_max)
        break;
      ++y;
    }
    if (x == *x_max)
      break;
    ++x;
  }
  return result;
}

}

export module star.random_point;

export namespace Star {
  using ::Star::point_callback;
  using ::Star::Random2dPointGenerator;
}
