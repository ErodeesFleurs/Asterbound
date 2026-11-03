import std;
import star.json;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.color;
import star.font;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.string;
import star.vector;
import star.text;
import star.poly;
import star.listener;
import star.input_event;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.format;
import star.version;

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
import star.configuration;
import star.mixer;


import star.image_metadata_database;

import star.byte_array;
namespace Star {

class Input;
class Voice;
class TileDrawer;

GuiContext::GuiContext(Root& root, std::shared_ptr<Mixer> mixer, std::shared_ptr<ApplicationController> appController)
  : m_root(root), m_mixer(std::move(mixer)), m_applicationController(std::move(appController)),
    m_interfaceScale(1), m_shiftHeld(false) {}

std::expected<std::shared_ptr<GuiContext>, StarException> GuiContext::create(Root& root, std::shared_ptr<Mixer> mixer, std::shared_ptr<ApplicationController> appController) {
  auto context = std::shared_ptr<GuiContext>(new GuiContext(root, std::move(mixer), std::move(appController)));
  auto loaded = context->refreshKeybindings();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  return context;
}

GuiContext::~GuiContext() = default;

Root& GuiContext::root() const {
  return m_root;
}

void GuiContext::bindInput(Input& input) {
  m_input = std::ref(input);
}

std::expected<std::reference_wrapper<Input>, StarException> GuiContext::input() const {
  if (!m_input)
    return std::unexpected(GuiContextException("GuiContext::input() called before bindInput"));
  return *m_input;
}

void GuiContext::bindVoice(Voice& voice) {
  m_voice = std::ref(voice);
}

std::expected<std::reference_wrapper<Voice>, StarException> GuiContext::voice() const {
  if (!m_voice)
    return std::unexpected(GuiContextException("GuiContext::voice() called before bindVoice"));
  return *m_voice;
}

void GuiContext::setTileDrawer(std::optional<std::reference_wrapper<TileDrawer>> tileDrawer) {
  m_tileDrawer = tileDrawer;
}

std::optional<std::reference_wrapper<TileDrawer>> GuiContext::tileDrawer() const {
  return m_tileDrawer;
}

std::expected<void, StarException> GuiContext::renderInit(std::shared_ptr<Renderer> renderer) {
  auto textureGroup = renderer->createTextureGroup();
  if (!textureGroup)
    return std::unexpected(std::move(textureGroup.error()));
  auto textures = std::make_shared<AssetTextureGroup>(m_root, *textureGroup);
  auto drawables = std::make_shared<DrawablePainter>(renderer, textures);
  auto text = TextPainter::create(m_root, renderer, *textureGroup);
  if (!text)
    return std::unexpected(std::move(text.error()));
  m_renderer = std::move(renderer);
  m_textureCollection = std::move(textures);
  m_drawablePainter = std::move(drawables);
  m_textPainter = std::move(*text);
  return {};
}

std::shared_ptr<Mixer> const& GuiContext::mixer() const {
  return m_mixer;
}

std::shared_ptr<ApplicationController> const& GuiContext::applicationController() const {
  return m_applicationController;
}

std::expected<std::reference_wrapper<Renderer>, StarException> GuiContext::renderer() const {
  if (!m_renderer)
    return std::unexpected(GuiContextException("GuiContext::renderer() called before renderInit"));
  return std::ref(*m_renderer);
}

std::expected<std::reference_wrapper<AssetTextureGroup>, StarException> GuiContext::assetTextureGroup() const {
  if (!m_textureCollection)
    return std::unexpected(GuiContextException("GuiContext::assetTextureGroup() called before renderInit"));
  return std::ref(*m_textureCollection);
}

std::expected<std::reference_wrapper<TextPainter>, StarException> GuiContext::textPainter() const {
  if (!m_textPainter)
    return std::unexpected(GuiContextException("GuiContext::textPainter() called before renderInit"));
  return std::ref(*m_textPainter);
}

std::expected<unsigned, StarException> GuiContext::windowWidth() const {
  auto size = windowSize();
  if (!size)
    return std::unexpected(std::move(size.error()));
  return (*size)[0];
}

std::expected<unsigned, StarException> GuiContext::windowHeight() const {
  auto size = windowSize();
  if (!size)
    return std::unexpected(std::move(size.error()));
  return (*size)[1];
}

std::expected<Vec2U, StarException> GuiContext::windowSize() const {
  auto target = renderer();
  if (!target)
    return std::unexpected(std::move(target.error()));
  return target->get().screenSize();
}

std::expected<Vec2U, StarException> GuiContext::windowInterfaceSize() const {
  auto size = windowSize();
  if (!size)
    return std::unexpected(std::move(size.error()));
  return Vec2U::ceil(Vec2F(*size) / interfaceScale());
}

float GuiContext::interfaceScale() const {
  float DisplayScale {std::max(1.0f, std::round(getDisplayScale()))};
  return m_interfaceScale * DisplayScale;
}

void GuiContext::setInterfaceScale(float interfaceScale) {
  m_interfaceScale = interfaceScale;
}

namespace {
std::optional<Vec2F> eventMousePosition(InputEvent const& event) {
  return std::visit(
    [](auto const& input) -> std::optional<Vec2F> {
      using Event = std::remove_cvref_t<decltype(input)>;
      if constexpr (std::same_as<Event, MouseMoveEvent> || std::same_as<Event, MouseButtonDownEvent>
                    || std::same_as<Event, MouseButtonUpEvent> || std::same_as<Event, MouseWheelEvent>)
        return input.mousePosition;
      else
        return {};
    },
    event);
}
}// namespace

std::optional<Vec2I> GuiContext::mousePosition(InputEvent const& event, float pixelRatio) const {
  return eventMousePosition(event).transform(
    [pixelRatio](Vec2F const& position) { return Vec2I(position / pixelRatio); });
}

std::optional<Vec2I> GuiContext::mousePosition(InputEvent const& event) const {
  return eventMousePosition(event).transform(
    [this](Vec2F const& position) { return Vec2I(position / interfaceScale()); });
}

Set<InterfaceAction> GuiContext::actions(InputEvent const& event) const {
  return m_keyBindings.actions(event);
}

Set<InterfaceAction> GuiContext::actionsForKey(Key key) const {
  return m_keyBindings.actionsForKey(key);
}

std::expected<void, StarException> GuiContext::refreshKeybindings() {
  auto configuration = m_root.configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto json = (*configuration)->get("bindings");
  if (!json)
    return std::unexpected(std::move(json.error()));
  auto bindings = KeyBindings::create(m_root, *json);
  if (!bindings)
    return std::unexpected(std::move(bindings.error()));
  m_keyBindings = std::move(*bindings);
  return {};
}

std::expected<void, StarException> GuiContext::setInterfaceScissorRect(RectI const& scissor) {
  auto target = renderer();
  if (!target)
    return std::unexpected(std::move(target.error()));
  return target->get().setScissorRect(RectI(RectF(scissor).scaled(interfaceScale())));
}

std::expected<void, StarException> GuiContext::resetInterfaceScissorRect() {
  auto target = renderer();
  if (!target)
    return std::unexpected(std::move(target.error()));
  return target->get().setScissorRect({});
}

std::expected<Vec2U, StarException> GuiContext::textureSize(AssetPath const& texName) {
  auto textures = assetTextureGroup();
  if (!textures)
    return std::unexpected(std::move(textures.error()));
  auto texture = textures->get().loadTexture(texName);
  if (!texture)
    return std::unexpected(std::move(texture.error()));
  return (*texture)->size();
}

std::expected<void, StarException> GuiContext::drawQuad(RectF const& screenCoords, Vec4B const& color) {
  auto target = renderer();
  if (!target)
    return std::unexpected(std::move(target.error()));
  target->get().immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(), screenCoords, color, 0.0f);
  return {};
}

std::expected<void, StarException> GuiContext::drawQuad(AssetPath const& texName, RectF const& screenCoords, Vec4B const& color) {
  auto target = renderer();
  if (!target)
    return std::unexpected(std::move(target.error()));
  auto textures = assetTextureGroup();
  if (!textures)
    return std::unexpected(std::move(textures.error()));
  auto texture = textures->get().loadTexture(texName);
  if (!texture)
    return std::unexpected(std::move(texture.error()));
  target->get().immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(), *texture, screenCoords, color, 0.0f);
  return {};
}

std::expected<void, StarException> GuiContext::drawQuad(AssetPath const& texName, Vec2F const& screenPos, float pixelRatio, Vec4B const& color) {
  auto target = renderer();
  if (!target)
    return std::unexpected(std::move(target.error()));
  auto textures = assetTextureGroup();
  if (!textures)
    return std::unexpected(std::move(textures.error()));
  auto texture = textures->get().loadTexture(texName);
  if (!texture)
    return std::unexpected(std::move(texture.error()));
  target->get().immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(), *texture, screenPos, pixelRatio, color, 0.0f);
  return {};
}

std::expected<void, StarException> GuiContext::drawQuad(AssetPath const& texName, RectF const& texCoords, RectF const& screenCoords, Vec4B const& color) {
  auto target = renderer();
  if (!target)
    return std::unexpected(std::move(target.error()));
  auto textures = assetTextureGroup();
  if (!textures)
    return std::unexpected(std::move(textures.error()));
  auto texture = textures->get().loadTexture(texName);
  if (!texture)
    return std::unexpected(std::move(texture.error()));
  target->get().immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(), *texture,
      screenCoords.min(), texCoords.min(),
      Vec2F(screenCoords.xMax(), screenCoords.yMin()), Vec2F(texCoords.xMax(), texCoords.yMin()),
      screenCoords.max(), texCoords.max(),
      Vec2F(screenCoords.xMin(), screenCoords.yMax()), Vec2F(texCoords.xMin(), texCoords.yMax()),
      color, 0.0f);
  return {};
}

std::expected<void, StarException> GuiContext::drawDrawable(Drawable drawable, Vec2F const& screenPos, float pixelRatio, Vec4B const& color) {
  if (!m_drawablePainter)
    return std::unexpected(GuiContextException("GuiContext::drawDrawable() called before renderInit"));
  if (drawable.isLine())
    drawable.linePart().width *= pixelRatio;
  drawable.scale(pixelRatio);
  drawable.translate(screenPos);
  drawable.color *= Color::rgba(color);
  return m_drawablePainter->drawDrawable(drawable);
}

std::expected<void, StarException> GuiContext::drawLine(Vec2F const& begin, Vec2F const end, Vec4B const& color, float lineWidth) {
  auto target = renderer();
  if (!target)
    return std::unexpected(std::move(target.error()));
  Vec2F left = vnorm(Vec2F(end) - Vec2F(begin)).rot90() * lineWidth / 2.0f;
  target->get().immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(), begin + left, begin - left, end - left, end + left, color, 0.0f);
  return {};
}

std::expected<void, StarException> GuiContext::drawPolyLines(PolyF const& poly, Vec4B const& color, float lineWidth) {
  for (std::size_t i = 0; i < poly.sides(); ++i) {
    auto drawn = drawLine(poly.vertex(i), poly.vertex(i + 1), color, lineWidth);
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  }
  return {};
}

std::expected<void, StarException> GuiContext::drawTriangles(List<std::tuple<Vec2F, Vec2F, Vec2F>> const& triangles, Vec4B const& color) {
  auto target = renderer();
  if (!target)
    return std::unexpected(std::move(target.error()));
  for (auto const& poly : triangles)
    target->get().immediatePrimitives().emplace_back(std::in_place_type_t<RenderTriangle>(), std::get<0>(poly), std::get<1>(poly), std::get<2>(poly), color, 0.0f);
  return {};
}

std::expected<void, StarException> GuiContext::drawInterfaceDrawable(Drawable drawable, Vec2F const& screenPos, Vec4B const& color) {
  return drawDrawable(std::move(drawable), screenPos * interfaceScale(), interfaceScale(), color);
}

std::expected<void, StarException> GuiContext::drawInterfaceLine(Vec2F const& begin, Vec2F const end, Vec4B const& color, float lineWidth) {
  return drawLine(begin * interfaceScale(), end * interfaceScale(), color, lineWidth * interfaceScale());
}

std::expected<void, StarException> GuiContext::drawInterfacePolyLines(PolyF poly, Vec4B const& color, float lineWidth) {
  poly.scale(interfaceScale());
  return drawPolyLines(poly, color, lineWidth * interfaceScale());
}

std::expected<void, StarException> GuiContext::drawInterfaceQuad(RectF const& screenCoords, Vec4B const& color) {
  return drawQuad(screenCoords.scaled(interfaceScale()), color);
}

std::expected<void, StarException> GuiContext::drawInterfaceQuad(AssetPath const& texName, Vec2F const& screenCoords, Vec4B const& color) {
  return drawQuad(texName, screenCoords * interfaceScale(), interfaceScale(), color);
}

std::expected<void, StarException> GuiContext::drawInterfaceQuad(AssetPath const& texName, Vec2F const& screenCoords, float scale, Vec4B const& color) {
  return drawQuad(texName, screenCoords * interfaceScale(), interfaceScale() * scale, color);
}

std::expected<void, StarException> GuiContext::drawInterfaceQuad(AssetPath const& texName, RectF const& texCoords, RectF const& screenCoords, Vec4B const& color) {
  return drawQuad(texName, texCoords, screenCoords.scaled(interfaceScale()), color);
}

std::expected<void, StarException> GuiContext::drawInterfaceTriangles(List<std::tuple<Vec2F, Vec2F, Vec2F>> const& triangles, Vec4B const& color) {
  return drawTriangles(triangles.transformed([this](std::tuple<Vec2F, Vec2F, Vec2F> const& poly) {
    return std::tuple<Vec2F, Vec2F, Vec2F>(std::get<0>(poly) * interfaceScale(), std::get<1>(poly) * interfaceScale(), std::get<2>(poly) * interfaceScale());
  }), color);
}

std::expected<void, StarException> GuiContext::drawImageStretchSet(ImageStretchSet const& imageSet, RectF const& screenPos, GuiDirection direction, Vec4B const& color) {
  Vec2F beginSize, endSize;
  if (!imageSet.begin.empty()) {
    auto size = textureSize(imageSet.begin);
    if (!size)
      return std::unexpected(std::move(size.error()));
    beginSize = Vec2F(*size);
  }
  if (!imageSet.end.empty()) {
    auto size = textureSize(imageSet.end);
    if (!size)
      return std::unexpected(std::move(size.error()));
    endSize = Vec2F(*size);
  }
  auto innerSizeResult = textureSize(imageSet.inner);
  if (!innerSizeResult)
    return std::unexpected(std::move(innerSizeResult.error()));
  Vec2F texSize(*innerSizeResult);
  bool horizontal = direction == GuiDirection::Horizontal;
  unsigned axis = horizontal ? 0 : 1;
  int offset = beginSize[axis];
  int length = std::max(0, int(screenPos.size()[axis]) - offset - int(endSize[axis]));
  RectF begin = RectF::withSize(screenPos.min(), horizontal ? Vec2F(offset, screenPos.height()) : Vec2F(screenPos.width(), offset));
  RectF inner = RectF::withSize(screenPos.min() + (horizontal ? Vec2F(offset, 0) : Vec2F(0, offset)),
      horizontal ? Vec2F(length, screenPos.height()) : Vec2F(screenPos.width(), length));
  RectF end = RectF::withSize(screenPos.min() + (horizontal ? Vec2F(offset + length, 0) : Vec2F(0, offset + length)),
      horizontal ? Vec2F(endSize[0], screenPos.height()) : Vec2F(screenPos.width(), endSize[1]));
  if (!imageSet.begin.empty()) {
    auto drawn = drawInterfaceQuad(imageSet.begin, RectF(Vec2F(), beginSize), begin, color);
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  }
  if (imageSet.type == ImageStretchSet::ImageStretchType::Stretch) {
    auto drawn = drawInterfaceQuad(imageSet.inner, RectF(Vec2F(), texSize), inner, color);
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  } else {
    if (texSize[axis] <= 0)
      return std::unexpected(GuiContextException("Cannot repeat an empty stretch-set texture"));
    int position = 0;
    while (position < length) {
      float partLength = std::min(float(length - position), texSize[axis]);
      RectF partial = horizontal ? RectF::withSize(Vec2F(), Vec2F(partLength, texSize[1]))
          : RectF::withSize(Vec2F(0, std::max(0.0f, texSize[1] - (length - position))), Vec2F(texSize[0], partLength));
      auto drawn = drawInterfaceQuad(imageSet.inner, partial,
          RectF::withSize(inner.min() + (horizontal ? Vec2F(position, 0) : Vec2F(0, position)), partial.size()), color);
      if (!drawn)
        return std::unexpected(std::move(drawn.error()));
      position += partLength;
    }
  }
  if (!imageSet.end.empty())
    return drawInterfaceQuad(imageSet.end, RectF(Vec2F(), endSize), end, color);
  return {};
}

std::expected<bool, StarException> GuiContext::trySetCursor(Drawable const& drawable, Vec2I const& offset, int pixelRatio) {
  if (!drawable.isImage())
    return false;
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto const& imagePath = drawable.imagePart().image;
  auto image = (*assets)->image(imagePath);
  if (!image)
    return std::unexpected(std::move(image.error()));
  return applicationController()->setCursorImage(AssetPath::join(imagePath), *image, pixelRatio, offset);
}

std::expected<RectF, StarException> GuiContext::renderText(String const& s, TextPositioning const& position) {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  return painter->get().renderText(s, position);
}

std::expected<RectF, StarException> GuiContext::renderInterfaceText(String const& s, TextPositioning const& position) {
  auto res = renderText(s, {position.pos * interfaceScale(), position.hAnchor, position.vAnchor,
      position.wrapWidth.transform(std::bind(std::multiplies<int>(), std::placeholders::_1, interfaceScale())), position.charLimit});
  if (!res)
    return std::unexpected(std::move(res.error()));
  return res->scaled(1.0f / interfaceScale());
}

std::expected<RectF, StarException> GuiContext::determineTextSize(String const& s, TextPositioning const& positioning) {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  return painter->get().determineTextSize(s, positioning);
}

std::expected<RectF, StarException> GuiContext::determineInterfaceTextSize(String const& s, TextPositioning const& positioning) {
  auto res = determineTextSize(s, {positioning.pos * interfaceScale(), positioning.hAnchor, positioning.vAnchor,
      positioning.wrapWidth.transform(std::bind(std::multiplies<int>(), std::placeholders::_1, interfaceScale())), positioning.charLimit});
  if (!res)
    return std::unexpected(std::move(res.error()));
  return res->scaled(1.0f / interfaceScale());
}

std::expected<void, StarException> GuiContext::setFontSize(unsigned size) {
  return setFontSize(size, interfaceScale());
}

std::expected<void, StarException> GuiContext::setFontSize(unsigned size, float pixelRatio) {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  painter->get().setFontSize(size * pixelRatio);
  return {};
}

std::expected<void, StarException> GuiContext::setFontColor(Vec4B const& color) {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  painter->get().setFontColor(color);
  return {};
}

std::expected<void, StarException> GuiContext::setFontMode(FontMode mode) {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  painter->get().setMode(mode);
  return {};
}

std::expected<void, StarException> GuiContext::setFontProcessingDirectives(String const& directives) {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  return painter->get().setProcessingDirectives(directives);
}

std::expected<void, StarException> GuiContext::setFont(String const& font) {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  return painter->get().setFont(font);
}

std::expected<void, StarException> GuiContext::setDefaultFont() {
  return setFont("");
}

std::expected<std::reference_wrapper<TextStyle>, StarException> GuiContext::setTextStyle(TextStyle const& textStyle, float pixelRatio) {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  auto style = painter->get().setTextStyle(textStyle);
  if (!style)
    return std::unexpected(std::move(style.error()));
  style->get().fontSize *= pixelRatio;
  return *style;
}

std::expected<std::reference_wrapper<TextStyle>, StarException> GuiContext::setTextStyle(TextStyle const& textStyle) {
  return setTextStyle(textStyle, interfaceScale());
}

std::expected<void, StarException> GuiContext::clearTextStyle() {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  return painter->get().clearTextStyle();
}

std::expected<void, StarException> GuiContext::setLineSpacing(float lineSpacing) {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  painter->get().setLineSpacing(lineSpacing);
  return {};
}

std::expected<void, StarException> GuiContext::setDefaultLineSpacing() {
  return setLineSpacing(DefaultLineSpacing);
}

std::expected<int, StarException> GuiContext::stringWidth(String const& s) {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  return painter->get().stringWidth(s);
}

std::expected<int, StarException> GuiContext::stringInterfaceWidth(String const& s) {
  if (interfaceScale() == 0)
    return 0;
  auto width = stringWidth(s);
  if (!width)
    return std::unexpected(std::move(width.error()));
  return *width / interfaceScale();
}

std::expected<StringList, StarException> GuiContext::wrapText(String const& s, std::optional<unsigned> wrapWidth) {
  auto painter = textPainter();
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  return painter->get().wrapText(s, wrapWidth);
}

std::expected<StringList, StarException> GuiContext::wrapInterfaceText(String const& s, std::optional<unsigned> wrapWidth) {
  if (wrapWidth)
    *wrapWidth *= interfaceScale();
  return wrapText(s, wrapWidth);
}

bool GuiContext::shiftHeld() const {
  return m_shiftHeld;
}

void GuiContext::setShiftHeld(bool held) {
  m_shiftHeld = held;
}

std::expected<void, StarException> GuiContext::playAudio(std::shared_ptr<AudioInstance> audioInstance) {
  return m_mixer->play(std::move(audioInstance));
}

std::expected<std::shared_ptr<AudioInstance>, StarException> GuiContext::playAudio(String const& audioAsset, int loops, float volume, float pitch) {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto audio = (*assets)->audio(audioAsset);
  if (!audio)
    return std::unexpected(std::move(audio.error()));
  auto audioInstance = AudioInstance::create(**audio);
  if (!audioInstance)
    return std::unexpected(std::move(audioInstance.error()));
  (*audioInstance)->setVolume(volume);
  (*audioInstance)->setPitchMultiplier(pitch);
  (*audioInstance)->setLoops(loops);
  auto played = m_mixer->play(*audioInstance);
  if (!played)
    return std::unexpected(std::move(played.error()));
  return std::move(*audioInstance);
}

String GuiContext::getClipboard() const {
  return m_applicationController->getClipboard().value_or(String{});
}

bool GuiContext::setClipboard(String text) {
  return m_applicationController->setClipboard(std::move(text));
}

bool GuiContext::setClipboardData(StringMap<ByteArray> data) {
  return m_applicationController->setClipboardData(std::move(data));
}

std::expected<bool, StarException> GuiContext::setClipboardImage(Image const& image, std::optional<std::reference_wrapper<ByteArray>> png, std::optional<std::reference_wrapper<String const>> path) {
  return m_applicationController->setClipboardImage(image, png, path);
}

std::expected<bool, StarException> GuiContext::setClipboardFile(String const& path) {
  return m_applicationController->setClipboardFile(path);
}

float GuiContext::getDisplayScale() const {
  return m_applicationController->getDisplayScale();
}

std::expected<void, StarException> GuiContext::cleanup() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/rendering.config:textureTimeout");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto timeout = config->toInt();
  if (!timeout)
    return std::unexpected(std::move(timeout.error()));
  if (m_textureCollection)
    m_textureCollection->cleanup(*timeout);
  if (m_textPainter)
    m_textPainter->cleanup(*timeout);
  return {};
}

}
