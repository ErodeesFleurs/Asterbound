import star.format;
import star.json;
import star.json_extra;
import star.interpolation;
import star.ordered_map;
import star.either;
import star.vector;
import star.game_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.data_stream_extra;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.optional;
import star.string;
import star.version;
import star.time;
import star.random_point;
import star.audio;
import star.map;
import star.set;

import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.celestial_graphics;
import star.mixer;
import star.sky_parameters;
import star.sky_render_data;

import star.sky;

import star.compression;
import star.celestial_database;

import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
import star.array;
import star.math_common;
import star.algorithm;
namespace Star {

Sky::Sky(Root& root) : m_root(root) {
  m_netInit = false;

  m_netGroup.setNeedsLoadCallback([this](bool) { return readNetStates(); });
  m_netGroup.setNeedsStoreCallback([this]() { return writeNetStates(); });
}

std::expected<std::shared_ptr<Sky>, StarException> Sky::create(Root& root) {
  auto sky = std::shared_ptr<Sky>(new Sky(root));
  auto settings = sky->skyParametersUpdated();
  if (!settings) return std::unexpected(std::move(settings.error()));
  auto element0 = sky->m_netGroup.addNetElement(sky->m_skyParametersNetState);
  if (!element0) return std::unexpected(std::move(element0.error()));
  auto element1 = sky->m_netGroup.addNetElement(sky->m_skyTypeNetState);
  if (!element1) return std::unexpected(std::move(element1.error()));
  auto element2 = sky->m_netGroup.addNetElement(sky->m_timeNetState);
  if (!element2) return std::unexpected(std::move(element2.error()));
  auto element3 = sky->m_netGroup.addNetElement(sky->m_flyingTypeNetState);
  if (!element3) return std::unexpected(std::move(element3.error()));
  auto element4 = sky->m_netGroup.addNetElement(sky->m_enterHyperspaceNetState);
  if (!element4) return std::unexpected(std::move(element4.error()));
  auto element5 = sky->m_netGroup.addNetElement(sky->m_startInWarpNetState);
  if (!element5) return std::unexpected(std::move(element5.error()));
  auto element6 = sky->m_netGroup.addNetElement(sky->m_worldMoveNetState);
  if (!element6) return std::unexpected(std::move(element6.error()));
  auto element7 = sky->m_netGroup.addNetElement(sky->m_starMoveNetState);
  if (!element7) return std::unexpected(std::move(element7.error()));
  auto element8 = sky->m_netGroup.addNetElement(sky->m_warpPhaseNetState);
  if (!element8) return std::unexpected(std::move(element8.error()));
  auto element9 = sky->m_netGroup.addNetElement(sky->m_flyingTimerNetState);
  if (!element9) return std::unexpected(std::move(element9.error()));
  return sky;
}

std::expected<std::shared_ptr<Sky>, StarException> Sky::create(Root& root, SkyParameters const& skyParameters, bool inOrbit) {
  auto sky = create(root);
  if (!sky) return std::unexpected(std::move(sky.error()));
  (*sky)->m_skyParameters = skyParameters;
  auto settings = (*sky)->skyParametersUpdated();
  if (!settings) return std::unexpected(std::move(settings.error()));
  (*sky)->m_skyType = inOrbit ? SkyType::Orbital : skyParameters.skyType;
  return sky;
}

std::expected<void, StarException> Sky::startFlying(bool enterHyperspace, bool startInWarp, Json settings) {
  if (startInWarp)
    m_flyingType = FlyingType::Warp;
  else
    m_flyingType = FlyingType::Disembarking;

  m_flyingTimer = 0;
  m_enterHyperspace = enterHyperspace;
  m_startInWarp = startInWarp;
  if (settings.isType(Json::Type::Object)) {
    m_skyParameters.settings = settings;
    auto updated = skyParametersUpdated();
    if (!updated) return std::unexpected(std::move(updated.error()));
  }
  return {};
}

std::expected<void, StarException> Sky::stopFlyingAt(std::optional<SkyParameters> dest) {
  m_destWorld = std::move(dest);
  return {};
}

std::expected<void, StarException> Sky::jumpTo(SkyParameters skyParameters) {
  m_skyParameters = std::move(skyParameters);
  return skyParametersUpdated();
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Sky::writeUpdate(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  return m_netGroup.writeNetState(fromVersion, rules);
}

std::expected<void, StarException> Sky::readUpdate(ByteArray data, NetCompatibilityRules rules) {
  return m_netGroup.readNetState(std::move(data), 0.0f, rules);
}

std::expected<void, StarException> Sky::stateUpdate() {
  if (m_lastFlyingType != m_flyingType) {
    m_flyingTimer = 0.0f;

    if (m_flyingType == FlyingType::Warp) {
      m_warpPhase = WarpPhase::SpeedingUp;
      if (m_startInWarp) {
        if (m_enterHyperspace)
          m_warpPhase = WarpPhase::Maintain;
        else
          m_flyingTimer = speedupTime();

        m_lastWarpPhase = m_warpPhase;
      }

      float maxVelocity = m_numericSettings.flyMaxVelocity;
      m_worldMoveOffset = Vec2F::withAngle(m_pathRotation, maxVelocity / 2.0 * speedupTime());
      m_starMoveOffset = Vec2F::withAngle(0, (maxVelocity * m_numericSettings.starVelocityFactor) / 2.0 * speedupTime());
    } else if (m_flyingType == FlyingType::Arriving) {
      m_sentSFX = false;

      m_worldOffset = {};
      m_starOffset = {};
    }
  }

  if (m_lastWarpPhase != m_warpPhase) {
    m_flyingTimer = 0.0f;

    if (m_warpPhase == WarpPhase::SpeedingUp)
      m_sentSFX = false;
    else if (m_warpPhase == WarpPhase::Maintain) {
      auto entered = enterHyperspace();
      if (!entered) return std::unexpected(std::move(entered.error()));
    } else if (m_warpPhase == WarpPhase::SlowingDown) {
      auto exited = exitHyperspace();
      if (!exited) return std::unexpected(std::move(exited.error()));
    }
  }

  m_lastFlyingType = m_flyingType;
  m_lastWarpPhase = m_warpPhase;
  return {};
}

std::expected<void, StarException> Sky::update(double dt) {
  if (m_referenceClock) {
    m_time = m_referenceClock->time();
    if (!m_clockTrackingTime) {
      m_clockTrackingTime = m_time;
    } else {
      // If our reference clock is set, and we have a valid tracking time, then
      // the dt should be driven by the reference clock.
      dt = m_time - *m_clockTrackingTime;
      m_clockTrackingTime = m_time;
    }
  } else {
    m_time += dt;
  }

  m_flashTimer = std::max(0.0, m_flashTimer - dt);

  if (flying()) {
    m_flyingTimer += dt;

    if (m_flyingType == FlyingType::Disembarking) {
      auto path = m_settings.getArray(m_skyParameters.skyType == SkyType::Space ? String("spaceDisembarkPath") : String("disembarkPath"));
      if (!path) return std::unexpected(std::move(path.error()));
      auto origin = m_settings.get(m_skyParameters.skyType == SkyType::Space ? String("spaceDisembarkOrigin") : String("disembarkOrigin"));
      if (!origin) return std::unexpected(std::move(origin.error()));
      auto finished = controlledMovement(*path, *origin, m_flyingTimer);
      if (!finished) return std::unexpected(std::move(finished.error()));
      if (*finished) {
        m_flyingType = FlyingType::Warp;
      }
    } else if (m_flyingType == FlyingType::Arriving) {
      auto path = m_settings.getArray(m_skyParameters.skyType == SkyType::Space ? String("spaceArrivalPath") : String("arrivalPath"));
      if (!path) return std::unexpected(std::move(path.error()));
      auto origin = m_settings.get(m_skyParameters.skyType == SkyType::Space ? String("spaceArrivalOrigin") : String("arrivalOrigin"));
      if (!origin) return std::unexpected(std::move(origin.error()));
      auto finished = controlledMovement(*path, *origin, m_flyingTimer);
      if (!finished) return std::unexpected(std::move(finished.error()));
      if (*finished) {
        m_flyingType = FlyingType::None;
      }

      m_starOffset -= m_starOffset * m_numericSettings.correctionPower;
      m_worldOffset -= m_worldOffset * m_numericSettings.correctionPower;
    } else if (m_flyingType == FlyingType::Warp) {
      float percentage = 0.0;
      float dir = static_cast<int>(m_warpPhase) < 0 ? -1.0 : 1.0;
      if (m_warpPhase == WarpPhase::SpeedingUp)
        percentage = std::powf(m_flyingTimer / speedupTime(), 2.0);
      else if (m_warpPhase == WarpPhase::Maintain)
        percentage = 1.0;
      else if (m_warpPhase == WarpPhase::SlowingDown)
        percentage = std::powf(1 - m_flyingTimer / slowdownTime(), 2.0);

      if (percentage < 1.0) {
        m_starOffset = (dir * m_starMoveOffset * percentage).rotate(-getStarRotation());
        m_worldOffset = (dir * m_worldMoveOffset * percentage).rotate(-getWorldRotation());
      } else {
        m_starOffset += Vec2F::withAngle(-getStarRotation(), m_numericSettings.flyMaxVelocity * dt * m_numericSettings.starVelocityFactor);
        m_worldOffset = m_worldMoveOffset;
      }

      if (m_warpPhase == WarpPhase::SpeedingUp && m_flyingTimer >= speedupTime()
          && !m_enterHyperspace
          && m_destWorld) {
        auto destination = takeOptional(m_destWorld);
        if (!destination) return std::unexpected(std::move(destination.error()));
        auto jumped = jumpTo(std::move(*destination));
        if (!jumped) return std::unexpected(std::move(jumped.error()));
        m_warpPhase = WarpPhase::SlowingDown;
      } else if (m_warpPhase == WarpPhase::SpeedingUp && m_flyingTimer >= speedupTime() && m_enterHyperspace) {
        m_warpPhase = WarpPhase::Maintain;
      } else if (m_warpPhase == WarpPhase::Maintain && m_flyingTimer >= m_numericSettings.flyingTimer
          && m_destWorld) {
        auto destination = takeOptional(m_destWorld);
        if (!destination) return std::unexpected(std::move(destination.error()));
        auto jumped = jumpTo(std::move(*destination));
        if (!jumped) return std::unexpected(std::move(jumped.error()));
        m_warpPhase = WarpPhase::SlowingDown;
      } else if (m_warpPhase == WarpPhase::SlowingDown && m_flyingTimer >= slowdownTime()) {
        m_flyingType = FlyingType::Arriving;
      }
    }
  } else {
    m_starOffset = {};
    m_worldOffset = {};
    m_pathOffset = {};
    m_worldRotation = m_pathRotation = 0;
  }

  auto state = stateUpdate();
  if (!state) return std::unexpected(std::move(state.error()));

  if (!flying())
    m_starRotation = constrainAngle(m_starRotation + dt / dayLength() * 2 * std::numbers::pi);
  else
    m_starRotation = 0;
  return {};
}

void Sky::setType(SkyType skyType) {
  m_skyType = skyType;
}

SkyType Sky::type() const {
  return m_skyType;
}

bool Sky::inSpace() const {
  return m_skyType == SkyType::Orbital || m_skyType == SkyType::Warp || m_skyType == SkyType::Space;
}

std::uint64_t Sky::seed() const {
  return m_skyParameters.seed;
}

float Sky::dayLength() const {
  return m_skyParameters.dayLength.value_or(DefaultDayLength);
}

std::uint32_t Sky::day() const {
  if (!m_skyParameters.dayLength)
    return 0;
  return std::floor(epochTime() / dayLength());
}

float Sky::timeOfDay() const {
  if (!m_skyParameters.dayLength)
    return 0;
  return std::fmod(epochTime(), static_cast<double>(dayLength()));
}

double Sky::epochTime() const {
  return m_time;
}

void Sky::setEpochTime(double epochTime) {
  m_time = epochTime;
}

float Sky::altitude() const {
  return m_altitude;
}

void Sky::setAltitude(float altitude) {
  m_altitude = altitude;
}

void Sky::setReferenceClock(std::shared_ptr<Clock const> const& referenceClock) {
  m_referenceClock = referenceClock;
  m_time = m_referenceClock->time();
  m_clockTrackingTime = {};
}

std::shared_ptr<Clock const> Sky::referenceClock() const {
  return m_referenceClock;
}

std::expected<String, StarException> Sky::ambientNoise() const {
  if (flying()) {
    if (m_flyingType == FlyingType::Warp && m_warpPhase == WarpPhase::Maintain) {
      return m_settings.queryString("hyperspaceAudio");
    } else {
      return m_settings.queryString("engineAudio");
    }
  }

  return "";
}

std::expected<List<std::shared_ptr<AudioInstance>>, StarException> Sky::pullSounds() {
  List<std::shared_ptr<AudioInstance>> res;
  if (m_flyingType == FlyingType::Warp) {
    if (m_warpPhase == WarpPhase::SpeedingUp && !m_sentSFX) {
      float triggerTime = speedupTime() - m_numericSettings.enterHyperspaceAudioLeadIn;
      if (triggerTime < 0 || !m_enterHyperspace) {
        m_sentSFX = true;
        return res;
      }

      if (m_flyingTimer >= triggerTime) {
        auto assets = m_root.assets();
        if (!assets) return std::unexpected(std::move(assets.error()));
        auto path = m_settings.queryString("enterHyperspaceAudio");
        if (!path) return std::unexpected(std::move(path.error()));
        auto sfx = (*assets)->audio(*path);
        if (!sfx) return std::unexpected(std::move(sfx.error()));
        auto instance = AudioInstance::create(**sfx);
        if (!instance) return std::unexpected(std::move(instance.error()));
        m_sentSFX = true;
        res.push_back(std::move(*instance));
        return res;
      }
    } else if (m_warpPhase == WarpPhase::Maintain && !m_sentSFX) {
      float triggerTime = m_numericSettings.flyingTimer - m_numericSettings.exitHyperspaceAudioLeadIn;
      if (triggerTime < 0) {
        m_sentSFX = true;
        return res;
      }

      if (m_flyingTimer >= triggerTime) {
        auto assets = m_root.assets();
        if (!assets) return std::unexpected(std::move(assets.error()));
        auto path = m_settings.queryString("exitHyperspaceAudio");
        if (!path) return std::unexpected(std::move(path.error()));
        auto sfx = (*assets)->audio(*path);
        if (!sfx) return std::unexpected(std::move(sfx.error()));
        auto instance = AudioInstance::create(**sfx);
        if (!instance) return std::unexpected(std::move(instance.error()));
        m_sentSFX = true;
        res.push_back(std::move(*instance));
        return res;
      }
    }
  } else if (m_flyingType == FlyingType::Arriving) {
    if (!m_sentSFX) {
      auto assets = m_root.assets();
      if (!assets) return std::unexpected(std::move(assets.error()));
      auto path = m_settings.queryString("arrivalAudio");
      if (!path) return std::unexpected(std::move(path.error()));
      auto sfx = (*assets)->audio(*path);
      if (!sfx) return std::unexpected(std::move(sfx.error()));
      auto instance = AudioInstance::create(**sfx);
      if (!instance) return std::unexpected(std::move(instance.error()));
      m_sentSFX = true;
      res.push_back(std::move(*instance));
      return res;
    }
  }
  return res;
}

float Sky::spaceLevel() const {
  if (type() == SkyType::Atmospheric && m_skyParameters.spaceLevel && m_skyParameters.surfaceLevel) {
    float altitudeRange = *m_skyParameters.spaceLevel - *m_skyParameters.surfaceLevel;
    float relativeAltitude = m_altitude - *m_skyParameters.surfaceLevel;
    return std::clamp(relativeAltitude / altitudeRange, 0.0f, 1.0f);
  }
  return 1.0f;
}

float Sky::orbitAngle() const {
  // TODO: What should we do here?  Used to divide by zero.  Now is saved by
  // DefaultDayLength, but it's a hack
  return 2 * std::numbers::pi * timeOfDay() / dayLength();
}

bool Sky::isDayTime() const {
  return dayLevel() >= 0.5;
}

float Sky::dayLevel() const {
  // Turn the dayCycle value into a value that blends evenly between 0.0 at
  // mid-night and 1.0 at mid-day and then back again.

  float dayCycle = Sky::dayCycle();
  if (dayCycle < 1.0)
    return dayCycle / 2 + 0.5f;
  else if (dayCycle > 3.0)
    return (dayCycle - 3) / 2;
  else
    return 1.0 - (dayCycle - 1.0) / 2;
}

float Sky::dayCycle() const {
  // Always middle of the night in orbit or warp space.
  if (type() == SkyType::Orbital || type() == SkyType::Warp)
    return 3.0f;

  // This will misbehave badly if dayTransitionTime is greater than dayLength /
  // 2

  float transitionTime = m_numericSettings.dayTransitionTime / 2;
  float dayLength = Sky::dayLength();
  float timeOfDay = Sky::timeOfDay();

  // timeOfDay() is defined such that 0.0 is mid-dawn.  For convenience, shift
  // the time of day forwards such that 0.0 is the beginning of the morning.
  float shiftedTime = pfmod(timeOfDay + transitionTime / 2, dayLength);

  // There are 5 times here, beginning of the morning, end of the morning,
  // beginning of the evening, end of the evening, and then the beginning of
  // the morning again (wrapping around).
  Array<float, 5> transitionPositions = {
      0.0f, transitionTime, dayLength / 2, dayLength / 2 + transitionTime, dayLength};
  // The values here are mid-night, mid-day, mid-day, mid-night, mid-night.
  Array<float, 5> transitionValues = {-1.0f, 1.0f, 1.0f, 3.0f, 3.0f};

  return pfmod(parametricInterpolate2(
                   transitionPositions, transitionValues, shiftedTime, SinWeightOperator<float>(), BoundMode::Clamp),
      4.0f);
}

float Sky::skyAlpha() const {
  if (m_skyType != SkyType::Atmospheric) {
    return 0.0f;
  } else {
    float skyLevel = 1.0f - spaceLevel();
    return std::clamp(std::pow(skyLevel, m_numericSettings.skyLevelExponent), 0.0f, 1.0f);
  }
}

Color Sky::environmentLight() const {
  if (m_skyType == SkyType::Orbital || m_skyType == SkyType::Warp)
    return Color::Black;

  if (m_skyParameters.skyColoring.isLeft()) {
    auto const& skyColoring = m_skyParameters.skyColoring.leftRef();

    Array<Vec4F, 4> colors;
    colors[0] = skyColoring.morningLightColor.toRgbaF();
    colors[1] = skyColoring.dayLightColor.toRgbaF();
    colors[2] = skyColoring.eveningLightColor.toRgbaF();
    colors[3] = skyColoring.nightLightColor.toRgbaF();

    return Color::rgbaf(listInterpolate2(colors, dayCycle(), SinWeightOperator<float>(), BoundMode::Wrap));
  } else {
    return m_skyParameters.skyColoring.rightRef();
  }
}

Color Sky::mainSkyColor() const {
  if (m_skyParameters.skyColoring.isLeft())
    return m_skyParameters.skyColoring.leftRef().mainColor;

  return Color::Black;
}

std::pair<Color, Color> Sky::skyRectColors() const {
  if (m_skyParameters.skyColoring.isLeft()) {
    auto const& skyColoring = m_skyParameters.skyColoring.leftRef();

    Array<Vec4F, 4> topColorList;
    topColorList[0] = skyColoring.morningColors.first.toRgbaF();
    topColorList[1] = skyColoring.dayColors.first.toRgbaF();
    topColorList[2] = skyColoring.eveningColors.first.toRgbaF();
    topColorList[3] = skyColoring.nightColors.first.toRgbaF();

    Array<Vec4F, 4> bottomColorList;
    bottomColorList[0] = skyColoring.morningColors.second.toRgbaF();
    bottomColorList[1] = skyColoring.dayColors.second.toRgbaF();
    bottomColorList[2] = skyColoring.eveningColors.second.toRgbaF();
    bottomColorList[3] = skyColoring.nightColors.second.toRgbaF();

    float cycle = dayCycle();
    auto topColor = Color::rgbaf(listInterpolate2(topColorList, cycle, SinWeightOperator<float>(), BoundMode::Wrap));
    auto bottomColor = Color::rgbaf(listInterpolate2(bottomColorList, cycle, SinWeightOperator<float>(), BoundMode::Wrap));

    float skyAlpha = Sky::skyAlpha();
    topColor.setAlpha(topColor.alpha() * skyAlpha);
    bottomColor.setAlpha(bottomColor.alpha() * skyAlpha);

    return {topColor, bottomColor};
  }

  return {Color::Clear, Color::Clear};
}

Color Sky::skyFlashColor() const {
  Color res = Color::White;
  float duration = m_numericSettings.flashTimer;
  res.setAlphaF(duration == 0.0f ? 0.0f : m_flashTimer / duration);
  return res;
}

bool Sky::flying() const {
  if (m_flyingType == FlyingType::None)
    return false;
  return true;
}

FlyingType Sky::flyingType() const {
  return m_flyingType;
}

float Sky::warpProgress() const {
  if (m_flyingType == FlyingType::Warp) {
    auto warpTime = speedupTime() + m_numericSettings.flyingTimer + slowdownTime();
    auto timer = m_flyingTimer;
    if (m_warpPhase < WarpPhase::SpeedingUp)
      timer += speedupTime();
    if (m_warpPhase < WarpPhase::Maintain)
      timer += m_numericSettings.flyingTimer;
    return timer / warpTime;
  }
  return 0.0;
}

WarpPhase Sky::warpPhase() const {
  return m_warpPhase;
}

bool Sky::inHyperspace() const {
  return m_flyingType == FlyingType::Warp && m_enterHyperspace;
}

std::expected<SkyRenderData, StarException> Sky::renderData() const {
  SkyRenderData renderData;
  renderData.settings = m_settings;
  renderData.skyParameters = m_skyParameters;
  renderData.type = m_skyType;
  renderData.dayLevel = dayLevel();
  renderData.skyAlpha = skyAlpha();

  renderData.dayLength = dayLength();
  renderData.timeOfDay = timeOfDay();
  renderData.epochTime = epochTime();

  renderData.starOffset = getStarOffset();
  renderData.starRotation = getStarRotation();
  renderData.worldOffset = getWorldOffset();
  renderData.worldRotation = getWorldRotation();
  renderData.orbitAngle = orbitAngle();

  renderData.starFrames = m_starFrames;
  renderData.starList = m_starList;
  renderData.hyperStarList = m_hyperStarList;

  renderData.environmentLight = environmentLight();
  renderData.mainSkyColor = mainSkyColor();
  std::tie(renderData.topRectColor, renderData.bottomRectColor) = skyRectColors();
  renderData.flashColor = skyFlashColor();

  return renderData;
}

std::expected<void, StarException> Sky::writeNetStates() {
  if (m_skyParametersUpdated) {
    auto json = m_skyParameters.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    auto bytes = DataStreamBuffer::serialize<Json>(*json);
    if (!bytes) return std::unexpected(std::move(bytes.error()));
    m_skyParametersNetState.set(std::move(*bytes));
    m_skyParametersUpdated = false;
  }

  m_skyTypeNetState.set(static_cast<int>(m_skyType));
  m_timeNetState.set(m_time);
  m_enterHyperspaceNetState.set(m_enterHyperspace);
  m_startInWarpNetState.set(m_startInWarp);

  m_flyingTypeNetState.set(static_cast<unsigned>(m_flyingType));
  m_warpPhaseNetState.set(static_cast<int>(m_warpPhase));

  m_flyingTimerNetState.set(m_flyingTimer);
  m_worldMoveNetState.set(m_worldMoveOffset);
  m_starMoveNetState.set(m_starMoveOffset);
  return {};
}

std::expected<void, StarException> Sky::readNetStates() {
  if (m_skyParametersNetState.pullUpdated()) {
    auto json = DataStreamBuffer::deserialize<Json>(m_skyParametersNetState.get());
    if (!json) return std::unexpected(std::move(json.error()));
    auto parameters = SkyParameters::create(*json);
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    m_skyParameters = std::move(*parameters);
    auto updated = skyParametersUpdated();
    if (!updated) return std::unexpected(std::move(updated.error()));
  }

  m_skyType = static_cast<SkyType>(m_skyTypeNetState.get());
  m_time = m_timeNetState.get();
  m_enterHyperspace = m_enterHyperspaceNetState.get();
  m_startInWarp = m_startInWarpNetState.get();

  m_flyingType = static_cast<FlyingType>(m_flyingTypeNetState.get());
  m_warpPhase = static_cast<WarpPhase>(m_warpPhaseNetState.get());
  auto state = stateUpdate();
  if (!state) return std::unexpected(std::move(state.error()));

  if (!m_netInit) {
    m_netInit = true;

    m_flyingTimer = m_flyingTimerNetState.get();
    m_worldMoveOffset = m_worldMoveNetState.get();
    m_starMoveOffset = m_starMoveNetState.get();
  }
  return {};
}

std::expected<void, StarException> Sky::enterHyperspace() {
  m_flashTimer = m_numericSettings.flashTimer;
  setType(SkyType::Warp);

  m_sentSFX = false;
  return {};
}

std::expected<void, StarException> Sky::exitHyperspace() {
  m_flashTimer = m_numericSettings.flashTimer;
  setType(SkyType::Orbital);

  m_sentSFX = false;
  auto origin = m_settings.getObject(m_skyParameters.skyType == SkyType::Space ? String("spaceArrivalOrigin") : String("arrivalOrigin"));
  if (!origin) return std::unexpected(std::move(origin.error()));
  Json originMap = *origin;
  auto offset = originMap.get("offset");
  if (!offset) return std::unexpected(std::move(offset.error()));
  auto position = jsonToVec2F(*offset);
  if (!position) return std::unexpected(std::move(position.error()));
  m_pathOffset = *position;
  auto rotation = originMap.getFloat("rotation");
  if (!rotation) return std::unexpected(std::move(rotation.error()));
  m_pathRotation = *rotation * Constants::deg2rad;

  float maxVelocity = m_numericSettings.flyMaxVelocity;
  float exitDistance = maxVelocity / 2.0 * slowdownTime();
  m_worldMoveOffset = Vec2F::withAngle(0, exitDistance);
  m_worldOffset = m_worldMoveOffset;

  exitDistance = (maxVelocity * m_numericSettings.starVelocityFactor) / 2.0 * slowdownTime();
  m_starMoveOffset = Vec2F::withAngle(0, exitDistance);
  m_starOffset = m_starMoveOffset;

  m_worldRotation = m_starRotation = 0;
  m_flyingTimer = 0;
  return {};
}

std::expected<bool, StarException> Sky::controlledMovement(JsonArray const& path, Json const& origin, float timeOffset) {
  float previousTime = 0;
  auto originOffset = origin.get("offset");
  if (!originOffset) return std::unexpected(std::move(originOffset.error()));
  auto previousPosition = jsonToVec2F(*originOffset);
  if (!previousPosition) return std::unexpected(std::move(previousPosition.error()));
  Vec2F previousOffset = *previousPosition;
  auto originRotation = origin.getFloat("rotation");
  if (!originRotation) return std::unexpected(std::move(originRotation.error()));
  float previousRotation = *originRotation * Constants::deg2rad;

  float stepTime = 0;
  Vec2F stepOffset;
  float stepRotation;
  for (auto const& entry : path) {
    auto offset = entry.get("offset");
    if (!offset) return std::unexpected(std::move(offset.error()));
    auto position = jsonToVec2F(*offset);
    if (!position) return std::unexpected(std::move(position.error()));
    stepOffset = *position;
    auto rotation = entry.getFloat("rotation");
    if (!rotation) return std::unexpected(std::move(rotation.error()));
    stepRotation = *rotation * Constants::deg2rad;
    auto time = entry.getFloat("time");
    if (!time) return std::unexpected(std::move(time.error()));
    stepTime += *time;

    if (timeOffset <= stepTime) {
      float percentage = (timeOffset - previousTime) / (stepTime - previousTime);
      m_pathOffset = lerp(percentage, previousOffset, stepOffset);
      m_pathRotation = lerp(percentage, previousRotation, stepRotation);
      return false;
    }

    previousTime = stepTime;
    previousOffset = stepOffset;
    previousRotation = stepRotation;
  }
  // if loop wasn't broken
  // then we're done with this phase of controlled movement
  // signal that we're ready to head out of this system

  return true;
}

Vec2F Sky::getStarOffset() const {
  return (m_starOffset + m_pathOffset);
}

float Sky::getStarRotation() const {
  return m_starRotation + m_pathRotation;
}

Vec2F Sky::getWorldOffset() const {
  return m_worldOffset + m_pathOffset;
}

float Sky::getWorldRotation() const {
  return m_worldRotation + m_pathRotation;
}

float Sky::speedupTime() const {
  if (m_enterHyperspace)
    return m_numericSettings.hyperspaceSpeedupTime;
  else
    return m_numericSettings.speedupTime;
}

float Sky::slowdownTime() const {
  if (m_enterHyperspace)
    return m_numericSettings.hyperspaceSlowdownTime;
  else
    return m_numericSettings.slowdownTime;
}

std::expected<void, StarException> Sky::skyParametersUpdated() {
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/sky.config");
  if (!config) return std::unexpected(std::move(config.error()));
  auto settings = jsonMerge(*config, m_skyParameters.settings);
  NumericSettings numeric;
  auto flyMaxVelocity = settings.queryFloat("flyMaxVelocity");
  if (!flyMaxVelocity) return std::unexpected(std::move(flyMaxVelocity.error()));
  numeric.flyMaxVelocity = *flyMaxVelocity;
  auto starVelocityFactor = settings.queryFloat("starVelocityFactor");
  if (!starVelocityFactor) return std::unexpected(std::move(starVelocityFactor.error()));
  numeric.starVelocityFactor = *starVelocityFactor;
  auto correctionPower = settings.queryFloat("correctionPower");
  if (!correctionPower) return std::unexpected(std::move(correctionPower.error()));
  numeric.correctionPower = *correctionPower;
  auto flyingTimer = settings.queryFloat("flyingTimer");
  if (!flyingTimer) return std::unexpected(std::move(flyingTimer.error()));
  numeric.flyingTimer = *flyingTimer;
  auto enterHyperspaceAudioLeadIn = settings.queryFloat("enterHyperspaceAudioLeadIn");
  if (!enterHyperspaceAudioLeadIn) return std::unexpected(std::move(enterHyperspaceAudioLeadIn.error()));
  numeric.enterHyperspaceAudioLeadIn = *enterHyperspaceAudioLeadIn;
  auto exitHyperspaceAudioLeadIn = settings.queryFloat("exitHyperspaceAudioLeadIn");
  if (!exitHyperspaceAudioLeadIn) return std::unexpected(std::move(exitHyperspaceAudioLeadIn.error()));
  numeric.exitHyperspaceAudioLeadIn = *exitHyperspaceAudioLeadIn;
  auto dayTransitionTime = settings.queryFloat("dayTransitionTime");
  if (!dayTransitionTime) return std::unexpected(std::move(dayTransitionTime.error()));
  numeric.dayTransitionTime = *dayTransitionTime;
  auto skyLevelExponent = settings.queryFloat("skyLevelExponent");
  if (!skyLevelExponent) return std::unexpected(std::move(skyLevelExponent.error()));
  numeric.skyLevelExponent = *skyLevelExponent;
  auto flashTimer = settings.queryFloat("flashTimer");
  if (!flashTimer) return std::unexpected(std::move(flashTimer.error()));
  numeric.flashTimer = *flashTimer;
  auto hyperspaceSpeedupTime = settings.queryFloat("hyperspaceSpeedupTime");
  if (!hyperspaceSpeedupTime) return std::unexpected(std::move(hyperspaceSpeedupTime.error()));
  numeric.hyperspaceSpeedupTime = *hyperspaceSpeedupTime;
  auto speedupTime = settings.queryFloat("speedupTime");
  if (!speedupTime) return std::unexpected(std::move(speedupTime.error()));
  numeric.speedupTime = *speedupTime;
  auto hyperspaceSlowdownTime = settings.queryFloat("hyperspaceSlowdownTime");
  if (!hyperspaceSlowdownTime) return std::unexpected(std::move(hyperspaceSlowdownTime.error()));
  numeric.hyperspaceSlowdownTime = *hyperspaceSlowdownTime;
  auto slowdownTime = settings.queryFloat("slowdownTime");
  if (!slowdownTime) return std::unexpected(std::move(slowdownTime.error()));
  numeric.slowdownTime = *slowdownTime;
  auto frames = settings.queryInt("stars.frames");
  if (!frames) return std::unexpected(std::move(frames.error()));
  auto stars = settings.query("stars.list");
  if (!stars) return std::unexpected(std::move(stars.error()));
  auto starList = jsonToStringList(*stars);
  if (!starList) return std::unexpected(std::move(starList.error()));
  auto hyperStars = settings.query("stars.hyperlist");
  if (!hyperStars) return std::unexpected(std::move(hyperStars.error()));
  auto hyperStarList = jsonToStringList(*hyperStars);
  if (!hyperStarList) return std::unexpected(std::move(hyperStarList.error()));
  m_settings = std::move(settings);
  m_numericSettings = numeric;
  m_starFrames = *frames;
  m_starList = std::move(*starList);
  m_hyperStarList = std::move(*hyperStarList);
  m_skyParametersUpdated = true;
  return {};
}

}
