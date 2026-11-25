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
  JsonBuilderStream() = default;
  JsonBuilderStream(JsonBuilderStream const&) = default;
  JsonBuilderStream& operator=(JsonBuilderStream const&) = default;
  JsonBuilderStream(JsonBuilderStream&& other) noexcept;
  JsonBuilderStream& operator=(JsonBuilderStream&& other) noexcept;
  std::expected<void, StarException> begin_object() override;
  std::expected<void, StarException> object_key(std::span<char32_t const> s) override;
  std::expected<void, StarException> object_key_utf8(std::string_view s) override;
  std::expected<void, StarException> end_object() override;
  std::expected<void, StarException> begin_array() override;
  std::expected<void, StarException> end_array() override;
  std::expected<void, StarException> put_string(std::span<char32_t const> s) override;
  std::expected<void, StarException> put_string_utf8(std::string_view s) override;
  std::expected<void, StarException> put_double(std::string_view s) override;
  std::expected<void, StarException> put_integer(std::string_view s) override;
  std::expected<void, StarException> put_boolean(bool b) override;
  std::expected<void, StarException> put_null() override;
  std::expected<void, StarException> put_whitespace(std::span<char32_t const> s) override;
  std::expected<void, StarException> put_colon() override;
  std::expected<void, StarException> put_comma() override;
  std::size_t stack_size();
  std::expected<Json, StarException> take_top();

protected:
  // Custom numeric parsers can insert an already converted value.
  void push_double(double value);
  void push_integer(std::int64_t value);

private:
  void push(Json v);
  std::expected<Json, StarException> pop();
  std::expected<void, StarException> set(Json v);
  void push_sentry();
  bool is_sentry();
  static constexpr std::size_t m_no_sentry = std::numeric_limits<std::size_t>::max();
  // Sentries link to their enclosing active index without a separate frame stack.
  List<std::variant<Json, std::size_t>> m_stack;
  std::size_t m_active_sentry = m_no_sentry;
};

template <typename JsonLike> class JsonStreamer {
public:
  static std::expected<void, StarException> to_json_stream(JsonLike const&, JsonStream&, bool) = delete;
};

template <>
class JsonStreamer<Json> {
public:
  static std::expected<void, StarException> to_json_stream(Json const& val, JsonStream& stream, bool sort);
  static std::expected<void, StarException> stream_object(JsonObject const& object, JsonStream& stream, bool sort);
};

template <typename Stream, typename JsonLike>
concept json_builder_stream =
  std::default_initializable<Stream> && std::convertible_to<Stream&, JsonStream&> && requires(Stream& stream) {
    { stream.take_top() } -> std::same_as<std::expected<JsonLike, StarException>>;
  };

template <typename JsonLike>
concept json_streamable = requires(JsonLike const& value, JsonStream& stream) {
  { JsonStreamer<JsonLike>::to_json_stream(value, stream, false) } -> std::same_as<std::expected<void, StarException>>;
};

// Unlike back_insert_iterator<String>, this sink propagates Unicode errors.
class JsonStringOutputIterator {
public:
  explicit JsonStringOutputIterator(String& string) : m_output(string) {}
  std::expected<void, StarException> push(char32_t c) { return m_output.push_back(c); }

private:
  String& m_output;
};

// UTF-8 output can append validated runs without an iterator assignment per byte.
class JsonUtf8StringOutputIterator {
public:
  explicit JsonUtf8StringOutputIterator(std::string& string) : m_output(string) {}
  std::expected<void, StarException> push(char c) {
    m_output.push_back(c);
    return {};
  }
  std::expected<void, StarException> append(std::string_view bytes) {
    m_output.append(bytes);
    return {};
  }

private:
  std::string& m_output;
};

template <json_input_iterator InputIterator, typename Stream = JsonBuilderStream, typename JsonLike = Json>
  requires json_builder_stream<Stream, JsonLike>
std::expected<JsonLike, StarException> input_utf32_json(InputIterator begin, InputIterator end,
                                                        JsonParseType parse_type) {
  Stream stream;
  // Only the ordinary DOM discards trivia. Custom streams keep the parser's
  // historical whitespace callbacks, including subclasses of the DOM builder.
  JsonParser<InputIterator, !std::is_same_v<Stream, JsonBuilderStream>> parser(stream);
  auto parsed = parser.parse(begin, end, parse_type);
  if (!parsed) {
    if (parsed.error().template is<JsonParserException>())
      return std::unexpected(JsonParsingException("Error parsing json", ExceptionDiagnostic(parsed.error())));
    return std::unexpected(std::move(parsed.error()));
  }
  if (*parsed != end)
    return std::unexpected(JsonParsingException(Star::format("Error extra data at end of input at {}:{}", parser.line(), parser.column())));
  return stream.take_top();
}

// Decode only the current codepoint. Unlike a validated Unicode range, this
// adapter must not inspect an unvisited suffix before an earlier parser or
// stream callback has had a chance to report its error.
template <std::forward_iterator InputIterator>
  requires std::convertible_to<std::iter_reference_t<InputIterator>, char>
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
      auto decoded = utf8_decode_char(
        std::string_view(m_current, m_end), m_point);
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
    auto decoded = utf8_decode_char(std::string_view(bytes, available), m_point);
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

  std::string_view utf8_remaining() const
    requires(std::contiguous_iterator<InputIterator> && std::same_as<std::iter_value_t<InputIterator>, char>)
  {
    if (m_current == m_end)
      return {};
    return std::string_view(m_current, m_end);
  }

  bool operator==(JsonUtf8InputIterator const& other) const { return m_current == other.m_current; }

private:
  InputIterator m_current{};
  InputIterator m_end{};
  mutable char32_t m_point = 0;
  mutable std::size_t m_width = 0;
};

template <std::forward_iterator InputIterator, typename Stream = JsonBuilderStream, typename JsonLike = Json>
  requires std::convertible_to<std::iter_reference_t<InputIterator>, char> && json_builder_stream<Stream, JsonLike>
std::expected<JsonLike, StarException> input_utf8_json(InputIterator begin, InputIterator end,
                                                       JsonParseType parse_type) {
  using Utf32Input = JsonUtf8InputIterator<InputIterator>;
  return input_utf32_json<Utf32Input, Stream, JsonLike>(Utf32Input(begin, end), Utf32Input(end, end), parse_type);
}

template <typename OutputIterator>
  requires json_output_iterator<OutputIterator, char>
std::expected<void, StarException> output_utf8_json(Json const& val, OutputIterator out, int pretty, bool sort) {
  JsonWriter<OutputIterator, true> writer(out, pretty);
  return JsonStreamer<Json>::to_json_stream(val, writer, sort);
}

template <typename OutputIterator, json_streamable JsonLike = Json>
  requires json_output_iterator<OutputIterator, char32_t>
std::expected<void, StarException> output_utf32_json(JsonLike const& val, OutputIterator out, int pretty, bool sort) {
  JsonWriter<OutputIterator> writer(out, pretty);
  return JsonStreamer<JsonLike>::to_json_stream(val, writer, sort);
}
}
export module star.json_builder;
export namespace Star {
using ::Star::input_utf32_json;
using ::Star::input_utf8_json;
using ::Star::json_builder_stream;
using ::Star::json_streamable;
using ::Star::JsonBuilderStream;
using ::Star::JsonStreamer;
using ::Star::JsonStringOutputIterator;
using ::Star::JsonUtf8StringOutputIterator;
using ::Star::output_utf32_json;
using ::Star::output_utf8_json;
}
