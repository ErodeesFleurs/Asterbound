import std;
import star.json;
import star.hash;
import star.xx_hash;
import star.string;
import star.list;
import star.map;
import star.data_stream;
import star.algorithm;
import star.lexical_cast;
import star.format;
import star.text_formatter;

import star.json_parser;
import star.json_builder;
import star.json_path;
import star.file;

namespace Star {

namespace {
using JsonLookup = std::expected<std::optional<std::reference_wrapper<Json const>>, StarException>;
using JsonReference = std::expected<std::reference_wrapper<Json const>, StarException>;

template <typename Value>
concept json_conversion_value = std::constructible_from<Json, Value>;

template <typename Key>
concept json_key = std::same_as<Key, String> || std::same_as<Key, std::size_t>;

template <json_conversion_value T> using JsonConversion = std::expected<T, StarException> (Json::*)() const;

template <json_key Key> JsonReference require_json(Json const& owner, Key const& key) {
  auto value = owner.find(key);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return **value;
  if constexpr (std::same_as<Key, String>)
    return std::unexpected(JsonException::format("No such key in Json::get(\"{}\")", key));
  else
    return std::unexpected(JsonException::format("Json::get({}) out of range", key));
}

template <json_conversion_value T>
std::expected<T, StarException> convert_json(JsonReference value, JsonConversion<T> convert) {
  if (!value)
    return std::unexpected(std::move(value.error()));
  return (value->get().*convert)();
}

template <json_conversion_value T, typename Default>
  requires std::convertible_to<Default&&, T>
std::expected<T, StarException> convert_json_or(JsonLookup value, JsonConversion<T> convert, Default&& def,
                                                bool null_uses_default) {
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value && (!null_uses_default || !(*value)->get().is_null()))
    return ((*value)->get().*convert)();
  return std::forward<Default>(def);
}

template <json_conversion_value T>
std::expected<std::optional<T>, StarException> convert_optional_json(JsonLookup value, JsonConversion<T> convert) {
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (!*value || (*value)->get().is_null())
    return std::optional<T>{};
  auto converted = ((*value)->get().*convert)();
  if (!converted)
    return std::unexpected(std::move(converted.error()));
  return std::optional<T>(std::move(*converted));
}

// The checked entry points validate bounds before this single result allocation.
Json rebuild_array(JsonArray const& source, std::size_t index, std::size_t erased,
                   std::optional<Json> replacement = {}) {
  JsonArray result;
  result.reserve(source.size() - erased + replacement.has_value());
  std::span<Json const> values(source);
  result.append_range(values.first(index));
  if (replacement)
    result.push_back(std::move(*replacement));
  result.append_range(values.subspan(index + erased));
  return Json(std::move(result));
}

struct JsonTextSink {
  TextBuffer& m_output;

  std::expected<void, StarException> push(char value) {
    m_output.push_back(value);
    return {};
  }

  std::expected<void, StarException> append(std::string_view bytes) {
    m_output.append(bytes);
    return {};
  }
};

// Classify binary64 without floating predicates, which production fast-math
// may fold for nonfinite values. Only exactly represented int64 values share
// the integer hash; large adjacent integers must not collapse through double.
std::optional<std::int64_t> exact_json_integer(double value) {
  static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == sizeof(std::uint64_t));
  auto bits = std::bit_cast<std::uint64_t>(value);
  auto exponent = (bits >> 52) & 0x7ff;
  auto fraction = bits & 0x000fffffffffffff;
  if (exponent == 0)
    return fraction == 0 ? std::optional<std::int64_t>(0) : std::nullopt;
  if (exponent < 1023 || exponent > 1086)
    return {};
  unsigned power = static_cast<unsigned>(exponent - 1023);
  auto magnitude = fraction | 0x0010000000000000;
  if (power < 52) {
    unsigned shift = 52 - power;
    if ((magnitude & ((std::uint64_t{1} << shift) - 1)) != 0)
      return {};
    magnitude >>= shift;
  } else {
    magnitude <<= power - 52;
  }
  constexpr auto limit = std::uint64_t{1} << 63;
  if ((bits >> 63) != 0) {
    if (magnitude == limit)
      return std::numeric_limits<std::int64_t>::min();
    if (magnitude < limit)
      return -static_cast<std::int64_t>(magnitude);
  } else if (magnitude < limit) {
    return static_cast<std::int64_t>(magnitude);
  }
  return {};
}
}// namespace

std::expected<Json::Type, StarException> Json::type_from_name(String const& t) {
  if (t == "float")
    return Type::Float;
  else if (t == "bool")
    return Type::Bool;
  else if (t == "int")
    return Type::Int;
  else if (t == "string")
    return Type::String;
  else if (t == "array")
    return Type::Array;
  else if (t == "object")
    return Type::Object;
  else if (t == "null")
    return Type::Null;
  else
    return std::unexpected(JsonException(Star::format("String '{}' is not a valid json type", t)));
}

String Json::type_name(Type t) {
  switch (t) {
    case Type::Float:
      return "float";
    case Type::Bool:
      return "bool";
    case Type::Int:
      return "int";
    case Type::String:
      return "string";
    case Type::Array:
      return "array";
    case Type::Object:
      return "object";
    default:
      return "null";
  }
}

bool Json::operator==(const Json& v) const {
  if (type() == Type::Null && v.type() == Type::Null) {
    return true;
  } else if (type() != v.type()) {
    if ((type() == Type::Float || type() == Type::Int) && (v.type() == Type::Float || v.type() == Type::Int))
      {
        auto const& integer = type() == Type::Int ? *this : v;
        auto const& floating = type() == Type::Float ? *this : v;
        auto i = std::get<std::int64_t>(integer.m_data);
        auto d = std::get<double>(floating.m_data);
        return d >= -0x1p63 && d < 0x1p63 && d == static_cast<double>(i) && static_cast<std::int64_t>(d) == i;
      }
    return false;
  } else {
    if (type() == Type::Float)
      return std::get<double>(m_data) == std::get<double>(v.m_data);
    else if (type() == Type::Bool)
      return std::get<bool>(m_data) == std::get<bool>(v.m_data);
    else if (type() == Type::Int)
      return std::get<std::int64_t>(m_data) == std::get<std::int64_t>(v.m_data);
    else if (type() == Type::String)
      return *std::get<std::shared_ptr<String const>>(m_data) == *std::get<std::shared_ptr<String const>>(v.m_data);
    else if (type() == Type::Array)
      return *std::get<std::shared_ptr<JsonArray const>>(m_data)
        == *std::get<std::shared_ptr<JsonArray const>>(v.m_data);
    else if (type() == Type::Object)
      return *std::get<std::shared_ptr<JsonObject const>>(m_data)
        == *std::get<std::shared_ptr<JsonObject const>>(v.m_data);
  }
  return false;
}

bool Json::operator!=(const Json& v) const {
  return !(*this == v);
}

bool Json::unique() const {
  if (std::holds_alternative<std::shared_ptr<String const>>(m_data))
    return std::get<std::shared_ptr<String const>>(m_data).use_count() == 1;
  else if (std::holds_alternative<std::shared_ptr<JsonArray const>>(m_data))
    return std::get<std::shared_ptr<JsonArray const>>(m_data).use_count() == 1;
  else if (std::holds_alternative<std::shared_ptr<JsonObject const>>(m_data))
    return std::get<std::shared_ptr<JsonObject const>>(m_data).use_count() == 1;
  else
    return true;
}

Json Json::of_type(Type t) {
  switch (t) {
    case Type::Float:
      return Json(0.0);
    case Type::Bool:
      return Json(false);
    case Type::Int:
      return Json(0);
    case Type::String:
      return Json("");
    case Type::Array:
      return Json(JsonArray());
    case Type::Object:
      return Json(JsonObject());
    default:
      return Json();
  }
}

std::expected<Json, StarException> Json::parse(String const& string) {
  auto const& bytes = string.utf8();
  return input_utf8_json(bytes.begin(), bytes.end(), JsonParseType::Value);
}

std::expected<Json, StarException> Json::parse_sequence(String const& sequence) {
  auto const& bytes = sequence.utf8();
  return input_utf8_json(bytes.begin(), bytes.end(), JsonParseType::Sequence);
}

std::expected<Json, StarException> Json::parse_json(String const& json) {
  auto const& bytes = json.utf8();
  return input_utf8_json(bytes.begin(), bytes.end(), JsonParseType::Top);
}

Json::Json() = default;

Json::Json(double d) { m_data = d; }

Json::Json(int i) { m_data = static_cast<std::int64_t>(i); }

Json::Json(long i) { m_data = static_cast<std::int64_t>(i); }

Json::Json(long long i) { m_data = static_cast<std::int64_t>(i); }

Json::Json(unsigned int i) { m_data = static_cast<std::int64_t>(i); }

Json::Json(unsigned long i) { m_data = static_cast<std::int64_t>(i); }

Json::Json(unsigned long long i) { m_data = static_cast<std::int64_t>(i); }

Json::Json(std::string_view s) { m_data = std::make_shared<String const>(s); }

std::expected<Json, StarException> Json::create(std::span<String::Char const> s) {
  auto string = String::create(std::u32string_view(s.data(), s.size()));
  if (!string)
    return std::unexpected(std::move(string.error()));
  return Json(std::move(*string));
}

Json::Json(String s) { m_data = std::make_shared<String const>(std::move(s)); }

Json::Json(std::string s) { m_data = std::make_shared<String const>((std::move(s))); }

Json::Json(JsonArray l) { m_data = std::make_shared<JsonArray const>(std::move(l)); }

Json::Json(JsonObject m) { m_data = std::make_shared<JsonObject const>(std::move(m)); }

std::expected<double, StarException> Json::to_double() const {
  if (type() == Type::Float)
    return std::get<double>(m_data);
  if (type() == Type::Int)
    return static_cast<double>(std::get<std::int64_t>(m_data));
  return std::unexpected(JsonException::format("Improper conversion to double from {}", type_name()));
}

std::expected<float, StarException> Json::to_float() const {
  auto value = to_double();
  if (!value)
    return std::unexpected(std::move(value.error()));
  return static_cast<float>(*value);
}

std::expected<bool, StarException> Json::to_bool() const {
  if (type() != Type::Bool)
    return std::unexpected(JsonException::format("Improper conversion to bool from {}", type_name()));
  return std::get<bool>(m_data);
}

std::expected<std::int64_t, StarException> Json::to_int() const {
  if (type() == Type::Float) {
    auto value = std::get<double>(m_data);
    if (!(value >= -0x1p63 && value < 0x1p63))
      return std::unexpected(JsonException::format("Cannot convert out-of-range float {} to int", value));
    return static_cast<std::int64_t>(value);
  }
  if (type() == Type::Int)
    return static_cast<std::int64_t>(std::get<std::int64_t>(m_data));
  return std::unexpected(JsonException::format("Improper conversion to int from {}", type_name()));
}

std::expected<std::uint64_t, StarException> Json::to_uint() const {
  if (type() == Type::Float) {
    auto value = std::get<double>(m_data);
    if (!(value > -1.0 && value < 0x1p64))
      return std::unexpected(JsonException::format("Cannot convert out-of-range float {} to unsigned int", value));
    return static_cast<std::uint64_t>(value);
  }
  if (type() == Type::Int)
    return static_cast<std::uint64_t>(std::get<std::int64_t>(m_data));
  return std::unexpected(JsonException::format("Improper conversion to unsigned int from {}", type_name()));
}

std::expected<String, StarException> Json::to_string() const {
  if (type() != Type::String)
    return std::unexpected(JsonException::format("Cannot convert from {} to string", type_name()));
  return *std::get<std::shared_ptr<String const>>(m_data);
}

std::expected<std::shared_ptr<String const>, StarException> Json::string_ptr() const {
  if (type() != Type::String)
    return std::unexpected(JsonException::format("Cannot convert from {} to string", type_name()));
  return std::get<std::shared_ptr<String const>>(m_data);
}

std::expected<std::reference_wrapper<JsonArray const>, StarException> Json::array_ref() const& {
  if (type() != Type::Array)
    return std::unexpected(JsonException::format("Improper conversion to JsonArray from {}", type_name()));
  return std::cref(*std::get<std::shared_ptr<JsonArray const>>(m_data));
}

std::expected<JsonArray, StarException> Json::to_array() const {
  auto array = array_ref();
  if (!array)
    return std::unexpected(std::move(array.error()));
  return array->get();
}

std::expected<std::shared_ptr<JsonArray const>, StarException> Json::array_ptr() const {
  if (type() != Type::Array)
    return std::unexpected(JsonException::format("Improper conversion to JsonArray from {}", type_name()));
  return std::get<std::shared_ptr<JsonArray const>>(m_data);
}

std::expected<Json::IteratorWrapper<JsonArray>, StarException> Json::iterate_array() const {
  auto pointer = array_ptr();
  if (!pointer)
    return std::unexpected(std::move(pointer.error()));
  return IteratorWrapper<JsonArray>{std::move(*pointer)};
}

std::expected<JsonObject, StarException> Json::to_object() const {
  if (type() != Type::Object)
    return std::unexpected(JsonException::format("Improper conversion to JsonObject from {}", type_name()));
  return *std::get<std::shared_ptr<JsonObject const>>(m_data);
}

std::expected<std::shared_ptr<JsonObject const>, StarException> Json::object_ptr() const {
  if (type() != Type::Object)
    return std::unexpected(JsonException::format("Improper conversion to JsonObject from {}", type_name()));
  return std::get<std::shared_ptr<JsonObject const>>(m_data);
}

std::expected<Json::IteratorWrapper<JsonObject>, StarException> Json::iterate_object() const {
  auto pointer = object_ptr();
  if (!pointer)
    return std::unexpected(std::move(pointer.error()));
  return IteratorWrapper<JsonObject>{std::move(*pointer)};
}

std::optional<Json> Json::opt() const {
  if (is_null())
    return {};
  return *this;
}

std::expected<std::optional<double>, StarException> Json::opt_double() const {
  return convert_optional_json(JsonLookup(std::cref(*this)), &Json::to_double);
}
std::expected<std::optional<float>, StarException> Json::opt_float() const {
  return convert_optional_json(JsonLookup(std::cref(*this)), &Json::to_float);
}
std::expected<std::optional<bool>, StarException> Json::opt_bool() const {
  return convert_optional_json(JsonLookup(std::cref(*this)), &Json::to_bool);
}
std::expected<std::optional<std::int64_t>, StarException> Json::opt_int() const {
  return convert_optional_json(JsonLookup(std::cref(*this)), &Json::to_int);
}
std::expected<std::optional<std::uint64_t>, StarException> Json::opt_uint() const {
  return convert_optional_json(JsonLookup(std::cref(*this)), &Json::to_uint);
}
std::expected<std::optional<String>, StarException> Json::opt_string() const {
  return convert_optional_json(JsonLookup(std::cref(*this)), &Json::to_string);
}
std::expected<std::optional<JsonArray>, StarException> Json::opt_array() const {
  return convert_optional_json(JsonLookup(std::cref(*this)), &Json::to_array);
}
std::expected<std::optional<JsonObject>, StarException> Json::opt_object() const {
  return convert_optional_json(JsonLookup(std::cref(*this)), &Json::to_object);
}

std::expected<std::size_t, StarException> Json::size() const {
  if (type() == Type::Array)
    return std::get<std::shared_ptr<JsonArray const>>(m_data)->size();
  if (type() == Type::Object)
    return std::get<std::shared_ptr<JsonObject const>>(m_data)->size();
  return std::unexpected(JsonException("size() called on improper json type"));
}

std::expected<bool, StarException> Json::contains(String const& key) const {
  if (type() == Type::Object)
    return std::get<std::shared_ptr<JsonObject const>>(m_data)->contains(key);
  return std::unexpected(JsonException("contains() called on improper json type"));
}

std::expected<Json, StarException> Json::get(std::size_t index) const {
  auto value = require_json(*this, index);
  if (!value)
    return std::unexpected(std::move(value.error()));
  return value->get();
}
std::expected<Json, StarException> Json::get(std::size_t index, Json def) const {
  auto value = find(index);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return (*value)->get();
  return def;
}
std::expected<double, StarException> Json::get_double(std::size_t index) const {
  return convert_json(require_json(*this, index), &Json::to_double);
}
std::expected<double, StarException> Json::get_double(std::size_t index, double def) const {
  return convert_json_or(find(index), &Json::to_double, def, false);
}
std::expected<float, StarException> Json::get_float(std::size_t index) const {
  return convert_json(require_json(*this, index), &Json::to_float);
}
std::expected<float, StarException> Json::get_float(std::size_t index, float def) const {
  return convert_json_or(find(index), &Json::to_float, def, false);
}
std::expected<bool, StarException> Json::get_bool(std::size_t index) const {
  return convert_json(require_json(*this, index), &Json::to_bool);
}
std::expected<bool, StarException> Json::get_bool(std::size_t index, bool def) const {
  return convert_json_or(find(index), &Json::to_bool, def, false);
}
std::expected<std::int64_t, StarException> Json::get_int(std::size_t index) const {
  return convert_json(require_json(*this, index), &Json::to_int);
}
std::expected<std::int64_t, StarException> Json::get_int(std::size_t index, std::int64_t def) const {
  return convert_json_or(find(index), &Json::to_int, def, false);
}
std::expected<std::uint64_t, StarException> Json::get_uint(std::size_t index) const {
  return convert_json(require_json(*this, index), &Json::to_uint);
}
std::expected<std::uint64_t, StarException> Json::get_uint(std::size_t index, std::int64_t def) const {
  return convert_json_or(find(index), &Json::to_uint, def, false);
}
std::expected<String, StarException> Json::get_string(std::size_t index) const {
  return convert_json(require_json(*this, index), &Json::to_string);
}
std::expected<String, StarException> Json::get_string(std::size_t index, String def) const {
  return convert_json_or(find(index), &Json::to_string, std::move(def), false);
}
std::expected<JsonArray, StarException> Json::get_array(std::size_t index) const {
  return convert_json(require_json(*this, index), &Json::to_array);
}
std::expected<JsonArray, StarException> Json::get_array(std::size_t index, JsonArray def) const {
  return convert_json_or(find(index), &Json::to_array, std::move(def), false);
}
std::expected<JsonObject, StarException> Json::get_object(std::size_t index) const {
  return convert_json(require_json(*this, index), &Json::to_object);
}
std::expected<JsonObject, StarException> Json::get_object(std::size_t index, JsonObject def) const {
  return convert_json_or(find(index), &Json::to_object, std::move(def), false);
}

std::expected<Json, StarException> Json::get(String const& key) const {
  auto value = require_json(*this, key);
  if (!value)
    return std::unexpected(std::move(value.error()));
  return value->get();
}
std::expected<Json, StarException> Json::get(String const& key, Json def) const {
  auto value = find(key);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return (*value)->get();
  return def;
}
std::expected<double, StarException> Json::get_double(String const& key) const {
  return convert_json(require_json(*this, key), &Json::to_double);
}
std::expected<double, StarException> Json::get_double(String const& key, double def) const {
  return convert_json_or(find(key), &Json::to_double, def, true);
}
std::expected<float, StarException> Json::get_float(String const& key) const {
  return convert_json(require_json(*this, key), &Json::to_float);
}
std::expected<float, StarException> Json::get_float(String const& key, float def) const {
  return convert_json_or(find(key), &Json::to_float, def, true);
}
std::expected<bool, StarException> Json::get_bool(String const& key) const {
  return convert_json(require_json(*this, key), &Json::to_bool);
}
std::expected<bool, StarException> Json::get_bool(String const& key, bool def) const {
  return convert_json_or(find(key), &Json::to_bool, def, true);
}
std::expected<std::int64_t, StarException> Json::get_int(String const& key) const {
  return convert_json(require_json(*this, key), &Json::to_int);
}
std::expected<std::int64_t, StarException> Json::get_int(String const& key, std::int64_t def) const {
  return convert_json_or(find(key), &Json::to_int, def, true);
}
std::expected<std::uint64_t, StarException> Json::get_uint(String const& key) const {
  return convert_json(require_json(*this, key), &Json::to_uint);
}
std::expected<std::uint64_t, StarException> Json::get_uint(String const& key, std::int64_t def) const {
  return convert_json_or(find(key), &Json::to_uint, def, true);
}
std::expected<String, StarException> Json::get_string(String const& key) const {
  return convert_json(require_json(*this, key), &Json::to_string);
}
std::expected<String, StarException> Json::get_string(String const& key, String def) const {
  return convert_json_or(find(key), &Json::to_string, std::move(def), true);
}
std::expected<JsonArray, StarException> Json::get_array(String const& key) const {
  return convert_json(require_json(*this, key), &Json::to_array);
}
std::expected<JsonArray, StarException> Json::get_array(String const& key, JsonArray def) const {
  return convert_json_or(find(key), &Json::to_array, std::move(def), true);
}
std::expected<JsonObject, StarException> Json::get_object(String const& key) const {
  return convert_json(require_json(*this, key), &Json::to_object);
}
std::expected<JsonObject, StarException> Json::get_object(String const& key, JsonObject def) const {
  return convert_json_or(find(key), &Json::to_object, std::move(def), true);
}

std::expected<std::optional<Json>, StarException> Json::opt(String const& key) const {
  auto value = find(key);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value && !(*value)->get().is_null())
    return std::optional<Json>((*value)->get());
  return std::optional<Json>{};
}
std::expected<std::optional<double>, StarException> Json::opt_double(String const& key) const {
  return convert_optional_json(find(key), &Json::to_double);
}
std::expected<std::optional<float>, StarException> Json::opt_float(String const& key) const {
  return convert_optional_json(find(key), &Json::to_float);
}
std::expected<std::optional<bool>, StarException> Json::opt_bool(String const& key) const {
  return convert_optional_json(find(key), &Json::to_bool);
}
std::expected<std::optional<std::int64_t>, StarException> Json::opt_int(String const& key) const {
  return convert_optional_json(find(key), &Json::to_int);
}
std::expected<std::optional<std::uint64_t>, StarException> Json::opt_uint(String const& key) const {
  return convert_optional_json(find(key), &Json::to_uint);
}
std::expected<std::optional<String>, StarException> Json::opt_string(String const& key) const {
  return convert_optional_json(find(key), &Json::to_string);
}
std::expected<std::optional<JsonArray>, StarException> Json::opt_array(String const& key) const {
  return convert_optional_json(find(key), &Json::to_array);
}
std::expected<std::optional<JsonObject>, StarException> Json::opt_object(String const& key) const {
  return convert_optional_json(find(key), &Json::to_object);
}

std::expected<Json, StarException> Json::query(String const& path) const {
  return JsonPath::path_get(*this, JsonPath::parse_query_path, path);
}
std::expected<double, StarException> Json::query_double(String const& path) const {
  return convert_json(JsonPath::path_get_ref(*this, JsonPath::parse_query_path, path), &Json::to_double);
}
std::expected<float, StarException> Json::query_float(String const& path) const {
  return convert_json(JsonPath::path_get_ref(*this, JsonPath::parse_query_path, path), &Json::to_float);
}
std::expected<bool, StarException> Json::query_bool(String const& path) const {
  return convert_json(JsonPath::path_get_ref(*this, JsonPath::parse_query_path, path), &Json::to_bool);
}
std::expected<std::int64_t, StarException> Json::query_int(String const& path) const {
  return convert_json(JsonPath::path_get_ref(*this, JsonPath::parse_query_path, path), &Json::to_int);
}
std::expected<std::uint64_t, StarException> Json::query_uint(String const& path) const {
  return convert_json(JsonPath::path_get_ref(*this, JsonPath::parse_query_path, path), &Json::to_uint);
}
std::expected<String, StarException> Json::query_string(String const& path) const {
  return convert_json(JsonPath::path_get_ref(*this, JsonPath::parse_query_path, path), &Json::to_string);
}
std::expected<JsonArray, StarException> Json::query_array(String const& path) const {
  return convert_json(JsonPath::path_get_ref(*this, JsonPath::parse_query_path, path), &Json::to_array);
}
std::expected<JsonObject, StarException> Json::query_object(String const& path) const {
  return convert_json(JsonPath::path_get_ref(*this, JsonPath::parse_query_path, path), &Json::to_object);
}
std::expected<Json, StarException> Json::query(String const& path, Json def) const {
  auto value = JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return (*value)->get();
  return def;
}
std::expected<double, StarException> Json::query_double(String const& path, double def) const {
  return convert_json_or(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_double, def, true);
}
std::expected<float, StarException> Json::query_float(String const& path, float def) const {
  return convert_json_or(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_float, def, true);
}
std::expected<bool, StarException> Json::query_bool(String const& path, bool def) const {
  return convert_json_or(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_bool, def, true);
}
std::expected<std::int64_t, StarException> Json::query_int(String const& path, std::int64_t def) const {
  return convert_json_or(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_int, def, true);
}
std::expected<std::uint64_t, StarException> Json::query_uint(String const& path, std::uint64_t def) const {
  return convert_json_or(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_uint, def, true);
}
std::expected<String, StarException> Json::query_string(String const& path, String const& def) const {
  return convert_json_or(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_string, def, true);
}
std::expected<JsonArray, StarException> Json::query_array(String const& path, JsonArray def) const {
  return convert_json_or(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_array,
                         std::move(def), true);
}
std::expected<JsonObject, StarException> Json::query_object(String const& path, JsonObject def) const {
  return convert_json_or(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_object,
                         std::move(def), true);
}
std::expected<std::optional<Json>, StarException> Json::opt_query(String const& path) const {
  auto value = JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value && !(*value)->get().is_null())
    return std::optional<Json>((*value)->get());
  return std::optional<Json>{};
}
std::expected<std::optional<double>, StarException> Json::opt_query_double(String const& path) const {
  return convert_optional_json(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_double);
}
std::expected<std::optional<float>, StarException> Json::opt_query_float(String const& path) const {
  return convert_optional_json(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_float);
}
std::expected<std::optional<bool>, StarException> Json::opt_query_bool(String const& path) const {
  return convert_optional_json(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_bool);
}
std::expected<std::optional<std::int64_t>, StarException> Json::opt_query_int(String const& path) const {
  return convert_optional_json(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_int);
}
std::expected<std::optional<std::uint64_t>, StarException> Json::opt_query_uint(String const& path) const {
  return convert_optional_json(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_uint);
}
std::expected<std::optional<String>, StarException> Json::opt_query_string(String const& path) const {
  return convert_optional_json(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_string);
}
std::expected<std::optional<JsonArray>, StarException> Json::opt_query_array(String const& path) const {
  return convert_optional_json(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_array);
}
std::expected<std::optional<JsonObject>, StarException> Json::opt_query_object(String const& path) const {
  return convert_optional_json(JsonPath::path_find_ref(*this, JsonPath::parse_query_path, path), &Json::to_object);
}

std::expected<Json, StarException> Json::set(String key, Json value) const {
  auto map = to_object();
  if (!map)
    return std::unexpected(std::move(map.error()));
  (*map)[std::move(key)] = std::move(value);
  return Json(std::move(*map));
}

std::expected<Json, StarException> Json::set_path(String path, Json value) const {
  return JsonPath::path_set(*this, JsonPath::parse_query_path, path, value);
}

std::expected<Json, StarException> Json::erase_path(String path) const {
  return JsonPath::path_remove(*this, JsonPath::parse_query_path, path);
}

std::expected<Json, StarException> Json::set_all(JsonObject values) const {
  auto map = to_object();
  if (!map)
    return std::unexpected(std::move(map.error()));
  for (auto&& p : values)
    (*map)[std::move(p.first)] = std::move(p.second);
  return Json(std::move(*map));
}

std::expected<Json, StarException> Json::erase(String key) const {
  auto map = to_object();
  if (!map)
    return std::unexpected(std::move(map.error()));
  map->erase(key);
  return Json(std::move(*map));
}

std::expected<Json, StarException> Json::set(std::size_t index, Json value) const {
  auto array = array_ref();
  if (!array)
    return std::unexpected(std::move(array.error()));
  if (index >= array->get().size())
    return std::unexpected(JsonException::format("Json::set({}) out of range", index));
  return rebuild_array(array->get(), index, 1, std::move(value));
}

std::expected<Json, StarException> Json::insert(std::size_t index, Json value) const {
  auto array = array_ref();
  if (!array)
    return std::unexpected(std::move(array.error()));
  if (index > array->get().size())
    return std::unexpected(JsonException::format("Json::insert({}) out of range", index));
  return rebuild_array(array->get(), index, 0, std::move(value));
}

std::expected<Json, StarException> Json::append(Json value) const {
  auto array = array_ref();
  if (!array)
    return std::unexpected(std::move(array.error()));
  return rebuild_array(array->get(), array->get().size(), 0, std::move(value));
}

std::expected<Json, StarException> Json::erase(std::size_t index) const {
  auto array = array_ref();
  if (!array)
    return std::unexpected(std::move(array.error()));
  if (index >= array->get().size())
    return std::unexpected(JsonException::format("Json::eraseIndex({}) out of range", index));
  return rebuild_array(array->get(), index, 1);
}

Json::Type Json::type() const { return static_cast<Type>(m_data.index()); }

String Json::type_name() const {
  return type_name(type());
}

std::expected<Json, StarException> Json::convert(Type target) const {
  if (type() == target)
    return *this;
  switch (target) {
    case Type::Null:
      return Json();
    case Type::Float: {
      auto value = to_double();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    case Type::Bool: {
      auto value = to_bool();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    case Type::Int: {
      auto value = to_int();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    case Type::String: {
      auto value = to_string();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    case Type::Array: {
      auto value = to_array();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    case Type::Object: {
      auto value = to_object();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    default:
      return std::unexpected(JsonException::format("Improper conversion to type {}", type_name(target)));
  }
}

bool Json::is_type(Type t) const {
  return type() == t;
}

bool Json::can_convert(Type t) const {
  if (type() == t)
    return true;

  if (t == Type::Null)
    return true;

  if ((type() == Type::Float || type() == Type::Int) && (t == Type::Float || t == Type::Int))
    return true;

  return false;
}

bool Json::is_null() const {
  return type() == Type::Null;
}

Json::operator bool() const {
  return !is_null();
}

std::expected<String, StarException> Json::repr(int pretty, bool sort) const {
  std::string result;
  auto written = output_utf8_json(*this, JsonUtf8StringOutputIterator(result), pretty, sort);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return String(std::move(result));
}

std::expected<String, StarException> Json::print_json(int pretty, bool sort) const {
  if (type() != Type::Object && type() != Type::Array)
    return std::unexpected(JsonException("printJson called on non-top-level JSON type"));

  return repr(pretty, sort);
}

std::expected<String, StarException> Json::print_string() const {
  if (type() == Type::String)
    return *std::get<std::shared_ptr<String const>>(m_data);
  return repr(0, false);
}

std::expected<void, StarException> write_text(TextBuffer& output, Json const& value) {
  return output_utf8_json(value, JsonTextSink{output}, 0, false);
}

std::expected<void, StarException> write_text(TextBuffer& output, JsonObject const& value) {
  JsonWriter<JsonTextSink, true> writer(JsonTextSink{output}, 0);
  return JsonStreamer<Json>::stream_object(value, writer, false);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Json const& value) {
  auto header = ds.write<std::uint8_t>(static_cast<std::uint8_t>(value.type()) + 1);
  if (!header)
    return std::unexpected(std::move(header.error()));
  switch (value.type()) {
    case Json::Type::Float: {
      auto converted = value.to_double();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto written = ds.write<double>(*converted);
      if (!written)
        return std::unexpected(std::move(written.error()));
      break;
    }
    case Json::Type::Bool: {
      auto converted = value.to_bool();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto written = ds.write<bool>(*converted);
      if (!written)
        return std::unexpected(std::move(written.error()));
      break;
    }
    case Json::Type::Int: {
      auto converted = value.to_int();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto written = ds.write_vlq_i(*converted);
      if (!written)
        return std::unexpected(std::move(written.error()));
      break;
    }
    case Json::Type::String: {
      auto converted = value.string_ptr();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto written = ds.write<String>(**converted);
      if (!written)
        return std::unexpected(std::move(written.error()));
      break;
    }
    case Json::Type::Array: {
      auto converted = value.array_ptr();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto count = ds.write_vlq_u((*converted)->size());
      if (!count)
        return std::unexpected(std::move(count.error()));
      for (auto const& entry : **converted) {
        auto item = ds.write<Json>(entry);
        if (!item)
          return std::unexpected(std::move(item.error()));
      }
      break;
    }
    case Json::Type::Object: {
      auto converted = value.object_ptr();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto count = ds.write_vlq_u((*converted)->size());
      if (!count)
        return std::unexpected(std::move(count.error()));
      for (auto const& entry : **converted) {
        auto key = ds.write<String>(entry.first);
        if (!key)
          return std::unexpected(std::move(key.error()));
        auto item = ds.write<Json>(entry.second);
        if (!item)
          return std::unexpected(std::move(item.error()));
      }
      break;
    }
    default:
      break;
  }
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Json& value) {
  auto header = ds.read<std::uint8_t>();
  if (!header)
    return std::unexpected(std::move(header.error()));
  auto type_index = *header;
  if (type_index > 0)
    --type_index;
  auto type = static_cast<Json::Type>(type_index);
  switch (type) {
    case Json::Type::Float: {
      auto item = ds.read<double>();
      if (!item)
        return std::unexpected(std::move(item.error()));
      value = Json(std::move(*item));
      break;
    }
    case Json::Type::Bool: {
      auto item = ds.read<bool>();
      if (!item)
        return std::unexpected(std::move(item.error()));
      value = Json(std::move(*item));
      break;
    }
    case Json::Type::Int: {
      auto item = ds.read_vlq_i();
      if (!item)
        return std::unexpected(std::move(item.error()));
      value = Json(std::move(*item));
      break;
    }
    case Json::Type::String: {
      auto item = ds.read<String>();
      if (!item)
        return std::unexpected(std::move(item.error()));
      value = Json(std::move(*item));
      break;
    }
    case Json::Type::Array: {
      auto count = ds.read_vlq_u();
      if (!count)
        return std::unexpected(std::move(count.error()));
      JsonArray items;
      for (std::uint64_t index = 0; index < *count; ++index) {
        auto item = ds.read<Json>();
        if (!item)
          return std::unexpected(std::move(item.error()));
        items.push_back(std::move(*item));
      }
      value = Json(std::move(items));
      break;
    }
    case Json::Type::Object: {
      auto count = ds.read_vlq_u();
      if (!count)
        return std::unexpected(std::move(count.error()));
      JsonObject items;
      for (std::uint64_t index = 0; index < *count; ++index) {
        auto key = ds.read<String>();
        if (!key)
          return std::unexpected(std::move(key.error()));
        auto item = ds.read<Json>();
        if (!item)
          return std::unexpected(std::move(item.error()));
        items[std::move(*key)] = std::move(*item);
      }
      value = Json(std::move(items));
      break;
    }
    default:
      break;
  }
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, JsonArray const& value) {
  auto result = ds.write_container(value);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, JsonArray& value) {
  auto result = ds.read_container(value);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, JsonObject const& value) {
  auto result = ds.write_map_container(value);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, JsonObject& value) {
  auto result = ds.read_map_container(value);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

void Json::get_hash(XXHash3& hasher) const {
  Json::Type type = static_cast<Json::Type>(m_data.index());
  if (type == Json::Type::Float) {
    if (auto integer = exact_json_integer(std::get<double>(m_data))) {
      // Preserve the old integer domain and bytes, recursively for containers.
      auto integer_type = Json::Type::Int;
      hasher.push(std::span<char const>(reinterpret_cast<char const*>(&integer_type), sizeof(integer_type)));
      hasher.push(std::span<char const>(reinterpret_cast<char const*>(&*integer), sizeof(*integer)));
      return;
    }
  }
  if (type == Json::Type::Bool)
    hasher.push(std::span<char const>(std::get<bool>(m_data) ? "\2\1" : "\2\0", 2));
  else {
    // Preserve the existing hash's native object bytes; these casts only expose
    // the active scalar representation to the byte-range hashing boundary.
    hasher.push(std::span<char const>(reinterpret_cast<char const*>(&type), sizeof(type)));
    if (type == Json::Type::Float)
      hasher.push(std::span<char const>(reinterpret_cast<char const*>(&std::get<double>(m_data)), sizeof(double)));
    else if (type == Json::Type::Int)
      hasher.push(
        std::span<char const>(reinterpret_cast<char const*>(&std::get<std::int64_t>(m_data)), sizeof(std::int64_t)));
    else if (type == Json::Type::String) {
      const String& str = *std::get<std::shared_ptr<String const>>(m_data);
      hasher.push(std::span<char const>(str.utf8()));
    }
    else if (type == Json::Type::Array) {
      for (Json const& json : *std::get<std::shared_ptr<JsonArray const>>(m_data))
        json.get_hash(hasher);
    }
    else if (type == Json::Type::Object) {
      auto& object = *std::get<std::shared_ptr<JsonObject const>>(m_data);
      // The type byte already represents an empty object; one member is
      // already in key order and needs no iterator scratch storage.
      if (object.empty())
        return;
      if (object.size() == 1) {
        auto iter = object.begin();
        hasher.push(std::span<char const>(iter->first.utf8()));
        iter->second.get_hash(hasher);
        return;
      }
      List<JsonObject::const_iterator> iterators;
      iterators.reserve(object.size());
      for (auto i = object.begin(); i != object.end(); ++i)
        iterators.push_back(i);
      iterators.sort([](JsonObject::const_iterator a, JsonObject::const_iterator b) {
        return a->first < b->first;
      });
      for (auto& iter : iterators) {
        hasher.push(std::span<char const>(iter->first.utf8()));
        iter->second.get_hash(hasher);
      }
    }
  }
}

std::size_t hash<Json>::operator()(Json const& v) const {
  XXHash3 hasher;
  v.get_hash(hasher);
  return hasher.digest();
}

std::expected<std::optional<std::reference_wrapper<Json const>>, StarException> Json::find(std::size_t index) const& {
  if (type() != Type::Array)
    return std::unexpected(JsonException::format("Cannot call get with index on Json type {}, must be Array type", type_name()));

  auto const& list = *std::get<std::shared_ptr<JsonArray const>>(m_data);
  if (index >= list.size())
    return std::nullopt;
  return std::cref(list[index]);
}

std::expected<std::optional<std::reference_wrapper<Json const>>, StarException> Json::find(String const& key) const& {
  if (type() != Type::Object)
    return std::unexpected(JsonException::format("Cannot call get with key on Json type {}, must be Object type", type_name()));
  auto const& map = std::get<std::shared_ptr<JsonObject const>>(m_data);

  return map->find_ref(key);
}

Json json_merge(Json const& base, Json const& merger) {
  if (base.type() == Json::Type::Object && merger.type() == Json::Type::Object) {
    JsonObject merged = *std::get<std::shared_ptr<JsonObject const>>(base.m_data);
    for (auto const& p : *std::get<std::shared_ptr<JsonObject const>>(merger.m_data)) {
      auto res = merged.insert(p);
      if (!res.second)
        res.first->second = json_merge(res.first->second, p.second);
    }
    return merged;
  }
  return merger.type() == Json::Type::Null ? base : merger;
}

Json json_merge_nulling(Json const& base, Json const& merger) {
  if (base.type() == Json::Type::Object && merger.type() == Json::Type::Object) {
    JsonObject merged = *std::get<std::shared_ptr<JsonObject const>>(base.m_data);
    for (auto const& p : *std::get<std::shared_ptr<JsonObject const>>(merger.m_data)) {
      if (p.second.is_null())
        merged.erase(p.first);
      else {
        auto res = merged.insert(p);
        if (!res.second)
          res.first->second = json_merge_nulling(res.first->second, p.second);
      }
    }
    return merged;
  }
  return merger;
}

bool json_partial_match(Json const& base, Json const& compare) {
  if (base.type() == Json::Type::Object && compare.type() == Json::Type::Object) {
    auto const& object = *std::get<std::shared_ptr<JsonObject const>>(base.m_data);
    auto const& pattern = *std::get<std::shared_ptr<JsonObject const>>(compare.m_data);
    if (&object == &pattern)
      return true;
    for (auto const& [key, value] : pattern) {
      auto entry = object.find(key);
      if (entry == object.end() || !json_partial_match(entry->second, value))
        return false;
    }
    return true;
  }
  if (base == compare)
    return true;
  if (base.type() == Json::Type::Array && compare.type() == Json::Type::Array) {
    for (auto const& c : *std::get<std::shared_ptr<JsonArray const>>(compare.m_data)) {
      bool similar = false;
      for (auto const& b : *std::get<std::shared_ptr<JsonArray const>>(base.m_data)) {
        if (json_partial_match(c, b)) {
          similar = true;
          break;
        }
      }
      if (!similar)
        return false;
    }
    return true;
  }
  return false;
}

}