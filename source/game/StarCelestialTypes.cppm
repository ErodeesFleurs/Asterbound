module;
import std;
import star.format;
import star.string;
import star.map;
import star.json;

import star.ordered_map;
import star.either;
import star.vector;
import star.celestial_coordinate;
import star.game_types;
import star.color;
import star.bi_map;
import star.sky_types;
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
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;

namespace Star {

using CelestialConstellation = List<std::pair<Vec2I, Vec2I>>;

struct CelestialOrbitRegion {
  String regionName;
  Vec2I orbitRange;
  float bodyProbability;
  WeightedPool<String> planetaryTypes;
  WeightedPool<String> satelliteTypes;
};

struct CelestialPlanet {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialPlanet& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialPlanet const& value);
  CelestialParameters planetParameters;
  HashMap<int, CelestialParameters> satelliteParameters;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialPlanet& planet);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialPlanet const& planet);

struct CelestialSystemObjects {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialSystemObjects& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialSystemObjects const& value);
  Vec3I systemLocation;
  HashMap<int, CelestialPlanet> planets;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialSystemObjects& systemObjects);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialSystemObjects const& systemObjects);

struct CelestialChunk {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialChunk& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialChunk const& value);
  CelestialChunk();
  [[nodiscard]] static std::expected<CelestialChunk, StarException> create(Json const& store);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  Vec2I chunkIndex;
  List<CelestialConstellation> constellations;
  HashMap<Vec3I, CelestialParameters> systemParameters;

  // System objects are kept separate from systemParameters here so that there
  // can be two phases of loading, one for basic system-level parameters for an
  // entire chunk the other for each set of sub objects for each system.
  HashMap<Vec3I, HashMap<int, CelestialPlanet>> systemObjects;
  
  // only persistent chunks are saved to storage
  bool persistent = false;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialChunk& chunk);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialChunk const& chunk);

using CelestialRequest = Either<Vec2I, Vec3I>;
using CelestialResponse = Either<CelestialChunk, CelestialSystemObjects>;

struct CelestialBaseInformation {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialBaseInformation& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialBaseInformation const& value);
  int planetOrbitalLevels;
  int satelliteOrbitalLevels;
  int chunkSize;
  Vec2I xyCoordRange;
  Vec2I zCoordRange;
  bool enforceCoordRange;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialBaseInformation& celestialInformation);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialBaseInformation const& celestialInformation);
}

export module star.celestial_types;

export namespace Star {
  using ::Star::CelestialSystemObjects;
  using ::Star::CelestialChunk;
  using ::Star::CelestialBaseInformation;
  using ::Star::CelestialConstellation;
  using ::Star::CelestialOrbitRegion;
  using ::Star::CelestialPlanet;
  using ::Star::CelestialRequest;
  using ::Star::CelestialResponse;
}
