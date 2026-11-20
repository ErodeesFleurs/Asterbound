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

std::expected<std::size_t, StarException> json_to_size(Json const& v) {
  if (v.is_null())
    return NPos;
  if (!v.can_convert(Json::Type::Int))
    return std::unexpected(JsonException("Json not an int in jsonToSize"));
  return v.to_uint();
}

Json json_from_size(std::size_t s) {
  return s == NPos ? Json() : Json(s);
}

namespace {
  template <typename Component, std::size_t dimension>
    requires (std::integral<Component> || std::floating_point<Component>)
  std::expected<Vector<Component, dimension>, StarException> json_vector(Json const& source, std::string_view diagnostic) {
    auto components = source.array_ptr();
    if (!components || (*components)->size() != dimension)
      return std::unexpected(JsonException(std::string(diagnostic)));
    Vector<Component, dimension> result;
    for (std::size_t i = 0; i < dimension; ++i) {
      auto const& component = (**components)[i];
      auto number = [&] {
        if constexpr (std::floating_point<Component>)
          return component.to_double();
        else
          return component.to_int(); // Unsigned vectors also narrow from signed integers.
      }();
      if (!number)
        return std::unexpected(std::move(number.error()));
      result[i] = static_cast<Component>(*number);
    }
    return result;
  }
}

std::expected<Vec2D, StarException> json_to_vec2d(Json const& v) {
  return json_vector<double, 2>(v, "Json not an array of size 2 in jsonToVec2D");
}
std::expected<Vec2F, StarException> json_to_vec2f(Json const& v) {
  return json_vector<float, 2>(v, "Json not an array of size 2 in jsonToVec2F");
}
std::expected<Vec2I, StarException> json_to_vec2i(Json const& v) {
  return json_vector<int, 2>(v, "Json not an array of size 2 in jsonToVec2I");
}
std::expected<Vec2U, StarException> json_to_vec2u(Json const& v) {
  return json_vector<unsigned, 2>(v, "Json not an array of size 2 in jsonToVec2I");
}
std::expected<Vec2B, StarException> json_to_vec2b(Json const& v) {
  return json_vector<std::uint8_t, 2>(v, "Json not an array of size 2 in jsonToVec2B");
}
std::expected<Vec3D, StarException> json_to_vec3d(Json const& v) {
  return json_vector<double, 3>(v, "Json not an array of size size 3 in jsonToVec3D");
}
std::expected<Vec3F, StarException> json_to_vec3f(Json const& v) {
  return json_vector<float, 3>(v, "Json not an array of size 3 in jsonToVec3D");
}
std::expected<Vec3I, StarException> json_to_vec3i(Json const& v) {
  return json_vector<int, 3>(v, "Json not an array of size 3 in jsonToVec3I");
}
std::expected<Vec3B, StarException> json_to_vec3b(Json const& v) {
  return json_vector<std::uint8_t, 3>(v, "Json not an array of size 3 in jsonToVec3B");
}
std::expected<Vec4B, StarException> json_to_vec4b(Json const& v) {
  return json_vector<std::uint8_t, 4>(v, "Json not an array of size 4 in jsonToVec4B");
}
std::expected<Vec4I, StarException> json_to_vec4i(Json const& v) {
  return json_vector<int, 4>(v, "Json not an array of size 4 in jsonToVec4B");
}
std::expected<Vec4F, StarException> json_to_vec4f(Json const& v) {
  return json_vector<float, 4>(v, "Json not an array of size 4 in jsonToVec4B");
}

Json json_from_vec2f(Vec2F const& v) { return JsonArray{v[0], v[1]}; }
Json json_from_vec2i(Vec2I const& v) { return JsonArray{v[0], v[1]}; }
Json json_from_vec2u(Vec2U const& v) { return JsonArray{v[0], v[1]}; }
Json json_from_vec2b(Vec2B const& v) { return JsonArray{v[0], v[1]}; }
Json json_from_vec3f(Vec3F const& v) { return JsonArray{v[0], v[1], v[2]}; }
Json json_from_vec3i(Vec3I const& v) { return JsonArray{v[0], v[1], v[2]}; }

namespace {
  template <typename Rect, typename Vec, typename Getter, typename Converter>
  std::expected<Rect, StarException> json_rect(Json const& v, Getter getter, Converter& converter, std::string_view name) {
    if (v.type() != Json::Type::Array)
      return std::unexpected(JsonException(Star::format("Json not an array in {}", name)));
    auto size = v.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    if (*size != 4 && *size != 2)
      return std::unexpected(JsonException(Star::format("Json not an array of proper size in {}", name)));
    if (*size == 4) {
      Vec lower_left, upper_right;
      for (std::size_t i = 0; i < 4; ++i) {
        auto value = getter(v, i);
        if (!value)
          return std::unexpected(std::move(value.error()));
        (i < 2 ? lower_left : upper_right)[i % 2] = *value;
      }
      return Rect(lower_left, upper_right);
    }
    auto inner = [&]() -> std::expected<Rect, StarException> {
      auto lower = v.get(0);
      if (!lower)
        return std::unexpected(std::move(lower.error()));
      auto upper = v.get(1);
      if (!upper)
        return std::unexpected(std::move(upper.error()));
      auto lower_left = converter(*lower);
      if (!lower_left)
        return std::unexpected(std::move(lower_left.error()));
      auto upper_right = converter(*upper);
      if (!upper_right)
        return std::unexpected(std::move(upper_right.error()));
      return Rect(*lower_left, *upper_right);
    }();
    if (!inner && inner.error().template is<JsonException>())
      return std::unexpected(JsonException(Star::format("Inner position not well formed in {}: {}", name, Star::output_exception(ExceptionDiagnostic(inner.error()), true))));
    return inner;
  }
}

std::expected<RectD, StarException> json_to_rectd(Json const& v) {
  return json_rect<RectD, Vec2D>(v, [](Json const& j, std::size_t i) { return j.get_double(i); }, json_to_vec2d, "jsonToRectD");
}
std::expected<RectF, StarException> json_to_rectf(Json const& v) {
  auto result = json_to_rectd(v);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return RectF(*result);
}
std::expected<RectI, StarException> json_to_recti(Json const& v) {
  return json_rect<RectI, Vec2I>(v, [](Json const& j, std::size_t i) { return j.get_int(i); }, json_to_vec2i, "jsonToRectI");
}
std::expected<RectU, StarException> json_to_rectu(Json const& v) {
  // The first component historically uses signed conversion, the others unsigned.
  return json_rect<RectU, Vec2U>(v, [](Json const& j, std::size_t i) -> std::expected<std::uint64_t, StarException> {
    if (i != 0)
      return j.get_uint(i);
    auto value = j.get_int(i);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return static_cast<std::uint64_t>(*value);
  }, json_to_vec2u, "jsonToRectU");
}
Json json_from_rectd(RectD const& r) { return JsonArray{r.xMin(), r.yMin(), r.xMax(), r.yMax()}; }
Json json_from_rectf(RectF const& r) { return JsonArray{r.xMin(), r.yMin(), r.xMax(), r.yMax()}; }
Json json_from_recti(RectI const& r) { return JsonArray{r.xMin(), r.yMin(), r.xMax(), r.yMax()}; }
Json json_from_rectu(RectU const& r) { return JsonArray{r.xMin(), r.yMin(), r.xMax(), r.yMax()}; }

std::expected<Color, StarException> json_to_color(Json const& v) {
  if (v.type() == Json::Type::Array) {
    auto size = v.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    if (*size != 3 && *size != 4)
      return std::unexpected(JsonException("Json not an array of size 3 or 4 in jsonToColor"));
    Vec4B channels(0, 0, 0, 255);
    for (std::size_t i = 0; i < *size; ++i) {
      auto channel = v.get_int(i);
      if (!channel)
        return std::unexpected(std::move(channel.error()));
      channels[i] = *channel;
    }
    return Color::rgba(channels[0], channels[1], channels[2], channels[3]);
  }
  if (v.type() == Json::Type::String) {
    auto text = v.to_string();
    if (!text)
      return std::unexpected(std::move(text.error()));
    return Color::create(*text);
  }
  return std::unexpected(JsonException(Star::format("Json of type {} cannot be converted to color", v.type_name())));
}
Json json_from_color(Color const& color) {
  JsonArray result{color.red(), color.green(), color.blue()};
  auto alpha = color.alpha();
  if (alpha < 255)
    result.push_back(alpha);
  return result;
}

namespace {
  template <typename Coordinate, typename Converter>
    requires requires(Converter& convert, Json const& vertex) {
      { convert(vertex) } -> std::same_as<std::expected<typename Polygon<Coordinate>::Vertex, StarException>>;
    }
  std::expected<Polygon<Coordinate>, StarException> json_polygon(Json const& source, Converter& convert) {
    auto vertices = source.array_ptr();
    if (!vertices)
      return std::unexpected(std::move(vertices.error()));
    Polygon<Coordinate> polygon;
    polygon.vertexes().reserve((*vertices)->size());
    for (auto const& vertex : **vertices) {
      auto converted = convert(vertex);
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      polygon.add(*converted);
    }
    return fix_inside_out_poly(std::move(polygon));
  }
}

std::expected<PolyD, StarException> json_to_polyd(Json const& source) {
  return json_polygon<double>(source, json_to_vec2d);
}
std::expected<PolyF, StarException> json_to_polyf(Json const& source) {
  return json_polygon<float>(source, json_to_vec2f);
}
std::expected<PolyI, StarException> json_to_polyi(Json const& source) {
  return json_polygon<int>(source, json_to_vec2i);
}
Json json_from_polyf(PolyF const& poly) {
  JsonArray result;
  result.reserve(poly.sides());
  for (auto const& vertex : poly.vertexes())
    result.push_back(JsonArray{vertex[0], vertex[1]});
  return result;
}
std::expected<Line2F, StarException> json_to_line2f(Json const& v) {
  auto lower = v.get(0);
  if (!lower)
    return std::unexpected(std::move(lower.error()));
  auto upper = v.get(1);
  if (!upper)
    return std::unexpected(std::move(upper.error()));
  auto min = json_to_vec2f(*lower);
  if (!min)
    return std::unexpected(std::move(min.error()));
  auto max = json_to_vec2f(*upper);
  if (!max)
    return std::unexpected(std::move(max.error()));
  return Line2F(*min, *max);
}
Json json_from_line2f(Line2F const& line) { return JsonArray{json_from_vec2f(line.min()), json_from_vec2f(line.max())}; }
std::expected<Mat3F, StarException> json_to_mat3f(Json const& v) {
  std::array<Vec3F, 3> rows;
  for (std::size_t i = 0; i < 3; ++i) {
    auto item = v.get(i);
    if (!item)
      return std::unexpected(std::move(item.error()));
    auto row = json_to_vec3f(*item);
    if (!row)
      return std::unexpected(std::move(row.error()));
    rows[i] = *row;
  }
  return Mat3F(rows[0], rows[1], rows[2]);
}
Json json_from_mat3f(Mat3F const& v) { return JsonArray{json_from_vec3f(v[0]), json_from_vec3f(v[1]), json_from_vec3f(v[2])}; }

std::expected<StringList, StarException> json_to_string_list(Json const& v) { return json_to_list<String>(v, [](Json const& j) { return j.to_string(); }); }
std::expected<List<float>, StarException> json_to_float_list(Json const& v) { return json_to_list<float>(v, [](Json const& j) { return j.to_float(); }); }
std::expected<List<int>, StarException> json_to_int_list(Json const& v) { return json_to_list<int>(v, [](Json const& j) { return j.to_int(); }); }
std::expected<List<Vec2I>, StarException> json_to_vec2i_list(Json const& v) { return json_to_list<Vec2I>(v, json_to_vec2i); }
std::expected<List<Vec2U>, StarException> json_to_vec2u_list(Json const& v) { return json_to_list<Vec2U>(v, json_to_vec2u); }
std::expected<List<Vec2F>, StarException> json_to_vec2f_list(Json const& v) { return json_to_list<Vec2F>(v, json_to_vec2f); }
std::expected<List<Vec4B>, StarException> json_to_vec4b_list(Json const& v) { return json_to_list<Vec4B>(v, json_to_vec4b); }
std::expected<List<Color>, StarException> json_to_color_list(Json const& v) { return json_to_list<Color>(v, json_to_color); }
std::expected<StringSet, StarException> json_to_string_set(Json const& v) { return json_to_set<StringSet>(v, [](Json const& j) { return j.to_string(); }); }
std::expected<List<Directives>, StarException> json_to_directives_list(Json const& v) {
  return json_to_list<Directives>(v, [](Json const& j) -> std::expected<Directives, StarException> {
    auto text = j.to_string();
    if (!text)
      return std::unexpected(std::move(text.error()));
    return Directives(*text);
  });
}
Json json_from_string_list(List<String> const& v) {
  JsonArray result;
  result.reserve(v.size());
  for (auto const& entry : v)
    result.push_back(entry);
  return result;
}
Json json_from_string_set(StringSet const& v) {
  JsonArray result;
  result.reserve(v.size());
  for (auto const& entry : v)
    result.push_back(entry);
  return result;
}
Json json_from_directives_list(List<Directives> const& v) {
  JsonArray result;
  for (auto const& entry : v)
    if (entry)
      if (auto text = entry.maybe_string())
        result.push_back(text->get());
  return result;
}

std::expected<Json, StarException> weighted_choice_from_json(RandomService& random, Json const& source, Json const& default_) {
  if (source.is_null())
    return default_;
  if (source.type() != Json::Type::Array)
    return std::unexpected(StarException("Json of array type expected."));
  auto array = source.array_ptr();
  if (!array)
    return std::unexpected(std::move(array.error()));
  List<std::pair<float, Json>> options;
  float sum = 0;
  for (std::size_t idx = 0; idx < (*array)->size(); ++idx) {
    float weight = 1;
    Json entry = (**array)[idx];
    if (entry.type() == Json::Type::Int || entry.type() == Json::Type::Float) {
      auto number = entry.to_double();
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
  float choice = random.uniform_float() * sum;
  for (auto const& entry : options) {
    if (entry.first >= choice)
      return entry.second;
    choice -= entry.first;
  }
  return options[options.size() - 1].second;
}

std::expected<Json, StarException> binned_choice_from_json(Json const& bins, float target, Json const& def) {
  auto entries = bins.iterate_array();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  // Extract checked keys before sorting: a comparator cannot propagate errors.
  List<std::pair<float, Json>> ordered;
  for (auto const& entry : *entries) {
    auto minimum = entry.get_float(0);
    if (!minimum)
      return std::unexpected(std::move(minimum.error()));
    ordered.push_back({*minimum, entry});
  }
  sort_by_key(ordered, [](auto const& entry) { return -entry.first; });
  for (auto const& entry : ordered)
    if (entry.first <= target)
      return entry.second.get(1);
  return def;
}

template <> std::expected<WeightedPool<int>, StarException> json_to_weighted_pool(Json const& source) {
  return json_to_weighted_pool<int>(source, [](Json const& v) { return v.to_int(); });
}
template <> std::expected<WeightedPool<unsigned>, StarException> json_to_weighted_pool(Json const& source) {
  return json_to_weighted_pool<unsigned>(source, [](Json const& v) { return v.to_uint(); });
}
template <> std::expected<WeightedPool<float>, StarException> json_to_weighted_pool(Json const& source) {
  return json_to_weighted_pool<float>(source, [](Json const& v) { return v.to_float(); });
}
template <> std::expected<WeightedPool<double>, StarException> json_to_weighted_pool(Json const& source) {
  return json_to_weighted_pool<double>(source, [](Json const& v) { return v.to_double(); });
}
template <> std::expected<WeightedPool<String>, StarException> json_to_weighted_pool(Json const& source) {
  return json_to_weighted_pool<String>(source, [](Json const& v) { return v.to_string(); });
}
template <> std::expected<WeightedPool<JsonArray>, StarException> json_to_weighted_pool(Json const& source) {
  return json_to_weighted_pool<JsonArray>(source, [](Json const& v) { return v.to_array(); });
}
template <> std::expected<WeightedPool<JsonObject>, StarException> json_to_weighted_pool(Json const& source) {
  return json_to_weighted_pool<JsonObject>(source, [](Json const& v) { return v.to_object(); });
}

}
