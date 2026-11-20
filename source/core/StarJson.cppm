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

// Keep constraints independent of container internals while Json is recursive.
template <typename Container>
concept json_container = std::same_as<Container, JsonArray> || std::same_as<Container, JsonObject>;

template <typename Value>
concept json_boolean = std::same_as<std::remove_cvref_t<Value>, bool>;

// Class for holding representation of JSON data.  Immutable and implicitly
// shared.
class Json {
public:
  template <json_container Container> struct IteratorWrapper {
    // Keep the immutable container alive when iterating a temporary Json.
    using const_iterator = typename Container::const_iterator;
    using iterator = const_iterator;

    const_iterator begin() const;
    const_iterator end() const;

    std::shared_ptr<Container const> m_container;
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

  static String type_name(Type t);
  static std::expected<Type, StarException> type_from_name(String const& t);

  static Json of_type(Type t);

  // Parses JSON or JSON sub-type
  static std::expected<Json, StarException> parse(String const& string);

  // Parses JSON sequence
  static std::expected<Json, StarException> parse_sequence(String const& sequence);

  // Parses JSON object or array only (the only top level types allowed by
  // JSON)
  static std::expected<Json, StarException> parse_json(String const& json);

  // Constructs type Null
  Json();

  Json(double);
  // Pointer-to-bool conversions must not steal UTF-8 view construction.
  template <json_boolean Bool> Json(Bool b) : m_data(b) {}
  Json(int);
  Json(long);
  Json(long long);
  Json(unsigned int);
  Json(unsigned long);
  Json(unsigned long long);
  Json(std::string_view);
  template <std::size_t size>
    requires(size > 0)
  Json(char const (&text)[size]) : Json(std::string_view(text, size - 1)) {}
  static std::expected<Json, StarException> create(std::span<String::Char const>);
  template <std::size_t size>
    requires(size > 0)
  static std::expected<Json, StarException> create(String::Char const (&text)[size]) {
    return create(std::span<String::Char const>(text, size - 1));
  }
  Json(String);
  Json(std::string);
  Json(JsonArray);
  Json(JsonObject);

  // Float and Int types are convertible between each other.  to_double,
  // to_float, to_int, to_uint may be called on either an Int or a Float.
  // Float-to-integer conversion truncates toward zero and reports non-finite
  // or out-of-range values rather than performing an undefined conversion.
  // Int conversions retain the original C-style numeric conversion semantics.
  //
  // Bools, Strings, Arrays, Objects, and Null are not automatically
  // convertible to any other type.

  std::expected<double, StarException> to_double() const;
  std::expected<float, StarException> to_float() const;
  std::expected<bool, StarException> to_bool() const;
  std::expected<std::int64_t, StarException> to_int() const;
  std::expected<std::uint64_t, StarException> to_uint() const;
  std::expected<String, StarException> to_string() const;
  std::expected<JsonArray, StarException> to_array() const;
  std::expected<JsonObject, StarException> to_object() const;

  // Internally, String, JsonArray, and JsonObject are shared via shared_ptr
  // since this class is immutable.  Use these methods to get at this pointer
  // without causing a copy.
  std::expected<std::shared_ptr<String const>, StarException> string_ptr() const;
  std::expected<std::shared_ptr<JsonArray const>, StarException> array_ptr() const;
  std::expected<std::shared_ptr<JsonObject const>, StarException> object_ptr() const;

  // As a convenience, make it easy to safely and quickly iterate over a
  // JsonArray or JsonObject contents by holding the container pointer.
  std::expected<IteratorWrapper<JsonArray>, StarException> iterate_array() const;
  std::expected<IteratorWrapper<JsonObject>, StarException> iterate_object() const;

  // opt* methods work like this, if the json is null, it returns none.  If the
  // json is convertible, it returns the converted type, otherwise an error.
  std::optional<Json> opt() const;
  std::expected<std::optional<double>, StarException> opt_double() const;
  std::expected<std::optional<float>, StarException> opt_float() const;
  std::expected<std::optional<bool>, StarException> opt_bool() const;
  std::expected<std::optional<std::int64_t>, StarException> opt_int() const;
  std::expected<std::optional<std::uint64_t>, StarException> opt_uint() const;
  std::expected<std::optional<String>, StarException> opt_string() const;
  std::expected<std::optional<JsonArray>, StarException> opt_array() const;
  std::expected<std::optional<JsonObject>, StarException> opt_object() const;

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
  std::expected<double, StarException> get_double(std::size_t index) const;
  std::expected<float, StarException> get_float(std::size_t index) const;
  std::expected<bool, StarException> get_bool(std::size_t index) const;
  std::expected<std::int64_t, StarException> get_int(std::size_t index) const;
  std::expected<std::uint64_t, StarException> get_uint(std::size_t index) const;
  std::expected<String, StarException> get_string(std::size_t index) const;
  std::expected<JsonArray, StarException> get_array(std::size_t index) const;
  std::expected<JsonObject, StarException> get_object(std::size_t index) const;

  // Indexed defaults apply only out of range. Present null is returned by get;
  // typed getters attempt conversion and report the ordinary null-type error.
  std::expected<Json, StarException> get(std::size_t index, Json def) const;
  std::expected<double, StarException> get_double(std::size_t index, double def) const;
  std::expected<float, StarException> get_float(std::size_t index, float def) const;
  std::expected<bool, StarException> get_bool(std::size_t index, bool def) const;
  std::expected<std::int64_t, StarException> get_int(std::size_t index, std::int64_t def) const;
  std::expected<std::uint64_t, StarException> get_uint(std::size_t index, std::int64_t def) const;
  std::expected<String, StarException> get_string(std::size_t index, String def) const;
  std::expected<JsonArray, StarException> get_array(std::size_t index, JsonArray def) const;
  std::expected<JsonObject, StarException> get_object(std::size_t index, JsonObject def) const;

  // If object type, whether object contains key
  std::expected<bool, StarException> contains(String const& key) const;

  // If this json is object type, get the value for the given key
  std::expected<Json, StarException> get(String const& key) const;
  std::expected<double, StarException> get_double(String const& key) const;
  std::expected<float, StarException> get_float(String const& key) const;
  std::expected<bool, StarException> get_bool(String const& key) const;
  std::expected<std::int64_t, StarException> get_int(String const& key) const;
  std::expected<std::uint64_t, StarException> get_uint(String const& key) const;
  std::expected<String, StarException> get_string(String const& key) const;
  std::expected<JsonArray, StarException> get_array(String const& key) const;
  std::expected<JsonObject, StarException> get_object(String const& key) const;

  // Typed keyed getters return the default if missing or null. Untyped get
  // returns a present null unchanged and uses the default only when missing.
  std::expected<Json, StarException> get(String const& key, Json def) const;
  std::expected<double, StarException> get_double(String const& key, double def) const;
  std::expected<float, StarException> get_float(String const& key, float def) const;
  std::expected<bool, StarException> get_bool(String const& key, bool def) const;
  std::expected<std::int64_t, StarException> get_int(String const& key, std::int64_t def) const;
  std::expected<std::uint64_t, StarException> get_uint(String const& key, std::int64_t def) const;
  std::expected<String, StarException> get_string(String const& key, String def) const;
  std::expected<JsonArray, StarException> get_array(String const& key, JsonArray def) const;
  std::expected<JsonObject, StarException> get_object(String const& key, JsonObject def) const;

  // Works the same way as opt methods above.  Will never return a null value,
  // if there is a null entry it will just return an empty std::optional.
  std::expected<std::optional<Json>, StarException> opt(String const& key) const;
  std::expected<std::optional<double>, StarException> opt_double(String const& key) const;
  std::expected<std::optional<float>, StarException> opt_float(String const& key) const;
  std::expected<std::optional<bool>, StarException> opt_bool(String const& key) const;
  std::expected<std::optional<std::int64_t>, StarException> opt_int(String const& key) const;
  std::expected<std::optional<std::uint64_t>, StarException> opt_uint(String const& key) const;
  std::expected<std::optional<String>, StarException> opt_string(String const& key) const;
  std::expected<std::optional<JsonArray>, StarException> opt_array(String const& key) const;
  std::expected<std::optional<JsonObject>, StarException> opt_object(String const& key) const;

  // Combines gets recursively in friendly expressions.  For
  // example, call like this: json.query("path.to.array[3][4]")
  std::expected<Json, StarException> query(String const& path) const;
  std::expected<double, StarException> query_double(String const& path) const;
  std::expected<float, StarException> query_float(String const& path) const;
  std::expected<bool, StarException> query_bool(String const& path) const;
  std::expected<std::int64_t, StarException> query_int(String const& path) const;
  std::expected<std::uint64_t, StarException> query_uint(String const& path) const;
  std::expected<String, StarException> query_string(String const& path) const;
  std::expected<JsonArray, StarException> query_array(String const& path) const;
  std::expected<JsonObject, StarException> query_object(String const& path) const;

  // Defaults apply on missing/nontraversable paths. Typed queries also default
  // on a terminal null; untyped query returns that present null unchanged.
  std::expected<Json, StarException> query(String const& path, Json def) const;
  std::expected<double, StarException> query_double(String const& path, double def) const;
  std::expected<float, StarException> query_float(String const& path, float def) const;
  std::expected<bool, StarException> query_bool(String const& path, bool def) const;
  std::expected<std::int64_t, StarException> query_int(String const& path, std::int64_t def) const;
  std::expected<std::uint64_t, StarException> query_uint(String const& path, std::uint64_t def) const;
  std::expected<String, StarException> query_string(String const& path, String const& def) const;
  std::expected<JsonArray, StarException> query_array(String const& path, JsonArray def) const;
  std::expected<JsonObject, StarException> query_object(String const& path, JsonObject def) const;

  // Missing/nontraversable paths and terminal null produce an empty optional;
  // malformed visited path components and terminal conversion errors fail.
  std::expected<std::optional<Json>, StarException> opt_query(String const& path) const;
  std::expected<std::optional<double>, StarException> opt_query_double(String const& path) const;
  std::expected<std::optional<float>, StarException> opt_query_float(String const& path) const;
  std::expected<std::optional<bool>, StarException> opt_query_bool(String const& path) const;
  std::expected<std::optional<std::int64_t>, StarException> opt_query_int(String const& path) const;
  std::expected<std::optional<std::uint64_t>, StarException> opt_query_uint(String const& path) const;
  std::expected<std::optional<String>, StarException> opt_query_string(String const& path) const;
  std::expected<std::optional<JsonArray>, StarException> opt_query_array(String const& path) const;
  std::expected<std::optional<JsonObject>, StarException> opt_query_object(String const& path) const;

  // Returns a *new* object with the given values set/erased.  Errors if not an
  // object.
  std::expected<Json, StarException> set(String key, Json value) const;
  std::expected<Json, StarException> set_path(String path, Json value) const;
  std::expected<Json, StarException> set_all(JsonObject values) const;
  std::expected<Json, StarException> erase(String key) const;
  std::expected<Json, StarException> erase_path(String path) const;

  // Returns a *new* array with the given values set/inserted/appended/erased.
  // Errors if not an array.
  std::expected<Json, StarException> set(std::size_t index, Json value) const;
  std::expected<Json, StarException> insert(std::size_t index, Json value) const;
  std::expected<Json, StarException> append(Json value) const;
  std::expected<Json, StarException> erase(std::size_t index) const;

  Type type() const;
  String type_name() const;
  std::expected<Json, StarException> convert(Type u) const;

  bool is_type(Type type) const;
  bool can_convert(Type type) const;

  // is_null returns true when the type of the Json is null.  operator bool() is
  // the opposite of is_null().
  bool is_null() const;
  explicit operator bool() const;

  // Prints JSON or JSON sub-type.  If sort is true, then any object anywhere
  // inside this value will be sorted alphanumerically before being written,
  // resulting in a known *unique* textual representation of the Json that is
  // cross-platform.
  std::expected<String, StarException> repr(int pretty = 0, bool sort = false) const;
  // Prints JSON object or array only (only top level types allowed by JSON)
  std::expected<String, StarException> print_json(int pretty = 0, bool sort = false) const;
  // Same but avoids quotation marks if this is a string
  std::expected<String, StarException> print_string() const;

  // operator== and operator!= compare for exact equality with all types, and
  // additionally equality with numeric conversion with Int <-> Float
  bool operator==(Json const& v) const;
  bool operator!=(Json const& v) const;

  // Does this Json not share its storage with any other Json?
  bool unique() const;

  // Numeric hashes follow equality: integral floats use the exact int64 hash,
  // including signed zero. Nonintegral floats retain their native hash bytes.
  void get_hash(XXHash3& hasher) const;

private:
  friend Json json_merge(Json const& base, Json const& merger);
  friend Json json_merge_nulling(Json const& base, Json const& merger);
  friend bool json_partial_match(Json const& base, Json const& compare);

  std::expected<std::reference_wrapper<JsonArray const>, StarException> array_ref() const&;

  std::variant<std::monostate, double, bool, std::int64_t, std::shared_ptr<String const>,
               std::shared_ptr<JsonArray const>, std::shared_ptr<JsonObject const>>
    m_data;
};

template <typename Value>
concept json_source = std::convertible_to<Value const&, Json>;

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
// is the combination of both objects, but for each repeated key json_merge is
// called recursively on both values to determine the result.
Json json_merge(Json const& base, Json const& merger);
// Same as above, but applies null mergers.
Json json_merge_nulling(Json const& base, Json const& merger);

template <json_source... T> Json json_merge(Json const& base, Json const& merger, T const&... rest);

// Similar to json_merge, but query only for a single key.  Gets a value equal
// to json_merge(jsons...).query(key, Json()), but much faster than doing an
// entire merge operation.
template <json_source... T>
std::expected<Json, StarException> json_merge_query(String const& key, Json const& first, T const&... rest);

// json_merge_query with a default.
template <json_source... T>
std::expected<Json, StarException> json_merge_query_def(String const& key, Json def, Json const& first, T const&... rest);

template <>
struct hash<Json> {
  std::size_t operator()(Json const& v) const;
};

template <json_container Container> auto Json::IteratorWrapper<Container>::begin() const -> const_iterator {
  return m_container->begin();
}

template <json_container Container> auto Json::IteratorWrapper<Container>::end() const -> const_iterator {
  return m_container->end();
}

template <json_source... T> Json json_merge(Json const& base, Json const& merger, T const&... rest) {
  return json_merge(json_merge(base, merger), rest...);
}

template <json_source... T> std::expected<Json, StarException> json_merge_query(String const&, Json def) { return def; }

inline std::expected<Json, StarException> json_merge_query_impl(String const& key, Json const& json) {
  return json.query(key, {});
}

template <json_source... T>
std::expected<Json, StarException> json_merge_query_impl(String const& key, Json const& base, Json const& first,
                                                         T const&... rest) {
  auto value = json_merge_query_impl(key, first, rest...);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value && !value->is_type(Json::Type::Object))
    return std::move(*value);
  auto base_value = base.query(key, {});
  if (!base_value)
    return std::unexpected(std::move(base_value.error()));
  return json_merge(*base_value, *value);
}

template <json_source... T>
std::expected<Json, StarException> json_merge_query(String const& key, Json const& first, T const&... rest) {
  return json_merge_query_impl(key, first, rest...);
}

template <json_source... T>
std::expected<Json, StarException> json_merge_query_def(String const& key, Json def, Json const& first, T const&... rest) {
  auto value = json_merge_query_impl(key, first, rest...);
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
bool json_partial_match(Json const& base, Json const& compare);

}

export module star.json;

export namespace Star {
  template <>
  struct hash<Json>;
}


export namespace Star {
using ::Star::hash;
using ::Star::Json;
using ::Star::json_boolean;
using ::Star::json_container;
using ::Star::json_source;
using ::Star::JsonArray;
using ::Star::JsonException;
using ::Star::JsonExceptionTag;
using ::Star::json_merge;
using ::Star::json_merge_nulling;
using ::Star::json_merge_query;
using ::Star::json_merge_query_def;
using ::Star::JsonObject;
using ::Star::JsonParsingException;
using ::Star::JsonParsingExceptionTag;
using ::Star::json_partial_match;
using ::Star::write_text;
using ::Star::operator<<;
using ::Star::operator>>;
}

