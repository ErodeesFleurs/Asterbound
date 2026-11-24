module;
import star.format;

import std;
import star.data_stream;
import star.json;
import star.math_common;

namespace Star {

struct GameTimer {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, GameTimer& gt);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, GameTimer const& gt);
  constexpr GameTimer() = default;
  constexpr explicit GameTimer(float duration) noexcept : duration(duration), remaining(duration) {}

  float duration = 0.0f;
  float remaining = 0.0f;

  constexpr bool tick(float dt) noexcept {
    remaining = approach(0.0f, remaining, dt);
    return ready();
  }

  // At most one expiration per call; discard overshoot, including for signed periods.
  constexpr bool wrap_tick(float dt) noexcept {
    if (!tick(dt))
      return false;
    reset();
    return true;
  }

  constexpr void reset() noexcept { remaining = duration; }
  constexpr void set_done() noexcept { remaining = 0.0f; }
  constexpr void invert() noexcept { remaining = duration - remaining; }

  [[nodiscard]] constexpr bool ready() const noexcept { return remaining == 0.0f; }
  [[nodiscard]] constexpr float remaining_fraction() const noexcept { return duration ? remaining / duration : 0.0f; }
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, GameTimer& gt);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, GameTimer const& gt);

template <typename Provider>
concept sample_provider = requires(Provider& provider) {
  { provider() } -> std::same_as<std::expected<float, StarException>>;
};

class SlidingWindow {
public:
  // Validate stored bits before passing potentially nonfinite values across a fast-math float ABI.
  [[nodiscard]] static std::expected<SlidingWindow, StarException> create(float const& window_size, std::size_t resolution, float const& initial_value);

  template <sample_provider Provider>
  [[nodiscard]] std::expected<void, StarException> update(float dt, Provider&& sample) {
    if (m_sample_timer.wrap_tick(dt)) {
      auto value = sample();
      if (!value) return std::unexpected(std::move(value.error()));
      record_sample(*value);
    }
    return {};
  }

  float average() const { return m_average; }

private:
  SlidingWindow(float period, std::size_t resolution, float initial_value);
  void record_sample(float value);

  GameTimer m_sample_timer;
  float m_average;
  std::size_t m_index = 0;
  std::vector<float> m_window;
};

// A backward epoch does not reduce elapsed time; it becomes the next baseline.
class EpochTimer {
public:
  [[nodiscard]] static std::expected<EpochTimer, StarException> from_json(Json const& json);
  Json to_json() const;

  void update(double epoch_time) noexcept;

  [[nodiscard]] double elapsed_time() const noexcept { return m_elapsed_time; }
  void set_elapsed_time(double elapsed) noexcept { m_elapsed_time = elapsed; }

  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EpochTimer& et);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EpochTimer const& et);

private:
  std::optional<double> m_last_epoch_time;
  double m_elapsed_time = 0.0;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EpochTimer& et);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EpochTimer const& et);

}

export module star.game_timers;

export namespace Star {
  using ::Star::GameTimer;
  using ::Star::SlidingWindow;
  using ::Star::EpochTimer;
}
