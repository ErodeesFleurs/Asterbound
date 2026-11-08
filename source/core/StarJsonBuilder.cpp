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

std::expected<void, StarException> JsonBuilderStream::beginObject() { pushSentry(); return {}; }
std::expected<void, StarException> JsonBuilderStream::objectKey(std::span<char32_t const> s) { return putString(s); }
std::expected<void, StarException> JsonBuilderStream::objectKeyUtf8(std::string_view s) { return putStringUtf8(s); }
std::expected<void, StarException> JsonBuilderStream::endObject() {
  JsonObject object;
  while (!isSentry()) {
    auto value = pop();
    if (!value) return std::unexpected(std::move(value.error()));
    auto keyJson = pop();
    if (!keyJson) return std::unexpected(std::move(keyJson.error()));
    auto key = keyJson->stringPtr();
    if (!key) return std::unexpected(std::move(key.error()));
    if (!object.insert(**key, std::move(*value)).second)
      return std::unexpected(JsonParsingException(strf("Json object contains a duplicate entry for key '{}'", **key)));
  }
  return set(Json(std::move(object)));
}
std::expected<void, StarException> JsonBuilderStream::beginArray() { pushSentry(); return {}; }
std::expected<void, StarException> JsonBuilderStream::endArray() {
  JsonArray array;
  while (!isSentry()) {
    auto value = pop();
    if (!value) return std::unexpected(std::move(value.error()));
    array.push_back(std::move(*value));
  }
  array.reverse();
  return set(Json(std::move(array)));
}
std::expected<void, StarException> JsonBuilderStream::putString(std::span<char32_t const> s) {
  auto value = Json::create(s);
  if (!value) return std::unexpected(std::move(value.error()));
  push(std::move(*value));
  return {};
}
std::expected<void, StarException> JsonBuilderStream::putStringUtf8(std::string_view s) {
  auto valid = utf8Length(s.data(), s.size());
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  push(Json(String(s)));
  return {};
}
std::expected<void, StarException> JsonBuilderStream::putDouble(std::string_view s) {
  auto value = lexicalCast<double>(s);
  if (!value) return std::unexpected(JsonParserException("bad double", ExceptionDiagnostic(value.error())));
  push(Json(*value));
  return {};
}
std::expected<void, StarException> JsonBuilderStream::putInteger(std::string_view s) {
  auto value = lexicalCast<long long>(s);
  if (!value) return std::unexpected(JsonParserException("bad integer", ExceptionDiagnostic(value.error())));
  push(Json(*value));
  return {};
}
std::expected<void, StarException> JsonBuilderStream::putBoolean(bool value) { push(Json(value)); return {}; }
std::expected<void, StarException> JsonBuilderStream::putNull() { push(Json()); return {}; }
std::expected<void, StarException> JsonBuilderStream::putWhitespace(std::span<char32_t const>) { return {}; }
std::expected<void, StarException> JsonBuilderStream::putColon() { return {}; }
std::expected<void, StarException> JsonBuilderStream::putComma() { return {}; }
std::size_t JsonBuilderStream::stackSize() { return m_stack.size(); }
std::expected<Json, StarException> JsonBuilderStream::takeTop() {
  if (m_stack.size() != 1 || !m_stack.back())
    return std::unexpected(JsonParsingException("Incomplete JSON builder stream"));
  return pop();
}
void JsonBuilderStream::push(Json value) { m_stack.push_back(std::move(value)); }
std::expected<Json, StarException> JsonBuilderStream::pop() {
  if (m_stack.empty() || !m_stack.back())
    return std::unexpected(JsonParsingException("Incomplete JSON builder stream"));
  Json value = std::move(*m_stack.back());
  m_stack.pop_back();
  return value;
}
std::expected<void, StarException> JsonBuilderStream::set(Json value) {
  if (!isSentry()) return std::unexpected(JsonParsingException("Missing JSON builder container"));
  m_stack.back() = std::move(value);
  return {};
}
void JsonBuilderStream::pushSentry() { m_stack.push_back({}); }
bool JsonBuilderStream::isSentry() { return !m_stack.empty() && !m_stack.back(); }

std::expected<void, StarException> JsonStreamer<Json>::toJsonStream(Json const& val, JsonStream& stream, bool sort) {
  switch (val.type()) {
    case Json::Type::Null: return stream.putNull();
    case Json::Type::Bool: {
      auto value = val.toBool();
      if (!value) return std::unexpected(std::move(value.error()));
      return stream.putBoolean(*value);
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
      return stream.putDouble(std::string_view(text.data(), static_cast<std::size_t>(formatted.size)));
    }
    case Json::Type::Int: {
      auto value = val.toInt();
      if (!value) return std::unexpected(std::move(value.error()));
      // digits10 excludes the final decimal digit; reserve that digit and a sign.
      std::array<char, std::numeric_limits<std::int64_t>::digits10 + 2> text;
      auto formatted = std::format_to_n(text.data(), text.size(), "{}", *value);
      if (formatted.size > static_cast<decltype(formatted.size)>(text.size()))
        return std::unexpected(JsonException("JSON integer formatting buffer exhausted"));
      return stream.putInteger(std::string_view(text.data(), static_cast<std::size_t>(formatted.size)));
    }
    case Json::Type::String: {
      auto value = val.stringPtr();
      if (!value) return std::unexpected(std::move(value.error()));
      return stream.putStringUtf8((*value)->utf8());
    }
    case Json::Type::Array: {
      auto elements = val.iterateArray();
      if (!elements) return std::unexpected(std::move(elements.error()));
      auto begun = stream.beginArray();
      if (!begun) return std::unexpected(std::move(begun.error()));
      bool first = true;
      for (auto const& element : *elements) {
        if (!first) {
          auto comma = stream.putComma();
          if (!comma) return std::unexpected(std::move(comma.error()));
        }
        first = false;
        auto written = toJsonStream(element, stream, sort);
        if (!written) return std::unexpected(std::move(written.error()));
      }
      return stream.endArray();
    }
    case Json::Type::Object: {
      auto object = val.objectPtr();
      if (!object) return std::unexpected(std::move(object.error()));
      auto begun = stream.beginObject();
      if (!begun) return std::unexpected(std::move(begun.error()));
      auto entry = [&](auto const& pair, bool first) -> std::expected<void, StarException> {
        if (!first) {
          auto comma = stream.putComma();
          if (!comma) return std::unexpected(std::move(comma.error()));
        }
        auto written = stream.objectKeyUtf8(pair.first.utf8());
        if (!written) return std::unexpected(std::move(written.error()));
        written = stream.putColon();
        if (!written) return std::unexpected(std::move(written.error()));
        return toJsonStream(pair.second, stream, sort);
      };
      bool first = true;
      if (sort) {
        List<JsonObject::const_iterator> iterators;
        iterators.reserve((*object)->size());
        for (auto i = (*object)->begin(); i != (*object)->end(); ++i)
          iterators.push_back(i);
        iterators.sort([](auto a, auto b) { return a->first < b->first; });
        for (auto i : iterators) {
          auto written = entry(*i, first);
          if (!written) return std::unexpected(std::move(written.error()));
          first = false;
        }
      } else {
        for (auto const& pair : **object) {
          auto written = entry(pair, first);
          if (!written) return std::unexpected(std::move(written.error()));
          first = false;
        }
      }
      return stream.endObject();
    }
  }
  return std::unexpected(JsonException("Invalid Json type"));
}

}
