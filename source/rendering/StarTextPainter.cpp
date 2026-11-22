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
namespace Star {

std::expected<void, StarException> TextPainter::apply_measurement_commands(MeasurementFonts& fonts,
    std::string_view remaining) const {
  while (!remaining.empty()) {
    auto comma = remaining.find(',');
    auto command = remaining.substr(0, comma);
    if (command == "reset") {
      fonts.current = fonts.saved;
    } else if (command == "set") {
      fonts.saved = fonts.current;
    } else if (command.starts_with("font=")) {
      auto selected = m_fontTextureGroup.named_font(command.substr(5));
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      fonts.current = std::move(*selected);
    }
    if (comma == std::string_view::npos)
      break;
    remaining.remove_prefix(comma + 1);
  }
  return {};
}

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
    auto parsed = json_to_vec2f(**pos);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    result.pos = *parsed;
  }
  auto horizontal = v.get_string("horizontalAnchor", "left");
  if (!horizontal)
    return std::unexpected(std::move(horizontal.error()));
  auto hAnchor = HorizontalAnchorNames->get_left(*horizontal);
  if (!hAnchor)
    return std::unexpected(std::move(hAnchor.error()));
  result.hAnchor = hAnchor->get();
  auto vertical = v.get_string("verticalAnchor", "top");
  if (!vertical)
    return std::unexpected(std::move(vertical.error()));
  auto vAnchor = VerticalAnchorNames->get_left(*vertical);
  if (!vAnchor)
    return std::unexpected(std::move(vAnchor.error()));
  result.vAnchor = vAnchor->get();
  auto wrap = v.opt_uint("wrapWidth");
  if (!wrap)
    return std::unexpected(std::move(wrap.error()));
  result.wrapWidth = *wrap;
  auto limit = v.opt_uint("charLimit");
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
  auto horizontal = HorizontalAnchorNames->get_right(hAnchor);
  if (!horizontal)
    return std::unexpected(std::move(horizontal.error()));
  auto vertical = VerticalAnchorNames->get_right(vAnchor);
  if (!vertical)
    return std::unexpected(std::move(vertical.error()));
  return JsonObject{
    {"position", json_from_vec2f(pos)},
    {"horizontalAnchor", horizontal->get()},
    {"verticalAnchor", vertical->get()},
    {"wrapWidth", json_from_optional(wrapWidth)}
  };
}

TextPositioning TextPositioning::translated(Vec2F translation) const {
  return {pos + translation, hAnchor, vAnchor, wrapWidth, charLimit};
}

TextPainter::TextPainter(Root& root, std::shared_ptr<Renderer> renderer, std::shared_ptr<TextureGroup> textureGroup)
    : m_root(root), m_renderer(std::move(renderer)), m_fontTextureGroup(root, std::move(textureGroup)),
      m_defaultRenderSettings(), m_renderSettings(), m_savedRenderSettings() {}

std::expected<std::shared_ptr<TextPainter>, StarException> TextPainter::create(Root& root, std::shared_ptr<Renderer> renderer, std::shared_ptr<TextureGroup> textureGroup) {
  auto painter = std::shared_ptr<TextPainter>(new TextPainter(root, std::move(renderer), std::move(textureGroup)));
  auto loaded = painter->reloadFonts();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  painter->m_reload_tracker = std::make_shared<TrackerListener>();
  painter->m_reload_subscription = root.subscribe_reload(painter->m_reload_tracker);
  return painter;
}

std::expected<RectF, StarException> TextPainter::renderText(StringView s, TextPositioning const& position) {
  auto charLimit = position.charLimit;
  auto rect = doRenderText(s, position, true, optional_ref(charLimit));
  if (!rect)
    return std::unexpected(std::move(rect.error()));
  renderPrimitives();
  return rect;
}

std::expected<RectF, StarException> TextPainter::renderLine(StringView s, TextPositioning const& position) {
  auto charLimit = position.charLimit;
  auto ready = refresh_fonts();
  if (!ready)
    return std::unexpected(std::move(ready.error()));
  auto rect = doRenderLine(s, position, {}, optional_ref(charLimit));
  if (!rect)
    return std::unexpected(std::move(rect.error()));
  renderPrimitives();
  return rect;
}

std::expected<RectF, StarException> TextPainter::renderGlyph(String::Char c, TextPositioning const& position) {
  auto ready = refresh_fonts();
  if (!ready)
    return std::unexpected(std::move(ready.error()));
  auto rect = doRenderGlyph(c, position, true, m_fontTextureGroup.m_active_font);
  if (!rect)
    return std::unexpected(std::move(rect.error()));
  renderPrimitives();
  return rect;
}

std::expected<RectF, StarException> TextPainter::determineTextSize(StringView s, TextPositioning const& position) {
  auto charLimit = position.charLimit;
  return doRenderText(s, position, false, optional_ref(charLimit));
}

std::expected<RectF, StarException> TextPainter::determineLineSize(StringView s, TextPositioning const& position) {
  auto charLimit = position.charLimit;
  auto ready = refresh_fonts();
  if (!ready)
    return std::unexpected(std::move(ready.error()));
  auto saved = m_fontTextureGroup.named_font(m_savedRenderSettings.font.utf8());
  if (!saved)
    return std::unexpected(std::move(saved.error()));
  MeasurementFonts fonts{m_fontTextureGroup.m_active_font, std::move(*saved)};
  return doRenderLine(s, position, std::ref(fonts), optional_ref(charLimit));
}

std::expected<RectF, StarException> TextPainter::determineGlyphSize(String::Char c, TextPositioning const& position) {
  auto ready = refresh_fonts();
  if (!ready)
    return std::unexpected(std::move(ready.error()));
  return doRenderGlyph(c, position, false, m_fontTextureGroup.m_active_font);
}

std::expected<int, StarException> TextPainter::glyphWidth(String::Char c) {
  auto ready = refresh_fonts();
  if (!ready)
    return std::unexpected(std::move(ready.error()));
  auto width = m_fontTextureGroup.glyph_width(c, m_renderSettings.font_size);
  if (!width)
    return std::unexpected(std::move(width.error()));
  return static_cast<int>(*width);
}

std::expected<int, StarException> TextPainter::stringWidth(StringView s, unsigned charLimit) {
  auto ready = refresh_fonts();
  if (!ready)
    return std::unexpected(std::move(ready.error()));
  auto const& active = m_fontTextureGroup.m_active_font;
  return string_width(s, charLimit, {active, active});
}

std::expected<int, StarException> TextPainter::string_width(StringView s, unsigned char_limit, MeasurementFonts fonts) const {
  if (s.empty())
    return 0;
  auto commands_callback = [&](StringView commands) -> std::expected<bool, StarException> {
    if (auto changed = apply_measurement_commands(fonts, commands.utf8()); !changed)
      return std::unexpected(std::move(changed.error()));
    return true;
  };
  int width = 0;
  auto text_callback = [&](StringView text) -> std::expected<bool, StarException> {
    Utf8Cursor cursor(text.utf8());
    while (!cursor.at_end()) {
      auto character = cursor.read();
      if (!character)
        return std::unexpected(std::move(character.error()));
      auto character_width = m_fontTextureGroup.glyph_width(fonts.current, *character, m_renderSettings.font_size);
      if (!character_width)
        return std::unexpected(std::move(character_width.error()));
      width += *character_width;
      if (char_limit && --char_limit == 0)
        return false;
      auto advanced = cursor.advance();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    }
    return true;
  };
  auto processed = Text::process_text(s, text_callback, commands_callback);
  if (!processed)
    return std::unexpected(std::move(processed.error()));
  return width;
}

std::expected<bool, StarException> TextPainter::processWrapText(StringView text, std::optional<std::reference_wrapper<unsigned>> wrapWidth, WrapTextCallback textFunc) {
  if (!textFunc)
    return std::unexpected(StarException("Cannot wrap text with an empty callback"));
  auto ready = refresh_fonts();
  if (!ready)
    return std::unexpected(std::move(ready.error()));
  auto const& active = m_fontTextureGroup.m_active_font;
  MeasurementFonts fonts{active, active};
  Utf8Cursor cursor(text.utf8());
  std::size_t cursorBase = 0;
  std::size_t iterator = 0, end = text.utf8_size();

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
    if (!noMoreCommands && (character == '^' || character == '\x1b')) {
      std::size_t index = iterator;
      if (commandStart == NPos) {
        auto range = Text::find_next_command(text.utf8(), index, true);
        commandStart = range ? range->begin : index;
        commandEnd = range ? range->end - 1 : NPos;
      }
      if (commandStart == index && commandEnd != NPos) {
        if (auto changed = apply_measurement_commands(
              fonts, text.utf8().substr(commandStart + 1, commandEnd - commandStart - 1));
            !changed)
          return std::unexpected(std::move(changed.error()));
        iterator = cursorBase = commandEnd + 1;
        cursor = Utf8Cursor(std::string_view(text.utf8()).substr(cursorBase));
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
      auto measured = m_fontTextureGroup.glyph_width(fonts.current, character, m_renderSettings.font_size);
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
  TextPainter::WrapTextCallback text_callback = [&](StringView text, unsigned line) {
    if (line == curLine && last != views.end() && last->utf8().end() == text.utf8().begin()) {
      *last = StringView(std::string_view(last->utf8().begin(), text.utf8().end()));
    } else {
      last = views.insert(views.end(), text);
      curLine = line;
    }
    return true;
  };

  auto wrapped = processWrapText(s, optional_ref(wrapWidth), text_callback);
  if (!wrapped)
    return std::unexpected(std::move(wrapped.error()));

  return views;
}

std::expected<StringList, StarException> TextPainter::wrapText(StringView s, std::optional<unsigned> wrapWidth) {
  StringList result;

  String current;
  unsigned lastLine = 0;
  TextPainter::WrapTextCallback text_callback = [&](StringView text, unsigned line) {
    if (lastLine != line) {
      result.push_back(std::move(current));
      lastLine = line;
    }
    current += text;
    return true;
  };

  auto wrapped = processWrapText(s, optional_ref(wrapWidth), text_callback);
  if (!wrapped)
    return std::unexpected(std::move(wrapped.error()));

  if (!current.empty())
    result.push_back(std::move(current));

  return result;
};

unsigned TextPainter::fontSize() const {
  return m_renderSettings.font_size;
}

void TextPainter::setFontSize(unsigned size) {
  m_renderSettings.font_size = size;
}

void TextPainter::setLineSpacing(float lineSpacing) {
  m_renderSettings.line_spacing = lineSpacing;
}

void TextPainter::setMode(FontMode mode) {
  m_renderSettings.shadow = fontModeToColor(mode).to_rgba();
}

void TextPainter::setFontColor(Vec4B color) {
  m_renderSettings.color = std::move(color);
}

std::expected<void, StarException> TextPainter::setProcessingDirectives(StringView directives, bool back) {
  Directives& target = back ? m_renderSettings.back_directives : m_renderSettings.directives;
  target = String(directives);
  return {};
}

std::expected<void, StarException> TextPainter::setFont(String const& font) {
  auto switched = m_fontTextureGroup.select_font(font);
  if (!switched)
    return std::unexpected(std::move(switched.error()));
  m_renderSettings.font = font;
  return {};
}

std::expected<std::reference_wrapper<TextStyle>, StarException> TextPainter::setTextStyle(TextStyle const& textStyle) {
  auto switched = m_fontTextureGroup.select_font(textStyle.font);
  if (!switched)
    return std::unexpected(std::move(switched.error()));
  m_renderSettings = textStyle;
  return std::ref(m_renderSettings);
}

std::expected<void, StarException> TextPainter::clearTextStyle() {
  return setTextStyle(m_defaultRenderSettings).transform([](std::reference_wrapper<TextStyle>) {});
}

std::expected<void, StarException> TextPainter::refresh_fonts() {
  if (m_reload_tracker->consume()) {
    auto reloaded = reloadFonts();
    if (!reloaded)
      return std::unexpected(std::move(reloaded.error()));
  }
  // setTextStyle exposes mutable settings; honor subsequent direct font edits.
  return m_fontTextureGroup.select_font(m_renderSettings.font);
}

std::expected<void, StarException> TextPainter::reloadFonts() {
  auto assets_result = m_root.assets();
  if (!assets_result)
    return std::unexpected(std::move(assets_result.error()));
  auto& assets = **assets_result;
  FontTextureGroup::FontSet fonts;
  auto config = assets.json("/interface.config:font");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto default_name = config->get_string("defaultFont");
  if (!default_name)
    return std::unexpected(std::move(default_name.error()));
  auto fallback_name = config->get_string("fallbackFont");
  if (!fallback_name)
    return std::unexpected(std::move(fallback_name.error()));
  auto emoji_name = config->get_string("emojiFont");
  if (!emoji_name)
    return std::unexpected(std::move(emoji_name.error()));
  for (auto extension : std::array<std::string_view, 2>{"ttf", "woff2"}) {
    auto paths = assets.scan_extension(extension);
    if (!paths)
      return std::unexpected(std::move(paths.error()));
    for (auto const& key : paths->get()) {
      auto const& path = key.string();
      auto bytes = AssetPath::filename(path).utf8();
      if (auto dot = bytes.find_last_of('.'); dot != std::string::npos)
        bytes.resize(dot);
      String name(std::move(bytes));
      auto source = assets.font(path);
      if (!source)
        return std::unexpected(std::move(source.error()));
      auto font = (*source)->clone();
      if (!font)
        return std::unexpected(std::move(font.error()));
      auto options = config->opt(name);
      if (!options)
        return std::unexpected(std::move(options.error()));
      if (*options) {
        auto threshold = (**options).get_uint("alphaThreshold", 0);
        if (!threshold)
          return std::unexpected(std::move(threshold.error()));
        (*font)->set_alpha_threshold(*threshold);
      }
      auto inserted = fonts.insert_or_assign(std::move(name), std::move(*font));
      if (!inserted)
        return std::unexpected(std::move(inserted.error()));
    }
  }
  return m_fontTextureGroup.replace_fonts(std::move(fonts), *default_name, *fallback_name, *emoji_name);
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
        return setTextStyle(m_savedRenderSettings).transform([](std::reference_wrapper<TextStyle>) {});
      } else if (command == "set") {
        m_savedRenderSettings = m_renderSettings;
      } else if (command.starts_with("shadow")) {
        if (command.size() == 6) {
          m_renderSettings.shadow = Color::Black.to_rgba();
        } else if (command[6] == '=') {
          auto color = Color::create(StringView(command.substr(7)));
          if (!color)
            return std::unexpected(std::move(color.error()));
          m_renderSettings.shadow = color->to_rgba();
        }
      } else if (command == "noshadow") {
        m_renderSettings.shadow = Color::Clear.to_rgba();
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
        color->set_alpha_f(color->alpha_f() * (static_cast<float>(m_savedRenderSettings.color[3])) / 255);
        m_renderSettings.color = color->to_rgba();
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


std::expected<RectF, StarException> TextPainter::doRenderText(StringView s, TextPositioning const& position, bool reallyRender, std::optional<std::reference_wrapper<unsigned>> charLimit) {
  Vec2F pos = position.pos;
  if (s.empty())
    return RectF(pos, pos);

  auto wrapped = wrapTextViews(s, position.wrapWidth);
  if (!wrapped)
    return std::unexpected(std::move(wrapped.error()));
  auto& lines = *wrapped;

  int height = (lines.size() - 1) * m_renderSettings.line_spacing * m_renderSettings.font_size + m_renderSettings.font_size;
  if (position.vAnchor == VerticalAnchor::BottomAnchor)
    pos[1] += (height - m_renderSettings.font_size);
  else if (position.vAnchor == VerticalAnchor::VMidAnchor)
    pos[1] += (height - m_renderSettings.font_size) / 2;

  auto layout = [&](MeasureOnly measure) -> std::expected<RectF, StarException> {
    RectF bounds = RectF::with_size(pos, Vec2F());
    for (auto line : lines) {
      auto result = doRenderLine(line, {pos, position.hAnchor, position.vAnchor, {}, {}}, measure, charLimit);
      if (!result)
        return std::unexpected(std::move(result.error()));
      bounds.combine(*result);
      pos[1] -= m_renderSettings.font_size * m_renderSettings.line_spacing;
      if (charLimit && charLimit->get() == 0)
        break;
    }
    return bounds;
  };
  if (!reallyRender) {
    auto const& active = m_fontTextureGroup.m_active_font;
    MeasurementFonts fonts{active, active};
    return layout(std::ref(fonts));
  }
  TextStyle backup = m_savedRenderSettings = m_renderSettings;
  auto result = layout({});
  m_renderSettings = std::move(backup);
  auto restored = m_fontTextureGroup.select_font(m_renderSettings.font);
  if (!restored)
    return std::unexpected(result ? std::move(restored.error()) : restored.error().with_context(result.error().what()));
  return result;
}

std::expected<RectF, StarException> TextPainter::doRenderLine(StringView text, TextPositioning const& position, MeasureOnly measure, std::optional<std::reference_wrapper<unsigned>> charLimit) {
  TextPositioning pos = position;
  if (pos.hAnchor == HorizontalAnchor::RightAnchor || pos.hAnchor == HorizontalAnchor::HMidAnchor) {
    StringView trimmedString = text;
    if (charLimit) {
      auto trimmed = text.substr(0, charLimit->get());
      if (!trimmed)
        return std::unexpected(std::move(trimmed.error()));
      trimmedString = *trimmed;
    }
    auto const& active = m_fontTextureGroup.m_active_font;
    auto measured = string_width(trimmedString, 0, measure ? measure->get() : MeasurementFonts{active, active});
    if (!measured)
      return std::unexpected(std::move(measured.error()));
    pos.pos[0] -= pos.hAnchor == HorizontalAnchor::RightAnchor ? static_cast<float>(*measured)
                                                               : std::floor(static_cast<float>(*measured) / 2);
    pos.hAnchor = HorizontalAnchor::LeftAnchor;
  }

  RectF bounds = RectF::with_size(pos.pos, Vec2F());
  auto text_callback = [&](StringView text) -> std::expected<bool, StarException> {
    Utf8Cursor cursor(text.utf8());
    while (!cursor.at_end()) {
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
      auto const& active = measure ? measure->get().current : m_fontTextureGroup.m_active_font;
      auto glyphBounds = doRenderGlyph(c, pos, !measure, active);
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

  auto commands_callback = [&](StringView commands) -> std::expected<bool, StarException> {
    auto applied = measure ? apply_measurement_commands(measure->get(), commands.utf8()) : applyCommands(commands);
    if (!applied)
      return std::unexpected(std::move(applied.error()));
    return true;
  };

  auto processed = Text::process_text(text, text_callback, commands_callback);
  if (!processed)
    return std::unexpected(std::move(processed.error()));

  return bounds;
}

std::expected<RectF, StarException> TextPainter::doRenderGlyph(String::Char c, TextPositioning const& position,
    bool really_render, std::shared_ptr<Font> const& active) {
  if (c == '\n' || c == '\v' || c == '\r')
    return RectF();

  auto resolved = m_fontTextureGroup.resolve_font(active, c);
  if (!resolved)
    return std::unexpected(std::move(resolved.error()));
  auto const& font = resolved->get();
  auto measured = font->width(c, m_renderSettings.font_size);
  if (!measured)
    return std::unexpected(std::move(measured.error()));
  int width = *measured;
  // Offset left by width if right anchored.
  float hOffset = 0;
  if (position.hAnchor == HorizontalAnchor::RightAnchor)
    hOffset = -width;
  else if (position.hAnchor == HorizontalAnchor::HMidAnchor)
    hOffset = -std::floor(static_cast<float>(width) / 2);

  float vOffset = 0;
  if (position.vAnchor == VerticalAnchor::VMidAnchor)
    vOffset = -std::floor(static_cast<float>(m_renderSettings.font_size) / 2);
  else if (position.vAnchor == VerticalAnchor::TopAnchor)
    vOffset = -static_cast<float>(m_renderSettings.font_size);

  Vec2F pos = position.pos + Vec2F(hOffset, vOffset);
  if (really_render && m_renderSettings.font_size) {
    auto lookup = [&](Directives const& directives) {
      return m_fontTextureGroup.glyph_texture(font, c, m_renderSettings.font_size, std::cref(directives), GlyphDirectivePolicy::Text);
    };
    auto append = [](List<RenderPrimitive>& out, FontTextureGroup::GlyphTexture const& glyph, Vec2F point, Vec4B color) {
      if (glyph.colored)
        color[0] = color[1] = color[2] = 255;
      out.emplace_back(std::in_place_type_t<RenderQuad>(), glyph.texture, Vec2F::round(point + glyph.offset), 1.0f, color, 0.0f);
    };
    bool has_shadow = m_renderSettings.shadow[3] > 0;
    bool has_back = m_renderSettings.back_directives;
    auto glyph = lookup(has_back ? m_renderSettings.back_directives : m_renderSettings.directives);
    if (!glyph)
      return std::unexpected(std::move(glyph.error()));
    if (has_shadow) {
      Vec4B shadow = m_renderSettings.shadow;
      std::uint8_t alpha_byte = m_renderSettings.color[3] * byte_to_float(shadow[3]);
      float alpha = byte_to_float(alpha_byte);
      shadow[3] = alpha_byte == 255 ? alpha_byte : float_to_byte(alpha * (1.5f - 0.5f * alpha));
      append(m_shadowPrimitives, glyph->get(), pos + Vec2F(0, -2), shadow);
    }
    if (has_back) {
      append(m_backPrimitives, glyph->get(), pos, m_renderSettings.color);
      // Finish using the borrowed back entry before a distinct front lookup:
      // an insertion can relocate the cache.
      if (font != m_fontTextureGroup.m_emoji_font && !m_renderSettings.directives.equals(m_renderSettings.back_directives)) {
        glyph = lookup(m_renderSettings.directives);
        if (!glyph)
          return std::unexpected(std::move(glyph.error()));
      }
    }
    auto& output = has_shadow || has_back ? m_frontPrimitives : m_renderer->immediatePrimitives();
    append(output, glyph->get(), pos, m_renderSettings.color);
  }

  return RectF::with_size(pos, {static_cast<float>(width), static_cast<int>(m_renderSettings.font_size)});
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

}
