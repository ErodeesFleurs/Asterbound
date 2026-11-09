import std;
import star.format;
import star.json;
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
import star.version;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.either;

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
import star.stack_widget;

namespace Star {

std::expected<void, StarException> StackWidget::showPage(std::size_t page) {
  if (page >= m_members.size())
    return std::unexpected(GuiException("Stack page index out of range"));
  if (m_shownPage) {
    auto hidden = m_shownPage->hide();
    if (!hidden)
      return std::unexpected(std::move(hidden.error()));
  }
  m_shownPage = m_members[page];
  m_page = makeLeft(page);
  if (m_shownPage)
    return m_shownPage->show();
  return {};
}

std::expected<void, StarException> StackWidget::showPage(String const& name) {
  auto page = m_memberHash.find_ref(name);
  if (!page)
    return std::unexpected(GuiException::format("Unknown stack page: {}", name));
  if (m_shownPage) {
    auto hidden = m_shownPage->hide();
    if (!hidden)
      return std::unexpected(std::move(hidden.error()));
  }
  m_shownPage = page->get();
  m_page = makeRight(name);
  if (m_shownPage)
    return m_shownPage->show();
  return {};
}

Either<std::size_t, String> StackWidget::currentPage() const {
  return m_page;
}

std::expected<void, StarException> StackWidget::addChild(String const& name, std::shared_ptr<Widget> member) {
  auto added = Widget::addChild(name, member);
  if (!added)
    return std::unexpected(std::move(added.error()));
  if (m_members.size() != 1)
    return member->hide();
  return showPage(0);
}

}
