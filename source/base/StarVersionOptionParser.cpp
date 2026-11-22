#include <cstdio>
import std;
import star.string;
import star.ordered_map;
import star.ordered_set;
import star.option_parser;
import star.format;
import star.version;
import star.version_option_parser;
import star.file;

namespace Star {

String VersionOptionParser::version_string() const {
  return Star::format("Asterbound{}{} v{} for v{} ({}) Source ID: {}", m_version_name.empty() ? "" : " ", m_version_name, product_version, base_game_version, target_architecture, source_identifier);
}

void VersionOptionParser::set_version_name(String name) {
  m_version_name = std::move(name);
}

std::expected<VersionOptionParser, StarException> VersionOptionParser::create() {
  VersionOptionParser parser;
  auto initialized = parser.initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return parser;
}

std::expected<void, StarException> VersionOptionParser::initialize() {
  auto help = add_switch("help", "Show help text");
  if (!help)
    return std::unexpected(std::move(help.error()));
  return add_switch("version", "Print version info");
}

std::expected<VersionOptionParser::Options, StarException> VersionOptionParser::parse_or_die(StringList const& cmd_line_arguments) const {
  auto parsed = OptionParser::parse_options(cmd_line_arguments);
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  auto& [options, errors] = *parsed;
  bool const show_version = options.switches.contains("version");
  bool const show_help = options.switches.contains("help");

  if (show_version) {
    auto written = Star::print("{}\n", version_string());
    if (!written)
      std::exit(1);
  }

  if (show_help) {
    auto written = print_help(*stdout);
    if (!written)
      std::exit(1);
  }

  if (show_version || show_help)
    std::exit(0);

  if (!errors.empty()) {
    for (auto const& err : errors) {
      auto written = Star::print("Error: {}\n", err);
      if (!written)
        std::exit(1);
    }
    auto written = Star::print("\n");
    if (!written)
      std::exit(1);
    written = print_help(*stdout);
    if (!written)
      std::exit(1);
    std::exit(1);
  }

  return std::move(options);
}

std::expected<VersionOptionParser::Options, StarException> VersionOptionParser::command_parse_or_die(std::span<char const* const> argv) {
  if (argv.empty())
    return parse_or_die({});
  set_command_name(File::base_name(String(std::string_view(argv.front()))));
  argv = argv.subspan(1);
  StringList arguments;
  arguments.reserve(argv.size());
  for (auto argument : argv)
    arguments.emplace_back(std::string_view(argument));
  return parse_or_die(arguments);
}

}
