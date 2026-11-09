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
import star.layout;

namespace Star {

// Super simple, only supports left to right, top to bottom flow layouts
// currently
class FlowLayout : public Layout {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<FlowLayout, Layout>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<FlowLayout>();
  }

  FlowLayout(GuiContext& context);
  [[nodiscard]] std::expected<void, StarException> update(float dt) override;
  void setSpacing(Vec2I const& spacing);
  void setWrapping(bool wrap);

private:
  Vec2I m_spacing;
  bool m_wrap;
};

}

export module star.flow_layout;

export namespace Star {
  using ::Star::FlowLayout;
}
