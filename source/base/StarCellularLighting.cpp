import std;
import star.format;
import star.string;
import star.json;
import star.rect;
import star.image;
import star.color;
import star.interpolation;
import star.list;
import star.vector;
import star.cellular_light_array;
import star.thread;
import star.math_common;
import star.cellular_lighting;

import star.algorithm;
namespace Star {

Lightmap::Lightmap() : m_width(0), m_height(0) {}

Lightmap::Lightmap(unsigned width, unsigned height) : Lightmap() {
  auto resized = resize_for_overwrite(width, height);
  if (!resized) {
    Star::fatal_error(resized.error().what(), false);
    std::terminate();
  }
  std::ranges::fill(span(), 0.0f);
}

Lightmap::Lightmap(Lightmap const& other) : Lightmap() {
  operator=(other);
}

Lightmap::Lightmap(Lightmap&& other) noexcept : Lightmap() {
  operator=(std::move(other));
}

Lightmap& Lightmap::operator=(Lightmap const& other) {
  if (this == &other)
    return *this;
  auto channels = other.span();
  if (channels.size() > m_capacity) {
    m_data = std::make_unique_for_overwrite<float[]>(channels.size());
    m_capacity = channels.size();
  }
  m_width = other.m_width;
  m_height = other.m_height;
  std::ranges::copy(channels, span().begin());
  return *this;
}

Lightmap& Lightmap::operator=(Lightmap&& other) noexcept {
  if (this == &other)
    return *this;
  m_width = std::exchange(other.m_width, 0);
  m_height = std::exchange(other.m_height, 0);
  m_data = std::move(other.m_data);
  m_capacity = std::exchange(other.m_capacity, 0);
  return *this;
}

void Lightmap::swap(Lightmap& other) noexcept {
  m_data.swap(other.m_data);
  std::swap(m_capacity, other.m_capacity);
  std::swap(m_width, other.m_width);
  std::swap(m_height, other.m_height);
}

std::expected<void, StarException> Lightmap::resize_for_overwrite(unsigned width, unsigned height) {
  auto bytes = Image::size_bytes({width, height}, PixelFormat::RGB_F);
  if (!bytes)
    return std::unexpected(LightmapException(bytes.error().what()));
  auto length = *bytes / sizeof(float);
  if (length > m_capacity) {
    m_data = std::make_unique_for_overwrite<float[]>(length);
    m_capacity = length;
  }
  m_width = width;
  m_height = height;
  return {};
}

Lightmap::operator ImageView() const & {
  ImageView view;
  auto pixels = span();
  view.data = {reinterpret_cast<std::uint8_t const*>(pixels.data()), pixels.size_bytes()};
  view.size = size();
  view.format = PixelFormat::RGB_F;
  return view;
}

namespace {
template <typename Array>
concept lighting_array = std::same_as<Array, ScalarCellularLightArray> || std::same_as<Array, ColoredCellularLightArray>;
}

CellularLightingCalculator::CellularLightingCalculator() = default;

CellularLightingCalculator::CellularLightingCalculator(bool monochrome)
    : m_light_array(monochrome ? LightArray(std::in_place_type<ScalarCellularLightArray>)
                               : LightArray(std::in_place_type<ColoredCellularLightArray>)) {}

std::expected<void, StarException> CellularLightingCalculator::set_monochrome(bool monochrome) {
  if (monochrome == std::holds_alternative<ScalarCellularLightArray>(m_light_array))
    return {};

  if (monochrome)
    m_light_array.emplace<ScalarCellularLightArray>();
  else
    m_light_array.emplace<ColoredCellularLightArray>();

  if (m_parameters)
    std::visit([&](lighting_array auto& array) { array.set_parameters(*m_parameters); }, m_light_array);
  return {};
}

namespace {
std::expected<float, StarException> light_float(Json const& config, String const& key) {
  auto value = config.get_double(key);
  if (!value) return std::unexpected(std::move(value.error()));
  if (!is_finite(*value) || *value < 0 || *value > std::numeric_limits<float>::max())
    return std::unexpected(LightmapException::format("{} must be a representable nonnegative lighting value", key));
  return static_cast<float>(*value);
}

std::expected<LightParameters, StarException> parse_light_parameters(Json const& config) {
  auto passes = config.get_double("spreadPasses");
  if (!passes) return std::unexpected(std::move(passes.error()));
  if (!is_finite(*passes) || *passes < 0 || *passes > std::numeric_limits<unsigned>::max() || std::trunc(*passes) != *passes)
    return std::unexpected(LightmapException("spreadPasses must be a nonnegative integer in range"));
  auto spread_air = light_float(config, "spreadMaxAir");
  if (!spread_air) return std::unexpected(std::move(spread_air.error()));
  auto spread_obstacle = light_float(config, "spreadMaxObstacle");
  if (!spread_obstacle) return std::unexpected(std::move(spread_obstacle.error()));
  auto point_air = light_float(config, "pointMaxAir");
  if (!point_air) return std::unexpected(std::move(point_air.error()));
  auto point_obstacle = light_float(config, "pointMaxObstacle");
  if (!point_obstacle) return std::unexpected(std::move(point_obstacle.error()));
  auto boost = light_float(config, "pointObstacleBoost");
  if (!boost) return std::unexpected(std::move(boost.error()));
  auto additive = config.get_bool("pointAdditive", false);
  if (!additive) return std::unexpected(std::move(additive.error()));
  return LightParameters::create(static_cast<std::int64_t>(*passes), *spread_air, *spread_obstacle, *point_air, *point_obstacle, *boost, *additive);
}

std::expected<RectI, StarException> begin_region(lighting_array auto& array, RectI const& query) {
  auto border = static_cast<std::int64_t>(array.border_cells());
  for (unsigned axis = 0; axis < 2; ++axis) {
    auto lo = std::int64_t(query.min()[axis]) - border;
    auto hi = std::int64_t(query.max()[axis]) + border;
    if (query.max()[axis] < query.min()[axis] || !std::in_range<int>(lo) || !std::in_range<int>(hi) || !std::in_range<int>(hi - lo))
      return std::unexpected(LightmapException("Lighting region or padded extent is out of range"));
  }
  auto region = query.padded(static_cast<int>(border));
  auto begun = array.begin(region.width(), region.height());
  if (!begun) return std::unexpected(std::move(begun.error()));
  return region;
}
}

std::expected<void, StarException> CellularLightingCalculator::set_parameters(Json const& config) {
  auto parameters = parse_light_parameters(config);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  auto has_limit = config.contains("brightnessLimit");
  if (!has_limit) return std::unexpected(std::move(has_limit.error()));
  std::optional<float> limit;
  if (*has_limit) {
    auto value = light_float(config, "brightnessLimit");
    if (!value) return std::unexpected(std::move(value.error()));
    limit = *value;
  }
  std::visit([&](lighting_array auto& array) { array.set_parameters(*parameters); }, m_light_array);
  m_parameters = *parameters;
  m_brightness_limit = limit;
  return {};
}

std::expected<void, StarException> CellularLightingCalculator::begin(RectI const& query_region) {
  auto region = std::visit([&](lighting_array auto& array) { return begin_region(array, query_region); }, m_light_array);
  if (!region) return std::unexpected(std::move(region.error()));
  m_query_region = query_region;
  m_calculation_region = *region;
  return {};
}

RectI CellularLightingCalculator::calculation_region() const {
  return m_calculation_region;
}

void CellularLightingCalculator::add_spread_light(Vec2F const& position, Vec3F const& light) {
  Vec2F array_position = position - Vec2F(m_calculation_region.min());
  if (std::holds_alternative<ScalarCellularLightArray>(m_light_array))
    std::get<ScalarCellularLightArray>(m_light_array).add_spread_light({.position = array_position, .value = light.max()});
  else
    std::get<ColoredCellularLightArray>(m_light_array).add_spread_light({.position = array_position, .value = light});
}

void CellularLightingCalculator::add_point_light(Vec2F const& position, Vec3F const& light, float beam, float beam_angle, float beam_ambience, bool as_spread) {
  auto array_position = position - Vec2F(m_calculation_region.min());
  auto add = [&]<lighting_array Array>(Array& array, typename Array::LightValue const& value) {
    array.add_point_light({.position = array_position, .value = value, .beam = beam,
                         .beam_angle = beam_angle, .beam_ambience = beam_ambience, .as_spread = as_spread});
  };
  if (std::holds_alternative<ScalarCellularLightArray>(m_light_array))
    add(std::get<ScalarCellularLightArray>(m_light_array), light.max());
  else
    add(std::get<ColoredCellularLightArray>(m_light_array), light);
}

void CellularLightingCalculator::calculate(Image& output) {
  Vec2S array_min = Vec2S(m_query_region.min() - m_calculation_region.min());
  Vec2S array_max = Vec2S(m_query_region.max() - m_calculation_region.min());

  std::visit([&]<lighting_array Array>(Array& array) {
    array.calculate(array_min[0], array_min[1], array_max[0], array_max[1]);
    output.reset(array_max[0] - array_min[0], array_max[1] - array_min[1], PixelFormat::RGB24);
    for (std::size_t x = array_min[0]; x < array_max[0]; ++x) {
      for (std::size_t y = array_min[1]; y < array_max[1]; ++y) {
        auto light = array.get_light(x, y);
        if constexpr (std::same_as<Array, ScalarCellularLightArray>)
          output.set24(x - array_min[0], y - array_min[1], Color::gray_f(light).to_rgb());
        else
          output.set24(x - array_min[0], y - array_min[1], Color::rgb_float_to_bytes(light));
      }
    }
  }, m_light_array);
}

std::expected<void, StarException> CellularLightingCalculator::calculate(Lightmap& output) {
  Vec2S array_min = Vec2S(m_query_region.min() - m_calculation_region.min());
  Vec2S array_max = Vec2S(m_query_region.max() - m_calculation_region.min());

  return std::visit([&]<lighting_array Array>(Array& array) -> std::expected<void, StarException> {
    if (!m_brightness_limit)
      return std::unexpected(LightmapException("Lightmap output requires brightnessLimit"));
    array.calculate(array_min[0], array_min[1], array_max[0], array_max[1]);
    auto resized = output.resize_for_overwrite(array_max[0] - array_min[0], array_max[1] - array_min[1]);
    if (!resized)
      return resized;

    for (std::size_t x = array_min[0]; x < array_max[0]; ++x) {
      for (std::size_t y = array_min[1]; y < array_max[1]; ++y) {
        auto light = array.get_light(x, y);
        if constexpr (std::same_as<Array, ScalarCellularLightArray>) {
          light = std::min(light, *m_brightness_limit);
        } else {
          float intensity = ColoredLightTraits::max_intensity(light);
          if (intensity > *m_brightness_limit)
            light *= *m_brightness_limit / intensity;
        }
        auto set = output.set(x - array_min[0], y - array_min[1], light);
        if (!set)
          return set;
      }
    }
    return {};
  }, m_light_array);
}

void CellularLightingCalculator::setup_image(Image& image, PixelFormat format) const {
  Vec2S array_min = Vec2S(m_query_region.min() - m_calculation_region.min());
  Vec2S array_max = Vec2S(m_query_region.max() - m_calculation_region.min());

  image.reset(array_max[0] - array_min[0], array_max[1] - array_min[1], format);
}

std::expected<void, StarException> CellularLightIntensityCalculator::set_parameters(Json const& config) {
  auto parameters = parse_light_parameters(config);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  m_light_array.set_parameters(*parameters);
  return {};
}

std::expected<void, StarException> CellularLightIntensityCalculator::begin(Vec2F const& query_position) {
  auto corner = query_position - Vec2F::filled(0.5f);
  for (float coordinate : corner) {
    if (!is_finite(coordinate) || double(coordinate) < std::numeric_limits<int>::min() ||
        double(coordinate) > std::numeric_limits<int>::max() - 2)
      return std::unexpected(LightmapException("Lighting query position is out of range"));
  }
  auto query = RectI::with_size(Vec2I::floor(corner), Vec2I(2, 2));
  auto region = begin_region(m_light_array, query);
  if (!region) return std::unexpected(std::move(region.error()));
  m_query_position = query_position;
  m_query_region = query;
  m_calculation_region = *region;
  return {};
}

RectI CellularLightIntensityCalculator::calculation_region() const {
  return m_calculation_region;
}

void CellularLightIntensityCalculator::set_cell(Vec2I const& position, Cell const& cell) {
  set_cell_column(position, std::span<Cell const>{&cell, 1});
}

void CellularLightIntensityCalculator::set_cell_column(Vec2I const& position, std::span<Cell const> cells) {
  std::size_t base_index = std::size_t(position[0] - m_calculation_region.x_min()) * m_calculation_region.height() + (position[1] - m_calculation_region.y_min());
  for (std::size_t i = 0; i < cells.size(); ++i)
    m_light_array.cell_at_index(base_index + i) = cells[i];
}

void CellularLightIntensityCalculator::add_spread_light(Vec2F const& position, float light) {
  Vec2F array_position = position - Vec2F(m_calculation_region.min());
  m_light_array.add_spread_light({.position = array_position, .value = light});
}

void CellularLightIntensityCalculator::add_point_light(Vec2F const& position, float light, float beam, float beam_angle, float beam_ambience) {
  Vec2F array_position = position - Vec2F(m_calculation_region.min());
  m_light_array.add_point_light({.position = array_position,
                              .value = light,
                              .beam = beam,
                              .beam_angle = beam_angle,
                              .beam_ambience = beam_ambience,
                              .as_spread = false});
}


float CellularLightIntensityCalculator::calculate() {
  Vec2S array_min = Vec2S(m_query_region.min() - m_calculation_region.min());
  Vec2S array_max = Vec2S(m_query_region.max() - m_calculation_region.min());

  m_light_array.calculate(array_min[0], array_min[1], array_max[0], array_max[1]);

  // Do 2d lerp to find lighting intensity

  float ll = m_light_array.get_light(array_min[0], array_min[1]);
  float lr = m_light_array.get_light(array_min[0] + 1, array_min[1]);
  float ul = m_light_array.get_light(array_min[0], array_min[1] + 1);
  float ur = m_light_array.get_light(array_min[0] + 1, array_min[1] + 1);

  float xl = m_query_position[0] - 0.5f - m_query_region.x_min();
  float yl = m_query_position[1] - 0.5f - m_query_region.y_min();

  return lerp(yl, lerp(xl, ll, lr), lerp(xl, ul, ur));
}

}
