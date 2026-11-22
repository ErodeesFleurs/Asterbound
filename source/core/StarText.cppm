module;
import std;
import star.format;
import star.string_view;
import star.unicode;
import star.string;
import star.vector;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.thread;
import star.directives;

namespace Star {

class Json;

inline constexpr unsigned default_font_size = 8;
inline constexpr float default_line_spacing = 1.3f;

struct TextStyle {
  float line_spacing = default_line_spacing;
  Vec4B color = Vec4B::filled(255);
  Vec4B shadow = Vec4B::filled(0);
  unsigned font_size = default_font_size;
  String font = "";
  Directives directives;
  Directives back_directives;

  TextStyle() = default;
  [[nodiscard]] static std::expected<TextStyle, StarException> create(Json const& config);
  [[nodiscard]] std::expected<void, StarException> load_json(Json const& config);
};

namespace Text {
  struct CommandRange { std::size_t begin, end; };
  std::optional<CommandRange> find_next_command(std::string_view text, std::size_t offset, bool allow_esc);
  String strip_escape_codes(String const& text);
  std::expected<std::size_t, StarException> visible_length(String const& text);

  template <typename Visitor>
  concept text_visitor = requires(Visitor& visit, StringView part) {
    { visit(part) } -> std::same_as<std::expected<bool, StarException>>;
  };

  template <text_visitor TextVisitor, text_visitor CommandVisitor>
  std::expected<bool, StarException> process_text(StringView input, TextVisitor&& visit_text, CommandVisitor&& visit_command, bool include_sides = false) {
    auto text = input.utf8();
    while (auto range = find_next_command(text, 0, true)) {
      if (range->begin) {
        auto result = visit_text(StringView(text.substr(0, range->begin)));
        if (!result || !*result) return result;
      }
      auto command = text.substr(range->begin, range->end - range->begin);
      if (!include_sides) command = command.substr(1, command.size() - 2);
      if (!command.empty()) {
        auto result = visit_command(StringView(command));
        if (!result || !*result) return result;
      }
      text.remove_prefix(range->end);
    }
    if (!text.empty()) return visit_text(StringView(text));
    return true;
  }
}

}

export module star.text;

export namespace Star {
  using ::Star::default_font_size;
  using ::Star::default_line_spacing;
  using ::Star::TextStyle;
}

export namespace Star::Text {
  using ::Star::Text::CommandRange;
  using ::Star::Text::find_next_command;
  using ::Star::Text::strip_escape_codes;
  using ::Star::Text::visible_length;
  using ::Star::Text::text_visitor;
  using ::Star::Text::process_text;
}
