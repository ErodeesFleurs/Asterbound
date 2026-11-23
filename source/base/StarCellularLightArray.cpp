import std;
import star.cellular_light_array;
import star.vector;

namespace Star {
namespace {

// The major axis is selected once per ray. The ordinary interior path uses the
// original int arithmetic. Clipped rays use wider signed indices so a
// rounded coordinate at an INT_MAX-sized grid edge is still representable.
template <unsigned major_axis, bool clip, typename Cell>
float wu_axis(std::span<Cell const> cells, std::size_t width, std::size_t height,
    float major1, float minor1, float major2, float minor2, float gradient,
    float per_obstacle, float max_attenuation) {
  using Index = std::conditional_t<clip, std::int64_t, int>;
  auto integer_part = [](auto value) {
    Index whole = static_cast<Index>(value);
    if constexpr (clip)
      return whole - static_cast<Index>(value < static_cast<decltype(value)>(whole));
    else
      return whole; // Interior Wu coordinates are nonnegative.
  };
  auto fractional_part = [&](auto value) -> decltype(value) { return value - integer_part(value); };
  auto reverse_fractional_part = [&](auto value) -> decltype(value) { return 1.0 - fractional_part(value); };
  auto nearest = [](float major) -> Index {
    if constexpr (clip)
      return static_cast<Index>(std::round(major));
    else
      return static_cast<Index>(major + 0.5);
  };
  auto obstacle = [&](Index major, Index minor) {
    auto x = major_axis == 0 ? major : minor;
    auto y = major_axis == 0 ? minor : major;
    if constexpr (clip)
      if (static_cast<std::size_t>(x) >= width || static_cast<std::size_t>(y) >= height)
        return false;
    return cells[static_cast<std::size_t>(x) * height + static_cast<std::size_t>(y)].obstacle;
  };

  if (major2 < major1) {
    std::swap(major1, major2);
    std::swap(minor1, minor2);
  }
  float attenuation = 0.0f;
  auto endpoint = [&](Index major, float minor, float gap) {
    auto base = integer_part(minor);
    if (obstacle(major, base))
      attenuation += reverse_fractional_part(minor) * gap * per_obstacle;
    if (obstacle(major, base + 1))
      attenuation += fractional_part(minor) * gap * per_obstacle;
  };
  // Both endpoints precede the interior recurrence, including saturation.
  Index first = nearest(major1);
  float first_minor = minor1 + gradient * (static_cast<float>(first) - major1);
  endpoint(first, first_minor, reverse_fractional_part(major1 + 0.5));
  if (attenuation >= max_attenuation)
    return max_attenuation;
  float minor = first_minor + gradient;

  Index last = nearest(major2);
  float last_minor = minor2 + gradient * (static_cast<float>(last) - major2);
  endpoint(last, last_minor, fractional_part(major2 + 0.5));
  if (attenuation >= max_attenuation)
    return max_attenuation;

  for (Index major = first + 1; major < last; ++major) {
    Index base = integer_part(minor);
    float part = minor - base;
    float reverse_part = 1.0 - part;
    if (obstacle(major, base))
      attenuation += reverse_part * per_obstacle;
    if (obstacle(major, base + 1))
      attenuation += part * per_obstacle;
    if (attenuation >= max_attenuation)
      return max_attenuation;
    minor += gradient;
  }
  return std::min(attenuation, max_attenuation);
}

template <bool clip, typename Cell>
float line_attenuation(std::span<Cell const> cells, std::size_t width, std::size_t height,
    Vec2F const& start, Vec2F const& end, float per_obstacle, float max_attenuation) {
  float x1 = start[0] - 0.5;
  float y1 = start[1] - 0.5;
  float x2 = end[0] - 0.5;
  float y2 = end[1] - 0.5;
  float dx = x2 - x1;
  float dy = y2 - y1;
  if (std::fabs(dx) < std::fabs(dy))
    return wu_axis<1, clip>(cells, width, height, y1, x1, y2, x2, dx / dy, per_obstacle, max_attenuation);
  if (dx == 0.0f)
    return 0.0f;
  return wu_axis<0, clip>(cells, width, height, x1, y1, x2, y2, dy / dx, per_obstacle, max_attenuation);
}

} // namespace

template <light_traits LightTraits>
void CellularLightArray<LightTraits>::calculate_point_lighting(
    std::size_t x_min, std::size_t y_min, std::size_t x_max, std::size_t y_max) {
  constexpr float beam_cutoff = std::same_as<LightTraits, ScalarLightTraits> ? 0.0001f : 0.0f;
  auto const& params = m_parameters;
  for (auto const& light : m_point_lights) {
    if (light.position[0] < 0 || light.position[0] > m_width - 1
        || light.position[1] < 0 || light.position[1] > m_height - 1)
      continue;

    float max_intensity = LightTraits::max_intensity(light.value);
    float air_range = light.as_spread ? params.spread_max_air : params.point_max_air;
    float per_block_air = 1.0f / air_range;
    float obstacle_range = light.as_spread ? params.spread_max_obstacle : params.point_max_obstacle;
    bool beamed = light.beam > beam_cutoff;
    Vec2F beam_direction = beamed ? Vec2F(1, 0).rotate(light.beam_angle) : Vec2F{};

    // Clip before converting to indices; rounding an integer sub-rectangle
    // limit to float can otherwise round its exclusive end out of the array.
    float range = max_intensity * air_range;
    auto lower = [&](float pos, std::size_t lo, std::size_t hi) -> std::size_t {
      return std::floor(std::clamp<double>(pos - range, lo, hi));
    };
    auto upper = [&](float pos, std::size_t lo, std::size_t hi) -> std::size_t {
      return std::ceil(std::clamp<double>(pos + range, lo, hi));
    };
    auto first_x = lower(light.position[0], x_min, x_max);
    auto first_y = lower(light.position[1], y_min, y_max);
    auto last_x = upper(light.position[0], x_min, x_max);
    auto last_y = upper(light.position[1], y_min, y_max);
    // Classify the entire light footprint just once. For these bounded
    // coordinates each float step has at most 1/256 cell of rounding
    // error; over 128 steps this is <= half a cell. The two-cell inset
    // covers that drift, rounded endpoints, and the second Wu neighbor.
    auto inside = [](double pos, std::size_t first, std::size_t last, std::size_t extent) {
      return pos >= 2 && pos <= extent - 2.0 && first >= 2 && last <= extent - 2.0
          && pos - first <= 126 && last - pos <= 126;
    };
    bool interior = std::max(m_width, m_height) <= 65536
        && inside(light.position[0], first_x, last_x, m_width)
        && inside(light.position[1], first_y, last_y, m_height);
    std::span<Cell const> cells(m_cells);
    for (auto x = first_x; x < last_x; ++x) {
      for (auto y = first_y; y < last_y; ++y) {
        Vec2F block_pos(x + 0.5f, y + 0.5f);
        Vec2F relative_position = block_pos - light.position;
        float distance = relative_position.magnitude();
        if (distance == 0.0f) {
          // At the source center even non-additive and hybrid lights add in full.
          auto& dest = cell(x, y).light;
          dest = light.value + dest;
          continue;
        }

        float attenuation = distance * per_block_air;
        if (attenuation >= 1.0f)
          continue;
        Vec2F direction = relative_position / distance;
        if (beamed) {
          attenuation += (1.0f - light.beam_ambience)
              * std::clamp(light.beam * (1.0f - direction * beam_direction), 0.0f, 1.0f);
          if (attenuation >= 1.0f)
            continue;
        }

        float remaining = max_intensity - attenuation;
        if (remaining <= 0.0f)
          continue;
        float circularized = (1.0f / obstacle_range) / std::max(std::fabs(direction[0]), std::fabs(direction[1]));
        float obstacle = interior
            ? Star::line_attenuation<false>(cells, m_width, m_height, block_pos, light.position, circularized, remaining)
            : Star::line_attenuation<true>(cells, m_width, m_height, block_pos, light.position, circularized, remaining);
        attenuation += obstacle;
        if (!light.as_spread)
          attenuation += std::min(obstacle, circularized) * params.point_obstacle_boost;

        if (attenuation < 1.0f) {
          auto contribution = LightTraits::subtract(light.value, attenuation);
          if (params.point_additive) {
            if (LightTraits::max_intensity(contribution) > 0.0001f) {
              auto& dest = cell(x, y).light;
              dest = dest + (light.as_spread ? contribution * 0.15f : contribution);
            }
          } else {
            auto& dest = cell(x, y).light;
            dest = LightTraits::max(contribution, dest);
          }
        }
      }
    }
  }
}

template void CellularLightArray<ScalarLightTraits>::calculate_point_lighting(
    std::size_t, std::size_t, std::size_t, std::size_t);
template void CellularLightArray<ColoredLightTraits>::calculate_point_lighting(
    std::size_t, std::size_t, std::size_t, std::size_t);

}