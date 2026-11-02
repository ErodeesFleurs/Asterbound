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

import star.json_parser;
import star.json_builder;
import star.json_path;
import star.file;

namespace Star {

namespace {
using JsonLookup = std::expected<std::optional<std::reference_wrapper<Json const>>, StarException>;
using JsonReference = std::expected<std::reference_wrapper<Json const>, StarException>;

template <typename T> using JsonConversion = std::expected<T, StarException> (Json::*)() const;

template <typename Key> JsonReference requireJson(Json const& owner, Key const& key) {
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

template <typename T> std::expected<T, StarException> convertJson(JsonReference value, JsonConversion<T> convert) {
  if (!value)
    return std::unexpected(std::move(value.error()));
  return (value->get().*convert)();
}

template <typename T, typename Default>
std::expected<T, StarException> convertJsonOr(JsonLookup value, JsonConversion<T> convert, Default&& def,
                                              bool nullUsesDefault) {
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value && (!nullUsesDefault || !(*value)->get().isNull()))
    return ((*value)->get().*convert)();
  return std::forward<Default>(def);
}

template <typename T>
std::expected<std::optional<T>, StarException> convertOptionalJson(JsonLookup value, JsonConversion<T> convert) {
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (!*value || (*value)->get().isNull())
    return std::optional<T>{};
  auto converted = ((*value)->get().*convert)();
  if (!converted)
    return std::unexpected(std::move(converted.error()));
  return std::optional<T>(std::move(*converted));
}

// Classify binary64 without floating predicates, which production fast-math
// may fold for nonfinite values. Only exactly represented int64 values share
// the integer hash; large adjacent integers must not collapse through double.
std::optional<std::int64_t> exactJsonInteger(double value) {
  static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == sizeof(std::uint64_t));
  std::uint64_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
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

std::expected<Json::Type, StarException> Json::typeFromName(String const& t) {
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
    return std::unexpected(JsonException(strf("String '{}' is not a valid json type", t)));
}

String Json::typeName(Type t) {
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
        return d >= -0x1p63 && d < 0x1p63 && d == (double)i && (std::int64_t)d == i;
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
      return *std::get<std::shared_ptr<JsonArray const>>(m_data) == *std::get<std::shared_ptr<JsonArray const>>(v.m_data);
    else if (type() == Type::Object)
      return *std::get<std::shared_ptr<JsonObject const>>(m_data) == *std::get<std::shared_ptr<JsonObject const>>(v.m_data);
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

Json Json::ofType(Type t) {
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
  return inputUtf8Json(bytes.begin(), bytes.end(), JsonParseType::Value);
}

std::expected<Json, StarException> Json::parseSequence(String const& sequence) {
  auto const& bytes = sequence.utf8();
  return inputUtf8Json(bytes.begin(), bytes.end(), JsonParseType::Sequence);
}

std::expected<Json, StarException> Json::parseJson(String const& json) {
  auto const& bytes = json.utf8();
  return inputUtf8Json(bytes.begin(), bytes.end(), JsonParseType::Top);
}

Json::Json() {}

Json::Json(double d) {
  m_data = d;
}


Json::Json(int i) {
  m_data = (std::int64_t)i;
}

Json::Json(long i) {
  m_data = (std::int64_t)i;
}

Json::Json(long long i) {
  m_data = (std::int64_t)i;
}

Json::Json(unsigned int i) {
  m_data = (std::int64_t)i;
}

Json::Json(unsigned long i) {
  m_data = (std::int64_t)i;
}

Json::Json(unsigned long long i) {
  m_data = (std::int64_t)i;
}

Json::Json(std::string_view s) {
  m_data = std::make_shared<String const>(s);
}

std::expected<Json, StarException> Json::create(std::span<String::Char const> s) {
  auto string = String::create(std::u32string_view(s.data(), s.size()));
  if (!string)
    return std::unexpected(std::move(string.error()));
  return Json(std::move(*string));
}

Json::Json(String s) {
  m_data = std::make_shared<String const>(std::move(s));
}

Json::Json(std::string s) {
  m_data = std::make_shared<String const>((std::move(s)));
}

Json::Json(JsonArray l) {
  m_data = std::make_shared<JsonArray const>(std::move(l));
}

Json::Json(JsonObject m) {
  m_data = std::make_shared<JsonObject const>(std::move(m));
}

std::expected<double, StarException> Json::toDouble() const {
  if (type() == Type::Float)
    return std::get<double>(m_data);
  if (type() == Type::Int)
    return (double)std::get<std::int64_t>(m_data);
  return std::unexpected(JsonException::format("Improper conversion to double from {}", typeName()));
}

std::expected<float, StarException> Json::toFloat() const {
  auto value = toDouble();
  if (!value)
    return std::unexpected(std::move(value.error()));
  return (float)*value;
}

std::expected<bool, StarException> Json::toBool() const {
  if (type() != Type::Bool)
    return std::unexpected(JsonException::format("Improper conversion to bool from {}", typeName()));
  return std::get<bool>(m_data);
}

std::expected<std::int64_t, StarException> Json::toInt() const {
  if (type() == Type::Float) {
    auto value = std::get<double>(m_data);
    if (!(value >= -0x1p63 && value < 0x1p63))
      return std::unexpected(JsonException::format("Cannot convert out-of-range float {} to int", value));
    return (std::int64_t)value;
  }
  if (type() == Type::Int)
    return (std::int64_t)std::get<std::int64_t>(m_data);
  return std::unexpected(JsonException::format("Improper conversion to int from {}", typeName()));
}

std::expected<std::uint64_t, StarException> Json::toUInt() const {
  if (type() == Type::Float) {
    auto value = std::get<double>(m_data);
    if (!(value > -1.0 && value < 0x1p64))
      return std::unexpected(JsonException::format("Cannot convert out-of-range float {} to unsigned int", value));
    return (std::uint64_t)value;
  }
  if (type() == Type::Int)
    return (std::uint64_t)std::get<std::int64_t>(m_data);
  return std::unexpected(JsonException::format("Improper conversion to unsigned int from {}", typeName()));
}

std::expected<String, StarException> Json::toString() const {
  if (type() != Type::String)
    return std::unexpected(JsonException::format("Cannot convert from {} to string", typeName()));
  return *std::get<std::shared_ptr<String const>>(m_data);
}

std::expected<std::shared_ptr<String const>, StarException> Json::stringPtr() const {
  if (type() != Type::String)
    return std::unexpected(JsonException::format("Cannot convert from {} to string", typeName()));
  return std::get<std::shared_ptr<String const>>(m_data);
}

std::expected<JsonArray, StarException> Json::toArray() const {
  if (type() != Type::Array)
    return std::unexpected(JsonException::format("Improper conversion to JsonArray from {}", typeName()));
  return *std::get<std::shared_ptr<JsonArray const>>(m_data);
}

std::expected<std::shared_ptr<JsonArray const>, StarException> Json::arrayPtr() const {
  if (type() != Type::Array)
    return std::unexpected(JsonException::format("Improper conversion to JsonArray from {}", typeName()));
  return std::get<std::shared_ptr<JsonArray const>>(m_data);
}

std::expected<Json::IteratorWrapper<JsonArray>, StarException> Json::iterateArray() const {
  auto pointer = arrayPtr();
  if (!pointer)
    return std::unexpected(std::move(pointer.error()));
  return IteratorWrapper<JsonArray>{std::move(*pointer)};
}

std::expected<JsonObject, StarException> Json::toObject() const {
  if (type() != Type::Object)
    return std::unexpected(JsonException::format("Improper conversion to JsonObject from {}", typeName()));
  return *std::get<std::shared_ptr<JsonObject const>>(m_data);
}

std::expected<std::shared_ptr<JsonObject const>, StarException> Json::objectPtr() const {
  if (type() != Type::Object)
    return std::unexpected(JsonException::format("Improper conversion to JsonObject from {}", typeName()));
  return std::get<std::shared_ptr<JsonObject const>>(m_data);
}

std::expected<Json::IteratorWrapper<JsonObject>, StarException> Json::iterateObject() const {
  auto pointer = objectPtr();
  if (!pointer)
    return std::unexpected(std::move(pointer.error()));
  return IteratorWrapper<JsonObject>{std::move(*pointer)};
}

std::optional<Json> Json::opt() const {
  if (isNull())
    return {};
  return *this;
}

std::expected<std::optional<double>, StarException> Json::optDouble() const {
  return convertOptionalJson(JsonLookup(std::cref(*this)), &Json::toDouble);
}
std::expected<std::optional<float>, StarException> Json::optFloat() const {
  return convertOptionalJson(JsonLookup(std::cref(*this)), &Json::toFloat);
}
std::expected<std::optional<bool>, StarException> Json::optBool() const {
  return convertOptionalJson(JsonLookup(std::cref(*this)), &Json::toBool);
}
std::expected<std::optional<std::int64_t>, StarException> Json::optInt() const {
  return convertOptionalJson(JsonLookup(std::cref(*this)), &Json::toInt);
}
std::expected<std::optional<std::uint64_t>, StarException> Json::optUInt() const {
  return convertOptionalJson(JsonLookup(std::cref(*this)), &Json::toUInt);
}
std::expected<std::optional<String>, StarException> Json::optString() const {
  return convertOptionalJson(JsonLookup(std::cref(*this)), &Json::toString);
}
std::expected<std::optional<JsonArray>, StarException> Json::optArray() const {
  return convertOptionalJson(JsonLookup(std::cref(*this)), &Json::toArray);
}
std::expected<std::optional<JsonObject>, StarException> Json::optObject() const {
  return convertOptionalJson(JsonLookup(std::cref(*this)), &Json::toObject);
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
  auto value = requireJson(*this, index);
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
std::expected<double, StarException> Json::getDouble(std::size_t index) const {
  return convertJson(requireJson(*this, index), &Json::toDouble);
}
std::expected<double, StarException> Json::getDouble(std::size_t index, double def) const {
  return convertJsonOr(find(index), &Json::toDouble, def, false);
}
std::expected<float, StarException> Json::getFloat(std::size_t index) const {
  return convertJson(requireJson(*this, index), &Json::toFloat);
}
std::expected<float, StarException> Json::getFloat(std::size_t index, float def) const {
  return convertJsonOr(find(index), &Json::toFloat, def, false);
}
std::expected<bool, StarException> Json::getBool(std::size_t index) const {
  return convertJson(requireJson(*this, index), &Json::toBool);
}
std::expected<bool, StarException> Json::getBool(std::size_t index, bool def) const {
  return convertJsonOr(find(index), &Json::toBool, def, false);
}
std::expected<std::int64_t, StarException> Json::getInt(std::size_t index) const {
  return convertJson(requireJson(*this, index), &Json::toInt);
}
std::expected<std::int64_t, StarException> Json::getInt(std::size_t index, std::int64_t def) const {
  return convertJsonOr(find(index), &Json::toInt, def, false);
}
std::expected<std::uint64_t, StarException> Json::getUInt(std::size_t index) const {
  return convertJson(requireJson(*this, index), &Json::toUInt);
}
std::expected<std::uint64_t, StarException> Json::getUInt(std::size_t index, std::int64_t def) const {
  return convertJsonOr(find(index), &Json::toUInt, def, false);
}
std::expected<String, StarException> Json::getString(std::size_t index) const {
  return convertJson(requireJson(*this, index), &Json::toString);
}
std::expected<String, StarException> Json::getString(std::size_t index, String def) const {
  return convertJsonOr(find(index), &Json::toString, std::move(def), false);
}
std::expected<JsonArray, StarException> Json::getArray(std::size_t index) const {
  return convertJson(requireJson(*this, index), &Json::toArray);
}
std::expected<JsonArray, StarException> Json::getArray(std::size_t index, JsonArray def) const {
  return convertJsonOr(find(index), &Json::toArray, std::move(def), false);
}
std::expected<JsonObject, StarException> Json::getObject(std::size_t index) const {
  return convertJson(requireJson(*this, index), &Json::toObject);
}
std::expected<JsonObject, StarException> Json::getObject(std::size_t index, JsonObject def) const {
  return convertJsonOr(find(index), &Json::toObject, std::move(def), false);
}

std::expected<Json, StarException> Json::get(String const& key) const {
  auto value = requireJson(*this, key);
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
std::expected<double, StarException> Json::getDouble(String const& key) const {
  return convertJson(requireJson(*this, key), &Json::toDouble);
}
std::expected<double, StarException> Json::getDouble(String const& key, double def) const {
  return convertJsonOr(find(key), &Json::toDouble, def, true);
}
std::expected<float, StarException> Json::getFloat(String const& key) const {
  return convertJson(requireJson(*this, key), &Json::toFloat);
}
std::expected<float, StarException> Json::getFloat(String const& key, float def) const {
  return convertJsonOr(find(key), &Json::toFloat, def, true);
}
std::expected<bool, StarException> Json::getBool(String const& key) const {
  return convertJson(requireJson(*this, key), &Json::toBool);
}
std::expected<bool, StarException> Json::getBool(String const& key, bool def) const {
  return convertJsonOr(find(key), &Json::toBool, def, true);
}
std::expected<std::int64_t, StarException> Json::getInt(String const& key) const {
  return convertJson(requireJson(*this, key), &Json::toInt);
}
std::expected<std::int64_t, StarException> Json::getInt(String const& key, std::int64_t def) const {
  return convertJsonOr(find(key), &Json::toInt, def, true);
}
std::expected<std::uint64_t, StarException> Json::getUInt(String const& key) const {
  return convertJson(requireJson(*this, key), &Json::toUInt);
}
std::expected<std::uint64_t, StarException> Json::getUInt(String const& key, std::int64_t def) const {
  return convertJsonOr(find(key), &Json::toUInt, def, true);
}
std::expected<String, StarException> Json::getString(String const& key) const {
  return convertJson(requireJson(*this, key), &Json::toString);
}
std::expected<String, StarException> Json::getString(String const& key, String def) const {
  return convertJsonOr(find(key), &Json::toString, std::move(def), true);
}
std::expected<JsonArray, StarException> Json::getArray(String const& key) const {
  return convertJson(requireJson(*this, key), &Json::toArray);
}
std::expected<JsonArray, StarException> Json::getArray(String const& key, JsonArray def) const {
  return convertJsonOr(find(key), &Json::toArray, std::move(def), true);
}
std::expected<JsonObject, StarException> Json::getObject(String const& key) const {
  return convertJson(requireJson(*this, key), &Json::toObject);
}
std::expected<JsonObject, StarException> Json::getObject(String const& key, JsonObject def) const {
  return convertJsonOr(find(key), &Json::toObject, std::move(def), true);
}

std::expected<std::optional<Json>, StarException> Json::opt(String const& key) const {
  auto value = find(key);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value && !(*value)->get().isNull())
    return std::optional<Json>((*value)->get());
  return std::optional<Json>{};
}
std::expected<std::optional<double>, StarException> Json::optDouble(String const& key) const {
  return convertOptionalJson(find(key), &Json::toDouble);
}
std::expected<std::optional<float>, StarException> Json::optFloat(String const& key) const {
  return convertOptionalJson(find(key), &Json::toFloat);
}
std::expected<std::optional<bool>, StarException> Json::optBool(String const& key) const {
  return convertOptionalJson(find(key), &Json::toBool);
}
std::expected<std::optional<std::int64_t>, StarException> Json::optInt(String const& key) const {
  return convertOptionalJson(find(key), &Json::toInt);
}
std::expected<std::optional<std::uint64_t>, StarException> Json::optUInt(String const& key) const {
  return convertOptionalJson(find(key), &Json::toUInt);
}
std::expected<std::optional<String>, StarException> Json::optString(String const& key) const {
  return convertOptionalJson(find(key), &Json::toString);
}
std::expected<std::optional<JsonArray>, StarException> Json::optArray(String const& key) const {
  return convertOptionalJson(find(key), &Json::toArray);
}
std::expected<std::optional<JsonObject>, StarException> Json::optObject(String const& key) const {
  return convertOptionalJson(find(key), &Json::toObject);
}

std::expected<Json, StarException> Json::query(String const& path) const {
  return JsonPath::pathGet(*this, JsonPath::parseQueryPath, path);
}
std::expected<double, StarException> Json::queryDouble(String const& path) const {
  return convertJson(JsonPath::pathGetRef(*this, JsonPath::parseQueryPath, path), &Json::toDouble);
}
std::expected<float, StarException> Json::queryFloat(String const& path) const {
  return convertJson(JsonPath::pathGetRef(*this, JsonPath::parseQueryPath, path), &Json::toFloat);
}
std::expected<bool, StarException> Json::queryBool(String const& path) const {
  return convertJson(JsonPath::pathGetRef(*this, JsonPath::parseQueryPath, path), &Json::toBool);
}
std::expected<std::int64_t, StarException> Json::queryInt(String const& path) const {
  return convertJson(JsonPath::pathGetRef(*this, JsonPath::parseQueryPath, path), &Json::toInt);
}
std::expected<std::uint64_t, StarException> Json::queryUInt(String const& path) const {
  return convertJson(JsonPath::pathGetRef(*this, JsonPath::parseQueryPath, path), &Json::toUInt);
}
std::expected<String, StarException> Json::queryString(String const& path) const {
  return convertJson(JsonPath::pathGetRef(*this, JsonPath::parseQueryPath, path), &Json::toString);
}
std::expected<JsonArray, StarException> Json::queryArray(String const& path) const {
  return convertJson(JsonPath::pathGetRef(*this, JsonPath::parseQueryPath, path), &Json::toArray);
}
std::expected<JsonObject, StarException> Json::queryObject(String const& path) const {
  return convertJson(JsonPath::pathGetRef(*this, JsonPath::parseQueryPath, path), &Json::toObject);
}
std::expected<Json, StarException> Json::query(String const& path, Json def) const {
  auto value = JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return (*value)->get();
  return def;
}
std::expected<double, StarException> Json::queryDouble(String const& path, double def) const {
  return convertJsonOr(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toDouble, def, true);
}
std::expected<float, StarException> Json::queryFloat(String const& path, float def) const {
  return convertJsonOr(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toFloat, def, true);
}
std::expected<bool, StarException> Json::queryBool(String const& path, bool def) const {
  return convertJsonOr(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toBool, def, true);
}
std::expected<std::int64_t, StarException> Json::queryInt(String const& path, std::int64_t def) const {
  return convertJsonOr(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toInt, def, true);
}
std::expected<std::uint64_t, StarException> Json::queryUInt(String const& path, std::uint64_t def) const {
  return convertJsonOr(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toUInt, def, true);
}
std::expected<String, StarException> Json::queryString(String const& path, String const& def) const {
  return convertJsonOr(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toString, def, true);
}
std::expected<JsonArray, StarException> Json::queryArray(String const& path, JsonArray def) const {
  return convertJsonOr(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toArray, std::move(def),
                       true);
}
std::expected<JsonObject, StarException> Json::queryObject(String const& path, JsonObject def) const {
  return convertJsonOr(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toObject, std::move(def),
                       true);
}
std::expected<std::optional<Json>, StarException> Json::optQuery(String const& path) const {
  auto value = JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value && !(*value)->get().isNull())
    return std::optional<Json>((*value)->get());
  return std::optional<Json>{};
}
std::expected<std::optional<double>, StarException> Json::optQueryDouble(String const& path) const {
  return convertOptionalJson(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toDouble);
}
std::expected<std::optional<float>, StarException> Json::optQueryFloat(String const& path) const {
  return convertOptionalJson(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toFloat);
}
std::expected<std::optional<bool>, StarException> Json::optQueryBool(String const& path) const {
  return convertOptionalJson(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toBool);
}
std::expected<std::optional<std::int64_t>, StarException> Json::optQueryInt(String const& path) const {
  return convertOptionalJson(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toInt);
}
std::expected<std::optional<std::uint64_t>, StarException> Json::optQueryUInt(String const& path) const {
  return convertOptionalJson(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toUInt);
}
std::expected<std::optional<String>, StarException> Json::optQueryString(String const& path) const {
  return convertOptionalJson(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toString);
}
std::expected<std::optional<JsonArray>, StarException> Json::optQueryArray(String const& path) const {
  return convertOptionalJson(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toArray);
}
std::expected<std::optional<JsonObject>, StarException> Json::optQueryObject(String const& path) const {
  return convertOptionalJson(JsonPath::pathFindRef(*this, JsonPath::parseQueryPath, path), &Json::toObject);
}

std::expected<Json, StarException> Json::set(String key, Json value) const {
  auto map = toObject();
  if (!map)
    return std::unexpected(std::move(map.error()));
  (*map)[std::move(key)] = std::move(value);
  return Json(std::move(*map));
}

std::expected<Json, StarException> Json::setPath(String path, Json value) const {
  return JsonPath::pathSet(*this, JsonPath::parseQueryPath, path, value);
}

std::expected<Json, StarException> Json::erasePath(String path) const {
  return JsonPath::pathRemove(*this, JsonPath::parseQueryPath, path);
}

std::expected<Json, StarException> Json::setAll(JsonObject values) const {
  auto map = toObject();
  if (!map)
    return std::unexpected(std::move(map.error()));
  for (auto& p : values)
    (*map)[std::move(p.first)] = std::move(p.second);
  return Json(std::move(*map));
}

std::expected<Json, StarException> Json::eraseKey(String key) const {
  auto map = toObject();
  if (!map)
    return std::unexpected(std::move(map.error()));
  map->erase(key);
  return Json(std::move(*map));
}

std::expected<Json, StarException> Json::set(std::size_t index, Json value) const {
  auto array = toArray();
  if (!array)
    return std::unexpected(std::move(array.error()));
  if (index >= array->size())
    return std::unexpected(JsonException::format("Json::set({}) out of range", index));
  (*array)[index] = std::move(value);
  return Json(std::move(*array));
}

std::expected<Json, StarException> Json::insert(std::size_t index, Json value) const {
  auto array = toArray();
  if (!array)
    return std::unexpected(std::move(array.error()));
  if (index > array->size())
    return std::unexpected(JsonException::format("Json::insert({}) out of range", index));
  array->insert(array->begin() + index, std::move(value));
  return Json(std::move(*array));
}

std::expected<Json, StarException> Json::append(Json value) const {
  auto array = toArray();
  if (!array)
    return std::unexpected(std::move(array.error()));
  array->append(std::move(value));
  return Json(std::move(*array));
}

std::expected<Json, StarException> Json::eraseIndex(std::size_t index) const {
  auto array = toArray();
  if (!array)
    return std::unexpected(std::move(array.error()));
  if (index >= array->size())
    return std::unexpected(JsonException::format("Json::eraseIndex({}) out of range", index));
  array->erase(array->begin() + index);
  return Json(std::move(*array));
}

Json::Type Json::type() const {
  return (Type)m_data.index();
}

String Json::typeName() const {
  return typeName(type());
}

std::expected<Json, StarException> Json::convert(Type target) const {
  if (type() == target)
    return *this;
  switch (target) {
    case Type::Null:
      return Json();
    case Type::Float: {
      auto value = toDouble();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    case Type::Bool: {
      auto value = toBool();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    case Type::Int: {
      auto value = toInt();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    case Type::String: {
      auto value = toString();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    case Type::Array: {
      auto value = toArray();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    case Type::Object: {
      auto value = toObject();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Json(std::move(*value));
    }
    default:
      return std::unexpected(JsonException::format("Improper conversion to type {}", typeName(target)));
  }
}

bool Json::isType(Type t) const {
  return type() == t;
}

bool Json::canConvert(Type t) const {
  if (type() == t)
    return true;

  if (t == Type::Null)
    return true;

  if ((type() == Type::Float || type() == Type::Int) && (t == Type::Float || t == Type::Int))
    return true;

  return false;
}

bool Json::isNull() const {
  return type() == Type::Null;
}

Json::operator bool() const {
  return !isNull();
}

std::expected<String, StarException> Json::repr(int pretty, bool sort) const {
  std::string result;
  auto written = outputUtf8Json(*this, JsonUtf8StringOutputIterator(result), pretty, sort);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return String(std::move(result));
}

std::expected<String, StarException> Json::printJson(int pretty, bool sort) const {
  if (type() != Type::Object && type() != Type::Array)
    return std::unexpected(JsonException("printJson called on non-top-level JSON type"));

  return repr(pretty, sort);
}

std::expected<String, StarException> Json::printString() const {
  if (type() == Type::String)
    return *std::get<std::shared_ptr<String const>>(m_data);
  return repr(0, false);
}

std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, Json const& value) {
  auto written = outputUtf8Json(value, std::ostream_iterator<char>(os), 0, false);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return std::ref(os);
}

std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, JsonObject const& value) {
  return os << Json(value);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Json const& value) {
  auto header = ds.write<std::uint8_t>((std::uint8_t)value.type() + 1);
  if (!header)
    return std::unexpected(std::move(header.error()));
  switch (value.type()) {
    case Json::Type::Float: {
      auto converted = value.toDouble();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto written = ds.write<double>(*converted);
      if (!written)
        return std::unexpected(std::move(written.error()));
      break;
    }
    case Json::Type::Bool: {
      auto converted = value.toBool();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto written = ds.write<bool>(*converted);
      if (!written)
        return std::unexpected(std::move(written.error()));
      break;
    }
    case Json::Type::Int: {
      auto converted = value.toInt();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto written = ds.writeVlqI(*converted);
      if (!written)
        return std::unexpected(std::move(written.error()));
      break;
    }
    case Json::Type::String: {
      auto converted = value.stringPtr();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto written = ds.write<String>(**converted);
      if (!written)
        return std::unexpected(std::move(written.error()));
      break;
    }
    case Json::Type::Array: {
      auto converted = value.arrayPtr();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto count = ds.writeVlqU((*converted)->size());
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
      auto converted = value.objectPtr();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      auto count = ds.writeVlqU((*converted)->size());
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
  auto typeIndex = *header;
  if (typeIndex > 0)
    --typeIndex;
  auto type = (Json::Type)typeIndex;
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
      auto item = ds.readVlqI();
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
      auto count = ds.readVlqU();
      if (!count)
        return std::unexpected(std::move(count.error()));
      JsonArray items;
      for (std::uint64_t index = 0; index < *count; ++index) {
        auto item = ds.read<Json>();
        if (!item)
          return std::unexpected(std::move(item.error()));
        items.append(std::move(*item));
      }
      value = Json(std::move(items));
      break;
    }
    case Json::Type::Object: {
      auto count = ds.readVlqU();
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
  auto result = ds.writeContainer(value);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, JsonArray& value) {
  auto result = ds.readContainer(value);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, JsonObject const& value) {
  auto result = ds.writeMapContainer(value);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, JsonObject& value) {
  auto result = ds.readMapContainer(value);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

void Json::getHash(XXHash3& hasher) const {
  Json::Type type = (Json::Type)m_data.index();
  if (type == Json::Type::Float) {
    if (auto integer = exactJsonInteger(std::get<double>(m_data))) {
      // Preserve the old integer domain and bytes, recursively for containers.
      auto integerType = Json::Type::Int;
      hasher.push(std::span<char const>(reinterpret_cast<char const*>(&integerType), sizeof(integerType)));
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
      hasher.push(std::span<char const>(reinterpret_cast<char const*>(&std::get<std::int64_t>(m_data)), sizeof(std::int64_t)));
    else if (type == Json::Type::String) {
      const String& str = *std::get<std::shared_ptr<String const>>(m_data);
      hasher.push(std::span<char const>(str.utf8()));
    }
    else if (type == Json::Type::Array) {
      for (Json const& json : *std::get<std::shared_ptr<JsonArray const>>(m_data))
        json.getHash(hasher);
    }
    else if (type == Json::Type::Object) {
      auto& object = *std::get<std::shared_ptr<JsonObject const>>(m_data);
      List<JsonObject::const_iterator> iterators;
      iterators.reserve(object.size());
      for (auto i = object.begin(); i != object.end(); ++i)
        iterators.append(i);
      iterators.sort([](JsonObject::const_iterator a, JsonObject::const_iterator b) {
        return a->first < b->first;
      });
      for (auto& iter : iterators) {
        hasher.push(std::span<char const>(iter->first.utf8()));
        iter->second.getHash(hasher);
      }
    }
  }
}

std::size_t hash<Json>::operator()(Json const& v) const {
  XXHash3 hasher;
  v.getHash(hasher);
  return hasher.digest();
}

std::expected<std::optional<std::reference_wrapper<Json const>>, StarException> Json::find(std::size_t index) const& {
  if (type() != Type::Array)
    return std::unexpected(JsonException::format("Cannot call get with index on Json type {}, must be Array type", typeName()));

  auto const& list = *std::get<std::shared_ptr<JsonArray const>>(m_data);
  if (index >= list.size())
    return std::nullopt;
  return std::cref(list[index]);
}

std::expected<std::optional<std::reference_wrapper<Json const>>, StarException> Json::find(String const& key) const& {
  if (type() != Type::Object)
    return std::unexpected(JsonException::format("Cannot call get with key on Json type {}, must be Object type", typeName()));
  auto const& map = std::get<std::shared_ptr<JsonObject const>>(m_data);

  return map->maybeRef(key);
}

Json jsonMerge(Json const& base, Json const& merger) {
  if (base.type() == Json::Type::Object && merger.type() == Json::Type::Object) {
    JsonObject merged = *std::get<std::shared_ptr<JsonObject const>>(base.m_data);
    for (auto const& p : *std::get<std::shared_ptr<JsonObject const>>(merger.m_data)) {
      auto res = merged.insert(p);
      if (!res.second)
        res.first->second = jsonMerge(res.first->second, p.second);
    }
    return merged;
  }
  return merger.type() == Json::Type::Null ? base : merger;
}

Json jsonMergeNulling(Json const& base, Json const& merger) {
  if (base.type() == Json::Type::Object && merger.type() == Json::Type::Object) {
    JsonObject merged = *std::get<std::shared_ptr<JsonObject const>>(base.m_data);
    for (auto const& p : *std::get<std::shared_ptr<JsonObject const>>(merger.m_data)) {
      if (p.second.isNull())
        merged.erase(p.first);
      else {
        auto res = merged.insert(p);
        if (!res.second)
          res.first->second = jsonMergeNulling(res.first->second, p.second);
      }
    }
    return merged;
  }
  return merger;
}

bool jsonPartialMatch(Json const& base, Json const& compare) {
  if (base.type() == Json::Type::Object && compare.type() == Json::Type::Object) {
    auto const& object = *std::get<std::shared_ptr<JsonObject const>>(base.m_data);
    auto const& pattern = *std::get<std::shared_ptr<JsonObject const>>(compare.m_data);
    if (&object == &pattern)
      return true;
    for (auto const& [key, value] : pattern) {
      auto entry = object.find(key);
      if (entry == object.end() || !jsonPartialMatch(entry->second, value))
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
        if (jsonPartialMatch(c, b)) {
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