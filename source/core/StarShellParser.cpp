import std;
import star.unicode;
import star.string;
import star.format;
import star.shell_parser;

namespace Star {

namespace {

char32_t read_hex_unit(std::string_view& input) {
  char32_t unit = 0;
  for (unsigned i = 0; i < 4; ++i) {
    if (input.empty())
      return U'\uFFFD';
    char digit = input.front();
    unsigned value;
    if (digit >= '0' && digit <= '9')
      value = digit - '0';
    else if (digit >= 'A' && digit <= 'F')
      value = digit - 'A' + 10;
    else if (digit >= 'a' && digit <= 'f')
      value = digit - 'a' + 10;
    else
      return U'\uFFFD';
    unit = (unit << 4) | value;
    input.remove_prefix(1);
  }
  return unit;
}

std::expected<char32_t, StarException> take_codepoint(std::string_view& input) {
  char32_t point = static_cast<unsigned char>(input.front());
  if (point < 0x80) {
    input.remove_prefix(1);
  } else {
    auto width = utf8_decode_char(input, point);
    if (!width)
      return std::unexpected(std::move(width.error()));
    input.remove_prefix(*width);
  }
  return point;
}

std::expected<char32_t, StarException> read_escape(std::string_view& input) {
  if (input.empty())
    return '\\';
  auto escaped = take_codepoint(input);
  if (!escaped)
    return escaped;
  switch (*escaped) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case 'b': return '\b';
    case 'v': return '\v';
    case 'f': return '\f';
    case 'a': return '\a';
    case '0': return '\0';
    case 'u': break;
    default: return escaped;
  }
  if (input.empty())
    return 'u';
  auto unit = read_hex_unit(input);
  if (is_utf16_trail_surrogate(unit))
    return U'\uFFFD';
  if (!is_utf16_lead_surrogate(unit))
    return unit;
  if (input.starts_with("\\u")) {
    auto lookahead = input.substr(2);
    auto trail = read_hex_unit(lookahead);
    if (is_utf16_trail_surrogate(trail)) {
      input = lookahead;
      return utf32_from_utf16_surrogate_pair(unit, trail);
    }
  }
  return U'\uFFFD';
}

}

std::expected<StringList, StarException> split_shell_words(String const& command) {
  StringList words;
  std::string_view input = command.utf8();
  char32_t quote = 0;
  bool word_started = false;
  auto current_word = [&]() -> String& {
    if (!word_started) {
      word_started = true;
      return words.emplace_back();
    }
    return words.back();
  };
  while (!input.empty()) {
    // Printable non-syntax ASCII can be copied without decoding or re-encoding.
    std::size_t run = 0;
    while (run < input.size()) {
      auto byte = static_cast<unsigned char>(input[run]);
      if (byte <= ' ' || byte >= 0x80 || byte == '\\' || byte == '\'' || byte == '"')
        break;
      ++run;
    }
    if (run != 0) {
      current_word().append(input.substr(0, run));
      input.remove_prefix(run);
      continue;
    }
    auto point = take_codepoint(input);
    if (!point)
      return std::unexpected(std::move(point.error()));
    if (!quote && String::is_space(*point)) {
      word_started = false;
      continue;
    }
    if ((*point == '\'' || *point == '"') && (!quote || quote == *point)) {
      current_word();
      quote = quote ? 0 : *point;
      continue;
    }
    if (*point == '\\') {
      point = read_escape(input);
      if (!point)
        return std::unexpected(std::move(point.error()));
    }
    auto appended = current_word().append(*point);
    if (!appended)
      return std::unexpected(std::move(appended.error()));
  }
  return words;
}

}
