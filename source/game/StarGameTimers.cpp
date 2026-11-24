import star.format;
import std;
import star.data_stream;
import star.json;
import star.json_extra;
import star.data_stream_extra;

import star.game_timers;

import star.math_common;
namespace Star {

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, GameTimer& timer) {
  GameTimer decoded;
  auto read = ds >> decoded.duration >> decoded.remaining;
  if (read)
    timer = decoded;
  return read;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, GameTimer const& timer) {
  return ds << timer.duration << timer.remaining;
}

std::expected<SlidingWindow, StarException> SlidingWindow::create(float const& window_size, std::size_t resolution, float const& initial_value) {
  if (!resolution || resolution > std::vector<float>().max_size())
    return std::unexpected(StarException("Invalid immersionResolution: expected a positive sample count"));
  if (!is_finite(window_size) || window_size <= 0)
    return std::unexpected(StarException("Invalid immersionWindow: expected a positive finite duration"));
  float period = window_size / resolution;
  if (period == 0)
    return std::unexpected(StarException("Invalid immersionWindow / immersionResolution: sample period underflows"));
  if (!is_finite(initial_value))
    return std::unexpected(StarException("Invalid initial immersion: expected a finite value"));
  return SlidingWindow(period, resolution, initial_value);
}

SlidingWindow::SlidingWindow(float period, std::size_t resolution, float initial_value)
  : m_sample_timer(period), m_average(initial_value), m_window(resolution, initial_value) {}

void SlidingWindow::record_sample(float value) {
#ifdef __clang__
#pragma clang fp reassociate(off)
#endif
  if (++m_index == m_window.size())
    m_index = 0;
  m_window[m_index] = value;

  // The physical slot order matters at the crop's immersion threshold.
  float total = 0;
  for (float entry : m_window)
    total += entry;
  m_average = total / m_window.size();
}

std::expected<EpochTimer, StarException> EpochTimer::from_json(Json const& json) {
  auto last = json.get("lastEpochTime").and_then([](Json const& value) { return value.opt_double(); });
  if (!last)
    return std::unexpected(std::move(last.error()));
  auto elapsed = json.get_double("elapsedTime");
  if (!elapsed)
    return std::unexpected(std::move(elapsed.error()));
  EpochTimer timer;
  timer.m_last_epoch_time = *last;
  timer.m_elapsed_time = *elapsed;
  return timer;
}

Json EpochTimer::to_json() const {
  return JsonObject{{"lastEpochTime", json_from_optional(m_last_epoch_time)}, {"elapsedTime", m_elapsed_time}};
}

void EpochTimer::update(double epoch_time) noexcept {
  if (m_last_epoch_time) {
    double delta = epoch_time - *m_last_epoch_time;
    if (delta > 0.0)
      m_elapsed_time += delta;
  }
  m_last_epoch_time = epoch_time;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EpochTimer& timer) {
  EpochTimer decoded;
  auto read = ds >> decoded.m_last_epoch_time >> decoded.m_elapsed_time;
  if (read)
    timer = decoded;
  return read;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EpochTimer const& timer) {
  return ds << timer.m_last_epoch_time << timer.m_elapsed_time;
}

}
