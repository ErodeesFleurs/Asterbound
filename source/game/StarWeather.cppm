module;
import std;
import star.format;
import star.string;
import star.json;
import star.net_compatibility;
import star.byte_array;

import star.random;
import star.weighted_pool;
import star.color;
import star.bi_map;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.animation;
import star.particle;
import star.weather_types;
import star.world_geometry;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_system;

import star.vector;
namespace Star {

class Clock;
class Projectile;
class Root;

// Callback used to determine whether weather effects should be spawned in
// the given tile location.  Other checks that enable / disable weather such as
// whether or not the region is below the underground level are performed
// separately of this, this is just to check the actual tile data.
using WeatherEffectsActiveQuery = std::function<std::expected<bool, StarException>(Vec2I)>;

class ServerWeather {
public:
  [[nodiscard]]
  static std::expected<std::shared_ptr<ServerWeather>, StarException> create(Root& root);

  void setup(WeatherPool weatherPool, float undergroundLevel, WorldGeometry worldGeometry,
      WeatherEffectsActiveQuery weatherEffectsActiveQuery);

  void setReferenceClock(std::shared_ptr<Clock const> referenceClock = {});

  void setClientVisibleRegions(List<RectI> regions);

  [[nodiscard]]
  std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeUpdate(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {});

  [[nodiscard]]
  std::expected<void, StarException> update(double dt);

  // Immediately sets the active weather index. If the index is NPos or out of
  // range, weather is cleared.  If force is true, weather will not automatically
  // change until setWeatherIndex/SetWeather is called again.
  [[nodiscard]]
  std::expected<void, StarException> setWeatherIndex(std::size_t weatherIndex, bool force = false);
  // Immediately sets the active weather type by name. If not found, weather is
  // cleared.  Behavior of |force| is the same as above.
  [[nodiscard]]
  std::expected<void, StarException> setWeather(String const& weatherName, bool force = false);

  StringList weatherList() const;

  // Set or clear forcing without changing the current weather
  void forceWeather(bool force);


  float wind() const;
  float weatherIntensity() const;

  StringList statusEffects() const;

  List<std::shared_ptr<Projectile>> pullNewProjectiles();
  
  [[nodiscard]]
  std::expected<std::optional<String>, StarException> activeWeather() const;

private:
  explicit ServerWeather(Root& root);
  [[nodiscard]]
  std::expected<void, StarException> setNetStates();

  [[nodiscard]]
  std::expected<void, StarException> spawnWeatherProjectiles(float dt);

  std::reference_wrapper<Root> m_root;
  WeatherPool m_weatherPool;
  float m_undergroundLevel;
  WorldGeometry m_worldGeometry;
  WeatherEffectsActiveQuery m_weatherEffectsActiveQuery;

  List<RectI> m_clientVisibleRegions;

  std::size_t m_currentWeatherIndex;
  std::optional<WeatherType> m_currentWeatherType;
  float m_currentWeatherIntensity;
  float m_currentWind;

  bool m_forceWeather;

  std::shared_ptr<Clock const> m_referenceClock;
  std::optional<double> m_clockTrackingTime;

  double m_currentTime;
  double m_lastWeatherChangeTime;
  double m_nextWeatherChangeTime;

  List<std::shared_ptr<Projectile>> m_newProjectiles;

  NetElementTopGroup m_netGroup;
  NetElementBytes m_weatherPoolNetState;
  NetElementFloat m_undergroundLevelNetState;
  NetElementSize m_currentWeatherIndexNetState;
  NetElementFloat m_currentWeatherIntensityNetState;
  NetElementFloat m_currentWindNetState;
};

class ClientWeather {
public:
  [[nodiscard]]
  static std::expected<std::shared_ptr<ClientWeather>, StarException> create(Root& root);

  void setup(WorldGeometry worldGeometry, WeatherEffectsActiveQuery weatherEffectsActiveQuery);

  [[nodiscard]]
  std::expected<void, StarException> readUpdate(ByteArray data, NetCompatibilityRules rules);

  [[nodiscard]]
  std::expected<void, StarException> clear();

  void setVisibleRegion(RectI visibleRegion);

  [[nodiscard]]
  std::expected<void, StarException> update(double dt);

  float wind() const;
  float weatherIntensity() const;

  StringList statusEffects() const;

  List<Particle> pullNewParticles();
  StringList weatherTrackOptions() const;
  std::optional<String> weatherParallax() const;
  
  [[nodiscard]]
  std::expected<std::optional<String>, StarException> activeWeather() const;

private:
  explicit ClientWeather(Root& root);
  [[nodiscard]]
  std::expected<void, StarException> getNetStates();

  [[nodiscard]]
  std::expected<void, StarException> spawnWeatherParticles(RectF newClientRegion, float dt);

  std::reference_wrapper<Root> m_root;
  WeatherPool m_weatherPool;
  float m_undergroundLevel;
  WorldGeometry m_worldGeometry;
  WeatherEffectsActiveQuery m_weatherEffectsActiveQuery;

  std::size_t m_currentWeatherIndex;
  std::optional<WeatherType> m_currentWeatherType;
  float m_currentWeatherIntensity;
  float m_currentWind;

  double m_currentTime;
  RectI m_visibleRegion;

  List<Particle> m_particles;
  RectF m_lastParticleVisibleRegion;

  NetElementTopGroup m_netGroup;
  NetElementBytes m_weatherPoolNetState;
  NetElementFloat m_undergroundLevelNetState;
  NetElementSize m_currentWeatherIndexNetState;
  NetElementFloat m_currentWeatherIntensityNetState;
  NetElementFloat m_currentWindNetState;
};

}

export module star.weather;

export namespace Star {
using ::Star::Clock;
using ::Star::Projectile;
using ::Star::ServerWeather;
using ::Star::ClientWeather;
using ::Star::WeatherEffectsActiveQuery;
}
