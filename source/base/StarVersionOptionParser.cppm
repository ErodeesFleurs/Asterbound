module;

import std;
import star.format;
import star.string;
import star.ordered_map;
import star.ordered_set;
import star.option_parser;

namespace Star {

// Adds -help and -version to the application options.
class VersionOptionParser : public OptionParser {
public:
  String version_string() const;
  void set_version_name(String name);

  [[nodiscard]] static std::expected<VersionOptionParser, StarException> create();

  // Exits after printing help, version information or command-line errors.
  [[nodiscard]] std::expected<Options, StarException> parse_or_die(std::span<String const> arguments) const;

  // argv includes the executable name; an empty argv leaves the command name unchanged.
  [[nodiscard]] std::expected<Options, StarException> command_parse_or_die(std::span<String const> argv);

protected:
  VersionOptionParser() = default;
  [[nodiscard]] std::expected<void, StarException> initialize();
  [[nodiscard]] std::span<String const> command_arguments(std::span<String const> argv);

private:
  String m_version_name;
};

}

export module star.version_option_parser;

export namespace Star {
  using ::Star::VersionOptionParser;
}
