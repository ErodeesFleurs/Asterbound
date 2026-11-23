module;
import std;
import star.string;
import star.format;
import star.json;

import star.poly;
import star.color;
import star.set;
import star.list;
import star.rect;

import star.array;
import star.vector;
import star.matrix3;
import star.line;
import star.algorithm;
import star.weighted_pool;
import star.random;
import star.directives;
namespace Star {


// Extra checked conversions out of pure JSON.
std::expected<std::size_t, StarException> json_to_size(Json const& v);
Json json_from_size(std::size_t s);

// Must be array of appropriate size.

std::expected<Vec2D, StarException> json_to_vec2d(Json const& v);
std::expected<Vec2F, StarException> json_to_vec2f(Json const& v);
Json json_from_vec2f(Vec2F const& v);
std::expected<Vec2I, StarException> json_to_vec2i(Json const& v);
Json json_from_vec2i(Vec2I const& v);
std::expected<Vec2U, StarException> json_to_vec2u(Json const& v);
Json json_from_vec2u(Vec2U const& v);
std::expected<Vec2B, StarException> json_to_vec2b(Json const& v);
Json json_from_vec2b(Vec2B const& v);

std::expected<Vec3D, StarException> json_to_vec3d(Json const& v);
std::expected<Vec3F, StarException> json_to_vec3f(Json const& v);
Json json_from_vec3f(Vec3F const& v);
std::expected<Vec3I, StarException> json_to_vec3i(Json const& v);
Json json_from_vec3i(Vec3I const& v);
std::expected<Vec3B, StarException> json_to_vec3b(Json const& v);

std::expected<Vec4B, StarException> json_to_vec4b(Json const& v);
std::expected<Vec4I, StarException> json_to_vec4i(Json const& v);
std::expected<Vec4F, StarException> json_to_vec4f(Json const& v);

// Must be array of size 4 or 2 arrays of size 2 in an array.
std::expected<RectD, StarException> json_to_rectd(Json const& v);
Json json_from_rectd(RectD const& rect);
std::expected<RectF, StarException> json_to_rectf(Json const& v);
Json json_from_rectf(RectF const& rect);
std::expected<RectI, StarException> json_to_recti(Json const& v);
Json json_from_recti(RectI const& rect);
std::expected<RectU, StarException> json_to_rectu(Json const& v);
Json json_from_rectu(RectU const& rect);

// A named/hex string or an RGB[A] array. Numeric channels convert through
// get_int (fractional values truncate), then narrow to bytes; absent alpha is 255.
std::expected<Color, StarException> json_to_color(Json const& v);
Json json_from_color(Color const& color);

// HACK: Fix clockwise specified polygons in coming from JSON
template <typename Float>
Polygon<Float> fix_inside_out_poly(Polygon<Float> p);

// Array of size 2 arrays
std::expected<PolyD, StarException> json_to_polyd(Json const& v);
std::expected<PolyF, StarException> json_to_polyf(Json const& v);
std::expected<PolyI, StarException> json_to_polyi(Json const& v);
Json json_from_polyf(PolyF const& poly);

// Expects a size 2 array of size 2 arrays
std::expected<Line2F, StarException> json_to_line2f(Json const& v);
Json json_from_line2f(Line2F const& line);

std::expected<Mat3F, StarException> json_to_mat3f(Json const& v);
Json json_from_mat3f(Mat3F const& v);

std::expected<StringList, StarException> json_to_string_list(Json const& v);
Json json_from_string_list(List<String> const& v);
std::expected<StringSet, StarException> json_to_string_set(Json const& v);
Json json_from_string_set(StringSet const& v);
std::expected<List<float>, StarException> json_to_float_list(Json const& v);
std::expected<List<int>, StarException> json_to_int_list(Json const& v);
std::expected<List<Vec2I>, StarException> json_to_vec2i_list(Json const& v);
std::expected<List<Vec2U>, StarException> json_to_vec2u_list(Json const& v);
std::expected<List<Vec2F>, StarException> json_to_vec2f_list(Json const& v);
std::expected<List<Vec4B>, StarException> json_to_vec4b_list(Json const& v);
std::expected<List<Color>, StarException> json_to_color_list(Json const& v);
std::expected<List<Directives>, StarException> json_to_directives_list(Json const& v);
Json json_from_directives_list(List<Directives> const& v);

std::expected<Json, StarException> weighted_choice_from_json(RandomService& random, Json const& source, Json const& default_);

// Assumes that the bins parameter is an array of pairs (arrays), where the
// first element is a minimum value and the second element is the actual
// important value.  Finds the pair with the highest value that is less than or
// equal to the given target, and returns the second element.
std::expected<Json, StarException> binned_choice_from_json(Json const& bins, float target, Json const& def = Json());

namespace json_extra_detail {
  template <typename Result>
  concept checked_converter_result = requires {
    typename std::remove_cvref_t<Result>::value_type;
    typename std::remove_cvref_t<Result>::error_type;
    requires std::same_as<std::remove_cvref_t<Result>,
        std::expected<typename std::remove_cvref_t<Result>::value_type,
                      typename std::remove_cvref_t<Result>::error_type>>;
  };

  template <typename T, typename Result>
  concept converter_result = !std::is_void_v<Result>
    && (!std::is_reference_v<Result> || std::is_constructible_v<std::remove_cvref_t<Result>, Result>)
    && ((checked_converter_result<Result>
         && requires(std::remove_cvref_t<Result>& converted) {
              std::expected<T, StarException>(std::in_place, std::move(*converted));
              { std::unexpected(std::move(converted.error())) } -> std::convertible_to<std::expected<T, StarException>>;
            })
        || (!checked_converter_result<Result>
            && requires(std::remove_cvref_t<Result>& converted) {
                 std::expected<T, StarException>(std::in_place, std::move(converted));
               }));
}

template <typename T, typename Converter, typename Value>
concept json_converter = std::invocable<Converter, Value const&>
  && json_extra_detail::converter_result<T, std::invoke_result_t<Converter, Value const&>>;

namespace json_extra_detail {
  // Nonfallible converters are allowed; fallible converters retain their error.
  template <typename T, typename Converter, typename Value>
    requires(json_converter<T, Converter, Value>)
  std::expected<T, StarException> convert(Converter&& converter, Value const& value) {
    if constexpr (!std::is_function_v<std::remove_reference_t<Converter>>
                  && requires { { converter == nullptr } -> std::convertible_to<bool>; }) {
      if (converter == nullptr)
        return std::unexpected(JsonException("Cannot invoke an empty Json converter"));
    }
    auto converted = std::invoke(std::forward<Converter>(converter), value);
    if constexpr (checked_converter_result<decltype(converted)>) {
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      return std::expected<T, StarException>(std::in_place, std::move(*converted));
    } else {
      return std::expected<T, StarException>(std::in_place, std::move(converted));
    }
  }

  template <typename T, std::size_t extent, typename Getter>
  std::expected<Array<T, extent>, StarException> array(Json const& v, Getter getter, std::string_view name) {
    auto size = v.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    if (*size != extent)
      return std::unexpected(JsonException(Star::format("Json array not of size {} in {}", extent, name)));
    Array<T, extent> result;
    for (std::size_t i = 0; i < extent; ++i) {
      auto item = std::invoke(getter, v, i);
      if (!item)
        return std::unexpected(std::move(item.error()));
      result[i] = std::move(*item);
    }
    return result;
  }
}

template <std::size_t extent>
std::expected<Array<unsigned, extent>, StarException> json_to_array_u(Json const& v) {
  return json_extra_detail::array<unsigned, extent>(v, [](Json const& j, std::size_t i) { return j.get_uint(i); }, "jsonToArrayU");
}
template <std::size_t extent>
std::expected<Array<std::size_t, extent>, StarException> json_to_array_s(Json const& v) {
  return json_extra_detail::array<std::size_t, extent>(v, [](Json const& j, std::size_t i) { return j.get_uint(i); }, "jsonToArrayS");
}
template <std::size_t extent>
std::expected<Array<int, extent>, StarException> json_to_array_i(Json const& v) {
  return json_extra_detail::array<int, extent>(v, [](Json const& j, std::size_t i) { return j.get_int(i); }, "jsonToArrayI");
}
template <std::size_t extent>
std::expected<Array<float, extent>, StarException> json_to_array_f(Json const& v) {
  return json_extra_detail::array<float, extent>(v, [](Json const& j, std::size_t i) { return j.get_float(i); }, "jsonToArrayF");
}
template <std::size_t extent>
std::expected<Array<double, extent>, StarException> json_to_array_d(Json const& v) {
  return json_extra_detail::array<double, extent>(v, [](Json const& j, std::size_t i) { return j.get_double(i); }, "jsonToArrayD");
}
template <std::size_t extent>
std::expected<Array<String, extent>, StarException> json_to_string_array(Json const& v) {
  return json_extra_detail::array<String, extent>(v, [](Json const& j, std::size_t i) { return j.get_string(i); }, "jsonToStringArray");
}

template <typename Value, typename Converter>
  requires (json_converter<Value, Converter&, Json>
            && requires(List<Value>& list, Value& entry, std::size_t count) {
                 list.reserve(count);
                 list.push_back(std::move(entry));
               })
std::expected<List<Value>, StarException> json_to_list(Json const& v, Converter&& converter) {
  auto entries = v.iterate_array();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  List<Value> result;
  result.reserve(entries->container->size());
  for (auto const& entry : *entries) {
    auto converted = json_extra_detail::convert<Value>(converter, entry);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    result.push_back(std::move(*converted));
  }
  return result;
}
template <typename Value>
  requires requires(Json const& source) { json_to_list<Value>(source, Construct<Value>()); }
std::expected<List<Value>, StarException> json_to_list(Json const& v) {
  return json_to_list<Value>(v, Construct<Value>());
}

namespace json_extra_detail {
  template <typename Range, typename Converter>
  concept json_array_source = std::ranges::input_range<Range const>
    && requires(Range const& source, JsonArray& array) { array.reserve(source.size()); }
    && json_converter<Json, Converter&, std::remove_reference_t<std::ranges::range_reference_t<Range const>>>;

  template <typename Range, typename Converter>
    requires json_array_source<Range, Converter>
  std::expected<Json, StarException> encode_array(Range const& source, Converter& converter) {
    JsonArray result;
    result.reserve(source.size());
    for (auto const& entry : source) {
      auto converted = json_extra_detail::convert<Json>(converter, entry);
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      result.push_back(std::move(*converted));
    }
    return Json(std::move(result));
  }
}

template <typename Value, typename Converter>
  requires json_extra_detail::json_array_source<List<Value>, Converter>
std::expected<Json, StarException> json_from_list(List<Value> const& list, Converter&& converter) {
  return json_extra_detail::encode_array(list, converter);
}
template <typename Value>
  requires json_extra_detail::json_array_source<List<Value>, Construct<Json>>
std::expected<Json, StarException> json_from_list(List<Value> const& list) {
  return json_from_list(list, Construct<Json>());
}

template <typename SetType, typename Converter>
  requires (std::default_initializable<SetType> && std::move_constructible<SetType>
            && json_converter<typename SetType::value_type, Converter&, Json>
            && requires(SetType& set, typename SetType::value_type& entry) { set.add(std::move(entry)); })
std::expected<SetType, StarException> json_to_set(Json const& v, Converter&& converter) {
  auto entries = v.iterate_array();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  SetType result;
  for (auto const& entry : *entries) {
    auto converted = json_extra_detail::convert<typename SetType::value_type>(converter, entry);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    result.add(std::move(*converted));
  }
  return result;
}
template <typename SetType>
  requires requires(Json const& source) { json_to_set<SetType>(source, Construct<typename SetType::value_type>()); }
std::expected<SetType, StarException> json_to_set(Json const& v) {
  return json_to_set<SetType>(v, Construct<typename SetType::value_type>());
}
template <typename SetType, typename Converter>
  requires json_extra_detail::json_array_source<SetType, Converter>
std::expected<Json, StarException> json_from_set(SetType const& set, Converter&& converter) {
  return json_extra_detail::encode_array(set, converter);
}
template <typename SetType>
  requires json_extra_detail::json_array_source<SetType, Construct<Json>>
std::expected<Json, StarException> json_from_set(SetType const& set) {
  return json_from_set(set, Construct<Json>());
}

template <typename MapType, typename KeyConverter, typename ValueConverter>
std::expected<MapType, StarException> json_to_map_kv(Json const& v, KeyConverter&& key_convert, ValueConverter&& value_convert) {
  auto entries = v.iterate_object();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  MapType result;
  for (auto const& pair : *entries) {
    auto key = json_extra_detail::convert<typename MapType::key_type>(key_convert, pair.first);
    if (!key)
      return std::unexpected(std::move(key.error()));
    auto value = json_extra_detail::convert<typename MapType::mapped_type>(value_convert, pair.second);
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto added = result.add(std::move(*key), std::move(*value));
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return result;
}
template <typename MapType, typename KeyConverter>
std::expected<MapType, StarException> json_to_map_k(Json const& v, KeyConverter&& converter) {
  return json_to_map_kv<MapType>(v, std::forward<KeyConverter>(converter), Construct<typename MapType::mapped_type>());
}
template <typename MapType, typename ValueConverter>
std::expected<MapType, StarException> json_to_map_v(Json const& v, ValueConverter&& converter) {
  return json_to_map_kv<MapType>(v, Construct<typename MapType::key_type>(), std::forward<ValueConverter>(converter));
}
template <typename MapType>
std::expected<MapType, StarException> json_to_map(Json const& v) {
  return json_to_map_kv<MapType>(v, Construct<typename MapType::key_type>(), Construct<typename MapType::mapped_type>());
}
template <typename MapType, typename KeyConverter, typename ValueConverter>
std::expected<Json, StarException> json_from_map_kv(MapType const& map, KeyConverter&& key_convert, ValueConverter&& value_convert) {
  JsonObject result;
  for (auto const& pair : map) {
    auto key = json_extra_detail::convert<String>(key_convert, pair.first);
    if (!key)
      return std::unexpected(std::move(key.error()));
    auto value = json_extra_detail::convert<Json>(value_convert, pair.second);
    if (!value)
      return std::unexpected(std::move(value.error()));
    result[std::move(*key)] = std::move(*value);
  }
  return Json(std::move(result));
}
template <typename MapType, typename KeyConverter>
std::expected<Json, StarException> json_from_map_k(MapType const& map, KeyConverter&& converter) {
  return json_from_map_kv(map, std::forward<KeyConverter>(converter), Construct<Json>());
}
template <typename MapType, typename ValueConverter>
std::expected<Json, StarException> json_from_map_v(MapType const& map, ValueConverter&& converter) {
  return json_from_map_kv(map, Construct<String>(), std::forward<ValueConverter>(converter));
}
template <typename MapType>
std::expected<Json, StarException> json_from_map(MapType const& map) {
  return json_from_map_kv(map, Construct<String>(), Construct<Json>());
}

template <typename T, typename Converter>
  requires(json_converter<Json, Converter, T>)
std::expected<Json, StarException> json_from_optional(std::optional<T> const& value, Converter&& converter) {
  if (!value)
    return Json();
  return json_extra_detail::convert<Json>(std::forward<Converter>(converter), *value);
}
template <typename T>
  requires requires(T const& value) { Json(value); }
Json json_from_optional(std::optional<T> const& value) {
  return value ? Json(*value) : Json();
}
template <typename T, typename Converter>
  requires(json_converter<T, Converter, Json>
           && requires(std::optional<T>& value, T& payload) { value.emplace(std::move(payload)); })
std::expected<std::optional<T>, StarException> json_to_optional(Json const& v, Converter&& converter) {
  if (v.is_null())
    return std::expected<std::optional<T>, StarException>(std::in_place);
  auto converted = json_extra_detail::convert<T>(std::forward<Converter>(converter), v);
  if (!converted)
    return std::unexpected(std::move(converted.error()));
  return std::expected<std::optional<T>, StarException>(std::in_place, std::in_place, std::move(*converted));
}
template <typename T>
  requires(json_converter<T, Construct<T>, Json>
           && requires(std::optional<T>& value, T& payload) { value.emplace(std::move(payload)); })
std::expected<std::optional<T>, StarException> json_to_optional(Json const& v) {
  return json_to_optional<T>(v, Construct<T>());
}

template <typename T, typename Converter>
  requires(json_converter<T, Converter&, Json>
           && requires(WeightedPool<T>& pool, T& item) { pool.insert(0.0, std::move(item)); })
std::expected<WeightedPool<T>, StarException> json_to_weighted_pool(Json const& source, Converter&& converter) {
  WeightedPool<T> result;
  if (source.is_null())
    return result;
  auto entries = source.iterate_array();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& entry : *entries) {
    auto weight = entry.is_type(Json::Type::Array) ? entry.get_double(0) : entry.get_double("weight");
    if (!weight)
      return std::unexpected(std::move(weight.error()));
    auto item = entry.is_type(Json::Type::Array) ? entry.get(1) : entry.get("item");
    if (!item)
      return std::unexpected(std::move(item.error()));
    auto converted = json_extra_detail::convert<T>(converter, *item);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    auto inserted = result.insert(*weight, std::move(*converted));
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
  }
  return result;
}

namespace json_extra_detail {
  template <typename T>
  concept weighted_pool_json_item =
    (std::same_as<T, int> || std::same_as<T, unsigned>
     || std::same_as<T, float> || std::same_as<T, double>
     || std::same_as<T, String> || std::same_as<T, JsonArray>
     || std::same_as<T, JsonObject> || json_converter<T, Construct<T>&, Json>)
    && requires(WeightedPool<T>& pool, T& item) { pool.insert(0.0, std::move(item)); };
}

template <typename T>
  requires(json_extra_detail::weighted_pool_json_item<T>)
std::expected<WeightedPool<T>, StarException> json_to_weighted_pool(Json const& source) {
  return json_to_weighted_pool<T>(source, Construct<T>());
}
template <typename T, typename Converter>
  requires(json_converter<Json, Converter&, T>)
std::expected<Json, StarException> json_from_weighted_pool(WeightedPool<T> const& pool, Converter&& converter) {
  JsonArray result;
  for (auto const& pair : pool.items()) {
    auto converted = json_extra_detail::convert<Json>(converter, pair.second);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    result.push_back(JsonObject{{"weight", pair.first}, {"item", std::move(*converted)}});
  }
  return Json(std::move(result));
}
template <typename T>
  requires(json_converter<Json, Construct<Json>&, T>)
std::expected<Json, StarException> json_from_weighted_pool(WeightedPool<T> const& pool) {
  return json_from_weighted_pool(pool, Construct<Json>());
}

template <> std::expected<WeightedPool<int>, StarException> json_to_weighted_pool(Json const& source);
template <> std::expected<WeightedPool<unsigned>, StarException> json_to_weighted_pool(Json const& source);
template <> std::expected<WeightedPool<float>, StarException> json_to_weighted_pool(Json const& source);
template <> std::expected<WeightedPool<double>, StarException> json_to_weighted_pool(Json const& source);
template <> std::expected<WeightedPool<String>, StarException> json_to_weighted_pool(Json const& source);
template <> std::expected<WeightedPool<JsonArray>, StarException> json_to_weighted_pool(Json const& source);
template <> std::expected<WeightedPool<JsonObject>, StarException> json_to_weighted_pool(Json const& source);

template <typename Float>
Polygon<Float> fix_inside_out_poly(Polygon<Float> p) {
  if (p.sides() > 2) {
    if ((p.side(1).diff() ^ p.side(0).diff()) > 0)
      std::reverse(p.vertices().begin(), p.vertices().end());
  }
  return p;
}

}

export module star.json_extra;

export namespace Star {
  using ::Star::json_to_size;
  using ::Star::json_from_size;
  using ::Star::json_to_vec2d;
  using ::Star::json_to_vec2f;
  using ::Star::json_from_vec2f;
  using ::Star::json_to_vec2i;
  using ::Star::json_from_vec2i;
  using ::Star::json_to_vec2u;
  using ::Star::json_from_vec2u;
  using ::Star::json_to_vec2b;
  using ::Star::json_from_vec2b;
  using ::Star::json_to_vec3d;
  using ::Star::json_to_vec3f;
  using ::Star::json_from_vec3f;
  using ::Star::json_to_vec3i;
  using ::Star::json_from_vec3i;
  using ::Star::json_to_vec3b;
  using ::Star::json_to_vec4b;
  using ::Star::json_to_vec4i;
  using ::Star::json_to_vec4f;
  using ::Star::json_to_rectd;
  using ::Star::json_from_rectd;
  using ::Star::json_to_rectf;
  using ::Star::json_from_rectf;
  using ::Star::json_to_recti;
  using ::Star::json_from_recti;
  using ::Star::json_to_rectu;
  using ::Star::json_from_rectu;
  using ::Star::json_to_color;
  using ::Star::json_from_color;
  using ::Star::fix_inside_out_poly;
  using ::Star::json_to_polyd;
  using ::Star::json_to_polyf;
  using ::Star::json_to_polyi;
  using ::Star::json_from_polyf;
  using ::Star::json_to_line2f;
  using ::Star::json_from_line2f;
  using ::Star::json_to_mat3f;
  using ::Star::json_from_mat3f;
  using ::Star::json_to_string_list;
  using ::Star::json_from_string_list;
  using ::Star::json_to_string_set;
  using ::Star::json_from_string_set;
  using ::Star::json_to_float_list;
  using ::Star::json_to_int_list;
  using ::Star::json_to_vec2i_list;
  using ::Star::json_to_vec2u_list;
  using ::Star::json_to_vec2f_list;
  using ::Star::json_to_vec4b_list;
  using ::Star::json_to_color_list;
  using ::Star::json_to_directives_list;
  using ::Star::json_from_directives_list;
  using ::Star::weighted_choice_from_json;
  using ::Star::binned_choice_from_json;
  using ::Star::json_to_weighted_pool;
  using ::Star::json_from_weighted_pool;
  using ::Star::json_to_array_u;
  using ::Star::json_to_array_s;
  using ::Star::json_to_array_i;
  using ::Star::json_to_array_f;
  using ::Star::json_to_array_d;
  using ::Star::json_to_string_array;
  using ::Star::json_to_list;
  using ::Star::json_from_list;
  using ::Star::json_to_set;
  using ::Star::json_from_set;
  using ::Star::json_to_map_kv;
  using ::Star::json_to_map_k;
  using ::Star::json_to_map_v;
  using ::Star::json_to_map;
  using ::Star::json_from_map_kv;
  using ::Star::json_from_map_k;
  using ::Star::json_from_map_v;
  using ::Star::json_from_map;
  using ::Star::json_converter;
  using ::Star::json_from_optional;
  using ::Star::json_to_optional;

  extern "C++" {
  template <>
  std::expected<WeightedPool<int>, StarException> json_to_weighted_pool(Json const& source);
  template <>
  std::expected<WeightedPool<unsigned>, StarException> json_to_weighted_pool(Json const& source);
  template <>
  std::expected<WeightedPool<float>, StarException> json_to_weighted_pool(Json const& source);
  template <>
  std::expected<WeightedPool<double>, StarException> json_to_weighted_pool(Json const& source);
  template <>
  std::expected<WeightedPool<String>, StarException> json_to_weighted_pool(Json const& source);
  template <>
  std::expected<WeightedPool<JsonArray>, StarException> json_to_weighted_pool(Json const& source);
  template <>
  std::expected<WeightedPool<JsonObject>, StarException> json_to_weighted_pool(Json const& source);
  }
}
