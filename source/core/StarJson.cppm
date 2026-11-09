module;
import std;
import star.hash;
import star.xx_hash;

import star.data_stream;
import star.string;
import star.list;
import star.format;
import star.text_formatter;

import star.algorithm;
namespace Star {

struct JsonExceptionTag {
  static constexpr std::string_view name() { return "JsonException"; }
};
using JsonException = StarError<JsonExceptionTag, StarException>;
struct JsonParsingExceptionTag {
  static constexpr std::string_view name() { return "JsonParsingException"; }
};
using JsonParsingException = StarError<JsonParsingExceptionTag, StarException>;

class Json;

using JsonArray = List<Json>;

using JsonObject = StringMap<Json>;

// Class for holding representation of JSON data.  Immutable and implicitly
// shared.
class Json {
public:
  template <typename Container>
  struct IteratorWrapper {
    // Keep the immutable container alive when iterating a temporary Json.
    using const_iterator = typename Container::const_iterator;
    using iterator = const_iterator;

    const_iterator begin() const;
    const_iterator end() const;

    std::shared_ptr<Container const> ptr;
  };

  enum class Type : std::uint8_t {
    Null = 0,
    Float = 1,
    Bool = 2,
    Int = 3,
    String = 4,
    Array = 5,
    Object = 6
  };

  static String typeName(Type t);
  static std::expected<Type, StarException> typeFromName(String const& t);

  static Json ofType(Type t);

  // Parses JSON or JSON sub-type
  static std::expected<Json, StarException> parse(String const& string);

  // Parses JSON sequence
  static std::expected<Json, StarException> parseSequence(String const& sequence);

  // Parses JSON object or array only (the only top level types allowed by
  // JSON)
  static std::expected<Json, StarException> parseJson(String const& json);

  // Constructs type Null
  Json();

  Json(double);
  // Pointer-to-bool conversions must not steal UTF-8 view construction.
  template <typename Bool> requires std::same_as<std::remove_cvref_t<Bool>, bool>
  Json(Bool b) : m_data(b) {}
  Json(int);
  Json(long);
  Json(long long);
  Json(unsigned int);
  Json(unsigned long);
  Json(unsigned long long);
  Json(std::string_view);
  template <std::size_t Size>
  Json(char const (&text)[Size]) : Json(std::string_view(text, Size - 1)) {}
  static std::expected<Json, StarException> create(std::span<String::Char const>);
  template <std::size_t Size>
  static std::expected<Json, StarException> create(String::Char const (&text)[Size]) {
    return create(std::span<String::Char const>(text, Size - 1));
  }
  Json(String);
  Json(std::string);
  Json(JsonArray);
  Json(JsonObject);

  // Float and Int types are convertible between each other.  toDouble,
  // toFloat, toInt, toUInt may be called on either an Int or a Float.
  // Float-to-integer conversion truncates toward zero and reports non-finite
  // or out-of-range values rather than performing an undefined conversion.
  // Int conversions retain the original C-style numeric conversion semantics.
  //
  // Bools, Strings, Arrays, Objects, and Null are not automatically
  // convertible to any other type.

  std::expected<double, StarException> toDouble() const;
  std::expected<float, StarException> toFloat() const;
  std::expected<bool, StarException> toBool() const;
  std::expected<std::int64_t, StarException> toInt() const;
  std::expected<std::uint64_t, StarException> toUInt() const;
  std::expected<String, StarException> toString() const;
  std::expected<JsonArray, StarException> toArray() const;
  std::expected<JsonObject, StarException> toObject() const;

  // Internally, String, JsonArray, and JsonObject are shared via shared_ptr
  // since this class is immutable.  Use these methods to get at this pointer
  // without causing a copy.
  std::expected<std::shared_ptr<String const>, StarException> stringPtr() const;
  std::expected<std::shared_ptr<JsonArray const>, StarException> arrayPtr() const;
  std::expected<std::shared_ptr<JsonObject const>, StarException> objectPtr() const;

  // As a convenience, make it easy to safely and quickly iterate over a
  // JsonArray or JsonObject contents by holding the container pointer.
  std::expected<IteratorWrapper<JsonArray>, StarException> iterateArray() const;
  std::expected<IteratorWrapper<JsonObject>, StarException> iterateObject() const;

  // opt* methods work like this, if the json is null, it returns none.  If the
  // json is convertible, it returns the converted type, otherwise an error.
  std::optional<Json> opt() const;
  std::expected<std::optional<double>, StarException> optDouble() const;
  std::expected<std::optional<float>, StarException> optFloat() const;
  std::expected<std::optional<bool>, StarException> optBool() const;
  std::expected<std::optional<std::int64_t>, StarException> optInt() const;
  std::expected<std::optional<std::uint64_t>, StarException> optUInt() const;
  std::expected<std::optional<String>, StarException> optString() const;
  std::expected<std::optional<JsonArray>, StarException> optArray() const;
  std::expected<std::optional<JsonObject>, StarException> optObject() const;

  // Size of array / object type json
  std::expected<std::size_t, StarException> size() const;

  // Synchronous borrows from this immutable owner. Missing entries are empty;
  // present JSON null remains an engaged reference. Wrong container types fail.
  // Keep the owner alive and unchanged for the entire lifetime of the result.
  std::expected<std::optional<std::reference_wrapper<Json const>>, StarException> find(std::size_t index) const&;
  std::expected<std::optional<std::reference_wrapper<Json const>>, StarException> find(String const& key) const&;
  std::expected<std::optional<std::reference_wrapper<Json const>>, StarException>
  find(std::size_t index) const&& = delete;
  std::expected<std::optional<std::reference_wrapper<Json const>>, StarException>
  find(String const& key) const&& = delete;

  // If this json is array type, get the value at the given index
  std::expected<Json, StarException> get(std::size_t index) const;
  std::expected<double, StarException> getDouble(std::size_t index) const;
  std::expected<float, StarException> getFloat(std::size_t index) const;
  std::expected<bool, StarException> getBool(std::size_t index) const;
  std::expected<std::int64_t, StarException> getInt(std::size_t index) const;
  std::expected<std::uint64_t, StarException> getUInt(std::size_t index) const;
  std::expected<String, StarException> getString(std::size_t index) const;
  std::expected<JsonArray, StarException> getArray(std::size_t index) const;
  std::expected<JsonObject, StarException> getObject(std::size_t index) const;

  // Indexed defaults apply only out of range. Present null is returned by get;
  // typed getters attempt conversion and report the ordinary null-type error.
  std::expected<Json, StarException> get(std::size_t index, Json def) const;
  std::expected<double, StarException> getDouble(std::size_t index, double def) const;
  std::expected<float, StarException> getFloat(std::size_t index, float def) const;
  std::expected<bool, StarException> getBool(std::size_t index, bool def) const;
  std::expected<std::int64_t, StarException> getInt(std::size_t index, std::int64_t def) const;
  std::expected<std::uint64_t, StarException> getUInt(std::size_t index, std::int64_t def) const;
  std::expected<String, StarException> getString(std::size_t index, String def) const;
  std::expected<JsonArray, StarException> getArray(std::size_t index, JsonArray def) const;
  std::expected<JsonObject, StarException> getObject(std::size_t index, JsonObject def) const;

  // If object type, whether object contains key
  std::expected<bool, StarException> contains(String const& key) const;

  // If this json is object type, get the value for the given key
  std::expected<Json, StarException> get(String const& key) const;
  std::expected<double, StarException> getDouble(String const& key) const;
  std::expected<float, StarException> getFloat(String const& key) const;
  std::expected<bool, StarException> getBool(String const& key) const;
  std::expected<std::int64_t, StarException> getInt(String const& key) const;
  std::expected<std::uint64_t, StarException> getUInt(String const& key) const;
  std::expected<String, StarException> getString(String const& key) const;
  std::expected<JsonArray, StarException> getArray(String const& key) const;
  std::expected<JsonObject, StarException> getObject(String const& key) const;

  // Typed keyed getters return the default if missing or null. Untyped get
  // returns a present null unchanged and uses the default only when missing.
  std::expected<Json, StarException> get(String const& key, Json def) const;
  std::expected<double, StarException> getDouble(String const& key, double def) const;
  std::expected<float, StarException> getFloat(String const& key, float def) const;
  std::expected<bool, StarException> getBool(String const& key, bool def) const;
  std::expected<std::int64_t, StarException> getInt(String const& key, std::int64_t def) const;
  std::expected<std::uint64_t, StarException> getUInt(String const& key, std::int64_t def) const;
  std::expected<String, StarException> getString(String const& key, String def) const;
  std::expected<JsonArray, StarException> getArray(String const& key, JsonArray def) const;
  std::expected<JsonObject, StarException> getObject(String const& key, JsonObject def) const;

  // Works the same way as opt methods above.  Will never return a null value,
  // if there is a null entry it will just return an empty std::optional.
  std::expected<std::optional<Json>, StarException> opt(String const& key) const;
  std::expected<std::optional<double>, StarException> optDouble(String const& key) const;
  std::expected<std::optional<float>, StarException> optFloat(String const& key) const;
  std::expected<std::optional<bool>, StarException> optBool(String const& key) const;
  std::expected<std::optional<std::int64_t>, StarException> optInt(String const& key) const;
  std::expected<std::optional<std::uint64_t>, StarException> optUInt(String const& key) const;
  std::expected<std::optional<String>, StarException> optString(String const& key) const;
  std::expected<std::optional<JsonArray>, StarException> optArray(String const& key) const;
  std::expected<std::optional<JsonObject>, StarException> optObject(String const& key) const;

  // Combines gets recursively in friendly expressions.  For
  // example, call like this: json.query("path.to.array[3][4]")
  std::expected<Json, StarException> query(String const& path) const;
  std::expected<double, StarException> queryDouble(String const& path) const;
  std::expected<float, StarException> queryFloat(String const& path) const;
  std::expected<bool, StarException> queryBool(String const& path) const;
  std::expected<std::int64_t, StarException> queryInt(String const& path) const;
  std::expected<std::uint64_t, StarException> queryUInt(String const& path) const;
  std::expected<String, StarException> queryString(String const& path) const;
  std::expected<JsonArray, StarException> queryArray(String const& path) const;
  std::expected<JsonObject, StarException> queryObject(String const& path) const;

  // Defaults apply on missing/nontraversable paths. Typed queries also default
  // on a terminal null; untyped query returns that present null unchanged.
  std::expected<Json, StarException> query(String const& path, Json def) const;
  std::expected<double, StarException> queryDouble(String const& path, double def) const;
  std::expected<float, StarException> queryFloat(String const& path, float def) const;
  std::expected<bool, StarException> queryBool(String const& path, bool def) const;
  std::expected<std::int64_t, StarException> queryInt(String const& path, std::int64_t def) const;
  std::expected<std::uint64_t, StarException> queryUInt(String const& path, std::uint64_t def) const;
  std::expected<String, StarException> queryString(String const& path, String const& def) const;
  std::expected<JsonArray, StarException> queryArray(String const& path, JsonArray def) const;
  std::expected<JsonObject, StarException> queryObject(String const& path, JsonObject def) const;

  // Missing/nontraversable paths and terminal null produce an empty optional;
  // malformed visited path components and terminal conversion errors fail.
  std::expected<std::optional<Json>, StarException> optQuery(String const& path) const;
  std::expected<std::optional<double>, StarException> optQueryDouble(String const& path) const;
  std::expected<std::optional<float>, StarException> optQueryFloat(String const& path) const;
  std::expected<std::optional<bool>, StarException> optQueryBool(String const& path) const;
  std::expected<std::optional<std::int64_t>, StarException> optQueryInt(String const& path) const;
  std::expected<std::optional<std::uint64_t>, StarException> optQueryUInt(String const& path) const;
  std::expected<std::optional<String>, StarException> optQueryString(String const& path) const;
  std::expected<std::optional<JsonArray>, StarException> optQueryArray(String const& path) const;
  std::expected<std::optional<JsonObject>, StarException> optQueryObject(String const& path) const;

  // Returns a *new* object with the given values set/erased.  Errors if not an
  // object.
  std::expected<Json, StarException> set(String key, Json value) const;
  std::expected<Json, StarException> setPath(String path, Json value) const;
  std::expected<Json, StarException> setAll(JsonObject values) const;
  std::expected<Json, StarException> eraseKey(String key) const;
  std::expected<Json, StarException> erasePath(String path) const;

  // Returns a *new* array with the given values set/inserted/appended/erased.
  // Errors if not an array.
  std::expected<Json, StarException> set(std::size_t index, Json value) const;
  std::expected<Json, StarException> insert(std::size_t index, Json value) const;
  std::expected<Json, StarException> append(Json value) const;
  std::expected<Json, StarException> eraseIndex(std::size_t index) const;

  Type type() const;
  String typeName() const;
  std::expected<Json, StarException> convert(Type u) const;

  bool isType(Type type) const;
  bool canConvert(Type type) const;

  // isNull returns true when the type of the Json is null.  operator bool() is
  // the opposite of isNull().
  bool isNull() const;
  explicit operator bool() const;

  // Prints JSON or JSON sub-type.  If sort is true, then any object anywhere
  // inside this value will be sorted alphanumerically before being written,
  // resulting in a known *unique* textual representation of the Json that is
  // cross-platform.
  std::expected<String, StarException> repr(int pretty = 0, bool sort = false) const;
  // Prints JSON object or array only (only top level types allowed by JSON)
  std::expected<String, StarException> printJson(int pretty = 0, bool sort = false) const;
  // Same but avoids quotation marks if this is a string
  std::expected<String, StarException> printString() const;

  // operator== and operator!= compare for exact equality with all types, and
  // additionally equality with numeric conversion with Int <-> Float
  bool operator==(Json const& v) const;
  bool operator!=(Json const& v) const;

  // Does this Json not share its storage with any other Json?
  bool unique() const;

  // Numeric hashes follow equality: integral floats use the exact int64 hash,
  // including signed zero. Nonintegral floats retain their native hash bytes.
  void getHash(XXHash3& hasher) const;

private:
  friend Json jsonMerge(Json const& base, Json const& merger);
  friend Json jsonMergeNulling(Json const& base, Json const& merger);
  friend bool jsonPartialMatch(Json const& base, Json const& compare);


  std::variant<std::monostate, double, bool, std::int64_t, std::shared_ptr<String const>, std::shared_ptr<JsonArray const>, std::shared_ptr<JsonObject const>> m_data;
};

[[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, Json const& value);

// Keep JSON object serialization distinct from the generic map diagnostic.
[[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, JsonObject const& value);

// Serialize json to DataStream.  Strings are stored as UTF-8, ints are stored
// as VLQ, doubles as 64 bit.
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Json const& v);
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Json& v);

// Convenience methods for Json containers
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, JsonArray const& l);
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, JsonArray& l);
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, JsonObject const& m);
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, JsonObject& m);

// Merges the two given json values and returns the result, by the following
// rules (applied in order):  If the base value is null, returns the merger.
// If the merger value is null, returns base.  For any two non-objects types,
// returns the merger.  If both values are objects, then the resulting object
// is the combination of both objects, but for each repeated key jsonMerge is
// called recursively on both values to determine the result.
Json jsonMerge(Json const& base, Json const& merger);
// Same as above, but applies null mergers.
Json jsonMergeNulling(Json const& base, Json const& merger);

template <typename... T>
Json jsonMerge(Json const& base, Json const& merger, T const&... rest);

// Similar to jsonMerge, but query only for a single key.  Gets a value equal
// to jsonMerge(jsons...).query(key, Json()), but much faster than doing an
// entire merge operation.
template <typename... T>
std::expected<Json, StarException> jsonMergeQuery(String const& key, Json const& first, T const&... rest);

// jsonMergeQuery with a default.
template <typename... T>
std::expected<Json, StarException> jsonMergeQueryDef(String const& key, Json def, Json const& first, T const&... rest);

template <>
struct hash<Json> {
  std::size_t operator()(Json const& v) const;
};

template <typename Container>
auto Json::IteratorWrapper<Container>::begin() const -> const_iterator {
  return ptr->begin();
}

template <typename Container>
auto Json::IteratorWrapper<Container>::end() const -> const_iterator {
  return ptr->end();
}

template <typename... T>
Json jsonMerge(Json const& base, Json const& merger, T const&... rest) {
  return jsonMerge(jsonMerge(base, merger), rest...);
}

template <typename... T>
std::expected<Json, StarException> jsonMergeQuery(String const&, Json def) {
  return def;
}

template <typename... T>
std::expected<Json, StarException> jsonMergeQueryImpl(String const& key, Json const& json) {
  return json.query(key, {});
}

template <typename... T>
std::expected<Json, StarException> jsonMergeQueryImpl(String const& key, Json const& base, Json const& first, T const&... rest) {
  auto value = jsonMergeQueryImpl(key, first, rest...);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value && !value->isType(Json::Type::Object))
    return std::move(*value);
  auto baseValue = base.query(key, {});
  if (!baseValue)
    return std::unexpected(std::move(baseValue.error()));
  return jsonMerge(*baseValue, *value);
}

template <typename... T>
std::expected<Json, StarException> jsonMergeQuery(String const& key, Json const& first, T const&... rest) {
  return jsonMergeQueryImpl(key, first, rest...);
}

template <typename... T>
std::expected<Json, StarException> jsonMergeQueryDef(String const& key, Json def, Json const& first, T const&... rest) {
  auto value = jsonMergeQueryImpl(key, first, rest...);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return std::move(*value);
  return def;
}

// Compares two JSON values to see if the second is a subset of the first.
// For objects, each key in the second object must exist in the first
// object and the values are recursively compared the same way. For arrays,
// each element in the second array must successfully compare with some
// element of the first array, regardless of order or duplication.
// For all other types, the values must be equal.
bool jsonPartialMatch(Json const& base, Json const& compare);

}

export module star.json;

export namespace Star {
  template <>
  struct hash<Json>;
}


export namespace Star {
  using ::Star::JsonExceptionTag;
  using ::Star::JsonException;
  using ::Star::JsonParsingExceptionTag;
  using ::Star::JsonParsingException;
  using ::Star::Json;
  using ::Star::JsonArray;
  using ::Star::JsonObject;
  using ::Star::jsonMerge;
  using ::Star::jsonMergeNulling;
  using ::Star::jsonMergeQuery;
  using ::Star::jsonMergeQueryImpl;
  using ::Star::jsonMergeQueryDef;
  using ::Star::jsonPartialMatch;
  using ::Star::hash;
  using ::Star::write_text;
  using ::Star::operator<<;
  using ::Star::operator>>;
}

