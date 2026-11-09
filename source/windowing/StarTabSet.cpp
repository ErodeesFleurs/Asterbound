import std;
import star.format;
import star.memory;
import star.json;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.format;
import star.version;
import star.string;
import star.text;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.either;
import star.lexical_cast;

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
import star.button_group;
import star.tab_set;
import star.button_widget;
import star.stack_widget;
import star.layout;
import star.flow_layout;
import star.widget_parsing;
import star.gui_reader;


import star.image_metadata_database;

namespace Star {

TabSetWidget::TabSetWidget(GuiContext& context, TabSetConfig const& config)
  : Widget(context), m_tabSetConfig(config) {}

std::expected<std::shared_ptr<TabSetWidget>, StarException> TabSetWidget::create(GuiContext& context, TabSetConfig const& config) {
  auto tabs = std::shared_ptr<TabSetWidget>(new TabSetWidget(context, config));
  auto metadata = context.root().imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  for (auto image : {std::cref(config.tabButtonBaseImage), std::cref(config.tabButtonHoverImage), std::cref(config.tabButtonPressedImage),
           std::cref(config.tabButtonBaseImageSelected), std::cref(config.tabButtonHoverImageSelected), std::cref(config.tabButtonPressedImageSelected)}) {
    auto size = (*metadata)->imageSize(image.get());
    if (!size)
      return std::unexpected(std::move(size.error()));
    tabs->m_tabHeight = std::max<int>(tabs->m_tabHeight, size->y());
  }
  tabs->m_tabBar = std::make_shared<FlowLayout>(context);
  tabs->m_tabBar->setSpacing(config.tabButtonSpacing);
  auto bar = tabs->Widget::addChild("tabBar", tabs->m_tabBar);
  if (!bar)
    return std::unexpected(std::move(bar.error()));
  tabs->m_stack = std::make_shared<StackWidget>(context);
  auto stack = tabs->addChild("tabs", tabs->m_stack);
  if (!stack)
    return std::unexpected(std::move(stack.error()));
  tabs->markAsContainer();
  return tabs;
}

void TabSetWidget::setSize(Vec2I const& size) {
  auto tabHeight = m_tabHeight;
  Widget::setSize(Vec2I(size.x(), std::max<int>(size.y(), tabHeight)));
  m_tabBar->setSize({size.x(), tabHeight});
  m_tabBar->setPosition({0, size.y() - tabHeight});
  m_stack->setSize({size.x(), size.y() - tabHeight});
}

std::expected<void, StarException> TabSetWidget::addTab(String const& widgetName, std::shared_ptr<Widget> widget, String const& title) {
  auto created = ButtonWidget::create(context());
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto button = *created;
  auto images = button->setImages(m_tabSetConfig.tabButtonBaseImage,
      m_tabSetConfig.tabButtonHoverImage, m_tabSetConfig.tabButtonPressedImage);
  if (!images)
    return std::unexpected(std::move(images.error()));
  auto checked = button->setCheckedImages(m_tabSetConfig.tabButtonBaseImageSelected,
      m_tabSetConfig.tabButtonHoverImageSelected, m_tabSetConfig.tabButtonPressedImageSelected);
  if (!checked)
    return std::unexpected(std::move(checked.error()));
  button->setCheckable(true);
  button->setText(title);
  button->setTextOffset(m_tabSetConfig.tabButtonTextOffset);
  button->setPressedOffset(m_tabSetConfig.tabButtonPressedOffset);
  auto page = m_tabBar->numChildren();
  button->setCallback([owner = std::weak_ptr<TabSetWidget>(as<TabSetWidget>(shared_from_this())), page](Widget&) -> std::expected<void, StarException> {
    auto tabs = owner.lock();
    if (!tabs) return std::unexpected(GuiException("Tab set callback target no longer exists"));
    return tabs->tabSelect(page);
  });
  auto addedButton = m_tabBar->addChild(Star::to_string(page), button);
  if (!addedButton)
    return std::unexpected(std::move(addedButton.error()));
  auto addedPage = m_stack->addChild(widgetName, widget);
  if (!addedPage)
    return std::unexpected(std::move(addedPage.error()));
  if (!m_lastSelected)
    return tabSelect(0);
  return {};
}

std::size_t TabSetWidget::tabCount() const {
  return m_tabBar->numChildren();
}

std::expected<void, StarException> TabSetWidget::tabSelect(std::size_t page) {
  if (page >= tabCount())
    return std::unexpected(GuiException("Tab index out of range"));
  if (m_lastSelected != page) {
    auto shown = m_stack->showPage(page);
    if (!shown)
      return std::unexpected(std::move(shown.error()));
    m_lastSelected = page;
    for (std::size_t i = 0; i < m_tabBar->numChildren(); ++i) {
      auto button = m_tabBar->getChildNum<ButtonWidget>(i);
      if (!button)
        return std::unexpected(std::move(button.error()));
      (*button)->setChecked(i == page);
    }
    if (m_callback)
      return m_callback(*this);
  } else {
    auto button = m_tabBar->getChildNum<ButtonWidget>(page);
    if (!button)
      return std::unexpected(std::move(button.error()));
    (*button)->setChecked(true);
  }
  return {};
}

std::size_t TabSetWidget::selectedTab() const {
  return m_lastSelected.value_or(NPos);
}

void TabSetWidget::setCallback(WidgetCallbackFunc callback) {
  m_callback = std::move(callback);
}

}
