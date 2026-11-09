module;
import std;
import star.format;
import star.json;

import star.game_types;
import star.vector;
import star.casting;
import star.input_event;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.color;
import star.font;
import star.bi_map;
import star.ref_ptr;
import star.renderer;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.ordered_map;
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
import star.game_timers;

namespace Star {

enum class PaneLayer {
  // A special class of window only meant to be used by PaneManager to display
  // tooltips given by Pane::createTooltip
  Tooltip,
  // A special class of window that is displayed above all other windows and
  // turns off input to other windows and the hud until it is dismissed.
  ModalWindow,
  // Window layer for regular windows that are regularly displayed and
  // dismissed and dragged around.
  Window,
  // The bottom GUI layer, for persistent hud elements that are always or almost
  // always shown.  Not key dismissable.
  Hud,
  // Layer for interface elements which are logically part of the world but
  // handled by GUI panes (such as wires)
  World,
  // Behind the regular GUI layers; may be rendered separately before native
  // overlays by an interface with multiple compositing phases.
  Background
};
extern std::expected<EnumMap<PaneLayer>, StarException> const PaneLayerNames;


// This class handles a set of panes to be drawn as a collective windowing
// interface.  It is a set of panes on separate distinct layers, where each
// layer contains a z-ordered list of panes to display.
class PaneManager {
public:
  using DismissCallback = std::function<std::expected<void, StarException>(std::shared_ptr<Pane> const&)>;

  PaneManager(GuiContext& context);
  [[nodiscard]] static std::expected<std::shared_ptr<PaneManager>, StarException> create(GuiContext& context);

  // Display a pane on any given layer.  The pane lifetime in this class is
  // only during display, once dismissed, the pane is forgotten completely.
  [[nodiscard]] std::expected<void, StarException> displayPane(PaneLayer paneLayer, std::shared_ptr<Pane> const& pane, DismissCallback onDismiss = {});

  bool isDisplayed(std::shared_ptr<Pane> const& pane) const;

  // Dismiss a given displayed pane.  Pane must already be displayed.
  [[nodiscard]] std::expected<void, StarException> dismissPane(std::shared_ptr<Pane> const& pane);

  // Dismisses all panes in the given layers.
  [[nodiscard]] std::expected<void, StarException> dismissAllPanes(Set<PaneLayer> const& paneLayers);
  [[nodiscard]] std::expected<void, StarException> dismissAllPanes();

  std::shared_ptr<Pane> topPane(Set<PaneLayer> const& paneLayers) const;
  std::shared_ptr<Pane> topPane() const;

  // Brign an already displayed pane to the top of its layer.
  [[nodiscard]] std::expected<void, StarException> bringToTop(std::shared_ptr<Pane> const& pane);

  // Position a pane adjacent to an anchor pane in a direction where
  // it will fit on the screen
  [[nodiscard]] std::expected<void, StarException> bringPaneAdjacent(std::shared_ptr<Pane> const& anchor, std::shared_ptr<Pane> const& adjacent, int gap);

  std::shared_ptr<Pane> getPaneAt(Set<PaneLayer> const& paneLayers, Vec2I const& position) const;
  std::shared_ptr<Pane> getPaneAt(Vec2I const& position) const;
  List<std::shared_ptr<Pane>> getAllPanes();

  void setBackgroundWidget(std::shared_ptr<Widget> bg);

  [[nodiscard]] std::expected<void, StarException> dismissWhere(std::function<std::expected<bool, StarException>(std::shared_ptr<Pane> const&)> func);

  // Returns the pane/widget that has captured the keyboard, if any.
  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> keyboardCapturedPane() const;
  [[nodiscard]] std::expected<std::shared_ptr<Widget>, StarException> keyboardCapturedWidget() const;
  // Returns true if the current widget that has captured the keyboard is
  // accepting text input.
  [[nodiscard]] std::expected<bool, StarException> keyboardCapturedForTextInput() const;
  // Keyboard action candidates in exactly the native dispatch order, including
  // its captured-pane exclusivity and first-modal barrier.
  [[nodiscard]] std::expected<List<std::shared_ptr<Pane>>, StarException> inputTargetPanes() const;

  [[nodiscard]] std::expected<bool, StarException> sendInputEvent(InputEvent const& event);

  [[nodiscard]] std::expected<void, StarException> render();
  // Draw an inclusive front-to-back layer range, without the background widget.
  [[nodiscard]] std::expected<void, StarException> renderLayers(PaneLayer front, PaneLayer back);
  [[nodiscard]] std::expected<void, StarException> update(float dt);

protected:
  [[nodiscard]] std::expected<void, StarException> initializePaneManager();

private:
  [[nodiscard]] std::expected<Vec2I, StarException> windowSize() const;
  [[nodiscard]] std::expected<Vec2I, StarException> calculatePaneOffset(std::shared_ptr<Pane> const& pane) const;
  [[nodiscard]] std::expected<Vec2I, StarException> calculateNewInterfacePosition(std::shared_ptr<Pane> const& pane, float interfaceScaleRatio) const;
  [[nodiscard]] std::expected<bool, StarException> dismiss(std::shared_ptr<Pane> const& pane);

  GuiContext& m_context;
  float m_prevInterfaceScale;

  // Map of each pane layer, where the 0th pane is the topmost pane in each layer.
  Map<PaneLayer, OrderedMap<std::shared_ptr<Pane>, DismissCallback>> m_displayedPanes;

  std::shared_ptr<Widget> m_backgroundWidget;

  float m_tooltipMouseoverRadius;
  Vec2I m_tooltipMouseOffset;
  GameTimer m_tooltipShowTimer;
  Vec2I m_tooltipLastMousePos;
  Vec2I m_tooltipInitialPosition;
  std::shared_ptr<Pane> m_activeTooltip;
  std::shared_ptr<Pane> m_tooltipParentPane;
};

}

export module star.pane_manager;

export namespace Star {
  using ::Star::PaneLayer;
  using ::Star::PaneManager;
  using ::Star::PaneLayerNames;
}
