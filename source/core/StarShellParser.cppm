module;
import std;
import star.list;
import star.string;
import star.byte_array;
import star.encode;
import star.bytes;
import star.format;

namespace Star {

// Currently the specification of the "language" is incredibly simple The only
// thing we process are quoted strings and backslashes Backslashes function as
// a useful subset of C++ This means: Newline: \n Tab: \t Backslash: \\ Single
// Quote: \' Double Quote: \" Null: \0 Space: "\ " (without quotes ofc, not
// actually C++) Also \v \b \a \f \r Plus Unicode \uxxxx Not implemented octal
// and hexadecimal, because it's possible to construct invalid unicode code
// points using them

struct ShellParsingExceptionTag {
  static constexpr std::string_view name() { return "ShellParsingException"; }
};
using ShellParsingException = StarError<ShellParsingExceptionTag, StarException>;

class ShellParser {
public:
  ShellParser();
  using Char = String::Char;

  enum class TokenType {
    Word,
    // TODO: braces, brackets, actual shell stuff

  };

  struct Token {
    TokenType type;
    String token;
  };

  std::expected<List<Token>, StarException> tokenize(String const& command);
  std::expected<StringList, StarException> tokenizeToStringList(String const& command);

private:
  std::expected<void, StarException> init(String const& command);

  std::expected<String, StarException> word();
  std::expected<Char, StarException> parseBackslash();
  std::expected<Char, StarException> parseUnicodeEscapeSequence(std::optional<Char> previousCodepoint = {});

  bool is_space(Char letter) const;
  bool isQuote(Char letter) const;

  bool inQuotedString() const;
  bool notDone() const;

  std::optional<Char> current() const;
  std::optional<Char> next();
  std::optional<Char> previous();

  String::const_iterator m_begin;
  String::const_iterator m_current;
  String::const_iterator m_end;

  Char m_quotedType;
};

}

export module star.shell_parser;

export namespace Star {
  using ::Star::ShellParsingExceptionTag;
  using ::Star::ShellParsingException;
  using ::Star::ShellParser;
}
