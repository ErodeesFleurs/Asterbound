import std;
import star.format;
import star.json;
import star.either;
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

Lightmap::Lightmap(unsigned width, unsigned height) : m_width(width), m_height(height) {
  m_data = std::make_unique<float[]>(len());
}

Lightmap::Lightmap(Lightmap const& lightMap) {
  operator=(lightMap);
}

Lightmap::Lightmap(Lightmap&& lightMap) noexcept {
  operator=(std::move(lightMap));
}

Lightmap& Lightmap::operator=(Lightmap const& lightMap) {
  m_width = lightMap.m_width;
  m_height = lightMap.m_height;
  if (lightMap.m_data) {
    m_data = std::make_unique<float[]>(len());
    std::memcpy(m_data.get(), lightMap.m_data.get(), len());
  }
  return *this;
}

Lightmap& Lightmap::operator=(Lightmap&& lightMap) noexcept {
  m_width = std::exchange(lightMap.m_width, 0);
  m_height = std::exchange(lightMap.m_height, 0);
  m_data = std::move(lightMap.m_data);
  return *this;
}

Lightmap::operator ImageView() {
  ImageView view;
  auto pixels = data();
  view.data = {reinterpret_cast<std::uint8_t const*>(pixels.data()), pixels.size_bytes()};
  view.size = size();
  view.format = PixelFormat::RGB_F;
  return view;
}

CellularLightingCalculator::CellularLightingCalculator()
    : CellularLightingCalculator(false) {}

CellularLightingCalculator::CellularLightingCalculator(bool monochrome)
    : m_monochrome(monochrome)
{
    if (monochrome)
        m_lightArray.setRight(ScalarCellularLightArray());
    else
        m_lightArray.setLeft(ColoredCellularLightArray());
}

std::expected<void, StarException> CellularLightingCalculator::setMonochrome(bool monochrome) {
  if (monochrome == m_monochrome)
    return {};

  m_monochrome = monochrome;
  if (monochrome)
    m_lightArray.setRight(ScalarCellularLightArray());
  else
    m_lightArray.setLeft(ColoredCellularLightArray());

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
  if (m_monochrome)
    m_lightArray.rightRef().setParameters(*passes, *spreadAir, *spreadObstacle, *pointAir, *pointObstacle, *boost, *additive);
  else
    m_lightArray.leftRef().setParameters(*passes, *spreadAir, *spreadObstacle, *pointAir, *pointObstacle, *boost, *additive);
  return {};
}

void CellularLightingCalculator::begin(RectI const& queryRegion) {
  m_queryRegion = queryRegion;
  if (m_monochrome) {
    m_calculationRegion = RectI(queryRegion).padded(static_cast<int>(m_lightArray.rightRef().borderCells()));
    m_lightArray.rightRef().begin(m_calculationRegion.width(), m_calculationRegion.height());
  } else {
    m_calculationRegion = RectI(queryRegion).padded(static_cast<int>(m_lightArray.leftRef().borderCells()));
    m_lightArray.leftRef().begin(m_calculationRegion.width(), m_calculationRegion.height());
  }
}

RectI CellularLightingCalculator::calculationRegion() const {
  return m_calculationRegion;
}

void CellularLightingCalculator::addSpreadLight(Vec2F const& position, Vec3F const& light) {
  Vec2F arrayPosition = position - Vec2F(m_calculationRegion.min());
  if (m_monochrome)
    m_lightArray.rightRef().addSpreadLight({.position = arrayPosition, .value = light.max()});
  else
    m_lightArray.leftRef().addSpreadLight({.position = arrayPosition, .value = light});
}

void CellularLightingCalculator::addPointLight(Vec2F const& position, Vec3F const& light, float beam, float beamAngle, float beamAmbience, bool asSpread) {
  Vec2F arrayPosition = position - Vec2F(m_calculationRegion.min());
  if (m_monochrome)
    m_lightArray.rightRef().addPointLight({.position = arrayPosition,
                                           .value = light.max(),
                                           .beam = beam,
                                           .beamAngle = beamAngle,
                                           .beamAmbience = beamAmbience,
                                           .asSpread = asSpread});
  else
    m_lightArray.leftRef().addPointLight({.position = arrayPosition,
                                          .value = light,
                                          .beam = beam,
                                          .beamAngle = beamAngle,
                                          .beamAmbience = beamAmbience,
                                          .asSpread = asSpread});
}

void CellularLightingCalculator::calculate(Image& output) {
  Vec2S arrayMin = Vec2S(m_queryRegion.min() - m_calculationRegion.min());
  Vec2S arrayMax = Vec2S(m_queryRegion.max() - m_calculationRegion.min());

  if (m_monochrome)
    m_lightArray.rightRef().calculate(arrayMin[0], arrayMin[1], arrayMax[0], arrayMax[1]);
  else
    m_lightArray.leftRef().calculate(arrayMin[0], arrayMin[1], arrayMax[0], arrayMax[1]);

  output.reset(arrayMax[0] - arrayMin[0], arrayMax[1] - arrayMin[1], PixelFormat::RGB24);

  if (m_monochrome) {
    for (std::size_t x = arrayMin[0]; x < arrayMax[0]; ++x) {
      for (std::size_t y = arrayMin[1]; y < arrayMax[1]; ++y) {
        output.set24(x - arrayMin[0], y - arrayMin[1], Color::grayf(m_lightArray.rightRef().getLight(x, y)).toRgb());
      }
    }
  } else {
    for (std::size_t x = arrayMin[0]; x < arrayMax[0]; ++x) {
      for (std::size_t y = arrayMin[1]; y < arrayMax[1]; ++y) {
        output.set24(x - arrayMin[0], y - arrayMin[1], Color::v3fToByte(m_lightArray.leftRef().getLight(x, y)));
      }
    }
  }
}

std::expected<void, StarException> CellularLightingCalculator::calculate(Lightmap& output) {
  Vec2S arrayMin = Vec2S(m_queryRegion.min() - m_calculationRegion.min());
  Vec2S arrayMax = Vec2S(m_queryRegion.max() - m_calculationRegion.min());

  if (m_monochrome)
    m_lightArray.rightRef().calculate(arrayMin[0], arrayMin[1], arrayMax[0], arrayMax[1]);
  else
    m_lightArray.leftRef().calculate(arrayMin[0], arrayMin[1], arrayMax[0], arrayMax[1]);

  output = Lightmap(arrayMax[0] - arrayMin[0], arrayMax[1] - arrayMin[1]);

  auto brightnessLimit = m_config.getFloat("brightnessLimit");
  if (!brightnessLimit) return std::unexpected(std::move(brightnessLimit.error()));

  if (m_monochrome) {
    for (std::size_t x = arrayMin[0]; x < arrayMax[0]; ++x) {
      for (std::size_t y = arrayMin[1]; y < arrayMax[1]; ++y) {
        auto light = std::min(m_lightArray.rightRef().getLight(x, y), *brightnessLimit);
        auto set = output.set(x - arrayMin[0], y - arrayMin[1], light);
        if (!set) return std::unexpected(std::move(set.error()));
      }
    }
  } else {
    for (std::size_t x = arrayMin[0]; x < arrayMax[0]; ++x) {
      for (std::size_t y = arrayMin[1]; y < arrayMax[1]; ++y) {
        auto light = m_lightArray.leftRef().getLight(x, y);
        float intensity = ColoredLightTraits::maxIntensity(light);
        if (intensity > *brightnessLimit)
          light *= *brightnessLimit / intensity;
        auto set = output.set(x - arrayMin[0], y - arrayMin[1], light);
        if (!set) return std::unexpected(std::move(set.error()));
      }
    }
  }
  return {};
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
