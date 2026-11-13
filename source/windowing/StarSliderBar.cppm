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
import star.button_group;
import star.button_widget;
import star.image_widget;

namespace Star {

// This class only does left right right now.  If you need advanced
// multiorientation physics please
// implement it in.
class SliderBarWidget : public Widget {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<SliderBarWidget, Widget>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SliderBarWidget>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<SliderBarWidget>, StarException> create(GuiContext& context, String const& grid, bool showSpinner = true);

  [[nodiscard]] std::expected<void, StarException> setJogImages(String const& baseImage, String const& hoverImage = "", String const& pressedImage = "", String const& disabledImage = "");

  [[nodiscard]] std::expected<void, StarException> setRange(int low, int high, int delta);
  [[nodiscard]] std::expected<void, StarException> setRange(Vec2I const& range, int delta);
  [[nodiscard]] std::expected<void, StarException> setVal(int val, bool callbackIfChanged = true);
  int val() const;

  void setEnabled(bool enabled);

  void setCallback(WidgetCallbackFunc callback);

  [[nodiscard]] std::expected<void, StarException> update(float dt) override;

  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;

private:
  explicit SliderBarWidget(GuiContext& context);
  [[nodiscard]] std::expected<void, StarException> initialize(String const& grid, bool showSpinner);
  [[nodiscard]] std::expected<void, StarException> leftCallback();
  [[nodiscard]] std::expected<void, StarException> rightCallback();

  std::shared_ptr<ButtonWidget> m_leftButton;
  std::shared_ptr<ButtonWidget> m_rightButton;
  std::shared_ptr<ImageWidget> m_grid;
  std::shared_ptr<ButtonWidget> m_jog;
  int m_low;
  int m_high;
  int m_delta;
  int m_val;

  bool m_updateJog;

  Vec2I m_savedJogPos;
  Vec2I m_jogDragPos;
  bool m_jogDragActive;

  bool m_enabled;

  WidgetCallbackFunc m_callback;
};
}

export module star.slider_bar;

export namespace Star {
  using ::Star::SliderBarWidget;
}
