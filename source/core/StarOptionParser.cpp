import std;
import star.memory;
import star.format;
import star.string;
import star.ordered_map;
import star.ordered_set;
import star.option_parser;

namespace Star {

void OptionParser::setCommandName(String commandName) {
  m_commandName = std::move(commandName);
}

void OptionParser::setSummary(String summary) {
  m_summary = std::move(summary);
}

void OptionParser::setAdditionalHelp(String help) {
  m_additionalHelp = std::move(help);
}

std::expected<void, StarException> OptionParser::addSwitch(String const& flag, String description) {
  if (!m_options.insert(flag, Switch{.flag = flag, .description = std::move(description)}).second)
    return std::unexpected(OptionParserException::format("Duplicate switch '-{}' added", flag));
  return {};
}

std::expected<void, StarException> OptionParser::addParameter(String const& flag, String argument, RequirementMode requirementMode, String description) {
  if (!m_options
         .insert(flag,
                 Parameter{.flag = flag,
                           .argument = std::move(argument),
                           .requirementMode = requirementMode,
                           .description = std::move(description)})
         .second)
    return std::unexpected(OptionParserException::format("Duplicate flag '-{}' added", flag));
  return {};
}

void OptionParser::addArgument(String argument, RequirementMode requirementMode, String description) {
  m_arguments.append(Argument{.argumentName = std::move(argument),
                              .requirementMode = requirementMode,
                              .description = std::move(description)});
}

std::expected<std::pair<OptionParser::Options, StringList>, StarException> OptionParser::parseOptions(StringList const& arguments) const {
  Options result;
  StringList errors;
  bool endOfFlags = false;

  auto it = arguments.begin();
  while (it != arguments.end()) {
    auto const& arg = *it++;
    if (arg == "--") {
      endOfFlags = true;
      continue;
    }

    if (!endOfFlags && !arg.empty() && arg.utf8().front() == '-') {
      auto parsedFlag = arg.substr(1);
      if (!parsedFlag)
        return std::unexpected(std::move(parsedFlag.error()));
      String flag = std::move(*parsedFlag);
      auto option = m_options.maybe(flag);
      if (!option) {
        errors.append(strf("No such option '-{}'", flag));
        continue;
      }

      if (std::holds_alternative<Switch>(*option)) {
        result.switches.add(std::move(flag));
      } else {
        auto const& parameter = std::get<Parameter>(*option);
        if (it == arguments.end()) {
          errors.append(strf("Option '-{}' must be followed by an argument", flag));
          continue;
        }
        String val = *it++;
        if (parameter.requirementMode != Multiple && result.parameters.contains(flag)) {
          errors.append(strf("Option with argument '-{}' specified multiple times", flag));
          continue;
        }
        result.parameters[std::move(flag)].append(std::move(val));
      }

    } else {
      result.arguments.append(arg);
    }
  }

  for (auto const& pair : m_options) {
    if (std::holds_alternative<Parameter>(pair.second)) {
      auto const& na = std::get<Parameter>(pair.second);
      if (na.requirementMode == Required && !result.parameters.contains(pair.first))
        errors.append(strf("Missing required flag with argument '-{}'", pair.first));
    }
  }

  std::size_t minimumArguments = 0;
  std::size_t maximumArguments = 0;
  for (auto const& argument : m_arguments) {
    if ((argument.requirementMode == Optional || argument.requirementMode == Required) && maximumArguments != NPos)
      ++maximumArguments;
    if (argument.requirementMode == Required)
      ++minimumArguments;
    if (argument.requirementMode == Multiple)
      maximumArguments = NPos;
  }
  if (result.arguments.size() < minimumArguments)
    errors.append(strf(
        "Too few positional arguments given, expected at least {} got {}", minimumArguments, result.arguments.size()));
  if (result.arguments.size() > maximumArguments)
    errors.append(strf(
        "Too many positional arguments given, expected at most {} got {}", maximumArguments, result.arguments.size()));

  return std::pair{std::move(result), std::move(errors)};
}

void OptionParser::printHelp(std::ostream& os) const {
  if (!m_commandName.empty() && !m_summary.empty())
    format(os, "{}: {}\n\n", m_commandName, m_summary);
  else if (!m_commandName.empty())
    format(os, "{}:\n\n", m_commandName);
  else if (!m_summary.empty())
    format(os, "{}\n\n", m_summary);

  String cmdLineText;

  for (auto const& p : m_options) {
    if (std::holds_alternative<Switch>(p.second)) {
      cmdLineText += strf(" [-{}]", p.first);
    } else {
      auto const& parameter = std::get<Parameter>(p.second);
      if (parameter.requirementMode == Optional)
        cmdLineText += strf(" [-{} <{}>]", parameter.flag, parameter.argument);
      else if (parameter.requirementMode == Required)
        cmdLineText += strf(" -{} <{}>", parameter.flag, parameter.argument);
      else if (parameter.requirementMode == Multiple)
        cmdLineText += strf(" [-{} <{}>]...", parameter.flag, parameter.argument);
    }
  }

  for (auto const& p : m_arguments) {
    if (p.requirementMode == Optional)
      cmdLineText += strf(" [<{}>]", p.argumentName);
    else if (p.requirementMode == Required)
      cmdLineText += strf(" <{}>", p.argumentName);
    else
      cmdLineText += strf(" [<{}>...]", p.argumentName);
  }

  if (m_commandName.empty())
    format(os, "Command Line Usage:{}\n", cmdLineText);
  else
    format(os, "Command Line Usage: {}{}\n", m_commandName, cmdLineText);

  for (auto const& p : m_options) {
    if (std::holds_alternative<Switch>(p.second)) {
      auto const& sw = std::get<Switch>(p.second);
      if (!sw.description.empty())
        format(os, "  -{}\t- {}\n", sw.flag, sw.description);
    }
    if (std::holds_alternative<Parameter>(p.second)) {
      auto const& parameter = std::get<Parameter>(p.second);
      if (!parameter.description.empty())
        format(os, "  -{} <{}>\t- {}\n", parameter.flag, parameter.argument, parameter.description);
    }
  }

  for (auto const& p : m_arguments) {
    if (!p.description.empty())
      format(os, "  <{}>\t- {}\n", p.argumentName, p.description);
  }

  if (!m_additionalHelp.empty())
    format(os, "\n{}\n", m_additionalHelp);
}

}
