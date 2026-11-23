module;
import std;
import star.format;
import star.string;
import star.json;

import star.celestial_coordinate;
import star.world_parameters;

import star.byte_array;
namespace Star {

class Root;

class CelestialParameters {
public:
  CelestialParameters();
  [[nodiscard]] static std::expected<CelestialParameters, StarException> create(Root& root, CelestialCoordinate coordinate, std::uint64_t seed, String name, Json parameters);
  [[nodiscard]] static std::expected<CelestialParameters, StarException> create(Json const& disk_store);
  [[nodiscard]] static std::expected<CelestialParameters, StarException> create(ByteArray net_store);

  [[nodiscard]] std::expected<Json, StarException> disk_store() const;
  [[nodiscard]] std::expected<ByteArray, StarException> net_store() const;

  CelestialCoordinate coordinate() const;
  String name() const;
  std::uint64_t seed() const;

  Json parameters() const;
  [[nodiscard]] std::expected<Json, StarException> get_parameter(String const& name, Json def = Json()) const;
  // Predictably select from a json array, given by the named parameter.
  // Selects based on the name hash and the system seed.
  [[nodiscard]] std::expected<Json, StarException> randomize_parameter_list(String const& name, std::int32_t mix = 0) const;
  // Predictably select from a range, given by the named parameter.  Works for
  // either floating or integral ranges.
  [[nodiscard]] std::expected<Json, StarException> randomize_parameter_range(String const& name, std::int32_t mix = 0) const;
  // Same function, but if you want to specify the range from an external source
  [[nodiscard]] std::expected<Json, StarException> randomize_parameter_range(JsonArray const& range, std::int32_t mix = 0, std::string_view name = {}) const;

  // Not all worlds are visitable, if the world is not visitable its
  // visitable_parameters will be empty.
  bool is_visitable() const;
  std::shared_ptr<VisitableWorldParameters const> visitable_parameters() const;
  void set_visitable_parameters(std::shared_ptr<VisitableWorldParameters> const& new_visitable_parameters);

private:
  CelestialCoordinate m_coordinate;
  std::uint64_t m_seed;
  String m_name;
  Json m_parameters;
  std::shared_ptr<VisitableWorldParameters const> m_visitable_parameters;
};

}

export module star.celestial_parameters;

export namespace Star {
  using ::Star::CelestialParameters;
}
