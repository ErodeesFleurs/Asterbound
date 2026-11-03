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
import star.version;

import star.asset_source;
import star.assets;
import star.tile_damage;

import star.math_common;
import star.algorithm;
namespace Star {

List<Vec2I> tileAreaBrush(float range, Vec2F const& centerOffset, bool squareMode) {
  if (range == 0)
    return {};

  List<Vec2I> result;
  float workingRange = range * (squareMode ? 1 : 2) + (squareMode ? 0 : 1);
  Vec2F offset = Vec2F::filled(-workingRange / 2.0f);
  Vec2I intOffset = Vec2I::round(offset + centerOffset);
  for (int x = 0; x < workingRange; ++x) {
    for (int y = 0; y < workingRange; ++y) {
      Vec2F fromCenter = Vec2F(x, y) + Vec2F(0.5, 0.5) + offset; // distance to the center of the tile
      Vec2I intPos = Vec2I(x, y);
      if (squareMode || fromCenter.magnitude() <= range) {
        result.append(intPos + intOffset);
      }
    }
  }
  std::sort(result.begin(), result.end(), [](Vec2I const& a, Vec2I const& b) {
      auto ams = a.magnitudeSquared();
      auto bms = b.magnitudeSquared();
      return std::tie(ams, a) < std::tie(bms, b);
    });

  return result;
}

extern std::expected<EnumMap<TileDamageType>, StarException> const TileDamageTypeNames = EnumMap<TileDamageType>::create({
  {TileDamageType::Protected, "protected"},
  {TileDamageType::Plantish, "plantish"},
  {TileDamageType::Blockish, "blockish"},
  {TileDamageType::Beamish, "beamish"},
  {TileDamageType::Explosive, "explosive"},
  {TileDamageType::Fire, "fire"},
  {TileDamageType::Tilling, "tilling"}
});

bool tileDamageIsPenetrating(TileDamageType damageType) {
  return damageType == TileDamageType::Explosive;
}

TileDamage::TileDamage() : type(), amount(), harvestLevel() {}

TileDamage::TileDamage(TileDamageType type, float amount, unsigned harvestLevel)
  : type(type), amount(amount), harvestLevel(harvestLevel) {}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TileDamage& tileDamage) {
  if (auto r = ds.read(tileDamage.type); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileDamage.amount); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileDamage.harvestLevel); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TileDamage const& tileDamage) {
  if (auto r = ds.write(tileDamage.type); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileDamage.amount); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileDamage.harvestLevel); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

TileDamageParameters::TileDamageParameters()
  : m_damageRecoveryPerSecond(0.0f), m_maximumEffectTime(0.0f), m_totalHealth(0), m_requiredHarvestLevel(0) {}

std::expected<TileDamageParameters, StarException> TileDamageParameters::create(Assets const& assets, Json config, std::optional<float> healthOverride, std::optional<unsigned> harvestLevelOverride) {
  std::optional<String> sourcePath;
  auto parsed = [&]() -> std::expected<TileDamageParameters, StarException> {
  TileDamageParameters result;
  if (config.type() == Json::Type::String) {
    auto path = config.toString();
    if (!path) return std::unexpected(std::move(path.error()));
    sourcePath = std::move(*path);
    auto loaded = assets.json(*sourcePath);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    config = std::move(*loaded);
  }
  if (!TileDamageTypeNames) return std::unexpected(TileDamageTypeNames.error());
  auto factors = config.getObject("damageFactors");
  if (!factors) return std::unexpected(std::move(factors.error()));
  for (auto const& pair : *factors) {
    auto type = TileDamageTypeNames->getLeft(pair.first);
    if (!type) return std::unexpected(std::move(type.error()));
    auto amount = pair.second.toFloat();
    if (!amount) return std::unexpected(std::move(amount.error()));
    result.m_damages[type->get()] = *amount;
  }
  auto recovery = config.getFloat("damageRecovery");
  if (!recovery) return std::unexpected(std::move(recovery.error()));
  result.m_damageRecoveryPerSecond = *recovery;
  auto harvest = config.getUInt("harvestLevel", 1);
  if (!harvest) return std::unexpected(std::move(harvest.error()));
  result.m_requiredHarvestLevel = *harvest;
  auto maximum = config.getFloat("maximumEffectTime", 1.5);
  if (!maximum) return std::unexpected(std::move(maximum.error()));
  result.m_maximumEffectTime = *maximum;
  auto health = config.getFloat("totalHealth", 1.0f);
  if (!health) return std::unexpected(std::move(health.error()));
  result.m_totalHealth = *health;
  if (healthOverride) result.m_totalHealth = *healthOverride;
  if (harvestLevelOverride) result.m_requiredHarvestLevel = *harvestLevelOverride;
  return result;
  }();
  if (!parsed && sourcePath)
    return std::unexpected(assets.withAssetContext(std::move(parsed.error()), *sourcePath, "Loading tile damage parameters"));
  return parsed;
}

std::expected<float, StarException> TileDamageParameters::damageDone(TileDamage const& damage) const {
  return m_damages.value(damage.type) * damage.amount;
}

float TileDamageParameters::recoveryPerSecond() const {
  return m_damageRecoveryPerSecond;
}

unsigned TileDamageParameters::requiredHarvestLevel() const {
  return m_requiredHarvestLevel;
}

float TileDamageParameters::maximumEffectTime() const {
  return m_maximumEffectTime;
}

float TileDamageParameters::totalHealth() const {
  return m_totalHealth;
}

TileDamageParameters TileDamageParameters::sum(TileDamageParameters const& other) const {
  TileDamageParameters result;

  result.m_damageRecoveryPerSecond = m_damageRecoveryPerSecond + other.m_damageRecoveryPerSecond;
  result.m_totalHealth = m_totalHealth + other.m_totalHealth;

  result.m_requiredHarvestLevel = std::max(m_requiredHarvestLevel, other.m_requiredHarvestLevel);
  result.m_maximumEffectTime = std::max(m_maximumEffectTime, other.m_maximumEffectTime);

  for (auto const& key : m_damages.keyView()) {
    if (other.m_damages.contains(key))
      result.m_damages[key] = result.m_totalHealth / ((m_totalHealth / m_damages.value(key, 0)) + (other.m_totalHealth / other.m_damages.value(key, 0)));
    else
      result.m_damages[key] = m_damages.value(key, 0);
  }

  for (auto const& key : m_damages.keyView()) {
    if (m_damages.contains(key))
      result.m_damages[key] = result.m_totalHealth / ((m_totalHealth / m_damages.value(key, 0)) + (other.m_totalHealth / other.m_damages.value(key, 0)));
    else
      result.m_damages[key] = other.m_damages.value(key, 0);
  }

  return result;
}

std::expected<Json, StarException> TileDamageParameters::toJson() const {
  if (!TileDamageTypeNames) return std::unexpected(TileDamageTypeNames.error());
  JsonObject factors;
  for (auto const& pair : m_damages) {
    auto name = TileDamageTypeNames->getRight(pair.first);
    if (!name) return std::unexpected(std::move(name.error()));
    factors.emplace(name->get(), pair.second);
  }
  return JsonObject{{"damageFactors", std::move(factors)},
    {"damageRecovery", m_damageRecoveryPerSecond},
    {"requiredHarvestLevel", m_requiredHarvestLevel},
    {"maximumEffectTime", m_maximumEffectTime}, {"totalHealth", m_totalHealth}};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TileDamageParameters& tileDamage) {
  if (auto r = ds.readMapContainer(tileDamage.m_damages); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileDamage.m_damageRecoveryPerSecond); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileDamage.m_requiredHarvestLevel); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileDamage.m_maximumEffectTime); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileDamage.m_totalHealth); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TileDamageParameters const& tileDamage) {
  if (auto r = ds.writeMapContainer(tileDamage.m_damages); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileDamage.m_damageRecoveryPerSecond); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileDamage.m_requiredHarvestLevel); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileDamage.m_maximumEffectTime); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileDamage.m_totalHealth); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

TileDamageStatus::TileDamageStatus() {
  reset();
}

void TileDamageStatus::reset() {
  m_damagePercentage = 0.0f;
  m_damageEffectTimeFactor = 0.0f;
  m_harvested = false;
  m_damageSourcePosition = Vec2F();
  m_damageType = TileDamageType::Protected;
  m_damageEffectPercentage = 0.0f;
}

std::expected<void, StarException> TileDamageStatus::damage(TileDamageParameters const& damageParameters, Vec2F const& sourcePosition, TileDamage const& damage) {
  auto done = damageParameters.damageDone(damage);
  if (!done) return std::unexpected(std::move(done.error()));
  auto percentageDelta = *done / damageParameters.totalHealth();
  m_damagePercentage = std::min(1.0f, m_damagePercentage + percentageDelta);
  m_harvested = damage.harvestLevel >= damageParameters.requiredHarvestLevel();
  m_damageSourcePosition = sourcePosition;
  m_damageType = damage.type;

  if (percentageDelta > 0)
    m_damageEffectTimeFactor = damageParameters.maximumEffectTime();

  updateDamageEffectPercentage();
  return {};
}

void TileDamageStatus::recover(TileDamageParameters const& damageParameters, float dt) {
  // Once the tile becomes dead, it should not recover from it
  if (healthy() || dead())
    return;

  m_damagePercentage -= damageParameters.recoveryPerSecond() * dt / damageParameters.totalHealth();
  m_damageEffectTimeFactor -= dt;

  if (m_damagePercentage <= 0.0f) {
    m_damagePercentage = 0.0f;
    m_damageEffectTimeFactor = 0.0f;
    m_damageType = TileDamageType::Protected;
  }

  updateDamageEffectPercentage();
}

bool TileDamageStatus::healthy() const {
  return m_damagePercentage <= 0.0f;
}

bool TileDamageStatus::damaged() const {
  return m_damagePercentage > 0.0f;
}

bool TileDamageStatus::damageProtected() const {
  return m_damageType == TileDamageType::Protected;
}

bool TileDamageStatus::dead() const {
  return m_damagePercentage >= 1.0f && m_damageType != TileDamageType::Protected;
}

bool TileDamageStatus::harvested() const {
  return m_harvested;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TileDamageStatus& tileDamageStatus) {
  if (auto r = ds.read(tileDamageStatus.m_damagePercentage); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileDamageStatus.m_damageEffectTimeFactor); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileDamageStatus.m_harvested); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileDamageStatus.m_damageSourcePosition); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(tileDamageStatus.m_damageType); !r) return std::unexpected(std::move(r.error()));

  tileDamageStatus.updateDamageEffectPercentage();

  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TileDamageStatus const& tileDamageStatus) {
  if (auto r = ds.write(tileDamageStatus.m_damagePercentage); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileDamageStatus.m_damageEffectTimeFactor); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileDamageStatus.m_harvested); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileDamageStatus.m_damageSourcePosition); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(tileDamageStatus.m_damageType); !r) return std::unexpected(std::move(r.error()));

  return std::ref(ds);
}

void TileDamageStatus::updateDamageEffectPercentage() {
  m_damageEffectPercentage = std::clamp(m_damageEffectTimeFactor, 0.0f, 1.0f) * m_damagePercentage;
}

EntityTileDamageStatus::EntityTileDamageStatus() {
  m_damagePercentage.setFixedPointBase(0.001f);
  m_damageEffectTimeFactor.setFixedPointBase(0.001f);
  m_damagePercentage.setInterpolator(lerp<float, float>);
  m_damageEffectTimeFactor.setInterpolator(lerp<float, float>);

}

std::expected<std::shared_ptr<EntityTileDamageStatus>, StarException> EntityTileDamageStatus::create() {
  auto status = std::shared_ptr<EntityTileDamageStatus>(new EntityTileDamageStatus());
  if (auto result = status->addNetElement(status->m_damagePercentage); !result) return std::unexpected(std::move(result.error()));
  if (auto result = status->addNetElement(status->m_damageEffectTimeFactor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = status->addNetElement(status->m_damageHarvested); !result) return std::unexpected(std::move(result.error()));
  if (auto result = status->addNetElement(status->m_damageType); !result) return std::unexpected(std::move(result.error()));
  return status;
}

float EntityTileDamageStatus::damagePercentage() const {
  return m_damagePercentage.get();
}

float EntityTileDamageStatus::damageEffectPercentage() const {
  return std::clamp(m_damageEffectTimeFactor.get(), 0.0f, 1.0f) * m_damagePercentage.get();
}

TileDamageType EntityTileDamageStatus::damageType() const {
  return m_damageType.get();
}

void EntityTileDamageStatus::reset() {
  m_damagePercentage.set(0.0f);
  m_damageEffectTimeFactor.set(0.0f);
  m_damageHarvested.set(false);
}

std::expected<void, StarException> EntityTileDamageStatus::damage(TileDamageParameters const& damageParameters, TileDamage const& damage) {
  auto done = damageParameters.damageDone(damage);
  if (!done) return std::unexpected(std::move(done.error()));
  auto percentageDelta = *done / damageParameters.totalHealth();
  m_damagePercentage.set(std::min(1.0f, m_damagePercentage.get() + percentageDelta));
  m_damageHarvested.set(damage.harvestLevel >= damageParameters.requiredHarvestLevel());
  m_damageType.set(damage.type);

  if (percentageDelta > 0)
    m_damageEffectTimeFactor.set(damageParameters.maximumEffectTime());
  return {};
}

void EntityTileDamageStatus::recover(TileDamageParameters const& damageParameters, float dt) {
  // Once the tile becomes dead, it should not recover from it
  if (healthy() || dead())
    return;

  m_damagePercentage.set(m_damagePercentage.get() - damageParameters.recoveryPerSecond() * dt / damageParameters.totalHealth());
  m_damageEffectTimeFactor.set(m_damageEffectTimeFactor.get() - dt);

  if (m_damagePercentage.get() <= 0.0f) {
    m_damagePercentage.set(0.0f);
    m_damageEffectTimeFactor.set(0.0f);
    m_damageType.set(TileDamageType::Protected);
  }
}

bool EntityTileDamageStatus::healthy() const {
  return m_damagePercentage.get() <= 0.0f;
}

bool EntityTileDamageStatus::damaged() const {
  return m_damagePercentage.get() > 0.0f;
}

bool EntityTileDamageStatus::damageProtected() const {
  return m_damageType.get() == TileDamageType::Protected;
}

bool EntityTileDamageStatus::dead() const {
  return m_damagePercentage.get() >= 1.0f && m_damageType.get() != TileDamageType::Protected;
}

bool EntityTileDamageStatus::harvested() const {
  return m_damageHarvested.get();
}

}
