import std;
import star.format;
import star.json;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
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
import star.layout;
import star.flow_layout;

namespace Star {

FlowLayout::FlowLayout(GuiContext& context) : Layout(context), m_wrap(true) {}

std::expected<void, StarException> FlowLayout::update(float dt) {
  auto updated = Layout::update(dt);
  if (!updated)
    return std::unexpected(std::move(updated.error()));

  int consumedWidth = 0;
  int rowHeight = 0;
  Vec2I currentOffset = {0, size()[1]};
  for (auto child : m_members) {
    if (m_wrap && consumedWidth + child->size()[0] > size()[0] && consumedWidth != 0) { // wrapping
      currentOffset[0] = 0;
      consumedWidth = 0;
      currentOffset[1] -= rowHeight + m_spacing[1];
    }
    if (rowHeight < child->size()[1]) {
      rowHeight = child->size()[1];
    }
    child->setPosition(Vec2I{currentOffset[0], currentOffset[1] - child->size()[1]});
    consumedWidth += child->size()[0] + m_spacing[0];
    currentOffset[0] = consumedWidth;
  }
  return {};
}

void FlowLayout::setSpacing(Vec2I const& spacing) {
  m_spacing = spacing;
}

void FlowLayout::setWrapping(bool wrap) {
  m_wrap = wrap;
}

}
