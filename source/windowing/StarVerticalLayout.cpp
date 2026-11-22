import std;
import star.format;
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
import star.layout;
import star.vertical_layout;

import star.algorithm;
namespace Star {

VerticalLayout::VerticalLayout(GuiContext& context) : VerticalLayout(context, VerticalAnchor::TopAnchor, 0) {}

VerticalLayout::VerticalLayout(GuiContext& context, VerticalAnchor verticalAnchor, int verticalSpacing) : Layout(context) {
  setVerticalAnchor(verticalAnchor);
  setVerticalSpacing(verticalSpacing);

  disableScissoring();
}

std::expected<void, StarException> VerticalLayout::update(float) {
  updateLayout();
  return {};
}

void VerticalLayout::updateLayout() {
  m_size = Vec2I(0, 0);

  if (m_members.empty())
    return;

  for (auto const& child : m_members) {
    auto childSize = child->size();
    m_size[1] += childSize[1];
    m_size[0] = std::max(m_size[0], childSize[0]);
  }
  m_size[1] += (m_members.size() - 1) * m_verticalSpacing;

  auto bounds = contentBoundRect();

  int verticalPos = m_fillDown ? bounds.y_max() : bounds.y_min();
  for (auto const& child : std::views::reverse(m_members)) {
    auto childSize = child->size();

    Vec2I targetPosition;

    if (m_horizontalAnchor == HorizontalAnchor::LeftAnchor)
      targetPosition[0] = bounds.x_min();
    else if (m_horizontalAnchor == HorizontalAnchor::RightAnchor)
      targetPosition[0] = bounds.x_max() - childSize[0];
    else if (m_horizontalAnchor == HorizontalAnchor::HMidAnchor)
      targetPosition[0] = -childSize[0] / 2;

    if (m_fillDown) {
      verticalPos -= childSize[1];
      targetPosition[1] = verticalPos;
      verticalPos -= m_verticalSpacing;
    } else {
      targetPosition[1] = verticalPos;
      verticalPos -= childSize[1];
      verticalPos -= m_verticalSpacing;
    }

    // needed because position is included in relativeBoundRect
    child->setPosition(Vec2I(0, 0));

    child->setPosition(targetPosition - child->relativeBoundRect().min());
  }
}

Vec2I VerticalLayout::size() const {
  return m_size;
}

RectI VerticalLayout::relativeBoundRect() const {
  return contentBoundRect().translated(relativePosition());
}

void VerticalLayout::setHorizontalAnchor(HorizontalAnchor horizontalAnchor) {
  m_horizontalAnchor = horizontalAnchor;
  updateLayout();
}

void VerticalLayout::setVerticalAnchor(VerticalAnchor verticalAnchor) {
  m_verticalAnchor = verticalAnchor;
  updateLayout();
}

void VerticalLayout::setVerticalSpacing(int verticalSpacing) {
  m_verticalSpacing = verticalSpacing;
  updateLayout();
}

void VerticalLayout::setFillDown(bool fillDown) {
  m_fillDown = fillDown;
  updateLayout();
}

RectI VerticalLayout::contentBoundRect() const {
  auto min = Vec2I(0, 0);

  if (m_horizontalAnchor == HorizontalAnchor::RightAnchor)
    min[0] -= m_size[0];
  else if (m_horizontalAnchor == HorizontalAnchor::HMidAnchor)
    min[0] -= m_size[0] / 2;

  if (m_verticalAnchor == VerticalAnchor::TopAnchor)
    min[1] -= m_size[1];
  else if (m_verticalAnchor == VerticalAnchor::VMidAnchor)
    min[1] -= m_size[1] / 2;

  return RectI::with_size(min, m_size);
}

}
