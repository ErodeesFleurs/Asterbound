module;
import std;
import star.format;
import star.string;
import star.map;
import star.json;

import star.either;
import star.vector;
import star.weighted_pool;
import star.list;
import star.data_stream;
import star.celestial_parameters;

namespace Star {

using CelestialConstellation = List<std::pair<Vec2I, Vec2I>>;

struct CelestialOrbitRegion {
  String region_name;
  Vec2I orbit_range;
  float body_probability;
  WeightedPool<String> planetary_types;
  WeightedPool<String> satellite_types;
};

struct CelestialPlanet {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialPlanet& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialPlanet const& value);
  CelestialParameters planet_parameters;
  HashMap<int, CelestialParameters> satellite_parameters;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialPlanet& planet);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialPlanet const& planet);

struct CelestialSystemObjects {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialSystemObjects& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialSystemObjects const& value);
  Vec3I system_location;
  HashMap<int, CelestialPlanet> planets;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialSystemObjects& system_objects);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialSystemObjects const& system_objects);

struct CelestialChunk {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialChunk& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialChunk const& value);
  CelestialChunk();
  [[nodiscard]] static std::expected<CelestialChunk, StarException> create(Json const& store);

  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  Vec2I chunk_index;
  List<CelestialConstellation> constellations;
  HashMap<Vec3I, CelestialParameters> system_parameters;

  // System objects are kept separate from system_parameters here so that there
  // can be two phases of loading, one for basic system-level parameters for an
  // entire chunk the other for each set of sub objects for each system.
  HashMap<Vec3I, HashMap<int, CelestialPlanet>> system_objects;
  
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
  int planet_orbital_levels;
  int satellite_orbital_levels;
  int chunk_size;
  Vec2I xy_coord_range;
  Vec2I z_coord_range;
  bool enforce_coord_range;
};
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialBaseInformation& celestial_information);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialBaseInformation const& celestial_information);
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
