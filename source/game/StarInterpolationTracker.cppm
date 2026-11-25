module;
import std;

import star.format;

namespace Star {

class Json;
class Root;

class InterpolationTracker {
public:
  [[nodiscard]] static std::expected<InterpolationTracker, StarException> from_config(Json const& config);
  [[nodiscard]] static std::expected<InterpolationTracker, StarException> create(Root& root, Json const& config);

  [[nodiscard]] bool interpolation_enabled() const { return m_interpolation_enabled; }
  [[nodiscard]] unsigned extrapolation_hint() const { return m_extrapolation_hint; }
  // Seconds between entity updates, even when interpolation is disabled.
  [[nodiscard]] float entity_update_delta() const { return m_entity_update_delta; }

  // The packet decoder has already validated the remote timestamp. Local time
  // is the world's own simulation clock; it may stay still or roll back.
  void receive_time_update(double remote_time) {
    if (m_interpolation_enabled)
      m_remote_time = remote_time;
  }
  void update(double local_time);

  // Evaluated against the latest heartbeat, even before the next update().
  [[nodiscard]] float interpolation_lead_time() const;

private:
  bool m_interpolation_enabled = false;
  unsigned m_extrapolation_hint = 0;
  float m_entity_update_delta = 0.05f;
  double m_time_lead = 0;
  double m_time_track_factor = 1;
  double m_time_max_distance = 0;

  double m_local_time = 0;
  std::optional<double> m_remote_time;
  std::optional<double> m_predicted_time;
};

}

export module star.interpolation_tracker;

export namespace Star {
using ::Star::InterpolationTracker;
}
