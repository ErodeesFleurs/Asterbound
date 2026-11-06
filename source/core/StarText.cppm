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

inline constexpr unsigned DefaultFontSize = 8;
inline constexpr float DefaultLineSpacing = 1.3f;

struct TextStyle {
  float lineSpacing = DefaultLineSpacing;
  Vec4B color = Vec4B::filled(255);
  Vec4B shadow = Vec4B::filled(0);
  unsigned fontSize = DefaultFontSize;
  String font = "";
  Directives directives;
  Directives backDirectives;

  TextStyle() = default;
  [[nodiscard]] static std::expected<TextStyle, StarException> create(Json const& config);
  [[nodiscard]] std::expected<std::reference_wrapper<TextStyle>, StarException> loadJson(Json const& config);
};

namespace Text {
  inline constexpr unsigned char StartEsc = '\x1b';
  inline constexpr unsigned char EndEsc = ';';
  inline constexpr unsigned char CmdEsc = '^';
  inline constexpr unsigned char SpecialCharLimit = ' ';
  inline constexpr std::array<char, 3> EscapeCharacters{static_cast<char>(CmdEsc), static_cast<char>(StartEsc),
                                                        static_cast<char>(EndEsc)};
  inline constexpr std::string_view AllEsc{EscapeCharacters.data(), 2};
  inline constexpr std::string_view AllEscEnd{EscapeCharacters.data(), EscapeCharacters.size()};

  String stripEscapeCodes(String const& s);
  inline bool isEscapeCode(Utf32Type c) { return c == CmdEsc || c == StartEsc; }

  typedef std::function<std::expected<bool, StarException>(StringView text)> TextCallback;
  typedef std::function<std::expected<bool, StarException>(StringView commands)> CommandsCallback;
  [[nodiscard]] std::expected<bool, StarException> processText(StringView text, TextCallback textFunc, CommandsCallback commandsFunc = CommandsCallback(), bool includeCommandSides = false);
}

}

export module star.text;

export namespace Star {
  using ::Star::DefaultFontSize;
  using ::Star::DefaultLineSpacing;
  using ::Star::TextStyle;
}

export namespace Star::Text {
  using ::Star::Text::StartEsc;
  using ::Star::Text::EndEsc;
  using ::Star::Text::CmdEsc;
  using ::Star::Text::SpecialCharLimit;
  using ::Star::Text::AllEsc;
  using ::Star::Text::AllEscEnd;
  using ::Star::Text::stripEscapeCodes;
  using ::Star::Text::isEscapeCode;
  using ::Star::Text::TextCallback;
  using ::Star::Text::CommandsCallback;
  using ::Star::Text::processText;
}
