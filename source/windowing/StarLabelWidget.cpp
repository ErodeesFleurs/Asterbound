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
import star.version;
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
import star.label_widget;

import star.format;
namespace Star {

LabelWidget::LabelWidget(GuiContext& context, HorizontalAnchor hAnchor, VerticalAnchor vAnchor, std::optional<unsigned> wrapWidth)
  : Widget(context), m_hAnchor(hAnchor), m_vAnchor(vAnchor), m_wrapWidth(wrapWidth) {}

std::expected<std::shared_ptr<LabelWidget>, StarException> LabelWidget::create(GuiContext& context, String text,
    Color const& color, HorizontalAnchor const& hAnchor, VerticalAnchor const& vAnchor,
    std::optional<unsigned> wrapWidth, std::optional<float> lineSpacing) {
  auto widget = std::shared_ptr<LabelWidget>(new LabelWidget(context, hAnchor, vAnchor, wrapWidth));
  auto assets = context.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto json = (*assets)->json("/interface.config:labelTextStyle");
  if (!json) return std::unexpected(std::move(json.error()));
  auto style = TextStyle::create(*json);
  if (!style) return std::unexpected(std::move(style.error()));
  widget->m_style = std::move(*style);
  widget->m_style.color = color.to_rgba();
  if (lineSpacing) widget->m_style.line_spacing = *lineSpacing;
  auto result = widget->setText(std::move(text));
  if (!result) return std::unexpected(std::move(result.error()));
  return widget;
}

String const& LabelWidget::text() const {
  return m_text;
}

std::optional<unsigned> LabelWidget::getTextCharLimit() const {
  return m_textCharLimit;
}

std::expected<void, StarException> LabelWidget::setText(String newText) {
  m_text = std::move(newText);
  return updateTextRegion();
}

std::expected<void, StarException> LabelWidget::setFontSize(int fontSize) {
  m_style.font_size = fontSize;
  return updateTextRegion();
}

void LabelWidget::setFontMode(FontMode fontMode) {
  m_style.shadow = fontModeToColor(fontMode).to_rgba();
}

void LabelWidget::setColor(Color newColor) {
  m_style.color = newColor.to_rgba();
}

std::expected<void, StarException> LabelWidget::setFont(String const& font) {
  m_style.font = font;
  return updateTextRegion();
}

std::expected<void, StarException> LabelWidget::setAnchor(HorizontalAnchor hAnchor, VerticalAnchor vAnchor) {
  m_hAnchor = hAnchor;
  m_vAnchor = vAnchor;
  return updateTextRegion();
}

std::expected<void, StarException> LabelWidget::setWrapWidth(std::optional<unsigned> wrapWidth) {
  m_wrapWidth = wrapWidth;
  return updateTextRegion();
}

std::expected<void, StarException> LabelWidget::setLineSpacing(std::optional<float> lineSpacing) {
  m_style.line_spacing = lineSpacing.value_or(default_line_spacing);
  return updateTextRegion();
}

std::expected<void, StarException> LabelWidget::setDirectives(String const& directives) {
  m_style.directives = directives;
  return updateTextRegion();
}

std::expected<void, StarException> LabelWidget::setTextCharLimit(std::optional<unsigned> charLimit) {
  m_textCharLimit = charLimit;
  return updateTextRegion();
}

std::expected<void, StarException> LabelWidget::setTextStyle(TextStyle const& textStyle) {
  m_style = textStyle;
  return updateTextRegion();
}


RectI LabelWidget::relativeBoundRect() const {
  return RectI(m_textRegion).translated(relativePosition());
}

RectI LabelWidget::getScissorRect() const {
  return noScissor();
}

std::expected<void, StarException> LabelWidget::renderImpl() {
  auto style = context().setTextStyle(m_style);
  if (!style) return std::unexpected(std::move(style.error()));
  auto result = context().renderInterfaceText(m_text, {Vec2F(screenPosition()), m_hAnchor, m_vAnchor, m_wrapWidth, m_textCharLimit});
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> LabelWidget::updateTextRegion() {
  auto style = context().setTextStyle(m_style);
  if (!style) return std::unexpected(std::move(style.error()));
  auto region = context().determineInterfaceTextSize(m_text, {Vec2F(), m_hAnchor, m_vAnchor, m_wrapWidth, m_textCharLimit});
  if (!region) return std::unexpected(std::move(region.error()));
  m_textRegion = RectI(*region);
  setSize(m_textRegion.size());
  return {};
}

}
