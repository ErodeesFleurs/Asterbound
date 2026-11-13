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
import star.either;
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

class StackWidget : public Widget {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<StackWidget, Widget>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<StackWidget>(); }

  explicit StackWidget(GuiContext& context) : Widget(context) {}

  [[nodiscard]] std::expected<void, StarException> showPage(std::size_t page);
  [[nodiscard]] std::expected<void, StarException> showPage(String const& name);

  Either<std::size_t, String> currentPage() const;

  [[nodiscard]] std::expected<void, StarException> addChild(String const& name,
                                                            std::shared_ptr<Widget> member) override;

private:
  std::shared_ptr<Widget> m_shownPage;
  Either<std::size_t, String> m_page;
};

}

export module star.stack_widget;

export namespace Star {
  using ::Star::StackWidget;
}
