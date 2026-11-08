module;
import std;
import star.string;
import star.data_stream;
import star.format;
import star.text_formatter;
import star.json;

import star.vector;

namespace Star {

struct CelestialExceptionTag {
  static constexpr std::string_view name() { return "CelestialException"; }
};
using CelestialException = StarError<CelestialExceptionTag, StarException>;

// Specifies coordinates to either a planetary system, a planetary body, or a
// satellite around such a planetary body.  The terms here are meant to be very
// generic, a "planetary body" could be an asteroid field, or a ship, or
// anything in orbit around the center of mass of a specific planetary system.
// The terms are really simply meant as a hierarchy of orbits.
//
// No validity checking is done here, any coordinate to any body whether it
// exists in a specific universe or not can be expressed.  isNull() simply
// checks whether the coordinate is the result of the empty constructor, not
// whether the coordinate points to a valid object or not.
class CelestialCoordinate {
public:
  // Creates the null CelestialCoordinate
  CelestialCoordinate();
  CelestialCoordinate(Vec3I location, int planetaryOrbitNumber = 0, int satelliteOrbitNumber = 0);
  // Parses JSON/string coordinates, preserving parsing context in returned errors.
  [[nodiscard]] static std::expected<CelestialCoordinate, StarException> create(Json const& json);

  // Is this coordanate the null coordinate?
  bool isNull() const;

  // Does this coordinate point to an entire planetary system?
  bool isSystem() const;
  // Is this world a body whose "designated gravity buddy" is the center of a
  // planetary system?
  bool isPlanetaryBody() const;
  // Is this world a body which orbits around a planetary body?
  bool isSatelliteBody() const;

  Vec3I location() const;

  // Returns just the system coordinate portion of this celestial coordinate.
  [[nodiscard]] std::expected<CelestialCoordinate, StarException> system() const;

  // Returns just the planet portion, or an error for null/system coordinates.
  [[nodiscard]] std::expected<CelestialCoordinate, StarException> planet() const;

  // Returns the orbit number for this body.  Returns 0 for system coordinates.
  [[nodiscard]] std::expected<int, StarException> orbitNumber() const;

  // Returns the parent, or an error for null/system coordinates.
  [[nodiscard]] std::expected<CelestialCoordinate, StarException> parent() const;

  // Returns a child, or an error for null/satellite coordinates.
  [[nodiscard]] std::expected<CelestialCoordinate, StarException> child(int orbitNumber) const;

  // Stores coordinate in json form that can be used to reconstruct it.
  Json toJson() const;

  // Returns coordinate in a parseable String format.
  String id() const;

  // Returns a fakey fake distance
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
  int m_planetaryOrbitNumber;
  int m_satelliteOrbitNumber;
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
  using ::Star::write_text;
}
