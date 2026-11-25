module;
import std;

import star.math_common;

namespace Star {

using LiquidId = std::uint8_t;
inline constexpr LiquidId empty_liquid_id = 0;

struct LiquidLevel {
  void drain(float amount);

  LiquidId liquid = empty_liquid_id;
  float level = 0.0f;
};

struct LiquidNetUpdate {
  LiquidId liquid = empty_liquid_id;
  std::uint8_t level = 0;

  [[nodiscard]] constexpr LiquidLevel liquid_level() const {
    return {liquid, byte_to_float(level)};
  }
};

struct LiquidStore : LiquidLevel {
  // Returns a LiquidStore of the given liquid
  static LiquidStore filled(LiquidId liquid, float level, std::optional<float> pressure = {});
  // Returns a LiquidStore source liquid block
  static LiquidStore endless(LiquidId liquid, float pressure);

  LiquidStore() = default;
  constexpr LiquidStore(LiquidId liquid, float level, float pressure, bool source)
    : LiquidLevel{liquid, level}, pressure(pressure), source(source) {}

  [[nodiscard]] LiquidNetUpdate net_update() const;

  // Returns whether the liquid ID or quantized level changed. Physics uses the
  // full level and pressure even when there is no change to send.
  [[nodiscard]] bool update(LiquidId liquid, float level, float pressure);

  void drain(float amount);

  float pressure = 0.0f;
  bool source = false;
};

}

export module star.liquid_types;

export namespace Star {
  using ::Star::LiquidId;
  using ::Star::empty_liquid_id;
  using ::Star::LiquidLevel;
  using ::Star::LiquidNetUpdate;
  using ::Star::LiquidStore;
}
