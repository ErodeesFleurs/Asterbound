import star.json_extra;
import std;
import star.memory;
import star.string;
import star.list;
import star.json;
import star.format;
import star.output_proxy;
import star.random;
import star.color;
import star.set;
import star.weighted_pool;
import star.directives;
import star.vector;
import star.matrix3;
import star.line;
import star.rect;
import star.poly;
import star.algorithm;

namespace Star {

std::expected<std::size_t, StarException> jsonToSize(Json const& v) {
  if (v.isNull())
    return NPos;
  if (!v.canConvert(Json::Type::Int))
    return std::unexpected(JsonException("Json not an int in jsonToSize"));
  return v.toUInt();
}

Json jsonFromSize(std::size_t s) {
  return s == NPos ? Json() : Json(s);
}

namespace {
  template <typename Vec, std::size_t Size, typename Getter>
  std::expected<Vec, StarException> jsonVector(Json const& v, Getter getter, std::string_view diagnostic) {
    if (v.type() != Json::Type::Array)
      return std::unexpected(JsonException(std::string(diagnostic)));
    auto size = v.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    if (*size != Size)
      return std::unexpected(JsonException(std::string(diagnostic)));
    Vec result;
    for (std::size_t i = 0; i < Size; ++i) {
      auto element = std::invoke(getter, v, i);
      if (!element)
        return std::unexpected(std::move(element.error()));
      result[i] = *element;
    }
    return result;
  }
}

std::expected<Vec2D, StarException> jsonToVec2D(Json const& v) {
  return jsonVector<Vec2D, 2>(v, [](Json const& j, std::size_t i) { return j.getDouble(i); }, "Json not an array of size 2 in jsonToVec2D");
}
std::expected<Vec2F, StarException> jsonToVec2F(Json const& v) {
  return jsonVector<Vec2F, 2>(v, [](Json const& j, std::size_t i) { return j.getFloat(i); }, "Json not an array of size 2 in jsonToVec2F");
}
std::expected<Vec2I, StarException> jsonToVec2I(Json const& v) {
  return jsonVector<Vec2I, 2>(v, [](Json const& j, std::size_t i) { return j.getInt(i); }, "Json not an array of size 2 in jsonToVec2I");
}
std::expected<Vec2U, StarException> jsonToVec2U(Json const& v) {
  return jsonVector<Vec2U, 2>(v, [](Json const& j, std::size_t i) { return j.getInt(i); }, "Json not an array of size 2 in jsonToVec2I");
}
std::expected<Vec2B, StarException> jsonToVec2B(Json const& v) {
  return jsonVector<Vec2B, 2>(v, [](Json const& j, std::size_t i) { return j.getInt(i); }, "Json not an array of size 2 in jsonToVec2B");
}
std::expected<Vec3D, StarException> jsonToVec3D(Json const& v) {
  return jsonVector<Vec3D, 3>(v, [](Json const& j, std::size_t i) { return j.getDouble(i); }, "Json not an array of size size 3 in jsonToVec3D");
}
std::expected<Vec3F, StarException> jsonToVec3F(Json const& v) {
  return jsonVector<Vec3F, 3>(v, [](Json const& j, std::size_t i) { return j.getFloat(i); }, "Json not an array of size 3 in jsonToVec3D");
}
std::expected<Vec3I, StarException> jsonToVec3I(Json const& v) {
  return jsonVector<Vec3I, 3>(v, [](Json const& j, std::size_t i) { return j.getInt(i); }, "Json not an array of size 3 in jsonToVec3I");
}
std::expected<Vec3B, StarException> jsonToVec3B(Json const& v) {
  return jsonVector<Vec3B, 3>(v, [](Json const& j, std::size_t i) { return j.getInt(i); }, "Json not an array of size 3 in jsonToVec3B");
}
std::expected<Vec4B, StarException> jsonToVec4B(Json const& v) {
  return jsonVector<Vec4B, 4>(v, [](Json const& j, std::size_t i) { return j.getInt(i); }, "Json not an array of size 4 in jsonToVec4B");
}
std::expected<Vec4I, StarException> jsonToVec4I(Json const& v) {
  return jsonVector<Vec4I, 4>(v, [](Json const& j, std::size_t i) { return j.getInt(i); }, "Json not an array of size 4 in jsonToVec4B");
}
std::expected<Vec4F, StarException> jsonToVec4F(Json const& v) {
  return jsonVector<Vec4F, 4>(v, [](Json const& j, std::size_t i) { return j.getFloat(i); }, "Json not an array of size 4 in jsonToVec4B");
}

Json jsonFromVec2F(Vec2F const& v) { return JsonArray{v[0], v[1]}; }
Json jsonFromVec2I(Vec2I const& v) { return JsonArray{v[0], v[1]}; }
Json jsonFromVec2U(Vec2U const& v) { return JsonArray{v[0], v[1]}; }
Json jsonFromVec2B(Vec2B const& v) { return JsonArray{v[0], v[1]}; }
Json jsonFromVec3F(Vec3F const& v) { return JsonArray{v[0], v[1], v[2]}; }
Json jsonFromVec3I(Vec3I const& v) { return JsonArray{v[0], v[1], v[2]}; }

namespace {
  template <typename Rect, typename Vec, typename Getter, typename Converter>
  std::expected<Rect, StarException> jsonRect(Json const& v, Getter getter, Converter& converter, std::string_view name) {
    if (v.type() != Json::Type::Array)
      return std::unexpected(JsonException(strf("Json not an array in {}", name)));
    auto size = v.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    if (*size != 4 && *size != 2)
      return std::unexpected(JsonException(strf("Json not an array of proper size in {}", name)));
    if (*size == 4) {
      Vec lowerLeft, upperRight;
      for (std::size_t i = 0; i < 4; ++i) {
        auto value = getter(v, i);
        if (!value)
          return std::unexpected(std::move(value.error()));
        (i < 2 ? lowerLeft : upperRight)[i % 2] = *value;
      }
      return Rect(lowerLeft, upperRight);
    }
    auto inner = [&]() -> std::expected<Rect, StarException> {
      auto lower = v.get(0);
      if (!lower)
        return std::unexpected(std::move(lower.error()));
      auto upper = v.get(1);
      if (!upper)
        return std::unexpected(std::move(upper.error()));
      auto lowerLeft = converter(*lower);
      if (!lowerLeft)
        return std::unexpected(std::move(lowerLeft.error()));
      auto upperRight = converter(*upper);
      if (!upperRight)
        return std::unexpected(std::move(upperRight.error()));
      return Rect(*lowerLeft, *upperRight);
    }();
    if (!inner && inner.error().template is<JsonException>())
      return std::unexpected(JsonException(strf("Inner position not well formed in {}: {}", name, outputException(ExceptionDiagnostic(inner.error()), true))));
    return inner;
  }
}

std::expected<RectD, StarException> jsonToRectD(Json const& v) {
  return jsonRect<RectD, Vec2D>(v, [](Json const& j, std::size_t i) { return j.getDouble(i); }, jsonToVec2D, "jsonToRectD");
}
std::expected<RectF, StarException> jsonToRectF(Json const& v) {
  auto result = jsonToRectD(v);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return RectF(*result);
}
std::expected<RectI, StarException> jsonToRectI(Json const& v) {
  return jsonRect<RectI, Vec2I>(v, [](Json const& j, std::size_t i) { return j.getInt(i); }, jsonToVec2I, "jsonToRectI");
}
std::expected<RectU, StarException> jsonToRectU(Json const& v) {
  // The first component historically uses signed conversion, the others unsigned.
  return jsonRect<RectU, Vec2U>(v, [](Json const& j, std::size_t i) -> std::expected<std::uint64_t, StarException> {
    if (i != 0)
      return j.getUInt(i);
    auto value = j.getInt(i);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return static_cast<std::uint64_t>(*value);
  }, jsonToVec2U, "jsonToRectU");
}
Json jsonFromRectD(RectD const& r) { return JsonArray{r.xMin(), r.yMin(), r.xMax(), r.yMax()}; }
Json jsonFromRectF(RectF const& r) { return JsonArray{r.xMin(), r.yMin(), r.xMax(), r.yMax()}; }
Json jsonFromRectI(RectI const& r) { return JsonArray{r.xMin(), r.yMin(), r.xMax(), r.yMax()}; }
Json jsonFromRectU(RectU const& r) { return JsonArray{r.xMin(), r.yMin(), r.xMax(), r.yMax()}; }

std::expected<Color, StarException> jsonToColor(Json const& v) {
  if (v.type() == Json::Type::Array) {
    auto size = v.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    if (*size != 3 && *size != 4)
      return std::unexpected(JsonException("Json not an array of size 3 or 4 in jsonToColor"));
    Vec4B channels(0, 0, 0, 255);
    for (std::size_t i = 0; i < *size; ++i) {
      auto channel = v.getInt(i);
      if (!channel)
        return std::unexpected(std::move(channel.error()));
      channels[i] = *channel;
    }
    return Color::rgba(channels[0], channels[1], channels[2], channels[3]);
  }
  if (v.type() == Json::Type::String) {
    auto text = v.toString();
    if (!text)
      return std::unexpected(std::move(text.error()));
    return Color::create(*text);
  }
  return std::unexpected(JsonException(strf("Json of type {} cannot be converted to color", v.typeName())));
}
Json jsonFromColor(Color const& color) {
  JsonArray result{color.red(), color.green(), color.blue()};
  if (color.alpha() < 255)
    result.push_back(color.alpha());
  return result;
}

std::expected<PolyD, StarException> jsonToPolyD(Json const& v) {
  auto vertices = v.iterateArray();
  if (!vertices)
    return std::unexpected(std::move(vertices.error()));
  PolyD poly;
  for (auto const& vertex : *vertices) {
    auto converted = jsonToVec2D(vertex);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    poly.add(*converted);
  }
  return fixInsideOutPoly(std::move(poly));
}
std::expected<PolyF, StarException> jsonToPolyF(Json const& v) {
  auto vertices = v.iterateArray();
  if (!vertices)
    return std::unexpected(std::move(vertices.error()));
  PolyF poly;
  for (auto const& vertex : *vertices) {
    auto converted = jsonToVec2F(vertex);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    poly.add(*converted);
  }
  return fixInsideOutPoly(std::move(poly));
}
std::expected<PolyI, StarException> jsonToPolyI(Json const& v) {
  auto vertices = v.iterateArray();
  if (!vertices)
    return std::unexpected(std::move(vertices.error()));
  PolyI poly;
  for (auto const& vertex : *vertices) {
    auto converted = jsonToVec2I(vertex);
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    poly.add(*converted);
  }
  return fixInsideOutPoly(std::move(poly));
}
Json jsonFromPolyF(PolyF const& poly) {
  JsonArray result;
  for (auto const& vertex : poly.vertexes())
    result.push_back(JsonArray{vertex[0], vertex[1]});
  return result;
}
std::expected<Line2F, StarException> jsonToLine2F(Json const& v) {
  auto lower = v.get(0);
  if (!lower)
    return std::unexpected(std::move(lower.error()));
  auto upper = v.get(1);
  if (!upper)
    return std::unexpected(std::move(upper.error()));
  auto min = jsonToVec2F(*lower);
  if (!min)
    return std::unexpected(std::move(min.error()));
  auto max = jsonToVec2F(*upper);
  if (!max)
    return std::unexpected(std::move(max.error()));
  return Line2F(*min, *max);
}
Json jsonFromLine2F(Line2F const& line) { return JsonArray{jsonFromVec2F(line.min()), jsonFromVec2F(line.max())}; }
std::expected<Mat3F, StarException> jsonToMat3F(Json const& v) {
  Vec3F columns[3];
  for (std::size_t i = 0; i < 3; ++i) {
    auto item = v.get(i);
    if (!item)
      return std::unexpected(std::move(item.error()));
    auto column = jsonToVec3F(*item);
    if (!column)
      return std::unexpected(std::move(column.error()));
    columns[i] = *column;
  }
  return Mat3F(columns[0], columns[1], columns[2]);
}
Json jsonFromMat3F(Mat3F const& v) { return JsonArray{jsonFromVec3F(v[0]), jsonFromVec3F(v[1]), jsonFromVec3F(v[2])}; }

std::expected<StringList, StarException> jsonToStringList(Json const& v) { return jsonToList<String>(v, [](Json const& j) { return j.toString(); }); }
std::expected<List<float>, StarException> jsonToFloatList(Json const& v) { return jsonToList<float>(v, [](Json const& j) { return j.toFloat(); }); }
std::expected<List<int>, StarException> jsonToIntList(Json const& v) { return jsonToList<int>(v, [](Json const& j) { return j.toInt(); }); }
std::expected<List<Vec2I>, StarException> jsonToVec2IList(Json const& v) { return jsonToList<Vec2I>(v, jsonToVec2I); }
std::expected<List<Vec2U>, StarException> jsonToVec2UList(Json const& v) { return jsonToList<Vec2U>(v, jsonToVec2U); }
std::expected<List<Vec2F>, StarException> jsonToVec2FList(Json const& v) { return jsonToList<Vec2F>(v, jsonToVec2F); }
std::expected<List<Vec4B>, StarException> jsonToVec4BList(Json const& v) { return jsonToList<Vec4B>(v, jsonToVec4B); }
std::expected<List<Color>, StarException> jsonToColorList(Json const& v) { return jsonToList<Color>(v, jsonToColor); }
std::expected<StringSet, StarException> jsonToStringSet(Json const& v) { return jsonToSet<StringSet>(v, [](Json const& j) { return j.toString(); }); }
std::expected<List<Directives>, StarException> jsonToDirectivesList(Json const& v) {
  return jsonToList<Directives>(v, [](Json const& j) -> std::expected<Directives, StarException> {
    auto text = j.toString();
    if (!text)
      return std::unexpected(std::move(text.error()));
    return Directives(*text);
  });
}
Json jsonFromStringList(List<String> const& v) {
  JsonArray result;
  for (auto const& entry : v)
    result.push_back(entry);
  return result;
}
Json jsonFromStringSet(StringSet const& v) {
  JsonArray result;
  for (auto const& entry : v)
    result.push_back(entry);
  return result;
}
Json jsonFromDirectivesList(List<Directives> const& v) {
  JsonArray result;
  for (auto const& entry : v)
    if (entry)
      if (auto text = entry.maybeString())
        result.push_back(text->get());
  return result;
}

std::expected<Json, StarException> weightedChoiceFromJson(RandomService& random, Json const& source, Json const& default_) {
  if (source.isNull())
    return default_;
  if (source.type() != Json::Type::Array)
    return std::unexpected(StarException("Json of array type expected."));
  auto array = source.arrayPtr();
  if (!array)
    return std::unexpected(std::move(array.error()));
  List<std::pair<float, Json>> options;
  float sum = 0;
  for (std::size_t idx = 0; idx < (*array)->size(); ++idx) {
    float weight = 1;
    Json entry = (**array)[idx];
    if (entry.type() == Json::Type::Int || entry.type() == Json::Type::Float) {
      auto number = entry.toDouble();
      if (!number)
        return std::unexpected(std::move(number.error()));
      weight = *number;
      if (++idx >= (*array)->size())
        return std::unexpected(StarException("Weighted companion cube cannot cry."));
      entry = (**array)[idx];
    }
    sum += weight;
    options.push_back(std::pair<float, Json>{weight, std::move(entry)});
  }
  if (options.empty())
    return default_;
  float choice = random.randf() * sum;
  for (auto const& entry : options) {
    if (entry.first >= choice)
      return entry.second;
    choice -= entry.first;
  }
  return options[options.size() - 1].second;
}

std::expected<Json, StarException> binnedChoiceFromJson(Json const& bins, float target, Json const& def) {
  auto entries = bins.iterateArray();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  // Extract checked keys before sorting: a comparator cannot propagate errors.
  List<std::pair<float, Json>> ordered;
  for (auto const& entry : *entries) {
    auto minimum = entry.getFloat(0);
    if (!minimum)
      return std::unexpected(std::move(minimum.error()));
    ordered.push_back({*minimum, entry});
  }
  sortByComputedValue(ordered, [](auto const& entry) { return -entry.first; });
  for (auto const& entry : ordered)
    if (entry.first <= target)
      return entry.second.get(1);
  return def;
}

template <> std::expected<WeightedPool<int>, StarException> jsonToWeightedPool(Json const& source) {
  return jsonToWeightedPool<int>(source, [](Json const& v) { return v.toInt(); });
}
template <> std::expected<WeightedPool<unsigned>, StarException> jsonToWeightedPool(Json const& source) {
  return jsonToWeightedPool<unsigned>(source, [](Json const& v) { return v.toUInt(); });
}
template <> std::expected<WeightedPool<float>, StarException> jsonToWeightedPool(Json const& source) {
  return jsonToWeightedPool<float>(source, [](Json const& v) { return v.toFloat(); });
}
template <> std::expected<WeightedPool<double>, StarException> jsonToWeightedPool(Json const& source) {
  return jsonToWeightedPool<double>(source, [](Json const& v) { return v.toDouble(); });
}
template <> std::expected<WeightedPool<String>, StarException> jsonToWeightedPool(Json const& source) {
  return jsonToWeightedPool<String>(source, [](Json const& v) { return v.toString(); });
}
template <> std::expected<WeightedPool<JsonArray>, StarException> jsonToWeightedPool(Json const& source) {
  return jsonToWeightedPool<JsonArray>(source, [](Json const& v) { return v.toArray(); });
}
template <> std::expected<WeightedPool<JsonObject>, StarException> jsonToWeightedPool(Json const& source) {
  return jsonToWeightedPool<JsonObject>(source, [](Json const& v) { return v.toObject(); });
}

}
