import std;
import star.lua;
import star.format;
import star.json;
import star.xx_hash;
import star.json_extra;
import star.utility_lua_bindings;
import star.data_stream;
import star.uuid;
import star.random;
import star.math_common;
import star.bi_map;
import star.interpolation;
import star.perlin;
import star.logging;
import star.rpc_promise;
import star.lua_converters;
import star.string;
import star.unicode;
import star.string_view;
import star.lexical_cast;
import star.vector;
import star.list;
import star.image_processing;
import star.hash;
import star.thread;
import star.directives;
import star.text;
import star.time;
import star.version;

import star.path_embed;
import star.data_stream_devices;
import star.static_random;
namespace Star {

#pragma float_control(precise, on, push)
[[gnu::noinline]] static double interpolate_static_random_double_overflow(double min, double max, double unit) {
  return (1.0 - unit) * min + unit * max;
}
#pragma float_control(pop)

static double static_random_unit(std::uint64_t hash) {
  return (hash & 0x7fffffffffffffff) / 9223372036854775808.0;
}

static double interpolate_static_random_double(double min, double max, std::uint64_t hash) {
  auto unit = static_random_unit(hash);
  auto width = max - min;
  return is_finite(width) ? unit * width + min : interpolate_static_random_double_overflow(min, max, unit);
}

struct LuaRandomSource : RandomSource {
  LuaRandomSource(RandomService& owner, std::uint64_t seed) : RandomSource(seed), owner(owner) {}
  RandomService& owner;
};

template <>
struct LuaConverter<LuaRandomSource> : LuaUserDataConverter<LuaRandomSource> {};

template <>
struct LuaUserDataMethods<LuaRandomSource> {
  [[nodiscard]] static std::expected<LuaMethods<LuaRandomSource>, StarException> make() {
    LuaMethods<LuaRandomSource> methods;

    if (auto result = methods.register_method("init", [](LuaRandomSource& source, std::optional<std::uint64_t> seed)
        { source.seed(seed ? *seed : source.owner.next_u64()); }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.register_method("addEntropy", [](LuaRandomSource& source, std::optional<std::uint64_t> seed)
        { source.mix_seed(seed ? *seed : source.owner.next_u64()); }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = methods.register_method_with_signature<std::uint32_t, LuaRandomSource&>("randu32", std::mem_fn(&RandomSource::next_u32)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.register_method_with_signature<std::uint64_t, LuaRandomSource&>("randu64", std::mem_fn(&RandomSource::next_u64)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.register_method_with_signature<std::int32_t, LuaRandomSource&>("randi32", std::mem_fn(&RandomSource::next_i32)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.register_method_with_signature<std::int64_t, LuaRandomSource&>("randi64", std::mem_fn(&RandomSource::next_i64)); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = methods.register_method("randf", [](LuaRandomSource& source, std::optional<float> arg1, std::optional<float> arg2)
        -> std::expected<float, StarException> {
          if (arg1 && arg2)
            return source.uniform_float(*arg1, *arg2);
          return source.uniform_float();
        }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.register_method("randd", [](LuaRandomSource& source, std::optional<double> arg1, std::optional<double> arg2)
        -> std::expected<double, StarException> {
          if (arg1 && arg2)
            return source.uniform_double(*arg1, *arg2);
          return source.uniform_double();
        }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = methods.register_method_with_signature<bool, LuaRandomSource&>("randb", std::mem_fn(&RandomSource::next_bool)); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = methods.register_method("randInt", [](LuaRandomSource& source, std::int64_t arg1, std::optional<std::int64_t> arg2)
        -> std::expected<std::int64_t, StarException> {
          if (arg2)
            return source.uniform_int(arg1, *arg2);
          return source.uniform_int(arg1);
        }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.register_method("randUInt", [](LuaRandomSource& source, std::uint64_t arg1, std::optional<std::uint64_t> arg2)
        -> std::expected<std::uint64_t, StarException> {
          if (arg2)
            return source.uniform_uint(arg1, *arg2);
          return source.uniform_uint(arg1);
        }); !result)
      return std::unexpected(std::move(result.error()));
    return methods;
  }
};

template <>
struct LuaConverter<PerlinF> : LuaUserDataConverter<PerlinF> {};

template <>
struct LuaUserDataMethods<PerlinF> {
  [[nodiscard]] static std::expected<LuaMethods<PerlinF>, StarException> make() {
    LuaMethods<PerlinF> methods;

    auto result = methods.register_method("get",
        [](PerlinF& perlin, float x, std::optional<float> y, std::optional<float> z) {
          if (y && z)
            return perlin.get(x, *y, *z);
          else if (y)
            return perlin.get(x, *y);
          else
            return perlin.get(x);
        });
    if (!result)
      return std::unexpected(std::move(result.error()));

    return methods;
  }
};

std::expected<String, StarException> LuaBindings::format_lua(String const& format, std::span<LuaValue const> args) {
  if (auto valid = format.size(); !valid) return std::unexpected(std::move(valid.error()));
  std::string_view rest = format.utf8();
  auto pos = rest.find('%');
  if (pos == rest.npos) return format;
  std::string output;
  output.reserve(rest.size());
  while (pos != rest.npos) {
    output.append(rest.substr(0, pos));
    rest.remove_prefix(pos + 1);
    if (rest.empty()) return std::unexpected(StarException("No specifier following '%'"));
    if (rest.front() == '%') output.push_back('%');
    else if (rest.front() == 's') {
      auto text = print_lua_value(args.empty() ? lua_nil : args.front());
      if (!text) return std::unexpected(std::move(text.error()));
      output.append(text->utf8());
      if (!args.empty()) args = args.subspan(1);
    } else {
      auto point = Utf8Cursor(rest).read();
      if (!point) return std::unexpected(std::move(point.error()));
      return std::unexpected(StarException::format("Improper lua log format specifier {}", static_cast<char>(*point)));
    }
    rest.remove_prefix(1);
    pos = rest.find('%');
  }
  output.append(rest);
  return String(std::move(output));
}

namespace {
std::expected<void, StarException> log_message(Logger& logger, LogLevel level, String const& str, LuaVariadic<LuaValue> const& args) {
  auto text = LuaBindings::format_lua(str, args);
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto logged = logger.log(level, std::string_view(text->utf8()).substr(0, text->utf8().find('\0')));
  if (!logged)
    return std::unexpected(LuaHostError(ExceptionDiagnostic(logged.error())));
  return {};
}

std::expected<void, StarException> set_log_map(LogMap& log_map, String const& key, String const& value, LuaVariadic<LuaValue> const& args) {
  auto text = LuaBindings::format_lua(value, args);
  if (!text)
    return std::unexpected(std::move(text.error()));
  log_map.set_value(key, std::move(*text));
  return {};
}

std::expected<LuaValue, StarException> interpolate_sin_ease(LuaEngine& engine, double offset, LuaValue const& value1, LuaValue const& value2) {
  auto first_number = engine.lua_maybe_to<double>(value1);
  if (!first_number)
    return std::unexpected(std::move(first_number.error()));
  if (*first_number) {
    auto second_number = engine.lua_to<double>(value2);
    if (!second_number)
      return std::unexpected(std::move(second_number.error()));
    return engine.lua_from(sin_ease(offset, **first_number, *second_number));
  }

  auto first_vector = engine.lua_to<Vec2F>(value1);
  if (!first_vector)
    return std::unexpected(std::move(first_vector.error()));
  auto second_vector = engine.lua_to<Vec2F>(value2);
  if (!second_vector)
    return std::unexpected(std::move(second_vector.error()));
  return engine.lua_from<Vec2F>(sin_ease(offset, *first_vector, *second_vector));
}

std::expected<std::uint64_t, StarException> hash_lua_values(LuaVariadic<LuaValue> const& values) {
    XXHash64 hash;

    for (auto const& value : values) {
      if (std::holds_alternative<LuaBoolean>(value))
        hash_append(hash, std::get<LuaBoolean>(value));
      else if (std::holds_alternative<LuaInt>(value))
        hash_append(hash, std::get<LuaInt>(value));
      else if (std::holds_alternative<LuaFloat>(value))
        hash_append(hash, std::get<LuaFloat>(value));
      else if (std::holds_alternative<LuaString>(value)) {
        auto text = std::get<LuaString>(value).view().utf8();
        hash_append(hash, text.substr(0, text.find('\0')));
      }
      else
        return std::unexpected(LuaException("Unhashable lua type passed to staticRandomXX binding"));
    }

    return hash.digest();
}
} // namespace

std::expected<LuaCallbacks, StarException> LuaBindings::make_utility_callbacks(RandomService& random, Logger& logger, LogMap& log_map) {
  LuaCallbacks callbacks;
  if (auto result = callbacks.register_callback("buildInfo", []() -> Json {
        return JsonObject{{"version", base_game_version}, {"architecture", target_architecture},
            {"product", "Asterbound"}, {"productVersion", product_version},
            {"assetVersion", bundled_assets_version}};
      }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("nrand", [&random](std::optional<double> const& stdev, std::optional<double> const& mean) {
        return random.normal_double(stdev.value_or(1.0), mean.value_or(0));
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("makeUuid", [&random]() { return Uuid(random).hex(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("logInfo", [&logger](String const& str, LuaVariadic<LuaValue> const& args) { return log_message(logger, LogLevel::Info, str, args); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("logWarn", [&logger](String const& str, LuaVariadic<LuaValue> const& args) { return log_message(logger, LogLevel::Warn, str, args); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("logError", [&logger](String const& str, LuaVariadic<LuaValue> const& args) { return log_message(logger, LogLevel::Error, str, args); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("setLogMap", [&log_map](String const& key, String const& value, LuaVariadic<LuaValue> const& args) {
        return set_log_map(log_map, key, value, args);
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("parseJson", [](String const& text) { return Json::parse(text); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("printJson", [](Json const& value, std::optional<int> pretty) { return value.repr(pretty.value_or(0)); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("print", print_lua_value); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("interpolateSinEase", interpolate_sin_ease); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("replaceTags", [](String const& text, StringMap<String> const& tags) { return text.replace_tags(tags); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("formatTags", [](String const& text, StringMap<String> const& tags) -> std::expected<String, StarException> {
        auto formatted = Star::format_tags(text.utf8(), [&tags](std::string_view name) -> std::optional<std::string_view> {
          if (auto found = tags.find_ref(name)) return found->get().utf8();
          return {};
        });
        if (!formatted)
          return std::unexpected(std::move(formatted.error()));
        return String(std::move(*formatted));
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("stripEscapeCodes", [](String const& text) { return Text::strip_escape_codes(text); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("parseJsonSequence", [](String const& json) { return Json::parse_sequence(json); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("jsonMerge", [](Json const& a, Json const& b) { return json_merge(a, b); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("jsonEqual", [](Json const& a, Json const& b) { return a == b; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("jsonQuery", [](Json const& json, String const& path, Json const& def) { return json.query(path, def); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("makeRandomSource", [&random](std::optional<std::uint64_t> seed) { return LuaRandomSource(random, seed ? *seed : random.next_u64()); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("makePerlinSource", [](Json const& config) { return PerlinF::create(config); }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.copy_callback("parseJson", "jsonFromString"); !result) // SE compat
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("staticRandomI32",
      [](LuaVariadic<LuaValue> const& hash_values) -> std::expected<std::int32_t, StarException> {
        auto hash = hash_lua_values(hash_values);
        if (!hash)
          return std::unexpected(std::move(hash.error()));
        return (std::int32_t)*hash;
      }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("staticRandomI32Range",
      [](std::int32_t min, std::int32_t max, LuaVariadic<LuaValue> const& hash_values) -> std::expected<std::int32_t, StarException> {
        if (max < min)
          return std::unexpected(LuaException("Maximum bound in staticRandomI32Range must be >= minimum bound!"));
        auto hash = hash_lua_values(hash_values);
        if (!hash)
          return std::unexpected(std::move(hash.error()));
        auto width = static_cast<std::uint32_t>(max) - static_cast<std::uint32_t>(min);
        auto offset = static_random_bounded(*hash, width);
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(min) + static_cast<std::uint32_t>(offset));
      }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("staticRandomDouble",
      [](LuaVariadic<LuaValue> const& hash_values) -> std::expected<double, StarException> {
        auto hash = hash_lua_values(hash_values);
        if (!hash)
          return std::unexpected(std::move(hash.error()));
        return static_random_unit(*hash);
      }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("staticRandomDoubleRange",
      [](double min, double max, LuaVariadic<LuaValue> const& hash_values) -> std::expected<double, StarException> {
        if (max < min)
          return std::unexpected(LuaException("Maximum bound in staticRandomDoubleRange must be >= minimum bound!"));
        if (!is_finite(min) || !is_finite(max))
          return std::unexpected(LuaException("Bounds in staticRandomDoubleRange must be finite!"));
        auto hash = hash_lua_values(hash_values);
        if (!hash)
          return std::unexpected(std::move(hash.error()));
        return interpolate_static_random_double(min, max, *hash);
      }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("millisecondsSinceEpoch",
      []() { return Time::milliseconds_since_epoch(); }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("monotonicMilliseconds",
      []() { return Time::monotonic_milliseconds(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("monotonicTime",
      []() { return Time::monotonic_time(); }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("makePromise",
      []() -> std::pair<RpcPromise<Json>, std::shared_ptr<AutoFailRpcPromiseKeeper<Json>>> {
        auto [promise, keeper] = RpcPromise<Json>::make_pair();
        return {std::move(promise), std::make_shared<AutoFailRpcPromiseKeeper<Json>>(std::move(keeper))};
      }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.register_callback("embedData",
      [](String const& data, String const& compression) -> std::expected<String, StarException> {
        auto method = embedded_compression_method(compression);
        if (!method) return std::unexpected(std::move(method.error()));
        std::string output = "/opensb_data;bin;";
        if (auto encoded = append_embedded_data(output, std::span(data.utf8()), *method); !encoded)
          return std::unexpected(std::move(encoded.error()));
        return String(std::move(output));
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("embedJson",
      [](Json const& data, String const& compression) -> std::expected<String, StarException> {
        auto method = embedded_compression_method(compression);
        if (!method) return std::unexpected(std::move(method.error()));
        DataStreamBuffer stream;
        if (auto written = stream.write(data); !written)
          return std::unexpected(std::move(written.error()));
        std::string output = "/opensb_data;json;";
        if (auto encoded = append_embedded_data(output, stream.span(), *method); !encoded)
          return std::unexpected(std::move(encoded.error()));
        return String(std::move(output));
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("embedText",
      [](String const& data) -> std::expected<String, StarException> {
        std::string output = "/opensb_data;text;";
        if (auto escaped = append_escaped_text(output, data.utf8()); !escaped)
          return std::unexpected(std::move(escaped.error()));
        return String(std::move(output));
      }); !result)
    return std::unexpected(std::move(result.error()));

  return callbacks;
}

}
