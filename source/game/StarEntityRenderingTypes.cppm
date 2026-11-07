module;
import std;
import star.json;
import star.format;

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

inline constexpr unsigned RenderLayerUpperBits = 5;
inline constexpr unsigned RenderLayerLowerBits = 32 - RenderLayerUpperBits;
inline constexpr EntityRenderLayer RenderLayerLowerMask = static_cast<EntityRenderLayer>(-1) >> RenderLayerUpperBits;

inline constexpr EntityRenderLayer RenderLayerBackgroundOverlay = 1 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerBackgroundTile = 2 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerPlatform = 3 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerPlant = 4 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerPlantDrop = 5 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerObject = 6 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerPreviewObject = 7 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerBackParticle = 8 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerVehicle = 9 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerEffect = 10 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerProjectile = 11 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerMonster = 12 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerNpc = 13 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerPlayer = 14 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerItemDrop = 15 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerLiquid = 16 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerMiddleParticle = 17 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerForegroundTile = 18 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerForegroundEntity = 19 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerForegroundOverlay = 20 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerFrontParticle = 21 << RenderLayerLowerBits;
inline constexpr EntityRenderLayer RenderLayerOverlay = 22 << RenderLayerLowerBits;

[[nodiscard]] std::expected<EntityRenderLayer, StarException> parseRenderLayer(String renderLayer);

struct PreviewTile {
  PreviewTile();
  PreviewTile(Vec2I const& position, bool foreground, MaterialId matId, MaterialHue hueShift, bool updateMatId);
  PreviewTile(Vec2I const& position, bool foreground, Vec3B const& light, bool updateLight);
  PreviewTile(Vec2I const& position, bool foreground, MaterialId matId, MaterialHue hueShift, bool updateMatId, Vec3B const& light, bool updateLight, MaterialColorVariant colorVariant);
  PreviewTile(Vec2I const& position, LiquidId liqId);

  Vec2I position;
  bool foreground;

  LiquidId liqId;
  MaterialId matId;
  MaterialHue hueShift;
  bool updateMatId;
  MaterialColorVariant colorVariant;
  Vec3B light;
  bool updateLight;
};

struct OverheadBar {
  OverheadBar();
  [[nodiscard]] static std::expected<OverheadBar, StarException> create(Json const& json);
  OverheadBar(std::optional<String> icon, float percentage, Color color, bool detailOnly);

  Vec2F entityPosition;
  std::optional<String> icon;
  float percentage;
  Color color;
  bool detailOnly;
};

enum class EntityHighlightEffectType {
  None,
  Interactive,
  Inspectable,
  Interesting,
  Inspected
};
inline constexpr auto EntityHighlightEffectTypeNames =
  sortedEnumNames(std::to_array<std::pair<EntityHighlightEffectType, std::string_view>>(
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
  using ::Star::parseRenderLayer;
  using ::Star::PreviewTile;
  using ::Star::OverheadBar;
  using ::Star::EntityHighlightEffectType;
  using ::Star::EntityHighlightEffectTypeNames;
  using ::Star::EntityHighlightEffect;
  using ::Star::RenderLayerUpperBits;
  using ::Star::RenderLayerLowerBits;
  using ::Star::RenderLayerLowerMask;
  using ::Star::RenderLayerBackgroundOverlay;
  using ::Star::RenderLayerBackgroundTile;
  using ::Star::RenderLayerPlatform;
  using ::Star::RenderLayerPlant;
  using ::Star::RenderLayerPlantDrop;
  using ::Star::RenderLayerObject;
  using ::Star::RenderLayerPreviewObject;
  using ::Star::RenderLayerBackParticle;
  using ::Star::RenderLayerVehicle;
  using ::Star::RenderLayerEffect;
  using ::Star::RenderLayerProjectile;
  using ::Star::RenderLayerMonster;
  using ::Star::RenderLayerNpc;
  using ::Star::RenderLayerPlayer;
  using ::Star::RenderLayerItemDrop;
  using ::Star::RenderLayerLiquid;
  using ::Star::RenderLayerMiddleParticle;
  using ::Star::RenderLayerForegroundTile;
  using ::Star::RenderLayerForegroundEntity;
  using ::Star::RenderLayerForegroundOverlay;
  using ::Star::RenderLayerFrontParticle;
  using ::Star::RenderLayerOverlay;
}
