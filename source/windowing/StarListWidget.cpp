import std;
import star.format;
import star.memory;
import star.json;
import star.json_extra;
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
import star.format;
import star.version;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.random;

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
import star.widget_parsing;
import star.gui_reader;
import star.list_widget;
import star.image_widget;

namespace Star {

ListWidget::ListWidget(GuiContext& context) : Widget(context), m_reader(context),
    m_selectedItem(NPos), m_fillDown(false), m_columns(1) {
  updateSizeAndPosition();
}

std::expected<std::shared_ptr<ListWidget>, StarException> ListWidget::create(GuiContext& context) {
  return std::shared_ptr<ListWidget>(new ListWidget(context));
}

std::expected<std::shared_ptr<ListWidget>, StarException> ListWidget::create(GuiContext& context, Json const& schema) {
  auto widget = std::shared_ptr<ListWidget>(new ListWidget(context));
  auto configured = widget->setSchema(schema);
  if (!configured)
    return std::unexpected(std::move(configured.error()));
  return widget;
}

RectI ListWidget::relativeBoundRect() const {
  if (m_fillDown)
    return RectI::withSize(relativePosition() - Vec2I(0, size()[1]), size());
  else
    return RectI::withSize(relativePosition(), size());
}

void ListWidget::setCallback(WidgetCallbackFunc callback) {
  m_callback = std::move(callback);
}

std::expected<bool, StarException> ListWidget::sendEvent(InputEvent const& event) {
  if (!m_visible)
    return false;
  for (std::size_t i = m_members.size(); i != 0; --i) {
    auto child = m_members[i - 1];
    auto handled = child->sendEvent(event);
    if (!handled)
      return std::unexpected(std::move(handled.error()));
    if (*handled || (std::holds_alternative<MouseButtonDownEvent>(event)
          && child->inMember(*context().mousePosition(event))
          && std::get<MouseButtonDownEvent>(event).mouseButton == MouseButton::Left)) {
      auto selected = setSelected(i - 1);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      return true;
    }
    auto hovered = setHovered(i - 1, std::holds_alternative<MouseMoveEvent>(event)
        && child->inMember(*context().mousePosition(event)));
    if (!hovered)
      return std::unexpected(std::move(hovered.error()));
  }
  return false;
}

std::expected<void, StarException> ListWidget::setSchema(Json const& schema) {
  auto cleared = clear();
  if (!cleared)
    return std::unexpected(std::move(cleared.error()));
  m_schema = schema;
  auto schemaError = [](StarException error) -> std::expected<void, StarException> {
    if (error.is<JsonException>())
      return std::unexpected(GuiException("Missing required value in map", ExceptionDiagnostic(error)));
    return std::unexpected(std::move(error));
  };
  for (auto entry : {std::pair{std::string_view("selectedBG"), std::ref(m_selectedBG)}, std::pair{std::string_view("unselectedBG"), std::ref(m_unselectedBG)},
           std::pair{std::string_view("hoverBG"), std::ref(m_hoverBG)}, std::pair{std::string_view("disabledBG"), std::ref(m_disabledBG)}}) {
    auto value = schema.getString(entry.first, "");
    if (!value)
      return schemaError(std::move(value.error()));
    entry.second.get() = std::move(*value);
  }
  if (m_disabledBG.empty() && !m_unselectedBG.empty()) {
    auto assets = context().root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto json = (*assets)->json("/interface.config:disabledButton");
    if (!json)
      return schemaError(std::move(json.error()));
    auto suffix = json->toString();
    if (!suffix)
      return schemaError(std::move(suffix.error()));
    m_disabledBG = m_unselectedBG + *suffix;
  }
  auto spacingJson = schema.get("spacing");
  if (!spacingJson)
    return schemaError(std::move(spacingJson.error()));
  auto spacing = jsonToVec2I(*spacingJson);
  if (!spacing)
    return schemaError(std::move(spacing.error()));
  auto sizeJson = schema.get("memberSize");
  if (!sizeJson)
    return schemaError(std::move(sizeJson.error()));
  auto size = jsonToVec2I(*sizeJson);
  if (!size)
    return schemaError(std::move(size.error()));
  m_spacing = *spacing;
  m_memberSize = *size;
  updateSizeAndPosition();
  return {};
}

std::expected<std::shared_ptr<Widget>, StarException> ListWidget::addItem() {
  auto item = constructWidget();
  if (!item)
    return std::unexpected(std::move(item.error()));
  return addItem(*item);
}

std::expected<std::shared_ptr<Widget>, StarException> ListWidget::addItem(std::size_t at) {
  auto item = constructWidget();
  if (!item)
    return std::unexpected(std::move(item.error()));
  auto added = addChildAt(Star::to_string(context().root().random().randu64()), *item, at);
  if (!added)
    return std::unexpected(std::move(added.error()));
  updateSizeAndPosition();
  if (m_selectedItem != NPos && at <= m_selectedItem) {
    auto selected = setSelected(m_selectedItem + 1);
    if (!selected)
      return std::unexpected(std::move(selected.error()));
  }
  return *item;
}

std::expected<std::shared_ptr<Widget>, StarException> ListWidget::addItem(std::shared_ptr<Widget> item) {
  auto added = addChild(Star::to_string(context().root().random().randu64()), item);
  if (!added)
    return std::unexpected(std::move(added.error()));
  updateSizeAndPosition();
  return item;
}

std::expected<std::shared_ptr<Widget>, StarException> ListWidget::constructWidget() {
  auto item = std::make_shared<Widget>(context());
  auto schema = m_schema.get("listTemplate");
  if (!schema)
    return std::unexpected(std::move(schema.error()));
  auto constructed = m_reader.construct(*schema, *item);
  if (!constructed)
    return std::unexpected(std::move(constructed.error()));
  item->setSize(m_memberSize);
  m_doScissor ? item->enableScissoring() : item->disableScissoring();
  return item;
}

void ListWidget::updateSizeAndPosition() {
  int rows = m_members.size() % m_columns ? m_members.size() / m_columns + 1 : m_members.size() / m_columns;
  for (std::size_t i = 0; i < m_members.size(); i++) {
    Vec2I currentOffset;
    int col = i % m_columns;
    int row = rows - (i / m_columns) - 1;
    if (m_fillDown)
      row -= rows;
    currentOffset = Vec2I((m_memberSize[0] + m_spacing[0]) * col, (m_memberSize[1] + m_spacing[1]) * row);
    if (!m_fillDown)
      currentOffset[1] += m_spacing[1];
    m_members[i]->setPosition(currentOffset);
  }
  if (m_members.size()) {
    auto width = (m_memberSize[0] + m_spacing[0]) * m_columns;
    auto height = (m_memberSize[1] + m_spacing[1]) * rows;
    setSize(Vec2I(width, height));
  } else {
    setSize(Vec2I());
  }
}

std::expected<void, StarException> ListWidget::setBackground(std::size_t pos, String const& image) {
  auto item = itemAt(pos);
  if (!item)
    return std::unexpected(GuiException("List item index out of range"));
  auto background = item->fetchChild<ImageWidget>("background");
  if (!background)
    return std::unexpected(std::move(background.error()));
  if (*background)
    return (*background)->setImage(image);
  return {};
}

std::expected<void, StarException> ListWidget::setEnabled(std::size_t pos, bool enabled) {
  if (pos != NPos && pos < listSize()) {
    if (enabled) {
      m_disabledItems.remove(pos);
      return setBackground(pos, pos == m_selectedItem ? m_selectedBG : m_unselectedBG);
    }
    m_disabledItems.add(pos);
    if (m_selectedItem == pos) {
      auto cleared = clearSelected();
      if (!cleared)
        return std::unexpected(std::move(cleared.error()));
    }
    return setBackground(pos, m_disabledBG);
  }
  return {};
}

std::expected<void, StarException> ListWidget::setHovered(std::size_t pos, bool hovered) {
  if (!m_hoverBG.empty() && pos != m_selectedItem && pos < listSize() && !m_disabledItems.contains(pos))
    return setBackground(pos, hovered ? m_hoverBG : m_unselectedBG);
  return {};
}

std::expected<void, StarException> ListWidget::setSelected(std::size_t pos) {
  if (pos != NPos && pos >= listSize())
    return std::unexpected(GuiException("List selection index out of range"));
  if (m_selectedItem != NPos && m_selectedItem < listSize()) {
    auto background = setBackground(m_selectedItem, m_unselectedBG);
    if (!background)
      return std::unexpected(std::move(background.error()));
  }
  if (!m_disabledItems.contains(pos) && m_selectedItem != pos) {
    m_selectedItem = pos;
    if (m_callback) {
      auto called = m_callback(*this);
      if (!called)
        return std::unexpected(std::move(called.error()));
    }
  }
  if (m_selectedItem != NPos)
    return setBackground(m_selectedItem, m_selectedBG);
  return {};
}

std::expected<void, StarException> ListWidget::clearSelected() {
  return setSelected(NPos);
}

std::expected<void, StarException> ListWidget::setSelectedWidget(std::shared_ptr<Widget> selected) {
  auto offset = itemPosition(selected);
  if (offset == NPos)
    return std::unexpected(GuiException("Attempted to select item not in list."));
  return setSelected(offset);
}

void ListWidget::registerMemberCallback(String const& name, WidgetCallbackFunc const& callback) {
  m_reader.registerCallback(name, callback);
}

void ListWidget::setFillDown(bool fillDown) {
  m_fillDown = fillDown;
}

std::expected<void, StarException> ListWidget::setColumns(std::uint64_t columns) {
  if (!columns)
    return std::unexpected(GuiException("List column count must be positive"));
  m_columns = columns;
  return {};
}

std::expected<void, StarException> ListWidget::removeItem(std::size_t at) {
  if (at >= m_members.size())
    return std::unexpected(GuiException("List removal index out of range"));
  removeChildAt(at);
  if (m_selectedItem == at) {
    auto selected = setSelected(NPos);
    if (!selected)
      return std::unexpected(std::move(selected.error()));
  } else if (m_selectedItem != NPos && m_selectedItem > at) {
    auto selected = setSelected(m_selectedItem - 1);
    if (!selected)
      return std::unexpected(std::move(selected.error()));
  }
  updateSizeAndPosition();
  return {};
}

std::expected<void, StarException> ListWidget::removeItem(std::shared_ptr<Widget> item) {
  auto offset = itemPosition(item);
  if (offset == NPos)
    return std::unexpected(GuiException("Attempted to remove item not in list."));
  return removeItem(offset);
}

std::expected<void, StarException> ListWidget::clear() {
  auto selected = setSelected(NPos);
  if (!selected)
    return std::unexpected(std::move(selected.error()));
  removeAllChildren();
  updateSizeAndPosition();
  return {};
}

std::size_t ListWidget::selectedItem() const {
  return m_selectedItem;
}

std::size_t ListWidget::itemPosition(std::shared_ptr<Widget> item) const {
  std::size_t offset = NPos;
  for (std::size_t i = 0; i < m_members.size(); ++i) {
    if (m_members[i] == item) {
      offset = i;
      break;
    }
  }
  return offset;
}

std::shared_ptr<Widget> ListWidget::itemAt(std::size_t n) const {
  if (n < m_members.size()) {
    return m_members[n];
  } else {
    return {};
  }
}

std::shared_ptr<Widget> ListWidget::selectedWidget() const {
  return itemAt(m_selectedItem);
}

List<std::shared_ptr<Widget>> const& ListWidget::list() const {
  return m_members;
}

std::size_t ListWidget::listSize() const {
  return numChildren();
}

}
