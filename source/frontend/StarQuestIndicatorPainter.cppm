module;

import std;
import star.format;
import star.string;
import star.map;
import star.vector;
import star.game_types;
import star.entity_rendering_types;
import star.animation;
import star.world_geometry;
import star.world_camera;
import star.drawable;

namespace Star {
class WorldClient;
}

namespace Star {

class UniverseClient;
class GuiContext;

class QuestIndicatorPainter {
public:
  QuestIndicatorPainter(GuiContext& context, std::shared_ptr<UniverseClient> const& client);

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::shared_ptr<WorldClient> const& world, WorldCamera const& camera);
  [[nodiscard]] std::expected<void, StarException> render(WorldClient const& world, WorldCamera const& camera, PresentationContext presentation);

private:
  struct Indicator {
    Vec2F world_position;
    String indicator_name;
    Animation animation;
    bool seen = true;
  };

  std::reference_wrapper<GuiContext> m_gui_context;
  std::shared_ptr<UniverseClient> m_client;
  Map<EntityId, Indicator> m_indicators;
};

}

export module star.quest_indicator_painter;

export namespace Star {
  using ::Star::UniverseClient;
  using ::Star::QuestIndicatorPainter;
}
