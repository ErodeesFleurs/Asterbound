import star.format;
import std;
import star.string;
import star.json;
import star.json_extra;
import star.strong_typedef;
import star.data_stream;
import star.id_map;
import star.status_types;
import star.data_stream_extra;

namespace Star {

bool StatBaseMultiplier::operator==(StatBaseMultiplier const& rhs) const {
  return std::tie(statName, baseMultiplier) == std::tie(rhs.statName, rhs.baseMultiplier);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, StatBaseMultiplier& value) {
  return ds >> value.statName >> value.baseMultiplier;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, StatBaseMultiplier const& value) {
  return ds << value.statName << value.baseMultiplier;
}

bool StatValueModifier::operator==(StatValueModifier const& rhs) const {
  return std::tie(statName, value) == std::tie(rhs.statName, rhs.value);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, StatValueModifier& value) {
  return ds >> value.statName >> value.value;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, StatValueModifier const& value) {
  return ds << value.statName << value.value;
}

bool StatEffectiveMultiplier::operator==(StatEffectiveMultiplier const& rhs) const {
  return std::tie(statName, effectiveMultiplier) == std::tie(rhs.statName, rhs.effectiveMultiplier);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, StatEffectiveMultiplier& value) {
  return ds >> value.statName >> value.effectiveMultiplier;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, StatEffectiveMultiplier const& value) {
  return ds << value.statName << value.effectiveMultiplier;
}

std::expected<StatModifier, StarException> jsonToStatModifier(Json const& config) {
  auto statName = config.getString("stat");
  if (!statName) return std::unexpected(std::move(statName.error()));
  auto baseMultiplier = config.optFloat("baseMultiplier");
  if (!baseMultiplier) return std::unexpected(std::move(baseMultiplier.error()));
  if (*baseMultiplier)
    return StatModifier(StatBaseMultiplier{.statName = std::move(*statName), .baseMultiplier = **baseMultiplier});
  auto amount = config.optFloat("amount");
  if (!amount) return std::unexpected(std::move(amount.error()));
  if (*amount)
    return StatModifier(StatValueModifier{.statName = std::move(*statName), .value = **amount});
  auto effectiveMultiplier = config.optFloat("effectiveMultiplier");
  if (!effectiveMultiplier) return std::unexpected(std::move(effectiveMultiplier.error()));
  if (*effectiveMultiplier)
    return StatModifier(
      StatEffectiveMultiplier{.statName = std::move(*statName), .effectiveMultiplier = **effectiveMultiplier});
  return std::unexpected(JsonException("Could not find 'baseMultiplier' or 'effectiveMultiplier' or 'amount' element in stat effect config"));
}

std::expected<Json, StarException> jsonFromStatModifier(StatModifier const& modifier) {
  if (std::holds_alternative<StatBaseMultiplier>(modifier)) {
    auto const& value = std::get<StatBaseMultiplier>(modifier);
    return JsonObject{{"stat", value.statName}, {"baseMultiplier", value.baseMultiplier}};
  }
  if (std::holds_alternative<StatValueModifier>(modifier)) {
    auto const& value = std::get<StatValueModifier>(modifier);
    return JsonObject{{"stat", value.statName}, {"amount", value.value}};
  }
  if (std::holds_alternative<StatEffectiveMultiplier>(modifier)) {
    auto const& value = std::get<StatEffectiveMultiplier>(modifier);
    return JsonObject{{"stat", value.statName}, {"effectiveMultiplier", value.effectiveMultiplier}};
  }
  return std::unexpected(JsonException("No 'baseMultiplier', 'amount', or 'effectiveMultiplier' member found in json"));
}

std::expected<PersistentStatusEffect, StarException> jsonToPersistentStatusEffect(Json const& config) {
  if (config.isType(Json::Type::String)) {
    auto name = config.toString();
    if (!name) return std::unexpected(std::move(name.error()));
    return PersistentStatusEffect(std::move(*name));
  }
  if (config.isType(Json::Type::Object)) {
    auto modifier = jsonToStatModifier(config);
    if (!modifier) return std::unexpected(std::move(modifier.error()));
    return PersistentStatusEffect(std::move(*modifier));
  }
  return std::unexpected(JsonException("Json is wrong type for persistent stat effect config"));
}

std::expected<Json, StarException> jsonFromPersistentStatusEffect(PersistentStatusEffect const& effect) {
  if (std::holds_alternative<UniqueStatusEffect>(effect))
    return Json(std::get<UniqueStatusEffect>(effect));
  if (std::holds_alternative<StatModifier>(effect))
    return jsonFromStatModifier(std::get<StatModifier>(effect));
  return Json();
}

bool EphemeralStatusEffect::operator==(EphemeralStatusEffect const& rhs) const {
  return std::tie(uniqueEffect, duration) == std::tie(rhs.uniqueEffect, rhs.duration);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EphemeralStatusEffect& value) {
  return ds >> value.uniqueEffect >> value.duration;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EphemeralStatusEffect const& value) {
  return ds << value.uniqueEffect << value.duration;
}

std::expected<EphemeralStatusEffect, StarException> jsonToEphemeralStatusEffect(Json const& config) {
  if (config.isType(Json::Type::String)) {
    auto name = config.toString();
    if (!name) return std::unexpected(std::move(name.error()));
    return EphemeralStatusEffect{.uniqueEffect = std::move(*name), .duration = {}};
  }
  if (config.isType(Json::Type::Object)) {
    auto name = config.getString("effect");
    if (!name) return std::unexpected(std::move(name.error()));
    auto duration = config.optFloat("duration");
    if (!duration) return std::unexpected(std::move(duration.error()));
    return EphemeralStatusEffect{.uniqueEffect = std::move(*name), .duration = *duration};
  }
  return std::unexpected(JsonException("Json is wrong type for ephemeral stat effect config"));
}

Json jsonFromEphemeralStatusEffect(EphemeralStatusEffect const& effect) {
  return JsonObject{{"effect", effect.uniqueEffect}, {"duration", jsonFromOptional(effect.duration)}};
}

}
