import std;
import star.memory;
import star.format;
import star.text_formatter;
import star.string;
import star.map;
import star.ordered_map;
import star.ordered_set;
import star.option_parser;

namespace Star {

std::optional<std::reference_wrapper<String const>> OptionParser::Options::first_parameter(std::string_view name) const& {
  auto it = parameters.find(name);
  if (it == parameters.end() || it->second.empty())
    return std::nullopt;
  return std::cref(it->second.front());
}

void OptionParser::set_command_name(String command_name) {
  m_command_name = std::move(command_name);
}

void OptionParser::set_summary(String summary) {
  m_summary = std::move(summary);
}

void OptionParser::set_additional_help(String help) {
  m_additional_help = std::move(help);
}

std::expected<void, StarException> OptionParser::add_switch(String flag, String description) {
  auto [it, inserted] = m_options.try_emplace(std::move(flag), std::in_place_type<Switch>, std::move(description));
  if (!inserted)
    return std::unexpected(OptionParserException::format("Duplicate switch '-{}' added", it->first));
  return {};
}

std::expected<void, StarException> OptionParser::add_parameter(String flag, String argument_name, RequirementMode mode, String description) {
  auto [it, inserted] = m_options.try_emplace(std::move(flag), std::in_place_type<Parameter>, std::move(argument_name), mode, std::move(description));
  if (!inserted)
    return std::unexpected(OptionParserException::format("Duplicate flag '-{}' added", it->first));
  return {};
}

void OptionParser::add_argument(String argument_name, RequirementMode mode, String description) {
  m_arguments.push_back({std::move(argument_name), mode, std::move(description)});
}

std::expected<OptionParser::ParseResult, StarException> OptionParser::parse_options(StringList const& arguments) const {
  std::expected<ParseResult, StarException> result(std::in_place);
  auto& [options, errors] = *result;

  auto it = arguments.begin();
  while (it != arguments.end()) {
    auto const& arg = *it++;
    if (arg == "--") {
      options.arguments.insert(options.arguments.end(), it, arguments.end());
      break;
    }

    if (arg.empty() || arg.utf8().front() != '-') {
      options.arguments.push_back(arg);
      continue;
    }

    auto flag = arg.substr(1);
    if (!flag)
      return std::unexpected(std::move(flag.error()));
    auto definition = m_options.find(*flag);
    if (definition == m_options.end()) {
      errors.push_back(Star::format("No such option '-{}'", *flag));
      continue;
    }
    if (std::holds_alternative<Switch>(definition->second)) {
      options.switches.add(std::move(*flag));
      continue;
    }
    if (it == arguments.end()) {
      errors.push_back(Star::format("Option '-{}' must be followed by an argument", *flag));
      continue;
    }

    auto const& value = *it++;
    auto [entry, inserted] = options.parameters.try_emplace(std::move(*flag));
    if (!inserted && std::get<Parameter>(definition->second).mode != Multiple) {
      errors.push_back(Star::format("Option with argument '-{}' specified multiple times", entry->first));
      continue;
    }
    entry->second.push_back(value);
  }

  for (auto const& [flag, definition] : m_options) {
    if (std::holds_alternative<Parameter>(definition) && std::get<Parameter>(definition).mode == Required
        && !options.parameters.contains(flag))
      errors.push_back(Star::format("Missing required flag with argument '-{}'", flag));
  }

  std::size_t minimum_arguments = 0;
  std::size_t maximum_arguments = 0;
  for (auto const& argument : m_arguments) {
    if (argument.mode == Required)
      ++minimum_arguments;
    if (argument.mode == Multiple)
      maximum_arguments = npos;
    else if ((argument.mode == Optional || argument.mode == Required) && maximum_arguments != npos)
      ++maximum_arguments;
  }
  if (options.arguments.size() < minimum_arguments)
    errors.push_back(Star::format("Too few positional arguments given, expected at least {} got {}", minimum_arguments, options.arguments.size()));
  if (options.arguments.size() > maximum_arguments)
    errors.push_back(Star::format("Too many positional arguments given, expected at most {} got {}", maximum_arguments, options.arguments.size()));

  return result;
}

std::expected<void, StarException> OptionParser::print_help(std::FILE& output) const {
  TextBuffer help;
  auto out = std::back_inserter(help);
  if (!m_command_name.empty() && !m_summary.empty())
    std::format_to(out, "{}: {}\n\n", m_command_name, m_summary);
  else if (!m_command_name.empty())
    std::format_to(out, "{}:\n\n", m_command_name);
  else if (!m_summary.empty())
    std::format_to(out, "{}\n\n", m_summary);

  help.append("Command Line Usage:");
  if (!m_command_name.empty())
    std::format_to(out, " {}", m_command_name);
  for (auto const& [flag, definition] : m_options) {
    if (std::holds_alternative<Switch>(definition)) {
      std::format_to(out, " [-{}]", flag);
    } else {
      auto const& parameter = std::get<Parameter>(definition);
      if (parameter.mode == Optional)
        std::format_to(out, " [-{} <{}>]", flag, parameter.argument_name);
      else if (parameter.mode == Required)
        std::format_to(out, " -{} <{}>", flag, parameter.argument_name);
      else if (parameter.mode == Multiple)
        std::format_to(out, " [-{} <{}>]...", flag, parameter.argument_name);
    }
  }
  for (auto const& argument : m_arguments) {
    if (argument.mode == Optional)
      std::format_to(out, " [<{}>]", argument.argument_name);
    else if (argument.mode == Required)
      std::format_to(out, " <{}>", argument.argument_name);
    else
      std::format_to(out, " [<{}>...]", argument.argument_name);
  }
  help.push_back('\n');

  for (auto const& [flag, definition] : m_options) {
    if (std::holds_alternative<Switch>(definition)) {
      auto const& description = std::get<Switch>(definition).description;
      if (!description.empty())
        std::format_to(out, "  -{}\t- {}\n", flag, description);
    } else {
      auto const& parameter = std::get<Parameter>(definition);
      if (!parameter.description.empty())
        std::format_to(out, "  -{} <{}>\t- {}\n", flag, parameter.argument_name, parameter.description);
    }
  }
  for (auto const& argument : m_arguments) {
    if (!argument.description.empty())
      std::format_to(out, "  <{}>\t- {}\n", argument.argument_name, argument.description);
  }
  if (!m_additional_help.empty())
    std::format_to(out, "\n{}\n", m_additional_help);
  return Star::print(output, "{}", help.view());
}

}
