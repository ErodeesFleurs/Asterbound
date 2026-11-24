import star.format;
import star.json;
import star.json_extra;
import star.interpolation;
import star.vector;
import star.bi_map;
import star.data_stream_extra;
import star.ordered_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.map;

import star.asset_source;
import star.assets;
import star.tile_damage;

import star.math_common;
import star.algorithm;
namespace Star {

List<Vec2I> tile_area_brush(float range, Vec2F const& center_offset, bool square_mode) {
  if (range == 0)
    return {};

  List<Vec2I> result;
  float working_range = range * (square_mode ? 1 : 2) + (square_mode ? 0 : 1);
  Vec2F offset = Vec2F::filled(-working_range / 2.0f);
  Vec2I int_offset = Vec2I::round(offset + center_offset);
  for (int x = 0; x < working_range; ++x) {
    for (int y = 0; y < working_range; ++y) {
      Vec2F from_center = Vec2F(x, y) + Vec2F(0.5, 0.5) + offset; // distance to the center of the tile
      Vec2I int_pos = Vec2I(x, y);
      if (square_mode || from_center.magnitude() <= range) {
        result.push_back(int_pos + int_offset);
      }
    }
  }
  std::sort(result.begin(), result.end(), [](Vec2I const& a, Vec2I const& b) {
    auto ams = a.magnitude_squared();
    auto bms = b.magnitude_squared();
    return std::tie(ams, a) < std::tie(bms, b);
  });

  return result;
}

extern std::expected<EnumMap<TileDamageType>, StarException> const tile_damage_type_names = EnumMap<TileDamageType>::create({
  {TileDamageType::Protected, "protected"},
  {TileDamageType::Plantish, "plantish"},
  {TileDamageType::Blockish, "blockish"},
  {TileDamageType::Beamish, "beamish"},
  {TileDamageType::Explosive, "explosive"},
  {TileDamageType::Fire, "fire"},
  {TileDamageType::Tilling, "tilling"}
});

bool tile_damage_is_penetrating(TileDamageType damage_type) {
  return damage_type == TileDamageType::Explosive;
}

TileDamage::TileDamage() : type(), amount(), harvest_level() {}

TileDamage::TileDamage(TileDamageType type, float amount, unsigned harvest_level)
  : type(type), amount(amount), harvest_level(harvest_level) {}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TileDamage& tile_damage) {
  if (auto r = ds.read(tile_damage.type); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tile_damage.amount); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tile_damage.harvest_level); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TileDamage const& tile_damage) {
  if (auto r = ds.write(tile_damage.type); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tile_damage.amount); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tile_damage.harvest_level); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

TileDamageParameters::TileDamageParameters()
  : m_damage_recovery_per_second(0.0f), m_maximum_effect_time(0.0f), m_total_health(0), m_required_harvest_level(0) {}

std::expected<TileDamageParameters, StarException> TileDamageParameters::create(Assets const& assets, Json const& config, std::optional<float> health_override, std::optional<unsigned> harvest_level_override) {
  if (config.type() != Json::Type::String)
    return parse_config(config, health_override, harvest_level_override);

  auto path = config.string_ptr();
  if (!path) return std::unexpected(std::move(path.error()));
  auto parsed = assets.json(**path).and_then([&](Json const& loaded) {
    return parse_config(loaded, health_override, harvest_level_override);
  });
  if (!parsed)
    return std::unexpected(assets.with_asset_context(std::move(parsed.error()), **path, "Loading tile damage parameters"));
  return parsed;
}

std::expected<TileDamageParameters, StarException> TileDamageParameters::parse_config(Json const& config, std::optional<float> health_override, std::optional<unsigned> harvest_level_override) {
  TileDamageParameters result;
  if (!tile_damage_type_names) return std::unexpected(tile_damage_type_names.error());
  auto factors = config.get("damageFactors").and_then([](Json const& value) { return value.iterate_object(); });
  if (!factors) return std::unexpected(std::move(factors.error()));
  for (auto const& pair : *factors) {
    auto type = tile_damage_type_names->get_left(pair.first);
    if (!type) return std::unexpected(std::move(type.error()));
    auto amount = pair.second.to_float();
    if (!amount) return std::unexpected(std::move(amount.error()));
    result.m_damages[type->get()] = *amount;
  }
  auto recovery = config.get_float("damageRecovery");
  if (!recovery) return std::unexpected(std::move(recovery.error()));
  result.m_damage_recovery_per_second = *recovery;
  // Configs use harvestLevel; persisted plant variants use requiredHarvestLevel.
  auto config_key = config.contains("harvestLevel");
  if (!config_key) return std::unexpected(std::move(config_key.error()));
  auto harvest = *config_key ? config.get_uint("harvestLevel", 1) : config.get_uint("requiredHarvestLevel", 1);
  if (!harvest) return std::unexpected(std::move(harvest.error()));
  result.m_required_harvest_level = *harvest;
  auto maximum = config.get_float("maximumEffectTime", 1.5);
  if (!maximum) return std::unexpected(std::move(maximum.error()));
  result.m_maximum_effect_time = *maximum;
  auto health = config.get_float("totalHealth", 1.0f);
  if (!health) return std::unexpected(std::move(health.error()));
  result.m_total_health = health_override.value_or(*health);
  if (!is_finite(result.m_total_health) || result.m_total_health < 0)
    return std::unexpected(TileDamageException("Tile totalHealth must be finite and nonnegative"));
  if (harvest_level_override) result.m_required_harvest_level = *harvest_level_override;
  return result;
}

namespace {
float health_fraction(float amount, float health) {
  // A zero-health tile breaks immediately from positive effective damage, not from an immune hit.
  return health != 0 ? amount / health : float((amount > 0) - (amount < 0));
}
}

float TileDamageParameters::damage_fraction(TileDamage const& damage) const {
  return health_fraction(m_damages.value(damage.type) * damage.amount, m_total_health);
}

float TileDamageParameters::recovery_fraction(float dt) const {
  return health_fraction(m_damage_recovery_per_second * dt, m_total_health);
}

unsigned TileDamageParameters::required_harvest_level() const {
  return m_required_harvest_level;
}

float TileDamageParameters::maximum_effect_time() const {
  return m_maximum_effect_time;
}

float TileDamageParameters::total_health() const {
  return m_total_health;
}

std::expected<TileDamageParameters, StarException> TileDamageParameters::sum(this TileDamageParameters result, TileDamageParameters const& other) {
  auto left_health = result.m_total_health;
  double combined_health = double(left_health) + other.m_total_health;
  if (combined_health > std::numeric_limits<float>::max())
    return std::unexpected(TileDamageException("Combined tile totalHealth exceeds the float range"));

  result.m_damage_recovery_per_second += other.m_damage_recovery_per_second;
  result.m_total_health = static_cast<float>(combined_health);

  result.m_required_harvest_level = std::max(result.m_required_harvest_level, other.m_required_harvest_level);
  result.m_maximum_effect_time = std::max(result.m_maximum_effect_time, other.m_maximum_effect_time);

  for (auto const& [type, right] : other.m_damages) {
    auto [entry, inserted] = result.m_damages.try_emplace(type, right);
    if (inserted)
      continue;
    float& left = entry->second;
    if (left == 0 || right == 0)
      left = 0; // An explicit immunity is not a missing factor.
    else if (left_health == 0 && other.m_total_health == 0)
      left = std::min(left, right); // No health weights; keep the less susceptible factor.
    else if (left_health == 0)
      left = right;
    else if (other.m_total_health != 0)
      left = combined_health / (double(left_health) / left + double(other.m_total_health) / right);
  }

  return result;
}

std::expected<Json, StarException> TileDamageParameters::to_json() const {
  if (!tile_damage_type_names) return std::unexpected(tile_damage_type_names.error());
  JsonObject factors;
  for (auto const& pair : m_damages) {
    auto name = tile_damage_type_names->get_right(pair.first);
    if (!name) return std::unexpected(std::move(name.error()));
    factors.emplace(name->get(), pair.second);
  }
  return JsonObject{{"damageFactors", std::move(factors)},
    {"damageRecovery", m_damage_recovery_per_second},
    {"requiredHarvestLevel", m_required_harvest_level},
    {"maximumEffectTime", m_maximum_effect_time}, {"totalHealth", m_total_health}};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TileDamageParameters& tile_damage) {
  if (auto r = ds.read_map_container(tile_damage.m_damages); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tile_damage.m_damage_recovery_per_second); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tile_damage.m_required_harvest_level); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tile_damage.m_maximum_effect_time); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tile_damage.m_total_health); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TileDamageParameters const& tile_damage) {
  if (auto r = ds.write_map_container(tile_damage.m_damages); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tile_damage.m_damage_recovery_per_second); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tile_damage.m_required_harvest_level); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tile_damage.m_maximum_effect_time); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tile_damage.m_total_health); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

TileDamageStatus::TileDamageStatus() {
  reset();
}

void TileDamageStatus::reset() {
  m_damage_percentage = 0.0f;
  m_damage_effect_time_factor = 0.0f;
  m_harvested = false;
  m_damage_source_position = Vec2F();
  m_damage_type = TileDamageType::Protected;
  m_damage_effect_percentage = 0.0f;
}

void TileDamageStatus::damage(TileDamageParameters const& damage_parameters, Vec2F const& source_position, TileDamage const& damage) {
  auto percentage_delta = damage_parameters.damage_fraction(damage);
  m_damage_percentage = std::min(1.0f, m_damage_percentage + percentage_delta);
  m_harvested = damage.harvest_level >= damage_parameters.required_harvest_level();
  m_damage_source_position = source_position;
  m_damage_type = damage.type;

  if (percentage_delta > 0)
    m_damage_effect_time_factor = damage_parameters.maximum_effect_time();

  update_damage_effect_percentage();
}

void TileDamageStatus::recover(TileDamageParameters const& damage_parameters, float dt) {
  // Once the tile becomes dead, it should not recover from it
  if (healthy() || dead())
    return;

  m_damage_percentage -= damage_parameters.recovery_fraction(dt);
  m_damage_effect_time_factor -= dt;

  if (m_damage_percentage <= 0.0f) {
    m_damage_percentage = 0.0f;
    m_damage_effect_time_factor = 0.0f;
    m_damage_type = TileDamageType::Protected;
  }

  update_damage_effect_percentage();
}

bool TileDamageStatus::healthy() const {
  return m_damage_percentage <= 0.0f;
}

bool TileDamageStatus::damaged() const {
  return m_damage_percentage > 0.0f;
}

bool TileDamageStatus::damage_protected() const {
  return m_damage_type == TileDamageType::Protected;
}

bool TileDamageStatus::dead() const {
  return m_damage_percentage >= 1.0f && m_damage_type != TileDamageType::Protected;
}

bool TileDamageStatus::harvested() const {
  return m_harvested;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TileDamageStatus& tile_damage_status) {
  if (auto r = ds.read(tile_damage_status.m_damage_percentage); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tile_damage_status.m_damage_effect_time_factor); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tile_damage_status.m_harvested); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tile_damage_status.m_damage_source_position); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tile_damage_status.m_damage_type); !r) return std::unexpected(std::move(r.error()));

  tile_damage_status.update_damage_effect_percentage();

  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TileDamageStatus const& tile_damage_status) {
  if (auto r = ds.write(tile_damage_status.m_damage_percentage); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tile_damage_status.m_damage_effect_time_factor); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tile_damage_status.m_harvested); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tile_damage_status.m_damage_source_position); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tile_damage_status.m_damage_type); !r) return std::unexpected(std::move(r.error()));

  return std::ref(ds);
}

void TileDamageStatus::update_damage_effect_percentage() {
  m_damage_effect_percentage = std::clamp(m_damage_effect_time_factor, 0.0f, 1.0f) * m_damage_percentage;
}

EntityTileDamageStatus::EntityTileDamageStatus(ConstructionKey) {
  m_damage_percentage.set_fixed_point_base(0.001f);
  m_damage_effect_time_factor.set_fixed_point_base(0.001f);
  m_damage_percentage.set_interpolator(lerp<float, float>);
  m_damage_effect_time_factor.set_interpolator(lerp<float, float>);

}

std::expected<std::shared_ptr<EntityTileDamageStatus>, StarException> EntityTileDamageStatus::create() {
  auto status = std::make_shared<EntityTileDamageStatus>(ConstructionKey{});
  if (auto result = status->add_net_element(status->m_damage_percentage); !result) return std::unexpected(std::move(result.error()));
  if (auto result = status->add_net_element(status->m_damage_effect_time_factor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = status->add_net_element(status->m_damage_harvested); !result) return std::unexpected(std::move(result.error()));
  if (auto result = status->add_net_element(status->m_damage_type); !result) return std::unexpected(std::move(result.error()));
  return status;
}

float EntityTileDamageStatus::damage_percentage() const {
  return m_damage_percentage.get();
}

float EntityTileDamageStatus::damage_effect_percentage() const {
  return std::clamp(m_damage_effect_time_factor.get(), 0.0f, 1.0f) * m_damage_percentage.get();
}

TileDamageType EntityTileDamageStatus::damage_type() const {
  return m_damage_type.get();
}

void EntityTileDamageStatus::reset() {
  m_damage_percentage.set(0.0f);
  m_damage_effect_time_factor.set(0.0f);
  m_damage_harvested.set(false);
}

void EntityTileDamageStatus::damage(TileDamageParameters const& damage_parameters, TileDamage const& damage) {
  auto percentage_delta = damage_parameters.damage_fraction(damage);
  m_damage_percentage.set(std::min(1.0f, m_damage_percentage.get() + percentage_delta));
  m_damage_harvested.set(damage.harvest_level >= damage_parameters.required_harvest_level());
  m_damage_type.set(damage.type);

  if (percentage_delta > 0)
    m_damage_effect_time_factor.set(damage_parameters.maximum_effect_time());
}

void EntityTileDamageStatus::recover(TileDamageParameters const& damage_parameters, float dt) {
  // Once the tile becomes dead, it should not recover from it
  if (healthy() || dead())
    return;

  m_damage_percentage.set(m_damage_percentage.get() - damage_parameters.recovery_fraction(dt));
  m_damage_effect_time_factor.set(m_damage_effect_time_factor.get() - dt);

  if (m_damage_percentage.get() <= 0.0f) {
    m_damage_percentage.set(0.0f);
    m_damage_effect_time_factor.set(0.0f);
    m_damage_type.set(TileDamageType::Protected);
  }
}

bool EntityTileDamageStatus::healthy() const {
  return m_damage_percentage.get() <= 0.0f;
}

bool EntityTileDamageStatus::damaged() const {
  return m_damage_percentage.get() > 0.0f;
}

bool EntityTileDamageStatus::damage_protected() const {
  return m_damage_type.get() == TileDamageType::Protected;
}

bool EntityTileDamageStatus::dead() const {
  return m_damage_percentage.get() >= 1.0f && m_damage_type.get() != TileDamageType::Protected;
}

bool EntityTileDamageStatus::harvested() const {
  return m_damage_harvested.get();
}

}
