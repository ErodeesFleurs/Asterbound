module;
import star.format;
import std;
import star.string;
import star.list;
import star.data_stream;
import star.json;

import star.color;
import star.bi_map;
import star.sky_types;
import star.vector;
import star.celestial_coordinate;
import star.sky_parameters;

namespace Star {

struct SkyRenderData {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, SkyRenderData& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, SkyRenderData const& value);
  Json settings;
  SkyParameters skyParameters;

  SkyType type;
  float dayLevel;
  float skyAlpha;

  float dayLength;
  float timeOfDay;
  double epochTime;

  Vec2F starOffset;
  float starRotation;
  Vec2F worldOffset;
  float worldRotation;
  float orbitAngle;

  std::size_t starFrames;
  StringList starList;
  StringList hyperStarList;

  Color environmentLight;
  Color mainSkyColor;
  Color topRectColor;
  Color bottomRectColor;
  Color flashColor;

  StringList const& starTypes() const;

  // Star and orbiter positions here are in view space, from (0, 0) to viewSize

  [[nodiscard]] std::expected<List<SkyOrbiter>, StarException> backOrbiters(Vec2F const& viewSize) const;
  [[nodiscard]] std::expected<SkyWorldHorizon, StarException> worldHorizon(Vec2F const& viewSize) const;
  [[nodiscard]] std::expected<List<SkyOrbiter>, StarException> frontOrbiters(Vec2F const& viewSize) const;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, SkyRenderData& skyRenderData);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, SkyRenderData const& skyRenderData);
}

export module star.sky_render_data;

export namespace Star {
  using ::Star::SkyRenderData;
}
