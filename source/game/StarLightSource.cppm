module;
import star.format;

import star.vector;
import std;
import star.data_stream;
import star.bi_map;

namespace Star {

enum class LightType : std::uint8_t {
  Spread = 0,
  Point = 1,
  PointAsSpread = 2 // Point with spread-like range
};

inline constexpr auto LightTypeNames = sortedEnumNames(std::to_array<std::pair<LightType, std::string_view>>(
  {{LightType::Spread, "Spread"}, {LightType::Point, "Point"}, {LightType::PointAsSpread, "PointAsSpread"}}));

struct LightSource {
  Vec2F position;
  Vec3F color;
  LightType type;
  // pointBeam of 0.0 means light has no beam component, as pointBeam goes up,
  // the dropoff from the beamAngle becomes faster and faster.
  float pointBeam;
  // The angle of the beam component of the light in radians
  float beamAngle;
  // beamAmbience provides a floor to the  dropoff for beamed lights, so that
  // even where the beam is not pointing there will still be some light.  0.0
  // means no ambient floor, 1.0 effectively turns off beaming.
  float beamAmbience;

  void translate(Vec2F const& pos);

  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, LightSource const& lightSource);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, LightSource& lightSource);
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, LightSource const& lightSource);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, LightSource& lightSource);
}

export module star.light_source;

export namespace Star {
  using ::Star::LightType;
  using ::Star::LightTypeNames;
  using ::Star::LightSource;
}
