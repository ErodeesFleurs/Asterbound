module;

import std;
import star.format;
import star.list;
import star.vector;

import star.math_common;
namespace Star {

// Scalar and RGB are the two point-kernel instantiations provided by this module.
struct ScalarLightTraits {
  using Value = float;

  static float subtract(float value, float drop) { return std::max(value - drop, 0.0f); }
  static float max_intensity(float value) { return value; }
  static float max(float left, float right) { return std::max(left, right); }
};

struct ColoredLightTraits {
  using Value = Vec3F;

  static Vec3F subtract(Vec3F value, float drop);
  static float max_intensity(Vec3F const& value) { return value.max(); }
  static Vec3F max(Vec3F const& left, Vec3F const& right) { return vmax(left, right); }
};

template <typename Traits>
concept light_traits = std::same_as<Traits, ScalarLightTraits> || std::same_as<Traits, ColoredLightTraits>;

// Only validated parameter sets can be applied. The direct array API and both
// JSON calculators consequently share the same range and conversion rules.
class LightParameters {
public:
  [[nodiscard]] static std::expected<LightParameters, StarException> create(std::int64_t passes,
      float spread_air, float spread_obstacle, float point_air, float point_obstacle, float boost, bool additive);

private:
  template <light_traits> friend class CellularLightArray;
  LightParameters() = default;
  unsigned spread_passes = 0;
  float spread_max_air = 1.0f;
  float spread_max_obstacle = 1.0f;
  float point_max_air = 1.0f;
  float point_max_obstacle = 1.0f;
  float spread_drop_air = 1.0f;
  float spread_drop_obstacle = 1.0f;
  float spread_drop_air_diag = std::numbers::sqrt2;
  float spread_drop_obstacle_diag = std::numbers::sqrt2;
  float point_obstacle_boost = 0.0f;
  bool point_additive = false;
  std::size_t spread_border = 1;
  std::size_t border = 1;
};

template <light_traits LightTraits>
class CellularLightArray {
public:
  using LightValue = typename LightTraits::Value;

  CellularLightArray() = default;
  CellularLightArray(CellularLightArray&&) noexcept = default;
  CellularLightArray& operator=(CellularLightArray&&) noexcept = default;

  struct Cell {
    LightValue light;
    bool obstacle;
  };

  struct SpreadLight {
    Vec2F position;
    LightValue value;
  };

  struct PointLight {
    Vec2F position;
    LightValue value;
    float beam;
    float beam_angle;
    float beam_ambience;
    bool as_spread;
  };

  void set_parameters(LightParameters const& parameters) { m_parameters = parameters; }

  // The border around the target lighting array where initial lighting / light
  // source data is required.  Based on parameters.
  std::size_t border_cells() const { return m_parameters.border; }

  // Retain capacity across shape changes. A rejected size leaves the current frame intact.
  [[nodiscard]] std::expected<void, StarException> begin(std::size_t width, std::size_t height);

  // Position is in index space, spread lights will have no effect if they are
  // outside of the array.  Integer points are assumed to be on the corners of
  // the grid (not the center)
  void add_spread_light(SpreadLight const& spread_light);
  void add_point_light(PointLight const& point_light);

  // Directly set the lighting values for this position.
  void set_light(std::size_t x, std::size_t y, LightValue const& light);

  // Get current light value.  Call after calling calculate() to pull final
  // data out.
  LightValue get_light(std::size_t x, std::size_t y) const;

  // Set obstacle values for this position
  void set_obstacle(std::size_t x, std::size_t y, bool obstacle);
  bool get_obstacle(std::size_t x, std::size_t y) const;

  Cell const& cell(std::size_t x, std::size_t y) const;
  Cell& cell(std::size_t x, std::size_t y);

  Cell const& cell_at_index(std::size_t index) const;
  Cell& cell_at_index(std::size_t index);

  // Calculate within the half-open rectangle clipped to the grid. Supply
  // initial light and obstacle data for its ambient border as well.
  void calculate(std::size_t x_min, std::size_t y_min, std::size_t x_max, std::size_t y_max);

private:
  // Set 4 points based on interpolated light position and free space
  // attenuation.
  void set_spread_lighting_points();

  // Spreads light out in an octagonal based cellular automata
  void calculate_light_spread(std::size_t xmin, std::size_t ymin, std::size_t xmax, std::size_t ymax);

  // Loops through each light and adds light strength based on distance and
  // obstacle attenuation.  Calculates within the given sub-rect
  void calculate_point_lighting(std::size_t xmin, std::size_t ymin, std::size_t xmax, std::size_t ymax);

  std::size_t m_width = 0;
  std::size_t m_height = 0;
  std::vector<Cell> m_cells;
  List<SpreadLight> m_spread_lights;
  List<PointLight> m_point_lights;
  LightParameters m_parameters;
};

using ColoredCellularLightArray = CellularLightArray<ColoredLightTraits>;
using ScalarCellularLightArray = CellularLightArray<ScalarLightTraits>;

inline Vec3F ColoredLightTraits::subtract(Vec3F c, float drop) {
  float max = std::max(std::max(c[0], c[1]), c[2]);
  if (max <= 0.0f)
    return c;

  for (std::size_t i = 0; i < 3; ++i) {
    float pdrop = (drop * c[i]) / max;
    if (c[i] > pdrop)
      c[i] -= pdrop;
    else
      c[i] = 0;
  }
  return c;
}

inline std::expected<LightParameters, StarException> LightParameters::create(std::int64_t passes,
    float spread_air, float spread_obstacle, float point_air, float point_obstacle, float boost, bool additive) {
  if (!std::in_range<unsigned>(passes))
    return std::unexpected(StarException("Light spread passes must be representable as unsigned"));
  auto finite = classify_finite(spread_air, spread_obstacle, point_air, point_obstacle, boost);
  if (!std::ranges::all_of(finite, std::identity{}) || spread_air <= 0 || spread_obstacle <= 0
      || point_air <= 0 || point_obstacle <= 0 || boost < 0)
    return std::unexpected(StarException("Lighting requires finite positive distances and a finite nonnegative obstacle boost"));

  double border = std::ceil(static_cast<double>(std::max(spread_air, point_air)));
  if (border > std::numeric_limits<int>::max())
    return std::unexpected(StarException("Light border exceeds the coordinate range"));

  LightParameters result;
  result.spread_passes = static_cast<unsigned>(passes);
  result.spread_max_air = spread_air;
  result.spread_max_obstacle = spread_obstacle;
  result.point_max_air = point_air;
  result.point_max_obstacle = point_obstacle;
  result.spread_drop_air = 1.0f / spread_air;
  result.spread_drop_obstacle = 1.0f / spread_obstacle;
  result.spread_drop_air_diag = result.spread_drop_air * std::numbers::sqrt2;
  result.spread_drop_obstacle_diag = result.spread_drop_obstacle * std::numbers::sqrt2;
  float point_drop_air = 1.0f / point_air;
  float point_drop_obstacle = 1.0f / point_obstacle;
  if (!std::ranges::all_of(classify_finite(result.spread_drop_air_diag, result.spread_drop_obstacle_diag,
          point_drop_air, point_drop_obstacle), std::identity{}))
    return std::unexpected(StarException("Light attenuation exceeds the floating-point range"));
  result.point_obstacle_boost = boost;
  result.point_additive = additive;
  result.spread_border = static_cast<std::size_t>(std::ceil(spread_air));
  result.border = static_cast<std::size_t>(border);
  return result;
}

template <light_traits LightTraits>
std::expected<void, StarException> CellularLightArray<LightTraits>::begin(std::size_t width, std::size_t height) {
  if (!width || !height || !std::in_range<int>(width) || !std::in_range<int>(height)
      || width > m_cells.max_size() / height)
    return std::unexpected(StarException("Invalid cellular lighting dimensions"));

  m_cells.assign(width * height, Cell{});
  m_width = width;
  m_height = height;
  m_spread_lights.clear();
  m_point_lights.clear();
  return {};
}

template <light_traits LightTraits>
void CellularLightArray<LightTraits>::add_spread_light(SpreadLight const& spread_light) {
  m_spread_lights.push_back(spread_light);
}

template <light_traits LightTraits>
void CellularLightArray<LightTraits>::add_point_light(PointLight const& point_light) {
  m_point_lights.push_back(point_light);
}

template <light_traits LightTraits>
void CellularLightArray<LightTraits>::set_light(std::size_t x, std::size_t y, LightValue const& light_value) {
  cell(x, y).light = light_value;
}

template <light_traits LightTraits>
void CellularLightArray<LightTraits>::set_obstacle(std::size_t x, std::size_t y, bool obstacle) {
  cell(x, y).obstacle = obstacle;
}

template <light_traits LightTraits>
auto CellularLightArray<LightTraits>::get_light(std::size_t x, std::size_t y) const -> LightValue {
  return cell(x, y).light;
}

template <light_traits LightTraits>
bool CellularLightArray<LightTraits>::get_obstacle(std::size_t x, std::size_t y) const {
  return cell(x, y).obstacle;
}

template <light_traits LightTraits>
auto CellularLightArray<LightTraits>::cell(std::size_t x, std::size_t y) const -> Cell const & {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((x < m_width && y < m_height));
  }
  return m_cells[x * m_height + y];
}

template <light_traits LightTraits>
auto CellularLightArray<LightTraits>::cell(std::size_t x, std::size_t y) -> Cell & {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((x < m_width && y < m_height));
  }
  return m_cells[x * m_height + y];
}

template <light_traits LightTraits>
auto CellularLightArray<LightTraits>::cell_at_index(std::size_t index) const -> Cell const & {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((index < m_width * m_height));
  }
  return m_cells[index];
}

template <light_traits LightTraits>
auto CellularLightArray<LightTraits>::cell_at_index(std::size_t index) -> Cell & {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((index < m_width * m_height));
  }
  return m_cells[index];
}

template <light_traits LightTraits>
void CellularLightArray<LightTraits>::calculate(std::size_t x_min, std::size_t y_min, std::size_t x_max, std::size_t y_max) {
  x_max = std::min(x_max, m_width);
  y_max = std::min(y_max, m_height);
  if (m_cells.empty() || x_min >= x_max || y_min >= y_max)
    return;
  set_spread_lighting_points();
  calculate_light_spread(x_min, y_min, x_max, y_max);
  calculate_point_lighting(x_min, y_min, x_max, y_max);
}

template <light_traits LightTraits>
void CellularLightArray<LightTraits>::set_spread_lighting_points() {
  for (SpreadLight const& light : m_spread_lights) {
    // Retain the original float subtraction/floor near half-cell boundaries.
    // Check the bounded footprint before converting to int (including remote sources).
    auto const& position = light.position;
    auto finite = classify_finite(position[0], position[1]);
    if (!finite[0] || !finite[1])
      continue;
    float base_x = std::floor(position[0] - 0.5f);
    float base_y = std::floor(position[1] - 0.5f);
    if (base_x < -1 || base_y < -1 || static_cast<double>(base_x) >= m_width
        || static_cast<double>(base_y) >= m_height)
      continue;
    int min_x = static_cast<int>(base_x);
    int min_y = static_cast<int>(base_y);
    float xdist = position[0] - min_x - 0.5f;
    float ydist = position[1] - min_y - 0.5f;
    int nearest_x = static_cast<int>(std::floor(position[0]));
    int nearest_y = static_cast<int>(std::floor(position[1]));
    auto inside = [&](int x, int y) {
      return static_cast<unsigned>(x) < m_width && static_cast<unsigned>(y) < m_height;
    };
    float attenuation = inside(nearest_x, nearest_y) && get_obstacle(nearest_x, nearest_y)
        ? m_parameters.spread_drop_obstacle : m_parameters.spread_drop_air;
    auto seed = [&](int x, int y, float xweight, float yweight) {
      if (inside(x, y)) {
        auto& dest = cell(x, y).light;
        dest = LightTraits::max(dest, LightTraits::subtract(light.value, attenuation * (2.0f - xweight - yweight)));
      }
    };
    seed(min_x, min_y, 1.0f - xdist, 1.0f - ydist);
    seed(min_x, min_y + 1, 1.0f - xdist, ydist);
    seed(min_x + 1, min_y, xdist, 1.0f - ydist);
    seed(min_x + 1, min_y + 1, xdist, ydist);
  }
}

template <light_traits LightTraits>
void CellularLightArray<LightTraits>::calculate_light_spread(std::size_t x_min, std::size_t y_min, std::size_t x_max, std::size_t y_max) {
  if (!m_parameters.spread_passes)
    return;
  auto border = m_parameters.spread_border;
  x_min -= std::min(x_min, border);
  y_min -= std::min(y_min, border);
  x_max += std::min(m_width - x_max, border);
  y_max += std::min(m_height - y_max, border);
  if (x_max - x_min < 3 || y_max - y_min < 3)
    return;

  float air = m_parameters.spread_drop_air;
  float obstacle = m_parameters.spread_drop_obstacle;
  float air_diag = m_parameters.spread_drop_air_diag;
  float obstacle_diag = m_parameters.spread_drop_obstacle_diag;
  auto propagate = [=](Cell const& source, Cell& straight_a, Cell& straight_b, Cell& diagonal_a, Cell& diagonal_b) {
    float straight_drop = source.obstacle ? obstacle : air;
    float diagonal_drop = source.obstacle ? obstacle_diag : air_diag;
    auto combine = [&](LightValue const& straight, LightValue const& diagonal) {
      straight_a.light = LightTraits::max(straight, straight_a.light);
      straight_b.light = LightTraits::max(straight, straight_b.light);
      diagonal_a.light = LightTraits::max(diagonal, diagonal_a.light);
      diagonal_b.light = LightTraits::max(diagonal, diagonal_b.light);
    };
    if constexpr (std::same_as<LightTraits, ScalarLightTraits>) {
      combine(source.light - straight_drop, source.light - diagonal_drop);
    } else {
      auto const& color = source.light;
      float peak = std::max(color[0], std::max(color[1], color[2]));
      if (peak <= 0.0f)
        return;
      // Spread divides first, then subtracts channel * drop. Point subtraction
      // multiplies first and clamps at zero; those are not interchangeable.
      auto candidate = [&](float drop) {
        drop /= peak;
        return Vec3F(color[0] - color[0] * drop, color[1] - color[1] * drop, color[2] - color[2] * drop);
      };
      combine(candidate(straight_drop), candidate(diagonal_drop));
    }
  };

  // The clipped rectangle has an interior and its one-cell halo. Slice
  // columns once, rather than checking five full-grid offsets at every cell.
  auto height = y_max - y_min;
  auto column = [&](std::size_t x) {
    return std::span{m_cells}.subspan(x * m_height + y_min, height);
  };
  auto sweep_column = [&]<bool reverse>(std::size_t x) {
    auto source = column(x);
    auto target = column(reverse ? x - 1 : x + 1);
    auto spread = [&](std::size_t y) {
      auto ahead = reverse ? y - 1 : y + 1;
      auto behind = reverse ? y + 1 : y - 1;
      propagate(source[y], target[y], source[ahead], target[ahead], target[behind]);
    };
    if constexpr (reverse) {
      for (std::size_t y = height - 2; y != 0; --y)
        spread(y);
    } else {
      for (std::size_t y = 1; y < height - 1; ++y)
        spread(y);
    }
  };
  for (unsigned p = 0; p < m_parameters.spread_passes; ++p) {
    for (std::size_t x = x_min + 1; x < x_max - 1; ++x)
      sweep_column.template operator()<false>(x);
    for (std::size_t x = x_max - 2; x > x_min; --x)
      sweep_column.template operator()<true>(x);
  }
}

}

export module star.cellular_light_array;

export namespace Star {
  using ::Star::ScalarLightTraits;
  using ::Star::ColoredLightTraits;
  using ::Star::light_traits;
  using ::Star::LightParameters;
  using ::Star::CellularLightArray;
  using ::Star::ColoredCellularLightArray;
  using ::Star::ScalarCellularLightArray;
}
