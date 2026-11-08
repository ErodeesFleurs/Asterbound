import std;
import star.json;
import star.json_extra;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.string;
import star.color;
import star.vector;
import star.casting;
import star.input_event;
import star.font;
import star.bi_map;
import star.text;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.format;
import star.game_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.version;
import star.strong_typedef;

import star.drawable;
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
import star.widget_parsing;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.durability_item;


import star.image_metadata_database;

import star.format;
namespace Star {

static String formatShortSize(std::uint64_t n) {
    if (n < 10000)
      return toString(n);

    std::uint64_t divisor = 1000ull;
    char suffix = 'k';

    if (n >= 1000000000000000000ull) {
      divisor = 1000000000000000000ull;
      suffix = 'Q';
    } else if (n >= 1000000000000000ull) {
      divisor = 1000000000000000ull;
      suffix = 'q';
    } else if (n >= 1000000000000ull) {
      divisor = 1000000000000ull;
      suffix = 't';
    } else if (n >= 1000000000ull) {
      divisor = 1000000000ull;
      suffix = 'b';
    } else if (n >= 1000000ull) {
      divisor = 1000000ull;
      suffix = 'm';
    }

    std::uint64_t whole = n / divisor;
    if (whole >= 100ull)
        return strf("{}{:c}", whole, suffix);

    std::uint64_t remainder = n - (whole * divisor);
    std::uint64_t frac = (remainder / (divisor / 1000));

    if (frac == 0)
      return strf("{}{:c}", whole, suffix);
    else if (whole >= 10)
      return strf("{}.{}{:c}", whole, frac / 100, suffix);
    else
      return strf("{}.{:02d}{:c}", whole, frac / 10, suffix);
}

ItemSlotWidget::ItemSlotWidget(GuiContext& context, std::shared_ptr<Item> const& item, String const& backingImage, Animation newItemIndicator, Animation highlightAnimation)
  : Widget(context), m_item(item), m_backingImage(backingImage), m_newItemIndicator(std::move(newItemIndicator)), m_highlightAnimation(std::move(highlightAnimation)) {
  m_drawBackingImageWhenFull = false;
  m_drawBackingImageWhenEmpty = true;
  m_progress = 1;
  m_countFontMode = FontMode::Normal;
  m_highlightEnabled = false;
  m_showDurability = false;
  m_showCount = true;
  m_showRarity = true;
  m_showLinkIndicator = false;
  m_showSecondaryIcon = false;
  disableScissoring();
}

std::expected<std::shared_ptr<ItemSlotWidget>, StarException> ItemSlotWidget::create(GuiContext& context, std::shared_ptr<Item> const& item, String const& backingImage) {
  auto assets = context.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface.config");
  if (!config) return std::unexpected(std::move(config.error()));
  auto newConfig = config->get("newItemAnimation");
  if (!newConfig) return std::unexpected(std::move(newConfig.error()));
  auto indicator = Animation::create(context.root(), *newConfig);
  if (!indicator) return std::unexpected(std::move(indicator.error()));
  auto cycle = newConfig->getDouble("animationCycle");
  if (!cycle) return std::unexpected(std::move(cycle.error()));
  auto loops = newConfig->getDouble("loops", 1.0f);
  if (!loops) return std::unexpected(std::move(loops.error()));
  indicator->update(*cycle * *loops);
  auto highlightConfig = config->get("highlightAnimation");
  if (!highlightConfig) return std::unexpected(std::move(highlightConfig.error()));
  auto highlight = Animation::create(context.root(), *highlightConfig);
  if (!highlight) return std::unexpected(std::move(highlight.error()));
  auto widget = std::shared_ptr<ItemSlotWidget>(new ItemSlotWidget(context, item, backingImage, std::move(*indicator), std::move(*highlight)));
  auto countJson = config->get("itemCountRightAnchor");
  if (!countJson) return std::unexpected(std::move(countJson.error()));
  auto count = jsonToVec2F(*countJson);
  if (!count) return std::unexpected(std::move(count.error()));
  widget->m_countPosition = TextPositioning(*count, HorizontalAnchor::RightAnchor);
  auto styleJson = config->get("itemSlotTextStyle");
  if (!styleJson) return std::unexpected(std::move(styleJson.error()));
  auto style = TextStyle::create(*styleJson);
  if (!style) return std::unexpected(std::move(style.error()));
  widget->m_textStyle = std::move(*style);
  auto areaJson = config->get("itemDraggableArea");
  if (!areaJson) return std::unexpected(std::move(areaJson.error()));
  auto area = jsonToRectI(*areaJson);
  if (!area) return std::unexpected(std::move(area.error()));
  widget->m_itemDraggableArea = *area;
  auto offsetJson = config->get("itemIconDurabilityOffset");
  if (!offsetJson) return std::unexpected(std::move(offsetJson.error()));
  auto offset = jsonToVec2I(*offsetJson);
  if (!offset) return std::unexpected(std::move(offset.error()));
  widget->m_durabilityOffset = *offset;
  Vec2I backingSize;
  if (!backingImage.empty()) {
    auto metadata = context.root().imageMetadataDatabase();
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    auto size = (*metadata)->imageSize(backingImage);
    if (!size) return std::unexpected(std::move(size.error()));
    backingSize = Vec2I(*size);
  }
  widget->setSize(area->max().piecewise_max(backingSize));
  auto slotConfig = (*assets)->json("/interface/itemSlot.config");
  if (!slotConfig) return std::unexpected(std::move(slotConfig.error()));
  auto children = slotConfig->get("config");
  if (!children) return std::unexpected(std::move(children.error()));
  WidgetParser parser(context);
  auto parsed = parser.construct(*children, *widget);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  auto bar = widget->requireChild<ProgressWidget>("durabilityBar");
  if (!bar) return std::unexpected(std::move(bar.error()));
  widget->m_durabilityBar = *bar;
  auto hidden = widget->m_durabilityBar->hide();
  if (!hidden) return std::unexpected(std::move(hidden.error()));
  return widget;
}

std::expected<void, StarException> ItemSlotWidget::update(float dt) {
  if (m_item)
    m_newItemIndicator.update(dt);
  if (m_highlightEnabled)
    m_highlightAnimation.update(dt);
  return Widget::update(dt);
}

std::expected<bool, StarException> ItemSlotWidget::sendEvent(InputEvent const& event) {
  if (m_visible) {
    if (std::holds_alternative<MouseButtonDownEvent>(event)) {
      auto const& mouseButton = std::get<MouseButtonDownEvent>(event);
      if (mouseButton.mouseButton == MouseButton::Left
        || (m_rightClickCallback && mouseButton.mouseButton == MouseButton::Right)
        || (m_middleClickCallback && mouseButton.mouseButton == MouseButton::Middle)) {
        Vec2I mousePos = *context().mousePosition(event);
        RectI itemArea = m_itemDraggableArea.translated(screenPosition());
        if (itemArea.contains(mousePos)) {
          auto const& callback = mouseButton.mouseButton == MouseButton::Right ? m_rightClickCallback
            : mouseButton.mouseButton == MouseButton::Middle ? m_middleClickCallback : m_callback;
          if (!callback) return std::unexpected(GuiException("Item slot has no click callback"));
          auto result = callback(*this);
          if (!result) return std::unexpected(std::move(result.error()));
          return true;
        }
      }
    }
  }

  return false;
}

void ItemSlotWidget::setCallback(WidgetCallbackFunc callback) { m_callback = std::move(callback); }

void ItemSlotWidget::setRightClickCallback(WidgetCallbackFunc callback) { m_rightClickCallback = std::move(callback); }

void ItemSlotWidget::setMiddleClickCallback(WidgetCallbackFunc callback) {
  m_middleClickCallback = std::move(callback);
}

void ItemSlotWidget::setItem(std::shared_ptr<Item> const& item) {
  m_item = item;
}

std::shared_ptr<Item> ItemSlotWidget::item() const {
  return m_item;
}

void ItemSlotWidget::setProgress(float progress) {
  m_progress = progress;
}

void ItemSlotWidget::setBackingImageAffinity(bool full, bool empty) {
  m_drawBackingImageWhenFull = full;
  m_drawBackingImageWhenEmpty = empty;
}

void ItemSlotWidget::setCountPosition(TextPositioning textPositioning) {
  m_countPosition = textPositioning;
}

void ItemSlotWidget::setCountFontMode(FontMode fontMode) {
  m_countFontMode = fontMode;
}

void ItemSlotWidget::showDurability(bool show) {
  m_showDurability = show;
}

void ItemSlotWidget::showCount(bool show) {
  m_showCount = show;
}

void ItemSlotWidget::showRarity(bool showRarity) {
  m_showRarity = showRarity;
}

void ItemSlotWidget::showLinkIndicator(bool showLinkIndicator) {
  m_showLinkIndicator = showLinkIndicator;
}

void ItemSlotWidget::showSecondaryIcon(bool show) {
  m_showSecondaryIcon = show;
}

void ItemSlotWidget::indicateNew() {
  m_newItemIndicator.reset();
}

void ItemSlotWidget::setHighlightEnabled(bool highlight) {
  if (!m_highlightEnabled && highlight)
    m_highlightAnimation.reset();
  m_highlightEnabled = highlight;
}

std::expected<void, StarException> ItemSlotWidget::renderImpl() {
  auto drawCooldown = [&slot = *this]() -> std::expected<void, StarException> {
    int frame = static_cast<int>(std::roundf(slot.m_progress * 18));
    return slot.context().drawInterfaceQuad(AssetPath(String(strf("/interface/cooldown.png:{}", frame))), Vec2F(slot.screenPosition()));
  };
  if (m_item) {
    if (m_drawBackingImageWhenFull && !m_backingImage.empty()) {
      auto result = context().drawInterfaceQuad(m_backingImage, Vec2F(screenPosition()));
      if (!result) return std::unexpected(std::move(result.error()));
    }
    auto icons = m_item->iconDrawables();
    if (!icons) return std::unexpected(std::move(icons.error()));
    if (m_showSecondaryIcon) {
      auto secondary = m_item->secondaryDrawables();
      if (!secondary) return std::unexpected(std::move(secondary.error()));
      if (*secondary) *icons = std::move(**secondary);
    }
    if (m_showRarity) {
      auto rarity = enumName<RarityNames>(m_item->rarity());
      if (!rarity) return std::unexpected(std::move(rarity.error()));
      auto value = m_item->instanceValue("rarityBorder", String(*rarity));
      if (!value) return std::unexpected(std::move(value.error()));
      auto name = value->toString();
      if (!name) return std::unexpected(std::move(name.error()));
      auto border = rarityBorder(std::move(*name));
      if (!border) return std::unexpected(std::move(border.error()));
      auto result = context().drawInterfaceQuad(*border, Vec2F(screenPosition()));
      if (!result) return std::unexpected(std::move(result.error()));
    }
    if (m_showLinkIndicator) {
      auto result = context().drawInterfaceQuad("/interface/inventory/itemlinkindicator.png", Vec2F(screenPosition() - Vec2I(1, 1)));
      if (!result) return std::unexpected(std::move(result.error()));
    }
    for (auto const& icon : *icons) {
      auto result = context().drawInterfaceDrawable(icon, Vec2F(screenPosition() + size() / 2));
      if (!result) return std::unexpected(std::move(result.error()));
    }
    if (!m_newItemIndicator.isComplete()) {
      auto drawable = m_newItemIndicator.drawable(1.0);
      if (!drawable) return std::unexpected(std::move(drawable.error()));
      auto result = context().drawInterfaceDrawable(*drawable, Vec2F(screenPosition() + size() / 2), Color::White.toRgba());
      if (!result) return std::unexpected(std::move(result.error()));
    }
    if (m_showDurability) {
      bool visible = false;
      if (auto item = as<DurabilityItem>(m_item)) {
        auto amount = item->durabilityStatus();
        if (!amount) return std::unexpected(std::move(amount.error()));
        m_durabilityBar->setCurrentProgressLevel(*amount);
        visible = *amount < 1;
      }
      auto result = m_durabilityBar->setVisibility(visible);
      if (!result) return std::unexpected(std::move(result.error()));
    }
    auto cooldown = drawCooldown();
    if (!cooldown) return std::unexpected(std::move(cooldown.error()));
    if (m_item->count() > 1 && m_showCount) {
      auto style = context().setTextStyle(m_textStyle);
      if (!style) return std::unexpected(std::move(style.error()));
      auto mode = context().setFontMode(m_countFontMode);
      if (!mode) return std::unexpected(std::move(mode.error()));
      auto text = context().renderInterfaceText(formatShortSize(m_item->count()), m_countPosition.translated(Vec2F(screenPosition())));
      auto cleared = context().clearTextStyle();
      if (!text) return std::unexpected(std::move(text.error()));
      if (!cleared) return std::unexpected(std::move(cleared.error()));
    }
  } else if (m_drawBackingImageWhenEmpty && !m_backingImage.empty()) {
    auto result = context().drawInterfaceQuad(m_backingImage, Vec2F(screenPosition()));
    if (!result) return std::unexpected(std::move(result.error()));
    auto cooldown = drawCooldown();
    if (!cooldown) return std::unexpected(std::move(cooldown.error()));
  }
  if (m_highlightEnabled) {
    auto drawable = m_highlightAnimation.drawable(1.0);
    if (!drawable) return std::unexpected(std::move(drawable.error()));
    auto result = context().drawInterfaceDrawable(*drawable, Vec2F(screenPosition() + size() / 2), Color::White.toRgba());
    if (!result) return std::unexpected(std::move(result.error()));
  }
  if (!m_item) return m_durabilityBar->hide();
  return {};
}

}
