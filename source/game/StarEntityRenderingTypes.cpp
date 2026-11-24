import star.json_extra;
import std;
import star.json;
import star.color;
import star.string;
import star.string_view;
import star.bi_map;
import star.vector;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.entity_rendering_types;
import star.lexical_cast;
import star.format;

namespace Star {

namespace {
constexpr auto render_layer_names = std::to_array<std::pair<EntityRenderLayer, std::string_view>>({
    {render_layer_background_overlay, "BackgroundOverlay"},
    {render_layer_background_tile, "BackgroundTile"},
    {render_layer_platform, "Platform"},
    {render_layer_plant, "Plant"},
    {render_layer_plant_drop, "PlantDrop"},
    {render_layer_object, "Object"},
    {render_layer_preview_object, "PreviewObject"},
    {render_layer_back_particle, "BackParticle"},
    {render_layer_vehicle, "Vehicle"},
    {render_layer_effect, "Effect"},
    {render_layer_projectile, "Projectile"},
    {render_layer_monster, "Monster"},
    {render_layer_npc, "Npc"},
    {render_layer_player, "Player"},
    {render_layer_item_drop, "ItemDrop"},
    {render_layer_liquid, "Liquid"},
    {render_layer_middle_particle, "MiddleParticle"},
    {render_layer_foreground_tile, "ForegroundTile"},
    {render_layer_foreground_entity, "ForegroundEntity"},
    {render_layer_foreground_overlay, "ForegroundOverlay"},
    {render_layer_front_particle, "FrontParticle"},
    {render_layer_overlay, "Overlay"},
});
}

std::expected<EntityRenderLayer, StarException> parse_render_layer(StringView render_layer) {
  auto name = render_layer.utf8();
  // '+' has priority even when '-' appears earlier in the spelling.
  auto separator = name.find('+');
  bool subtract = separator == std::string_view::npos;
  if (subtract)
    separator = name.find('-');
  EntityRenderLayer offset = 0;
  if (separator != std::string_view::npos) {
    auto amount = lexical_cast<int>(name.substr(separator + 1));
    if (!amount) {
      // Legacy delimiter searches diagnose malformed UTF-8 before a bad
      // number, but only if the searches actually traverse those bytes.
      for (StringView sign : {StringView("+"), StringView("-")}) {
        if (auto found = render_layer.contains(sign); !found)
          return std::unexpected(std::move(found.error()));
      }
      return std::unexpected(std::move(amount.error()));
    }
    offset = static_cast<EntityRenderLayer>(*amount);
    if (subtract)
      offset = 0u - offset;
    name = name.substr(0, separator);
  }
  auto layer = enum_value<render_layer_names>(name);
  if (!layer)
    return std::unexpected(std::move(layer.error()));
  return *layer + offset;
}

PreviewTile::PreviewTile(
    Vec2I const& position, bool foreground, MaterialId mat_id, MaterialHue hue_shift, bool update_mat_id)
  : position(position), foreground(foreground), mat_id(mat_id), hue_shift(hue_shift), update_mat_id(update_mat_id) {}

PreviewTile::PreviewTile(Vec2I const& position, bool foreground, Vec3B const& light, bool update_light)
  : position(position), foreground(foreground), light(light), update_light(update_light) {}

PreviewTile::PreviewTile(Vec2I const& position,
    bool foreground,
    MaterialId mat_id,
    MaterialHue hue_shift,
    bool update_mat_id,
    Vec3B const& light,
    bool update_light,
    MaterialColorVariant color_variant)
  : position(position),
    foreground(foreground),
    mat_id(mat_id),
    hue_shift(hue_shift),
    update_mat_id(update_mat_id),
    color_variant(color_variant),
    light(light),
    update_light(update_light) {}

PreviewTile::PreviewTile(Vec2I const& position, LiquidId liq_id)
  : position(position), foreground(true), liq_id(liq_id) {}

std::expected<OverheadBar, StarException> OverheadBar::create(Json const& json) {
  OverheadBar bar;
  auto position = json.opt("position");
  if (!position)
    return std::unexpected(std::move(position.error()));
  if (*position) {
    auto parsed = json_to_vec2f(**position);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    bar.entity_position = *parsed;
  }
  auto icon = json.opt_string("icon");
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  bar.icon = std::move(*icon);
  auto percentage = json.get_float("percentage");
  if (!percentage)
    return std::unexpected(std::move(percentage.error()));
  bar.percentage = *percentage;
  auto color_config = json.get("color");
  if (!color_config)
    return std::unexpected(std::move(color_config.error()));
  auto color = json_to_color(*color_config);
  if (!color)
    return std::unexpected(std::move(color.error()));
  bar.color = std::move(*color);
  auto detail_only = json.get_bool("detailOnly", false);
  if (!detail_only)
    return std::unexpected(std::move(detail_only.error()));
  bar.detail_only = *detail_only;
  return bar;
}

OverheadBar::OverheadBar(std::optional<String> icon, float percentage, Color color, bool detail_only)
  : icon(std::move(icon)), percentage(percentage), color(std::move(color)), detail_only(detail_only) {}

}
