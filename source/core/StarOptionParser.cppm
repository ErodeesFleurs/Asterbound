module;
import std;
import star.list;
import star.format;
import star.string;
import star.ordered_map;
import star.ordered_set;

namespace Star {

struct OptionParserExceptionTag {
  static constexpr std::string_view name() { return "OptionParserException"; }
};
using OptionParserException = StarError<OptionParserExceptionTag, StarException>;

// Simple command line argument parsing and help printing, only simple single
// dash flags are supported, no flag combining is allowed and all components
// must be separated by a space.
//
// A 'flag' here refers to a component that is preceded by a dash, like -f or
// -quiet.
//
// Three kinds of things are parsed:
// - 'switches' which are flags that do not have a value, like `-q` for quiet
// - 'parameters' are flags with a value that follows, like `-mode full`
// - 'arguments' are everything else, sorted positionally
// The first standalone `--` ends flag recognition. A parameter consumes its
// following token literally, even when that token is `--`.
class OptionParser {
public:
  enum RequirementMode {
    Optional,
    Required,
    Multiple
  };

  struct Options {
    OrderedSet<String> switches;
    StringMap<StringList> parameters;
    StringList arguments;

    [[nodiscard]] std::optional<std::reference_wrapper<String const>> first_parameter(std::string_view name) const&;
    void first_parameter(std::string_view) const&& = delete;
  };

  struct ParseResult {
    Options options;
    StringList errors;
  };

  void set_command_name(String command_name);
  void set_summary(String summary);
  void set_additional_help(String help);

  [[nodiscard]] std::expected<void, StarException> add_switch(String flag, String description = {});
  [[nodiscard]] std::expected<void, StarException> add_parameter(String flag, String argument_name, RequirementMode mode, String description = {});
  void add_argument(String argument_name, RequirementMode mode, String description = {});

  // Parse the given arguments into an options set, returns the options parsed
  // and a list of all the errors encountered while parsing.
  [[nodiscard]] std::expected<ParseResult, StarException> parse_options(StringList const& arguments) const;

  // Print help text, checking writes and flushing the destination.
  [[nodiscard]] std::expected<void, StarException> print_help(std::FILE& output) const;

private:
  struct Switch {
    String description;
  };

  struct Parameter {
    String argument_name;
    RequirementMode mode;
    String description;
  };

  struct Argument {
    String argument_name;
    RequirementMode mode;
    String description;
  };

  using Option = std::variant<Switch, Parameter>;

  String m_command_name;
  String m_summary;
  String m_additional_help;

  OrderedHashMap<String, Option> m_options;
  List<Argument> m_arguments;
};

}

export module star.option_parser;

export namespace Star {
  using ::Star::OptionParserExceptionTag;
  using ::Star::OptionParserException;
  using ::Star::OptionParser;
}
