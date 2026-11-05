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

namespace Star {

enum class SpecialRepeatKeyCodes : String::Char { None, Delete, Backspace, Left, Right };

class TextBoxWidget : public Widget {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<TextBoxWidget, Widget>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<TextBoxWidget>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<TextBoxWidget>, StarException> create(GuiContext& context, String const& startingText, String const& hint, WidgetCallbackFunc callback);

  [[nodiscard]] virtual std::expected<void, StarException> update(float dt) override;

  String const& getText() const;
  [[nodiscard]] std::expected<bool, StarException> setText(String const& text, bool callback = true, bool moveCursor = true);

  String const& getHint() const;
  void setHint(String const& hint);

  int const& getCursorPosition() const;
  [[nodiscard]] std::expected<void, StarException> setCursorPosition(int cursorPosition);
  [[nodiscard]] std::expected<void, StarException> setSelection(int anchor, int cursor);
  [[nodiscard]] std::expected<Json, StarException> editorState() const;
  void setEditorRendering(bool enabled);
  void setTextStyle(TextStyle const& style);


  bool getHidden() const;
  void setHidden(bool hidden);

  // Set the regex that the text-box must match.  Defaults to .*
  String getRegex();
  void setRegex(String const& regex);

  void setColor(Color const& color);
  void setDirectives(String const& directives);
  void setFontSize(int fontSize);
  void setMaxWidth(int maxWidth);
  void setOverfillMode(bool overfillMode);

  void setOnBlurCallback(WidgetCallbackFunc onBlur);
  void setOnEnterKeyCallback(WidgetCallbackFunc onEnterKey);
  void setOnEscapeKeyCallback(WidgetCallbackFunc onEscapeKey);

  void setNextFocus(std::optional<String> nextFocus);
  void setPrevFocus(std::optional<String> prevFocus);

  void setFont(String const& font);

  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;

  void setDrawBorder(bool drawBorder);
  void setTextAlign(HorizontalAnchor hAnchor);
  [[nodiscard]] std::expected<int, StarException> getCursorDrawOffset() const;

  [[nodiscard]] virtual std::expected<void, StarException> mouseOver() override;
  [[nodiscard]] virtual std::expected<void, StarException> mouseOut() override;
  [[nodiscard]] virtual std::expected<void, StarException> mouseReturnStillDown() override;

  [[nodiscard]] virtual std::expected<void, StarException> blur() override;

  [[nodiscard]] virtual std::expected<KeyboardCaptureMode, StarException> keyboardCaptureMode() const override;
  [[nodiscard]] virtual std::expected<std::optional<std::pair<RectI, int>>, StarException> keyboardCaptureArea() const override;

protected:
  [[nodiscard]] virtual std::expected<void, StarException> renderImpl() override;

private:
  TextBoxWidget(GuiContext& context, String const& startingText, String const& hint, WidgetCallbackFunc callback);
  [[nodiscard]] std::expected<void, StarException> initialize();
  [[nodiscard]] std::expected<String, StarException> displayText() const;
  [[nodiscard]] std::expected<bool, StarException> innerSendEvent(InputEvent const& event);
  [[nodiscard]] std::expected<bool, StarException> modText(String const& text);
  [[nodiscard]] std::expected<bool, StarException> editText(std::size_t start, std::size_t count, String const& replacement);
  [[nodiscard]] std::expected<bool, StarException> newTextValid(String const& text) const;
  [[nodiscard]] std::expected<bool, StarException> replaceSelection(String const& replacement);

  bool m_textHidden;
  String m_text;
  String m_hint;
  String m_regex;
  HorizontalAnchor m_hAnchor;
  VerticalAnchor m_vAnchor;
  TextStyle m_textStyle;
  int m_maxWidth;
  int m_cursorOffset;
  int m_selectionAnchor;
  bool m_repeatSelecting;
  bool m_editorRendering;
  bool m_isHover;
  bool m_isPressed;
  SpecialRepeatKeyCodes m_repeatCode;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_repeatKeyThreshold;
  WidgetCallbackFunc m_callback;
  WidgetCallbackFunc m_onBlur;
  WidgetCallbackFunc m_onEnterKey;
  WidgetCallbackFunc m_onEscapeKey;
  std::optional<String> m_nextFocus;
  std::optional<String> m_prevFocus;
  bool m_drawBorder;
  Vec2I m_cursorHoriz;
  Vec2I m_cursorVert;
  bool m_overfillMode;
};

}

export module star.text_box_widget;

export namespace Star {
  using ::Star::SpecialRepeatKeyCodes;
  using ::Star::TextBoxWidget;
}
