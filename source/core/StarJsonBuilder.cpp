import std;
import star.string;
import star.list;
import star.format;
import star.unicode;
import star.json;
import star.lexical_cast;
import star.json_parser;
import star.json_builder;

namespace Star {

JsonBuilderStream::JsonBuilderStream(JsonBuilderStream&& other) noexcept
    : m_stack(std::move(other.m_stack)), m_active_sentry(std::exchange(other.m_active_sentry, m_no_sentry)) {}

JsonBuilderStream& JsonBuilderStream::operator=(JsonBuilderStream&& other) noexcept {
  if (this != std::addressof(other)) {
    m_stack = std::move(other.m_stack);
    m_active_sentry = std::exchange(other.m_active_sentry, m_no_sentry);
  }
  return *this;
}

std::expected<void, StarException> JsonBuilderStream::begin_object() {
  push_sentry();
  return {};
}
std::expected<void, StarException> JsonBuilderStream::object_key(std::span<char32_t const> s) { return put_string(s); }
std::expected<void, StarException> JsonBuilderStream::object_key_utf8(std::string_view s) { return put_string_utf8(s); }
std::expected<void, StarException> JsonBuilderStream::end_object() {
  JsonObject object;
  while (!is_sentry()) {
    auto value = pop();
    if (!value) return std::unexpected(std::move(value.error()));
    auto key_json = pop();
    if (!key_json)
      return std::unexpected(std::move(key_json.error()));
    auto key = key_json->string_ptr();
    if (!key) return std::unexpected(std::move(key.error()));
    if (!object.insert(**key, std::move(*value)).second)
      return std::unexpected(JsonParsingException(Star::format("Json object contains a duplicate entry for key '{}'", **key)));
  }
  return set(Json(std::move(object)));
}
std::expected<void, StarException> JsonBuilderStream::begin_array() {
  push_sentry();
  return {};
}
std::expected<void, StarException> JsonBuilderStream::end_array() {
  JsonArray array;
  if (m_active_sentry == m_no_sentry) {
    // A missing container historically consumes every value before failing.
    while (true) {
      auto value = pop();
      if (!value)
        return std::unexpected(std::move(value.error()));
    }
  }
  array.reserve(m_stack.size() - m_active_sentry - 1);
  for (auto index = m_active_sentry + 1; index < m_stack.size(); ++index)
    array.push_back(std::move(std::get<Json>(m_stack[index])));
  m_stack.resize(m_active_sentry + 1);
  return set(Json(std::move(array)));
}
std::expected<void, StarException> JsonBuilderStream::put_string(std::span<char32_t const> s) {
  auto value = Json::create(s);
  if (!value) return std::unexpected(std::move(value.error()));
  push(std::move(*value));
  return {};
}
std::expected<void, StarException> JsonBuilderStream::put_string_utf8(std::string_view s) {
  auto valid = utf8_length(s);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  push(Json(String(s)));
  return {};
}
std::expected<void, StarException> JsonBuilderStream::put_double(std::string_view s) {
  auto value = lexical_cast<double>(s);
  if (!value) return std::unexpected(JsonParserException("bad double", ExceptionDiagnostic(value.error())));
  push(Json(*value));
  return {};
}
std::expected<void, StarException> JsonBuilderStream::put_integer(std::string_view s) {
  auto value = lexical_cast<long long>(s);
  if (!value) return std::unexpected(JsonParserException("bad integer", ExceptionDiagnostic(value.error())));
  push(Json(*value));
  return {};
}
std::expected<void, StarException> JsonBuilderStream::put_boolean(bool value) {
  push(Json(value));
  return {};
}
std::expected<void, StarException> JsonBuilderStream::put_null() {
  push(Json());
  return {};
}
std::expected<void, StarException> JsonBuilderStream::put_whitespace(std::span<char32_t const>) { return {}; }
std::expected<void, StarException> JsonBuilderStream::put_colon() { return {}; }
std::expected<void, StarException> JsonBuilderStream::put_comma() { return {}; }
std::size_t JsonBuilderStream::stack_size() { return m_stack.size(); }
std::expected<Json, StarException> JsonBuilderStream::take_top() {
  if (m_stack.size() != 1 || !std::holds_alternative<Json>(m_stack.back()))
    return std::unexpected(JsonParsingException("Incomplete JSON builder stream"));
  return pop();
}
void JsonBuilderStream::push(Json value) { m_stack.push_back(std::move(value)); }
std::expected<Json, StarException> JsonBuilderStream::pop() {
  if (m_stack.empty() || !std::holds_alternative<Json>(m_stack.back()))
    return std::unexpected(JsonParsingException("Incomplete JSON builder stream"));
  Json value = std::move(std::get<Json>(m_stack.back()));
  m_stack.pop_back();
  return value;
}
std::expected<void, StarException> JsonBuilderStream::set(Json value) {
  if (!is_sentry())
    return std::unexpected(JsonParsingException("Missing JSON builder container"));
  auto previous_sentry = std::get<std::size_t>(m_stack.back());
  m_stack.back() = std::move(value);
  m_active_sentry = previous_sentry;
  return {};
}
void JsonBuilderStream::push_sentry() {
  m_stack.push_back(m_active_sentry);
  m_active_sentry = m_stack.size() - 1;
}
bool JsonBuilderStream::is_sentry() { return !m_stack.empty() && std::holds_alternative<std::size_t>(m_stack.back()); }

std::expected<void, StarException> JsonStreamer<Json>::to_json_stream(Json const& val, JsonStream& stream, bool sort) {
  switch (val.type()) {
  case Json::Type::Null: return stream.put_null();
  case Json::Type::Bool: {
    auto value = val.toBool();
    if (!value)
      return std::unexpected(std::move(value.error()));
    return stream.put_boolean(*value);
  }
    case Json::Type::Float: {
      auto value = val.toDouble();
      if (!value) return std::unexpected(std::move(value.error()));
      // Binary64's shortest default form needs at most 17 significant digits,
      // a sign, decimal point and e+/-ddd (24 characters); fixed form is no longer.
      static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53
          && std::numeric_limits<double>::max_exponent == 1024);
      std::array<char, 32> text;
      auto formatted = std::format_to_n(text.data(), text.size(), "{}", *value);
      if (formatted.size > static_cast<decltype(formatted.size)>(text.size()))
        return std::unexpected(JsonException("JSON floating-point formatting buffer exhausted"));
      return stream.put_double(std::string_view(text.data(), static_cast<std::size_t>(formatted.size)));
    }
    case Json::Type::Int: {
      auto value = val.toInt();
      if (!value) return std::unexpected(std::move(value.error()));
      // digits10 excludes the final decimal digit; reserve that digit and a sign.
      std::array<char, std::numeric_limits<std::int64_t>::digits10 + 2> text;
      auto formatted = std::format_to_n(text.data(), text.size(), "{}", *value);
      if (formatted.size > static_cast<decltype(formatted.size)>(text.size()))
        return std::unexpected(JsonException("JSON integer formatting buffer exhausted"));
      return stream.put_integer(std::string_view(text.data(), static_cast<std::size_t>(formatted.size)));
    }
    case Json::Type::String: {
      auto value = val.string_ptr();
      if (!value) return std::unexpected(std::move(value.error()));
      return stream.put_string_utf8((*value)->utf8());
    }
    case Json::Type::Array: {
      auto elements = val.iterate_array();
      if (!elements) return std::unexpected(std::move(elements.error()));
      auto begun = stream.begin_array();
      if (!begun) return std::unexpected(std::move(begun.error()));
      bool first = true;
      for (auto const& element : *elements) {
        if (!first) {
          auto comma = stream.put_comma();
          if (!comma) return std::unexpected(std::move(comma.error()));
        }
        first = false;
        auto written = to_json_stream(element, stream, sort);
        if (!written) return std::unexpected(std::move(written.error()));
      }
      return stream.end_array();
    }
    case Json::Type::Object: {
      auto object = val.object_ptr();
      if (!object) return std::unexpected(std::move(object.error()));
      return stream_object(**object, stream, sort);
    }
  }
  return std::unexpected(JsonException("Invalid Json type"));
}

std::expected<void, StarException> JsonStreamer<Json>::stream_object(JsonObject const& object, JsonStream& stream,
                                                                     bool sort) {
  auto begun = stream.begin_object();
  if (!begun)
    return std::unexpected(std::move(begun.error()));
  auto entry = [&](JsonObject::value_type const& pair, bool first) -> std::expected<void, StarException> {
    if (!first) {
      auto comma = stream.put_comma();
      if (!comma)
        return std::unexpected(std::move(comma.error()));
    }
    auto written = stream.object_key_utf8(pair.first.utf8());
    if (!written)
      return std::unexpected(std::move(written.error()));
    written = stream.put_colon();
    if (!written)
      return std::unexpected(std::move(written.error()));
    return to_json_stream(pair.second, stream, sort);
  };
  bool first = true;
  if (sort) {
    List<JsonObject::const_iterator> iterators;
    iterators.reserve(object.size());
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator)
      iterators.push_back(iterator);
    iterators.sort(
      [](JsonObject::const_iterator left, JsonObject::const_iterator right) { return left->first < right->first; });
    for (auto iterator : iterators) {
      auto written = entry(*iterator, first);
      if (!written)
        return std::unexpected(std::move(written.error()));
      first = false;
    }
  } else {
    for (auto const& pair : object) {
      auto written = entry(pair, first);
      if (!written)
        return std::unexpected(std::move(written.error()));
      first = false;
    }
  }
  return stream.end_object();
}
}
