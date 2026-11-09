module;
import std;
import star.format;
import star.string;
import star.json;

import star.random;
import star.weighted_pool;
import star.color;
import star.bi_map;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.animation;
import star.particle;

import star.vector;
namespace Star {

class Root;

struct WeatherType {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WeatherType& weatherType);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WeatherType const& weatherType);
  struct ParticleConfig {
    Particle particle;
    float density;
    bool autoRotate;
  };

  struct ProjectileConfig {
    String projectile;
    Json parameters;
    Vec2F velocity;
    float ratePerX;
    int spawnAboveRegion;
    int spawnHorizontalPad;
    float windAffectAmount;
  };

  WeatherType();
  [[nodiscard]]
  static std::expected<WeatherType, StarException> create(Root& root, Json config, String path = String());

  [[nodiscard]]
  std::expected<Json, StarException> toJson() const;

  String name;

  List<ParticleConfig> particles;
  List<ProjectileConfig> projectiles;
  StringList statusEffects;

  float maximumWind;
  Vec2F duration;
  StringList weatherNoises;

  // Resolved from local assets and intentionally excluded from legacy binary serialization.
  std::optional<String> parallax;
};

using WeatherPool = WeightedPool<String>;

[[nodiscard]]
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, WeatherType& weatherType);
[[nodiscard]]
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, WeatherType const& weatherType);
}

export module star.weather_types;

export namespace Star {
  using ::Star::WeatherType;
  using ::Star::WeatherPool;
}
