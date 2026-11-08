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
std::expected<std::size_t, StarException> jsonToSize(Json const& v);
Json jsonFromSize(std::size_t s);

// Must be array of appropriate size.

std::expected<Vec2D, StarException> jsonToVec2D(Json const& v);
std::expected<Vec2F, StarException> jsonToVec2F(Json const& v);
Json jsonFromVec2F(Vec2F const& v);
std::expected<Vec2I, StarException> jsonToVec2I(Json const& v);
Json jsonFromVec2I(Vec2I const& v);
std::expected<Vec2U, StarException> jsonToVec2U(Json const& v);
Json jsonFromVec2U(Vec2U const& v);
std::expected<Vec2B, StarException> jsonToVec2B(Json const& v);
Json jsonFromVec2B(Vec2B const& v);

std::expected<Vec3D, StarException> jsonToVec3D(Json const& v);
std::expected<Vec3F, StarException> jsonToVec3F(Json const& v);
Json jsonFromVec3F(Vec3F const& v);
std::expected<Vec3I, StarException> jsonToVec3I(Json const& v);
Json jsonFromVec3I(Vec3I const& v);
std::expected<Vec3B, StarException> jsonToVec3B(Json const& v);

std::expected<Vec4B, StarException> jsonToVec4B(Json const& v);
std::expected<Vec4I, StarException> jsonToVec4I(Json const& v);
std::expected<Vec4F, StarException> jsonToVec4F(Json const& v);

// Must be array of size 4 or 2 arrays of size 2 in an array.
std::expected<RectD, StarException> jsonToRectD(Json const& v);
Json jsonFromRectD(RectD const& rect);
std::expected<RectF, StarException> jsonToRectF(Json const& v);
Json jsonFromRectF(RectF const& rect);
std::expected<RectI, StarException> jsonToRectI(Json const& v);
Json jsonFromRectI(RectI const& rect);
std::expected<RectU, StarException> jsonToRectU(Json const& v);
Json jsonFromRectU(RectU const& rect);

// Can be a string, array of size 3 or 4 of doubles or ints.  If double, range
// is 0.0 to 1.0, if int range is 0-255
std::expected<Color, StarException> jsonToColor(Json const& v);
Json jsonFromColor(Color const& color);

// HACK: Fix clockwise specified polygons in coming from JSON
template <typename Float>
Polygon<Float> fixInsideOutPoly(Polygon<Float> p);

// Array of size 2 arrays
std::expected<PolyD, StarException> jsonToPolyD(Json const& v);
std::expected<PolyF, StarException> jsonToPolyF(Json const& v);
std::expected<PolyI, StarException> jsonToPolyI(Json const& v);
Json jsonFromPolyF(PolyF const& poly);

// Expects a size 2 array of size 2 arrays
std::expected<Line2F, StarException> jsonToLine2F(Json const& v);
Json jsonFromLine2F(Line2F const& line);

std::expected<Mat3F, StarException> jsonToMat3F(Json const& v);
Json jsonFromMat3F(Mat3F const& v);

std::expected<StringList, StarException> jsonToStringList(Json const& v);
Json jsonFromStringList(List<String> const& v);
std::expected<StringSet, StarException> jsonToStringSet(Json const& v);
Json jsonFromStringSet(StringSet const& v);
std::expected<List<float>, StarException> jsonToFloatList(Json const& v);
std::expected<List<int>, StarException> jsonToIntList(Json const& v);
std::expected<List<Vec2I>, StarException> jsonToVec2IList(Json const& v);
std::expected<List<Vec2U>, StarException> jsonToVec2UList(Json const& v);
std::expected<List<Vec2F>, StarException> jsonToVec2FList(Json const& v);
std::expected<List<Vec4B>, StarException> jsonToVec4BList(Json const& v);
std::expected<List<Color>, StarException> jsonToColorList(Json const& v);
std::expected<List<Directives>, StarException> jsonToDirectivesList(Json const& v);
Json jsonFromDirectivesList(List<Directives> const& v);

std::expected<Json, StarException> weightedChoiceFromJson(RandomService& random, Json const& source, Json const& default_);

// Assumes that the bins parameter is an array of pairs (arrays), where the
// first element is a minimum value and the second element is the actual
// important value.  Finds the pair with the highest value that is less than or
// equal to the given target, and returns the second element.
std::expected<Json, StarException> binnedChoiceFromJson(Json const& bins, float target, Json const& def = Json());

namespace JsonExtraDetail {
  // Nonfallible converters are allowed; fallible converters retain their error.
  template <typename T, typename Converter, typename Value>
  std::expected<T, StarException> convert(Converter&& converter, Value const& value) {
    if constexpr (requires { static_cast<bool>(converter); }) {
      if (!static_cast<bool>(converter))
        return std::unexpected(JsonException("Cannot invoke an empty Json converter"));
    }
    auto converted = std::invoke(std::forward<Converter>(converter), value);
    if constexpr (requires { converted.error(); }) {
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      return T(std::move(*converted));
    } else {
      return T(std::move(converted));
    }
  }

  template <typename T, std::size_t Size, typename Getter>
  std::expected<Array<T, Size>, StarException> array(Json const& v, Getter getter, std::string_view name) {
    auto size = v.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    if (*size != Size)
      return std::unexpected(JsonException(Star::format("Json array not of size {} in {}", Size, name)));
    Array<T, Size> result;
    for (std::size_t i = 0; i < Size; ++i) {
      auto item = std::invoke(getter, v, i);
      if (!item)
        return std::unexpected(std::move(item.error()));
      result[i] = std::move(*item);
    }
    return result;
  }
}

template <std::size_t Size>
std::expected<Array<unsigned, Size>, StarException> jsonToArrayU(Json const& v) {
  return JsonExtraDetail::array<unsigned, Size>(v, [](Json const& j, std::size_t i) { return j.getUInt(i); }, "jsonToArrayU");
}
template <std::size_t Size>
std::expected<Array<std::size_t, Size>, StarException> jsonToArrayS(Json const& v) {
  return JsonExtraDetail::array<std::size_t, Size>(v, [](Json const& j, std::size_t i) { return j.getUInt(i); }, "jsonToArrayS");
}
template <std::size_t Size>
std::expected<Array<int, Size>, StarException> jsonToArrayI(Json const& v) {
  return JsonExtraDetail::array<int, Size>(v, [](Json const& j, std::size_t i) { return j.getInt(i); }, "jsonToArrayI");
}
template <std::size_t Size>
std::expected<Array<float, Size>, StarException> jsonToArrayF(Json const& v) {
  return JsonExtraDetail::array<float, Size>(v, [](Json const& j, std::size_t i) { return j.getFloat(i); }, "jsonToArrayF");
}
template <std::size_t Size>
std::expected<Array<double, Size>, StarException> jsonToArrayD(Json const& v) {
  return JsonExtraDetail::array<double, Size>(v, [](Json const& j, std::size_t i) { return j.getDouble(i); }, "jsonToArrayD");
}
template <std::size_t Size>
std::expected<Array<String, Size>, StarException> jsonToStringArray(Json const& v) {
  return JsonExtraDetail::array<String, Size>(v, [](Json const& j, std::size_t i) { return j.getString(i); }, "jsonToStringArray");
}

template <typename Value, typename Converter>
std::expected<List<Value>, StarException> jsonToList(Json const& v, Converter&& converter) {
  auto entries = v.iterateArray();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  List<Value> result;
  result.reserve(entries->ptr->size());
  for (auto const& entry : *entries) {
    auto converted = JsonExtraDetail::convert<Value>(converter, entry);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    result.push_back(std::move(*converted));
  }
  return result;
}
template <typename Value>
std::expected<List<Value>, StarException> jsonToList(Json const& v) {
  return jsonToList<Value>(v, construct<Value>());
}

template <typename Value, typename Converter>
std::expected<Json, StarException> jsonFromList(List<Value> const& list, Converter&& converter) {
  JsonArray result;
  result.reserve(list.size());
  for (auto const& entry : list) {
    auto converted = JsonExtraDetail::convert<Json>(converter, entry);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    result.push_back(std::move(*converted));
  }
  return Json(std::move(result));
}
template <typename Value>
std::expected<Json, StarException> jsonFromList(List<Value> const& list) {
  return jsonFromList(list, construct<Json>());
}

template <typename SetType, typename Converter>
std::expected<SetType, StarException> jsonToSet(Json const& v, Converter&& converter) {
  auto entries = v.iterateArray();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  SetType result;
  for (auto const& entry : *entries) {
    auto converted = JsonExtraDetail::convert<typename SetType::value_type>(converter, entry);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    result.add(std::move(*converted));
  }
  return result;
}
template <typename SetType>
std::expected<SetType, StarException> jsonToSet(Json const& v) {
  return jsonToSet<SetType>(v, construct<typename SetType::value_type>());
}
template <typename SetType, typename Converter>
std::expected<Json, StarException> jsonFromSet(SetType const& set, Converter&& converter) {
  JsonArray result;
  result.reserve(set.size());
  for (auto const& entry : set) {
    auto converted = JsonExtraDetail::convert<Json>(converter, entry);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    result.push_back(std::move(*converted));
  }
  return Json(std::move(result));
}
template <typename SetType>
std::expected<Json, StarException> jsonFromSet(SetType const& set) {
  return jsonFromSet(set, construct<Json>());
}

template <typename MapType, typename KeyConverter, typename ValueConverter>
std::expected<MapType, StarException> jsonToMapKV(Json const& v, KeyConverter&& keyConvert, ValueConverter&& valueConvert) {
  auto entries = v.iterateObject();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  MapType result;
  for (auto const& pair : *entries) {
    auto key = JsonExtraDetail::convert<typename MapType::key_type>(keyConvert, pair.first);
    if (!key)
      return std::unexpected(std::move(key.error()));
    auto value = JsonExtraDetail::convert<typename MapType::mapped_type>(valueConvert, pair.second);
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto added = result.add(std::move(*key), std::move(*value));
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return result;
}
template <typename MapType, typename KeyConverter>
std::expected<MapType, StarException> jsonToMapK(Json const& v, KeyConverter&& converter) {
  return jsonToMapKV<MapType>(v, std::forward<KeyConverter>(converter), construct<typename MapType::mapped_type>());
}
template <typename MapType, typename ValueConverter>
std::expected<MapType, StarException> jsonToMapV(Json const& v, ValueConverter&& converter) {
  return jsonToMapKV<MapType>(v, construct<typename MapType::key_type>(), std::forward<ValueConverter>(converter));
}
template <typename MapType>
std::expected<MapType, StarException> jsonToMap(Json const& v) {
  return jsonToMapKV<MapType>(v, construct<typename MapType::key_type>(), construct<typename MapType::mapped_type>());
}
template <typename MapType, typename KeyConverter, typename ValueConverter>
std::expected<Json, StarException> jsonFromMapKV(MapType const& map, KeyConverter&& keyConvert, ValueConverter&& valueConvert) {
  JsonObject result;
  for (auto const& pair : map) {
    auto key = JsonExtraDetail::convert<String>(keyConvert, pair.first);
    if (!key)
      return std::unexpected(std::move(key.error()));
    auto value = JsonExtraDetail::convert<Json>(valueConvert, pair.second);
    if (!value)
      return std::unexpected(std::move(value.error()));
    result[std::move(*key)] = std::move(*value);
  }
  return Json(std::move(result));
}
template <typename MapType, typename KeyConverter>
std::expected<Json, StarException> jsonFromMapK(MapType const& map, KeyConverter&& converter) {
  return jsonFromMapKV(map, std::forward<KeyConverter>(converter), construct<Json>());
}
template <typename MapType, typename ValueConverter>
std::expected<Json, StarException> jsonFromMapV(MapType const& map, ValueConverter&& converter) {
  return jsonFromMapKV(map, construct<String>(), std::forward<ValueConverter>(converter));
}
template <typename MapType>
std::expected<Json, StarException> jsonFromMap(MapType const& map) {
  return jsonFromMapKV(map, construct<String>(), construct<Json>());
}

template <typename T, typename Converter>
std::expected<Json, StarException> jsonFromOptional(std::optional<T> const& value, Converter&& converter) {
  if (!value)
    return Json();
  return JsonExtraDetail::convert<Json>(std::forward<Converter>(converter), *value);
}
template <typename T>
Json jsonFromOptional(std::optional<T> const& value) {
  return value ? Json(*value) : Json();
}
template <typename T, typename Converter>
std::expected<std::optional<T>, StarException> jsonToOptional(Json const& v, Converter&& converter) {
  if (v.isNull())
    return std::optional<T>{};
  auto converted = JsonExtraDetail::convert<T>(std::forward<Converter>(converter), v);
  if (!converted)
    return std::unexpected(std::move(converted.error()));
  return std::optional<T>(std::move(*converted));
}
template <typename T>
std::expected<std::optional<T>, StarException> jsonToOptional(Json const& v) {
  return jsonToOptional<T>(v, construct<T>());
}

template <typename T, typename Converter>
std::expected<WeightedPool<T>, StarException> jsonToWeightedPool(Json const& source, Converter&& converter) {
  WeightedPool<T> result;
  if (source.isNull())
    return result;
  auto entries = source.iterateArray();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& entry : *entries) {
    auto weight = entry.isType(Json::Type::Array) ? entry.getDouble(0) : entry.getDouble("weight");
    if (!weight)
      return std::unexpected(std::move(weight.error()));
    auto item = entry.isType(Json::Type::Array) ? entry.get(1) : entry.get("item");
    if (!item)
      return std::unexpected(std::move(item.error()));
    auto converted = JsonExtraDetail::convert<T>(converter, *item);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    result.add(*weight, std::move(*converted));
  }
  return result;
}
template <typename T>
std::expected<WeightedPool<T>, StarException> jsonToWeightedPool(Json const& source) {
  return jsonToWeightedPool<T>(source, construct<T>());
}
template <typename T, typename Converter>
std::expected<Json, StarException> jsonFromWeightedPool(WeightedPool<T> const& pool, Converter&& converter) {
  JsonArray result;
  for (auto const& pair : pool.items()) {
    auto converted = JsonExtraDetail::convert<Json>(converter, pair.second);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    result.push_back(JsonObject{{"weight", pair.first}, {"item", std::move(*converted)}});
  }
  return Json(std::move(result));
}
template <typename T>
std::expected<Json, StarException> jsonFromWeightedPool(WeightedPool<T> const& pool) {
  return jsonFromWeightedPool(pool, construct<Json>());
}

template <> std::expected<WeightedPool<int>, StarException> jsonToWeightedPool(Json const& source);
template <> std::expected<WeightedPool<unsigned>, StarException> jsonToWeightedPool(Json const& source);
template <> std::expected<WeightedPool<float>, StarException> jsonToWeightedPool(Json const& source);
template <> std::expected<WeightedPool<double>, StarException> jsonToWeightedPool(Json const& source);
template <> std::expected<WeightedPool<String>, StarException> jsonToWeightedPool(Json const& source);
template <> std::expected<WeightedPool<JsonArray>, StarException> jsonToWeightedPool(Json const& source);
template <> std::expected<WeightedPool<JsonObject>, StarException> jsonToWeightedPool(Json const& source);

template <typename Float>
Polygon<Float> fixInsideOutPoly(Polygon<Float> p) {
  if (p.sides() > 2) {
    if ((p.side(1).diff() ^ p.side(0).diff()) > 0)
      std::reverse(p.vertexes().begin(), p.vertexes().end());
  }
  return p;
}

}

export module star.json_extra;

export namespace Star {
  using ::Star::jsonToSize;
  using ::Star::jsonFromSize;
  using ::Star::jsonToVec2D;
  using ::Star::jsonToVec2F;
  using ::Star::jsonFromVec2F;
  using ::Star::jsonToVec2I;
  using ::Star::jsonFromVec2I;
  using ::Star::jsonToVec2U;
  using ::Star::jsonFromVec2U;
  using ::Star::jsonToVec2B;
  using ::Star::jsonFromVec2B;
  using ::Star::jsonToVec3D;
  using ::Star::jsonToVec3F;
  using ::Star::jsonFromVec3F;
  using ::Star::jsonToVec3I;
  using ::Star::jsonFromVec3I;
  using ::Star::jsonToVec3B;
  using ::Star::jsonToVec4B;
  using ::Star::jsonToVec4I;
  using ::Star::jsonToVec4F;
  using ::Star::jsonToRectD;
  using ::Star::jsonFromRectD;
  using ::Star::jsonToRectF;
  using ::Star::jsonFromRectF;
  using ::Star::jsonToRectI;
  using ::Star::jsonFromRectI;
  using ::Star::jsonToRectU;
  using ::Star::jsonFromRectU;
  using ::Star::jsonToColor;
  using ::Star::jsonFromColor;
  using ::Star::fixInsideOutPoly;
  using ::Star::jsonToPolyD;
  using ::Star::jsonToPolyF;
  using ::Star::jsonToPolyI;
  using ::Star::jsonFromPolyF;
  using ::Star::jsonToLine2F;
  using ::Star::jsonFromLine2F;
  using ::Star::jsonToMat3F;
  using ::Star::jsonFromMat3F;
  using ::Star::jsonToStringList;
  using ::Star::jsonFromStringList;
  using ::Star::jsonToStringSet;
  using ::Star::jsonFromStringSet;
  using ::Star::jsonToFloatList;
  using ::Star::jsonToIntList;
  using ::Star::jsonToVec2IList;
  using ::Star::jsonToVec2UList;
  using ::Star::jsonToVec2FList;
  using ::Star::jsonToVec4BList;
  using ::Star::jsonToColorList;
  using ::Star::jsonToDirectivesList;
  using ::Star::jsonFromDirectivesList;
  using ::Star::weightedChoiceFromJson;
  using ::Star::binnedChoiceFromJson;
  using ::Star::jsonToWeightedPool;
  using ::Star::jsonFromWeightedPool;
  using ::Star::jsonToArrayU;
  using ::Star::jsonToArrayS;
  using ::Star::jsonToArrayI;
  using ::Star::jsonToArrayF;
  using ::Star::jsonToArrayD;
  using ::Star::jsonToStringArray;
  using ::Star::jsonToList;
  using ::Star::jsonFromList;
  using ::Star::jsonToSet;
  using ::Star::jsonFromSet;
  using ::Star::jsonToMapKV;
  using ::Star::jsonToMapK;
  using ::Star::jsonToMapV;
  using ::Star::jsonToMap;
  using ::Star::jsonFromMapKV;
  using ::Star::jsonFromMapK;
  using ::Star::jsonFromMapV;
  using ::Star::jsonFromMap;
  using ::Star::jsonFromOptional;
  using ::Star::jsonToOptional;

  extern "C++" {
  template <>
  std::expected<WeightedPool<int>, StarException> jsonToWeightedPool(Json const& source);
  template <>
  std::expected<WeightedPool<unsigned>, StarException> jsonToWeightedPool(Json const& source);
  template <>
  std::expected<WeightedPool<float>, StarException> jsonToWeightedPool(Json const& source);
  template <>
  std::expected<WeightedPool<double>, StarException> jsonToWeightedPool(Json const& source);
  template <>
  std::expected<WeightedPool<String>, StarException> jsonToWeightedPool(Json const& source);
  template <>
  std::expected<WeightedPool<JsonArray>, StarException> jsonToWeightedPool(Json const& source);
  template <>
  std::expected<WeightedPool<JsonObject>, StarException> jsonToWeightedPool(Json const& source);
  }
}
