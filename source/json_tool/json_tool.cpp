#include <stdio.h>

import std;
import star.string;
import star.list;
import star.format;
import star.output_proxy;
import star.json;
import star.file;
import star.logging;
import star.json_tool;
import star.json_editor;
import star.lexical_cast;
import star.formatted_json;
import star.json_path;
import star.native_runtime;

// Tool for scripting and mass-editing of JSON+Comments files without affecting
// formatting.

import star.algorithm;
using namespace Star;

std::expected<FormattedJson, StarException> GenericInputFormat::toJson(String const& input) const {
  return FormattedJson::parse(input);
}

std::expected<String, StarException> GenericInputFormat::fromJson(FormattedJson const& json) const {
  return json.repr();
}

std::expected<FormattedJson, StarException> GenericInputFormat::getDefault() const {
  return FormattedJson::ofType(Json::Type::Null);
}

std::expected<FormattedJson, StarException> CommaSeparatedStrings::toJson(String const& input) const {
  auto trimmed = input.trim();
  if (!trimmed)
    return std::unexpected(std::move(trimmed.error()));
  if (trimmed->empty())
    return FormattedJson::ofType(Json::Type::Array);
  auto fields = input.split(',');
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  JsonArray array;
  for (auto const& text : *fields) {
    auto field = text.trim();
    if (!field)
      return std::unexpected(std::move(field.error()));
    array.append(Json(std::move(*field)));
  }
  return FormattedJson::create(Json(std::move(array)));
}

std::expected<String, StarException> CommaSeparatedStrings::fromJson(FormattedJson const& json) const {
  auto array = json.toJson().toArray();
  if (!array)
    return std::unexpected(std::move(array.error()));
  StringList strings;
  for (auto const& entry : *array) {
    auto text = entry.toString();
    if (!text)
      return std::unexpected(std::move(text.error()));
    strings.append(std::move(*text));
  }
  return strings.join(", ");
}

std::expected<FormattedJson, StarException> CommaSeparatedStrings::getDefault() const {
  return FormattedJson::ofType(Json::Type::Array);
}

std::expected<FormattedJson, StarException> StringInputFormat::toJson(String const& input) const {
  return FormattedJson::create(Json(input));
}

std::expected<String, StarException> StringInputFormat::fromJson(FormattedJson const& json) const {
  return json.toJson().toString();
}

std::expected<FormattedJson, StarException> StringInputFormat::getDefault() const {
  return FormattedJson::create(Json(""));
}

std::expected<String, StarException> Star::reprWithLineEnding(FormattedJson const& json) {
  auto repr = json.repr();
  if (!repr)
    return std::unexpected(std::move(repr.error()));
  return repr->utf8().find('\r') != std::string::npos ? strf("{}\r\n", *repr) : strf("{}\n", *repr);
}

std::expected<void, StarException> OutputOnSeparateLines::out(FormattedJson const& json) {
  auto repr = reprWithLineEnding(json);
  if (!repr)
    return std::unexpected(std::move(repr.error()));
  coutf("{}", *repr);
  return {};
}

std::expected<void, StarException> OutputOnSeparateLines::flush() {
  return {};
}

std::expected<void, StarException> ArrayOutput::out(FormattedJson const& json) {
  if (!m_unique || !m_results.contains(json))
    m_results.append(json);
  return {};
}

std::expected<void, StarException> ArrayOutput::flush() {
  auto array = FormattedJson::createArray(m_results);
  if (!array)
    return std::unexpected(std::move(array.error()));
  auto repr = reprWithLineEnding(*array);
  if (!repr)
    return std::unexpected(std::move(repr.error()));
  coutf("{}", *repr);
  return {};
}

std::expected<FormattedJson, StarException> Star::addOrSet(bool add,
    std::shared_ptr<::Star::JsonPath::Path> path,
    FormattedJson const& input,
    InsertLocation insertLocation,
    FormattedJson const& value) {
  JsonPath::EmptyPathOp<FormattedJson> emptyPathOp = [&value, add](FormattedJson const& document)
      -> std::expected<FormattedJson, StarException> {
    if (!add || document.type() == Json::Type::Null)
      return value;
    return std::unexpected(JsonException("Cannot add a value to the entire document, it is not empty."));
  };
  JsonPath::ObjectOp<FormattedJson> objectOp = [&value, &insertLocation](
      FormattedJson const& object, String const& key) -> std::expected<FormattedJson, StarException> {
    if (std::holds_alternative<AtBeginning>(insertLocation))
      return object.prepend(key, value);
    if (std::holds_alternative<AtEnd>(insertLocation))
      return object.append(key, value);
    if (std::holds_alternative<BeforeKey>(insertLocation))
      return object.insertBefore(key, value, std::get<BeforeKey>(insertLocation).key);
    if (std::holds_alternative<AfterKey>(insertLocation))
      return object.insertAfter(key, value, std::get<AfterKey>(insertLocation).key);
    return object.set(key, value);
  };
  JsonPath::ArrayOp<FormattedJson> arrayOp = [&value, add](FormattedJson const& array, std::optional<std::size_t> i)
      -> std::expected<FormattedJson, StarException> {
    if (i) {
      if (add)
        return array.insert(*i, value);
      return array.set(*i, value);
    }
    return array.append(value);
  };
  return path->apply(input, emptyPathOp, objectOp, arrayOp);
}

[[nodiscard]]
std::expected<void, StarException> forEachFileRecursive(Logger& logger, String const& directory,
    std::function<std::expected<void, StarException>(String const&)> const& func) {
  auto entries = File::directory_entries(directory);
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& entry : *entries) {
    String filename = File::relative_to(directory, entry.first);
    if (entry.second) {
      auto symlink = File::is_symlink(filename);
      if (!symlink)
        return std::unexpected(std::move(symlink.error()));
      if (*symlink) {
        auto logged = logger.warn("json_tool: Not descending into '{}' because it is a symlink", filename);
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        continue;
      }
    }
    if (entry.second) {
      auto visited = forEachFileRecursive(logger, filename, func);
      if (!visited)
        return std::unexpected(std::move(visited.error()));
    } else {
      auto visited = func(filename);
      if (!visited)
        return std::unexpected(std::move(visited.error()));
    }
  }
  return {};
}

std::expected<StringList, StarException> Star::findFiles(Logger& logger, FindInput const& findArgs) {
  StringList matches;
  auto visited = forEachFileRecursive(logger, findArgs.directory,
      [&findArgs, &matches](String const& filename) -> std::expected<void, StarException> {
        auto matchesSuffix = filename.endsWith(findArgs.filenameSuffix);
        if (!matchesSuffix)
          return std::unexpected(std::move(matchesSuffix.error()));
        if (*matchesSuffix)
          matches.append(filename);
        return {};
      });
  if (!visited)
    return std::unexpected(std::move(visited.error()));
  return matches;
}

using JsonOutput = std::function<std::expected<void, StarException>(FormattedJson const&)>;

[[nodiscard]]
std::expected<void, StarException> forEachChild(FormattedJson const& parent, JsonOutput const& output) {
  if (parent.isType(Json::Type::Object)) {
    auto object = parent.toJson().toObject();
    if (!object)
      return std::unexpected(std::move(object.error()));
    for (auto const& key : object->keys()) {
      auto child = parent.get(key);
      if (!child)
        return std::unexpected(std::move(child.error()));
      auto emitted = output(*child);
      if (!emitted)
        return std::unexpected(std::move(emitted.error()));
    }
  } else if (parent.isType(Json::Type::Array)) {
    auto size = parent.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    for (std::size_t i = 0; i < *size; ++i) {
      auto child = parent.get(i);
      if (!child)
        return std::unexpected(std::move(child.error()));
      auto emitted = output(*child);
      if (!emitted)
        return std::unexpected(std::move(emitted.error()));
    }
  } else {
    return std::unexpected(JsonPath::TraversalException::format(
        "Cannot get the children of Json type {}, must be either Array or Object", parent.typeName()));
  }
  return {};
}

[[nodiscard]]
std::expected<void, StarException> process(JsonOutput const& output,
    Command const& command, Options const& options, FormattedJson const& input) {
  if (std::holds_alternative<GetCommand>(command)) {
    auto const& get = std::get<GetCommand>(command);
    auto value = get.path->get(input);
    if (!value) {
      if (get.opt && value.error().is<JsonPath::TraversalException>())
        return {};
      return std::unexpected(std::move(value.error()));
    }
    auto emitted = get.children ? forEachChild(*value, output) : output(*value);
    if (!emitted && get.opt && emitted.error().is<JsonPath::TraversalException>())
      return {};
    return emitted;
  }
  if (std::holds_alternative<SetCommand>(command)) {
    auto const& set = std::get<SetCommand>(command);
    auto value = addOrSet(false, set.path, input, options.insertLocation, set.value);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return output(*value);
  }
  if (std::holds_alternative<AddCommand>(command)) {
    auto const& add = std::get<AddCommand>(command);
    auto value = addOrSet(true, add.path, input, options.insertLocation, add.value);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return output(*value);
  }
  if (std::holds_alternative<RemoveCommand>(command)) {
    auto const& remove = std::get<RemoveCommand>(command);
    auto value = remove.path->remove(input);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return output(*value);
  }
  return output(input);
}

[[nodiscard]]
std::expected<void, StarException> process(
    JsonOutput const& output, Command const& command, Options const& options, String const& input) {
  auto json = FormattedJson::parse(input);
  if (!json)
    return std::unexpected(std::move(json.error()));
  return process(output, command, options, *json);
}

[[nodiscard]]
std::expected<void, StarException> process(Logger& logger,
    JsonOutput output, Command const& command, Options const& options, Input const& input) {
  if (std::holds_alternative<JsonLiteralInput>(input))
    return process(output, command, options, std::get<JsonLiteralInput>(input).json);
  StringList files;
  if (std::holds_alternative<FileInput>(input)) {
    files.append(std::get<FileInput>(input).filename);
  } else {
    auto found = findFiles(logger, std::get<FindInput>(input));
    if (!found)
      return std::unexpected(std::move(found.error()));
    files = std::move(*found);
  }
  for (String const& file : files) {
    if (options.inPlace) {
      output = [&file](FormattedJson const& json) -> std::expected<void, StarException> {
        auto repr = reprWithLineEnding(json);
        if (!repr)
          return std::unexpected(std::move(repr.error()));
        return File::overwrite_file_with_rename(*repr, file);
      };
    }
    auto contents = File::read_file_string(file);
    if (!contents)
      return std::unexpected(std::move(contents.error()));
    auto processed = process(output, command, options, *contents);
    if (!processed)
      return std::unexpected(std::move(processed.error()));
  }
  return {};
}

std::shared_ptr<::Star::JsonPath::Path> parsePath(String const& path) {
  if (path.utf8().starts_with("/"))
    return std::make_shared<JsonPath::Pointer>(path);
  return std::make_shared<JsonPath::QueryPath>(path);
}

std::pair<std::shared_ptr<::Star::JsonPath::Path>, bool> parseGetPath(String const& path) {
  auto text = path.utf8();
  bool children = false;
  if (text.ends_with("/*") || text.ends_with(".*")) {
    text.resize(text.size() - 2);
    children = true;
  } else if (text.ends_with("[*]")) {
    text.resize(text.size() - 3);
    children = true;
  }
  return {parsePath(String(std::move(text))), children};
}

[[nodiscard]]
std::expected<std::optional<ParsedArgs>, StarException> parseArgs(int argc, char** argv) {
  ParsedArgs parsed;
  int next = 1;
  auto available = [&] { return argc - next; };
  auto take = [&] { return String(std::string_view(argv[next++])); };
  auto noCommand = [&] { return std::holds_alternative<std::monostate>(parsed.command); };
  auto noLocation = [&] { return std::holds_alternative<std::monostate>(parsed.options.insertLocation); };

  while (available() > 0) {
    String arg = take();
    if (arg == "--get" || arg == "--opt") {
      if (!noCommand() || available() < 1)
        return {};
      auto [path, children] = parseGetPath(take());
      parsed.command = GetCommand{std::move(path), arg == "--opt", children};
    } else if (arg == "--set" || arg == "--add") {
      if (!noCommand() || available() < 2)
        return {};
      auto path = parsePath(take());
      auto value = FormattedJson::parse(take());
      if (!value)
        return std::unexpected(std::move(value.error()));
      if (arg == "--set")
        parsed.command = SetCommand{std::move(path), std::move(*value)};
      else
        parsed.command = AddCommand{std::move(path), std::move(*value)};
    } else if (arg == "--remove" || arg == "--edit") {
      if (!noCommand() || available() < 1)
        return {};
      auto path = parsePath(take());
      if (arg == "--remove")
        parsed.command = RemoveCommand{std::move(path)};
      else
        parsed.command = EditCommand{std::move(path)};
    } else if (arg == "--editor-image") {
      if (available() < 1)
        return {};
      parsed.options.editorImages.append(parsePath(take()));
    } else if (arg == "--input") {
      if (available() < 1)
        return {};
      String format = take();
      if (format == "json" || format == "generic")
        parsed.options.editFormat = std::make_shared<GenericInputFormat>();
      else if (format == "css" || format == "csv")
        parsed.options.editFormat = std::make_shared<CommaSeparatedStrings>();
      else if (format == "string")
        parsed.options.editFormat = std::make_shared<StringInputFormat>();
      else
        return {};
    } else if (arg == "--array" || arg == "--array-unique") {
      parsed.options.output = std::make_shared<ArrayOutput>(arg == "--array-unique");
    } else if (arg == "--help") {
      return {};
    } else if (arg == "-j") {
      if (available() < 1)
        return {};
      parsed.inputs.append(JsonLiteralInput{take()});
    } else if (arg == "--find") {
      if (available() < 2)
        return {};
      String directory = take();
      String suffix = take();
      parsed.inputs.append(FindInput{std::move(directory), std::move(suffix)});
    } else if (arg == "-i") {
      parsed.options.inPlace = true;
    } else if (arg == "--at") {
      if (!noLocation() || available() < 1)
        return {};
      String position = take();
      if (position == "beginning" || position == "start")
        parsed.options.insertLocation = AtBeginning{};
      else if (position == "end")
        parsed.options.insertLocation = AtEnd{};
      else
        return {};
    } else if (arg == "--before" || arg == "--after") {
      if (!noLocation() || available() < 1)
        return {};
      String key = take();
      if (arg == "--before")
        parsed.options.insertLocation = BeforeKey{std::move(key)};
      else
        parsed.options.insertLocation = AfterKey{std::move(key)};
    } else {
      auto exists = File::exists(arg);
      if (!exists)
        return std::unexpected(std::move(exists.error()));
      if (!*exists) {
        cerrf("File {} doesn't exist\n", arg);
        return {};
      }
      parsed.inputs.append(FileInput{std::move(arg)});
    }
  }

  if (!parsed.options.output)
    parsed.options.output = std::make_shared<OutputOnSeparateLines>();
  bool anyFileInputs = false;
  bool anyNonFileInputs = false;
  for (auto const& input : parsed.inputs) {
    if (std::holds_alternative<FindInput>(input) || std::holds_alternative<FileInput>(input))
      anyFileInputs = true;
    else
      anyNonFileInputs = true;
  }
  bool editing = std::holds_alternative<EditCommand>(parsed.command);
  if (editing && !anyFileInputs) {
    cerrf("Files to edit must be supplied when using --edit.\n");
    return {};
  }
  if (parsed.options.inPlace && !anyFileInputs) {
    cerrf("In-place writing (-i) can only be used with files specified on the command line.\n");
    return {};
  }
  if (parsed.options.inPlace && editing) {
    cerrf("Interactive edit (--edit) is always in-place. Explicitly specifying -i is not needed.\n");
    return {};
  }
  if (editing && anyNonFileInputs) {
    cerrf("Interactive edit (--edit) can only be used with file input sources.\n");
    return {};
  }
  if (parsed.options.editFormat && !editing) {
    cerrf("--input can only be used with --edit.\n");
    return {};
  }
  if (!parsed.options.editFormat && editing)
    parsed.options.editFormat = std::make_shared<GenericInputFormat>();
  return std::optional<ParsedArgs>(std::move(parsed));
}

[[nodiscard]]
std::expected<String, StarException> readStdin() {
  String result;
  char buffer[1024];
  while (!std::feof(stdin)) {
    std::size_t readBytes = std::fread(buffer, 1, sizeof(buffer), stdin);
    result.append(std::string_view(buffer, readBytes));
    if (std::ferror(stdin))
      return std::unexpected(IOException("Failed reading JSON from standard input"));
  }
  return result;
}

[[nodiscard]]
std::expected<int, StarException> run(int argc, char** argv) {
    Logger logger;
    auto parsed = parseArgs(argc, argv);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    if (!*parsed) {
      cerrf("Usage: {} [--get <json-path>] (-j <json> | <json-file>*)\n", argv[0]);
      cerrf(
          "Usage: {} --set <json-path> <json> [-i] [(--at (beginning|end) | --before <key> | --after <key>)] (-j "
          "<json> | <json-file>*)\n",
          argv[0]);
      cerrf(
          "Usage: {} --add <json-path> <json> [-i] [(--at (beginning|end) | --before <key> | --after <key>)] (-j "
          "<json> | <json-file>*)\n",
          argv[0]);
      cerrf(
          "Usage: {} --edit <json-path> [(--at (beginning|end) | --before <key> | --after <key>)] [--input "
          "(csv|json|string)] <json-file>+\n",
          argv[0]);
      cerrf("\n");
      cerrf("Example: {} --get /dialog/0/message guard.npctype\n", argv[0]);
      cerrf("Example: {} --get 'foo[0]' -j '{{\"foo\":[0,1,2,3]}}'\n", argv[0]);
      cerrf("Example: {} --edit /tags --input csv --find ../assets/ .object\n", argv[0]);
      return 1;
    }

    ParsedArgs const& parsedArgs = **parsed;
    auto const& output = parsedArgs.options.output;
    if (std::holds_alternative<EditCommand>(parsedArgs.command)) {
      return edit(logger, argc, argv, std::get<EditCommand>(parsedArgs.command).path, parsedArgs.options, parsedArgs.inputs);
    }
    if (parsedArgs.inputs.empty()) {
      auto input = readStdin();
      if (!input)
        return std::unexpected(std::move(input.error()));
      auto processed = process(output->toFunction(), parsedArgs.command, parsedArgs.options, *input);
      if (!processed)
        return std::unexpected(std::move(processed.error()));
    } else {
      for (Input const& input : parsedArgs.inputs) {
        auto processed = process(logger, output->toFunction(), parsedArgs.command, parsedArgs.options, input);
        if (!processed)
          return std::unexpected(std::move(processed.error()));
      }
    }
    auto flushed = output->flush();
    if (!flushed)
      return std::unexpected(std::move(flushed.error()));
    return 0;
}

int main(int argc, char** argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime) {
    cerrf("Error initializing native runtime: {}\n", outputException(ExceptionDiagnostic(nativeRuntime.error()), true));
    return 1;
  }
  auto result = run(argc, argv);
  if (result)
    return *result;
  if (result.error().is<JsonParsingException>() || result.error().is<JsonException>())
    cerrf("{}\n", result.error().what());
  else
    cerrf("Error: {}\n", outputException(ExceptionDiagnostic(result.error()), true));
  return 1;
}
