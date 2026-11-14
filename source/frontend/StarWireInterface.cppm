module;
import std;
import star.format;
import star.type_identity;
import star.json;

import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;


import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.pane;
import star.world_geometry;
import star.wiring;

namespace Star {

class WorldClient;
class WorldPainter;
class Player;

class WirePane : public Pane, public WireConnector {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<WirePane, Pane>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<WirePane>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<WirePane>, StarException> create(GuiContext& guiContext, std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, std::shared_ptr<WorldPainter> worldPainter);
  ~WirePane() override = default;

  [[nodiscard]] std::expected<void, StarException> update(float dt) override;
  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;

  [[nodiscard]] std::expected<SwingResult, StarException> swing(WorldGeometry const& geometry, Vec2F position,
                                                                FireMode mode) override;
  bool connecting() override;

  virtual void reset();

protected:
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

private:
  WirePane(GuiContext& guiContext, std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, std::shared_ptr<WorldPainter> worldPainter);
  [[nodiscard]] std::expected<void, StarException> initialize();
  [[nodiscard]] std::expected<void, StarException> renderWire(Vec2F from, Vec2F to, Color baseColor);

  std::shared_ptr<WorldClient> m_worldClient;
  std::shared_ptr<Player> m_player;
  // Panes can escape their manager and be displayed again after dismissal.
  // Retain the painter until a terminal detach protocol prevents further rendering.
  std::shared_ptr<WorldPainter> m_worldPainter;
  Vec2I m_mousePos;
  bool m_connecting;
  WireDirection m_sourceDirection;
  WireConnection m_sourceConnector;

  Vec2F m_inSize;
  Vec2F m_outSize;
  Vec2F m_nodeSize;

  float m_beamWidthDev;
  float m_minBeamWidth;
  float m_maxBeamWidth;
  float m_beamTransDev;
  float m_minBeamTrans;
  float m_maxBeamTrans;
  float m_innerBrightnessScale;
  float m_firstStripeThickness;
  float m_secondStripeThickness;
};

}

export module star.wire_interface;

export namespace Star {
  using ::Star::WorldClient;
  using ::Star::WorldPainter;
  using ::Star::Player;
  using ::Star::WirePane;
}
