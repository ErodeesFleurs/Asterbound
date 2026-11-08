import std;
import star.format;
import star.string;
import star.unicode;
import star.json;
import star.json_extra;
import star.lexical_cast;
import star.json_path;

namespace Star::JsonPath {

std::expected<TypeHint, StarException> parsePointer(String& buffer, String const& path, Utf8Cursor& cursor) {
  buffer.clear();
  if (cursor.at_end())
    return std::unexpected(ParsingException::format("Missing leading '/' in Json pointer \"{}\"", path));
  auto character = cursor.read();
  if (!character)
    return std::unexpected(std::move(character.error()));
  if (*character != '/')
    return std::unexpected(ParsingException::format("Missing leading '/' in Json pointer \"{}\"", path));
  auto advanced = cursor.advance();
  if (!advanced)
    return std::unexpected(std::move(advanced.error()));
  while (!cursor.at_end()) {
    character = cursor.read();
    if (!character)
      return std::unexpected(std::move(character.error()));
    if (*character == '/')
      break;
    if (*character == '~') {
      advanced = cursor.advance();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
      if (cursor.at_end())
        return std::unexpected(ParsingException::format("Incomplete escape sequence in Json pointer \"{}\"", path));
      character = cursor.read();
      if (!character)
        return std::unexpected(std::move(character.error()));
      if (*character != '0' && *character != '1')
        return std::unexpected(ParsingException::format("Invalid escape sequence in Json pointer \"{}\"", path));
      auto appended = buffer.append(*character == '0' ? '~' : '/');
      if (!appended)
        return std::unexpected(std::move(appended.error()));
    } else {
      auto appended = buffer.append(*character);
      if (!appended)
        return std::unexpected(std::move(appended.error()));
    }
    advanced = cursor.advance();
    if (!advanced)
      return std::unexpected(std::move(advanced.error()));
  }
  auto index = maybe_lexical_cast<std::size_t>(buffer);
  if (index || (buffer == "-" && cursor.at_end()))
    return TypeHint::Array;
  return TypeHint::Object;
}

std::expected<TypeHint, StarException> parseQueryPath(String& buffer, String const& path, Utf8Cursor& cursor) {
  buffer.clear();
  if (cursor.at_end())
    return TypeHint::Object;
  auto character = cursor.read();
  if (!character)
    return std::unexpected(std::move(character.error()));
  if (*character == '.')
    return std::unexpected(ParsingException::format("Entry starts with '.' in query path \"{}\"", path));

  TypeHint hint = TypeHint::Object;
  if (*character == '[') {
    hint = TypeHint::Array;
    auto advanced = cursor.advance();
    if (!advanced)
      return std::unexpected(std::move(advanced.error()));
    while (!cursor.at_end()) {
      character = cursor.read();
      if (!character)
        return std::unexpected(std::move(character.error()));
      if (*character < '0' || *character > '9')
        break;
      auto appended = buffer.append(*character);
      if (!appended)
        return std::unexpected(std::move(appended.error()));
      advanced = cursor.advance();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    }
    if (cursor.at_end() || *character != ']')
      return std::unexpected(ParsingException::format("Array has no trailing ']' or has invalid character in query path \"{}\"", path));
    advanced = cursor.advance();
    if (!advanced)
      return std::unexpected(std::move(advanced.error()));
  } else {
    while (!cursor.at_end()) {
      character = cursor.read();
      if (!character)
        return std::unexpected(std::move(character.error()));
      if (*character == '.' || *character == '[')
        break;
      auto appended = buffer.append(*character);
      if (!appended)
        return std::unexpected(std::move(appended.error()));
      auto advanced = cursor.advance();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    }
  }
  // Consume a single trailing dot without inspecting the following component.
  if (!cursor.at_end()) {
    character = cursor.read();
    if (!character)
      return std::unexpected(std::move(character.error()));
    if (*character == '.') {
      auto advanced = cursor.advance();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    }
  }
  return hint;
}

namespace {

template <bool Optional>
std::expected<std::optional<std::reference_wrapper<Json const>>, StarException>
findPath(Json const& base, PathParser const& parser, String const& path) {
  if (!parser)
    return std::unexpected(ParsingException("Cannot invoke an empty Json path parser"));
  String buffer;
  buffer.reserve(path.utf8_size());
  Utf8Cursor cursor(path.utf8());
  auto value = std::cref(base);
  while (!cursor.at_end()) {
    auto parsed = parser(buffer, path, cursor);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    if (value.get().type() == Json::Type::Array) {
      if (buffer == "-") {
        if constexpr (Optional)
          return std::nullopt;
        return std::unexpected(
          TraversalException::format("Tried to get key '{}' in non-object type in pathGet(\"{}\")", buffer, path));
      }
      auto index = maybe_lexical_cast<std::size_t>(buffer);
      if (!index) {
        if constexpr (Optional)
          return std::nullopt;
        return std::unexpected(
          TraversalException::format("Cannot parse '{}' as index in pathGet(\"{}\")", buffer, path));
      }
      auto child = value.get().find(*index);
      if (!child)
        return std::unexpected(std::move(child.error()));
      if (!*child) {
        if constexpr (Optional)
          return std::nullopt;
        return std::unexpected(TraversalException::format("Index {} out of range in pathGet(\"{}\")", buffer, path));
      }
      value = **child;
    } else if (value.get().type() == Json::Type::Object) {
      auto child = value.get().find(buffer);
      if (!child)
        return std::unexpected(std::move(child.error()));
      if (!*child) {
        if constexpr (Optional)
          return std::nullopt;
        return std::unexpected(TraversalException::format("No such key '{}' in pathGet(\"{}\")", buffer, path));
      }
      value = **child;
    } else {
      if constexpr (Optional)
        return std::nullopt;
      return std::unexpected(
        TraversalException::format("Tried to get key '{}' in non-object type in pathGet(\"{}\")", buffer, path));
    }
  }
  return value;
}

}// namespace

std::expected<std::reference_wrapper<Json const>, StarException> pathGetRef(Json const& base, PathParser const& parser,
                                                                            String const& path) {
  auto value = findPath<false>(base, parser, path);
  if (!value)
    return std::unexpected(std::move(value.error()));
  return **value;
}

std::expected<std::optional<std::reference_wrapper<Json const>>, StarException>
pathFindRef(Json const& base, PathParser const& parser, String const& path) {
  return findPath<true>(base, parser, path);
}
}
