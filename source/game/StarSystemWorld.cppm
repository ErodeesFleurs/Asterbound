module;
import star.format;
import std;
import star.string;
import star.map;
import star.json;
import star.net_compatibility;
import star.byte_array;
import star.vector;
import star.game_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.strong_typedef;
import star.either;

import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.uuid;
import star.warping;
import star.sky_parameters;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_system;

namespace Star {

class CelestialDatabase;
class Celestial;
class Clock;
class ClientContext;
class SystemWorldServer;
class SystemObject;

struct SystemObjectConfig;

struct CelestialOrbit {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialOrbit& orbit);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialOrbit const& orbit);
  [[nodiscard]] static std::expected<CelestialOrbit, StarException> fromJson(Json const& json);
  Json toJson() const;

  CelestialCoordinate target;
  int direction;
  double enterTime;
  Vec2F enterPosition;

  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const;
  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds);

  bool operator==(CelestialOrbit const& rhs) const;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialOrbit& orbit);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialOrbit const& orbit);

// in transit, at a planet, orbiting a planet,, at a system object, or at a vector position
using SystemLocation = std::variant<std::monostate, CelestialCoordinate, CelestialOrbit, Uuid, Vec2F>;
Json jsonFromSystemLocation(SystemLocation const& location);
[[nodiscard]] std::expected<SystemLocation, StarException> jsonToSystemLocation(Json const& json);

struct SystemWorldConfig {
  [[nodiscard]] static std::expected<SystemWorldConfig, StarException> fromJson(Json const& config);

  float starGravitationalConstant;
  float planetGravitationalConstant;

  Map<unsigned, float> planetSizes;
  float emptyOrbitSize;
  float unvisitablePlanetSize;
  StringMap<float> floatingDungeonWorldSizes;

  float starSize;
  Vec2F planetaryOrbitPadding;
  Vec2F satelliteOrbitPadding;

  Vec2F arrivalRange;

  float objectSpawnPadding;
  float clientObjectSpawnPadding;
  Vec2F objectSpawnInterval;
  double objectSpawnCycle;
  float minObjectOrbitTime;

  float asteroidBeamDistance;

  SkyParameters emptySkyParameters;
};

class Root;
class SystemWorld {
  Root& m_root;
public:
  Root& root() const { return m_root; }
public:
  SystemWorld(Root& root, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase);

  virtual ~SystemWorld() = default;

  SystemWorldConfig const& systemConfig() const;
  double time() const;
  Vec3I location() const;
  [[nodiscard]] std::expected<List<CelestialCoordinate>, StarException> planets() const;

  [[nodiscard]] std::expected<std::uint64_t, StarException> coordinateSeed(CelestialCoordinate const& coord, String const& seedMix) const;
  [[nodiscard]] std::expected<float, StarException> planetOrbitDistance(CelestialCoordinate const& coord) const;
  // assumes circular orbit
  float orbitInterval(float distance, bool isMoon) const;
  [[nodiscard]] std::expected<Vec2F, StarException> orbitPosition(CelestialOrbit const& orbit) const;
  [[nodiscard]] std::expected<float, StarException> clusterSize(CelestialCoordinate const& planet) const;
  [[nodiscard]] std::expected<float, StarException> planetSize(CelestialCoordinate const& planet) const;
  [[nodiscard]] std::expected<Vec2F, StarException> planetPosition(CelestialCoordinate const& planet) const;
  [[nodiscard]] std::expected<std::optional<Vec2F>, StarException> systemLocationPosition(SystemLocation const& position) const;
  Vec2F randomArrivalPosition() const;
  [[nodiscard]] std::expected<std::optional<WarpAction>, StarException> objectWarpAction(Uuid const& uuid) const;

  virtual List<std::shared_ptr<SystemObject>> objects() const = 0;
  virtual List<Uuid> objectKeys() const = 0;
  virtual std::shared_ptr<SystemObject> getObject(Uuid const& uuid) const = 0;

  [[nodiscard]] std::expected<SystemObjectConfig, StarException> systemObjectConfig(String const& name, Uuid const& uuid) const;
  [[nodiscard]] static std::expected<Json, StarException> systemObjectTypeConfig(Root& root, String const& typeName);

protected:
  [[nodiscard]] std::expected<void, StarException> initialize();
  Vec3I m_location;
  std::shared_ptr<CelestialDatabase> m_celestialDatabase;

private:
  std::shared_ptr<Clock const> m_universeClock;
  SystemWorldConfig m_config;
};

struct SystemObjectConfig {
  String name;

  bool moving;
  float speed;
  float orbitDistance;
  float lifeTime;

  // permanent system objects may only have a solar orbit and can never be removed
  bool permanent;

  WarpAction warpAction;
  std::optional<float> threatLevel;
  SkyParameters skyParameters;
  StringMap<String> generatedParameters;
  JsonObject parameters;
};

class SystemObject {
  Root& m_root;
  Root& root() const { return m_root; }
public:
  [[nodiscard]] static std::expected<std::shared_ptr<SystemObject>, StarException> create(Root& root, SystemObjectConfig config, Uuid uuid, Vec2F const& position, JsonObject parameters = {});
  [[nodiscard]] static std::expected<std::shared_ptr<SystemObject>, StarException> create(Root& root, SystemObjectConfig config, Uuid uuid, Vec2F const& position, double spawnTime, JsonObject parameters = {});
  [[nodiscard]] static std::expected<std::shared_ptr<SystemObject>, StarException> create(Root& root, SystemWorld& system, Json const& diskStore);

  Uuid uuid() const;
  String name() const;
  bool permanent() const;
  Vec2F position() const;

  WarpAction warpAction() const;
  std::optional<float> threatLevel() const;
  SkyParameters skyParameters() const;
  [[nodiscard]] std::expected<JsonObject, StarException> parameters() const;

  bool shouldDestroy() const;

  void enterOrbit(CelestialCoordinate const& target, Vec2F const& targetPosition, double time);
  std::optional<CelestialCoordinate> orbitTarget() const;
  std::optional<CelestialOrbit> orbit() const;

  [[nodiscard]] std::expected<void, StarException> clientUpdate(float dt);
  [[nodiscard]] std::expected<void, StarException> serverUpdate(SystemWorldServer& system, float dt);

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules = {});
  [[nodiscard]] std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules = {});

  [[nodiscard]] std::expected<ByteArray, StarException> netStore() const;
  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
private:
  SystemObject(Root& root, SystemObjectConfig config, Uuid uuid, Vec2F const& position, double spawnTime, JsonObject parameters);
  [[nodiscard]] std::expected<void, StarException> init();

  void setPosition(Vec2F const& position);

  SystemObjectConfig m_config;
  Uuid m_uuid;
  double m_spawnTime;
  JsonObject m_parameters;

  std::optional<CelestialCoordinate> m_approach;

  bool m_shouldDestroy;

  NetElementTopGroup m_netGroup;
  NetElementFloat m_xPosition;
  NetElementFloat m_yPosition;
  NetElementData<std::optional<CelestialOrbit>> m_orbit;
};

class SystemClientShip {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<SystemClientShip>, StarException> create(SystemWorld& world, Uuid uuid, float speed, SystemLocation const& position);
  [[nodiscard]] static std::expected<std::shared_ptr<SystemClientShip>, StarException> create(SystemWorld& world, Uuid uuid, SystemLocation const& position);

  Uuid uuid() const;
  Vec2F position() const;
  SystemLocation systemLocation() const;
  SystemLocation destination() const;
  void setDestination(SystemLocation const& destination);
  void setSpeed(float speed);

  bool flying() const;

  // update is only called on master
  [[nodiscard]] std::expected<void, StarException> clientUpdate(float dt);
  [[nodiscard]] std::expected<void, StarException> serverUpdate(SystemWorld& system, float dt);

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules = {});
  [[nodiscard]] std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules = {});

  [[nodiscard]] std::expected<ByteArray, StarException> netStore() const;
private:
  explicit SystemClientShip(Uuid uuid);
  [[nodiscard]] std::expected<void, StarException> initialize(SystemWorld& system, float speed, SystemLocation const& position);
  struct ClientShipConfig {
    float orbitDistance;
    float departTime;
    float spaceDepartTime;
  };

  void setPosition(Vec2F const& position);

  Uuid m_uuid;

  ClientShipConfig m_config;
  float m_departTimer;
  float m_speed;

  std::optional<CelestialOrbit> m_orbit;

  NetElementTopGroup m_netGroup;
  NetElementData<SystemLocation> m_systemLocation;
  NetElementData<SystemLocation> m_destination;
  NetElementFloat m_xPosition;
  NetElementFloat m_yPosition;
};

}

export module star.system_world;

export namespace Star {
  using ::Star::CelestialDatabase;
  using ::Star::Celestial;
  using ::Star::Clock;
  using ::Star::ClientContext;
  using ::Star::SystemWorld;
  using ::Star::SystemWorldServer;
  using ::Star::SystemClientShip;
  using ::Star::SystemObject;
  using ::Star::SystemObjectConfig;
  using ::Star::CelestialOrbit;
  using ::Star::SystemLocation;
  using ::Star::jsonFromSystemLocation;
  using ::Star::jsonToSystemLocation;
  using ::Star::SystemWorldConfig;
}
