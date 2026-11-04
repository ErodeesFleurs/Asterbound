import std;
import star.format;
import star.unicode;
import star.optional;
import star.memory;
import star.json;
import star.json_extra;
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
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.version;
import star.string;
import star.vector;
import star.text;

import star.image;
import star.renderer;
import star.font_texture_group;
import star.anchor_types;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.text_painter;

import star.math_common;
import star.pythonic;
namespace Star {

namespace {

std::expected<void, StarException> applyMeasurementFontCommands(FontTextureGroup& fontTextureGroup, String& font,
                                                                String& setFont, std::string_view remaining) {
  while (!remaining.empty()) {
    auto comma = remaining.find(',');
    auto command = remaining.substr(0, comma);
    std::expected<void, StarException> changed;
    if (command == "reset")
      changed = fontTextureGroup.switchFont(font = setFont);
    else if (command == "set")
      setFont = font;
    else if (command.starts_with("font="))
      changed = fontTextureGroup.switchFont(font = String(command.substr(5)));
    if (!changed)
      return std::unexpected(std::move(changed.error()));
    if (comma == std::string_view::npos)
      break;
    remaining.remove_prefix(comma + 1);
  }
  return {};
}

}// namespace

TextPositioning::TextPositioning() {
  pos = Vec2F();
  hAnchor = HorizontalAnchor::LeftAnchor;
  vAnchor = VerticalAnchor::BottomAnchor;
}

TextPositioning::TextPositioning(Vec2F pos, HorizontalAnchor hAnchor, VerticalAnchor vAnchor,
    std::optional<unsigned> wrapWidth, std::optional<unsigned> charLimit)
  : pos(pos), hAnchor(hAnchor), vAnchor(vAnchor), wrapWidth(wrapWidth), charLimit(charLimit) {}

std::expected<TextPositioning, StarException> TextPositioning::create(Json const& v) {
  if (!HorizontalAnchorNames)
    return std::unexpected(HorizontalAnchorNames.error());
  if (!VerticalAnchorNames)
    return std::unexpected(VerticalAnchorNames.error());
  TextPositioning result;
  auto pos = v.opt("position");
  if (!pos)
    return std::unexpected(std::move(pos.error()));
  if (*pos) {
    auto parsed = jsonToVec2F(**pos);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    result.pos = *parsed;
  }
  auto horizontal = v.getString("horizontalAnchor", "left");
  if (!horizontal)
    return std::unexpected(std::move(horizontal.error()));
  auto hAnchor = HorizontalAnchorNames->getLeft(*horizontal);
  if (!hAnchor)
    return std::unexpected(std::move(hAnchor.error()));
  result.hAnchor = hAnchor->get();
  auto vertical = v.getString("verticalAnchor", "top");
  if (!vertical)
    return std::unexpected(std::move(vertical.error()));
  auto vAnchor = VerticalAnchorNames->getLeft(*vertical);
  if (!vAnchor)
    return std::unexpected(std::move(vAnchor.error()));
  result.vAnchor = vAnchor->get();
  auto wrap = v.optUInt("wrapWidth");
  if (!wrap)
    return std::unexpected(std::move(wrap.error()));
  result.wrapWidth = *wrap;
  auto limit = v.optUInt("charLimit");
  if (!limit)
    return std::unexpected(std::move(limit.error()));
  result.charLimit = *limit;
  return result;
}

std::expected<Json, StarException> TextPositioning::toJson() const {
  if (!HorizontalAnchorNames)
    return std::unexpected(HorizontalAnchorNames.error());
  if (!VerticalAnchorNames)
    return std::unexpected(VerticalAnchorNames.error());
  auto horizontal = HorizontalAnchorNames->getRight(hAnchor);
  if (!horizontal)
    return std::unexpected(std::move(horizontal.error()));
  auto vertical = VerticalAnchorNames->getRight(vAnchor);
  if (!vertical)
    return std::unexpected(std::move(vertical.error()));
  return JsonObject{
    {"position", jsonFromVec2F(pos)},
    {"horizontalAnchor", horizontal->get()},
    {"verticalAnchor", vertical->get()},
    {"wrapWidth", jsonFromOptional(wrapWidth)}
  };
}

TextPositioning TextPositioning::translated(Vec2F translation) const {
  return {pos + translation, hAnchor, vAnchor, wrapWidth, charLimit};
}

TextPainter::TextPainter(Root& root, std::shared_ptr<Renderer> renderer, std::shared_ptr<TextureGroup> textureGroup)
  : m_root(root),
    m_renderer(renderer),
    m_fontTextureGroup(root, textureGroup),
    m_defaultRenderSettings(),
    m_renderSettings(),
    m_savedRenderSettings() {
}

std::expected<std::shared_ptr<TextPainter>, StarException> TextPainter::create(Root& root, std::shared_ptr<Renderer> renderer, std::shared_ptr<TextureGroup> textureGroup) {
  auto painter = std::shared_ptr<TextPainter>(new TextPainter(root, std::move(renderer), std::move(textureGroup)));
  auto loaded = painter->reloadFonts();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  painter->m_reloadTracker = std::make_shared<TrackerListener>();
  painter->m_reloadSubscription = root.registerReloadListener(painter->m_reloadTracker);
  return painter;
}

std::expected<RectF, StarException> TextPainter::renderText(StringView s, TextPositioning const& position) {
  auto charLimit = position.charLimit;
  auto rect = doRenderText(s, position, true, optionalRef(charLimit));
  if (!rect)
    return std::unexpected(std::move(rect.error()));
  renderPrimitives();
  return rect;
}

std::expected<RectF, StarException> TextPainter::renderLine(StringView s, TextPositioning const& position) {
  auto charLimit = position.charLimit;
  auto rect = doRenderLine(s, position, true, optionalRef(charLimit));
  if (!rect)
    return std::unexpected(std::move(rect.error()));
  renderPrimitives();
  return rect;
}

std::expected<RectF, StarException> TextPainter::renderGlyph(String::Char c, TextPositioning const& position) {
  auto rect = doRenderGlyph(c, position, true);
  if (!rect)
    return std::unexpected(std::move(rect.error()));
  renderPrimitives();
  return rect;
}

std::expected<RectF, StarException> TextPainter::determineTextSize(StringView s, TextPositioning const& position) {
  auto charLimit = position.charLimit;
  return doRenderText(s, position, false, optionalRef(charLimit));
}

std::expected<RectF, StarException> TextPainter::determineLineSize(StringView s, TextPositioning const& position) {
  auto charLimit = position.charLimit;
  return doRenderLine(s, position, false, optionalRef(charLimit));
}

std::expected<RectF, StarException> TextPainter::determineGlyphSize(String::Char c, TextPositioning const& position) {
  return doRenderGlyph(c, position, false);
}

std::expected<int, StarException> TextPainter::glyphWidth(String::Char c) {
  auto width = m_fontTextureGroup.glyphWidth(c, m_renderSettings.fontSize);
  if (!width)
    return std::unexpected(std::move(width.error()));
  return static_cast<int>(*width);
}

std::expected<int, StarException> TextPainter::stringWidth(StringView s, unsigned charLimit) {
  if (s.empty())
    return 0;
  String font = m_renderSettings.font, setFont = font;
  auto switched = m_fontTextureGroup.switchFont(font);
  if (!switched)
    return std::unexpected(std::move(switched.error()));
  Text::CommandsCallback commandsCallback = [&](StringView commands) -> std::expected<bool, StarException> {
    if (auto changed = applyMeasurementFontCommands(m_fontTextureGroup, font, setFont, commands.utf8()); !changed)
      return std::unexpected(std::move(changed.error()));
    return true;
  };
  int width = 0;
  Text::TextCallback textCallback = [&](StringView text) -> std::expected<bool, StarException> {
    Utf8Cursor cursor(text.utf8());
    while (!cursor.atEnd()) {
      auto character = cursor.read();
      if (!character)
        return std::unexpected(std::move(character.error()));
      auto characterWidth = glyphWidth(*character);
      if (!characterWidth)
        return std::unexpected(std::move(characterWidth.error()));
      width += *characterWidth;
      if (charLimit && --charLimit == 0)
        return false;
      auto advanced = cursor.advance();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    }
    return true;
  };
  auto processed = Text::processText(s, textCallback, commandsCallback);
  if (!processed)
    return std::unexpected(std::move(processed.error()));
  return width;
}

std::expected<bool, StarException> TextPainter::processWrapText(StringView text, std::optional<std::reference_wrapper<unsigned>> wrapWidth, WrapTextCallback textFunc) {
  if (!textFunc)
    return std::unexpected(StarException("Cannot wrap text with an empty callback"));
  String font = m_renderSettings.font, setFont = font;
  auto switched = m_fontTextureGroup.switchFont(font);
  if (!switched)
    return std::unexpected(std::move(switched.error()));
  Utf8Cursor cursor(text.utf8());
  std::size_t cursorBase = 0;
  std::size_t iterator = 0, end = text.utf8Size();

  unsigned lines = 0;
  auto lineStartIterator = iterator, splitIterator = end;
  unsigned linePixelWidth = 0, splitPixelWidth = 0;
  std::size_t commandStart = NPos, commandEnd = NPos;
  bool finished = true;

  auto slice = [&](std::size_t a, std::size_t b) -> StringView {
    return StringView(text.utf8().substr(a, b - a));
  };

  while (iterator != end) {
    auto decoded = cursor.read();
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    auto character = *decoded;
    finished = false; // assume at least one character if we get here
    bool noMoreCommands = commandStart != NPos && commandEnd == NPos;
    if (!noMoreCommands && Text::isEscapeCode(character)) {
      std::size_t index = iterator;
      if (commandStart == NPos) {
        for (std::size_t escOrEnd = commandStart = index;
        (escOrEnd = text.utf8().find_first_of(Text::AllEscEnd, escOrEnd + 1)) != NPos;) {
          if (text.utf8().at(escOrEnd) != Text::EndEsc)
            commandStart = escOrEnd;
          else {
            commandEnd = escOrEnd;
            break;
          }
        }
      }
      if (commandStart == index && commandEnd != NPos) {
        if (auto changed = applyMeasurementFontCommands(
              m_fontTextureGroup, font, setFont, text.utf8().substr(commandStart + 1, commandEnd - commandStart - 1));
            !changed)
          return std::unexpected(std::move(changed.error()));
        iterator = cursorBase = commandEnd + 1;
        cursor = Utf8Cursor(text.utf8().substr(cursorBase));
        commandStart = commandEnd = NPos;
        continue;
      }
    }
    // is this a linefeed / cr / whatever that forces a line split ?
    if (character == '\n' || character == '\v') {
      // knock one off the end because we don't render the CR
      auto continued = textFunc(slice(lineStartIterator, iterator), lines++);
      if (!continued)
        return std::unexpected(std::move(continued.error()));
      if (!*continued)
        return false;

      lineStartIterator = iterator;
      ++lineStartIterator;
      // next line starts after the CR with no characters in it and no known splits.
      linePixelWidth = 0;
      splitIterator = end;
      finished = true;
    } else {
      auto measured = glyphWidth(character);
      if (!measured)
        return std::unexpected(std::move(measured.error()));
      int characterWidth = *measured;
      // is it a place where we might want to split the line ?
      if (character == ' ' || character == '\t') {
        splitIterator = iterator;
        splitPixelWidth = linePixelWidth + characterWidth;
      }

      // would the line be too long if we render this next character ?
      if (wrapWidth && (linePixelWidth + characterWidth) > wrapWidth->get()) {
        // did we find somewhere to split the line ?
        if (splitIterator != end) {
          auto continued = textFunc(slice(lineStartIterator, splitIterator), lines++);
          if (!continued)
            return std::unexpected(std::move(continued.error()));
          if (!*continued)
            return false;
          // do not include the split character on the next line
          unsigned stringWidth = linePixelWidth - splitPixelWidth;
          linePixelWidth = stringWidth + characterWidth;
          lineStartIterator = ++splitIterator;
          splitIterator = end;
        } else {
          auto continued = textFunc(slice(lineStartIterator, iterator), lines++);
          if (!continued)
            return std::unexpected(std::move(continued.error()));
          if (!*continued)
            return false;
          // include that character on the next line
          lineStartIterator = iterator;  
          linePixelWidth = characterWidth;
          finished = false;
        }
      } else {
        linePixelWidth += characterWidth;
      }
    }

    auto advanced = cursor.advance();
    if (!advanced)
      return std::unexpected(std::move(advanced.error()));
    iterator = cursorBase + cursor.offset();
  };

  // if we hit the end of the string before hitting the end of the line
  if (finished)
    return true;
  return textFunc(slice(lineStartIterator, end), lines);
}

std::expected<List<StringView>, StarException> TextPainter::wrapTextViews(StringView s, std::optional<unsigned> wrapWidth) {
  List<StringView> views = {};
  auto last = views.end();
  unsigned curLine = 0;
  TextPainter::WrapTextCallback textCallback = [&](StringView text, unsigned line) {
    if (line == curLine && last != views.end() && last->utf8().end() == text.utf8().begin()) {
      *last = StringView(std::string_view(last->utf8().begin(), text.utf8().end()));
    } else {
      last = views.insert(views.end(), text);
      curLine = line;
    }
    return true;
  };

  auto wrapped = processWrapText(s, optionalRef(wrapWidth), textCallback);
  if (!wrapped)
    return std::unexpected(std::move(wrapped.error()));

  return views;
}

std::expected<StringList, StarException> TextPainter::wrapText(StringView s, std::optional<unsigned> wrapWidth) {
  StringList result;

  String current;
  unsigned lastLine = 0;
  TextPainter::WrapTextCallback textCallback = [&](StringView text, unsigned line) {
    if (lastLine != line) {
      result.append(std::move(current));
      lastLine = line;
    }
    current += text;
    return true;
  };

  auto wrapped = processWrapText(s, optionalRef(wrapWidth), textCallback);
  if (!wrapped)
    return std::unexpected(std::move(wrapped.error()));

  if (!current.empty())
    result.append(std::move(current));

  return result;
};

unsigned TextPainter::fontSize() const {
  return m_renderSettings.fontSize;
}

void TextPainter::setFontSize(unsigned size) {
  m_renderSettings.fontSize = size;
}

void TextPainter::setLineSpacing(float lineSpacing) {
  m_renderSettings.lineSpacing = lineSpacing;
}

void TextPainter::setMode(FontMode mode) {
  m_renderSettings.shadow = fontModeToColor(mode).toRgba();
}

void TextPainter::setFontColor(Vec4B color) {
  m_renderSettings.color = std::move(color);
}

std::expected<void, StarException> TextPainter::setProcessingDirectives(StringView directives, bool back) {
  Directives& target = back ? m_renderSettings.backDirectives : m_renderSettings.directives;
  return modifyDirectives(target = String(directives));
}

std::expected<void, StarException> TextPainter::setFont(String const& font) {
  auto switched = m_fontTextureGroup.switchFont(font);
  if (!switched)
    return std::unexpected(std::move(switched.error()));
  m_renderSettings.font = font;
  return {};
}

std::expected<std::reference_wrapper<TextStyle>, StarException> TextPainter::setTextStyle(TextStyle const& textStyle) {
  TextStyle& style = m_renderSettings = textStyle;
  auto front = modifyDirectives(style.directives);
  if (!front)
    return std::unexpected(std::move(front.error()));
  auto back = modifyDirectives(style.backDirectives);
  if (!back)
    return std::unexpected(std::move(back.error()));
  auto switched = m_fontTextureGroup.switchFont(style.font);
  if (!switched)
    return std::unexpected(std::move(switched.error()));
  return std::ref(style);
}

std::expected<void, StarException> TextPainter::clearTextStyle() {
  m_renderSettings = m_defaultRenderSettings;
  return m_fontTextureGroup.switchFont(m_renderSettings.font);
}

std::expected<void, StarException> TextPainter::addFont(std::shared_ptr<Font> const& font, String const& name) {
  return m_fontTextureGroup.addFont(font, name);
}

std::expected<void, StarException> TextPainter::reloadFonts() {
  auto assetsResult = m_root.assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = *assetsResult;
  List<std::pair<std::shared_ptr<Font>, String>> fonts;
  auto loadFontsByExtension = [&](String const& ext) -> std::expected<void, StarException> {
    auto paths = assets->scanExtension(ext);
    if (!paths)
      return std::unexpected(std::move(paths.error()));
    for (auto const& assetKey : paths->get()) {
      auto const& fontPath = assetKey.string();
      auto filename = AssetPath::filename(fontPath);
      auto const& bytes = filename.utf8();
      String name(bytes.substr(0, bytes.find_last_of('.')));
      auto loaded = loadFont(fontPath, name);
      if (!loaded)
        return std::unexpected(std::move(loaded.error()));
      fonts.emplace_back(std::move(*loaded), std::move(name));
    }
    return {};
  };
  auto ttf = loadFontsByExtension("ttf");
  if (!ttf)
    return std::unexpected(std::move(ttf.error()));
  auto woff = loadFontsByExtension("woff2");
  if (!woff)
    return std::unexpected(std::move(woff.error()));
  auto fontConfig = assets->json("/interface.config:font");
  if (!fontConfig)
    return std::unexpected(std::move(fontConfig.error()));
  auto defaultFont = fontConfig->getString("defaultFont");
  if (!defaultFont)
    return std::unexpected(std::move(defaultFont.error()));
  auto fallbackFont = fontConfig->getString("fallbackFont");
  if (!fallbackFont)
    return std::unexpected(std::move(fallbackFont.error()));
  auto emojiFont = fontConfig->getString("emojiFont");
  if (!emojiFont)
    return std::unexpected(std::move(emojiFont.error()));
  m_fontTextureGroup.clearFonts();
  m_fontTextureGroup.cleanup(0);
  for (auto const& [font, name] : fonts) {
    auto added = addFont(font, name);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return m_fontTextureGroup.setFixedFonts(*defaultFont, *fallbackFont, *emojiFont);
}

void TextPainter::cleanup(std::int64_t timeout) {
  m_fontTextureGroup.cleanup(timeout);
}

std::expected<void, StarException> TextPainter::applyCommands(StringView unsplitCommands) {
  auto remaining = unsplitCommands.utf8();
  while (!remaining.empty()) {
    auto comma = remaining.find(',');
    auto command = remaining.substr(0, comma);
    auto apply = [&]() -> std::expected<void, StarException> {
      if (command == "reset") {
        m_renderSettings = m_savedRenderSettings;
        return m_fontTextureGroup.switchFont(m_renderSettings.font);
      } else if (command == "set") {
        m_savedRenderSettings = m_renderSettings;
      } else if (command.starts_with("shadow")) {
        if (command.size() == 6) {
          m_renderSettings.shadow = Color::Black.toRgba();
        } else if (command[6] == '=') {
          auto color = Color::create(StringView(command.substr(7)));
          if (!color)
            return std::unexpected(std::move(color.error()));
          m_renderSettings.shadow = color->toRgba();
        }
      } else if (command == "noshadow") {
        m_renderSettings.shadow = Color::Clear.toRgba();
      } else if (command.starts_with("font=")) {
        return setFont(String(command.substr(5)));
      } else if (command.starts_with("directives=")) {
        return setProcessingDirectives(StringView(command.substr(11)), false);
      } else if (command.starts_with("backdirectives=")) {
        return setProcessingDirectives(StringView(command.substr(15)), true);
      } else {
        auto color = Color::create(StringView(command));
        if (!color)
          return std::unexpected(std::move(color.error()));
        color->setAlphaF(color->alphaF() * ((float)m_savedRenderSettings.color[3]) / 255);
        m_renderSettings.color = color->toRgba();
      }
      return {};
    };
    auto applied = apply();
    if (!applied && !applied.error().is<JsonException>() && !applied.error().is<ColorException>())
      return std::unexpected(std::move(applied.error()));
    if (comma == std::string_view::npos)
      break;
    remaining.remove_prefix(comma + 1);
  }
  return {};
}

std::expected<void, StarException> TextPainter::modifyDirectives(Directives& directives) {
  if (directives) {
    directives.loadOperations();
    auto shared = directives.get();
    if (!shared)
      return std::unexpected(std::move(shared.error()));
    for (auto& entry : shared->get().entries) {
      if (std::holds_alternative<BorderImageOperation>(entry.operation))
        std::get<BorderImageOperation>(entry.operation).includeTransparent = true;
    }
  }
  return {};
}

std::expected<RectF, StarException> TextPainter::doRenderText(StringView s, TextPositioning const& position, bool reallyRender, std::optional<std::reference_wrapper<unsigned>> charLimit) {
  Vec2F pos = position.pos;
  if (s.empty())
    return RectF(pos, pos);

  auto wrapped = wrapTextViews(s, position.wrapWidth);
  if (!wrapped)
    return std::unexpected(std::move(wrapped.error()));
  auto& lines = *wrapped;

  TextStyle backup = m_savedRenderSettings = m_renderSettings;
  int height = (lines.size() - 1) * backup.lineSpacing * backup.fontSize + backup.fontSize;
  if (position.vAnchor == VerticalAnchor::BottomAnchor)
    pos[1] += (height - backup.fontSize);
  else if (position.vAnchor == VerticalAnchor::VMidAnchor)
    pos[1] += (height - backup.fontSize) / 2;

  RectF bounds = RectF::withSize(pos, Vec2F());
  for (auto& i : lines) {
    auto line = doRenderLine(i, { pos, position.hAnchor, position.vAnchor, {}, {} }, reallyRender, charLimit);
    if (!line) {
      m_renderSettings = std::move(backup);
      auto restored = m_fontTextureGroup.switchFont(m_renderSettings.font);
      if (!restored)
        return std::unexpected(restored.error().withContext(line.error().what()));
      return std::unexpected(std::move(line.error()));
    }
    bounds.combine(*line);
    pos[1] -= m_renderSettings.fontSize * m_renderSettings.lineSpacing;

    if (charLimit && charLimit->get() == 0)
      break;
  }

  m_renderSettings = std::move(backup);
  auto restored = m_fontTextureGroup.switchFont(m_renderSettings.font);
  if (!restored)
    return std::unexpected(std::move(restored.error()));

  return bounds;
}

std::expected<RectF, StarException> TextPainter::doRenderLine(StringView text, TextPositioning const& position, bool reallyRender, std::optional<std::reference_wrapper<unsigned>> charLimit) {
  if (m_reloadTracker->pullTriggered()) {
    auto reloaded = reloadFonts();
    if (!reloaded)
      return std::unexpected(std::move(reloaded.error()));
  }
  TextPositioning pos = position;
  if (pos.hAnchor == HorizontalAnchor::RightAnchor || pos.hAnchor == HorizontalAnchor::HMidAnchor) {
    StringView trimmedString = text;
    if (charLimit) {
      auto trimmed = text.substr(0, charLimit->get());
      if (!trimmed)
        return std::unexpected(std::move(trimmed.error()));
      trimmedString = *trimmed;
    }
    auto measured = stringWidth(trimmedString, 0);
    if (!measured)
      return std::unexpected(std::move(measured.error()));
    pos.pos[0] -= pos.hAnchor == HorizontalAnchor::RightAnchor ? (float)*measured : std::floor((float)*measured / 2);
    pos.hAnchor = HorizontalAnchor::LeftAnchor;
  }

  String escapeCode;
  RectF bounds = RectF::withSize(pos.pos, Vec2F());
  Text::TextCallback textCallback = [&](StringView text) -> std::expected<bool, StarException> {
    Utf8Cursor cursor(text.utf8());
    while (!cursor.atEnd()) {
      auto character = cursor.read();
      if (!character)
        return std::unexpected(std::move(character.error()));
      String::Char c = *character;
      if (charLimit) {
        if (charLimit->get() == 0)
          return false;
        else
          --charLimit->get();
      }
      auto glyphBounds = doRenderGlyph(c, pos, reallyRender);
      if (!glyphBounds)
        return std::unexpected(std::move(glyphBounds.error()));
      bounds.combine(*glyphBounds);
      pos.pos[0] += glyphBounds->width();
      auto advanced = cursor.advance();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    }
    return true;
  };

  Text::CommandsCallback commandsCallback = [&](StringView commands) -> std::expected<bool, StarException> {
    auto applied = applyCommands(commands);
    if (!applied)
      return std::unexpected(std::move(applied.error()));
    return true;
  };

  auto switched = m_fontTextureGroup.switchFont(m_renderSettings.font);
  if (!switched)
    return std::unexpected(std::move(switched.error()));
  auto processed = Text::processText(text, textCallback, commandsCallback);
  if (!processed)
    return std::unexpected(std::move(processed.error()));

  return bounds;
}

std::expected<RectF, StarException> TextPainter::doRenderGlyph(String::Char c, TextPositioning const& position, bool reallyRender) {
  if (c == '\n' || c == '\v' || c == '\r')
    return RectF();

  auto measured = glyphWidth(c);
  if (!measured)
    return std::unexpected(std::move(measured.error()));
  int width = *measured;
  // Offset left by width if right anchored.
  float hOffset = 0;
  if (position.hAnchor == HorizontalAnchor::RightAnchor)
    hOffset = -width;
  else if (position.hAnchor == HorizontalAnchor::HMidAnchor)
    hOffset = -std::floor((float)width / 2);

  float vOffset = 0;
  if (position.vAnchor == VerticalAnchor::VMidAnchor)
    vOffset = -std::floor((float)m_renderSettings.fontSize / 2);
  else if (position.vAnchor == VerticalAnchor::TopAnchor)
    vOffset = -(float)m_renderSettings.fontSize;

  std::optional<std::reference_wrapper<Directives const>> directives;
  if (m_renderSettings.directives)
    directives = std::cref(m_renderSettings.directives);

  Vec2F pos = position.pos + Vec2F(hOffset, vOffset);
  if (reallyRender) {
    bool hasShadow = m_renderSettings.shadow[3] > 0;
    bool hasBackDirectives = m_renderSettings.backDirectives;
    if (hasShadow) {
      //Kae: unlike vanilla we draw only one shadow glyph instead of two, so i'm tweaking the alpha here
      Vec4B shadow = m_renderSettings.shadow;
      std::uint8_t alphaU = m_renderSettings.color[3] * byteToFloat(shadow[3]);
      if (alphaU != 255) {
        float alpha = byteToFloat(alphaU);
        shadow[3] = floatToByte(alpha * (1.5f - 0.5f * alpha));
      }
      else
        shadow[3] = alphaU;

      auto shadowDirectives = hasBackDirectives ? std::optional{std::cref(m_renderSettings.backDirectives)} : directives;
      auto rendered = renderGlyph(c, pos + Vec2F(0, -2), m_shadowPrimitives, m_renderSettings.fontSize, 1, shadow, shadowDirectives);
      if (!rendered)
        return std::unexpected(std::move(rendered.error()));
    }
    if (hasBackDirectives) {
      auto rendered = renderGlyph(c, pos, m_backPrimitives, m_renderSettings.fontSize, 1, m_renderSettings.color, std::cref(m_renderSettings.backDirectives));
      if (!rendered)
        return std::unexpected(std::move(rendered.error()));
    }

    auto& output = (hasShadow || hasBackDirectives) ? m_frontPrimitives : m_renderer->immediatePrimitives(); 
    auto rendered = renderGlyph(c, pos, output, m_renderSettings.fontSize, 1, m_renderSettings.color, directives);
    if (!rendered)
      return std::unexpected(std::move(rendered.error()));
  }

  return RectF::withSize(pos, {(float)width, (int)m_renderSettings.fontSize});
}

void TextPainter::renderPrimitives() {
  auto& destination = m_renderer->immediatePrimitives();
  std::move(std::begin(m_shadowPrimitives), std::end(m_shadowPrimitives), std::back_inserter(destination));
  m_shadowPrimitives.clear();
  std::move(std::begin(m_backPrimitives), std::end(m_backPrimitives), std::back_inserter(destination));
  m_backPrimitives.clear();
  std::move(std::begin(m_frontPrimitives), std::end(m_frontPrimitives), std::back_inserter(destination));
  m_frontPrimitives.clear();
}

std::expected<void, StarException> TextPainter::renderGlyph(String::Char c, Vec2F const& screenPos, List<RenderPrimitive>& out, unsigned fontSize,
    float scale, Vec4B color, std::optional<std::reference_wrapper<Directives const>> processingDirectives) {
  if (!fontSize)
    return {};
  auto glyph = m_fontTextureGroup.glyphTexture(c, fontSize, processingDirectives);
  if (!glyph)
    return std::unexpected(std::move(glyph.error()));
  auto const& glyphTexture = glyph->get();
  if (glyphTexture.colored)
    color[0] = color[1] = color[2] = 255;
  out.emplace_back(std::in_place_type_t<RenderQuad>(),
    glyphTexture.texture, Vec2F::round(screenPos + glyphTexture.offset * scale), scale, color, 0.0f);
  return {};
}

std::expected<std::shared_ptr<Font>, StarException> TextPainter::loadFont(String const& fontPath, std::optional<String> fontName) {
  if (!fontName) {
    auto name = AssetPath::filename(fontPath);
    fontName.emplace(name.utf8().substr(0, name.utf8().find_last_of('.')));
  }
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto source = (*assets)->font(fontPath);
  if (!source)
    return std::unexpected(std::move(source.error()));
  auto font = (*source)->clone();
  if (!font)
    return std::unexpected(std::move(font.error()));
  auto config = (*assets)->json("/interface.config:font");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto fontConfig = config->opt(*fontName);
  if (!fontConfig)
    return std::unexpected(std::move(fontConfig.error()));
  if (*fontConfig) {
    auto threshold = (**fontConfig).getUInt("alphaThreshold", 0);
    if (!threshold)
      return std::unexpected(std::move(threshold.error()));
    (*font)->setAlphaThreshold(*threshold);
  }
  return std::move(*font);
}

}
