import std;
import star.format;
import star.map;
import star.json;
import star.json_extra;
import star.ordered_map;
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
import star.celestial_types;
import star.data_stream_extra;

import star.byte_array;
namespace Star {

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialPlanet& planet) {
  CelestialPlanet decoded;
  auto bytes = ds.read<ByteArray>();
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  auto parameters = CelestialParameters::create(std::move(*bytes));
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  decoded.planet_parameters = std::move(*parameters);
  auto satellites = ds.read_map_container(
    decoded.satellite_parameters,
    [](DataStream& stream, int& orbit, CelestialParameters& parameters) -> std::expected<void, StarException> {
      auto read_orbit = stream.read(orbit);
      if (!read_orbit)
        return std::unexpected(std::move(read_orbit.error()));
      auto bytes = stream.read<ByteArray>();
      if (!bytes)
        return std::unexpected(std::move(bytes.error()));
      auto decoded = CelestialParameters::create(std::move(*bytes));
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      parameters = std::move(*decoded);
      return {};
    });
  if (!satellites)
    return std::unexpected(std::move(satellites.error()));
  planet = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialPlanet const& planet) {
  auto bytes = planet.planet_parameters.net_store();
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  auto parameters = ds.write(*bytes);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  auto satellites = ds.write_map_container(
    planet.satellite_parameters,
    [](DataStream& stream, int orbit, CelestialParameters const& parameters) -> std::expected<void, StarException> {
      auto written_orbit = stream.write(orbit);
      if (!written_orbit)
        return std::unexpected(std::move(written_orbit.error()));
      auto bytes = parameters.net_store();
      if (!bytes)
        return std::unexpected(std::move(bytes.error()));
      return stream.write(*bytes);
    });
  if (!satellites)
    return std::unexpected(std::move(satellites.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialSystemObjects& system_objects) {
  CelestialSystemObjects decoded;
  auto fields = ds >> decoded.system_location >> decoded.planets;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  system_objects = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialSystemObjects const& system_objects) {
  return ds << system_objects.system_location << system_objects.planets;
}

CelestialChunk::CelestialChunk() = default;

std::expected<CelestialChunk, StarException> CelestialChunk::create(Json const& store) {
  CelestialChunk chunk;
  chunk.persistent = true;
  auto index_json = store.get("chunkIndex");
  if (!index_json)
    return std::unexpected(std::move(index_json.error()));
  auto index = celestial_integer_vector<2>(*index_json);
  if (!index)
    return std::unexpected(std::move(index.error()));
  chunk.chunk_index = *index;
  auto constellations = store.get("constellations").and_then(&Json::array_ptr);
  if (!constellations)
    return std::unexpected(std::move(constellations.error()));
  chunk.constellations.reserve((*constellations)->size());
  for (auto const& lines_json : **constellations) {
    auto lines = lines_json.array_ptr();
    if (!lines)
      return std::unexpected(std::move(lines.error()));
    CelestialConstellation constellation;
    constellation.reserve((*lines)->size());
    for (auto const& line : **lines) {
      auto start_json = line.get(0);
      if (!start_json)
        return std::unexpected(std::move(start_json.error()));
      auto start = celestial_integer_vector<2>(*start_json);
      if (!start)
        return std::unexpected(std::move(start.error()));
      auto end_json = line.get(1);
      if (!end_json)
        return std::unexpected(std::move(end_json.error()));
      auto end = celestial_integer_vector<2>(*end_json);
      if (!end)
        return std::unexpected(std::move(end.error()));
      constellation.push_back({*start, *end});
    }
    chunk.constellations.push_back(std::move(constellation));
  }
  auto systems = store.get("systemParameters").and_then(&Json::array_ptr);
  if (!systems)
    return std::unexpected(std::move(systems.error()));
  for (auto const& system : **systems) {
    auto location_json = system.get(0);
    if (!location_json)
      return std::unexpected(std::move(location_json.error()));
    auto location = celestial_integer_vector<3>(*location_json);
    if (!location)
      return std::unexpected(std::move(location.error()));
    auto parameters_json = system.get(1);
    if (!parameters_json)
      return std::unexpected(std::move(parameters_json.error()));
    auto parameters = CelestialParameters::create(*parameters_json);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    chunk.system_parameters[*location] = std::move(*parameters);
  }
  auto objects = store.get("systemObjects").and_then(&Json::array_ptr);
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  for (auto const& system : **objects) {
    auto location_json = system.get(0);
    if (!location_json)
      return std::unexpected(std::move(location_json.error()));
    auto location = celestial_integer_vector<3>(*location_json);
    if (!location)
      return std::unexpected(std::move(location.error()));
    auto planets = system.get(1).and_then(&Json::array_ptr);
    if (!planets)
      return std::unexpected(std::move(planets.error()));
    HashMap<int, CelestialPlanet> system_objects;
    for (auto const& pair : **planets) {
      auto orbit = pair.get(0).and_then(celestial_integer<>);
      if (!orbit)
        return std::unexpected(std::move(orbit.error()));
      auto planet_json = pair.get(1);
      if (!planet_json)
        return std::unexpected(std::move(planet_json.error()));
      auto parameters_json = planet_json->get("parameters");
      if (!parameters_json)
        return std::unexpected(std::move(parameters_json.error()));
      auto parameters = CelestialParameters::create(*parameters_json);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      CelestialPlanet planet;
      planet.planet_parameters = std::move(*parameters);
      auto satellites = planet_json->get("satellites").and_then(&Json::array_ptr);
      if (!satellites)
        return std::unexpected(std::move(satellites.error()));
      for (auto const& satellite : **satellites) {
        auto satellite_orbit = satellite.get(0).and_then(celestial_integer<>);
        if (!satellite_orbit)
          return std::unexpected(std::move(satellite_orbit.error()));
        auto satellite_json = satellite.get(1);
        if (!satellite_json)
          return std::unexpected(std::move(satellite_json.error()));
        auto satellite_parameters = CelestialParameters::create(*satellite_json);
        if (!satellite_parameters)
          return std::unexpected(std::move(satellite_parameters.error()));
        auto added = planet.satellite_parameters.add(*satellite_orbit, std::move(*satellite_parameters));
        if (!added)
          return std::unexpected(std::move(added.error()));
      }
      auto added = system_objects.add(*orbit, std::move(planet));
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
    chunk.system_objects[*location] = std::move(system_objects);
  }
  return chunk;
}

std::expected<Json, StarException> CelestialChunk::to_json() const {
  JsonArray constellation_store;
  constellation_store.reserve(constellations.size());
  for (auto const& constellation : constellations) {
    JsonArray lines;
    lines.reserve(constellation.size());
    for (auto const& pair : constellation)
      lines.push_back(JsonArray{json_from_vec2i(pair.first), json_from_vec2i(pair.second)});
    constellation_store.push_back(std::move(lines));
  }
  JsonArray system_parameters_store;
  system_parameters_store.reserve(system_parameters.size());
  for (auto const& pair : system_parameters) {
    auto parameters = pair.second.disk_store();
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    system_parameters_store.push_back(JsonArray{json_from_vec3i(pair.first), std::move(*parameters)});
  }
  JsonArray system_objects_store;
  system_objects_store.reserve(system_objects.size());
  for (auto const& system : system_objects) {
    JsonArray planets_store;
    planets_store.reserve(system.second.size());
    for (auto const& planet : system.second) {
      JsonArray satellites_store;
      satellites_store.reserve(planet.second.satellite_parameters.size());
      for (auto const& satellite : planet.second.satellite_parameters) {
        auto parameters = satellite.second.disk_store();
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        satellites_store.push_back(JsonArray{satellite.first, std::move(*parameters)});
      }
      auto parameters = planet.second.planet_parameters.disk_store();
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      planets_store.push_back(JsonArray{
        planet.first, JsonObject{{"parameters", std::move(*parameters)}, {"satellites", std::move(satellites_store)}}});
    }
    system_objects_store.push_back(JsonArray{json_from_vec3i(system.first), std::move(planets_store)});
  }
  return JsonObject{{"chunkIndex", json_from_vec2i(chunk_index)},
      {"constellations", std::move(constellation_store)},
      {"systemParameters", std::move(system_parameters_store)},
      {"systemObjects", std::move(system_objects_store)}};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialChunk& chunk) {
  CelestialChunk decoded;
  decoded.persistent = chunk.persistent;
  auto prefix = ds >> decoded.chunk_index >> decoded.constellations;
  if (!prefix)
    return std::unexpected(std::move(prefix.error()));
  auto systems = ds.read_map_container(
    decoded.system_parameters,
    [](DataStream& stream, Vec3I& location, CelestialParameters& parameters) -> std::expected<void, StarException> {
      auto read_location = stream.read(location);
      if (!read_location)
        return std::unexpected(std::move(read_location.error()));
      auto bytes = stream.read<ByteArray>();
      if (!bytes)
        return std::unexpected(std::move(bytes.error()));
      auto decoded = CelestialParameters::create(std::move(*bytes));
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      parameters = std::move(*decoded);
      return {};
    });
  if (!systems)
    return std::unexpected(std::move(systems.error()));
  auto objects = ds.read(decoded.system_objects);
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  chunk = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialChunk const& chunk) {
  auto prefix = ds << chunk.chunk_index << chunk.constellations;
  if (!prefix)
    return std::unexpected(std::move(prefix.error()));
  auto systems =
    ds.write_map_container(chunk.system_parameters,
                           [](DataStream& stream, Vec3I const& location,
                              CelestialParameters const& parameters) -> std::expected<void, StarException> {
                             auto written_location = stream.write(location);
                             if (!written_location)
                               return std::unexpected(std::move(written_location.error()));
                             auto bytes = parameters.net_store();
                             if (!bytes)
                               return std::unexpected(std::move(bytes.error()));
                             return stream.write(*bytes);
                           });
  if (!systems)
    return std::unexpected(std::move(systems.error()));
  return ds << chunk.system_objects;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialBaseInformation& celestial_information) {
  CelestialBaseInformation decoded{};
  auto fields = ds >> decoded.planet_orbital_levels >> decoded.satellite_orbital_levels
      >> decoded.chunk_size >> decoded.xy_coord_range >> decoded.z_coord_range;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  celestial_information.planet_orbital_levels = decoded.planet_orbital_levels;
  celestial_information.satellite_orbital_levels = decoded.satellite_orbital_levels;
  celestial_information.chunk_size = decoded.chunk_size;
  celestial_information.xy_coord_range = decoded.xy_coord_range;
  celestial_information.z_coord_range = decoded.z_coord_range;
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialBaseInformation const& celestial_information) {
  return ds << celestial_information.planet_orbital_levels << celestial_information.satellite_orbital_levels
      << celestial_information.chunk_size << celestial_information.xy_coord_range << celestial_information.z_coord_range;
}

}
