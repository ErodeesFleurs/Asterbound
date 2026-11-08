import std;
import star.format;
import star.string_view;

import star.lexical_cast;

namespace Star {

BadLexicalCast lexical_cast_error(std::errc ec, std::string_view str) {
  if (ec == std::errc::invalid_argument)
    return BadLexicalCast(Star::format("Lexical cast failed on '{}' (invalid argument)", str));
  else
    return BadLexicalCast(Star::format("Lexical cast failed on '{}'", str));
}

}