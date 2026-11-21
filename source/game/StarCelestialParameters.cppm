module;
import std;
import star.format;
import star.string;
import star.json;

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

import star.byte_array;
namespace Star {

class Root;

class CelestialParameters {
public:
  CelestialParameters();
  [[nodiscard]] static std::expected<CelestialParameters, StarException> create(Root& root, CelestialCoordinate coordinate, std::uint64_t seed, String name, Json parameters);
  [[nodiscard]] static std::expected<CelestialParameters, StarException> create(Json const& diskStore);
  [[nodiscard]] static std::expected<CelestialParameters, StarException> create(ByteArray net_store);

  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
  [[nodiscard]] std::expected<ByteArray, StarException> net_store() const;

  CelestialCoordinate coordinate() const;
  String name() const;
  std::uint64_t seed() const;

  Json parameters() const;
  [[nodiscard]] std::expected<Json, StarException> getParameter(String const& name, Json def = Json()) const;
  // Predictably select from a json array, given by the named parameter.
  // Selects based on the name hash and the system seed.
  [[nodiscard]] std::expected<Json, StarException> randomizeParameterList(String const& name, std::int32_t mix = 0) const;
  // Predictably select from a range, given by the named parameter.  Works for
  // either floating or integral ranges.
  [[nodiscard]] std::expected<Json, StarException> randomizeParameterRange(String const& name, std::int32_t mix = 0) const;
  // Same function, but if you want to specify the range from an external source
  [[nodiscard]] std::expected<Json, StarException> randomizeParameterRange(JsonArray const& range, std::int32_t mix = 0, std::optional<String> const& name = {}) const;

  // Not all worlds are visitable, if the world is not visitable its
  // visitableParameters will be empty.
  bool isVisitable() const;
  std::shared_ptr<VisitableWorldParameters const> visitableParameters() const;
  void setVisitableParameters(std::shared_ptr<VisitableWorldParameters> const& newVisitableParameters);

private:
  CelestialCoordinate m_coordinate;
  std::uint64_t m_seed;
  String m_name;
  Json m_parameters;
  std::shared_ptr<VisitableWorldParameters const> m_visitableParameters;
};

}

export module star.celestial_parameters;

export namespace Star {
  using ::Star::CelestialParameters;
}
