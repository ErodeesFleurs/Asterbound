module;
import std;
import star.format;
import star.type_identity;
import star.json;

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

namespace Star {

class LuaCallbacks;
class AudioInstance;
class GuiReader;
class Item;

enum class PaneAnchor {
  None,
  BottomLeft,
  BottomRight,
  TopLeft,
  TopRight,
  CenterBottom,
  CenterTop,
  CenterLeft,
  CenterRight,
  Center
};
extern std::expected<EnumMap<PaneAnchor>, StarException> const PaneAnchorNames;

// Interface-pixel origin of an anchored pane, before authored offsets.
Vec2I paneAnchorBase(PaneAnchor anchor, Vec2I windowSize, Vec2I paneSize);

class Pane : public Widget {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<Pane, Widget>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<Pane>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<Pane>, StarException> create(GuiContext& context);
  ~Pane() override;

  using Widget::removeChild;
  bool removeChild(Widget& member) override;

  struct BGResult {
    String header;
    String body;
    String footer;
  };

  [[nodiscard]] virtual std::expected<void, StarException> displayed();
  [[nodiscard]] virtual std::expected<void, StarException> dismissed();

  void dismiss();
  bool isDismissed() const;
  bool isDisplayed() const;

  Vec2I centerOffset() const;

  // members are drawn strictly in the order they are added,
  // so add them in the correct order.

  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;
  [[nodiscard]] virtual std::expected<void, StarException> setFocus(Widget const& focus);
  virtual void removeFocus(Widget const& focus);
  virtual void removeFocus();

  [[nodiscard]] std::expected<void, StarException> update(float dt) override;
  [[nodiscard]] virtual std::expected<void, StarException> tick(float dt);

  bool dragActive() const;
  Vec2I dragMouseOrigin() const;
  void setDragActive(bool dragActive, Vec2I dragMouseOrigin);
  void drag(Vec2I mousePosition);

  bool inWindow(Vec2I const& position) const;
  bool inDragArea(Vec2I const& position) const;
  Vec2I cursorRelativeToPane(Vec2I const& position) const;

  [[nodiscard]] std::expected<void, StarException> setBG(BGResult const& res);
  [[nodiscard]] std::expected<void, StarException> setBG(String const& header, String const& body = "", String const& footer = "");
  BGResult getBG() const;

  void lockPosition();
  void unlockPosition();

  [[nodiscard]] std::expected<void, StarException> setTitle(std::shared_ptr<Widget> icon, String const& title, String const& subTitle);
  void setTitleString(String const& title, String const& subTitle);
  [[nodiscard]] std::expected<void, StarException> setTitleIcon(std::shared_ptr<Widget> icon);
  String title() const;
  String subTitle() const;
  std::shared_ptr<Widget> titleIcon() const;

  std::optional<std::reference_wrapper<Pane>> window() override;
  std::optional<std::reference_wrapper<Pane const>> window() const override;

  PaneAnchor anchor();
  void setAnchor(PaneAnchor anchor);
  Vec2I anchorOffset() const;
  void setAnchorOffset(Vec2I anchorOffset);
  bool hasDisplayed() const;

  // If a tooltip popup should be created at the given mouse position, return a
  // new pane to be used as the tooltip.
  [[nodiscard]] virtual std::expected<std::shared_ptr<Pane>, StarException> createTooltip(Vec2I const& screenPosition);
  [[nodiscard]] virtual std::expected<std::optional<String>, StarException> cursorOverride(Vec2I const& screenPosition);
  [[nodiscard]] virtual std::expected<std::optional<std::shared_ptr<Item>>, StarException> shiftItemFromInventory(std::shared_ptr<Item> const& input);

  [[nodiscard]] virtual std::expected<LuaCallbacks, StarException> makePaneCallbacks();
protected:
  Pane(GuiContext& context);
  [[nodiscard]] std::expected<void, StarException> initializePane();
  virtual std::shared_ptr<GuiReader> reader();
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

  String m_bgHeader;
  String m_bgBody;
  String m_bgFooter;

  Vec2I m_footerSize;
  Vec2I m_bodySize;
  Vec2I m_headerSize;

  bool m_dismissed;
  bool m_dragActive;
  Vec2I m_dragMouseOrigin;
  bool m_lockPosition;
  Vec2I m_centerOffset;

  std::shared_ptr<Widget> m_mouseOver;
  std::shared_ptr<Widget> m_clickDown;
  std::shared_ptr<Widget> m_focusWidget;

  std::shared_ptr<Widget> m_icon;
  String m_title;
  String m_subTitle;
  TextStyle m_textStyle;
  Vec2I m_iconOffset;
  Vec2I m_titleOffset;
  Vec2I m_subTitleOffset;
  Color m_titleColor;
  Color m_subTitleColor;

  PaneAnchor m_anchor;
  Vec2I m_anchorOffset;
  bool m_hasDisplayed;

  List<std::pair<String, std::shared_ptr<AudioInstance>>> m_playingSounds;
};

std::expected<void, StarException> Pane::displayed() {
  auto shown = show();
  if (!shown)
    return std::unexpected(std::move(shown.error()));
  m_dismissed = false;
  m_hasDisplayed = true;
  return {};
}

}

export module star.pane;

export namespace Star {
using ::Star::AudioInstance;
using ::Star::GuiReader;
using ::Star::Item;
using ::Star::LuaCallbacks;
using ::Star::Pane;
using ::Star::PaneAnchor;
using ::Star::paneAnchorBase;
using ::Star::PaneAnchorNames;
}
