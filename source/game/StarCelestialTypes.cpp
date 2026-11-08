import std;
import star.format;
import star.map;
import star.json;
import star.json_extra;
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
  decoded.planetParameters = std::move(*parameters);
  auto satellites = ds.readMapContainer(decoded.satelliteParameters,
      [](DataStream& stream, int& orbit, CelestialParameters& parameters) -> std::expected<void, StarException> {
        auto readOrbit = stream.read(orbit);
        if (!readOrbit)
          return std::unexpected(std::move(readOrbit.error()));
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
  auto bytes = planet.planetParameters.netStore();
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  auto parameters = ds.write(*bytes);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  auto satellites = ds.writeMapContainer(planet.satelliteParameters,
      [](DataStream& stream, int orbit, CelestialParameters const& parameters) -> std::expected<void, StarException> {
        auto writtenOrbit = stream.write(orbit);
        if (!writtenOrbit)
          return std::unexpected(std::move(writtenOrbit.error()));
        auto bytes = parameters.netStore();
        if (!bytes)
          return std::unexpected(std::move(bytes.error()));
        return stream.write(*bytes);
      });
  if (!satellites)
    return std::unexpected(std::move(satellites.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialSystemObjects& systemObjects) {
  CelestialSystemObjects decoded;
  auto fields = ds >> decoded.systemLocation >> decoded.planets;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  systemObjects = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialSystemObjects const& systemObjects) {
  return ds << systemObjects.systemLocation << systemObjects.planets;
}

CelestialChunk::CelestialChunk() = default;

std::expected<CelestialChunk, StarException> CelestialChunk::create(Json const& store) {
  CelestialChunk chunk;
  chunk.persistent = true;
  auto indexJson = store.get("chunkIndex");
  if (!indexJson)
    return std::unexpected(std::move(indexJson.error()));
  auto index = jsonToVec2I(*indexJson);
  if (!index)
    return std::unexpected(std::move(index.error()));
  chunk.chunkIndex = *index;
  auto constellations = store.getArray("constellations");
  if (!constellations)
    return std::unexpected(std::move(constellations.error()));
  for (auto const& linesJson : *constellations) {
    auto lines = linesJson.toArray();
    if (!lines)
      return std::unexpected(std::move(lines.error()));
    CelestialConstellation constellation;
    for (auto const& line : *lines) {
      auto startJson = line.get(0);
      if (!startJson)
        return std::unexpected(std::move(startJson.error()));
      auto start = jsonToVec2I(*startJson);
      if (!start)
        return std::unexpected(std::move(start.error()));
      auto endJson = line.get(1);
      if (!endJson)
        return std::unexpected(std::move(endJson.error()));
      auto end = jsonToVec2I(*endJson);
      if (!end)
        return std::unexpected(std::move(end.error()));
      constellation.push_back({*start, *end});
    }
    chunk.constellations.push_back(std::move(constellation));
  }
  auto systems = store.getArray("systemParameters");
  if (!systems)
    return std::unexpected(std::move(systems.error()));
  for (auto const& system : *systems) {
    auto locationJson = system.get(0);
    if (!locationJson)
      return std::unexpected(std::move(locationJson.error()));
    auto location = jsonToVec3I(*locationJson);
    if (!location)
      return std::unexpected(std::move(location.error()));
    auto parametersJson = system.get(1);
    if (!parametersJson)
      return std::unexpected(std::move(parametersJson.error()));
    auto parameters = CelestialParameters::create(*parametersJson);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    chunk.systemParameters[*location] = std::move(*parameters);
  }
  auto objects = store.getArray("systemObjects");
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  for (auto const& system : *objects) {
    auto locationJson = system.get(0);
    if (!locationJson)
      return std::unexpected(std::move(locationJson.error()));
    auto location = jsonToVec3I(*locationJson);
    if (!location)
      return std::unexpected(std::move(location.error()));
    auto planets = system.getArray(1);
    if (!planets)
      return std::unexpected(std::move(planets.error()));
    HashMap<int, CelestialPlanet> systemObjects;
    for (auto const& pair : *planets) {
      auto orbit = pair.getInt(0);
      if (!orbit)
        return std::unexpected(std::move(orbit.error()));
      auto planetJson = pair.get(1);
      if (!planetJson)
        return std::unexpected(std::move(planetJson.error()));
      auto parametersJson = planetJson->get("parameters");
      if (!parametersJson)
        return std::unexpected(std::move(parametersJson.error()));
      auto parameters = CelestialParameters::create(*parametersJson);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      CelestialPlanet planet;
      planet.planetParameters = std::move(*parameters);
      auto satellites = planetJson->getArray("satellites");
      if (!satellites)
        return std::unexpected(std::move(satellites.error()));
      for (auto const& satellite : *satellites) {
        auto satelliteOrbit = satellite.getInt(0);
        if (!satelliteOrbit)
          return std::unexpected(std::move(satelliteOrbit.error()));
        auto satelliteJson = satellite.get(1);
        if (!satelliteJson)
          return std::unexpected(std::move(satelliteJson.error()));
        auto satelliteParameters = CelestialParameters::create(*satelliteJson);
        if (!satelliteParameters)
          return std::unexpected(std::move(satelliteParameters.error()));
        auto added = planet.satelliteParameters.add(*satelliteOrbit, std::move(*satelliteParameters));
        if (!added)
          return std::unexpected(std::move(added.error()));
      }
      auto added = systemObjects.add(*orbit, std::move(planet));
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
    chunk.systemObjects[*location] = std::move(systemObjects);
  }
  return chunk;
}

std::expected<Json, StarException> CelestialChunk::toJson() const {
  JsonArray constellationStore;
  for (auto const& constellation : constellations) {
    JsonArray lines;
    for (auto const& pair : constellation)
      lines.push_back(JsonArray{jsonFromVec2I(pair.first), jsonFromVec2I(pair.second)});
    constellationStore.push_back(std::move(lines));
  }
  JsonArray systemParametersStore;
  for (auto const& pair : systemParameters) {
    auto parameters = pair.second.diskStore();
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    systemParametersStore.push_back(JsonArray{jsonFromVec3I(pair.first), std::move(*parameters)});
  }
  JsonArray systemObjectsStore;
  for (auto const& system : systemObjects) {
    JsonArray planetsStore;
    for (auto const& planet : system.second) {
      JsonArray satellitesStore;
      for (auto const& satellite : planet.second.satelliteParameters) {
        auto parameters = satellite.second.diskStore();
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        satellitesStore.push_back(JsonArray{satellite.first, std::move(*parameters)});
      }
      auto parameters = planet.second.planetParameters.diskStore();
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      planetsStore.push_back(JsonArray{
        planet.first, JsonObject{{"parameters", std::move(*parameters)}, {"satellites", std::move(satellitesStore)}}});
    }
    systemObjectsStore.push_back(JsonArray{jsonFromVec3I(system.first), std::move(planetsStore)});
  }
  return JsonObject{{"chunkIndex", jsonFromVec2I(chunkIndex)},
      {"constellations", std::move(constellationStore)},
      {"systemParameters", std::move(systemParametersStore)},
      {"systemObjects", std::move(systemObjectsStore)}};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialChunk& chunk) {
  CelestialChunk decoded;
  decoded.persistent = chunk.persistent;
  auto prefix = ds >> decoded.chunkIndex >> decoded.constellations;
  if (!prefix)
    return std::unexpected(std::move(prefix.error()));
  auto systems = ds.readMapContainer(decoded.systemParameters,
      [](DataStream& stream, Vec3I& location, CelestialParameters& parameters) -> std::expected<void, StarException> {
        auto readLocation = stream.read(location);
        if (!readLocation)
          return std::unexpected(std::move(readLocation.error()));
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
  auto objects = ds.read(decoded.systemObjects);
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  chunk = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialChunk const& chunk) {
  auto prefix = ds << chunk.chunkIndex << chunk.constellations;
  if (!prefix)
    return std::unexpected(std::move(prefix.error()));
  auto systems = ds.writeMapContainer(chunk.systemParameters,
      [](DataStream& stream, Vec3I const& location, CelestialParameters const& parameters) -> std::expected<void, StarException> {
        auto writtenLocation = stream.write(location);
        if (!writtenLocation)
          return std::unexpected(std::move(writtenLocation.error()));
        auto bytes = parameters.netStore();
        if (!bytes)
          return std::unexpected(std::move(bytes.error()));
        return stream.write(*bytes);
      });
  if (!systems)
    return std::unexpected(std::move(systems.error()));
  return ds << chunk.systemObjects;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialBaseInformation& celestialInformation) {
  CelestialBaseInformation decoded{};
  auto fields = ds >> decoded.planetOrbitalLevels >> decoded.satelliteOrbitalLevels
      >> decoded.chunkSize >> decoded.xyCoordRange >> decoded.zCoordRange;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  celestialInformation.planetOrbitalLevels = decoded.planetOrbitalLevels;
  celestialInformation.satelliteOrbitalLevels = decoded.satelliteOrbitalLevels;
  celestialInformation.chunkSize = decoded.chunkSize;
  celestialInformation.xyCoordRange = decoded.xyCoordRange;
  celestialInformation.zCoordRange = decoded.zCoordRange;
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialBaseInformation const& celestialInformation) {
  return ds << celestialInformation.planetOrbitalLevels << celestialInformation.satelliteOrbitalLevels
      << celestialInformation.chunkSize << celestialInformation.xyCoordRange << celestialInformation.zCoordRange;
}

}
