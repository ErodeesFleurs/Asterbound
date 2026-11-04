module;

import std;
import star.format;
import star.string;
import star.map;
import star.set;
import star.vector;
import star.game_types;
import star.entity_rendering_types;




import star.world_geometry;
import star.world_camera;
import star.drawable;

namespace Star {
class WorldClient;
}

namespace Star {

class UniverseClient;
class Animation;
class GuiContext;

class QuestIndicatorPainter {
public:
  QuestIndicatorPainter(GuiContext& context, std::shared_ptr<UniverseClient> const& client);

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::shared_ptr<WorldClient> const& world, WorldCamera const& camera);
  [[nodiscard]] std::expected<void, StarException> render(WorldClient const& world, WorldCamera const& camera, PresentationContext presentation);

private:
  struct Indicator {
    [[nodiscard]] std::expected<Drawable, StarException> render(float pixelRatio) const;

    EntityId entityId;
    Vec2F worldPosition;
    String indicatorName;
    std::shared_ptr<Animation> animation;
  };

  std::reference_wrapper<GuiContext> m_guiContext;
  std::shared_ptr<UniverseClient> m_client;
  Map<EntityId, Indicator> m_indicators;
  Set<EntityId> m_foundIndicators;
};

}

export module star.quest_indicator_painter;

export namespace Star {
  using ::Star::UniverseClient;
  using ::Star::QuestIndicatorPainter;
  using ::Star::Animation;
}
