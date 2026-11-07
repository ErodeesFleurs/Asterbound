module;

import std;
import star.string;
import star.list;
import star.format;
import star.json;
import star.strong_typedef;
import star.data_stream;
import star.id_map;

namespace Star {

struct StatusExceptionTag {
  static constexpr std::string_view name() { return "StatusException"; }
};
using StatusException = StarError<StatusExceptionTag, StarException>;

// Multipliers act exactly the way you'd expect: 0.0 is a 100% reduction of the
// base stat, while 2.0 is a 100% increase. Since these are *base* multipliers
// they do not interact with each other, thus stacking a 0.0 and a 2.0 leaves
// the stat unmodified
struct StatBaseMultiplier {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, StatBaseMultiplier& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, StatBaseMultiplier const& value);
  String statName;
  float baseMultiplier;

  bool operator==(StatBaseMultiplier const& rhs) const;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, StatBaseMultiplier& baseMultiplier);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, StatBaseMultiplier const& baseMultiplier);

struct StatValueModifier {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, StatValueModifier& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, StatValueModifier const& value);
  String statName;
  float value;

  bool operator==(StatValueModifier const& rhs) const;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, StatValueModifier& valueModifier);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, StatValueModifier const& valueModifier);

// Unlike base multipliers, these all stack multiplicatively with the final
// stat value (including all base and value modifiers) such that an effective
// multiplier of 0.0 will ALWAYS reduce the stat to 0 regardless of other
// effects
struct StatEffectiveMultiplier {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, StatEffectiveMultiplier& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, StatEffectiveMultiplier const& value);
  String statName;
  float effectiveMultiplier;

  bool operator==(StatEffectiveMultiplier const& rhs) const;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, StatEffectiveMultiplier& effectiveMultiplier);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, StatEffectiveMultiplier const& effectiveMultiplier);

using StatModifier = std::variant<std::monostate, StatValueModifier, StatBaseMultiplier, StatEffectiveMultiplier>;

[[nodiscard]] std::expected<StatModifier, StarException> jsonToStatModifier(Json const& config);
[[nodiscard]] std::expected<Json, StarException> jsonFromStatModifier(StatModifier const& modifier);

using StatModifierGroupId = std::uint32_t;
using StatModifierGroupMap = IdMap<StatModifierGroupId, List<StatModifier>>;

// Unique stat effects are identified uniquely by name.
using UniqueStatusEffect = String;

// Second element here is *percentage* of duration remaining, based on the
// highest duration that the effect has had
using ActiveUniqueStatusEffectSummary = List<std::pair<UniqueStatusEffect, std::optional<float>>>;

// Persistent status effects can either be a modifier effect or unique effect
using PersistentStatusEffect = std::variant<std::monostate, StatModifier, UniqueStatusEffect>;

// Reads either a name of a unique stat effect or a stat modifier object
[[nodiscard]] std::expected<PersistentStatusEffect, StarException> jsonToPersistentStatusEffect(Json const& config);
[[nodiscard]] std::expected<Json, StarException> jsonFromPersistentStatusEffect(PersistentStatusEffect const& effect);

// Ephemeral effects are always unique effects and either use the default
// duration in their config or optionally the default
struct EphemeralStatusEffect {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EphemeralStatusEffect& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EphemeralStatusEffect const& value);
  UniqueStatusEffect uniqueEffect;
  std::optional<float> duration;

  bool operator==(EphemeralStatusEffect const& rhs) const;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EphemeralStatusEffect& ephemeralStatusEffect);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EphemeralStatusEffect const& ephemeralStatusEffect);

// Reads either a name of a unique stat effect or an object containing the
// type name and optionally the duration.
[[nodiscard]] std::expected<EphemeralStatusEffect, StarException> jsonToEphemeralStatusEffect(Json const& config);
Json jsonFromEphemeralStatusEffect(EphemeralStatusEffect const& effect);

}

export module star.status_types;

export namespace Star {
  using ::Star::StatusExceptionTag;
  using ::Star::StatusException;
  using ::Star::StatBaseMultiplier;
  using ::Star::StatValueModifier;
  using ::Star::StatEffectiveMultiplier;
  using ::Star::StatModifier;
  using ::Star::jsonToStatModifier;
  using ::Star::jsonFromStatModifier;
  using ::Star::StatModifierGroupId;
  using ::Star::StatModifierGroupMap;
  using ::Star::UniqueStatusEffect;
  using ::Star::ActiveUniqueStatusEffectSummary;
  using ::Star::PersistentStatusEffect;
  using ::Star::jsonToPersistentStatusEffect;
  using ::Star::jsonFromPersistentStatusEffect;
  using ::Star::EphemeralStatusEffect;
  using ::Star::jsonToEphemeralStatusEffect;
  using ::Star::jsonFromEphemeralStatusEffect;
}
