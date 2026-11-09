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
import star.button_widget;
import star.image_widget;

namespace Star {

class ScrollThumb : public Widget {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ScrollThumb, Widget>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ScrollThumb>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<ScrollThumb>, StarException> create(GuiContext& context, GuiDirection direction);
  ~ScrollThumb() override = default;

  void setDirection(GuiDirection direction);
  void setImages(ImageStretchSet const& base,
      ImageStretchSet const& hover = ImageStretchSet(),
      ImageStretchSet const& pressed = ImageStretchSet());
  [[nodiscard]] std::expected<void, StarException> setImages(Json const& images);

  bool isHovered() const;
  bool isPressed() const;

  void setHovered(bool hovered);
  void setPressed(bool pressed);

  [[nodiscard]] std::expected<void, StarException> mouseOver() override;
  [[nodiscard]] std::expected<void, StarException> mouseOut() override;

  [[nodiscard]] std::expected<Vec2U, StarException> baseSize() const;

protected:
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

private:
  ScrollThumb(GuiContext& context, GuiDirection direction);

  GuiDirection m_direction;

  ImageStretchSet m_baseThumb;
  ImageStretchSet m_hoverThumb;
  ImageStretchSet m_pressedThumb;

  bool m_hovered;
  bool m_pressed;
};

class ScrollBar : public Widget {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ScrollBar, Widget>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ScrollBar>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<ScrollBar>, StarException> create(GuiContext& context, GuiDirection direction, WidgetCallbackFunc forwardFunc, WidgetCallbackFunc backwardFunc);

  [[nodiscard]] std::expected<void, StarException> setButtonImages(Json const& images);

  [[nodiscard]] std::expected<int, StarException> trackSize() const;
  [[nodiscard]] std::expected<float, StarException> sizeRatio() const;
  Vec2I size() const override;
  [[nodiscard]] std::expected<float, StarException> scrollRatio() const;
  [[nodiscard]] std::expected<Vec2I, StarException> offsetFromThumbPosition(Vec2I const& thumbPosition) const;

  std::shared_ptr<ButtonWidget> forwardButton() const;
  std::shared_ptr<ButtonWidget> backwardButton() const;
  std::shared_ptr<ScrollThumb> thumb() const;

protected:
  [[nodiscard]] std::expected<void, StarException> drawChildren() override;

private:
  ScrollBar(GuiContext& context, GuiDirection direction);
  [[nodiscard]] std::expected<void, StarException> initialize(WidgetCallbackFunc forwardFunc, WidgetCallbackFunc backwardFunc);
  GuiDirection m_direction;

  std::shared_ptr<ButtonWidget> m_forward; // up or right, makes the offset higher
  std::shared_ptr<ButtonWidget> m_backward; // down or left, makes the offset lower
  std::shared_ptr<ScrollThumb> m_thumb;

  ImageStretchSet m_track;
};

class ScrollArea : public Widget {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ScrollArea, Widget>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ScrollArea>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<ScrollArea>, StarException> create(GuiContext& context);

  [[nodiscard]] std::expected<void, StarException> setButtonImages(Json const& images);
  [[nodiscard]] std::expected<void, StarException> setThumbImages(Json const& images);

  RectI contentBoundRect() const;
  Vec2I contentSize() const;

  Vec2I areaSize() const;

  void scrollAreaBy(Vec2I const& offset);

  Vec2I scrollOffset() const;
  Vec2I maxScrollPosition() const;

  bool horizontalScroll() const;
  void setHorizontalScroll(bool horizontal);

  bool verticalScroll() const;
  void setVerticalScroll(bool vertical);

  void setUpdatesChildren(bool slop);

  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;
  [[nodiscard]] std::expected<void, StarException> update(float dt) override;

protected:
  [[nodiscard]] std::expected<void, StarException> drawChildren() override;

private:
  explicit ScrollArea(GuiContext& context);
  [[nodiscard]] std::expected<void, StarException> initialize();
  int advanceFactorHelper();

  int m_buttonAdvance;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_advanceLimiter;

  Vec2I m_scrollOffset;
  Vec2I m_lastMaxScroll;
  Vec2I m_contentSize;

  bool m_dragActive;
  GuiDirection m_dragDirection;
  Vec2I m_dragOffset;

  std::shared_ptr<ScrollBar> m_vBar;
  std::shared_ptr<ScrollBar> m_hBar;
  std::shared_ptr<ImageWidget> m_cornerBlock;

  bool m_horizontalScroll;
  bool m_verticalScroll;

  bool m_updatesChildren;
};
}

export module star.scroll_area;

export namespace Star {
  using ::Star::ScrollThumb;
  using ::Star::ScrollBar;
  using ::Star::ScrollArea;
}
