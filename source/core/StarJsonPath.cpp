import std;
import star.format;
import star.string;
import star.unicode;
import star.json;
import star.json_extra;
import star.lexical_cast;
import star.json_path;

namespace Star::JsonPath {

std::expected<TypeHint, StarException> parse_pointer(String& buffer, String const& path, Utf8Cursor& cursor) {
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

std::expected<TypeHint, StarException> parse_query_path(String& buffer, String const& path, Utf8Cursor& cursor) {
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
}
