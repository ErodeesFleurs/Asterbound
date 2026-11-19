import std;
import star.format;
import star.json;
import star.rect;
import star.image;
import star.color;
import star.interpolation;
import star.list;
import star.vector;
import star.cellular_light_array;
import star.thread;
import star.cellular_lighting;

import star.algorithm;
namespace Star {

Lightmap::Lightmap() : m_width(0), m_height(0) {}

Lightmap::Lightmap(unsigned width, unsigned height) : Lightmap() {
  auto resized = resizeForOverwrite(width, height);
  if (!resized) {
    Star::fatal_error(resized.error().what(), false);
    std::terminate();
  }
  std::ranges::fill(data(), 0.0f);
}

Lightmap::Lightmap(Lightmap const& lightMap) : Lightmap() {
  operator=(lightMap);
}

Lightmap::Lightmap(Lightmap&& lightMap) noexcept : Lightmap() {
  operator=(std::move(lightMap));
}

Lightmap& Lightmap::operator=(Lightmap const& lightMap) {
  if (this == &lightMap)
    return *this;
  auto length = lightMap.len();
  if (length > m_capacity) {
    m_data = std::make_unique_for_overwrite<float[]>(length);
    m_capacity = length;
  }
  std::ranges::copy(lightMap.data(), std::span(m_data.get(), length).begin());
  m_width = lightMap.m_width;
  m_height = lightMap.m_height;
  return *this;
}

Lightmap& Lightmap::operator=(Lightmap&& lightMap) noexcept {
  if (this == &lightMap)
    return *this;
  m_width = std::exchange(lightMap.m_width, 0);
  m_height = std::exchange(lightMap.m_height, 0);
  m_data = std::move(lightMap.m_data);
  m_capacity = std::exchange(lightMap.m_capacity, 0);
  return *this;
}

void Lightmap::swap(Lightmap& other) noexcept {
  m_data.swap(other.m_data);
  std::swap(m_capacity, other.m_capacity);
  std::swap(m_width, other.m_width);
  std::swap(m_height, other.m_height);
}

std::expected<void, StarException> Lightmap::resizeForOverwrite(unsigned width, unsigned height) {
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

Lightmap::operator ImageView() {
  ImageView view;
  auto pixels = data();
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

std::expected<void, StarException> CellularLightingCalculator::setMonochrome(bool monochrome) {
  if (monochrome == std::holds_alternative<ScalarCellularLightArray>(m_light_array))
    return {};

  if (monochrome)
    m_light_array.emplace<ScalarCellularLightArray>();
  else
    m_light_array.emplace<ColoredCellularLightArray>();

  if (m_config)
    return setParameters(m_config);
  return {};
}

std::expected<void, StarException> CellularLightingCalculator::setParameters(Json const& config) {
  auto passes = config.getInt("spreadPasses");
  if (!passes) return std::unexpected(std::move(passes.error()));
  auto spreadAir = config.getFloat("spreadMaxAir");
  if (!spreadAir) return std::unexpected(std::move(spreadAir.error()));
  auto spreadObstacle = config.getFloat("spreadMaxObstacle");
  if (!spreadObstacle) return std::unexpected(std::move(spreadObstacle.error()));
  auto pointAir = config.getFloat("pointMaxAir");
  if (!pointAir) return std::unexpected(std::move(pointAir.error()));
  auto pointObstacle = config.getFloat("pointMaxObstacle");
  if (!pointObstacle) return std::unexpected(std::move(pointObstacle.error()));
  auto boost = config.getFloat("pointObstacleBoost");
  if (!boost) return std::unexpected(std::move(boost.error()));
  auto additive = config.getBool("pointAdditive", false);
  if (!additive) return std::unexpected(std::move(additive.error()));
  m_config = config;
  std::visit([&](lighting_array auto& array) {
    array.setParameters(*passes, *spreadAir, *spreadObstacle, *pointAir, *pointObstacle, *boost, *additive);
  }, m_light_array);
  return {};
}

void CellularLightingCalculator::begin(RectI const& queryRegion) {
  m_queryRegion = queryRegion;
  std::visit([&](lighting_array auto& array) {
    m_calculationRegion = queryRegion.padded(static_cast<int>(array.borderCells()));
    array.begin(m_calculationRegion.width(), m_calculationRegion.height());
  }, m_light_array);
}

RectI CellularLightingCalculator::calculationRegion() const {
  return m_calculationRegion;
}

void CellularLightingCalculator::addSpreadLight(Vec2F const& position, Vec3F const& light) {
  Vec2F arrayPosition = position - Vec2F(m_calculationRegion.min());
  if (std::holds_alternative<ScalarCellularLightArray>(m_light_array))
    std::get<ScalarCellularLightArray>(m_light_array).addSpreadLight({.position = arrayPosition, .value = light.max()});
  else
    std::get<ColoredCellularLightArray>(m_light_array).addSpreadLight({.position = arrayPosition, .value = light});
}

void CellularLightingCalculator::addPointLight(Vec2F const& position, Vec3F const& light, float beam, float beamAngle, float beamAmbience, bool asSpread) {
  auto array_position = position - Vec2F(m_calculationRegion.min());
  auto add = [&]<lighting_array Array>(Array& array, typename Array::LightValue const& value) {
    array.addPointLight({.position = array_position, .value = value, .beam = beam,
                         .beamAngle = beamAngle, .beamAmbience = beamAmbience, .asSpread = asSpread});
  };
  if (std::holds_alternative<ScalarCellularLightArray>(m_light_array))
    add(std::get<ScalarCellularLightArray>(m_light_array), light.max());
  else
    add(std::get<ColoredCellularLightArray>(m_light_array), light);
}

void CellularLightingCalculator::calculate(Image& output) {
  Vec2S array_min = Vec2S(m_queryRegion.min() - m_calculationRegion.min());
  Vec2S array_max = Vec2S(m_queryRegion.max() - m_calculationRegion.min());

  std::visit([&]<lighting_array Array>(Array& array) {
    array.calculate(array_min[0], array_min[1], array_max[0], array_max[1]);
    output.reset(array_max[0] - array_min[0], array_max[1] - array_min[1], PixelFormat::RGB24);
    for (std::size_t x = array_min[0]; x < array_max[0]; ++x) {
      for (std::size_t y = array_min[1]; y < array_max[1]; ++y) {
        auto light = array.getLight(x, y);
        if constexpr (std::same_as<Array, ScalarCellularLightArray>)
          output.set24(x - array_min[0], y - array_min[1], Color::gray_f(light).to_rgb());
        else
          output.set24(x - array_min[0], y - array_min[1], Color::rgb_float_to_bytes(light));
      }
    }
  }, m_light_array);
}

std::expected<void, StarException> CellularLightingCalculator::calculate(Lightmap& output) {
  Vec2S array_min = Vec2S(m_queryRegion.min() - m_calculationRegion.min());
  Vec2S array_max = Vec2S(m_queryRegion.max() - m_calculationRegion.min());

  return std::visit([&]<lighting_array Array>(Array& array) -> std::expected<void, StarException> {
    array.calculate(array_min[0], array_min[1], array_max[0], array_max[1]);
    auto brightness_limit = m_config.getFloat("brightnessLimit");
    if (!brightness_limit)
      return std::unexpected(std::move(brightness_limit.error()));
    auto resized = output.resizeForOverwrite(array_max[0] - array_min[0], array_max[1] - array_min[1]);
    if (!resized)
      return resized;

    for (std::size_t x = array_min[0]; x < array_max[0]; ++x) {
      for (std::size_t y = array_min[1]; y < array_max[1]; ++y) {
        auto light = array.getLight(x, y);
        if constexpr (std::same_as<Array, ScalarCellularLightArray>) {
          light = std::min(light, *brightness_limit);
        } else {
          float intensity = ColoredLightTraits::maxIntensity(light);
          if (intensity > *brightness_limit)
            light *= *brightness_limit / intensity;
        }
        auto set = output.set(x - array_min[0], y - array_min[1], light);
        if (!set)
          return set;
      }
    }
    return {};
  }, m_light_array);
}

void CellularLightingCalculator::setupImage(Image& image, PixelFormat format) const {
  Vec2S arrayMin = Vec2S(m_queryRegion.min() - m_calculationRegion.min());
  Vec2S arrayMax = Vec2S(m_queryRegion.max() - m_calculationRegion.min());

  image.reset(arrayMax[0] - arrayMin[0], arrayMax[1] - arrayMin[1], format);
}

std::expected<void, StarException> CellularLightIntensityCalculator::setParameters(Json const& config) {
  auto passes = config.getInt("spreadPasses");
  if (!passes) return std::unexpected(std::move(passes.error()));
  auto spreadAir = config.getFloat("spreadMaxAir");
  if (!spreadAir) return std::unexpected(std::move(spreadAir.error()));
  auto spreadObstacle = config.getFloat("spreadMaxObstacle");
  if (!spreadObstacle) return std::unexpected(std::move(spreadObstacle.error()));
  auto pointAir = config.getFloat("pointMaxAir");
  if (!pointAir) return std::unexpected(std::move(pointAir.error()));
  auto pointObstacle = config.getFloat("pointMaxObstacle");
  if (!pointObstacle) return std::unexpected(std::move(pointObstacle.error()));
  auto boost = config.getFloat("pointObstacleBoost");
  if (!boost) return std::unexpected(std::move(boost.error()));
  auto additive = config.getBool("pointAdditive", false);
  if (!additive) return std::unexpected(std::move(additive.error()));
  m_lightArray.setParameters(*passes, *spreadAir, *spreadObstacle, *pointAir, *pointObstacle, *boost, *additive);
  return {};
}

void CellularLightIntensityCalculator::begin(Vec2F const& queryPosition) {
  m_queryPosition = queryPosition;
  m_queryRegion = RectI::withSize(Vec2I::floor(queryPosition - Vec2F::filled(0.5f)), Vec2I(2, 2));
  m_calculationRegion = RectI(m_queryRegion).padded(static_cast<int>(m_lightArray.borderCells()));

  m_lightArray.begin(m_calculationRegion.width(), m_calculationRegion.height());
}

RectI CellularLightIntensityCalculator::calculationRegion() const {
  return m_calculationRegion;
}

void CellularLightIntensityCalculator::setCell(Vec2I const& position, Cell const& cell) {
  setCellColumn(position, std::span<Cell const>{&cell, 1});
}

void CellularLightIntensityCalculator::setCellColumn(Vec2I const& position, std::span<Cell const> cells) {
  std::size_t baseIndex = (position[0] - m_calculationRegion.xMin()) * m_calculationRegion.height() + position[1] - m_calculationRegion.yMin();
  for (std::size_t i = 0; i < cells.size(); ++i)
    m_lightArray.cellAtIndex(baseIndex + i) = cells[i];
}

void CellularLightIntensityCalculator::addSpreadLight(Vec2F const& position, float light) {
  Vec2F arrayPosition = position - Vec2F(m_calculationRegion.min());
  m_lightArray.addSpreadLight({.position = arrayPosition, .value = light});
}

void CellularLightIntensityCalculator::addPointLight(Vec2F const& position, float light, float beam, float beamAngle, float beamAmbience) {
  Vec2F arrayPosition = position - Vec2F(m_calculationRegion.min());
  m_lightArray.addPointLight({.position = arrayPosition,
                              .value = light,
                              .beam = beam,
                              .beamAngle = beamAngle,
                              .beamAmbience = beamAmbience,
                              .asSpread = false});
}


float CellularLightIntensityCalculator::calculate() {
  Vec2S arrayMin = Vec2S(m_queryRegion.min() - m_calculationRegion.min());
  Vec2S arrayMax = Vec2S(m_queryRegion.max() - m_calculationRegion.min());

  m_lightArray.calculate(arrayMin[0], arrayMin[1], arrayMax[0], arrayMax[1]);

  // Do 2d lerp to find lighting intensity

  float ll = m_lightArray.getLight(arrayMin[0], arrayMin[1]);
  float lr = m_lightArray.getLight(arrayMin[0] + 1, arrayMin[1]);
  float ul = m_lightArray.getLight(arrayMin[0], arrayMin[1] + 1);
  float ur = m_lightArray.getLight(arrayMin[0] + 1, arrayMin[1] + 1);

  float xl = m_queryPosition[0] - 0.5f - m_queryRegion.xMin();
  float yl = m_queryPosition[1] - 0.5f - m_queryRegion.yMin();

  return lerp(yl, lerp(xl, ll, lr), lerp(xl, ul, ur));
}

}
