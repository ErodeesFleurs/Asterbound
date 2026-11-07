import star.json;
import star.json_extra;
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
import star.static_random;
import star.data_stream_devices;
import star.data_stream_extra;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;

import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.animation;
import star.particle;
import star.weather_types;

import star.byte_array;
import star.pythonic;
namespace Star {

CelestialParameters::CelestialParameters() : m_seed(0) {}

std::expected<CelestialParameters, StarException> CelestialParameters::create(
    Root& root, CelestialCoordinate coordinate, std::uint64_t seed, String name, Json parameters) {
  CelestialParameters result;
  result.m_coordinate = std::move(coordinate);
  result.m_seed = seed;
  result.m_name = std::move(name);
  result.m_parameters = std::move(parameters);
  auto typeJson = result.getParameter("worldType");
  if (!typeJson)
    return std::unexpected(std::move(typeJson.error()));
  auto worldType = typeJson->optString();
  if (!worldType)
    return std::unexpected(std::move(worldType.error()));
  if (*worldType) {
    auto terrestrial = (**worldType).equalsIgnoreCase("Terrestrial");
    if (!terrestrial)
      return std::unexpected(std::move(terrestrial.error()));
    auto asteroids = *terrestrial ? std::expected<bool, StarException>(false) : (**worldType).equalsIgnoreCase("Asteroids");
    if (!asteroids)
      return std::unexpected(std::move(asteroids.error()));
    auto dungeon = (*terrestrial || *asteroids) ? std::expected<bool, StarException>(false) : (**worldType).equalsIgnoreCase("FloatingDungeon");
    if (!dungeon)
      return std::unexpected(std::move(dungeon.error()));
    if (*terrestrial) {
      auto sizeJson = result.getParameter("worldSize");
      if (!sizeJson)
        return std::unexpected(std::move(sizeJson.error()));
      auto worldSize = sizeJson->toString();
      if (!worldSize)
        return std::unexpected(std::move(worldSize.error()));
      auto selected = result.randomizeParameterList("terrestrialType");
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      auto type = selected->toString();
      if (!type)
        return std::unexpected(std::move(type.error()));
      auto visitable = generateTerrestrialWorldParameters(root, *type, *worldSize, seed);
      if (!visitable)
        return std::unexpected(std::move(visitable.error()));
      result.m_visitableParameters = std::move(*visitable);
    } else if (*asteroids) {
      auto visitable = generateAsteroidsWorldParameters(root, seed);
      if (!visitable)
        return std::unexpected(std::move(visitable.error()));
      result.m_visitableParameters = std::move(*visitable);
    } else if (*dungeon) {
      auto dungeonJson = result.getParameter("dungeonWorld");
      if (!dungeonJson)
        return std::unexpected(std::move(dungeonJson.error()));
      auto dungeonName = dungeonJson->toString();
      if (!dungeonName)
        return std::unexpected(std::move(dungeonName.error()));
      auto visitable = generateFloatingDungeonWorldParameters(root, *dungeonName);
      if (!visitable)
        return std::unexpected(std::move(visitable.error()));
      result.m_visitableParameters = std::move(*visitable);
    }
  }
  return result;
}

std::expected<CelestialParameters, StarException> CelestialParameters::create(ByteArray netStore) {
  CelestialParameters result;
  DataStreamBuffer ds(std::move(netStore));
  auto fields = ds >> result.m_coordinate >> result.m_seed >> result.m_name >> result.m_parameters;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  auto bytes = ds.read<ByteArray>();
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  auto visitable = netLoadVisitableWorldParameters(*bytes);
  if (!visitable)
    return std::unexpected(std::move(visitable.error()));
  result.m_visitableParameters = std::move(*visitable);
  return result;
}

std::expected<CelestialParameters, StarException> CelestialParameters::create(Json const& variant) {
  CelestialParameters result;
  auto coordinateJson = variant.get("coordinate");
  if (!coordinateJson)
    return std::unexpected(std::move(coordinateJson.error()));
  auto coordinate = CelestialCoordinate::create(*coordinateJson);
  if (!coordinate)
    return std::unexpected(std::move(coordinate.error()));
  auto seed = variant.getUInt("seed");
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  auto name = variant.getString("name");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto parameters = variant.get("parameters");
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  auto visitableJson = variant.get("visitableParameters");
  if (!visitableJson)
    return std::unexpected(std::move(visitableJson.error()));
  auto visitable = diskLoadVisitableWorldParameters(*visitableJson);
  if (!visitable)
    return std::unexpected(std::move(visitable.error()));
  result.m_coordinate = std::move(*coordinate);
  result.m_seed = *seed;
  result.m_name = std::move(*name);
  result.m_parameters = std::move(*parameters);
  result.m_visitableParameters = std::move(*visitable);
  return result;
}

std::expected<Json, StarException> CelestialParameters::diskStore() const {
  auto visitable = diskStoreVisitableWorldParameters(m_visitableParameters);
  if (!visitable)
    return std::unexpected(std::move(visitable.error()));
  return JsonObject{{"coordinate", m_coordinate.toJson()},
      {"seed", m_seed},
      {"name", m_name},
      {"parameters", m_parameters},
      {"visitableParameters", std::move(*visitable)}};
}

std::expected<ByteArray, StarException> CelestialParameters::netStore() const {
  DataStreamBuffer ds;
  auto fields = ds << m_coordinate << m_seed << m_name << m_parameters;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  auto visitable = netStoreVisitableWorldParameters(m_visitableParameters);
  if (!visitable)
    return std::unexpected(std::move(visitable.error()));
  auto written = ds.write(*visitable);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return ds.takeData();
}

CelestialCoordinate CelestialParameters::coordinate() const {
  return m_coordinate;
}

String CelestialParameters::name() const {
  return m_name;
}

std::uint64_t CelestialParameters::seed() const {
  return m_seed;
}

Json CelestialParameters::parameters() const {
  return m_parameters;
}

std::expected<Json, StarException> CelestialParameters::getParameter(String const& name, Json def) const {
  return m_parameters.get(name, std::move(def));
}

std::expected<Json, StarException> CelestialParameters::randomizeParameterList(String const& name, std::int32_t mix) const {
  auto parameter = getParameter(name, Json());
  if (!parameter)
    return std::unexpected(std::move(parameter.error()));
  if (parameter->isNull())
    return Json();
  auto array = parameter->toArray();
  if (!array)
    return std::unexpected(std::move(array.error()));
  auto selected = staticRandomFrom(*array, mix, m_seed, name);
  if (!selected)
    return std::unexpected(std::move(selected.error()));
  return selected->get();
}

std::expected<Json, StarException> CelestialParameters::randomizeParameterRange(String const& name, std::int32_t mix) const {
  auto parameter = getParameter(name, Json());
  if (!parameter)
    return std::unexpected(std::move(parameter.error()));
  if (parameter->isNull())
    return Json();
  auto list = parameter->toArray();
  if (!list)
    return std::unexpected(std::move(list.error()));
  if (list->size() != 2)
    return std::unexpected(CelestialException(
        strf("Parameter '{}' does not appear to be a range in CelestialParameters::randomizeRange", name)));
  return randomizeParameterRange(*list, mix, name);
}

std::expected<Json, StarException> CelestialParameters::randomizeParameterRange(
    JsonArray const& range, std::int32_t mix, std::optional<String> const& name) const {
  if (range[0].type() == Json::Type::Int) {
    auto min = range[0].toInt();
    if (!min)
      return std::unexpected(std::move(min.error()));
    auto max = range[1].toInt();
    if (!max)
      return std::unexpected(std::move(max.error()));
    return Json(staticRandomU64(mix, m_seed, name.value_or("")) % (*max - *min + 1) + *min);
  } else {
    auto min = range[0].toDouble();
    if (!min)
      return std::unexpected(std::move(min.error()));
    auto max = range[1].toDouble();
    if (!max)
      return std::unexpected(std::move(max.error()));
    return Json(staticRandomDouble(mix, m_seed, name.value_or("")) * (*max - *min) + *min);
  }
}

bool CelestialParameters::isVisitable() const { return static_cast<bool>(m_visitableParameters); }

std::shared_ptr<VisitableWorldParameters const> CelestialParameters::visitableParameters() const {
  return m_visitableParameters;
}

void CelestialParameters::setVisitableParameters(std::shared_ptr<VisitableWorldParameters> const& newVisitableParameters) {
  m_visitableParameters = newVisitableParameters;
}

}
