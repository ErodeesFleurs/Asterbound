import std;
import star.json;
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
import star.image_stretch_widget;

import star.format;
namespace Star {

std::expected<std::shared_ptr<ImageStretchWidget>, StarException> ImageStretchWidget::create(GuiContext& context, ImageStretchSet const& imageStretchSet, GuiDirection direction) {
  return std::shared_ptr<ImageStretchWidget>(new ImageStretchWidget(context, imageStretchSet, direction));
}

ImageStretchWidget::ImageStretchWidget(GuiContext& context, ImageStretchSet const& imageStretchSet, GuiDirection direction)
  : Widget(context), m_imageStretchSet(imageStretchSet), m_direction(direction) {

}

void ImageStretchWidget::setImageStretchSet(String const& beginImage, String const& innerImage, String const& endImage) {
  m_imageStretchSet.begin = beginImage;
  m_imageStretchSet.inner = innerImage;
  m_imageStretchSet.end = endImage;
}

std::expected<void, StarException> ImageStretchWidget::renderImpl() {
  return context().drawImageStretchSet(m_imageStretchSet, RectF(screenBoundRect()), m_direction);
}

}
