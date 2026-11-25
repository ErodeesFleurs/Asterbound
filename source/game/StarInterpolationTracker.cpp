import std;
import star.assets;
import star.format;
import star.interpolation_tracker;
import star.json;
import star.math_common;
import star.root;
import star.string;

namespace Star {
namespace {
constexpr double vanilla_steps_per_second = 60;
constexpr double max_float_seconds = std::numeric_limits<float>::max();
}

std::expected<InterpolationTracker, StarException> InterpolationTracker::create(Root& root, Json const& config) {
  if (!config.is_type(Json::Type::String))
    return from_config(config);
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto path = config.string_ptr();
  if (!path) return std::unexpected(std::move(path.error()));
  auto loaded = (*assets)->json(**path);
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  return from_config(*loaded);
}

std::expected<InterpolationTracker, StarException> InterpolationTracker::from_config(Json const& config) {
  InterpolationTracker tracker;
  if (config.is_null())
    return tracker;

  auto enabled = config.get_bool("interpolationEnabled", false);
  if (!enabled) return std::unexpected(std::move(enabled.error()));
  tracker.m_interpolation_enabled = *enabled;

  auto interval = config.get_double("entityUpdateDelta", 3);
  if (!interval) return std::unexpected(std::move(interval.error()));
  if (!is_finite(*interval) || *interval < 0 || *interval > max_float_seconds * vanilla_steps_per_second)
    return std::unexpected(JsonException("entityUpdateDelta must be a finite nonnegative float interval in seconds"));
  tracker.m_entity_update_delta = static_cast<float>(*interval / vanilla_steps_per_second);
  if (*interval != 0 && tracker.m_entity_update_delta == 0)
    return std::unexpected(JsonException("entityUpdateDelta underflows to a zero-second interval"));

  auto lead = config.get_double("stepLead", 0);
  if (!lead) return std::unexpected(std::move(lead.error()));
  if (!is_finite(*lead))
    return std::unexpected(JsonException("stepLead must be finite"));
  tracker.m_time_lead = *lead / vanilla_steps_per_second;

  auto hint = config.get_double("extrapolationHint", 0);
  if (!hint) return std::unexpected(std::move(hint.error()));
  // Like Json::to_uint, truncate fractional values towards zero. Check the
  // destination range before converting (including floats outside uint64_t).
  if (!is_finite(*hint) || *hint <= -1 || *hint >= std::ldexp(1.0, std::numeric_limits<unsigned>::digits))
    return std::unexpected(JsonException("extrapolationHint must fit in an unsigned integer"));
  if (*enabled)
    tracker.m_extrapolation_hint = static_cast<unsigned>(*hint);

  auto factor = config.get_double("stepTrackFactor", 1);
  if (!factor) return std::unexpected(std::move(factor.error()));
  if (!is_finite(*factor))
    return std::unexpected(JsonException("stepTrackFactor must be finite"));
  tracker.m_time_track_factor = *factor;

  auto distance = config.get_double("stepMaxDistance", 0);
  if (!distance) return std::unexpected(std::move(distance.error()));
  if (!is_finite(*distance) || *distance < 0)
    return std::unexpected(JsonException("stepMaxDistance must be finite and nonnegative"));
  tracker.m_time_max_distance = *distance / vanilla_steps_per_second;
  return tracker;
}

// Preserve the sequence of binary64 clock additions before correcting the
// prediction. Extreme finite heartbeats can overflow the difference itself.
#pragma float_control(precise, on, push)
void InterpolationTracker::update(double local_time) {
  if (!m_interpolation_enabled)
    return;
  auto const dt = local_time - m_local_time;
  m_local_time = local_time;
  if (!m_predicted_time || dt < 0) {
    m_predicted_time = m_remote_time;
    return;
  }
  // A prediction is only established from an actual heartbeat.
  auto& remote = *m_remote_time;
  auto& predicted = *m_predicted_time;
  remote += dt;
  predicted += dt;
  predicted += (remote - predicted) * m_time_track_factor;
  auto const lower = remote - m_time_max_distance;
  auto const upper = remote + m_time_max_distance;
  if (predicted < lower) predicted = lower;
  if (predicted > upper) predicted = upper;
  // The correction can be NaN (e.g. an overflowing difference times zero),
  // or an overflowing bound can leave an infinite correction unclipped.
  constexpr auto limit = std::numeric_limits<double>::max();
  if (!(predicted >= -limit && predicted <= limit)) [[unlikely]]
    predicted = remote;
}

float InterpolationTracker::interpolation_lead_time() const {
  if (!m_predicted_time)
    return 0;
  auto const lead = *m_remote_time - *m_predicted_time + m_time_lead;
  return static_cast<float>(std::clamp(lead, -max_float_seconds, max_float_seconds));
}
#pragma float_control(pop)

}
