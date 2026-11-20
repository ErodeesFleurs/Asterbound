import star.format;
import star.json_extra;
import std;
import star.string;
import star.data_stream;
import star.json;
import star.color;
import star.bi_map;
import star.sky_types;
import star.data_stream_extra;

import star.vector;
namespace Star {

extern std::expected<EnumMap<SkyType>, StarException> const SkyTypeNames = EnumMap<SkyType>::create({
    {SkyType::Barren, "barren"},
    {SkyType::Atmospheric, "atmospheric"},
    {SkyType::Atmosphereless, "atmosphereless"},
    {SkyType::Orbital, "orbital"},
    {SkyType::Warp, "warp"},
    {SkyType::Space, "space"}});

extern std::expected<EnumMap<FlyingType>, StarException> const FlyingTypeNames = EnumMap<FlyingType>::create({
    {FlyingType::None, "none"},
    {FlyingType::Disembarking, "disembarking"},
    {FlyingType::Warp, "warp"},
    {FlyingType::Arriving, "arriving"}});

extern std::expected<EnumMap<WarpPhase>, StarException> const WarpPhaseNames = EnumMap<WarpPhase>::create({
    {WarpPhase::SlowingDown, "slowingdown"},
    {WarpPhase::Maintain, "maintain"},
    {WarpPhase::SpeedingUp, "speedingup"}});

SkyColoring::SkyColoring() {
  mainColor = Color::Clear;

  morningColors = {Color::Clear, Color::Clear};
  dayColors = {Color::Clear, Color::Clear};
  eveningColors = {Color::Clear, Color::Clear};
  nightColors = {Color::Clear, Color::Clear};

  morningLightColor = Color::Clear;
  dayLightColor = Color::Clear;
  eveningLightColor = Color::Clear;
  nightLightColor = Color::Clear;
}

std::expected<SkyColoring, StarException> SkyColoring::create(Json const& variant) {
  SkyColoring coloring;
  auto getColor = [&](String const& key) -> std::expected<Color, StarException> {
    auto value = variant.get(key);
    if (!value) return std::unexpected(std::move(value.error()));
    return json_to_color(*value);
  };
  auto getColorPair = [&](String const& key) -> std::expected<std::pair<Color, Color>, StarException> {
    auto pair = variant.get(key);
    if (!pair) return std::unexpected(std::move(pair.error()));
    auto first = pair->get(0);
    if (!first) return std::unexpected(std::move(first.error()));
    auto second = pair->get(1);
    if (!second) return std::unexpected(std::move(second.error()));
    auto firstColor = json_to_color(*first);
    if (!firstColor) return std::unexpected(std::move(firstColor.error()));
    auto secondColor = json_to_color(*second);
    if (!secondColor) return std::unexpected(std::move(secondColor.error()));
    return std::pair{*firstColor, *secondColor};
  };
  auto mainColor = getColor("mainColor");
  if (!mainColor) return std::unexpected(std::move(mainColor.error()));
  coloring.mainColor = *mainColor;
  auto morningColors = getColorPair("morningColors");
  if (!morningColors) return std::unexpected(std::move(morningColors.error()));
  coloring.morningColors = *morningColors;
  auto dayColors = getColorPair("dayColors");
  if (!dayColors) return std::unexpected(std::move(dayColors.error()));
  coloring.dayColors = *dayColors;
  auto eveningColors = getColorPair("eveningColors");
  if (!eveningColors) return std::unexpected(std::move(eveningColors.error()));
  coloring.eveningColors = *eveningColors;
  auto nightColors = getColorPair("nightColors");
  if (!nightColors) return std::unexpected(std::move(nightColors.error()));
  coloring.nightColors = *nightColors;
  auto morningLightColor = getColor("morningLightColor");
  if (!morningLightColor) return std::unexpected(std::move(morningLightColor.error()));
  coloring.morningLightColor = *morningLightColor;
  auto dayLightColor = getColor("dayLightColor");
  if (!dayLightColor) return std::unexpected(std::move(dayLightColor.error()));
  coloring.dayLightColor = *dayLightColor;
  auto eveningLightColor = getColor("eveningLightColor");
  if (!eveningLightColor) return std::unexpected(std::move(eveningLightColor.error()));
  coloring.eveningLightColor = *eveningLightColor;
  auto nightLightColor = getColor("nightLightColor");
  if (!nightLightColor) return std::unexpected(std::move(nightLightColor.error()));
  coloring.nightLightColor = *nightLightColor;
  return coloring;
}

Json SkyColoring::toJson() const {
  auto makeColorPair = [](std::pair<Color, Color> const& p) {
    return JsonArray{json_from_color(p.first), json_from_color(p.second)};
  };

  return JsonObject{{"mainColor", json_from_color(mainColor)},
      {"morningColors", makeColorPair(morningColors)},
      {"dayColors", makeColorPair(dayColors)},
      {"eveningColors", makeColorPair(eveningColors)},
      {"nightColors", makeColorPair(nightColors)},
      {"morningLightColor", json_from_color(morningLightColor)},
      {"dayLightColor", json_from_color(dayLightColor)},
      {"eveningLightColor", json_from_color(eveningLightColor)},
      {"nightLightColor", json_from_color(nightLightColor)}};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, SkyColoring& skyColoring) {
  SkyColoring value;
  auto result = ds >> value.mainColor >> value.morningColors >> value.dayColors >> value.eveningColors
      >> value.nightColors >> value.morningLightColor >> value.dayLightColor >> value.eveningLightColor >> value.nightLightColor;
  if (!result) return std::unexpected(std::move(result.error()));
  skyColoring = std::move(value);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, SkyColoring const& skyColoring) {
  auto result = ds << skyColoring.mainColor << skyColoring.morningColors << skyColoring.dayColors << skyColoring.eveningColors
      << skyColoring.nightColors << skyColoring.morningLightColor << skyColoring.dayLightColor << skyColoring.eveningLightColor << skyColoring.nightLightColor;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

SkyOrbiter::SkyOrbiter() : type(), scale(), angle() {}

SkyOrbiter::SkyOrbiter(SkyOrbiterType type, float scale, float angle, String const& image, Vec2F position)
  : type(type), scale(scale), angle(angle), image(image), position(position) {}

SkyWorldHorizon::SkyWorldHorizon() : scale(), rotation() {}

SkyWorldHorizon::SkyWorldHorizon(Vec2F center, float scale, float rotation)
  : center(center), scale(scale), rotation(rotation) {}

bool SkyWorldHorizon::empty() const {
  return scale <= 0 || layers.empty();
}

}
