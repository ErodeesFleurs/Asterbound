module;
import std;
import star.string;
import star.data_stream;
import star.format;
import star.text_formatter;
import star.json;
import star.list;
import star.math_common;

import star.vector;

namespace Star {

struct CelestialExceptionTag {
  static constexpr std::string_view name() { return "CelestialException"; }
};
using CelestialException = StarError<CelestialExceptionTag, StarException>;

// Unlike general JSON-to-vector conversion, a celestial position or orbit
// must not truncate or wrap: it identifies a persistent universe object.
template <typename T>
concept celestial_integer_target = std::same_as<T, int> || std::same_as<T, unsigned> || std::same_as<T, std::size_t>;

template <celestial_integer_target T = int>
[[nodiscard]] std::expected<T, StarException> celestial_integer(Json const& value) {
  if (value.is_type(Json::Type::Float)) {
    auto number = value.to_double();
    if (!number)
      return std::unexpected(std::move(number.error()));
    // An exclusive power-of-two bound remains exact even for 64-bit destinations.
    constexpr double upper = double(std::numeric_limits<T>::max() / 2 + 1) * 2;
    constexpr double lower = std::is_signed_v<T> ? -upper : 0;
    if (!is_finite(*number) || *number < lower || *number >= upper || std::trunc(*number) != *number)
      return std::unexpected(CelestialException("Celestial value is not a representable integer"));
    return static_cast<T>(*number);
  }
  auto number = value.to_int();
  if (!number)
    return std::unexpected(std::move(number.error()));
  if (!std::in_range<T>(*number))
    return std::unexpected(CelestialException("Celestial integer is out of range"));
  return static_cast<T>(*number);
}

template <std::size_t dimension>
concept celestial_dimension = dimension == 2 || dimension == 3;

template <std::size_t dimension, celestial_integer_target T = int> requires celestial_dimension<dimension>
[[nodiscard]] std::expected<Vector<T, dimension>, StarException> celestial_integer_vector(Json const& value) {
  auto components = value.array_ptr();
  if (!components)
    return std::unexpected(std::move(components.error()));
  if ((*components)->size() != dimension)
    return std::unexpected(CelestialException("Celestial vector has the wrong number of components"));
  Vector<T, dimension> result;
  for (std::size_t i = 0; i < dimension; ++i) {
    auto component = celestial_integer<T>((**components)[i]);
    if (!component)
      return std::unexpected(std::move(component.error()));
    result[i] = *component;
  }
  return result;
}

// Specifies coordinates to either a planetary system, a planetary body, or a
// satellite around such a planetary body.  The terms here are meant to be very
// generic, a "planetary body" could be an asteroid field, or a ship, or
// anything in orbit around the center of mass of a specific planetary system.
// The terms are really simply meant as a hierarchy of orbits.
//
// An orbit number of zero means that level is absent. A satellite requires a
// planet. Only the all-zero coordinate is null; existence is a database query.
class CelestialCoordinate {
public:
  // Creates the null CelestialCoordinate
  CelestialCoordinate();
  // Native callers supply a well-formed hierarchy; decoding validates external input.
  CelestialCoordinate(Vec3I location, int planetary_orbit_number = 0, int satellite_orbit_number = 0);
  // Parses JSON/string coordinates, preserving parsing context in returned errors.
  [[nodiscard]] static std::expected<CelestialCoordinate, StarException> create(Json const& json);

  // Is this coordinate the null coordinate?
  bool is_null() const;

  // Does this coordinate point to an entire planetary system?
  bool is_system() const;
  // Is this world a body whose "designated gravity buddy" is the center of a
  // planetary system?
  bool is_planetary_body() const;
  // Is this world a body which orbits around a planetary body?
  bool is_satellite_body() const;

  Vec3I location() const;
  int planetary_orbit_number() const noexcept { return m_planetary_orbit_number; }
  int satellite_orbit_number() const noexcept { return m_satellite_orbit_number; }

  // Returns just the system coordinate portion of this celestial coordinate.
  [[nodiscard]] std::expected<CelestialCoordinate, StarException> system() const;

  // Returns just the planet portion, or an error for null/system coordinates.
  [[nodiscard]] std::expected<CelestialCoordinate, StarException> planet() const;

  // Returns the orbit number for this body.  Returns 0 for system coordinates.
  [[nodiscard]] std::expected<int, StarException> orbit_number() const;

  // Returns the parent, or an error for null/system coordinates.
  [[nodiscard]] std::expected<CelestialCoordinate, StarException> parent() const;

  // Returns a child, or an error for null/satellite coordinates.
  [[nodiscard]] std::expected<CelestialCoordinate, StarException> child(int orbit_number) const;

  // Stores coordinate in json form that can be used to reconstruct it.
  Json to_json() const;

  // Returns coordinate in a parseable String format.
  String id() const;

  // Planar distance between systems; depth and orbits do not participate.
  double distance(CelestialCoordinate const& rhs) const;

  // Returns a slightly different string format than id(), which is still in an
  // accepted format, but more appropriate for filenames.
  String filename() const;

  // Returns true if not null
  explicit operator bool() const;

  bool operator<(CelestialCoordinate const& rhs) const;
  bool operator==(CelestialCoordinate const& rhs) const;
  bool operator!=(CelestialCoordinate const& rhs) const;

  // Prints in the same format accepted by parser.  Each coordinate is unique.
  friend void write_text(TextBuffer& buffer, CelestialCoordinate const& coord);

  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialCoordinate& coordinate);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialCoordinate const& coordinate);

private:
  Vec3I m_location;
  int m_planetary_orbit_number;
  int m_satellite_orbit_number;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialCoordinate& coordinate);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialCoordinate const& coordinate);
void write_text(TextBuffer& buffer, CelestialCoordinate const& coord);

}

export module star.celestial_coordinate;

export template <> struct std::formatter<Star::CelestialCoordinate> : Star::TextFormatter {};

export namespace Star {
  using ::Star::CelestialExceptionTag;
  using ::Star::CelestialException;
  using ::Star::CelestialCoordinate;
  using ::Star::celestial_dimension;
  using ::Star::celestial_integer_target;
  using ::Star::celestial_integer;
  using ::Star::celestial_integer_vector;
  using ::Star::write_text;
}
