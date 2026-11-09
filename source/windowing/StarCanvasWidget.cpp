import std;
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
import star.version;
import star.string;
import star.text;
import star.poly;
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
import star.canvas_widget;

import star.math_common;
import star.algorithm;
import star.format;
namespace Star {

std::expected<std::shared_ptr<CanvasWidget>, StarException> CanvasWidget::create(GuiContext& context) {
  return std::shared_ptr<CanvasWidget>(new CanvasWidget(context));
}

CanvasWidget::CanvasWidget(GuiContext& context) : Widget(context) {
  m_ignoreInterfaceScale = m_captureKeyboard = m_captureMouse = false;
}

void CanvasWidget::setCaptureMouseEvents(bool captureMouse) {
  m_captureMouse = captureMouse;
}

void CanvasWidget::setCaptureKeyboardEvents(bool captureKeyboard) {
  m_captureKeyboard = captureKeyboard;
}

void CanvasWidget::setIgnoreInterfaceScale(bool ignoreInterfaceScale) {
  m_ignoreInterfaceScale = ignoreInterfaceScale;
}

bool CanvasWidget::ignoreInterfaceScale() const {
  return m_ignoreInterfaceScale;
}

void CanvasWidget::clear() {
  m_renderOps.clear();
  m_clipHead.reset();
}

void CanvasWidget::pushClipRect(RectF const& rect) {
  auto clipped = rect;
  if (m_clipHead)
    clipped.limit(std::get<PushClipOp>(m_renderOps[*m_clipHead]).rect);
  clipped = RectF(clipped.min(), clipped.max().piecewise_max(clipped.min()));
  auto index = m_renderOps.size();
  m_renderOps.push_back(PushClipOp{.rect = clipped, .parent = m_clipHead});
  m_clipHead = index;
}

std::expected<void, StarException> CanvasWidget::popClipRect() {
  if (!m_clipHead)
    return std::unexpected(GuiException("Canvas clip stack is empty"));
  auto parent = std::get<PushClipOp>(m_renderOps[*m_clipHead]).parent;
  std::optional<RectF> restored;
  if (parent)
    restored = std::get<PushClipOp>(m_renderOps[*parent]).rect;
  m_renderOps.push_back(PopClipOp{restored});
  m_clipHead = parent;
  return {};
}

void CanvasWidget::drawImage(String texName, Vec2F const& position, float scale, Vec4B const& color) {
  m_renderOps.push_back(std::make_tuple(std::move(texName), position, scale, color, false));
}

void CanvasWidget::drawImageCentered(String texName, Vec2F const& position, float scale, Vec4B const& color) {
  m_renderOps.push_back(std::make_tuple(std::move(texName), position, scale, color, true));
}

void CanvasWidget::drawImageRect(String texName, RectF const& texCoords, RectF const& screenCoords, Vec4B const& color) {
  m_renderOps.push_back(std::make_tuple(std::move(texName), texCoords, screenCoords, color));
}

void CanvasWidget::drawDrawable(Drawable drawable, Vec2F const& screenPos, float scale, Vec2F const& translation, float finalScale) {
  if (translation != Vec2F())
    drawable.translate(translation);
  if (scale != 1)
    drawable.scale(scale);
  if (finalScale != 1)
    drawable.scale(finalScale);
  m_renderOps.push_back(std::make_tuple(std::move(drawable), screenPos));
}

void CanvasWidget::drawDrawables(List<Drawable> drawables, Vec2F const& screenPos, float scale, Vec2F const& translation, float finalScale) {
  for (auto& drawable : drawables)
    drawDrawable(std::move(drawable), screenPos, scale, translation, finalScale);
}

void CanvasWidget::drawTiledImage(String texName, float textureScale, Vec2D const& offset, RectF const& screenCoords, Vec4B const& color) {
  m_renderOps.push_back(std::make_tuple(std::move(texName), textureScale, offset, screenCoords, color));
}

void CanvasWidget::drawLine(Vec2F const& begin, Vec2F const end, Vec4B const& color, float lineWidth) {
  m_renderOps.push_back(std::make_tuple(begin, end, color, lineWidth));
}

void CanvasWidget::drawRect(RectF const& coords, Vec4B const& color) {
  m_renderOps.push_back(std::make_tuple(coords, color));
}

void CanvasWidget::drawPoly(PolyF const& poly, Vec4B const& color, float lineWidth) {
  m_renderOps.push_back(std::make_tuple(poly, color, lineWidth));
}

void CanvasWidget::drawTriangles(List<std::tuple<Vec2F, Vec2F, Vec2F>> const& triangles, Vec4B const& color) {
  m_renderOps.push_back(std::make_tuple(triangles, color));
}

void CanvasWidget::drawText(String s, TextPositioning position, unsigned fontSize, Vec4B const& color, FontMode mode, float lineSpacing, String font, String processingDirectives) {
  TextStyle style;
  style.fontSize = fontSize;
  style.color = color;
  style.shadow = fontModeToColor(mode).toRgba();
  style.lineSpacing = lineSpacing;
  style.font = std::move(font);
  style.directives = processingDirectives;
  m_renderOps.push_back(std::make_tuple(std::move(s), std::move(position), std::move(style)));
}

void CanvasWidget::drawText(String s, TextPositioning position, TextStyle style) {
  m_renderOps.push_back(std::make_tuple(std::move(s), std::move(position), std::move(style)));
}

std::expected<RectF, StarException> CanvasWidget::measureText(String const& s, TextPositioning const& position, TextStyle const& style) const {
  auto& context = this->context();
  auto applied = context.setTextStyle(style, m_ignoreInterfaceScale ? 1 : context.interfaceScale());
  if (!applied)
    return std::unexpected(std::move(applied.error()));
  auto result = m_ignoreInterfaceScale ? context.determineTextSize(s, position) : context.determineInterfaceTextSize(s, position);
  auto cleared = context.clearTextStyle();
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (!cleared)
    return std::unexpected(std::move(cleared.error()));
  return *result;
}

Vec2I CanvasWidget::mousePosition() const {
  return m_mousePosition;
}

List<CanvasWidget::ClickEvent> CanvasWidget::pullClickEvents() { return std::exchange(m_clickEvents, {}); }

List<CanvasWidget::KeyEvent> CanvasWidget::pullKeyEvents() { return std::exchange(m_keyEvents, {}); }

std::expected<bool, StarException> CanvasWidget::sendEvent(InputEvent const& event) {
  if (!m_visible)
    return false;

  auto& context = (this->context());
  auto interfaceScale = m_ignoreInterfaceScale ? 1 : context.interfaceScale();
  if (std::holds_alternative<MouseButtonDownEvent>(event)) {
    auto const& mouseButtonDown = std::get<MouseButtonDownEvent>(event);
    if (inMember(*context.mousePosition(event, interfaceScale)) && m_captureMouse) {
      m_clickEvents.push_back({.position = *context.mousePosition(event, interfaceScale) - screenPosition(),
                               .button = mouseButtonDown.mouseButton,
                               .buttonDown = true});
      m_clickEvents.truncate_back(MaximumEventBuffer);
      return true;
    }
  } else if (std::holds_alternative<MouseButtonUpEvent>(event)) {
    auto const& mouseButtonUp = std::get<MouseButtonUpEvent>(event);
    if (m_captureMouse) {
      m_clickEvents.push_back({.position = *context.mousePosition(event, interfaceScale) - screenPosition(),
                               .button = mouseButtonUp.mouseButton,
                               .buttonDown = false});
      m_clickEvents.truncate_back(MaximumEventBuffer);
      return true;
    }
  } else if (std::holds_alternative<MouseMoveEvent>(event)) {
    m_mousePosition = *context.mousePosition(event, interfaceScale) - screenPosition();
    return false;
  } else if (std::holds_alternative<KeyDownEvent>(event)) {
    auto const& keyDown = std::get<KeyDownEvent>(event);
    if (m_captureKeyboard) {
      m_keyEvents.push_back({.key = keyDown.key, .keyDown = true});
      return true;
    }
  } else if (std::holds_alternative<KeyUpEvent>(event)) {
    auto const& keyUp = std::get<KeyUpEvent>(event);
    if (m_captureKeyboard) {
      m_keyEvents.push_back({.key = keyUp.key, .keyDown = false});
      m_keyEvents.truncate_back(MaximumEventBuffer);
      return true;
    }
  }

  return Widget::sendEvent(event);
}

std::expected<KeyboardCaptureMode, StarException> CanvasWidget::keyboardCaptureMode() const {
  return m_captureKeyboard ? KeyboardCaptureMode::KeyEvents : KeyboardCaptureMode::None;
}

std::expected<void, StarException> CanvasWidget::renderImpl() {
  auto offset = Vec2F(screenPosition());
  auto renderer = context().renderer();
  if (!renderer)
    return std::unexpected(std::move(renderer.error()));
  auto& target = renderer->get();
  auto priorClip = target.scissorRect();
  bool changedClip = false;
  auto setClip = [&](std::optional<RectF> const& local) -> std::expected<void, StarException> {
    auto clip = priorClip;
    if (local) {
      auto physical = local->translated(offset);
      if (!m_ignoreInterfaceScale)
        physical.scale(context().interfaceScale());
      RectI pixels(physical);
      if (priorClip)
        pixels.limit(*priorClip);
      clip = RectI(pixels.min(), pixels.max().piecewise_max(pixels.min()));
    }
    changedClip = true;
    return target.setScissorRect(clip);
  };
  std::expected<void, StarException> rendered;
  for (auto const& op : m_renderOps) {
    auto result = std::visit([&](auto const& args) -> std::expected<void, StarException> {
      using T = std::remove_cvref_t<decltype(args)>;
      if constexpr (std::is_same_v<T, ImageOp>)
        return std::apply(std::bind_front(&CanvasWidget::renderImage, this, offset), args);
      else if constexpr (std::is_same_v<T, ImageRectOp>)
        return std::apply(std::bind_front(&CanvasWidget::renderImageRect, this, offset), args);
      else if constexpr (std::is_same_v<T, DrawableOp>)
        return std::apply(std::bind_front(&CanvasWidget::renderDrawable, this, offset), args);
      else if constexpr (std::is_same_v<T, TiledImageOp>)
        return std::apply(std::bind_front(&CanvasWidget::renderTiledImage, this, offset), args);
      else if constexpr (std::is_same_v<T, LineOp>)
        return std::apply(std::bind_front(&CanvasWidget::renderLine, this, offset), args);
      else if constexpr (std::is_same_v<T, RectOp>)
        return std::apply(std::bind_front(&CanvasWidget::renderRect, this, offset), args);
      else if constexpr (std::is_same_v<T, PolyOp>)
        return std::apply(std::bind_front(&CanvasWidget::renderPoly, this, offset), args);
      else if constexpr (std::is_same_v<T, TrianglesOp>)
        return std::apply(std::bind_front(&CanvasWidget::renderTriangles, this, offset), args);
      else if constexpr (std::is_same_v<T, TextOp>)
        return std::apply(std::bind_front(&CanvasWidget::renderText, this, offset), args);
      else if constexpr (std::is_same_v<T, PushClipOp>)
        return setClip(args.rect);
      else if constexpr (std::is_same_v<T, PopClipOp>)
        return setClip(args.rect);
      else
        return {};
    }, op);
    if (!result) {
      rendered = std::unexpected(std::move(result.error()));
      break;
    }
  }
  if (changedClip) {
    auto restored = target.setScissorRect(priorClip);
    if (!restored) {
      if (!rendered)
        return std::unexpected(rendered.error().with_context(Star::format("Restoring Canvas clipping also failed: {}", Star::output_exception(restored.error(), true))));
      return std::unexpected(std::move(restored.error()));
    }
  }
  return rendered;
}

std::expected<void, StarException> CanvasWidget::renderImage(Vec2F const& renderingOffset, String const& texName, Vec2F const& position, float scale, Vec4B const& color, bool centered) {
  auto& context = this->context();
  auto textureSize = context.textureSize(texName);
  if (!textureSize) return std::unexpected(std::move(textureSize.error()));
  auto texSize = Vec2F(*textureSize);
  Vec2F pos = centered ? (position - scale * texSize / 2.0f) : position;

  RectF screenCoords;
  if (m_ignoreInterfaceScale)
    screenCoords = RectF::withSize(renderingOffset + pos, texSize * scale);
  else
    screenCoords = RectF::withSize(renderingOffset * context.interfaceScale() + pos * context.interfaceScale(), texSize * scale * context.interfaceScale());

  return context.drawQuad(texName, screenCoords, color);
}

std::expected<void, StarException> CanvasWidget::renderImageRect(Vec2F const& renderingOffset, String const& texName, RectF const& texCoords, RectF const& screenCoords, Vec4B const& color) {
  auto& context = this->context();
  if (m_ignoreInterfaceScale)
    return context.drawQuad(texName, texCoords, screenCoords.translated(renderingOffset), color);
  return context.drawQuad(texName, texCoords, screenCoords.scaled(context.interfaceScale()).translated(renderingOffset * context.interfaceScale()), color);
}

std::expected<void, StarException> CanvasWidget::renderDrawable(Vec2F const& renderingOffset, Drawable drawable, Vec2F const& screenPos) {
  auto& context = this->context();
  if (m_ignoreInterfaceScale)
    return context.drawDrawable(std::move(drawable), renderingOffset + screenPos, 1);
  drawable.scale(context.interfaceScale());
  return context.drawDrawable(std::move(drawable), renderingOffset * context.interfaceScale() + screenPos * context.interfaceScale(), 1);
}

std::expected<void, StarException> CanvasWidget::renderTiledImage(Vec2F const& renderingOffset, String const& texName, float textureScale, Vec2D const& offset, RectF const& screenCoords, Vec4B const& color) {
  auto& context = this->context();
  auto textureSize = context.textureSize(texName);
  if (!textureSize) return std::unexpected(std::move(textureSize.error()));
  Vec2F texSize(*textureSize);
  Vec2F texScaledSize = texSize * textureScale;
  Vec2I textureCount = Vec2I::ceil(screenCoords.size().piecewise_divide(texScaledSize)) + Vec2I(2, 2);
  Vec2F screenLowerLeft = screenCoords.min() - Vec2F(pfmod<double>(texScaledSize[0] - offset[0], texScaledSize[0]), pfmod<double>(texScaledSize[1] - offset[1], texScaledSize[1]));

  for (int x = 0; x < textureCount[0]; ++x) {
    for (int y = 0; y < textureCount[1]; ++y) {
      Vec2F screenPos =
        screenLowerLeft + texScaledSize.piecewise_multiply({static_cast<float>(x), static_cast<float>(y)});
      RectF screenRect = RectF::withSize(screenPos, texScaledSize);

      RectF limitedScreenRect;
      limitedScreenRect.setXMin(std::max(screenRect.xMin(), screenCoords.xMin()));
      limitedScreenRect.setYMin(std::max(screenRect.yMin(), screenCoords.yMin()));
      limitedScreenRect.setXMax(std::min(screenRect.xMax(), screenCoords.xMax()));
      limitedScreenRect.setYMax(std::min(screenRect.yMax(), screenCoords.yMax()));

      RectF limitedTexRect = limitedScreenRect.translated(-screenPos).scaled(1 / textureScale);

      if (limitedScreenRect.isEmpty())
        continue;

      auto result = m_ignoreInterfaceScale
        ? context.drawQuad(texName, limitedTexRect, limitedScreenRect.translated(renderingOffset), color)
        : context.drawQuad(texName, limitedTexRect, limitedScreenRect.translated(renderingOffset).scaled(context.interfaceScale()), color);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> CanvasWidget::renderLine(Vec2F const& renderingOffset, Vec2F const& begin, Vec2F const end, Vec4B const& color, float lineWidth) {
  auto& context = this->context();
  if (m_ignoreInterfaceScale)
    return context.drawLine(renderingOffset + begin, renderingOffset + end, color, lineWidth);
  else {
    return context.drawLine(
      renderingOffset * context.interfaceScale() + begin * context.interfaceScale(),
      renderingOffset * context.interfaceScale() + end * context.interfaceScale(),
      color, lineWidth);
  }
}

std::expected<void, StarException> CanvasWidget::renderRect(Vec2F const& renderingOffset, RectF const& coords, Vec4B const& color) {
  auto& context = this->context();
  if (m_ignoreInterfaceScale)
    return context.drawQuad(coords.translated(renderingOffset), color);
  return context.drawQuad(coords.scaled(context.interfaceScale()).translated(renderingOffset * context.interfaceScale()), color);
}

std::expected<void, StarException> CanvasWidget::renderPoly(Vec2F const& renderingOffset, PolyF poly, Vec4B const& color, float lineWidth) {
  auto& context = this->context();
  poly.translate(renderingOffset);
  if (m_ignoreInterfaceScale)
    return context.drawPolyLines(poly, color, lineWidth);
  return context.drawInterfacePolyLines(poly, color, lineWidth);
}

std::expected<void, StarException> CanvasWidget::renderTriangles(Vec2F const& renderingOffset, List<std::tuple<Vec2F, Vec2F, Vec2F>> const& triangles, Vec4B const& color) {
  auto& context = (this->context());
  auto translated = triangles.transformed([&renderingOffset](std::tuple<Vec2F, Vec2F, Vec2F> const& poly) {
      return std::tuple<Vec2F, Vec2F, Vec2F>(std::get<0>(poly) + renderingOffset,
        std::get<1>(poly) + renderingOffset,
        std::get<2>(poly) + renderingOffset);
    });
  if (m_ignoreInterfaceScale)
    return context.drawTriangles(translated, color);
  return context.drawInterfaceTriangles(translated, color);
}

std::expected<void, StarException> CanvasWidget::renderText(Vec2F const& renderingOffset, String const& s, TextPositioning const& position, TextStyle const& style) {
  auto& context = this->context();
  auto applied = context.setTextStyle(style, m_ignoreInterfaceScale ? 1 : context.interfaceScale());
  if (!applied) return std::unexpected(std::move(applied.error()));
  TextPositioning translatedPosition = position;
  translatedPosition.pos += renderingOffset;
  auto result = m_ignoreInterfaceScale ? context.renderText(s, translatedPosition) : context.renderInterfaceText(s, translatedPosition);
  auto cleared = context.clearTextStyle();
  if (!result) return std::unexpected(std::move(result.error()));
  if (!cleared) return std::unexpected(std::move(cleared.error()));
  return {};
}

}
