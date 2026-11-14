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
import star.rect;
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
import star.button_group;

import star.either;
namespace Star {

class ButtonWidget : public Widget {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<ButtonWidget, Widget>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ButtonWidget>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<ButtonWidget>, StarException> create(GuiContext& context);
  [[nodiscard]] static std::expected<std::shared_ptr<ButtonWidget>, StarException> create(GuiContext& context, WidgetCallbackFunc callback,
      String const& baseImage, String const& hoverImage = "",
      String const& pressedImage = "", String const& disabledImage = "");
  ~ButtonWidget() override;

  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;
  [[nodiscard]] std::expected<void, StarException> mouseOver() override;
  [[nodiscard]] std::expected<void, StarException> mouseOut() override;
  [[nodiscard]] std::expected<void, StarException> mouseReturnStillDown() override;
  [[nodiscard]] std::expected<void, StarException> hide() override;

  // Callback is called when the checked / pressed state is changed.
  void setCallback(WidgetCallbackFunc callback);

  std::shared_ptr<ButtonGroup> buttonGroup() const;
  // Sets the button group for this widget, and adds it to the button group if
  // it is not already added.  Additionally, sets the button as checkable.
  void setButtonGroup(std::shared_ptr<ButtonGroup> buttonGroup, int id = ButtonGroup::NoButton);
  // If a button group is set, returns this button's id in the button group.
  int buttonGroupId();

  bool isHovered() const;

  bool isPressed() const;
  [[nodiscard]] std::expected<void, StarException> setPressed(bool pressed);

  bool isCheckable() const;
  void setCheckable(bool checkable);

  bool isHighlighted() const;
  void setHighlighted(bool highlighted);

  bool isChecked() const;
  void setChecked(bool checked);
  // Either checks a button, or toggles the state, depending on whether the
  // button is part of an exclusive group or not.
  [[nodiscard]] std::expected<void, StarException> check();

  bool sustainCallbackOnDownHold();
  void setSustainCallbackOnDownHold(bool sustain);

  [[nodiscard]] std::expected<void, StarException> setImages(String const& baseImage,
      String const& hoverImage = "",
      String const& pressedImage = "",
      String const& disabledImage = "");
  [[nodiscard]] std::expected<void, StarException> setCheckedImages(String const& baseImage,
      String const& hoverImage = "",
      String const& pressedImage = "",
      String const& disabledImage = "");
  void setOverlayImage(String const& overlayImage = "");

  // Used to offset drawing when the button is being pressed / checked
  Vec2I const& pressedOffset() const;
  void setPressedOffset(Vec2I const& offset);

  virtual String const& getText() const;
  virtual void setText(String const& text);
  virtual void setFontSize(int size);
  virtual void setFontDirectives(String directives);
  virtual void setTextOffset(Vec2I textOffset);

  void setTextAlign(HorizontalAnchor hAnchor);
  void setFontColor(Color color);
  void setFontColorDisabled(Color color);
  void setFontColorChecked(Color color);

  std::shared_ptr<Widget> getChildAt(Vec2I const& pos) override;

  void disable();
  void enable();
  void setEnabled(bool enabled);

  void setInvisible(bool invisible);

protected:
  RectI getScissorRect() const override;
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

  [[nodiscard]] std::expected<void, StarException> drawButtonPart(String const& image, Vec2F const& position);
  [[nodiscard]] std::expected<void, StarException> updateSize();

  WidgetCallbackFunc m_callback;
  std::weak_ptr<ButtonGroup> m_buttonGroup;

  bool m_hovered;
  bool m_pressed;
  bool m_checkable;
  bool m_checked;

  bool m_disabled;
  bool m_highlighted;

  String m_baseImage;
  String m_hoverImage;
  String m_pressedImage;
  String m_disabledImage;

  bool m_hasCheckedImages;
  String m_baseImageChecked;
  String m_hoverImageChecked;
  String m_pressedImageChecked;
  String m_disabledImageChecked;

  String m_overlayImage;

  bool m_invisible;

  Vec2I m_pressedOffset;
  Vec2U m_buttonBoundSize;

  TextStyle m_textStyle;
  String m_text;
  Vec2I m_textOffset;

  StringList m_clickSounds;
  StringList m_releaseSounds;
  StringList m_hoverSounds;
  StringList m_hoverOffSounds;

  bool m_sustain;

protected:
  explicit ButtonWidget(GuiContext& context);
  [[nodiscard]] std::expected<void, StarException> initialize();

private:
  HorizontalAnchor m_hTextAnchor;
  Color m_fontColor;
  Color m_fontColorDisabled;
  std::optional<Color> m_fontColorChecked;
};

ButtonWidget::~ButtonWidget() {
  if (auto group = m_buttonGroup.lock())
    group->removeButton(*this);
}

}

export module star.button_widget;

export namespace Star {
  using ::Star::ButtonWidget;
}
