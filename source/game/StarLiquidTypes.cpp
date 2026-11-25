import std;
import star.math_common;
import star.liquid_types;

namespace Star {

void LiquidLevel::drain(float amount) {
  if (liquid == empty_liquid_id || amount <= 0.0f || !is_finite(amount))
    return;

  level -= std::min(amount, level);
  if (level <= 0.0f)
    liquid = empty_liquid_id;
}

LiquidStore LiquidStore::filled(LiquidId liquid, float level, std::optional<float> pressure) {
  if (liquid == empty_liquid_id)
    return {};
  return LiquidStore(liquid, level, pressure.value_or(level), false);
}

LiquidStore LiquidStore::endless(LiquidId liquid, float pressure) {
  if (liquid == empty_liquid_id)
    return {};
  return LiquidStore(liquid, 1.0f, pressure, true);
}

LiquidNetUpdate LiquidStore::net_update() const {
  return LiquidNetUpdate{liquid, float_to_byte(level, true)};
}

bool LiquidStore::update(LiquidId liquid, float level, float pressure) {
  if (source) {
    if (this->liquid != liquid)
      return false;
    level = std::max(level, this->level);
    pressure = std::max(pressure, this->pressure);
  }

  if (level <= 0.0f) {
    liquid = empty_liquid_id;
    pressure = 0.0f;
  }

  bool wire_changed = this->liquid != liquid
      || (this->level != level && float_to_byte(this->level, true) != float_to_byte(level, true));

  this->liquid = liquid;
  this->level = level;
  this->pressure = pressure;

  return wire_changed;
}

void LiquidStore::drain(float amount) {
  if (source || liquid == empty_liquid_id)
    return;
  LiquidLevel::drain(amount);
  if (liquid == empty_liquid_id)
    pressure = 0.0f;
}

}
