import std;
import star.json;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;

import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.renderer;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
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
import star.image_widget;

import star.format;
namespace Star {

std::expected<std::shared_ptr<ImageWidget>, StarException> ImageWidget::create(GuiContext& context, String const& image) {
  auto widget = std::shared_ptr<ImageWidget>(new ImageWidget(context));
  auto result = widget->setImage(image);
  if (!result) return std::unexpected(std::move(result.error()));
  return widget;
}

ImageWidget::ImageWidget(GuiContext& context) : Widget(context) {
  m_centered = false;
  m_trim = false;
  m_scale = 1;
  m_rotation = 0;
  m_maxSize = Vec2I{4096, 4096};
  m_minSize = Vec2I{0, 0};
  m_offset = Vec2I{0, 0};
}

std::expected<void, StarException> ImageWidget::renderImpl() {
  auto screenPos = screenPosition();
  for (auto const& drawable : m_drawables) {
    auto result = context().drawDrawable(drawable, Vec2F(screenPos) * context().interfaceScale() + Vec2F(m_offset), context().interfaceScale(), Vec4B::filled(255));
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

bool ImageWidget::interactive() const {
  return false;
}

std::expected<void, StarException> ImageWidget::setImage(String const& image) {
  if (image.empty())
    return setDrawables({});
  auto metadata = context().root().image_metadata_database();
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  auto drawable = Drawable::make_image(**metadata, image, 1.0f, false, Vec2F());
  if (!drawable) return std::unexpected(std::move(drawable.error()));
  return setDrawables({std::move(*drawable)});
}

std::expected<void, StarException> ImageWidget::setScale(float scale) {
  m_scale = scale;
  return transform_drawables();
}

std::expected<void, StarException> ImageWidget::setRotation(float rotation) {
  m_rotation = rotation;
  return transform_drawables();
}

std::expected<String, StarException> ImageWidget::image() const {
  if (!m_drawables.empty() && !m_drawables[0].is_image())
    return std::unexpected(GuiException("Image widget drawable is not an image"));
  if (m_drawables.empty())
    return "";
  else
    return AssetPath::join(m_drawables[0].image_part().image);
}

std::expected<void, StarException> ImageWidget::setDrawables(List<Drawable> drawables) {
  m_baseDrawables = std::move(drawables);
  return transform_drawables();
}

Vec2I ImageWidget::offset() {
  return m_offset;
}

void ImageWidget::setOffset(Vec2I const& offset) {
  m_offset = offset;
}

bool ImageWidget::centered() {
  return m_centered;
}

std::expected<void, StarException> ImageWidget::setCentered(bool centered) {
  m_centered = centered;
  return transform_drawables();
}

bool ImageWidget::trim() {
  return m_trim;
}

std::expected<void, StarException> ImageWidget::setTrim(bool trim) {
  m_trim = trim;
  return transform_drawables();
}

std::expected<void, StarException> ImageWidget::setMaxSize(Vec2I const& size) {
  m_maxSize = size;
  return transform_drawables();
}

std::expected<void, StarException> ImageWidget::setMinSize(Vec2I const& size) {
  m_minSize = size;
  return transform_drawables();
}

RectI ImageWidget::screenBoundRect() const {
  RectI base = RectI::with_size(screenPosition(), size());
  if (m_centered) {
    base.translate(-Vec2I::floor(size() / 2));
  }

  return base;
}

std::expected<void, StarException> ImageWidget::transform_drawables() {
  m_drawables = m_baseDrawables;

  if (m_rotation != 0)
    Drawable::rotate_all(m_drawables, m_rotation);

  auto metadata = context().root().image_metadata_database();
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  auto bounds = Drawable::bound_box_all(**metadata, m_drawables, m_trim);
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  if (m_centered && !bounds->is_null())
    Drawable::translate_all(m_drawables, -bounds->center());
  auto size = bounds->is_null() ? Vec2F() : bounds->size().piecewise_max({0, 0});
  if (size[0] && size[1]) {
    if ((size[0] > m_maxSize[0]) || (size[1] > m_maxSize[1])) {
      m_scale = std::min(m_maxSize[0] / size[0], m_maxSize[1] / size[1]);
    } else if ((size[0] < m_minSize[0]) || (size[1] < m_minSize[1])) {
      m_scale = std::min(m_minSize[0] / size[0], m_minSize[1] / size[1]);
    }
  }

  Drawable::scale_all(m_drawables, m_scale);

  setSize(Vec2I((size * m_scale).ceil()));
  return {};
}

}
