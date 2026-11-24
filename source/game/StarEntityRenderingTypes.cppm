module;
import std;
import star.json;
import star.format;

import star.color;
import star.string;
import star.string_view;
import star.bi_map;
import star.vector;
import star.game_types;
import star.liquid_types;
import star.material_types;

namespace Star {

// Captured once for a display frame. World geometry, lights and overlays share
// the same sample and scheduler-discontinuity epoch; this is not simulation time.
class PresentationContext {
public:
  explicit PresentationContext(float interpolation = 1.0f, std::uint64_t epoch = 0)
    : m_interpolation(std::clamp(interpolation, 0.0f, 1.0f)), m_epoch(epoch) {}

  float interpolation() const { return m_interpolation; }
  std::uint64_t epoch() const { return m_epoch; }
  PresentationContext current() const { return PresentationContext(1.0f, m_epoch); }

private:
  float m_interpolation;
  std::uint64_t m_epoch;
};

using EntityRenderLayer = std::uint32_t;

inline constexpr unsigned render_layer_upper_bits = 5;
inline constexpr unsigned render_layer_lower_bits = 32 - render_layer_upper_bits;
inline constexpr EntityRenderLayer render_layer_lower_mask = static_cast<EntityRenderLayer>(-1) >> render_layer_upper_bits;

inline constexpr EntityRenderLayer render_layer_background_overlay = 1u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_background_tile = 2u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_platform = 3u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_plant = 4u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_plant_drop = 5u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_object = 6u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_preview_object = 7u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_back_particle = 8u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_vehicle = 9u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_effect = 10u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_projectile = 11u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_monster = 12u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_npc = 13u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_player = 14u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_item_drop = 15u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_liquid = 16u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_middle_particle = 17u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_foreground_tile = 18u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_foreground_entity = 19u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_foreground_overlay = 20u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_front_particle = 21u << render_layer_lower_bits;
inline constexpr EntityRenderLayer render_layer_overlay = 22u << render_layer_lower_bits;

constexpr EntityRenderLayer vehicle_layer_offset(EntityId entity_id, EntityRenderLayer vehicle_layer) {
  return (static_cast<EntityRenderLayer>(entity_id) * 4u + vehicle_layer) & render_layer_lower_mask;
}

[[nodiscard]] std::expected<EntityRenderLayer, StarException> parse_render_layer(StringView render_layer);

struct PreviewTile {
  PreviewTile() = default;
  PreviewTile(Vec2I const& position, bool foreground, MaterialId mat_id, MaterialHue hue_shift, bool update_mat_id);
  PreviewTile(Vec2I const& position, bool foreground, Vec3B const& light, bool update_light);
  PreviewTile(Vec2I const& position, bool foreground, MaterialId mat_id, MaterialHue hue_shift, bool update_mat_id, Vec3B const& light, bool update_light, MaterialColorVariant color_variant);
  PreviewTile(Vec2I const& position, LiquidId liq_id);

  Vec2I position;
  bool foreground = false;

  LiquidId liq_id = EmptyLiquidId;
  MaterialId mat_id = NullMaterialId;
  MaterialHue hue_shift = 0;
  bool update_mat_id = false;
  MaterialColorVariant color_variant = DefaultMaterialColorVariant;
  Vec3B light;
  bool update_light = false;
};

struct OverheadBar {
  OverheadBar() = default;
  [[nodiscard]] static std::expected<OverheadBar, StarException> create(Json const& json);
  OverheadBar(std::optional<String> icon, float percentage, Color color, bool detail_only);

  Vec2F entity_position;
  std::optional<String> icon;
  float percentage = 0.0f;
  Color color;
  bool detail_only = false;
};

enum class EntityHighlightEffectType {
  None,
  Interactive,
  Inspectable,
  Interesting,
  Inspected
};
inline constexpr auto entity_highlight_effect_type_names =
  sorted_enum_names(std::to_array<std::pair<EntityHighlightEffectType, std::string_view>>(
    {{EntityHighlightEffectType::None, "none"},
     {EntityHighlightEffectType::Interactive, "interactive"},
     {EntityHighlightEffectType::Inspectable, "inspectable"},
     {EntityHighlightEffectType::Interesting, "interesting"},
     {EntityHighlightEffectType::Inspected, "inspected"}}));

struct EntityHighlightEffect {
  EntityHighlightEffectType type = EntityHighlightEffectType::None;
  float level = 0.0f;
};

}

export module star.entity_rendering_types;

export namespace Star {
  using ::Star::PresentationContext;
  using ::Star::EntityRenderLayer;
  using ::Star::parse_render_layer;
  using ::Star::vehicle_layer_offset;
  using ::Star::PreviewTile;
  using ::Star::OverheadBar;
  using ::Star::EntityHighlightEffectType;
  using ::Star::entity_highlight_effect_type_names;
  using ::Star::EntityHighlightEffect;
  using ::Star::render_layer_upper_bits;
  using ::Star::render_layer_lower_bits;
  using ::Star::render_layer_lower_mask;
  using ::Star::render_layer_background_overlay;
  using ::Star::render_layer_background_tile;
  using ::Star::render_layer_platform;
  using ::Star::render_layer_plant;
  using ::Star::render_layer_plant_drop;
  using ::Star::render_layer_object;
  using ::Star::render_layer_preview_object;
  using ::Star::render_layer_back_particle;
  using ::Star::render_layer_vehicle;
  using ::Star::render_layer_effect;
  using ::Star::render_layer_projectile;
  using ::Star::render_layer_monster;
  using ::Star::render_layer_npc;
  using ::Star::render_layer_player;
  using ::Star::render_layer_item_drop;
  using ::Star::render_layer_liquid;
  using ::Star::render_layer_middle_particle;
  using ::Star::render_layer_foreground_tile;
  using ::Star::render_layer_foreground_entity;
  using ::Star::render_layer_foreground_overlay;
  using ::Star::render_layer_front_particle;
  using ::Star::render_layer_overlay;
}
