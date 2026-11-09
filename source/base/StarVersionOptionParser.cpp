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

String VersionOptionParser::getVersionString() const {
  String displayName = m_versionName ? *m_versionName : String();
  return Star::format("OpenStarbound{} v{} for v{} ({}) Source ID: {}", displayName.empty() ? "" : Star::format(" {}", displayName), OpenStarVersionString, StarVersionString, StarArchitectureString, StarSourceIdentifierString);
}

void VersionOptionParser::setVersionName(String const& name) {
  m_versionName = name;
}

std::expected<VersionOptionParser, StarException> VersionOptionParser::create() {
  VersionOptionParser parser;
  auto initialized = parser.initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return parser;
}

std::expected<void, StarException> VersionOptionParser::initialize() {
  auto help = addSwitch("help", "Show help text");
  if (!help)
    return std::unexpected(std::move(help.error()));
  return addSwitch("version", "Print version info");
}

std::expected<VersionOptionParser::Options, StarException> VersionOptionParser::parseOrDie(StringList const& cmdLineArguments) const {
  auto parsed = OptionParser::parseOptions(cmdLineArguments);
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  auto& [options, errors] = *parsed;

  if (options.switches.contains("version")) {
    auto written = Star::print("{}\n", getVersionString());
    if (!written)
      std::exit(1);
  }

  if (options.switches.contains("help")) {
    auto written = print_help(stdout);
    if (!written)
      std::exit(1);
  }

  if (options.switches.contains("version") || options.switches.contains("help"))
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
    written = print_help(stdout);
    if (!written)
      std::exit(1);
    std::exit(1);
  }

  return std::move(options);
}

std::expected<VersionOptionParser::Options, StarException> VersionOptionParser::commandParseOrDie(int argc, char** argv) {
  setCommandName(File::base_name(String(std::string_view(argv[0]))));
  StringList arguments;
  arguments.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0);
  for (int i = 1; i < argc; ++i)
    arguments.push_back(String(std::string_view(argv[i])));
  return parseOrDie(arguments);
}

}
