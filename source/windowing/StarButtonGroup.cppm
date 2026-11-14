module;
import std;
import star.format;
import star.type_identity;
import star.json;

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

class ButtonWidget;

// Manages group of buttons in which *at most* a single button can be checked
// at any time.
class ButtonGroup {
public:
  friend class ButtonWidget;

  static constexpr int NoButton = -1;

  // Callback is called when any child buttons checked state is changed, and
  // its parameter is the button being checked.
  void setCallback(WidgetCallbackFunc callback);

  std::optional<std::reference_wrapper<ButtonWidget>> button(int id) const;
  List<std::reference_wrapper<ButtonWidget>> buttons() const;
  std::size_t buttonCount() const;

  int addButton(ButtonWidget& button, int id = NoButton);
  void removeButton(ButtonWidget& button);

  int id(ButtonWidget const& button) const;

  [[nodiscard]] std::expected<void, StarException> select(int id);

  // Will return empty if no button is checked.
  std::optional<std::reference_wrapper<ButtonWidget>> checkedButton() const;
  // Will return NoButton if no button is checked.
  int checkedId() const;

  // when true it is not required for one of the buttons to be selected
  bool toggle() const;
  void setToggle(bool toggleMode);

protected:
  // Should be called by child button widgets when they are changed from
  // unchecked to checked.
  [[nodiscard]] std::expected<void, StarException> wasChecked(ButtonWidget& self);

private:
  WidgetCallbackFunc m_callback;
  Map<int, std::weak_ptr<Widget>> m_buttons;
  Map<std::weak_ptr<Widget const>, int, std::owner_less<std::weak_ptr<Widget const>>> m_buttonIds;
  bool m_toggle = false;
};

class ButtonGroupWidget : public ButtonGroup, public Widget {
public:
  void const* query_type(TypeKey key) const noexcept override {
    // ButtonGroup is a non-polymorphic sibling, not a query-protocol base.
    if (key == type_key<ButtonGroup>())
      return static_cast<ButtonGroup const*>(this);
    return query_type_of<ButtonGroupWidget, Widget>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ButtonGroupWidget>(); }

  explicit ButtonGroupWidget(GuiContext& context) : Widget(context) {}
};
}

export module star.button_group;

export namespace Star {
  using ::Star::ButtonGroup;
  using ::Star::ButtonGroupWidget;
  using ::Star::ButtonWidget;
}
