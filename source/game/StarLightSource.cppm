module;
import star.format;

import star.vector;
import std;
import star.data_stream;
import star.bi_map;
import star.json;

namespace Star {

// The byte values are part of the legacy wire format.
enum class LightType : std::uint8_t {
  Spread = 0,
  Point = 1,
  PointAsSpread = 2 // Point with spread-like range
};

inline constexpr auto light_type_names = sorted_enum_names(std::to_array<std::pair<LightType, std::string_view>>(
  {{LightType::Spread, "Spread"}, {LightType::Point, "Point"}, {LightType::PointAsSpread, "PointAsSpread"}}));

[[nodiscard]] std::expected<LightType, StarException> parse_light_type(Json const& config);

struct LightSource {
  Vec2F position{};
  Vec3F color{};
  LightType type = LightType::Spread;
  // Zero disables beaming; larger values make light fall off faster away from beam_angle.
  float point_beam = 0.0f;
  // Angle of the beam in radians.
  float beam_angle = 0.0f;
  // Floor on beam falloff: zero is no floor, one effectively disables beaming.
  float beam_ambience = 0.0f;

  constexpr void translate(Vec2F const& offset) noexcept {
    position += offset;
  }
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& stream, LightSource const& light_source);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& stream, LightSource& light_source);
}

export module star.light_source;

export namespace Star {
  using ::Star::LightType;
  using ::Star::light_type_names;
  using ::Star::parse_light_type;
  using ::Star::LightSource;
  using ::Star::operator<<;
  using ::Star::operator>>;
}
