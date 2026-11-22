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

namespace Star {

std::expected<TextStyle, StarException> TextStyle::create(Json const& config) {
  TextStyle result;
  if (config.is_type(Json::Type::String)) {
    auto font = config.to_string();
    if (!font) return std::unexpected(std::move(font.error()));
    result.font = std::move(*font);
  } else {
    auto loaded = result.load_json(config);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
  }
  return result;
}
std::expected<void, StarException> TextStyle::load_json(Json const& config) {
  if (!config) return {};
  auto spacing = config.get_float("lineSpacing", line_spacing);
  if (!spacing) return std::unexpected(std::move(spacing.error()));
  auto color_json = config.opt("color");
  if (!color_json) return std::unexpected(std::move(color_json.error()));
  auto next_color = color;
  if (*color_json) {
    auto converted = json_to_color(**color_json);
    if (!converted) return std::unexpected(std::move(converted.error()));
    next_color = converted->to_rgba();
  }
  auto shadow_json = config.opt("shadow");
  if (!shadow_json) return std::unexpected(std::move(shadow_json.error()));
  auto next_shadow = shadow;
  if (*shadow_json) {
    auto converted = json_to_color(**shadow_json);
    if (!converted) return std::unexpected(std::move(converted.error()));
    next_shadow = converted->to_rgba();
  }
  auto size = config.get_uint("fontSize", font_size);
  if (!size) return std::unexpected(std::move(size.error()));
  if (!std::in_range<unsigned>(*size)) return std::unexpected(StarException("fontSize out of range"));
  auto font_json = config.opt_string("font");
  if (!font_json) return std::unexpected(std::move(font_json.error()));
  auto directives_json = config.opt_string("directives");
  if (!directives_json) return std::unexpected(std::move(directives_json.error()));
  auto back_directives_json = config.opt_string("backDirectives");
  if (!back_directives_json) return std::unexpected(std::move(back_directives_json.error()));
  line_spacing = *spacing;
  font_size = static_cast<unsigned>(*size);
  color = next_color;
  shadow = next_shadow;
  if (*font_json) font = std::move(**font_json);
  if (*directives_json) directives = **directives_json;
  if (*back_directives_json) back_directives = **back_directives_json;
  return {};
}

namespace Text {
  std::optional<CommandRange> find_next_command(std::string_view text, std::size_t offset, bool allow_esc) {
    auto starts = allow_esc ? std::string_view("^\x1b") : std::string_view("^");
    auto delimiters = allow_esc ? std::string_view("^\x1b;") : std::string_view("^;");
    auto begin = text.find_first_of(starts, offset);
    while (begin != text.npos) {
      auto next = text.find_first_of(delimiters, begin + 1);
      if (next == text.npos) break;
      if (text[next] != ';') { begin = next; continue; }
      // The historical stripping regex consumes Unicode scalars, not invalid bytes.
      if (allow_esc || StringView(text.substr(begin + 1, next - begin - 1)).size()) return CommandRange{begin, next + 1};
      begin = text.find('^', next + 1);
    }
    return {};
  }

  String strip_escape_codes(String const& input) {
    std::string_view text = input.utf8();
    auto range = find_next_command(text, 0, false);
    if (!range) return input;
    std::string result;
    result.reserve(text.size() - (range->end - range->begin));
    do {
      result.append(text.substr(0, range->begin));
      text.remove_prefix(range->end);
      range = find_next_command(text, 0, false);
    } while (range);
    result.append(text);
    return String(std::move(result));
  }

  std::expected<std::size_t, StarException> visible_length(String const& input) {
    std::string_view text = input.utf8();
    std::size_t count = 0;
    while (auto range = find_next_command(text, 0, false)) {
      auto size = StringView(text.substr(0, range->begin)).size();
      if (!size) return strip_escape_codes(input).size(); // deletion can join split UTF-8 bytes
      count += *size;
      text.remove_prefix(range->end);
    }
    auto tail = StringView(text).size();
    if (!tail) return std::unexpected(std::move(tail.error()));
    return count + *tail;
  }
}

}