import star.json_extra;
import std;
import star.text_formatter;
import star.string;
import star.format;
import star.json;
import star.vector;
import star.celestial_coordinate;
import star.ordered_map;
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
import star.static_random;
import star.data_stream_extra;

namespace Star {

CelestialCoordinate::CelestialCoordinate() : m_planetary_orbit_number(0), m_satellite_orbit_number(0) {}

CelestialCoordinate::CelestialCoordinate(Vec3I location, int planetary_orbit_number, int satellite_orbit_number)
  : m_location(std::move(location)),
    m_planetary_orbit_number(planetary_orbit_number),
    m_satellite_orbit_number(satellite_orbit_number) {}

namespace {

bool valid_orbits(int planet, int satellite) {
  return planet >= 0 && satellite >= 0 && (planet != 0 || satellite == 0);
}

std::expected<CelestialCoordinate, StarException> parse_coordinate(String const& text) {
  auto null_name = text.equals_ignore_case("null");
  if (!null_name)
    return std::unexpected(std::move(null_name.error()));
  if (text.empty() || *null_name)
    return CelestialCoordinate();

  // split_any historically accepts runs of any of these three separators.
  constexpr std::string_view separators = " _:";
  std::string_view remaining(text.utf8());
  std::array<int, 5> components{};
  std::size_t count = 0;
  for (;;) {
    auto start = remaining.find_first_not_of(separators);
    if (start == std::string_view::npos)
      break;
    remaining.remove_prefix(start);
    auto token = remaining.substr(0, remaining.find_first_of(separators));
    if (count == components.size())
      return std::unexpected(CelestialException("Too many celestial coordinate components"));
    auto parsed = std::from_chars(token.data(), token.data() + token.size(), components[count++]);
    if (parsed.ec != std::errc() || parsed.ptr != token.data() + token.size())
      return std::unexpected(CelestialException(Star::format("Invalid integer coordinate component '{}'", token)));
    remaining.remove_prefix(token.size());
  }
  if (count < 3 || !valid_orbits(components[3], components[4]))
    return std::unexpected(CelestialException("Invalid celestial coordinate hierarchy"));
  return CelestialCoordinate({components[0], components[1], components[2]}, components[3], components[4]);
}

}

std::expected<CelestialCoordinate, StarException> CelestialCoordinate::create(Json const& value) {
  if (value.is_null())
    return CelestialCoordinate();
  if (value.is_type(Json::Type::String)) {
    auto text = value.string_ptr();
    if (!text)
      return std::unexpected(std::move(text.error()));
    auto parsed = parse_coordinate(**text);
    if (!parsed)
      return std::unexpected(CelestialException(Star::format("Error parsing CelestialCoordinate from '{}'", **text), parsed.error()));
    return parsed;
  }
  if (!value.is_type(Json::Type::Object))
    return std::unexpected(CelestialException(Star::format("Improper celestial coordinate type {}", value.type_name())));

  auto position = value.get("location").and_then(celestial_integer_vector<3>);
  if (!position)
    return std::unexpected(std::move(position.error()));
  auto orbit = [&](String const& key) -> std::expected<int, StarException> {
    auto field = value.find(key);
    if (!field)
      return std::unexpected(std::move(field.error()));
    if (!*field || (*field)->get().is_null())
      return 0;
    return celestial_integer((*field)->get());
  };
  auto planet = orbit("planet");
  if (!planet)
    return std::unexpected(std::move(planet.error()));
  auto satellite = orbit("satellite");
  if (!satellite)
    return std::unexpected(std::move(satellite.error()));
  if (!valid_orbits(*planet, *satellite))
    return std::unexpected(CelestialException("Invalid celestial coordinate hierarchy"));
  return CelestialCoordinate(*position, *planet, *satellite);
}

bool CelestialCoordinate::is_null() const {
  return m_location == Vec3I() && m_planetary_orbit_number == 0 && m_satellite_orbit_number == 0;
}

bool CelestialCoordinate::is_system() const {
  return m_planetary_orbit_number == 0 && m_satellite_orbit_number == 0 && m_location != Vec3I();
}

bool CelestialCoordinate::is_planetary_body() const {
  return m_planetary_orbit_number > 0 && m_satellite_orbit_number == 0;
}

bool CelestialCoordinate::is_satellite_body() const {
  return m_planetary_orbit_number > 0 && m_satellite_orbit_number > 0;
}

Vec3I CelestialCoordinate::location() const {
  return m_location;
}

std::expected<CelestialCoordinate, StarException> CelestialCoordinate::system() const {
  if (is_null())
    return std::unexpected(CelestialException("CelestialCoordinate::system() called on null coordinate"));
  return CelestialCoordinate(m_location, 0, 0);
}

std::expected<CelestialCoordinate, StarException> CelestialCoordinate::planet() const {
  if (is_planetary_body())
    return *this;
  if (is_satellite_body())
    return CelestialCoordinate(m_location, m_planetary_orbit_number, 0);
  return std::unexpected(CelestialException("CelestialCoordinate::planet() called on null or system coordinate type"));
}

std::expected<int, StarException> CelestialCoordinate::orbit_number() const {
  if (is_satellite_body())
    return m_satellite_orbit_number;
  if (is_planetary_body())
    return m_planetary_orbit_number;
  if (is_system())
    return 0;
  return std::unexpected(CelestialException("CelestialCoordinate::orbit_number() called on null coordinate"));
}

std::expected<CelestialCoordinate, StarException> CelestialCoordinate::parent() const {
  if (is_satellite_body())
    return CelestialCoordinate(m_location, m_planetary_orbit_number, 0);
  if (is_planetary_body())
    return CelestialCoordinate(m_location, 0, 0);
  return std::unexpected(CelestialException("CelestialCoordinate::parent() called on null or system coordinate"));
}

std::expected<CelestialCoordinate, StarException> CelestialCoordinate::child(int orbit_number) const {
  if (orbit_number <= 0)
    return std::unexpected(CelestialException("Child orbit number must be positive"));
  if (is_system())
    return CelestialCoordinate(m_location, orbit_number, 0);
  if (is_planetary_body())
    return CelestialCoordinate(m_location, m_planetary_orbit_number, orbit_number);
  return std::unexpected(CelestialException("CelestialCoordinate::child called on null or satellite coordinate"));
}

Json CelestialCoordinate::to_json() const {
  if (is_null()) {
    return Json();
  } else {
    return JsonObject{{"location", json_from_vec3i(m_location)},
        {"planet", m_planetary_orbit_number},
        {"satellite", m_satellite_orbit_number}};
  }
}

String CelestialCoordinate::id() const {
  return Star::to_string(*this);
}

double CelestialCoordinate::distance(CelestialCoordinate const& rhs) const {
  return Vec2D(double(m_location[0]) - rhs.m_location[0], double(m_location[1]) - rhs.m_location[1]).magnitude();
}

String CelestialCoordinate::filename() const {
  auto filename = id().utf8();
  std::replace(filename.begin(), filename.end(), ':', '_');
  return filename;
}

CelestialCoordinate::operator bool() const {
  return !is_null();
}

bool CelestialCoordinate::operator<(CelestialCoordinate const& rhs) const {
  return std::tie(m_location, m_planetary_orbit_number, m_satellite_orbit_number)
      < std::tie(rhs.m_location, rhs.m_planetary_orbit_number, rhs.m_satellite_orbit_number);
}

bool CelestialCoordinate::operator==(CelestialCoordinate const& rhs) const {
  return std::tie(m_location, m_planetary_orbit_number, m_satellite_orbit_number)
      == std::tie(rhs.m_location, rhs.m_planetary_orbit_number, rhs.m_satellite_orbit_number);
}

bool CelestialCoordinate::operator!=(CelestialCoordinate const& rhs) const {
  return std::tie(m_location, m_planetary_orbit_number, m_satellite_orbit_number)
      != std::tie(rhs.m_location, rhs.m_planetary_orbit_number, rhs.m_satellite_orbit_number);
}

void write_text(TextBuffer& buffer, CelestialCoordinate const& coord) {
  if (coord.is_null()) {
    append_text(buffer, "null");
  } else {
    std::format_to(std::back_inserter(buffer), "{}:{}:{}", coord.m_location[0], coord.m_location[1], coord.m_location[2]);

    if (coord.m_planetary_orbit_number) {
      std::format_to(std::back_inserter(buffer), ":{}", coord.m_planetary_orbit_number);
      if (coord.m_satellite_orbit_number)
        std::format_to(std::back_inserter(buffer), ":{}", coord.m_satellite_orbit_number);
    }
  }
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialCoordinate& coordinate) {
  CelestialCoordinate decoded;
  auto location = ds.read(decoded.m_location);
  if (!location)
    return std::unexpected(std::move(location.error()));
  auto planet = ds.read(decoded.m_planetary_orbit_number);
  if (!planet)
    return std::unexpected(std::move(planet.error()));
  auto satellite = ds.read(decoded.m_satellite_orbit_number);
  if (!satellite)
    return std::unexpected(std::move(satellite.error()));
  if (!valid_orbits(decoded.m_planetary_orbit_number, decoded.m_satellite_orbit_number))
    return std::unexpected(CelestialException("Invalid celestial coordinate hierarchy in stream"));
  coordinate = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialCoordinate const& coordinate) {
  auto location = ds.write(coordinate.m_location);
  if (!location)
    return std::unexpected(std::move(location.error()));
  auto planet = ds.write(coordinate.m_planetary_orbit_number);
  if (!planet)
    return std::unexpected(std::move(planet.error()));
  auto satellite = ds.write(coordinate.m_satellite_orbit_number);
  if (!satellite)
    return std::unexpected(std::move(satellite.error()));
  return std::ref(ds);
}

}
