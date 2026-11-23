module;
import std;
import star.format;
import star.string;

import star.celestial_coordinate;
import star.list;
import star.celestial_parameters;

namespace Star {

class CelestialDatabase;
class Root;

// Functions for generating and drawing worlds from a celestial database.
// Guards against drawing unloaded celestial objects, will return empty if no
// information is returned from the celestial database.
//
// Drawing methods return the stack of images to draw and the scale to draw
// them at.
class CelestialGraphics {
public:
  // Some static versions of drawing functions are given that do not require an
  // active CelestialDatabase to draw.

  [[nodiscard]] static std::expected<List<std::pair<String, float>>, StarException> draw_system_planetary_object(CelestialParameters const& celestial_parameters);
  [[nodiscard]] static std::expected<List<std::pair<String, float>>, StarException> draw_system_central_body(CelestialParameters const& celestial_parameters);

  // Specify the shadowing parameters in order to use the shadowing
  // information from that body instead of the primary one.
  [[nodiscard]] static std::expected<List<std::pair<String, float>>, StarException> draw_world(
      Root& root, CelestialParameters const& celestial_parameters, std::optional<CelestialParameters> const& shadowing_parameters = {});
  [[nodiscard]] static std::expected<List<std::pair<String, String>>, StarException> world_horizon_images(Root& root, CelestialParameters const& celestial_parameters);
  [[nodiscard]] static std::expected<int, StarException> world_radial_position(Root& root, CelestialParameters const& celestial_parameters);

  // Each orbiting body will occupy a unique orbital slot, but to give
  // graphical diversity, will also fit into exactly one radial slot for
  // display purposes.  The range of radial numbers is [0, radial slot count)
  [[nodiscard]] static std::expected<int, StarException> planet_radial_positions(Root& root);
  [[nodiscard]] static std::expected<int, StarException> satellite_radial_positions(Root& root);

  [[nodiscard]] static std::expected<List<std::pair<String, float>>, StarException> draw_system_twinkle(Root& root, CelestialDatabase& celestial_database, CelestialCoordinate const& system, double twinkle_time);

  // Returns the small graphic for the given planetary object appropriate for a
  // system-level view.
  [[nodiscard]] static std::expected<List<std::pair<String, float>>, StarException> draw_system_planetary_object(CelestialDatabase& celestial_database, CelestialCoordinate const& coordinate);
  [[nodiscard]] static std::expected<List<std::pair<String, float>>, StarException> draw_system_central_body(CelestialDatabase& celestial_database, CelestialCoordinate const& coordinate);

  // Returns the graphics appropriate to draw an entire world (planetary object
  // or satellite object) in a map view.  Shadows the satellite the same as
  // its parent planetary object.
  [[nodiscard]] static std::expected<List<std::pair<String, float>>, StarException> draw_world(Root& root, CelestialDatabase& celestial_database, CelestialCoordinate const& coordinate);

  // Draw all of the left and right image pairs for all the layers for the
  // world horizon.
  [[nodiscard]] static std::expected<List<std::pair<String, String>>, StarException> world_horizon_images(Root& root, CelestialDatabase& celestial_database, CelestialCoordinate const& coordinate);

  [[nodiscard]] static std::expected<int, StarException> world_radial_position(Root& root, CelestialDatabase& celestial_database, CelestialCoordinate const& coordinate);
};

}

export module star.celestial_graphics;

export namespace Star {
  using ::Star::CelestialGraphics;
  using ::Star::CelestialDatabase;
}
