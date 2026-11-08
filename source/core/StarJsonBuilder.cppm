module;
import std;
import star.string;
import star.list;
import star.unicode;
import star.format;
import star.json;
import star.json_parser;

namespace Star {

class JsonBuilderStream : public JsonStream {
public:
  std::expected<void, StarException> beginObject() override;
  std::expected<void, StarException> objectKey(std::span<char32_t const> s) override;
  std::expected<void, StarException> objectKeyUtf8(std::string_view s) override;
  std::expected<void, StarException> endObject() override;
  std::expected<void, StarException> beginArray() override;
  std::expected<void, StarException> endArray() override;
  std::expected<void, StarException> putString(std::span<char32_t const> s) override;
  std::expected<void, StarException> putStringUtf8(std::string_view s) override;
  std::expected<void, StarException> putDouble(std::string_view s) override;
  std::expected<void, StarException> putInteger(std::string_view s) override;
  std::expected<void, StarException> putBoolean(bool b) override;
  std::expected<void, StarException> putNull() override;
  std::expected<void, StarException> putWhitespace(std::span<char32_t const> s) override;
  std::expected<void, StarException> putColon() override;
  std::expected<void, StarException> putComma() override;
  std::size_t stackSize();
  std::expected<Json, StarException> takeTop();
private:
  void push(Json v);
  std::expected<Json, StarException> pop();
  std::expected<void, StarException> set(Json v);
  void pushSentry();
  bool isSentry();
  List<std::optional<Json>> m_stack;
};

template <typename Jsonlike>
class JsonStreamer {
public:
  static std::expected<void, StarException> toJsonStream(Jsonlike const& val, JsonStream& stream, bool sort);
};

template <>
class JsonStreamer<Json> {
public:
  static std::expected<void, StarException> toJsonStream(Json const& val, JsonStream& stream, bool sort);
};

// Unlike back_insert_iterator<String>, this sink propagates Unicode errors.
class JsonStringOutputIterator {
public:
  explicit JsonStringOutputIterator(String& string) : m_string(string) {}
  std::expected<void, StarException> push(char32_t c) { return m_string.push_back(c); }
private:
  String& m_string;
};

// UTF-8 output can append validated runs without an iterator assignment per byte.
class JsonUtf8StringOutputIterator {
public:
  explicit JsonUtf8StringOutputIterator(std::string& string) : m_string(string) {}
  std::expected<void, StarException> push(char c) {
    m_string.push_back(c);
    return {};
  }
  std::expected<void, StarException> append(std::string_view bytes) {
    m_string.append(bytes);
    return {};
  }

private:
  std::string& m_string;
};

template <typename InputIterator, typename Stream = JsonBuilderStream, typename Jsonlike = Json>
std::expected<Jsonlike, StarException> inputUtf32Json(InputIterator begin, InputIterator end, JsonParseType parseType) {
  Stream stream;
  // Only the ordinary DOM discards trivia. Custom streams keep the parser's
  // historical whitespace callbacks, including subclasses of the DOM builder.
  JsonParser<InputIterator, !std::is_same_v<Stream, JsonBuilderStream>> parser(stream);
  auto parsed = parser.parse(begin, end, parseType);
  if (!parsed) {
    if (parsed.error().template is<JsonParserException>())
      return std::unexpected(JsonParsingException("Error parsing json", ExceptionDiagnostic(parsed.error())));
    return std::unexpected(std::move(parsed.error()));
  }
  if (*parsed != end)
    return std::unexpected(JsonParsingException(strf("Error extra data at end of input at {}:{}", parser.line(), parser.column())));
  return stream.takeTop();
}

// Decode only the current codepoint. Unlike a validated Unicode range, this
// adapter must not inspect an unvisited suffix before an earlier parser or
// stream callback has had a chance to report its error.
template <typename InputIterator>
class JsonUtf8InputIterator {
public:
  JsonUtf8InputIterator() = default;
  JsonUtf8InputIterator(InputIterator current, InputIterator end) : m_current(current), m_end(end) {}

  std::expected<char32_t, StarException> read() const {
    if (m_current == m_end)
      return std::unexpected(UnicodeException("Cannot read past end of UTF-8 range"));
    if (m_width != 0)
      return m_point;
    if constexpr (std::contiguous_iterator<InputIterator> && std::same_as<std::iter_value_t<InputIterator>, char>) {
      auto decoded = utf8_decode_char(std::to_address(m_current), &m_point, static_cast<std::size_t>(m_end - m_current));
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      m_width = *decoded;
      return m_point;
    }
    char bytes[4];
    auto lookahead = m_current;
    bytes[0] = *lookahead++;
    auto first = static_cast<unsigned char>(bytes[0]);
    unsigned wanted = 1;
    if ((first & 0xe0) == 0xc0)
      wanted = 2;
    else if ((first & 0xf0) == 0xe0)
      wanted = 3;
    else if ((first & 0xf8) == 0xf0)
      wanted = 4;
    std::size_t available = 1;
    while (available < wanted && lookahead != m_end)
      bytes[available++] = *lookahead++;
    auto decoded = utf8_decode_char(bytes, &m_point, available);
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    m_width = *decoded;
    return m_point;
  }

  std::expected<void, StarException> advance() {
    auto point = read();
    if (!point)
      return std::unexpected(std::move(point.error()));
    std::advance(m_current, m_width);
    m_width = 0;
    return {};
  }

  char const* utf8Position() const
    requires(std::contiguous_iterator<InputIterator> && std::same_as<std::iter_value_t<InputIterator>, char>)
  {
    return std::to_address(m_current);
  }

  bool operator==(JsonUtf8InputIterator const& other) const {
    return m_current == other.m_current;
  }

private:
  InputIterator m_current{};
  InputIterator m_end{};
  mutable char32_t m_point = 0;
  mutable std::size_t m_width = 0;
};

template <typename InputIterator, typename Stream = JsonBuilderStream, typename Jsonlike = Json>
std::expected<Jsonlike, StarException> inputUtf8Json(InputIterator begin, InputIterator end, JsonParseType parseType) {
  using Utf32Input = JsonUtf8InputIterator<InputIterator>;
  return inputUtf32Json<Utf32Input, Stream, Jsonlike>(Utf32Input(begin, end), Utf32Input(end, end), parseType);
}

template <typename OutputIterator>
std::expected<void, StarException> outputUtf8Json(Json const& val, OutputIterator out, int pretty, bool sort) {
  JsonWriter<OutputIterator, true> writer(out, pretty);
  return JsonStreamer<Json>::toJsonStream(val, writer, sort);
}

template <typename OutputIterator, typename Jsonlike = Json>
std::expected<void, StarException> outputUtf32Json(Jsonlike const& val, OutputIterator out, int pretty, bool sort) {
  JsonWriter<OutputIterator> writer(out, pretty);
  return JsonStreamer<Jsonlike>::toJsonStream(val, writer, sort);
}

}
export module star.json_builder;
export namespace Star {
using ::Star::inputUtf32Json;
using ::Star::inputUtf8Json;
using ::Star::JsonBuilderStream;
using ::Star::JsonStreamer;
using ::Star::JsonStringOutputIterator;
using ::Star::JsonUtf8StringOutputIterator;
using ::Star::outputUtf32Json;
using ::Star::outputUtf8Json;
}
