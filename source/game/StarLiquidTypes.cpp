import std;
import star.math_common;
import star.liquid_types;

namespace Star {

LiquidLevel LiquidLevel::take(float amount) {
  if (liquid == EmptyLiquidId)
    return LiquidLevel();
  amount = std::min(amount, level);

  LiquidLevel taken = {liquid, amount};

  level -= amount;
  if (level <= 0.0f)
    liquid = EmptyLiquidId;

  return taken;
}

LiquidStore LiquidStore::filled(LiquidId liquid, float level, std::optional<float> pressure) {
  if (liquid == EmptyLiquidId)
    return LiquidStore();
  return LiquidStore(liquid, level, pressure.value_or(level), false);
}

LiquidStore LiquidStore::endless(LiquidId liquid, float pressure) {
  if (liquid == EmptyLiquidId)
    return LiquidStore();
  return LiquidStore(liquid, 1.0f, pressure, true);
}

LiquidStore::LiquidStore() : LiquidLevel(), pressure(0), source(false) {}

LiquidStore::LiquidStore(LiquidId liquid, float level, float pressure, bool source)
  : LiquidLevel(liquid, level), pressure(pressure), source(source) {}

LiquidNetUpdate LiquidStore::netUpdate() const {
  return LiquidNetUpdate{liquid, float_to_byte(level, true)};
}

std::optional<LiquidNetUpdate> LiquidStore::update(LiquidId liquid, float level, float pressure) {
  if (source) {
    if (this->liquid != liquid)
      return {};
    level = std::max(level, this->level);
    pressure = std::max(pressure, this->pressure);
  }

  if (level <= 0.0f) {
    liquid = EmptyLiquidId;
    pressure = 0.0f;
  }

  bool netUpdate = this->liquid != liquid || float_to_byte(this->level, true) != float_to_byte(level, true);

  this->liquid = liquid;
  this->level = level;
  this->pressure = pressure;

  if (netUpdate)
    return LiquidNetUpdate{liquid, float_to_byte(level, true)};
  else
    return {};
}

LiquidLevel LiquidStore::take(float amount) {
  if (source)
    return LiquidLevel(liquid, amount);

  auto taken = LiquidLevel::take(amount);
  if (level == 0.0f)
    pressure = 0.0f;
  return taken;
}

}
