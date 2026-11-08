module;
import star.format;
import std;

import star.byte_array;
import star.unicode;

namespace Star {

struct JsonStream {
  // Each text range is borrowed only through its synchronous callback.
  virtual ~JsonStream() = default;
  virtual std::expected<void, StarException> beginObject() = 0;
  virtual std::expected<void, StarException> objectKey(std::span<char32_t const>) = 0;
  virtual std::expected<void, StarException> endObject() = 0;
  virtual std::expected<void, StarException> beginArray() = 0;
  virtual std::expected<void, StarException> endArray() = 0;
  virtual std::expected<void, StarException> putString(std::span<char32_t const>) = 0;
  // UTF-8 producers retain their borrowed storage through the synchronous call.
  // UTF-32 consumers receive a complete, validated string before their callback.
  virtual std::expected<void, StarException> objectKeyUtf8(std::string_view bytes) {
    auto text = decodeUtf8(bytes);
    if (!text) return std::unexpected(std::move(text.error()));
    return objectKey(*text);
  }
  virtual std::expected<void, StarException> putStringUtf8(std::string_view bytes) {
    auto text = decodeUtf8(bytes);
    if (!text) return std::unexpected(std::move(text.error()));
    return putString(*text);
  }
  // Numeric tokens are ASCII, borrowed through the synchronous callback.
  virtual std::expected<void, StarException> putDouble(std::string_view) = 0;
  virtual std::expected<void, StarException> putInteger(std::string_view) = 0;
  virtual std::expected<void, StarException> putBoolean(bool) = 0;
  virtual std::expected<void, StarException> putNull() = 0;
  virtual std::expected<void, StarException> putWhitespace(std::span<char32_t const>) = 0;
  virtual std::expected<void, StarException> putWhitespaceUtf8(std::string_view bytes) {
    auto text = decodeUtf8(bytes);
    if (!text)
      return std::unexpected(std::move(text.error()));
    return putWhitespace(*text);
  }
  virtual std::expected<void, StarException> putColon() = 0;
  virtual std::expected<void, StarException> putComma() = 0;
private:
  static std::expected<std::u32string, StarException> decodeUtf8(std::string_view bytes) {
    std::u32string text;
    while (!bytes.empty()) {
      char32_t point;
      auto width = utf8_decode_char(bytes.data(), &point, bytes.size());
      if (!width) return std::unexpected(std::move(width.error()));
      text.push_back(point);
      bytes.remove_prefix(*width);
    }
    return text;
  }
};

struct JsonParserExceptionTag {
  static constexpr std::string_view name() { return "JsonParserException"; }
};
using JsonParserException = StarError<JsonParserExceptionTag, StarException>;

enum class JsonParseType : std::uint8_t {
  Top, // Top-level Object or Array
  Value, // Any singular Json value
  Sequence // Like an array, but without needing the [] or commas.
};

// Will parse JSON and output to a given JsonStream.  Parses an *extension* to
// the JSON format that includes comments.
template <typename InputIterator, bool RetainWhitespace = true> class JsonParser {
public:
  explicit JsonParser(JsonStream& stream) : m_stream(stream) {}

  std::expected<InputIterator, StarException> parse(InputIterator begin, InputIterator end,
      JsonParseType parseType = JsonParseType::Top) {
    m_current = begin;
    m_end = end;
    m_line = m_column = 0;
    auto initialized = read();
    if (!initialized)
      return diagnostic(std::move(initialized.error()));
    auto parsed = parseDocument(parseType);
    if (!parsed)
      return diagnostic(std::move(parsed.error()));
    return m_current;
  }

  std::size_t line() const { return m_line + 1; }
  std::size_t column() const { return m_column + 1; }

private:
  using CharArray = std::basic_string<char32_t>;

  std::unexpected<StarException> diagnostic(StarException error) const {
    return std::unexpected(error.withContext(Star::format("Error parsing json at {}:{}", line(), column())));
  }

  std::expected<void, StarException> error(std::string_view message) const {
    return std::unexpected(JsonParserException(std::string(message)));
  }

  std::expected<void, StarException> read() {
    if (m_current == m_end) {
      m_char = 0;
      return {};
    }
    if constexpr (requires { m_current.read(); }) {
      auto c = m_current.read();
      if (!c)
        return std::unexpected(std::move(c.error()));
      m_char = *c;
    } else {
      m_char = *m_current;
    }
    return {};
  }

  std::expected<void, StarException> next() {
    if (m_current == m_end)
      return {};
    if (m_char == '\n') {
      ++m_line;
      m_column = 0;
    } else {
      ++m_column;
    }
    if constexpr (requires { m_current.advance(); }) {
      auto advanced = m_current.advance();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    } else {
      ++m_current;
    }
    return read();
  }

  std::expected<void, StarException> parseDocument(JsonParseType parseType) {
    auto whitespace = white();
    if (!whitespace)
      return std::unexpected(std::move(whitespace.error()));
    std::expected<void, StarException> result;
    if (parseType == JsonParseType::Sequence)
      result = sequence();
    else if (parseType == JsonParseType::Top && m_char != '{' && m_char != '[')
      return error("expected JSON object or array at top level");
    else
      result = value();
    if (!result)
      return std::unexpected(std::move(result.error()));
    return white();
  }

  std::expected<void, StarException> value() {
    switch (m_char) {
      case '{': return object();
      case '[': return array();
      case '"': return parseString();
      case '-': return number();
      case 0: return error("unexpected end of stream parsing value");
      default: return m_char >= '0' && m_char <= '9' ? number() : word();
    }
  }

  std::expected<void, StarException> object() {
    auto advance = next();
    if (!advance)
      return std::unexpected(std::move(advance.error()));
    auto begun = m_stream.beginObject();
    if (!begun)
      return std::unexpected(std::move(begun.error()));
    auto whitespace = white();
    if (!whitespace)
      return std::unexpected(std::move(whitespace.error()));
    if (m_char == '}') {
      auto advanced = next();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
      return m_stream.endObject();
    }
    while (true) {
      auto written = parseString(true);
      if (!written)
        return std::unexpected(std::move(written.error()));
      whitespace = white();
      if (!whitespace)
        return std::unexpected(std::move(whitespace.error()));
      if (m_char != ':')
        return error("bad object, should be ':'");
      advance = next();
      if (!advance)
        return std::unexpected(std::move(advance.error()));
      written = m_stream.putColon();
      if (!written)
        return std::unexpected(std::move(written.error()));
      whitespace = white();
      if (!whitespace)
        return std::unexpected(std::move(whitespace.error()));
      auto parsed = value();
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      whitespace = white();
      if (!whitespace)
        return std::unexpected(std::move(whitespace.error()));
      if (m_char == '}') {
        advance = next();
        if (!advance)
          return std::unexpected(std::move(advance.error()));
        return m_stream.endObject();
      }
      if (m_char != ',')
        return error(m_char == 0 ? "unexpected end of stream parsing object." : "bad object, should be '}' or ','");
      advance = next();
      if (!advance)
        return std::unexpected(std::move(advance.error()));
      written = m_stream.putComma();
      if (!written)
        return std::unexpected(std::move(written.error()));
      whitespace = white();
      if (!whitespace)
        return std::unexpected(std::move(whitespace.error()));
    }
  }

  std::expected<void, StarException> array() {
    auto advanced = next();
    if (!advanced)
      return std::unexpected(std::move(advanced.error()));
    auto begun = m_stream.beginArray();
    if (!begun)
      return std::unexpected(std::move(begun.error()));
    auto whitespace = white();
    if (!whitespace)
      return std::unexpected(std::move(whitespace.error()));
    if (m_char == ']') {
      advanced = next();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
      return m_stream.endArray();
    }
    while (true) {
      auto parsed = value();
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      whitespace = white();
      if (!whitespace)
        return std::unexpected(std::move(whitespace.error()));
      if (m_char == ']') {
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
        return m_stream.endArray();
      }
      if (m_char != ',')
        return error(m_char == 0 ? "unexpected end of stream parsing array." : "bad array, should be ',' or ']'");
      advanced = next();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
      auto comma = m_stream.putComma();
      if (!comma)
        return std::unexpected(std::move(comma.error()));
      whitespace = white();
      if (!whitespace)
        return std::unexpected(std::move(whitespace.error()));
    }
  }

  std::expected<void, StarException> sequence() {
    auto begun = m_stream.beginArray();
    if (!begun)
      return std::unexpected(std::move(begun.error()));
    while (m_char != 0) {
      if (is_space(m_char)) {
        auto advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
        continue;
      }
      if (m_char == '{' || m_char == '[' || m_char == '"' || m_char == '-') {
        auto parsed = value();
        if (!parsed)
          return std::unexpected(std::move(parsed.error()));
      } else {
        auto begin = m_current;
        auto character = m_char;
        auto savedLine = m_line;
        auto savedColumn = m_column;
        bool recognized = false;
        if ((m_char >= '0' && m_char <= '9') || m_char == 't' || m_char == 'f' || m_char == 'n') {
          // Only lexical failures permit the sequence's existing bare-string
          // recovery. Stream/callback failures must never be swallowed.
          auto token = m_char >= '0' && m_char <= '9' ? number(true) : word(true);
          if (token) {
            recognized = true;
          } else if (!token.error().template is<JsonParserException>()) {
            return std::unexpected(std::move(token.error()));
          } else {
            m_current = begin;
            m_char = character;
            m_line = savedLine;
            m_column = savedColumn;
          }
        }
        if (!recognized) {
          CharArray text;
          do {
            text += m_char;
            auto advanced = next();
            if (!advanced)
              return std::unexpected(std::move(advanced.error()));
          } while (m_char != 0 && !is_space(m_char));
          auto written = m_stream.putString(text);
          if (!written)
            return std::unexpected(std::move(written.error()));
        }
      }
      auto advanced = next();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    }
    return m_stream.endArray();
  }


  std::expected<void, StarException> number(bool seq = false) {
    std::string buffer;
    auto begin = [&] {
      if constexpr (requires { m_current.utf8Position(); })
        return m_current.utf8Position();
      else
        return static_cast<char const*>(nullptr);
    }();
    bool isDouble = false;
    auto consume = [&]() -> std::expected<void, StarException> {
      if constexpr (!requires { m_current.utf8Position(); })
        buffer += static_cast<char>(m_char);
      return next();
    };
    if (m_char == '-') {
      auto result = consume();
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
    if (m_char == '0') {
      auto result = consume();
      if (!result)
        return std::unexpected(std::move(result.error()));
    } else if (m_char > '0' && m_char <= '9') {
      do {
        auto result = consume();
        if (!result)
          return std::unexpected(std::move(result.error()));
      } while (m_char >= '0' && m_char <= '9');
    } else {
      return error("bad number, must start with digit");
    }
    if (m_char == '.' || m_char == 'e' || m_char == 'E') {
      if (m_char == '.') {
        isDouble = true;
        auto result = consume();
        if (!result)
          return std::unexpected(std::move(result.error()));
        while (m_char >= '0' && m_char <= '9') {
          result = consume();
          if (!result)
            return std::unexpected(std::move(result.error()));
        }
      }
      if (m_char == 'e' || m_char == 'E') {
        isDouble = true;
        auto result = consume();
        if (!result)
          return std::unexpected(std::move(result.error()));
        if (m_char == '-' || m_char == '+') {
          result = consume();
          if (!result)
            return std::unexpected(std::move(result.error()));
        }
        while (m_char >= '0' && m_char <= '9') {
          result = consume();
          if (!result)
            return std::unexpected(std::move(result.error()));
        }
      }
    }
    if (seq && m_char != 0 && !is_space(m_char))
      return error("unexpected character after number");
    std::string_view token = buffer;
    if constexpr (requires { m_current.utf8Position(); })
      token = {begin, static_cast<std::size_t>(m_current.utf8Position() - begin)};
    return isDouble ? m_stream.putDouble(token) : m_stream.putInteger(token);
  }

  std::expected<void, StarException> word(bool seq = false) {
    char32_t first = m_char;
    std::u32string_view tail;
    if (first == 't')
      tail = U"rue";
    else if (first == 'f')
      tail = U"alse";
    else if (first == 'n')
      tail = U"ull";
    else
      return error("unexpected character parsing word");
    auto advanced = next();
    if (!advanced)
      return std::unexpected(std::move(advanced.error()));
    for (auto c : tail) {
      auto checked = check(c);
      if (!checked)
        return std::unexpected(std::move(checked.error()));
    }
    if (seq && m_char != 0 && !is_space(m_char))
      return error("unexpected character after word");
    return first == 'n' ? m_stream.putNull() : m_stream.putBoolean(first == 't');
  }

  std::expected<void, StarException> parseString(bool key = false) {
    if (m_char != '"')
      return error("bad string, should be '\"'");
    auto advanced = next();
    if (!advanced)
      return std::unexpected(std::move(advanced.error()));
    CharArray text;
    if constexpr (requires { m_current.utf8Position(); }) {
      // Decode only visited codepoints, retaining a borrowed byte token until
      // an escape actually requires the existing UTF-32 assembly path.
      auto begin = m_current.utf8Position();
      while (m_char != '\\' && m_char != '"' && m_char != 0) {
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
      }
      auto end = m_current.utf8Position();
      if (m_char == '"') {
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
        std::string_view bytes(begin, static_cast<std::size_t>(end - begin));
        return key ? m_stream.objectKeyUtf8(bytes) : m_stream.putStringUtf8(bytes);
      }
      while (begin != end) {
        char32_t point;
        auto width = utf8_decode_char(begin, &point, static_cast<std::size_t>(end - begin));
        if (!width)
          return std::unexpected(std::move(width.error()));
        text += point;
        begin += *width;
      }
    }
    while (true) {
      if (m_char == '\\') {
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
        if (m_char == 'u') {
          auto hex = readHexEscape();
          if (!hex)
            return std::unexpected(std::move(hex.error()));
          auto codepoint = hex_string_to_utf32(*hex);
          if (!codepoint)
            return std::unexpected(std::move(codepoint.error()));
          if (is_utf16_lead_surrogate(*codepoint)) {
            auto checked = check('\\');
            if (!checked)
              return std::unexpected(std::move(checked.error()));
            checked = check('u');
            if (!checked)
              return std::unexpected(std::move(checked.error()));
            hex = readHexDigits();
            if (!hex)
              return std::unexpected(std::move(hex.error()));
            codepoint = hex_string_to_utf32(*hex, *codepoint);
            if (!codepoint)
              return std::unexpected(std::move(codepoint.error()));
          }
          text += *codepoint;
        } else {
          switch (m_char) {
            case '"': text += '"'; break;
            case '\\': text += '\\'; break;
            case '/': text += '/'; break;
            case 'b': text += '\b'; break;
            case 'f': text += '\f'; break;
            case 'n': text += '\n'; break;
            case 'r': text += '\r'; break;
            case 't': text += '\t'; break;
            default: return std::unexpected(JsonParserException("bad string escape character"));
          }
          advanced = next();
          if (!advanced)
            return std::unexpected(std::move(advanced.error()));
        }
      } else if (m_char == '"') {
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
        return key ? m_stream.objectKey(text) : m_stream.putString(text);
      } else if (m_char == 0) {
        return std::unexpected(JsonParserException("unexpected end of stream reading string!"));
      } else {
        text += m_char;
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
      }
    }
  }

  std::expected<std::string, StarException> readHexEscape() {
    auto advanced = next();
    if (!advanced)
      return std::unexpected(std::move(advanced.error()));
    return readHexDigits();
  }

  std::expected<std::string, StarException> readHexDigits() {
    std::string hex;
    for (unsigned i = 0; i < 4; ++i) {
      if (m_char > 127)
        return std::unexpected(UnicodeException("Invalid unicode escape"));
      hex.push_back(static_cast<char>(m_char));
      auto advanced = next();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    }
    return hex;
  }

  std::expected<void, StarException> check(char32_t c) {
    if (m_char == 0)
      return error("unexpected end of stream parsing word");
    if (m_char != c)
      return error("unexpected character in word");
    return next();
  }

  std::expected<void, StarException> white() {
    CharArray buffer;
    char const* begin = nullptr;
    if constexpr (RetainWhitespace && requires { m_current.utf8Position(); })
      begin = m_current.utf8Position();
    auto consume = [&]() -> std::expected<void, StarException> {
      if constexpr (RetainWhitespace && !requires { m_current.utf8Position(); })
        buffer += m_char;
      return next();
    };
    while (m_current != m_end) {
      if (m_char == '/') {
        auto consumed = consume();
        if (!consumed)
          return std::unexpected(std::move(consumed.error()));
        if (m_char == '/') {
          do {
            consumed = consume();
            if (!consumed)
              return std::unexpected(std::move(consumed.error()));
          } while (m_current != m_end && m_char != '\n');
        } else if (m_char == '*') {
          consumed = consume();
          if (!consumed)
            return std::unexpected(std::move(consumed.error()));
          while (m_current != m_end) {
            auto c = m_char;
            consumed = consume();
            if (!consumed)
              return std::unexpected(std::move(consumed.error()));
            if (c == '*' && m_char == '/') {
              consumed = consume();
              if (!consumed)
                return std::unexpected(std::move(consumed.error()));
              break;
            }
            if (c != '*' && m_current == m_end)
              return error("/* comment has no matching */");
          }
        } else {
          return error("/ character in whitespace is not followed by '/' or '*', invalid comment");
        }
      } else if (is_space(m_char)) {
        auto consumed = consume();
        if (!consumed)
          return std::unexpected(std::move(consumed.error()));
      } else {
        break;
      }
    }
    if constexpr (RetainWhitespace) {
      if constexpr (requires { m_current.utf8Position(); }) {
        auto end = m_current.utf8Position();
        if (begin != end)
          return m_stream.putWhitespaceUtf8(std::string_view(begin, static_cast<std::size_t>(end - begin)));
      } else if (!buffer.empty()) {
        return m_stream.putWhitespace(buffer);
      }
    }
    return {};
  }

  static bool is_space(char32_t c) {
    return c == 0x20 || c == 0x09 || c == 0x0a || c == 0x0d || c == 0xfeff;
  }

  char32_t m_char = 0;
  InputIterator m_current;
  InputIterator m_end;
  std::size_t m_line = 0;
  std::size_t m_column = 0;
  JsonStream& m_stream;
};

// Write JSON through JsonStream interface.
template <typename OutputIterator, bool Utf8 = false>
class JsonWriter : public JsonStream {
public:
  JsonWriter(OutputIterator out, unsigned pretty = 0) : m_out(out), m_pretty(pretty) {}
  std::expected<void, StarException> beginObject() override {
    auto r = startValue();
    if (!r) return std::unexpected(std::move(r.error()));
    m_state.push_back(Object);
    return write('{');
  }
  std::expected<void, StarException> objectKey(std::span<char32_t const> s) override {
    if (state() != ObjectElement) m_state.push_back(ObjectElement);
    auto r = indent();
    if (!r) return std::unexpected(std::move(r.error()));
    r = write('"');
    if (!r) return std::unexpected(std::move(r.error()));
    for (std::size_t i = 0; i < s.size() && s[i]; ++i) {
      r = write(s[i]);
      if (!r) return std::unexpected(std::move(r.error()));
    }
    r = write('"');
    if (!r) return std::unexpected(std::move(r.error()));
    return m_pretty ? write(' ') : std::expected<void, StarException>{};
  }
  std::expected<void, StarException> objectKeyUtf8(std::string_view bytes) override {
    auto valid = utf8_length(bytes.data(), bytes.size());
    if (!valid) return std::unexpected(std::move(valid.error()));
    if (state() != ObjectElement) m_state.push_back(ObjectElement);
    auto r = indent();
    if (!r) return std::unexpected(std::move(r.error()));
    r = write('"');
    if (!r) return std::unexpected(std::move(r.error()));
    // Object keys historically bypass value escaping and stop at the first
    // NUL. Validate the entire key above, including that unprinted suffix.
    auto end = bytes.find('\0');
    r = utf8Text(bytes.substr(0, end));
    if (!r) return std::unexpected(std::move(r.error()));
    r = write('"');
    if (!r) return std::unexpected(std::move(r.error()));
    return m_pretty ? write(' ') : std::expected<void, StarException>{};
  }
  std::expected<void, StarException> endObject() override {
    bool nonempty = state() == ObjectElement;
    pop(Object);
    if (nonempty) {
      auto r = indent();
      if (!r) return std::unexpected(std::move(r.error()));
    }
    return write('}');
  }
  std::expected<void, StarException> beginArray() override {
    auto r = startValue();
    if (!r) return std::unexpected(std::move(r.error()));
    m_state.push_back(Array);
    return write('[');
  }
  std::expected<void, StarException> endArray() override { pop(Array); return write(']'); }
  std::expected<void, StarException> putString(std::span<char32_t const> s) override {
    auto r = startValue();
    if (!r) return std::unexpected(std::move(r.error()));
    return quoted(s);
  }
  std::expected<void, StarException> putStringUtf8(std::string_view bytes) override {
    // Preserve wide_string's all-or-nothing validation before startValue or any
    // output, but do not inspect later JSON values before earlier output.
    auto valid = utf8_length(bytes.data(), bytes.size());
    if (!valid) return std::unexpected(std::move(valid.error()));
    auto r = startValue();
    if (!r) return std::unexpected(std::move(r.error()));
    r = write('"');
    if (!r) return std::unexpected(std::move(r.error()));
    while (!bytes.empty()) {
      std::size_t count = 0;
      while (count < bytes.size()) {
        auto c = static_cast<unsigned char>(bytes[count]);
        if (c < 0x20 || c == '"' || c == '\\') break;
        ++count;
      }
      if (count != 0) {
        r = utf8Text(bytes.substr(0, count));
        bytes.remove_prefix(count);
      } else {
        r = escaped(static_cast<unsigned char>(bytes.front()));
        bytes.remove_prefix(1);
      }
      if (!r) return std::unexpected(std::move(r.error()));
    }
    return write('"');
  }
  std::expected<void, StarException> putDouble(std::string_view s) override {
    auto r = startValue();
    if (!r) return std::unexpected(std::move(r.error()));
    return asciiText(s);
  }
  std::expected<void, StarException> putInteger(std::string_view s) override { return putDouble(s); }
  std::expected<void, StarException> putBoolean(bool b) override { return putDouble(b ? "true" : "false"); }
  std::expected<void, StarException> putNull() override { return putDouble("null"); }
  std::expected<void, StarException> putWhitespace(std::span<char32_t const> s) override { return text(s); }
  std::expected<void, StarException> putWhitespaceUtf8(std::string_view bytes) override {
    auto valid = utf8_length(bytes.data(), bytes.size());
    if (!valid)
      return std::unexpected(std::move(valid.error()));
    return utf8Text(bytes);
  }
  std::expected<void, StarException> putColon() override {
    auto r = write(':');
    if (!r) return std::unexpected(std::move(r.error()));
    return m_pretty ? write(' ') : std::expected<void, StarException>{};
  }
  std::expected<void, StarException> putComma() override { return write(','); }
private:
  enum State { Top, Object, ObjectElement, Array, ArrayElement };
  std::expected<void, StarException> quoted(std::span<char32_t const> s) {
    auto r = write('"');
    if (!r) return std::unexpected(std::move(r.error()));
    for (std::size_t i = 0; i < s.size(); ++i) {
      auto r = escaped(s[i]);
      if (!r) return std::unexpected(std::move(r.error()));
    }
    return write('"');
  }
  std::expected<void, StarException> escaped(char32_t c) {
    std::u32string_view escape;
    switch (c) {
      case '"': escape = U"\\\""; break;
      case '\\': escape = U"\\\\"; break;
      case '\b': escape = U"\\b"; break;
      case '\f': escape = U"\\f"; break;
      case '\n': escape = U"\\n"; break;
      case '\r': escape = U"\\r"; break;
      case '\t': escape = U"\\t"; break;
      default: break;
    }
    if (!escape.empty())
      return text(escape);
    if (c < 0x20) {
      constexpr char32_t digits[] = U"0123456789abcdef";
      char32_t encoded[] = {U'\\', U'u', U'0', U'0', digits[c >> 4], digits[c & 0xf]};
      return text(encoded);
    }
    if (c > 0x10ffff) {
      auto hex = hex_string_from_utf32(c);
      if (!hex) return std::unexpected(std::move(hex.error()));
      if (hex->size() != 4 && hex->size() != 8)
        return std::unexpected(UnicodeException("Internal Error: Received invalid unicode hex from hexStringFromUtf32."));
      for (std::size_t j = 0; j < hex->size(); j += 4) {
        auto r = text(std::span<char32_t const>(U"\\u", 2));
        if (!r) return std::unexpected(std::move(r.error()));
        for (std::size_t k = j; k < j + 4; ++k) {
          r = write(static_cast<unsigned char>((*hex)[k]));
          if (!r) return std::unexpected(std::move(r.error()));
        }
      }
      return {};
    }
    return write(c);
  }
  std::expected<void, StarException> text(std::span<char32_t const> s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
      auto r = write(s[i]);
      if (!r) return std::unexpected(std::move(r.error()));
    }
    return {};
  }
  std::expected<void, StarException> asciiText(std::string_view s) {
    if constexpr (Utf8)
      return utf8Text(s);
    for (unsigned char c : s) {
      auto r = write(c);
      if (!r)
        return std::unexpected(std::move(r.error()));
    }
    return {};
  }
  // bytes have already passed complete-string UTF-8 validation.
  std::expected<void, StarException> utf8Text(std::string_view bytes) {
    if constexpr (Utf8) {
      if constexpr (requires { m_out.append(bytes); }) {
        return m_out.append(bytes);
      } else {
        for (char c : bytes) {
          auto r = writeByte(c);
          if (!r)
            return std::unexpected(std::move(r.error()));
        }
      }
    } else {
      while (!bytes.empty()) {
        char32_t point;
        auto width = utf8_decode_char(bytes.data(), &point, bytes.size());
        if (!width) return std::unexpected(std::move(width.error()));
        auto r = write(point);
        if (!r) return std::unexpected(std::move(r.error()));
        bytes.remove_prefix(*width);
      }
    }
    return {};
  }
  std::expected<void, StarException> startValue() {
    if (state() == ArrayElement && m_pretty) return write(' ');
    if (state() == Array) m_state.push_back(ArrayElement);
    return {};
  }
  std::expected<void, StarException> indent() {
    if (m_pretty) {
      auto r = write('\n');
      if (!r) return std::unexpected(std::move(r.error()));
    }
    for (unsigned i = 0; i < m_state.size() / 2; ++i)
      for (unsigned j = 0; j < m_pretty; ++j) {
        auto r = write(' ');
        if (!r) return std::unexpected(std::move(r.error()));
      }
    return {};
  }
  void pop(State target) {
    while (!m_state.empty()) {
      auto last = m_state.back();
      m_state.pop_back();
      if (last == target) return;
    }
  }
  State state() const { return m_state.empty() ? Top : m_state.back(); }
  std::expected<void, StarException> write(char32_t c) {
    if constexpr (Utf8) {
      if (c < 0x80) return writeByte(static_cast<char>(c));
      char bytes[4];
      auto width = utf8_encode_char(bytes, c, sizeof(bytes));
      if (!width) return std::unexpected(std::move(width.error()));
      return utf8Text(std::string_view(bytes, *width));
    } else {
      if constexpr (requires { m_out.push(c); }) {
        return m_out.push(c);
      } else {
        *m_out = c;
        ++m_out;
        return {};
      }
    }
  }
  std::expected<void, StarException> writeByte(char c) {
    if constexpr (requires { m_out.push(c); }) {
      return m_out.push(c);
    } else {
      *m_out = c;
      ++m_out;
      return {};
    }
  }
  OutputIterator m_out;
  unsigned m_pretty;
  std::vector<State> m_state;
};

}

export module star.json_parser;

export namespace Star {
  using ::Star::JsonStream;
  using ::Star::JsonParserExceptionTag;
  using ::Star::JsonParserException;
  using ::Star::JsonParseType;
  using ::Star::JsonParser;
  using ::Star::JsonWriter;
}
