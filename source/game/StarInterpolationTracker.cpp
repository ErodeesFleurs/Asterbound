import star.format;
import star.json;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;

import star.asset_source;
import star.assets;
import star.configuration;
import star.root;

import star.interpolation_tracker;

import star.math_common;
namespace Star {

constexpr double VanillaStepsPerSecond = 60.0;

InterpolationTracker::InterpolationTracker()
  : m_interpolationEnabled(false), m_entityUpdateDelta(3.0 / VanillaStepsPerSecond), m_timeLead(0),
    m_extrapolationHint(0), m_timeTrackFactor(1.0), m_timeMaxDistance(0), m_currentTime(0) {}

std::expected<InterpolationTracker, StarException> InterpolationTracker::create(Root& root, Json config) {
  InterpolationTracker result;
  if (config.is_null()) {
    config = JsonObject();
  } else if (config.type() == Json::Type::String) {
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto path = config.to_string();
    if (!path) return std::unexpected(std::move(path.error()));
    auto loaded = (*assets)->json(*path);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    config = std::move(*loaded);
  }
  if (auto r = result.readConfig(config); !r) return std::unexpected(std::move(r.error()));
  return result;
}

std::expected<void, StarException> InterpolationTracker::readConfig(Json const& config) {

  auto m_interpolationEnabledResult = config.get_bool("interpolationEnabled", false);
  if (!m_interpolationEnabledResult) return std::unexpected(std::move(m_interpolationEnabledResult.error()));
  m_interpolationEnabled = *m_interpolationEnabledResult;
  auto m_entityUpdateDeltaResult = config.get_double("entityUpdateDelta", 3);
  if (!m_entityUpdateDeltaResult) return std::unexpected(std::move(m_entityUpdateDeltaResult.error()));
  m_entityUpdateDelta = *m_entityUpdateDeltaResult / VanillaStepsPerSecond;
  auto m_timeLeadResult = config.get_double("stepLead", 0);
  if (!m_timeLeadResult) return std::unexpected(std::move(m_timeLeadResult.error()));
  m_timeLead = *m_timeLeadResult / VanillaStepsPerSecond;
  auto m_extrapolationHintResult = config.get_uint("extrapolationHint", 0);
  if (!m_extrapolationHintResult) return std::unexpected(std::move(m_extrapolationHintResult.error()));
  m_extrapolationHint = *m_extrapolationHintResult;
  auto m_timeTrackFactorResult = config.get_double("stepTrackFactor", 1.0);
  if (!m_timeTrackFactorResult) return std::unexpected(std::move(m_timeTrackFactorResult.error()));
  m_timeTrackFactor = *m_timeTrackFactorResult;
  auto m_timeMaxDistanceResult = config.get_double("stepMaxDistance", 0.0);
  if (!m_timeMaxDistanceResult) return std::unexpected(std::move(m_timeMaxDistanceResult.error()));
  m_timeMaxDistance = *m_timeMaxDistanceResult / VanillaStepsPerSecond;

  m_currentTime = 0.0;
  return {};
}

bool InterpolationTracker::interpolationEnabled() const {
  return m_interpolationEnabled;
}

unsigned InterpolationTracker::extrapolationHint() const {
  if (m_interpolationEnabled)
    return m_extrapolationHint;
  else
    return 0;
}

float InterpolationTracker::entityUpdateDelta() const {
  return m_entityUpdateDelta;
}

void InterpolationTracker::receiveTimeUpdate(double remoteStep) {
  m_lastTimeUpdate = remoteStep;
}

void InterpolationTracker::update(double newLocalTime) {
  double dt = newLocalTime - m_currentTime;
  m_currentTime = newLocalTime;
  if (!m_predictedTime || !m_lastTimeUpdate || dt < 0.0f) {
    m_predictedTime = m_lastTimeUpdate;
  } else {
    *m_lastTimeUpdate += dt;
    *m_predictedTime += dt;
    *m_predictedTime += (*m_lastTimeUpdate - *m_predictedTime) * m_timeTrackFactor;
    m_predictedTime = clamp<double>(*m_predictedTime, *m_lastTimeUpdate - m_timeMaxDistance, *m_lastTimeUpdate + m_timeMaxDistance);
  }
}

float InterpolationTracker::interpolationLeadTime() const {
  if (!m_interpolationEnabled || !m_predictedTime || !m_lastTimeUpdate)
    return 0.0f;
  return *m_lastTimeUpdate - *m_predictedTime + m_timeLead;
}

}
