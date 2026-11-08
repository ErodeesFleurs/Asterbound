#include <ctype.h>
import std;
import star.list;
import star.unicode;
import star.string;
import star.byte_array;
import star.encode;
import star.bytes;
import star.format;
import star.shell_parser;

namespace Star {

ShellParser::ShellParser()
  : m_current(), m_end(), m_quotedType('\0') {}

auto ShellParser::tokenize(String const& command) -> std::expected<List<Token>, StarException> {
  List<Token> res;

  auto initialized = init(command);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));

  while (notDone()) {
    auto token = word();
    if (!token)
      return std::unexpected(std::move(token.error()));
    res.push_back(Token{.type = TokenType::Word, .token = std::move(*token)});
  }

  return res;
}

std::expected<StringList, StarException> ShellParser::tokenizeToStringList(String const& command) {
  StringList res;
  auto tokens = tokenize(command);
  if (!tokens)
    return std::unexpected(std::move(tokens.error()));
  for (auto& token : *tokens) {
    if (token.type == TokenType::Word) {
      res.push_back(std::move(token.token));
    }
  }

  return res;
}

std::expected<void, StarException> ShellParser::init(String const& string) {
  auto begin = string.begin();
  if (!begin)
    return std::unexpected(std::move(begin.error()));
  m_begin = *begin;
  m_current = m_begin;
  m_end = string.end();
  m_quotedType = '\0';
  return {};
}

std::expected<String, StarException> ShellParser::word() {
  String res;

  while (notDone()) {
    auto letter = *current();
    bool escapedLetter = false;

    if (letter == '\\') {
      escapedLetter = true;
      auto parsedLetter = parseBackslash();
      if (!parsedLetter)
        return std::unexpected(std::move(parsedLetter.error()));
      letter = *parsedLetter;
    }

    if (!escapedLetter) {
      if (is_space(letter) && !inQuotedString()) {
        next();
        if (!res.empty()) {
          return res;
        }
        continue;
      }

      if (isQuote(letter)) {
        if (inQuotedString() && letter == m_quotedType) {
          m_quotedType = '\0';
          next();
          continue;
        }

        if (!inQuotedString()) {
          m_quotedType = letter;
          next();
          continue;
        }
      }
    }

    auto appended = res.append(letter);
    if (!appended)
      return std::unexpected(std::move(appended.error()));
    next();
  }

  return res;
}

bool ShellParser::is_space(Char letter) const {
  return String::is_space(letter);
}

bool ShellParser::isQuote(Char letter) const {
  return letter == '\'' || letter == '"';
}

bool ShellParser::inQuotedString() const {
  return m_quotedType != '\0';
}

auto ShellParser::current() const -> std::optional<Char> {
  if (m_current == m_end) {
    return {};
  }

  return *m_current;
}

auto ShellParser::next() -> std::optional<Char> {
  if (m_current != m_end) {
    ++m_current;
  }

  return current();
}

auto ShellParser::previous() -> std::optional<Char> {
  if (m_current != m_begin) {
    --m_current;
  }

  return current();
}

auto ShellParser::parseBackslash() -> std::expected<Char, StarException> {
  auto letter = next();

  if (!letter) {
    return '\\';
  }

  switch (*letter) {
    case ' ':
      return ' ';
    case 'n':
      return '\n';
    case 't':
      return '\t';
    case 'r':
      return '\r';
    case 'b':
      return '\b';
    case 'v':
      return '\v';
    case 'f':
      return '\f';
    case 'a':
      return '\a';
    case '\'':
      return '\'';
    case '"':
      return '"';
    case '\\':
      return '\\';
    case '0':
      return '\0';
    case 'u': {
      auto parsedLetter = parseUnicodeEscapeSequence();
      if (!parsedLetter)
        return std::unexpected(std::move(parsedLetter.error()));
      auto letter = *parsedLetter;
      if (is_utf16_lead_surrogate(letter)) {
        auto shouldBeSlash = next();
        if (shouldBeSlash && shouldBeSlash == '\\') {
          auto shouldBeU = next();
          if (shouldBeU && shouldBeU == 'u') {
            return parseUnicodeEscapeSequence(letter);
          } else {
            previous();
          }
        }
        previous();
        return STAR_UTF32_REPLACEMENT_CHAR;
      } else {
        return letter;
      }
    }
    default:
      return *letter;
  }
}

auto ShellParser::parseUnicodeEscapeSequence(std::optional<Char> previousCodepoint) -> std::expected<Char, StarException> {
  String codepoint;

  auto letter = current();

  while (!is_space(*letter) && codepoint.utf8_size() < 4) {
    auto letter = next();
    if (!letter) {
      break;
    }

    if (!std::isxdigit(*letter)) {
      return STAR_UTF32_REPLACEMENT_CHAR;
    }

    auto appended = codepoint.append(*letter);
    if (!appended)
      return std::unexpected(std::move(appended.error()));
  }

  if (codepoint.empty()) {
    return 'u';
  }

  if (codepoint.utf8_size() != 4) // exactly 4 digits are required by \u
    return STAR_UTF32_REPLACEMENT_CHAR;

  auto converted = hex_string_to_utf32(codepoint.utf8(), previousCodepoint);
  if (!converted)
    return STAR_UTF32_REPLACEMENT_CHAR;
  return *converted;
}

bool ShellParser::notDone() const {
  return m_current != m_end;
}

}
