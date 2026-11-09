module;
import std;
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

import star.format;
namespace Star {

class ProgressWidget : public Widget {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ProgressWidget, Widget>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ProgressWidget>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<ProgressWidget>, StarException> create(GuiContext& context, String const& background,
      String const& overlay,
      ImageStretchSet const& progressSet,
      GuiDirection direction);
  ~ProgressWidget() override = default;

  void setCurrentProgressLevel(float amount);
  void setMaxProgressLevel(float amount);

  void setColor(Color const& color);
  void setOverlay(String const& overlay);

protected:
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

  float m_progressLevel;
  float m_maxLevel;

  Color m_color;

  String m_background;
  String m_overlay;
  ImageStretchSet m_bar;
  GuiDirection m_direction;

private:
  ProgressWidget(GuiContext& context, String const& background, String const& overlay, ImageStretchSet const& progressSet, GuiDirection direction);
};

}

export module star.progress_widget;

export namespace Star {
  using ::Star::ProgressWidget;
}
