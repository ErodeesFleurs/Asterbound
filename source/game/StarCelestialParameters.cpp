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
namespace Star {

CelestialParameters::CelestialParameters() : m_seed(0) {}

std::expected<CelestialParameters, StarException> CelestialParameters::create(
    Root& root, CelestialCoordinate coordinate, std::uint64_t seed, String name, Json parameters) {
  CelestialParameters result;
  result.m_coordinate = std::move(coordinate);
  result.m_seed = seed;
  result.m_name = std::move(name);
  result.m_parameters = std::move(parameters);
  auto type_json = result.get_parameter("worldType");
  if (!type_json)
    return std::unexpected(std::move(type_json.error()));
  auto world_type = type_json->opt_string();
  if (!world_type)
    return std::unexpected(std::move(world_type.error()));
  if (*world_type) {
    auto terrestrial = (**world_type).equals_ignore_case("Terrestrial");
    if (!terrestrial)
      return std::unexpected(std::move(terrestrial.error()));
    auto asteroids = *terrestrial ? std::expected<bool, StarException>(false) : (**world_type).equals_ignore_case("Asteroids");
    if (!asteroids)
      return std::unexpected(std::move(asteroids.error()));
    auto dungeon = (*terrestrial || *asteroids) ? std::expected<bool, StarException>(false) : (**world_type).equals_ignore_case("FloatingDungeon");
    if (!dungeon)
      return std::unexpected(std::move(dungeon.error()));
    if (*terrestrial) {
      auto size_json = result.get_parameter("worldSize");
      if (!size_json)
        return std::unexpected(std::move(size_json.error()));
      auto world_size = size_json->to_string();
      if (!world_size)
        return std::unexpected(std::move(world_size.error()));
      auto selected = result.randomize_parameter_list("terrestrialType");
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      auto type = selected->to_string();
      if (!type)
        return std::unexpected(std::move(type.error()));
      auto visitable = generateTerrestrialWorldParameters(root, *type, *world_size, seed);
      if (!visitable)
        return std::unexpected(std::move(visitable.error()));
      result.m_visitable_parameters = std::move(*visitable);
    } else if (*asteroids) {
      auto visitable = generateAsteroidsWorldParameters(root, seed);
      if (!visitable)
        return std::unexpected(std::move(visitable.error()));
      result.m_visitable_parameters = std::move(*visitable);
    } else if (*dungeon) {
      auto dungeon_json = result.get_parameter("dungeonWorld");
      if (!dungeon_json)
        return std::unexpected(std::move(dungeon_json.error()));
      auto dungeon_name = dungeon_json->to_string();
      if (!dungeon_name)
        return std::unexpected(std::move(dungeon_name.error()));
      auto visitable = generateFloatingDungeonWorldParameters(root, *dungeon_name);
      if (!visitable)
        return std::unexpected(std::move(visitable.error()));
      result.m_visitable_parameters = std::move(*visitable);
    }
  }
  return result;
}

std::expected<CelestialParameters, StarException> CelestialParameters::create(ByteArray net_store) {
  CelestialParameters result;
  DataStreamBuffer ds(std::move(net_store));
  auto fields = ds >> result.m_coordinate >> result.m_seed >> result.m_name >> result.m_parameters;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  auto bytes = ds.read<ByteArray>();
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  auto visitable = net_load_visitable_world_parameters(*bytes);
  if (!visitable)
    return std::unexpected(std::move(visitable.error()));
  result.m_visitable_parameters = std::move(*visitable);
  return result;
}

std::expected<CelestialParameters, StarException> CelestialParameters::create(Json const& variant) {
  CelestialParameters result;
  auto coordinate_json = variant.get("coordinate");
  if (!coordinate_json)
    return std::unexpected(std::move(coordinate_json.error()));
  auto coordinate = CelestialCoordinate::create(*coordinate_json);
  if (!coordinate)
    return std::unexpected(std::move(coordinate.error()));
  auto seed = variant.get_uint("seed");
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  auto name = variant.get_string("name");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto parameters = variant.get("parameters");
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  auto visitable_json = variant.get("visitableParameters");
  if (!visitable_json)
    return std::unexpected(std::move(visitable_json.error()));
  auto visitable = diskLoadVisitableWorldParameters(*visitable_json);
  if (!visitable)
    return std::unexpected(std::move(visitable.error()));
  result.m_coordinate = std::move(*coordinate);
  result.m_seed = *seed;
  result.m_name = std::move(*name);
  result.m_parameters = std::move(*parameters);
  result.m_visitable_parameters = std::move(*visitable);
  return result;
}

std::expected<Json, StarException> CelestialParameters::disk_store() const {
  auto visitable = diskStoreVisitableWorldParameters(m_visitable_parameters);
  if (!visitable)
    return std::unexpected(std::move(visitable.error()));
  return JsonObject{{"coordinate", m_coordinate.to_json()},
      {"seed", m_seed},
      {"name", m_name},
      {"parameters", m_parameters},
      {"visitableParameters", std::move(*visitable)}};
}

std::expected<ByteArray, StarException> CelestialParameters::net_store() const {
  DataStreamBuffer ds;
  auto fields = ds << m_coordinate << m_seed << m_name << m_parameters;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  auto visitable = net_store_visitable_world_parameters(m_visitable_parameters);
  if (!visitable)
    return std::unexpected(std::move(visitable.error()));
  auto written = ds.write(*visitable);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return ds.take_data();
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

std::expected<Json, StarException> CelestialParameters::get_parameter(String const& name, Json def) const {
  return m_parameters.get(name, std::move(def));
}

std::expected<Json, StarException> CelestialParameters::randomize_parameter_list(String const& name, std::int32_t mix) const {
  auto parameter = m_parameters.find(name);
  if (!parameter)
    return std::unexpected(std::move(parameter.error()));
  if (!*parameter || (*parameter)->get().is_null())
    return Json();
  auto array = (*parameter)->get().array_ptr();
  if (!array)
    return std::unexpected(std::move(array.error()));
  auto selected = static_random_from(**array, mix, m_seed, name);
  if (!selected)
    return std::unexpected(std::move(selected.error()));
  return selected->get();
}

std::expected<Json, StarException> CelestialParameters::randomize_parameter_range(String const& name, std::int32_t mix) const {
  auto parameter = m_parameters.find(name);
  if (!parameter)
    return std::unexpected(std::move(parameter.error()));
  if (!*parameter || (*parameter)->get().is_null())
    return Json();
  auto list = (*parameter)->get().array_ptr();
  if (!list)
    return std::unexpected(std::move(list.error()));
  return randomize_parameter_range(**list, mix, name.utf8());
}

std::expected<Json, StarException> CelestialParameters::randomize_parameter_range(
    JsonArray const& range, std::int32_t mix, std::string_view name) const {
  if (range.size() != 2)
    return std::unexpected(CelestialException(Star::format("Expected two endpoints for celestial parameter range '{}'", name)));
  if (range[0].type() == Json::Type::Int) {
    auto min = range[0].to_int();
    if (!min)
      return std::unexpected(std::move(min.error()));
    auto max = range[1].to_int();
    if (!max)
      return std::unexpected(std::move(max.error()));
    // A fractional upper endpoint may truncate up to the integer minimum.
    if (*max < *min || (*max == *min && range[1].is_type(Json::Type::Float)
            && *range[1].to_double() < static_cast<double>(*min)))
      return std::unexpected(CelestialException(Star::format("Reversed celestial parameter range '{}'", name)));
    // Keep the historical modulo mapping, including negative minima. Unsigned
    // width wraps to zero only for the complete int64 domain (no reduction).
    auto low = static_cast<std::uint64_t>(*min);
    auto width = static_cast<std::uint64_t>(*max) - low + 1;
    auto draw = static_random_u64(mix, m_seed, name);
    return Json(std::bit_cast<std::int64_t>(low + (width ? draw % width : draw)));
  } else {
    auto min = range[0].to_double();
    if (!min)
      return std::unexpected(std::move(min.error()));
    auto max = range[1].to_double();
    if (!max)
      return std::unexpected(std::move(max.error()));
    // Near int64 limits, even an integer upper bound below the minimum can
    // round to the very same double. Compare the integral values in that tie.
    if (*min == *max && range[1].is_type(Json::Type::Int)
        && (*min >= 0x1p63 || static_cast<std::int64_t>(*min) > *range[1].to_int()))
      return std::unexpected(CelestialException(Star::format("Reversed celestial parameter range '{}'", name)));
    auto draw = static_random_double_range(*min, *max, mix, m_seed, name);
    if (!draw)
      return std::unexpected(std::move(draw.error()));
    return Json(*draw);
  }
}

bool CelestialParameters::is_visitable() const { return static_cast<bool>(m_visitable_parameters); }

std::shared_ptr<VisitableWorldParameters const> CelestialParameters::visitable_parameters() const {
  return m_visitable_parameters;
}

void CelestialParameters::set_visitable_parameters(std::shared_ptr<VisitableWorldParameters> const& new_visitable_parameters) {
  m_visitable_parameters = new_visitable_parameters;
}

}
