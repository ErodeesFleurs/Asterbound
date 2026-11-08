import std;
import star.format;
import star.string_view;

import star.lexical_cast;

namespace Star {

BadLexicalCast lexicalCastError(std::errc ec, std::string_view str) {
  if (ec == std::errc::invalid_argument)
    return BadLexicalCast(Star::format("Lexical cast failed on '{}' (invalid argument)", str));
  else
    return BadLexicalCast(Star::format("Lexical cast failed on '{}'", str));
}

template <>
bool tryLexicalCast(bool& result, std::string_view text) {
  if (text == "true") {
    result = true;
    return true;
  }
  if (text == "false") {
    result = false;
    return true;
  }

  // The "true" case used to fall through to the assignment below, so a
  // successful cast of "true" reported success but stored false.
  return false;
}

template <>
std::expected<bool, StarException> lexicalCast(std::string_view text) {
  if (text == "true")
    return true;
  else if (text != "false")
    return std::unexpected(lexicalCastError(std::errc(), text));

  return false;
}

}