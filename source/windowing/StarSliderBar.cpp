import std;
import star.format;
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
import star.button_widget;
import star.image_widget;
import star.slider_bar;
import star.widget_parsing;
import star.gui_reader;


import star.image_metadata_database;

import star.math_common;
import star.pythonic;
namespace Star {

SliderBarWidget::SliderBarWidget(GuiContext& context)
  : Widget(context),
    m_low(0),
    m_high(1),
    m_delta(1),
    m_val(0),
    m_updateJog(true),
    m_jogDragActive(false),
    m_enabled(true) {

}

std::expected<std::shared_ptr<SliderBarWidget>, StarException> SliderBarWidget::create(GuiContext& context, String const& grid, bool showSpinner) {
  auto slider = std::shared_ptr<SliderBarWidget>(new SliderBarWidget(context));
  auto initialized = slider->initialize(grid, showSpinner);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return slider;
}

std::expected<void, StarException> SliderBarWidget::initialize(String const& grid, bool showSpinner) {
  auto image = ImageWidget::create(context(), grid);
  if (!image)
    return std::unexpected(std::move(image.error()));
  m_grid = *image;
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto metadata = context().root().imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto config = (*assets)->json("/interface.config:slider");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto jogImage = config->getString("jog");
  if (!jogImage)
    return std::unexpected(std::move(jogImage.error()));
  auto jog = ButtonWidget::create(context());
  if (!jog)
    return std::unexpected(std::move(jog.error()));
  m_jog = *jog;
  auto images = m_jog->setImages(*jogImage);
  if (!images)
    return std::unexpected(std::move(images.error()));
  m_jog->setPressedOffset({0, 0});
  if (showSpinner) {
    GuiReader reader(context());
    reader.registerCallback("spinner.down", [owner = std::weak_ptr<SliderBarWidget>(as<SliderBarWidget>(shared_from_this()))](Widget&) -> std::expected<void, StarException> {
      auto slider = owner.lock();
      if (!slider) return std::unexpected(GuiException("Slider callback target no longer exists"));
      return slider->leftCallback();
    });
    reader.registerCallback("spinner.up", [owner = std::weak_ptr<SliderBarWidget>(as<SliderBarWidget>(shared_from_this()))](Widget&) -> std::expected<void, StarException> {
      auto slider = owner.lock();
      if (!slider) return std::unexpected(GuiException("Slider callback target no longer exists"));
      return slider->rightCallback();
    });
    auto leftBase = config->getString("leftBase");
    if (!leftBase)
      return std::unexpected(std::move(leftBase.error()));
    auto leftHover = config->getString("leftHover");
    if (!leftHover)
      return std::unexpected(std::move(leftHover.error()));
    auto rightBase = config->getString("rightBase");
    if (!rightBase)
      return std::unexpected(std::move(rightBase.error()));
    auto rightHover = config->getString("rightHover");
    if (!rightHover)
      return std::unexpected(std::move(rightHover.error()));
    auto padding = config->getInt("defaultPadding");
    if (!padding)
      return std::unexpected(std::move(padding.error()));
    auto leftSize = (*metadata)->imageSize(*leftBase);
    if (!leftSize)
      return std::unexpected(std::move(leftSize.error()));
    auto gridSize = (*metadata)->imageSize(grid);
    if (!gridSize)
      return std::unexpected(std::move(gridSize.error()));
    float gridOffset = (*leftSize)[0] + *padding;
    float rightOffset = gridOffset + (*gridSize)[0] + *padding;
    Json spinner = JsonObject{{"spinner", JsonObject{{"type", "spinner"},
        {"leftBase", *leftBase}, {"leftHover", *leftHover},
        {"rightBase", *rightBase}, {"rightHover", *rightHover},
        {"position", JsonArray{0, 0}}, {"upOffset", rightOffset}}}};
    auto constructed = reader.construct(spinner, *this);
    if (!constructed)
      return std::unexpected(std::move(constructed.error()));
    m_grid->setPosition(Vec2I(gridOffset, 0));
    auto left = fetchChild<ButtonWidget>("spinner.down");
    if (!left)
      return std::unexpected(std::move(left.error()));
    auto right = fetchChild<ButtonWidget>("spinner.up");
    if (!right)
      return std::unexpected(std::move(right.error()));
    m_leftButton = *left;
    m_rightButton = *right;
  }
  auto addedGrid = addChild("grid", m_grid);
  if (!addedGrid)
    return std::unexpected(std::move(addedGrid.error()));
  auto addedJog = addChild("jog", m_jog);
  if (!addedJog)
    return std::unexpected(std::move(addedJog.error()));
  markAsContainer();
  disableScissoring();
  return {};
}

std::expected<void, StarException> SliderBarWidget::setJogImages(String const& baseImage, String const& hoverImage, String const& pressedImage, String const& disabledImage) {
  return m_jog->setImages(baseImage, hoverImage, pressedImage, disabledImage);
}

std::expected<void, StarException> SliderBarWidget::setRange(int low, int high, int delta) {
  m_low = low;
  m_high = high;
  m_high = std::max(m_low + 1, m_high);
  if (delta <= 0)
    m_delta = 1;
  else
    m_delta = delta;
  return setVal(m_val);
}

std::expected<void, StarException> SliderBarWidget::setRange(Vec2I const& range, int delta) {
  return setRange(range[0], range[1], delta);
}

std::expected<void, StarException> SliderBarWidget::setVal(int val, bool callbackIfChanged) {
  bool doCallback = false;
  if (m_callback && callbackIfChanged && m_val != val)
    doCallback = true;

  if (m_val != val)
    m_updateJog = true;

  m_val = val;
  m_val = std::clamp(m_val, m_low, m_high);

  if (doCallback)
    return m_callback(*this);
  return {};
}

int SliderBarWidget::val() const {
  return m_val;
}

void SliderBarWidget::setEnabled(bool enabled) {
  if (m_enabled != enabled) {
    m_enabled = enabled;
    m_jogDragActive = false;
    m_jog->setEnabled(enabled);
    if (m_rightButton)
      m_rightButton->setEnabled(enabled);
    if (m_leftButton)
      m_leftButton->setEnabled(enabled);
  }
}

void SliderBarWidget::setCallback(WidgetCallbackFunc callback) { m_callback = std::move(callback); }

std::expected<void, StarException> SliderBarWidget::update(float dt) {
  float gridLow = m_grid->position()[0];
  float gridHigh = gridLow + m_grid->size()[0];

  if (m_jogDragActive) {
    auto clampedPos = clamp<float>(m_jogDragPos[0], gridLow, gridHigh);
    float clampedPercent = (clampedPos - gridLow) / (gridHigh - gridLow);
    auto changed = setVal(clampedPercent * (m_high - m_low), true);
    if (!changed)
      return std::unexpected(std::move(changed.error()));
  }

  if (m_updateJog) {
    float percent = static_cast<float>(m_val) / (m_high - m_low);

    float jogPos = (gridHigh - gridLow) * percent + gridLow - m_jog->size()[0] * 0.5;

    m_jog->setPosition(Vec2I(jogPos, 0));
    m_updateJog = false;
  }

  return Widget::update(dt);
}

std::expected<bool, StarException> SliderBarWidget::sendEvent(InputEvent const& event) {
  if (std::holds_alternative<MouseButtonUpEvent>(event)) {
    auto blurred = blur();
    if (!blurred)
      return std::unexpected(std::move(blurred.error()));
    m_jogDragActive = false;
  }

  if (m_enabled) {
    if (std::holds_alternative<MouseButtonDownEvent>(event)) {
      if (m_jog->inMember(*context().mousePosition(event))) {
        auto focused = focus();
        if (!focused)
          return std::unexpected(std::move(focused.error()));
        m_jogDragPos = *context().mousePosition(event) - screenPosition();
        m_jogDragActive = true;
      }
    }

    if (std::holds_alternative<MouseMoveEvent>(event)) {
      if (m_jogDragActive)
        m_jogDragPos = *context().mousePosition(event) - screenPosition();
    }
  }

  return Widget::sendEvent(event);
}

std::expected<void, StarException> SliderBarWidget::leftCallback() {
  return setVal(std::max(m_low, m_val - m_delta), true);
}

std::expected<void, StarException> SliderBarWidget::rightCallback() {
  return setVal(std::min(m_high, m_val + m_delta), true);
}

}
