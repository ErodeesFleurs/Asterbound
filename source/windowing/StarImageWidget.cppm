module;
import std;
import star.type_identity;
import star.json;
import star.format;

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

class ImageWidget : public Widget {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<ImageWidget, Widget>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ImageWidget>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<ImageWidget>, StarException> create(GuiContext& context, String const& image = {});

  bool interactive() const override;
  [[nodiscard]] std::expected<void, StarException> setImage(String const& image);
  [[nodiscard]] std::expected<void, StarException> setScale(float scale);
  [[nodiscard]] std::expected<void, StarException> setRotation(float rotation);
  [[nodiscard]] std::expected<String, StarException> image() const;

  [[nodiscard]] std::expected<void, StarException> setDrawables(List<Drawable> drawables);
  Vec2I offset();
  void setOffset(Vec2I const& offset);
  bool centered();
  [[nodiscard]] std::expected<void, StarException> setCentered(bool centered);
  bool trim();
  [[nodiscard]] std::expected<void, StarException> setTrim(bool trim);

  [[nodiscard]] std::expected<void, StarException> setMaxSize(Vec2I const& size);
  [[nodiscard]] std::expected<void, StarException> setMinSize(Vec2I const& size);

  RectI screenBoundRect() const override;

protected:
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

private:
  explicit ImageWidget(GuiContext& context);
  [[nodiscard]] std::expected<void, StarException> transformDrawables();

  List<Drawable> m_baseDrawables;
  List<Drawable> m_drawables;
  bool m_centered;
  bool m_trim;
  float m_scale;
  float m_rotation;
  Vec2I m_offset;
  Vec2I m_maxSize;
  Vec2I m_minSize;
};

}

export module star.image_widget;

export namespace Star {
  using ::Star::ImageWidget;
}
