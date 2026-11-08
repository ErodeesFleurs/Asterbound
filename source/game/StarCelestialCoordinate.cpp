import star.json_extra;
import std;
import star.text_formatter;
import star.string;
import star.format;
import star.json;
import star.vector;
import star.celestial_coordinate;
import star.ordered_map;
import star.either;
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
import star.lexical_cast;
import star.static_random;
import star.data_stream_extra;

namespace Star {

CelestialCoordinate::CelestialCoordinate() : m_planetaryOrbitNumber(0), m_satelliteOrbitNumber(0) {}

CelestialCoordinate::CelestialCoordinate(Vec3I location, int planetaryOrbitNumber, int satelliteOrbitNumber)
  : m_location(std::move(location)),
    m_planetaryOrbitNumber(planetaryOrbitNumber),
    m_satelliteOrbitNumber(satelliteOrbitNumber) {}

std::expected<CelestialCoordinate, StarException> CelestialCoordinate::create(Json const& variant) {
  CelestialCoordinate coordinate;
  if (variant.isType(Json::Type::String)) {
    auto text = variant.toString();
    if (!text)
      return std::unexpected(std::move(text.error()));
    auto nullName = text->equals_ignore_case("null");
    if (!nullName)
      return std::unexpected(std::move(nullName.error()));
    if (!text->empty() && !*nullName) {
      auto parse = [&]() -> std::expected<void, StarException> {
        auto parts = text->split_any(" _:");
        if (!parts)
          return std::unexpected(std::move(parts.error()));
        if (parts->size() < 3)
          return std::unexpected(CelestialException("CelestialCoordinate requires at least three location components"));
        for (std::size_t i = 0; i != 3; ++i) {
          auto value = lexical_cast<int>((*parts)[i]);
          if (!value)
            return std::unexpected(std::move(value.error()));
          coordinate.m_location[i] = *value;
        }
        if (parts->size() > 3) {
          auto value = lexical_cast<int>((*parts)[3]);
          if (!value)
            return std::unexpected(std::move(value.error()));
          coordinate.m_planetaryOrbitNumber = *value;
        }
        if (parts->size() > 4) {
          auto value = lexical_cast<int>((*parts)[4]);
          if (!value)
            return std::unexpected(std::move(value.error()));
          coordinate.m_satelliteOrbitNumber = *value;
        }
        if (coordinate.m_planetaryOrbitNumber <= 0)
          return std::unexpected(CelestialException(Star::format("Planetary body number out of range in '{}'", *text)));
        if (coordinate.m_satelliteOrbitNumber < 0)
          return std::unexpected(CelestialException(Star::format("Satellite body number out of range in '{}'", *text)));
        return {};
      };
      auto parsed = parse();
      if (!parsed)
        return std::unexpected(CelestialException(Star::format("Error parsing CelestialCoordinate from '{}'", *text), parsed.error()));
    }
  } else if (variant.isType(Json::Type::Object)) {
    auto locationJson = variant.get("location");
    if (!locationJson)
      return std::unexpected(std::move(locationJson.error()));
    auto location = jsonToVec3I(*locationJson);
    if (!location)
      return std::unexpected(std::move(location.error()));
    auto planet = variant.getInt("planet", 0);
    if (!planet)
      return std::unexpected(std::move(planet.error()));
    auto satellite = variant.getInt("satellite", 0);
    if (!satellite)
      return std::unexpected(std::move(satellite.error()));
    coordinate.m_location = *location;
    coordinate.m_planetaryOrbitNumber = *planet;
    coordinate.m_satelliteOrbitNumber = *satellite;
  } else if (!variant.isNull()) {
    return std::unexpected(CelestialException(
        Star::format("Improper variant type {} trying to convert to SystemCoordinate", variant.typeName())));
  }
  return coordinate;
}

bool CelestialCoordinate::isNull() const {
  return m_location == Vec3I() && m_planetaryOrbitNumber == 0 && m_satelliteOrbitNumber == 0;
}

bool CelestialCoordinate::isSystem() const {
  return !isNull() && m_planetaryOrbitNumber == 0;
}

bool CelestialCoordinate::isPlanetaryBody() const {
  return !isNull() && m_planetaryOrbitNumber != 0 && m_satelliteOrbitNumber == 0;
}

bool CelestialCoordinate::isSatelliteBody() const {
  return !isNull() && m_planetaryOrbitNumber != 0 && m_satelliteOrbitNumber != 0;
}

Vec3I CelestialCoordinate::location() const {
  return m_location;
}

std::expected<CelestialCoordinate, StarException> CelestialCoordinate::system() const {
  if (isNull())
    return std::unexpected(CelestialException("CelestialCoordinate::system() called on null coordinate"));
  return CelestialCoordinate(m_location, 0, 0);
}

std::expected<CelestialCoordinate, StarException> CelestialCoordinate::planet() const {
  if (isPlanetaryBody())
    return *this;
  if (isSatelliteBody())
    return CelestialCoordinate(m_location, m_planetaryOrbitNumber, 0);
  return std::unexpected(CelestialException("CelestialCoordinate::planet() called on null or system coordinate type"));
}

std::expected<int, StarException> CelestialCoordinate::orbitNumber() const {
  if (isSatelliteBody())
    return m_satelliteOrbitNumber;
  if (isPlanetaryBody())
    return m_planetaryOrbitNumber;
  if (isSystem())
    return 0;
  return std::unexpected(CelestialException("CelestialCoordinate::orbitNumber() called on null coordinate"));
}

std::expected<CelestialCoordinate, StarException> CelestialCoordinate::parent() const {
  if (isSatelliteBody())
    return CelestialCoordinate(m_location, m_planetaryOrbitNumber, 0);
  if (isPlanetaryBody())
    return CelestialCoordinate(m_location, 0, 0);
  return std::unexpected(CelestialException("CelestialCoordinate::parent() called on null or system coordinate"));
}

std::expected<CelestialCoordinate, StarException> CelestialCoordinate::child(int orbitNumber) const {
  if (isSystem())
    return CelestialCoordinate(m_location, orbitNumber, 0);
  if (isPlanetaryBody())
    return CelestialCoordinate(m_location, m_planetaryOrbitNumber, orbitNumber);
  return std::unexpected(CelestialException("CelestialCoordinate::child called on null or satellite coordinate"));
}

Json CelestialCoordinate::toJson() const {
  if (isNull()) {
    return Json();
  } else {
    return JsonObject{{"location", jsonFromVec3I(m_location)},
        {"planet", m_planetaryOrbitNumber},
        {"satellite", m_satelliteOrbitNumber}};
  }
}

String CelestialCoordinate::id() const {
  return Star::to_string(*this);
}

double CelestialCoordinate::distance(CelestialCoordinate const& rhs) const {
  return Vec2D(m_location[0] - rhs.m_location[0], m_location[1] - rhs.m_location[1]).magnitude();
}

String CelestialCoordinate::filename() const {
  auto filename = id().utf8();
  std::replace(filename.begin(), filename.end(), ':', '_');
  return filename;
}

CelestialCoordinate::operator bool() const {
  return !isNull();
}

bool CelestialCoordinate::operator<(CelestialCoordinate const& rhs) const {
  return std::tie(m_location, m_planetaryOrbitNumber, m_satelliteOrbitNumber)
      < std::tie(rhs.m_location, rhs.m_planetaryOrbitNumber, rhs.m_satelliteOrbitNumber);
}

bool CelestialCoordinate::operator==(CelestialCoordinate const& rhs) const {
  return std::tie(m_location, m_planetaryOrbitNumber, m_satelliteOrbitNumber)
      == std::tie(rhs.m_location, rhs.m_planetaryOrbitNumber, rhs.m_satelliteOrbitNumber);
}

bool CelestialCoordinate::operator!=(CelestialCoordinate const& rhs) const {
  return std::tie(m_location, m_planetaryOrbitNumber, m_satelliteOrbitNumber)
      != std::tie(rhs.m_location, rhs.m_planetaryOrbitNumber, rhs.m_satelliteOrbitNumber);
}

void write_text(TextBuffer& buffer, CelestialCoordinate const& coord) {
  if (coord.isNull()) {
    append_text(buffer, "null");
  } else {
    std::format_to(std::back_inserter(buffer), "{}:{}:{}", coord.m_location[0], coord.m_location[1], coord.m_location[2]);

    if (coord.m_planetaryOrbitNumber) {
      std::format_to(std::back_inserter(buffer), ":{}", coord.m_planetaryOrbitNumber);
      if (coord.m_satelliteOrbitNumber)
        std::format_to(std::back_inserter(buffer), ":{}", coord.m_satelliteOrbitNumber);
    }
  }
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialCoordinate& coordinate) {
  CelestialCoordinate decoded;
  auto location = ds.read(decoded.m_location);
  if (!location)
    return std::unexpected(std::move(location.error()));
  auto planet = ds.read(decoded.m_planetaryOrbitNumber);
  if (!planet)
    return std::unexpected(std::move(planet.error()));
  auto satellite = ds.read(decoded.m_satelliteOrbitNumber);
  if (!satellite)
    return std::unexpected(std::move(satellite.error()));
  coordinate = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialCoordinate const& coordinate) {
  auto location = ds.write(coordinate.m_location);
  if (!location)
    return std::unexpected(std::move(location.error()));
  auto planet = ds.write(coordinate.m_planetaryOrbitNumber);
  if (!planet)
    return std::unexpected(std::move(planet.error()));
  auto satellite = ds.write(coordinate.m_satelliteOrbitNumber);
  if (!satellite)
    return std::unexpected(std::move(satellite.error()));
  return std::ref(ds);
}

}
