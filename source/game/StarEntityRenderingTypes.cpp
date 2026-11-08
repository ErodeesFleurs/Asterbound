import star.json_extra;
import std;
import star.json;
import star.color;
import star.string;
import star.data_stream;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.drawable;
import star.bi_map;
import star.vector;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.entity_rendering_types;
import star.lexical_cast;
import star.format;

namespace Star {

std::expected<EntityRenderLayer, StarException> parseRenderLayer(String renderLayer) {
  static auto const RenderLayerMap = []() -> std::expected<CaseInsensitiveStringMap<EntityRenderLayer>, StarException> {
    std::initializer_list<std::pair<String, EntityRenderLayer>> layers{
    {"BackgroundOverlay", RenderLayerBackgroundOverlay},
    {"BackgroundTile", RenderLayerBackgroundTile},
    {"Platform", RenderLayerPlatform},
    {"Plant", RenderLayerPlant},
    {"PlantDrop", RenderLayerPlantDrop},
    {"Object", RenderLayerObject},
    {"PreviewObject", RenderLayerPreviewObject},
    {"BackParticle", RenderLayerBackParticle},
    {"Vehicle", RenderLayerVehicle},
    {"Effect", RenderLayerEffect},
    {"Projectile", RenderLayerProjectile},
    {"Monster", RenderLayerMonster},
    {"Npc", RenderLayerNpc},
    {"Player", RenderLayerPlayer},
    {"ItemDrop", RenderLayerItemDrop},
    {"Liquid", RenderLayerLiquid},
    {"MiddleParticle", RenderLayerMiddleParticle},
    {"ForegroundTile", RenderLayerForegroundTile},
    {"ForegroundEntity", RenderLayerForegroundEntity},
    {"ForegroundOverlay", RenderLayerForegroundOverlay},
    {"FrontParticle", RenderLayerFrontParticle},
    {"Overlay", RenderLayerOverlay},
    };
    CaseInsensitiveStringMap<EntityRenderLayer> result;
    for (auto const& [name, layer] : layers) {
      auto key = CaseInsensitiveString::create(name);
      if (!key) return std::unexpected(std::move(key.error()));
      result.set(std::move(*key), layer);
    }
    return result;
  }();

  int offset = 0;
  auto plus = renderLayer.contains("+");
  if (!plus)
    return std::unexpected(std::move(plus.error()));
  auto minus = renderLayer.contains("-");
  if (!minus)
    return std::unexpected(std::move(minus.error()));
  if (*plus || *minus) {
    auto parts = renderLayer.split(*plus ? "+" : "-", 1);
    if (!parts)
      return std::unexpected(std::move(parts.error()));
    auto name = parts->at(0);
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto amount = parts->at(1);
    if (!amount)
      return std::unexpected(std::move(amount.error()));
    auto parsed = lexical_cast<int>(amount->get());
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    renderLayer = name->get();
    offset = *plus ? *parsed : -*parsed;
  }

  if (!RenderLayerMap)
    return std::unexpected(RenderLayerMap.error());
  auto key = CaseInsensitiveStringView::create(renderLayer);
  if (!key)
    return std::unexpected(std::move(key.error()));
  auto layer = RenderLayerMap->get(*key);
  if (!layer)
    return std::unexpected(std::move(layer.error()));
  return layer->get() + offset;
}

PreviewTile::PreviewTile()
  : foreground(false),
    liqId(EmptyLiquidId),
    matId(NullMaterialId),
    hueShift(0),
    updateMatId(false),
    colorVariant(DefaultMaterialColorVariant),
    updateLight(false) {}

PreviewTile::PreviewTile(
    Vec2I const& position, bool foreground, MaterialId matId, MaterialHue hueShift, bool updateMatId)
  : position(position),
    foreground(foreground),
    liqId(EmptyLiquidId),
    matId(matId),
    hueShift(hueShift),
    updateMatId(updateMatId),
    colorVariant(DefaultMaterialColorVariant),
    updateLight(false) {}

PreviewTile::PreviewTile(Vec2I const& position, bool foreground, Vec3B const& light, bool updateLight)
  : position(position),
    foreground(foreground),
    liqId(EmptyLiquidId),
    matId(NullMaterialId),
    hueShift(0),
    updateMatId(false),
    colorVariant(DefaultMaterialColorVariant),
    light(light),
    updateLight(updateLight) {}

PreviewTile::PreviewTile(Vec2I const& position,
    bool foreground,
    MaterialId matId,
    MaterialHue hueShift,
    bool updateMatId,
    Vec3B const& light,
    bool updateLight,
    MaterialColorVariant colorVariant)
  : position(position),
    foreground(foreground),
    liqId(EmptyLiquidId),
    matId(matId),
    hueShift(hueShift),
    updateMatId(updateMatId),
    colorVariant(colorVariant),
    light(light),
    updateLight(updateLight) {}

PreviewTile::PreviewTile(Vec2I const& position, LiquidId liqId)
  : position(position),
    foreground(true),
    liqId(liqId),
    matId(NullMaterialId),
    hueShift(0),
    updateMatId(false),
    colorVariant(DefaultMaterialColorVariant),
    updateLight(false) {}

OverheadBar::OverheadBar() : percentage(0.0f), detailOnly(false) {}

std::expected<OverheadBar, StarException> OverheadBar::create(Json const& json) {
  OverheadBar bar;
  auto position = json.opt("position");
  if (!position)
    return std::unexpected(std::move(position.error()));
  if (*position) {
    auto parsed = jsonToVec2F(**position);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    bar.entityPosition = *parsed;
  }
  auto icon = json.optString("icon");
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  bar.icon = std::move(*icon);
  auto percentage = json.getFloat("percentage");
  if (!percentage)
    return std::unexpected(std::move(percentage.error()));
  bar.percentage = *percentage;
  auto colorConfig = json.get("color");
  if (!colorConfig)
    return std::unexpected(std::move(colorConfig.error()));
  auto color = jsonToColor(*colorConfig);
  if (!color)
    return std::unexpected(std::move(color.error()));
  bar.color = std::move(*color);
  auto detailOnly = json.getBool("detailOnly", false);
  if (!detailOnly)
    return std::unexpected(std::move(detailOnly.error()));
  bar.detailOnly = *detailOnly;
  return bar;
}

OverheadBar::OverheadBar(std::optional<String> icon, float percentage, Color color, bool detailOnly)
  : icon(std::move(icon)), percentage(percentage), color(std::move(color)), detailOnly(detailOnly) {}

}
