import std;
import star.memory;
import star.format;
import star.json;
import star.json_extra;
import star.string;
import star.string_view;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.thread;
import star.directives;
import star.text;
#include <re2/re2.h>

namespace Star {

std::expected<TextStyle, StarException> TextStyle::create(Json const& config) {
  TextStyle result;
  if (config.isType(Json::Type::String)) {
    auto font = config.toString();
    if (!font) return std::unexpected(std::move(font.error()));
    result.font = std::move(*font);
  } else {
    auto loaded = result.loadJson(config);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
  }
  return result;
}
std::expected<std::reference_wrapper<TextStyle>, StarException> TextStyle::loadJson(Json const& config) {
  if (!config) return std::ref(*this);
  auto spacing = config.getFloat("lineSpacing", lineSpacing);
  if (!spacing) return std::unexpected(std::move(spacing.error()));
  lineSpacing = *spacing;
  auto jColor = config.opt("color");
  if (!jColor) return std::unexpected(std::move(jColor.error()));
  if (*jColor) {
    auto converted = jsonToColor(**jColor);
    if (!converted) return std::unexpected(std::move(converted.error()));
    color = converted->toRgba();
  }
  auto jShadow = config.opt("shadow");
  if (!jShadow) return std::unexpected(std::move(jShadow.error()));
  if (*jShadow) {
    auto converted = jsonToColor(**jShadow);
    if (!converted) return std::unexpected(std::move(converted.error()));
    shadow = converted->toRgba();
  }
  auto size = config.getUInt("fontSize", fontSize);
  if (!size) return std::unexpected(std::move(size.error()));
  fontSize = *size;
  auto jFont = config.optString("font");
  if (!jFont) return std::unexpected(std::move(jFont.error()));
  if (*jFont) font = std::move(**jFont);
  auto jDirectives = config.optString("directives");
  if (!jDirectives) return std::unexpected(std::move(jDirectives.error()));
  if (*jDirectives) directives = **jDirectives;
  auto jBackDirectives = config.optString("backDirectives");
  if (!jBackDirectives) return std::unexpected(std::move(jBackDirectives.error()));
  if (*jBackDirectives) backDirectives = **jBackDirectives;
  return std::ref(*this);
}

namespace Text {
  static RE2 stripEscapeRegex = strf("\\{:c}[^;{:c}]*{:c}", CmdEsc, CmdEsc, EndEsc);
  String stripEscapeCodes(String const& s) {
    if (s.empty())
      return s;
    std::string result = s.utf8();
    RE2::GlobalReplace(&result, stripEscapeRegex, "");
    return String(std::move(result));
  }

  std::expected<bool, StarException> processText(StringView text, TextCallback textFunc, CommandsCallback commandsFunc, bool includeCommandSides) {
    std::string_view str = text.utf8();
    std::size_t start = 0;
    while (true) {
      std::size_t escape = str.find_first_of(AllEsc, start);
      start = 0;
      if (escape != NPos) {
        escape = str.find_first_not_of(AllEsc, escape) - 1; // jump to the last ^
        std::size_t end = str.find_first_of(AllEscEnd, escape + 1);
        if (end != NPos) {
          if (str[end] != EndEsc) {
            start = end;
            continue;
          }
          if (escape) {
            if (!textFunc) return std::unexpected(StarException("Text callback is empty"));
            auto result = textFunc(str.substr(0, escape));
            if (!result) return std::unexpected(std::move(result.error()));
            if (!*result) return false;
          }
          if (commandsFunc) {
            StringView commands = includeCommandSides
              ? str.substr(escape, end - escape + 1)
              : str.substr(escape + 1, end - escape - 1);
            if (!commands.empty()) {
              auto result = commandsFunc(commands);
              if (!result) return std::unexpected(std::move(result.error()));
              if (!*result) return false;
            }
          }
          str = str.substr(end + 1);
          continue;
        }
      }

      if (!str.empty()) {
        if (!textFunc) return std::unexpected(StarException("Text callback is empty"));
        return textFunc(str);
      }

      return true;
    }
  }
}

}