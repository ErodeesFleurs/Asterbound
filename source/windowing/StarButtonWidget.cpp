import std;
import star.format;
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
import star.button_widget;
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
import star.input;

namespace Star {

ButtonWidget::ButtonWidget(GuiContext& context) : Widget(context) {
  m_hovered = false;
  m_pressed = false;
  m_checkable = false;
  m_checked = false;
  m_disabled = false;
  m_highlighted = false;
  m_hasCheckedImages = false;
  m_sustain = false;
  m_invisible = false;
  m_hTextAnchor = HorizontalAnchor::HMidAnchor;
  m_fontColor = Color::White;
  m_fontColorDisabled = Color::Gray;

}

std::expected<std::shared_ptr<ButtonWidget>, StarException> ButtonWidget::create(GuiContext& context) {
  auto button = std::shared_ptr<ButtonWidget>(new ButtonWidget(context));
  auto initialized = button->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return button;
}

std::expected<void, StarException> ButtonWidget::initialize() {
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto offsetJson = config->get("buttonPressedOffset");
  if (!offsetJson)
    return std::unexpected(std::move(offsetJson.error()));
  auto offset = jsonToVec2I(*offsetJson);
  if (!offset)
    return std::unexpected(std::move(offset.error()));
  m_pressedOffset = *offset;
  auto styleJson = config->get("buttonTextStyle");
  if (!styleJson)
    return std::unexpected(std::move(styleJson.error()));
  auto style = TextStyle::create(*styleJson);
  if (!style)
    return std::unexpected(std::move(style.error()));
  m_textStyle = std::move(*style);
  for (auto entry : {std::pair{std::string_view("buttonClickSound"), std::ref(m_clickSounds)},
           std::pair{std::string_view("buttonReleaseSound"), std::ref(m_releaseSounds)},
           std::pair{std::string_view("buttonHoverSound"), std::ref(m_hoverSounds)},
           std::pair{std::string_view("buttonHoverOffSound"), std::ref(m_hoverOffSounds)}}) {
    auto json = config->get(entry.first);
    if (!json)
      return std::unexpected(std::move(json.error()));
    auto sounds = jsonToStringList(*json);
    if (!sounds)
      return std::unexpected(std::move(sounds.error()));
    entry.second.get() = std::move(*sounds);
  }
  return {};
}

std::expected<std::shared_ptr<ButtonWidget>, StarException> ButtonWidget::create(GuiContext& context, WidgetCallbackFunc callback,
    String const& baseImage, String const& hoverImage, String const& pressedImage, String const& disabledImage) {
  auto button = create(context);
  if (!button)
    return std::unexpected(std::move(button.error()));
  (*button)->setCallback(std::move(callback));
  auto images = (*button)->setImages(baseImage, hoverImage, pressedImage, disabledImage);
  if (!images)
    return std::unexpected(std::move(images.error()));
  return *button;
}


std::expected<void, StarException> ButtonWidget::renderImpl() {
  if (isPressed() && sustainCallbackOnDownHold() && m_callback) {
    auto input = context().input();
    if (!input)
      return std::unexpected(std::move(input.error()));
    auto unlocker = input->get().unlockClipboard();
    auto called = m_callback(*this);
    if (!called)
      return std::unexpected(std::move(called.error()));
  }

  Vec2F position = Vec2F(screenPosition());
  Vec2F textPosition = position + Vec2F(m_textOffset);
  if (m_hTextAnchor == HorizontalAnchor::HMidAnchor)
    textPosition += Vec2F(size()) / 2;
  else if (m_hTextAnchor == HorizontalAnchor::RightAnchor)
    textPosition += Vec2F(size()[0], size()[1] / 2);
  else if (m_hTextAnchor == HorizontalAnchor::LeftAnchor)
    textPosition += Vec2F(0, size()[1] / 2);
  // We need to show the down button offset if we're pressing the button or
  // don't have checked images and thus need some way to show that the button
  // is checked (there's probably some better default behavior in that case)
  if (m_pressed || (m_checked && !m_hasCheckedImages)) {
    position += Vec2F(m_pressedOffset);
    textPosition += Vec2F(m_pressedOffset);
  }

  std::optional<std::reference_wrapper<String const>> image;
  if (m_hasCheckedImages && m_checked) {
    if (m_disabled)
      image = std::cref(m_disabledImageChecked);
    else if ((m_pressed || m_highlighted) && !m_pressedImageChecked.empty())
      image = std::cref(m_pressedImageChecked);
    else if ((m_pressed || m_hovered || m_highlighted) && !m_hoverImageChecked.empty())
      image = std::cref(m_hoverImageChecked);
    else
      image = std::cref(m_baseImageChecked);
  } else {
    if (m_disabled)
      image = std::cref(m_disabledImage);
    else if ((m_pressed || m_highlighted) && !m_pressedImage.empty())
      image = std::cref(m_pressedImage);
    else if ((m_pressed || m_hovered || m_highlighted) && !m_hoverImage.empty())
      image = std::cref(m_hoverImage);
    else if (!m_invisible)
      image = std::cref(m_baseImage);
  }
  if (image) {
    auto drawn = drawButtonPart(image->get(), position);
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  }
  if (!m_overlayImage.empty()) {
    auto drawn = drawButtonPart(m_overlayImage, position);
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  }
  if (!m_text.empty()) {
    auto& gui = context();
    auto style = gui.setTextStyle(m_textStyle);
    if (!style)
      return std::unexpected(std::move(style.error()));
    auto color = gui.setFontColor((m_disabled ? m_fontColorDisabled
        : m_fontColorChecked && m_checked ? *m_fontColorChecked : m_fontColor).toRgba());
    if (!color)
      return std::unexpected(std::move(color.error()));
    auto rendered = gui.renderInterfaceText(m_text, {textPosition, m_hTextAnchor, VerticalAnchor::VMidAnchor});
    auto cleared = gui.clearTextStyle();
    if (!rendered)
      return std::unexpected(std::move(rendered.error()));
    if (!cleared)
      return std::unexpected(std::move(cleared.error()));
  }
  return {};
}

std::expected<bool, StarException> ButtonWidget::sendEvent(InputEvent const& event) {
  if (m_visible && !m_disabled) {
    if (std::holds_alternative<MouseButtonDownEvent>(event) && std::get<MouseButtonDownEvent>(event).mouseButton == MouseButton::Left) {
      if (inMember(*context().mousePosition(event))) {
        if (!isPressed()) {
          auto sound = this->context().root().random().randValueFrom(m_clickSounds, "");
          if (!sound.empty()) {
            auto played = context().playAudio(sound);
            if (!played)
              return std::unexpected(std::move(played.error()));
          }
        }
        auto pressed = setPressed(true);
        if (!pressed)
          return std::unexpected(std::move(pressed.error()));
        if (m_callback) {
          auto focused = focus();
          if (!focused)
            return std::unexpected(std::move(focused.error()));
          return true;
        }
      } else {
        auto blurred = blur();
        if (!blurred)
          return std::unexpected(std::move(blurred.error()));
        return false;
      }
    } else if (std::holds_alternative<MouseButtonUpEvent>(event)) {
      if (isPressed()) {
        auto sound = context().root().random().randValueFrom(m_releaseSounds, "");
        if (!sound.empty()) {
          auto played = context().playAudio(sound);
          if (!played)
            return std::unexpected(std::move(played.error()));
        }
      }
      auto pressed = setPressed(false);
      if (!pressed)
        return std::unexpected(std::move(pressed.error()));
      return false;
    }
  }

  return false;
}

std::expected<void, StarException> ButtonWidget::mouseOver() {
  auto over = Widget::mouseOver();
  if (!over)
    return std::unexpected(std::move(over.error()));
  if (!m_disabled) {
    if (!m_hovered) {
      auto sound = context().root().random().randValueFrom(m_hoverSounds);
      if (!sound.empty()) {
        auto played = context().playAudio(sound);
        if (!played)
          return std::unexpected(std::move(played.error()));
      }
    }
    m_hovered = true;
  }
  return {};
}

std::expected<void, StarException> ButtonWidget::mouseOut() {
  auto out = Widget::mouseOut();
  if (!out)
    return std::unexpected(std::move(out.error()));
  if (!m_disabled && m_hovered) {
    auto sound = context().root().random().randValueFrom(m_hoverOffSounds);
    if (!sound.empty()) {
      auto played = context().playAudio(sound);
      if (!played)
        return std::unexpected(std::move(played.error()));
    }
  }
  m_hovered = false;
  m_pressed = false;
  return {};
}

std::expected<void, StarException> ButtonWidget::mouseReturnStillDown() {
  auto returned = Widget::mouseReturnStillDown();
  if (!returned)
    return std::unexpected(std::move(returned.error()));
  if (!isPressed()) {
    auto sound = context().root().random().randValueFrom(m_clickSounds, "");
    if (!sound.empty()) {
      auto played = context().playAudio(sound);
      if (!played)
        return std::unexpected(std::move(played.error()));
    }
  }
  m_hovered = true;
  m_pressed = true;
  return {};
}

std::expected<void, StarException> ButtonWidget::hide() {
  auto hidden = Widget::hide();
  if (!hidden)
    return std::unexpected(std::move(hidden.error()));
  m_pressed = false;
  m_hovered = false;
  return {};
}

void ButtonWidget::setCallback(WidgetCallbackFunc callback) {
  m_callback = std::move(callback);
}

std::shared_ptr<ButtonGroup> ButtonWidget::buttonGroup() const {
  return m_buttonGroup.lock();
}

void ButtonWidget::setButtonGroup(std::shared_ptr<ButtonGroup> newGroup, int id) {
  if (auto oldGroup = m_buttonGroup.lock()) {
    if (oldGroup == newGroup)
      return;
    oldGroup->removeButton(*this);
  }

  m_buttonGroup = newGroup;
  if (newGroup) {
    setCheckable(true);
    newGroup->addButton(*this, id);
  }
}

int ButtonWidget::buttonGroupId() {
  if (auto group = m_buttonGroup.lock())
    return group->id(*this);
  else
    return ButtonGroup::NoButton;
}

bool ButtonWidget::isHovered() const {
  return m_hovered;
}

bool ButtonWidget::isPressed() const {
  return m_pressed;
}

std::expected<void, StarException> ButtonWidget::setPressed(bool pressed) {
  if (m_pressed != pressed) {
    if (m_pressed) {
      auto checked = check();
      if (!checked)
        return std::unexpected(std::move(checked.error()));
      if (m_callback) {
        auto input = context().input();
        if (!input)
          return std::unexpected(std::move(input.error()));
        auto unlocker = input->get().unlockClipboard();
        auto called = m_callback(*this);
        if (!called)
          return std::unexpected(std::move(called.error()));
      }
    }
    m_pressed = pressed;
  }
  return {};
}

bool ButtonWidget::isCheckable() const {
  return m_checkable;
}

void ButtonWidget::setCheckable(bool checkable) {
  m_checkable = checkable;
}

bool ButtonWidget::isHighlighted() const {
  return m_highlighted;
}

void ButtonWidget::setHighlighted(bool highlighted) {
  m_highlighted = highlighted;
}

bool ButtonWidget::isChecked() const {
  return m_checked;
}

void ButtonWidget::setChecked(bool checked) {
  // might cause button groups to have multiple selected against its rules, be careful with direct poking, use check()
  // instead.
  m_checked = checked;
}

std::expected<void, StarException> ButtonWidget::check() {
  if (m_checkable) {
    if (auto group = m_buttonGroup.lock()) {
      if (group->toggle() || !isChecked()) {
        setChecked(!group->toggle() || !isChecked());
        return group->wasChecked(*this);
      }
    } else {
      setChecked(!isChecked());
    }
  }
  return {};
}

bool ButtonWidget::sustainCallbackOnDownHold() {
  return m_sustain;
}

void ButtonWidget::setSustainCallbackOnDownHold(bool sustain) {
  m_sustain = sustain;
}

std::expected<void, StarException> ButtonWidget::setImages(String const& baseImage, String const& hoverImage, String const& pressedImage, String const& disabledImage) {
  String disabled = disabledImage;
  if (disabled.empty() && !baseImage.empty()) {
    auto assets = context().root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto json = (*assets)->json("/interface.config:disabledButton");
    if (!json)
      return std::unexpected(std::move(json.error()));
    auto suffix = json->toString();
    if (!suffix)
      return std::unexpected(std::move(suffix.error()));
    disabled = baseImage + *suffix;
  }
  m_baseImage = baseImage;
  m_hoverImage = hoverImage;
  m_pressedImage = pressedImage;
  m_disabledImage = std::move(disabled);
  return updateSize();
}

std::expected<void, StarException> ButtonWidget::setCheckedImages(String const& baseImage, String const& hoverImage, String const& pressedImage, String const& disabledImage) {
  String disabled = disabledImage;
  if (!baseImage.empty() && disabled.empty()) {
    auto assets = context().root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto json = (*assets)->json("/interface.config:disabledButton");
    if (!json)
      return std::unexpected(std::move(json.error()));
    auto suffix = json->toString();
    if (!suffix)
      return std::unexpected(std::move(suffix.error()));
    disabled = baseImage + *suffix;
  }
  m_hasCheckedImages = !baseImage.empty();
  m_baseImageChecked = baseImage;
  m_hoverImageChecked = hoverImage;
  m_pressedImageChecked = pressedImage;
  m_disabledImageChecked = std::move(disabled);
  return updateSize();
}

void ButtonWidget::setOverlayImage(String const& overlayImage) {
  m_overlayImage = overlayImage;
}

Vec2I const& ButtonWidget::pressedOffset() const {
  return m_pressedOffset;
}

void ButtonWidget::setPressedOffset(Vec2I const& offset) {
  m_pressedOffset = offset;
}

String const& ButtonWidget::getText() const {
  return m_text;
}

void ButtonWidget::setText(String const& text) {
  m_text = text;
}

void ButtonWidget::setFontSize(int size) {
  m_textStyle.fontSize = size;
}

void ButtonWidget::setFontDirectives(String directives) {
  m_textStyle.directives = directives;
}

void ButtonWidget::setTextOffset(Vec2I textOffset) {
  m_textOffset = textOffset;
}

void ButtonWidget::setTextAlign(HorizontalAnchor hAnchor) {
  m_hTextAnchor = hAnchor;
}

void ButtonWidget::setFontColor(Color color) {
  m_textStyle.color = (m_fontColor = color).toRgba();
}

void ButtonWidget::setFontColorDisabled(Color color) {
  m_fontColorDisabled = color;
}

void ButtonWidget::setFontColorChecked(Color color) {
  m_fontColorChecked = color;
}

// Although ButtonWidget wraps other widgets from time to time.  These should never be "accessible"
std::shared_ptr<Widget> ButtonWidget::getChildAt(Vec2I const&) {
  return {};
}

void ButtonWidget::disable() {
  m_disabled = true;
  m_pressed = false;
}

void ButtonWidget::enable() {
  m_disabled = false;
}

void ButtonWidget::setEnabled(bool enabled) {
  m_disabled = !enabled;
  m_pressed &= enabled; // and off the pressed flag if the button is no longer enabled.
}

void ButtonWidget::setInvisible(bool invisible) {
  m_invisible = invisible;
}

RectI ButtonWidget::getScissorRect() const {
  if (m_pressed || (m_checked && !m_hasCheckedImages)) {
    return RectI::withSize(screenPosition() + m_pressedOffset, size());
  }
  return RectI::withSize(screenPosition(), size());
}

std::expected<void, StarException> ButtonWidget::drawButtonPart(String const& image, Vec2F const& position) {
  auto imageSize = context().textureSize(image);
  if (!imageSize)
    return std::unexpected(std::move(imageSize.error()));
  return context().drawInterfaceQuad(image, position + Vec2F(m_buttonBoundSize - *imageSize) / 2);
}

std::expected<void, StarException> ButtonWidget::updateSize() {
  if (m_invisible || m_baseImage.empty())
    return {};
  auto baseSize = context().textureSize(m_baseImage);
  if (!baseSize)
    return std::unexpected(std::move(baseSize.error()));
  auto bounds = *baseSize;
  for (auto image : {std::cref(m_hoverImage), std::cref(m_pressedImage), std::cref(m_baseImageChecked),
           std::cref(m_hoverImageChecked), std::cref(m_pressedImageChecked), std::cref(m_disabledImageChecked)}) {
    if (!image.get().empty()) {
      auto imageSize = context().textureSize(image.get());
      if (!imageSize)
        return std::unexpected(std::move(imageSize.error()));
      bounds = bounds.piecewise_max(*imageSize);
    }
  }
  m_buttonBoundSize = bounds;
  setSize(Vec2I(bounds));
  return {};
}

}
