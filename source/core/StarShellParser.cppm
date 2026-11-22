module;
import std;
import star.string;
import star.format;

namespace Star {

struct ShellParsingExceptionTag {
  static constexpr std::string_view name() { return "ShellParsingException"; }
};
using ShellParsingException = StarError<ShellParsingExceptionTag, StarException>;

[[nodiscard]] std::expected<StringList, StarException> split_shell_words(String const& command);

}

export module star.shell_parser;

export namespace Star {
  using ::Star::ShellParsingExceptionTag;
  using ::Star::ShellParsingException;
  using ::Star::split_shell_words;
}
