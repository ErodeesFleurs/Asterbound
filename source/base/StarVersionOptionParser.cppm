module;

import std;
import star.format;
import star.string;
import star.ordered_map;
import star.ordered_set;
import star.option_parser;
import star.version;

namespace Star {

// Option parser that accepts -h to print the help and exit and -v to print the
// version and exit.
class VersionOptionParser : public OptionParser {
public:
  String version_string() const;
  void set_version_name(String name);

  [[nodiscard]] static std::expected<VersionOptionParser, StarException> create();

  // Parse the command line options, or, in the case of an error, -h, or -v,
  // prints the appropriate text and immediately exits.
  [[nodiscard]] std::expected<Options, StarException> parse_or_die(StringList const& cmd_line_arguments) const;

  // First sets the command name based on argv[0], then calls parse_or_die.
  [[nodiscard]] std::expected<Options, StarException> command_parse_or_die(std::span<char const* const> argv);

protected:
  VersionOptionParser() = default;
  [[nodiscard]] std::expected<void, StarException> initialize();

private:
  String m_version_name;
};

}

export module star.version_option_parser;

export namespace Star {
  using ::Star::VersionOptionParser;
}
