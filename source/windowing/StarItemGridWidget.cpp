import std;
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
import star.strong_typedef;

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
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
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
import star.progress_widget;
import star.animation;
import star.item_slot_widget;
import star.item_grid_widget;

import star.item_bag;

import star.format;
namespace Star {

std::expected<std::shared_ptr<ItemGridWidget>, StarException> ItemGridWidget::create(GuiContext& context, std::shared_ptr<ItemBag const> bag, Vec2I const& dimensions, Vec2I const& spacing, String const& backingImage, unsigned bagOffset) {
  return create(context, std::move(bag), dimensions, {spacing[0], 0}, {0, spacing[1]}, backingImage, bagOffset);
}

std::expected<std::shared_ptr<ItemGridWidget>, StarException> ItemGridWidget::create(GuiContext& context, std::shared_ptr<ItemBag const> bag, Vec2I const& dimensions, Vec2I const& rowSpacing, Vec2I const& columnSpacing, String const& backingImage, unsigned bagOffset) {
  auto widget = std::shared_ptr<ItemGridWidget>(new ItemGridWidget(context, dimensions, rowSpacing, columnSpacing, backingImage, bagOffset));
  auto assets = context.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto areaJson = (*assets)->json("/interface.config:itemDraggableArea");
  if (!areaJson) return std::unexpected(std::move(areaJson.error()));
  auto area = jsonToRectI(*areaJson);
  if (!area) return std::unexpected(std::move(area.error()));
  widget->m_itemDraggableArea = *area;
  auto durabilityJson = (*assets)->json("/interface.config:itemIconDurabilityOffset");
  if (!durabilityJson) return std::unexpected(std::move(durabilityJson.error()));
  auto durabilityOffset = jsonToVec2I(*durabilityJson);
  if (!durabilityOffset) return std::unexpected(std::move(durabilityOffset.error()));
  auto anchorJson = (*assets)->json("/interface.config:itemCountRightAnchor");
  if (!anchorJson) return std::unexpected(std::move(anchorJson.error()));
  auto anchor = jsonToVec2I(*anchorJson);
  if (!anchor) return std::unexpected(std::move(anchor.error()));
  widget->m_scissorExtra = (*durabilityOffset * -1).piecewiseMax(*anchor * -1).piecewiseMax(Vec2I());
  Vec2I calculatedSize = {
    dimensions[0] * rowSpacing[0] + dimensions[1] * columnSpacing[0],
    dimensions[0] * rowSpacing[1] + dimensions[1] * columnSpacing[1]
  };
  widget->setSize(calculatedSize.piecewiseMax(area->size()));
  auto result = widget->setItemBag(std::move(bag));
  if (!result) return std::unexpected(std::move(result.error()));
  return widget;
}

ItemGridWidget::ItemGridWidget(GuiContext& context, Vec2I const& dimensions, Vec2I const& rowSpacing, Vec2I const& columnSpacing, String const& backingImage, unsigned bagOffset)
  : Widget(context), m_bagOffset(bagOffset), m_dimensions(dimensions), m_rowSpacing(rowSpacing), m_columnSpacing(columnSpacing), m_backingImage(backingImage) {
  m_selectedIndex = 0;
  m_progress = 1;
  m_drawBackingImageWhenFull = false;
  m_drawBackingImageWhenEmpty = true;
  m_showDurability = false;
  m_highlightEmpty = false;
  disableScissoring();
  markAsContainer();
}

std::shared_ptr<ItemBag const> ItemGridWidget::bag() const {
  return m_bag;
}

std::expected<std::shared_ptr<Item>, StarException> ItemGridWidget::itemAt(Vec2I const& position) const {
  auto pos = bagLocationAt(position);
  if (pos != NPos && m_bag)
    return m_bag->at(pos);

  return std::shared_ptr<Item>{};
}

std::expected<std::shared_ptr<Item>, StarException> ItemGridWidget::itemAt(std::size_t index) const {
  if (m_bag && index < m_bag->size())
    return m_bag->at(index);
  return std::shared_ptr<Item>{};
}

std::expected<std::shared_ptr<Item>, StarException> ItemGridWidget::selectedItem() const {
  return itemAt(selectedIndex());
}

std::expected<std::shared_ptr<ItemSlotWidget>, StarException> ItemGridWidget::itemWidgetAt(Vec2I const& position) const {
  auto pos = bagLocationAt(position);
  if (pos == NPos) return std::shared_ptr<ItemSlotWidget>{};
  auto slot = m_slots.at(pos);
  if (!slot) return std::unexpected(std::move(slot.error()));
  return slot->get();
}

std::shared_ptr<ItemSlotWidget> ItemGridWidget::itemWidgetAt(std::size_t index) const {
  if (index < m_slots.size())
    return m_slots[index];
  return {};
}

Vec2I ItemGridWidget::dimensions() const {
  return m_dimensions;
}

std::size_t ItemGridWidget::itemSlots() const {
  return m_dimensions.x() * m_dimensions.y();
}

std::size_t ItemGridWidget::bagSize() const {
  if (!m_bag)
    return 0;

  return m_bag->size();
}

std::size_t ItemGridWidget::effectiveSize() const {
  return std::min(itemSlots(), bagSize());
}

std::size_t ItemGridWidget::bagLocationAt(Vec2I const& position) const {
  if (m_bag) {
    for (std::size_t i = 0;
         i < (m_bag->size() - m_bagOffset) && i < static_cast<unsigned>(m_dimensions[0] * m_dimensions[1]); ++i) {
      Vec2I loc = locOfItemSlot(i);
      RectI bagItemArea = RectI(m_itemDraggableArea).translated(screenPosition() + loc);
      if (bagItemArea.contains(position))
        return i + m_bagOffset;
    }
  }

  return NPos;
}

std::expected<Vec2I, StarException> ItemGridWidget::positionOfSlot(std::size_t slotNumber) {
  auto slot = m_slots.at(slotNumber);
  if (!slot) return std::unexpected(std::move(slot.error()));
  return (slot->get())->position() + position();
}

std::expected<bool, StarException> ItemGridWidget::sendEvent(InputEvent const& event) {
  if (m_visible) {
    if (std::holds_alternative<MouseButtonDownEvent>(event)) {
      auto const& mouseButton = std::get<MouseButtonDownEvent>(event);
      if (mouseButton.mouseButton == MouseButton::Left
        || (m_rightClickCallback && mouseButton.mouseButton == MouseButton::Right)
        || (m_middleClickCallback && mouseButton.mouseButton == MouseButton::Middle)) {
        Vec2I mousePos = *context().mousePosition(event);
        for (std::size_t i = 0;
             i < (m_bag->size() - m_bagOffset) && i < static_cast<unsigned>(m_dimensions[0] * m_dimensions[1]); ++i) {
          Vec2I loc = locOfItemSlot(i);
          RectI bagItemArea = RectI(m_itemDraggableArea).translated(screenPosition() + loc);

          if (bagItemArea.contains(mousePos)) {
            m_selectedIndex = i;
            auto const& callback = mouseButton.mouseButton == MouseButton::Right ? m_rightClickCallback
              : mouseButton.mouseButton == MouseButton::Middle ? m_middleClickCallback : m_callback;
            if (!callback) return std::unexpected(GuiException("Item grid has no click callback"));
            auto result = callback(*this);
            if (!result) return std::unexpected(std::move(result.error()));
            return true;
          }
        }
      }
    }
  }
  return false;
}

Vec2I ItemGridWidget::locOfItemSlot(unsigned slot) const {
  Vec2I loc = {
    static_cast<int>(slot) % static_cast<int>(m_dimensions[0]) * m_rowSpacing[0] +    // x contribution from row
      static_cast<int>(slot) / static_cast<int>(m_dimensions[0]) * m_columnSpacing[0],// x contribution from column
    (m_dimensions[0] - 1) * m_rowSpacing[1]
      - static_cast<int>(slot) % static_cast<int>(m_dimensions[0]) * m_rowSpacing[1] +// y contribution from row
      (m_dimensions[1] - 1) * m_columnSpacing[1]
      - static_cast<int>(slot) / static_cast<int>(m_dimensions[0]) * m_columnSpacing[1]// y contribution from column
  };
  return loc;
}

void ItemGridWidget::setCallback(WidgetCallbackFunc callback) { m_callback = std::move(callback); }

void ItemGridWidget::setRightClickCallback(WidgetCallbackFunc callback) { m_rightClickCallback = std::move(callback); }

void ItemGridWidget::setMiddleClickCallback(WidgetCallbackFunc callback) {
  m_middleClickCallback = std::move(callback);
}

std::expected<void, StarException> ItemGridWidget::setItemBag(std::shared_ptr<ItemBag const> bag) {
  m_bag = std::move(bag);
  if (!m_bag) return {};
  removeAllChildren();
  m_slots.clear();
  for (std::size_t i = 0;
       i < m_bag->size() - m_bagOffset && i < static_cast<unsigned>(m_dimensions[0]) * m_dimensions[1]; ++i) {
    auto item = m_bag->at(i);
    if (!item) return std::unexpected(std::move(item.error()));
    auto slot = ItemSlotWidget::create(context(), *item, m_backingImage);
    if (!slot) return std::unexpected(std::move(slot.error()));
    auto child = addChild(toString(i), *slot);
    if (!child) return std::unexpected(std::move(child.error()));
    m_slots.push_back(*slot);
    (*slot)->setBackingImageAffinity(m_drawBackingImageWhenFull, m_drawBackingImageWhenEmpty);
    (*slot)->setProgress(m_progress);
    (*slot)->setPosition(locOfItemSlot(i));
    (*slot)->showDurability(m_showDurability);
  }
  m_itemNames = slotItemNames();
  return {};
}

std::expected<void, StarException> ItemGridWidget::setProgress(float progress) {
  m_progress = progress;
  if (!m_bag) return {};
  for (std::size_t i = 0;
       i < m_bag->size() - m_bagOffset && i < static_cast<unsigned>(m_dimensions[0]) * m_dimensions[1]; ++i) {
    auto slot = m_slots.at(i);
    if (!slot) return std::unexpected(std::move(slot.error()));
    (slot->get())->setProgress(m_progress);
  }
  return {};
}

std::size_t ItemGridWidget::selectedIndex() const {
  return m_selectedIndex + m_bagOffset;
}

std::expected<void, StarException> ItemGridWidget::updateAllItemSlots() {
  if (!m_bag) return {};
  for (std::size_t i = 0;
       i < m_bag->size() - m_bagOffset && i < static_cast<unsigned>(m_dimensions[0]) * m_dimensions[1]; ++i) {
    auto item = m_bag->at(i + m_bagOffset);
    if (!item) return std::unexpected(std::move(item.error()));
    auto slot = m_slots.at(i);
    if (!slot) return std::unexpected(std::move(slot.error()));
    (slot->get())->setItem(*item);
    (slot->get())->setHighlightEnabled(!*item && m_highlightEmpty);
  }
  return {};
}

std::expected<void, StarException> ItemGridWidget::updateItemState() {
  auto result = updateAllItemSlots();
  if (!result) return std::unexpected(std::move(result.error()));
  auto newState = slotItemNames();
  for (std::size_t i = 0; i < newState.size(); ++i) {
    if (newState[i].empty())
      m_changedSlots.remove(i);
    else if (newState[i] != m_itemNames[i])
      m_changedSlots.insert(i);
  }
  m_itemNames = std::move(newState);
  return {};
}

void ItemGridWidget::indicateChangedSlots() {
  for (auto i : m_changedSlots)
    m_slots[i]->indicateNew();
}

void ItemGridWidget::setHighlightEmpty(bool highlight) {
  m_highlightEmpty = highlight;
}

void ItemGridWidget::clearChangedSlots() {
  m_changedSlots.clear();
}

bool ItemGridWidget::slotsChanged() {
  return m_changedSlots.size() > 0;
}

HashSet<ItemDescriptor> ItemGridWidget::uniqueItemState() {
  HashSet<ItemDescriptor> state;
  if (!m_bag)
    return state;
  for (auto const& item : m_bag->items()) {
    if (item)
      state.add(item->descriptor().singular());
  }
  return state;
}

List<String> ItemGridWidget::slotItemNames() {
  List<String> itemNames;
  for (auto const& slot : m_slots) {
    if (slot->item())
      itemNames.push_back(slot->item()->name());
    else
      itemNames.push_back(String());
  }
  return itemNames;
}

std::expected<void, StarException> ItemGridWidget::setBackingImageAffinity(bool full, bool empty) {
  m_drawBackingImageWhenFull = full;
  m_drawBackingImageWhenEmpty = empty;
  if (!m_bag) return {};
  for (std::size_t i = 0;
       i < m_bag->size() - m_bagOffset && i < static_cast<unsigned>(m_dimensions[0]) * m_dimensions[1]; ++i) {
    auto slot = m_slots.at(i);
    if (!slot) return std::unexpected(std::move(slot.error()));
    (slot->get())->setBackingImageAffinity(m_drawBackingImageWhenFull, m_drawBackingImageWhenEmpty);
  }
  return {};
}

std::expected<void, StarException> ItemGridWidget::showDurability(bool show) {
  m_showDurability = show;
  if (!m_bag) return {};
  for (std::size_t i = 0;
       i < m_bag->size() - m_bagOffset && i < static_cast<unsigned>(m_dimensions[0]) * m_dimensions[1]; ++i) {
    auto slot = m_slots.at(i);
    if (!slot) return std::unexpected(std::move(slot.error()));
    (slot->get())->showDurability(m_showDurability);
  }
  return {};
}

RectI ItemGridWidget::getScissorRect() const {
  return RectI::withSize(screenPosition() - m_scissorExtra, size() + m_scissorExtra);
}

std::expected<void, StarException> ItemGridWidget::renderImpl() {
  return updateAllItemSlots();
}

}
