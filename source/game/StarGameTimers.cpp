import star.format;
import std;
import star.data_stream;
import star.json;
import star.json_extra;
import star.data_stream_extra;

import star.game_timers;

import star.math_common;
namespace Star {

GameTimer::GameTimer() : time(), timer() {}

GameTimer::GameTimer(float time) : time(time) {
  reset();
}

bool GameTimer::tick(float dt) {
  timer = approach(0.0f, timer, dt);
  return timer == 0.0f;
}

bool GameTimer::ready() const {
  return timer == 0.0f;
}

bool GameTimer::wrapTick(float dt) {
  auto res = tick(dt);
  if (res)
    reset();
  return res;
}

void GameTimer::reset() {
  timer = time;
}

void GameTimer::setDone() {
  timer = 0.0f;
}

void GameTimer::invert() {
  timer = time - timer;
}

float GameTimer::percent() const {
  if (time)
    return timer / time;
  else
    return 0.0f;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, GameTimer& gt) {
  if (auto r = ds >> gt.time; !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds >> gt.timer; !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, GameTimer const& gt) {
  if (auto r = ds << gt.time; !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds << gt.timer; !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

SlidingWindow::SlidingWindow() : windowSize(1.0f), resolution(1) {}

SlidingWindow::SlidingWindow(float windowSize, std::size_t resolution, float initialValue)
  : windowSize(windowSize), resolution(resolution) {
  sampleTimer = GameTimer(windowSize / resolution);
  window = std::vector<float>(resolution);
  reset(initialValue);
}

void SlidingWindow::reset(float initialValue) {
  sampleTimer.reset();
  currentIndex = 0;
  currentMin = initialValue;
  currentMax = initialValue;
  currentAverage = initialValue;
  std::fill(window.begin(), window.end(), initialValue);
}

std::expected<void, StarException> SlidingWindow::update(float dt, std::function<std::expected<float, StarException>()> sampleFunction) {
  if (sampleTimer.wrapTick(dt)) {
    if (!sampleFunction) return std::unexpected(StarException("SlidingWindow sample function is empty"));
    auto sample = sampleFunction();
    if (!sample) return std::unexpected(std::move(sample.error()));
    processUpdate(*sample);
  }
  return {};
}

void SlidingWindow::update(float dt, float newValue) {
  if (sampleTimer.wrapTick(dt)) {
    processUpdate(newValue);
  }
}

void SlidingWindow::processUpdate(float newValue) {
  ++currentIndex;
  currentIndex = currentIndex % resolution;
  window[currentIndex] = newValue;

  currentMin = std::numeric_limits<float>::max();
  currentMax = 0;
  float total = 0;
  for (float v : window) {
    total += v;
    currentMin = std::min(currentMin, v);
    currentMax = std::max(currentMax, v);
  }
  currentAverage = total / resolution;
}

float SlidingWindow::min() {
  return currentMin;
}

float SlidingWindow::max() {
  return currentMax;
}

float SlidingWindow::average() {
  return currentAverage;
}

EpochTimer::EpochTimer() : m_elapsedTime(0.0) {}

std::expected<EpochTimer, StarException> EpochTimer::create(Json json) {
  EpochTimer result;
  auto last = json.get("lastEpochTime");
  if (!last) return std::unexpected(std::move(last.error()));
  auto lastTime = last->optDouble();
  if (!lastTime) return std::unexpected(std::move(lastTime.error()));
  result.m_lastSeenEpochTime = *lastTime;
  auto elapsed = json.getDouble("elapsedTime");
  if (!elapsed) return std::unexpected(std::move(elapsed.error()));
  result.m_elapsedTime = *elapsed;
  return result;
}

Json EpochTimer::toJson() const {
  return JsonObject{{"lastEpochTime", json_from_optional(m_lastSeenEpochTime)}, {"elapsedTime", m_elapsedTime}};
}

void EpochTimer::update(double newEpochTime) {
  if (!m_lastSeenEpochTime) {
    m_lastSeenEpochTime = newEpochTime;
  } else {
    // Don't allow elapsed time to go backwards in the case of the epoch time
    // being lost or wrong.
    double difference = newEpochTime - *m_lastSeenEpochTime;
    if (difference > 0)
      m_elapsedTime += difference;
    m_lastSeenEpochTime = newEpochTime;
  }
}

double EpochTimer::elapsedTime() const {
  return m_elapsedTime;
}

void EpochTimer::setElapsedTime(double elapsedTime) {
  m_elapsedTime = elapsedTime;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EpochTimer& et) {
  if (auto r = ds >> et.m_lastSeenEpochTime; !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds >> et.m_elapsedTime; !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EpochTimer const& et) {
  if (auto r = ds << et.m_lastSeenEpochTime; !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds << et.m_elapsedTime; !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

}
