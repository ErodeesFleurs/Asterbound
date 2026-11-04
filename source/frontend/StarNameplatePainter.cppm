module;
import std;
import star.format;
import star.json;

import star.game_types;


import star.rect;
import star.list;
import star.set;


import star.color;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.string;
import star.vector;
import star.text;



import star.world_geometry;
import star.chat_bubble_separation;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.world_camera;

namespace Star {

class WorldClient;
class GuiContext;

class NameplatePainter {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<NameplatePainter>, StarException> create(GuiContext& context);

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::shared_ptr<WorldClient> const& world, WorldCamera const& camera, bool inspectionMode);
  [[nodiscard]] std::expected<void, StarException> render(WorldClient const& world, WorldCamera const& camera, float interpolation);

private:
  explicit NameplatePainter(GuiContext& context);
  [[nodiscard]] std::expected<void, StarException> initialize();
  struct Nametag {
    String name;
    std::optional<String> statusText;
    Vec3B color;
    float opacity;
    EntityId entityId;
    Vec2F origin;
  };

  TextPositioning namePosition(Vec2F bubblePosition) const;
  TextPositioning statusPosition(Vec2F bubblePosition) const;
  [[nodiscard]] std::expected<RectF, StarException> determineBoundBox(Vec2F bubblePosition, Nametag const& nametag) const;

  std::reference_wrapper<GuiContext> m_guiContext;

  bool m_showMasterNames;
  float m_opacityRate;
  float m_inspectOpacityRate;
  Vec2F m_offset;
  Vec2F m_statusOffset;
  TextStyle m_textStyle;
  TextStyle m_statusTextStyle;
  float m_opacityBoost;


  Set<EntityId> m_entitiesWithNametags;
  Set<EntityId> m_foundEntities;
  BubbleSeparator<Nametag> m_nametags;
};

}

export module star.nameplate_painter;

export namespace Star {
  using ::Star::WorldClient;
  using ::Star::NameplatePainter;
}
