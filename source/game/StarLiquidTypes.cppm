module;
import std;

import star.math_common;

namespace Star {

using LiquidId = std::uint8_t;
inline constexpr LiquidId EmptyLiquidId = 0;

struct LiquidLevel {
  LiquidLevel();
  LiquidLevel(LiquidId liquid, float level);

  LiquidLevel take(float amount);

  LiquidId liquid;
  float level;
};

struct LiquidNetUpdate {
  LiquidId liquid;
  std::uint8_t level;

  LiquidLevel liquidLevel() const;
};

struct LiquidStore : LiquidLevel {
  // Returns a LiquidStore of the given liquid
  static LiquidStore filled(LiquidId liquid, float level, std::optional<float> pressure = {});
  // Returns a LiquidStore source liquid block
  static LiquidStore endless(LiquidId liquid, float pressure);

  LiquidStore();
  LiquidStore(LiquidId liquid, float level, float pressure, bool source);

  LiquidNetUpdate netUpdate() const;

  std::optional<LiquidNetUpdate> update(LiquidId liquid, float level, float pressure);

  LiquidLevel take(float amount);

  float pressure;
  bool source;
};

inline LiquidLevel::LiquidLevel()
  : liquid(EmptyLiquidId), level(0.0f) {}

inline LiquidLevel::LiquidLevel(LiquidId liquid, float level)
  : liquid(liquid), level(level) {}

inline LiquidLevel LiquidNetUpdate::liquidLevel() const {
  return LiquidLevel{liquid, byteToFloat(level)};
}

}

export module star.liquid_types;

export namespace Star {
  using ::Star::LiquidId;
  using ::Star::EmptyLiquidId;
  using ::Star::LiquidLevel;
  using ::Star::LiquidNetUpdate;
  using ::Star::LiquidStore;
}
