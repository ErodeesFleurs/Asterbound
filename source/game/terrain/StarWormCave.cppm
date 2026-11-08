module;
import std;
import star.format;
import star.list;
import star.json;
import star.string;
import star.json_extra;

import star.ordered_map;
import star.lru_cache;
import star.vector;
import star.random;
import star.math_common;

export module star.terrain_worm_cave;
import star.terrain_database;

import star.static_random;
export namespace Star {

class WormCaveSector {
public:
  [[nodiscard]] static std::expected<WormCaveSector, StarException> create(int sectorSize, Vec2I sector, Json const& config, std::size_t seed, float commonality);

  float get(int x, int y);

private:
  WormCaveSector(int sectorSize, Vec2I sector) : m_sectorSize(sectorSize), m_sector(sector), m_values(sectorSize * sectorSize) {}
  bool inside(int x, int y);
  void set(int x, int y, float value);

  int m_sectorSize;
  Vec2I m_sector;
  List<float> m_values;

  float m_maxValue;
};

class WormCaveSelector : public TerrainSelector {
public:
  static std::string_view const Name;

  [[nodiscard]] static std::expected<std::shared_ptr<WormCaveSelector>, StarException> create(Json const& config, TerrainSelectorParameters const& parameters);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  WormCaveSelector(ConstructionKey, Json const& config, TerrainSelectorParameters const& parameters, int sectorSize) : TerrainSelector(Name, config, parameters), m_sectorSize(sectorSize) {}

  [[nodiscard]] std::expected<float, StarException> get(int x, int y) const override;

private:
  int m_sectorSize;
  mutable HashLruCache<Vec2I, WormCaveSector> m_cache;
};

}

namespace Star {

std::string_view const WormCaveSelector::Name = "wormcave";

std::expected<WormCaveSector, StarException> WormCaveSector::create(int sectorSize, Vec2I sector, Json const& config, std::size_t seed, float commonality) {
  WormCaveSector result(sectorSize, sector);
  struct Worm {
    Vec2F pos;
    float angle;
    float goalAngle;
    float size;
    float length;
    float goalLength;
  };
  List<Worm> worms;

  auto readRange = [&](String const& name) -> std::expected<Vec2F, StarException> {
    auto value = config.get(name);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return jsonToVec2F(*value);
  };
  auto countRange = readRange("numberOfWormsPerSectorRange");
  if (!countRange)
    return std::unexpected(std::move(countRange.error()));
  Vec2F numberOfWormsPerSectorRange = *countRange;
  auto sizeRange = readRange("wormSizeRange");
  if (!sizeRange)
    return std::unexpected(std::move(sizeRange.error()));
  Vec2F wormSizeRange = *sizeRange;
  auto lengthRange = readRange("wormLengthRange");
  if (!lengthRange)
    return std::unexpected(std::move(lengthRange.error()));
  Vec2F wormLengthRange = *lengthRange;
  auto taperDistance = config.getFloat("wormTaperDistance");
  if (!taperDistance)
    return std::unexpected(std::move(taperDistance.error()));
  float wormTaperDistance = *taperDistance;
  auto angleRange = readRange("wormAngleRange");
  if (!angleRange)
    return std::unexpected(std::move(angleRange.error()));
  Vec2F wormAngleRange = *angleRange;
  auto turnChance = config.getFloat("wormTurnChance");
  if (!turnChance)
    return std::unexpected(std::move(turnChance.error()));
  float wormTurnChance = *turnChance;
  auto turnRate = config.getFloat("wormTurnRate");
  if (!turnRate)
    return std::unexpected(std::move(turnRate.error()));
  float wormTurnRate = *turnRate;
  auto speed = config.getFloat("wormSpeed", 1);
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  float wormSpeed = *speed;

  float twoPi = std::numbers::pi * 2;

  result.m_maxValue = wormSizeRange[1] / 2;

  // determine worms for the neighbouring sectors
  auto configuredRadius = config.getInt("sectorRadius");
  if (!configuredRadius)
    return std::unexpected(std::move(configuredRadius.error()));
  int sectorRadius = sectorSize * *configuredRadius;
  for (int x = sector[0] - sectorRadius; x <= sector[0] + sectorRadius; x += sectorSize)
    for (int y = sector[1] - sectorRadius; y <= sector[1] + sectorRadius; y += sectorSize) {
      RandomSource rs(static_random_u64(x, y, seed));
      auto drawnCount = rs.randf(numberOfWormsPerSectorRange[0], numberOfWormsPerSectorRange[1]);
      if (!drawnCount)
        return std::unexpected(std::move(drawnCount.error()));
      float numberOfWorms = *drawnCount * commonality;
      int intNumberOfWorms = static_cast<int>(numberOfWorms);
      if (rs.randf() < (numberOfWorms - intNumberOfWorms))
        intNumberOfWorms++;
      for (int n = 0; n < intNumberOfWorms; ++n) {
        Worm worm;
        auto xOffset = rs.randf(0, sectorSize);
        if (!xOffset)
          return std::unexpected(std::move(xOffset.error()));
        auto yOffset = rs.randf(0, sectorSize);
        if (!yOffset)
          return std::unexpected(std::move(yOffset.error()));
        worm.pos = Vec2F(x, y) + Vec2F(*xOffset, *yOffset);
        auto angle = rs.randf(wormAngleRange[0], wormAngleRange[1]);
        if (!angle)
          return std::unexpected(std::move(angle.error()));
        worm.angle = *angle;
        auto goalAngle = rs.randf(wormAngleRange[0], wormAngleRange[1]);
        if (!goalAngle)
          return std::unexpected(std::move(goalAngle.error()));
        worm.goalAngle = *goalAngle;
        auto size = rs.randf(wormSizeRange[0], wormSizeRange[1]);
        if (!size)
          return std::unexpected(std::move(size.error()));
        worm.size = *size * commonality;
        worm.length = 0;
        auto length = rs.randf(wormLengthRange[0], wormLengthRange[1]);
        if (!length)
          return std::unexpected(std::move(length.error()));
        worm.goalLength = *length * commonality;
        worms.push_back(worm);
      }
    }

  while (true) {
    bool idle = true;

    for (auto& worm : worms) {
      if (worm.length >= worm.goalLength)
        continue;

      idle = false;

      // taper size
      float wormRadius = worm.size / 2;
      float taperFactor = 1.0f;
      if (worm.length < wormTaperDistance)
        taperFactor = std::sin(0.5 * std::numbers::pi * worm.length / wormTaperDistance);
      else if (worm.goalLength - worm.length < wormTaperDistance)
        taperFactor = std::sin(0.5 * std::numbers::pi * (worm.goalLength - worm.length) / wormTaperDistance);
      wormRadius *= taperFactor;

      // carve out worm area
      int size = std::ceil(wormRadius);
      for (float dx = -size; dx <= size; dx += 1)
        for (float dy = -size; dy <= size; dy += 1) {
          float m = std::sqrt((dx * dx) + (dy * dy));
          if (m <= wormRadius) {
            int x = std::floor(dx + worm.pos[0]);
            int y = std::floor(dy + worm.pos[1]);
            if (result.inside(x, y)) {
              float v = result.get(x, y);
              result.set(x, y, std::max(v, wormRadius - m));
            }
          }
        }

      // move the worm, slowing down a bit as we reach the ends to reduce
      // stutter
      float thisSpeed = std::max(0.75f, wormSpeed * taperFactor);
      worm.pos += Vec2F::with_angle(worm.angle) * thisSpeed;
      worm.length += thisSpeed;

      // maybe set new goal angle
      if (static_random_float(worm.pos[0], worm.pos[1], seed, 1) < wormTurnChance * thisSpeed) {
        worm.goalAngle = pfmod(
          static_random_float_range(wormAngleRange[0], wormAngleRange[1], worm.pos[0], worm.pos[1], seed, 2), twoPi);
      }

      if (worm.angle != worm.goalAngle) {
        // turn the worm toward goal angle
        float angleDiff = worm.goalAngle - worm.angle;

        // stop if we're close enough
        if (std::abs(angleDiff) < wormTurnRate * thisSpeed)
          worm.angle = worm.goalAngle;
        else {
          // turn the shortest angular distance
          if (std::abs(angleDiff) > twoPi)
            angleDiff = -angleDiff;
          worm.angle = pfmod(worm.angle + std::copysign(wormTurnRate * thisSpeed, angleDiff), twoPi);
        }
      }
    }
    if (idle)
      break;
  }
  return result;
}

float WormCaveSector::get(int x, int y) {
  auto val = m_values[(x - m_sector[0]) + m_sectorSize * (y - m_sector[1])];
  if (val > 0)
    return val;
  else
    return -m_maxValue;
}

bool WormCaveSector::inside(int x, int y) {
  int x_ = x - m_sector[0];
  if (x_ < 0 || x_ >= m_sectorSize)
    return false;
  int y_ = y - m_sector[1];
  if (y_ < 0 || y_ >= m_sectorSize)
    return false;
  return true;
}

void WormCaveSector::set(int x, int y, float value) {
  m_values[(x - m_sector[0]) + m_sectorSize * (y - m_sector[1])] = value;
}

std::expected<std::shared_ptr<WormCaveSelector>, StarException> WormCaveSelector::create(Json const& config, TerrainSelectorParameters const& parameters) {
  auto sectorSize = config.getUInt("sectorSize", 64);
  if (!sectorSize)
    return std::unexpected(std::move(sectorSize.error()));
  auto result = std::make_shared<WormCaveSelector>(ConstructionKey{}, config, parameters, *sectorSize);
  auto cacheSize = config.getUInt("lruCacheSize", 16);
  if (!cacheSize)
    return std::unexpected(std::move(cacheSize.error()));
  result->m_cache.setMaxSize(*cacheSize);
  return result;
}

std::expected<float, StarException> WormCaveSelector::get(int x, int y) const {
  Vec2I sector = Vec2I(x - pmod(x, m_sectorSize), y - pmod(y, m_sectorSize));
  auto cached = m_cache.get(sector, [this](Vec2I const& sector) {
    return WormCaveSector::create(m_sectorSize, sector, config, parameters.seed, parameters.commonality);
  });
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  return cached->get().get(x, y);
}

}
