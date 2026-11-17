module;
import star.format;
import std;

import star.byte_array;
import star.unicode;

namespace Star {

struct JsonStream {
  // Each text range is borrowed only through its synchronous callback.
  virtual ~JsonStream() = default;
  virtual std::expected<void, StarException> begin_object() = 0;
  virtual std::expected<void, StarException> object_key(std::span<char32_t const>) = 0;
  virtual std::expected<void, StarException> end_object() = 0;
  virtual std::expected<void, StarException> begin_array() = 0;
  virtual std::expected<void, StarException> end_array() = 0;
  virtual std::expected<void, StarException> put_string(std::span<char32_t const>) = 0;
  // UTF-8 producers retain their borrowed storage through the synchronous call.
  // UTF-32 consumers receive a complete, validated string before their callback.
  virtual std::expected<void, StarException> object_key_utf8(std::string_view bytes) {
    auto text = decode_utf8(bytes);
    if (!text) return std::unexpected(std::move(text.error()));
    return object_key(*text);
  }
  virtual std::expected<void, StarException> put_string_utf8(std::string_view bytes) {
    auto text = decode_utf8(bytes);
    if (!text) return std::unexpected(std::move(text.error()));
    return put_string(*text);
  }
  // Numeric tokens are ASCII, borrowed through the synchronous callback.
  virtual std::expected<void, StarException> put_double(std::string_view) = 0;
  virtual std::expected<void, StarException> put_integer(std::string_view) = 0;
  virtual std::expected<void, StarException> put_boolean(bool) = 0;
  virtual std::expected<void, StarException> put_null() = 0;
  virtual std::expected<void, StarException> put_whitespace(std::span<char32_t const>) = 0;
  virtual std::expected<void, StarException> put_whitespace_utf8(std::string_view bytes) {
    auto text = decode_utf8(bytes);
    if (!text)
      return std::unexpected(std::move(text.error()));
    return put_whitespace(*text);
  }
  virtual std::expected<void, StarException> put_colon() = 0;
  virtual std::expected<void, StarException> put_comma() = 0;

private:
  static std::expected<std::u32string, StarException> decode_utf8(std::string_view bytes) {
    std::u32string text;
    auto decoded = for_each_codepoint(bytes, [&](char32_t point, std::size_t, std::size_t) {
      text.push_back(point);
    });
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
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

// Parsing needs copyable positions for sequence recovery, but UTF-8 adapters
// intentionally expose fallible read/advance rather than iterator dereference.
template <typename Input>
concept json_input_adapter = std::copyable<Input> && std::default_initializable<Input>
  && std::equality_comparable<Input> && requires(Input& input) {
       { input.read() } -> std::same_as<std::expected<char32_t, StarException>>;
       { input.advance() } -> std::same_as<std::expected<void, StarException>>;
     };

template <typename Input>
concept json_input_iterator = json_input_adapter<Input>
  || (std::input_iterator<Input> && std::copyable<Input> && std::default_initializable<Input>
      && std::sentinel_for<Input, Input> && std::convertible_to<std::iter_reference_t<Input>, char32_t>);

template <typename Input>
concept json_utf8_view_input = json_input_iterator<Input> && requires(Input const& input) {
  { input.utf8_remaining() } -> std::same_as<std::string_view>;
};

template <typename Output, typename Character>
concept json_push_output = requires(Output& output, Character character) {
  { output.push(character) } -> std::same_as<std::expected<void, StarException>>;
};

template <typename Output>
concept json_append_output = requires(Output& output, std::string_view bytes) {
  { output.append(bytes) } -> std::same_as<std::expected<void, StarException>>;
};

template <typename Output, typename Character>
concept json_write_output = requires(Output& output, Character character) {
  *output = character;
  ++output;
};

template <typename Output, typename Character>
concept json_output_iterator =
  std::move_constructible<Output> && (json_push_output<Output, Character> || json_write_output<Output, Character>);

// Will parse JSON and output to a given JsonStream.  Parses an *extension* to
// the JSON format that includes comments.
template <json_input_iterator InputIterator, bool retain_whitespace = true> class JsonParser {
public:
  explicit JsonParser(JsonStream& stream) : m_stream(stream) {}

  std::expected<InputIterator, StarException> parse(InputIterator begin, InputIterator end,
                                                    JsonParseType parse_type = JsonParseType::Top) {
    m_current = begin;
    m_end = end;
    m_line = m_column = 0;
    auto initialized = read();
    if (!initialized)
      return diagnostic(std::move(initialized.error()));
    auto parsed = parse_document(parse_type);
    if (!parsed)
      return diagnostic(std::move(parsed.error()));
    return m_current;
  }

  std::size_t line() const { return m_line + 1; }
  std::size_t column() const { return m_column + 1; }

private:
  using CharArray = std::basic_string<char32_t>;

  std::unexpected<StarException> diagnostic(StarException error) const {
    return std::unexpected(error.with_context(Star::format("Error parsing json at {}:{}", line(), column())));
  }

  std::expected<void, StarException> error(std::string_view message) const {
    return std::unexpected(JsonParserException(std::string(message)));
  }

  std::expected<void, StarException> read() {
    if (m_current == m_end) {
      m_character = 0;
      return {};
    }
    if constexpr (json_input_adapter<InputIterator>) {
      auto c = m_current.read();
      if (!c)
        return std::unexpected(std::move(c.error()));
      m_character = *c;
    } else {
      m_character = *m_current;
    }
    return {};
  }

  std::expected<void, StarException> next() {
    if (m_current == m_end)
      return {};
    if (m_character == '\n') {
      ++m_line;
      m_column = 0;
    } else {
      ++m_column;
    }
    if constexpr (json_input_adapter<InputIterator>) {
      auto advanced = m_current.advance();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    } else {
      ++m_current;
    }
    return read();
  }

  std::expected<void, StarException> parse_document(JsonParseType parse_type) {
    auto whitespace = white();
    if (!whitespace)
      return std::unexpected(std::move(whitespace.error()));
    std::expected<void, StarException> result;
    if (parse_type == JsonParseType::Sequence)
      result = sequence();
    else if (parse_type == JsonParseType::Top && m_character != '{' && m_character != '[')
      return error("expected JSON object or array at top level");
    else
      result = value();
    if (!result)
      return std::unexpected(std::move(result.error()));
    return white();
  }

  std::expected<void, StarException> value() {
    switch (m_character) {
    case '{': return object();
    case '[': return array();
    case '"': return parse_string();
    case '-': return number();
    case 0: return error("unexpected end of stream parsing value");
    default: return m_character >= '0' && m_character <= '9' ? number() : word();
    }
  }

  std::expected<void, StarException> object() {
    auto advance = next();
    if (!advance)
      return std::unexpected(std::move(advance.error()));
    auto begun = m_stream.begin_object();
    if (!begun)
      return std::unexpected(std::move(begun.error()));
    auto whitespace = white();
    if (!whitespace)
      return std::unexpected(std::move(whitespace.error()));
    if (m_character == '}') {
      auto advanced = next();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
      return m_stream.end_object();
    }
    while (true) {
      auto written = parse_string(true);
      if (!written)
        return std::unexpected(std::move(written.error()));
      whitespace = white();
      if (!whitespace)
        return std::unexpected(std::move(whitespace.error()));
      if (m_character != ':')
        return error("bad object, should be ':'");
      advance = next();
      if (!advance)
        return std::unexpected(std::move(advance.error()));
      written = m_stream.put_colon();
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
      if (m_character == '}') {
        advance = next();
        if (!advance)
          return std::unexpected(std::move(advance.error()));
        return m_stream.end_object();
      }
      if (m_character != ',')
        return error(m_character == 0 ? "unexpected end of stream parsing object."
                                      : "bad object, should be '}' or ','");
      advance = next();
      if (!advance)
        return std::unexpected(std::move(advance.error()));
      written = m_stream.put_comma();
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
    auto begun = m_stream.begin_array();
    if (!begun)
      return std::unexpected(std::move(begun.error()));
    auto whitespace = white();
    if (!whitespace)
      return std::unexpected(std::move(whitespace.error()));
    if (m_character == ']') {
      advanced = next();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
      return m_stream.end_array();
    }
    while (true) {
      auto parsed = value();
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      whitespace = white();
      if (!whitespace)
        return std::unexpected(std::move(whitespace.error()));
      if (m_character == ']') {
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
        return m_stream.end_array();
      }
      if (m_character != ',')
        return error(m_character == 0 ? "unexpected end of stream parsing array." : "bad array, should be ',' or ']'");
      advanced = next();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
      auto comma = m_stream.put_comma();
      if (!comma)
        return std::unexpected(std::move(comma.error()));
      whitespace = white();
      if (!whitespace)
        return std::unexpected(std::move(whitespace.error()));
    }
  }

  std::expected<void, StarException> sequence() {
    auto begun = m_stream.begin_array();
    if (!begun)
      return std::unexpected(std::move(begun.error()));
    while (m_character != 0) {
      if (is_space(m_character)) {
        auto advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
        continue;
      }
      if (m_character == '{' || m_character == '[' || m_character == '"' || m_character == '-') {
        auto parsed = value();
        if (!parsed)
          return std::unexpected(std::move(parsed.error()));
      } else {
        auto begin = m_current;
        auto character = m_character;
        auto saved_line = m_line;
        auto saved_column = m_column;
        bool recognized = false;
        if ((m_character >= '0' && m_character <= '9') || m_character == 't' || m_character == 'f'
            || m_character == 'n') {
          // Only lexical failures permit the sequence's existing bare-string
          // recovery. Stream/callback failures must never be swallowed.
          auto token = m_character >= '0' && m_character <= '9' ? number(true) : word(true);
          if (token) {
            recognized = true;
          } else if (!token.error().template is<JsonParserException>()) {
            return std::unexpected(std::move(token.error()));
          } else {
            m_current = begin;
            m_character = character;
            m_line = saved_line;
            m_column = saved_column;
          }
        }
        if (!recognized) {
          CharArray text;
          do {
            text += m_character;
            auto advanced = next();
            if (!advanced)
              return std::unexpected(std::move(advanced.error()));
          } while (m_character != 0 && !is_space(m_character));
          auto written = m_stream.put_string(text);
          if (!written)
            return std::unexpected(std::move(written.error()));
        }
      }
      auto advanced = next();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    }
    return m_stream.end_array();
  }


  std::expected<void, StarException> number(bool seq = false) {
    std::string buffer;
    std::string_view begin;
    if constexpr (json_utf8_view_input<InputIterator>)
      begin = m_current.utf8_remaining();
    bool is_double = false;
    auto consume = [&]() -> std::expected<void, StarException> {
      if constexpr (!json_utf8_view_input<InputIterator>)
        buffer += static_cast<char>(m_character);
      return next();
    };
    if (m_character == '-') {
      auto result = consume();
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
    if (m_character == '0') {
      auto result = consume();
      if (!result)
        return std::unexpected(std::move(result.error()));
    } else if (m_character > '0' && m_character <= '9') {
      do {
        auto result = consume();
        if (!result)
          return std::unexpected(std::move(result.error()));
      } while (m_character >= '0' && m_character <= '9');
    } else {
      return error("bad number, must start with digit");
    }
    if (m_character == '.' || m_character == 'e' || m_character == 'E') {
      if (m_character == '.') {
        is_double = true;
        auto result = consume();
        if (!result)
          return std::unexpected(std::move(result.error()));
        while (m_character >= '0' && m_character <= '9') {
          result = consume();
          if (!result)
            return std::unexpected(std::move(result.error()));
        }
      }
      if (m_character == 'e' || m_character == 'E') {
        is_double = true;
        auto result = consume();
        if (!result)
          return std::unexpected(std::move(result.error()));
        if (m_character == '-' || m_character == '+') {
          result = consume();
          if (!result)
            return std::unexpected(std::move(result.error()));
        }
        while (m_character >= '0' && m_character <= '9') {
          result = consume();
          if (!result)
            return std::unexpected(std::move(result.error()));
        }
      }
    }
    if (seq && m_character != 0 && !is_space(m_character))
      return error("unexpected character after number");
    std::string_view token = buffer;
    if constexpr (json_utf8_view_input<InputIterator>)
      token = begin.substr(0, begin.size() - m_current.utf8_remaining().size());
    return is_double ? m_stream.put_double(token) : m_stream.put_integer(token);
  }

  std::expected<void, StarException> word(bool seq = false) {
    char32_t first = m_character;
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
    if (seq && m_character != 0 && !is_space(m_character))
      return error("unexpected character after word");
    return first == 'n' ? m_stream.put_null() : m_stream.put_boolean(first == 't');
  }

  std::expected<void, StarException> parse_string(bool key = false) {
    if (m_character != '"')
      return error("bad string, should be '\"'");
    auto advanced = next();
    if (!advanced)
      return std::unexpected(std::move(advanced.error()));
    CharArray text;
    if constexpr (json_utf8_view_input<InputIterator>) {
      // Decode only visited codepoints, retaining a borrowed byte token until
      // an escape actually requires the existing UTF-32 assembly path.
      auto begin = m_current.utf8_remaining();
      while (m_character != '\\' && m_character != '"' && m_character != 0) {
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
      }
      auto bytes = begin.substr(0, begin.size() - m_current.utf8_remaining().size());
      if (m_character == '"') {
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
        return key ? m_stream.object_key_utf8(bytes) : m_stream.put_string_utf8(bytes);
      }
      auto decoded = for_each_codepoint(bytes, [&](char32_t point, std::size_t, std::size_t) { text += point; });
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
    }
    while (true) {
      if (m_character == '\\') {
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
        if (m_character == 'u') {
          auto codepoint = read_hex_escape();
          if (!codepoint)
            return std::unexpected(std::move(codepoint.error()));
          if (is_utf16_lead_surrogate(*codepoint)) {
            auto checked = check('\\');
            if (!checked)
              return std::unexpected(std::move(checked.error()));
            checked = check('u');
            if (!checked)
              return std::unexpected(std::move(checked.error()));
            auto trail = read_hex_digits();
            if (!trail)
              return std::unexpected(std::move(trail.error()));
            codepoint = utf32_from_utf16_surrogate_pair(*codepoint, *trail);
            if (!codepoint)
              return std::unexpected(std::move(codepoint.error()));
          }
          text += *codepoint;
        } else {
          switch (m_character) {
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
      } else if (m_character == '"') {
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
        return key ? m_stream.object_key(text) : m_stream.put_string(text);
      } else if (m_character == 0) {
        return std::unexpected(JsonParserException("unexpected end of stream reading string!"));
      } else {
        text += m_character;
        advanced = next();
        if (!advanced)
          return std::unexpected(std::move(advanced.error()));
      }
    }
  }

  std::expected<char32_t, StarException> read_hex_escape() {
    auto advanced = next();
    if (!advanced)
      return std::unexpected(std::move(advanced.error()));
    return read_hex_digits();
  }

  std::expected<char32_t, StarException> read_hex_digits() {
    char32_t codeunit = 0;
    for (unsigned i = 0; i < 4; ++i) {
      unsigned digit;
      if (m_character >= U'0' && m_character <= U'9')
        digit = m_character - U'0';
      else if (m_character >= U'a' && m_character <= U'f')
        digit = m_character - U'a' + 10;
      else if (m_character >= U'A' && m_character <= U'F')
        digit = m_character - U'A' + 10;
      else
        return std::unexpected(JsonParserException("expected hexadecimal digit in unicode escape"));
      codeunit = (codeunit << 4) | digit;
      auto advanced = next();
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
    }
    return codeunit;
  }

  std::expected<void, StarException> check(char32_t c) {
    if (m_character == 0)
      return error("unexpected end of stream parsing word");
    if (m_character != c)
      return error("unexpected character in word");
    return next();
  }

  std::expected<void, StarException> white() {
    CharArray buffer;
    std::string_view begin;
    if constexpr (retain_whitespace && json_utf8_view_input<InputIterator>)
      begin = m_current.utf8_remaining();
    auto consume = [&]() -> std::expected<void, StarException> {
      if constexpr (retain_whitespace && !json_utf8_view_input<InputIterator>)
        buffer += m_character;
      return next();
    };
    while (m_current != m_end) {
      if (m_character == '/') {
        auto consumed = consume();
        if (!consumed)
          return std::unexpected(std::move(consumed.error()));
        if (m_character == '/') {
          do {
            consumed = consume();
            if (!consumed)
              return std::unexpected(std::move(consumed.error()));
          } while (m_current != m_end && m_character != '\n');
        } else if (m_character == '*') {
          consumed = consume();
          if (!consumed)
            return std::unexpected(std::move(consumed.error()));
          while (m_current != m_end) {
            auto c = m_character;
            consumed = consume();
            if (!consumed)
              return std::unexpected(std::move(consumed.error()));
            if (c == '*' && m_character == '/') {
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
      } else if (is_space(m_character)) {
        auto consumed = consume();
        if (!consumed)
          return std::unexpected(std::move(consumed.error()));
      } else {
        break;
      }
    }
    if constexpr (retain_whitespace) {
      if constexpr (json_utf8_view_input<InputIterator>) {
        auto count = begin.size() - m_current.utf8_remaining().size();
        if (count != 0)
          return m_stream.put_whitespace_utf8(begin.substr(0, count));
      } else if (!buffer.empty()) {
        return m_stream.put_whitespace(buffer);
      }
    }
    return {};
  }

  static bool is_space(char32_t c) {
    return c == 0x20 || c == 0x09 || c == 0x0a || c == 0x0d || c == 0xfeff;
  }

  char32_t m_character = 0;
  InputIterator m_current;
  InputIterator m_end;
  std::size_t m_line = 0;
  std::size_t m_column = 0;
  JsonStream& m_stream;
};

// Write JSON through JsonStream interface.
// Escaped object keys are an explicit opt-in; ordinary JSON output retains its
// historical raw-key spelling and first-NUL truncation.
template <typename OutputIterator, bool utf8 = false, bool escape_object_keys = false>
  requires json_output_iterator<OutputIterator, std::conditional_t<utf8, char, char32_t>>
class JsonWriter : public JsonStream {
public:
  JsonWriter(OutputIterator out, unsigned pretty = 0) : m_output(out), m_pretty(pretty) {}
  std::expected<void, StarException> begin_object() override {
    auto r = start_value();
    if (!r) return std::unexpected(std::move(r.error()));
    m_state.push_back(Object);
    return write('{');
  }
  std::expected<void, StarException> object_key(std::span<char32_t const> s) override {
    if (state() != ObjectElement)
      m_state.push_back(ObjectElement);
    auto r = indent();
    if (!r) return std::unexpected(std::move(r.error()));
    if constexpr (escape_object_keys) {
      r = quoted(s);
      if (!r) return std::unexpected(std::move(r.error()));
      return m_pretty ? write(' ') : std::expected<void, StarException>{};
    }
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
  std::expected<void, StarException> object_key_utf8(std::string_view bytes) override {
    auto valid = utf8_length(bytes);
    if (!valid) return std::unexpected(std::move(valid.error()));
    if (state() != ObjectElement)
      m_state.push_back(ObjectElement);
    auto r = indent();
    if (!r) return std::unexpected(std::move(r.error()));
    if constexpr (escape_object_keys) {
      r = put_string_utf8(bytes);
      if (!r) return std::unexpected(std::move(r.error()));
      return m_pretty ? write(' ') : std::expected<void, StarException>{};
    }
    r = write('"');
    if (!r) return std::unexpected(std::move(r.error()));
    // Object keys historically bypass value escaping and stop at the first
    // NUL. Validate the entire key above, including that unprinted suffix.
    auto end = bytes.find('\0');
    r = utf8_text(bytes.substr(0, end));
    if (!r) return std::unexpected(std::move(r.error()));
    r = write('"');
    if (!r) return std::unexpected(std::move(r.error()));
    return m_pretty ? write(' ') : std::expected<void, StarException>{};
  }
  std::expected<void, StarException> end_object() override {
    bool nonempty = state() == ObjectElement;
    pop(Object);
    if (nonempty) {
      auto r = indent();
      if (!r) return std::unexpected(std::move(r.error()));
    }
    return write('}');
  }
  std::expected<void, StarException> begin_array() override {
    auto r = start_value();
    if (!r) return std::unexpected(std::move(r.error()));
    m_state.push_back(Array);
    return write('[');
  }
  std::expected<void, StarException> end_array() override {
    pop(Array);
    return write(']');
  }
  std::expected<void, StarException> put_string(std::span<char32_t const> s) override {
    auto r = start_value();
    if (!r) return std::unexpected(std::move(r.error()));
    return quoted(s);
  }
  std::expected<void, StarException> put_string_utf8(std::string_view bytes) override {
    // Preserve wide_string's all-or-nothing validation before start_value or any
    // output, but do not inspect later JSON values before earlier output.
    auto valid = utf8_length(bytes);
    if (!valid) return std::unexpected(std::move(valid.error()));
    auto r = start_value();
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
        r = utf8_text(bytes.substr(0, count));
        bytes.remove_prefix(count);
      } else {
        r = escaped(static_cast<unsigned char>(bytes.front()));
        bytes.remove_prefix(1);
      }
      if (!r) return std::unexpected(std::move(r.error()));
    }
    return write('"');
  }
  std::expected<void, StarException> put_double(std::string_view s) override {
    auto r = start_value();
    if (!r) return std::unexpected(std::move(r.error()));
    return ascii_text(s);
  }
  std::expected<void, StarException> put_integer(std::string_view s) override { return put_double(s); }
  std::expected<void, StarException> put_boolean(bool b) override { return put_double(b ? "true" : "false"); }
  std::expected<void, StarException> put_null() override { return put_double("null"); }
  std::expected<void, StarException> put_whitespace(std::span<char32_t const> s) override { return text(s); }
  std::expected<void, StarException> put_whitespace_utf8(std::string_view bytes) override {
    auto valid = utf8_length(bytes);
    if (!valid)
      return std::unexpected(std::move(valid.error()));
    return utf8_text(bytes);
  }
  std::expected<void, StarException> put_colon() override {
    auto r = write(':');
    if (!r) return std::unexpected(std::move(r.error()));
    return m_pretty ? write(' ') : std::expected<void, StarException>{};
  }
  std::expected<void, StarException> put_comma() override { return write(','); }

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
    if (c > 0x10ffff)
      return std::unexpected(UnicodeException("Invalid codepoint passed to utf32ToUtf16SurrogatePair"));
    return write(c);
  }
  std::expected<void, StarException> text(std::span<char32_t const> s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
      auto r = write(s[i]);
      if (!r) return std::unexpected(std::move(r.error()));
    }
    return {};
  }
  std::expected<void, StarException> ascii_text(std::string_view s) {
    if constexpr (utf8)
      return utf8_text(s);
    for (unsigned char c : s) {
      auto r = write(c);
      if (!r)
        return std::unexpected(std::move(r.error()));
    }
    return {};
  }
  // bytes have already passed complete-string UTF-8 validation.
  std::expected<void, StarException> utf8_text(std::string_view bytes) {
    if constexpr (utf8) {
      if constexpr (json_append_output<OutputIterator>) {
        return m_output.append(bytes);
      } else {
        for (char c : bytes) {
          auto r = write_byte(c);
          if (!r)
            return std::unexpected(std::move(r.error()));
        }
      }
    } else {
      return for_each_codepoint(bytes, [&](char32_t point, std::size_t, std::size_t) {
        return write(point);
      });
    }
    return {};
  }
  std::expected<void, StarException> start_value() {
    if (state() == ArrayElement && m_pretty)
      return write(' ');
    if (state() == Array)
      m_state.push_back(ArrayElement);
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
    if constexpr (utf8) {
      if (c < 0x80)
        return write_byte(static_cast<char>(c));
      char bytes[4];
      auto width = utf8_encode_char(std::span<char>(bytes), c);
      if (!width) return std::unexpected(std::move(width.error()));
      return utf8_text(std::string_view(bytes, *width));
    } else {
      if constexpr (json_push_output<OutputIterator, char32_t>) {
        return m_output.push(c);
      } else {
        *m_output = c;
        ++m_output;
        return {};
      }
    }
  }
  std::expected<void, StarException> write_byte(char c) {
    if constexpr (json_push_output<OutputIterator, char>) {
      return m_output.push(c);
    } else {
      *m_output = c;
      ++m_output;
      return {};
    }
  }
  OutputIterator m_output;
  unsigned m_pretty;
  std::vector<State> m_state;
};
}

export module star.json_parser;

export namespace Star {
using ::Star::json_append_output;
using ::Star::json_input_adapter;
using ::Star::json_input_iterator;
using ::Star::json_output_iterator;
using ::Star::json_push_output;
using ::Star::json_utf8_view_input;
using ::Star::json_write_output;
using ::Star::JsonParser;
using ::Star::JsonParserException;
using ::Star::JsonParserExceptionTag;
using ::Star::JsonParseType;
using ::Star::JsonStream;
using ::Star::JsonWriter;
}
