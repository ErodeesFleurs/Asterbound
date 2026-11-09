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
import star.bi_map;
import star.interpolation;
import star.perlin;
import star.logging;
import star.rpc_promise;
import star.lua_converters;
import star.string;
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

import star.static_random;
namespace Star {

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

    if (auto result = methods.registerMethod("init", [](LuaRandomSource& source, std::optional<std::uint64_t> seed)
        { source.init(seed ? *seed : source.owner.randu64()); }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.registerMethod("addEntropy", [](LuaRandomSource& source, std::optional<std::uint64_t> seed)
        { source.addEntropy(seed ? *seed : source.owner.randu64()); }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = methods.registerMethodWithSignature<std::uint32_t, LuaRandomSource&>("randu32", std::mem_fn(&RandomSource::randu32)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.registerMethodWithSignature<std::uint64_t, LuaRandomSource&>("randu64", std::mem_fn(&RandomSource::randu64)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.registerMethodWithSignature<std::int32_t, LuaRandomSource&>("randi32", std::mem_fn(&RandomSource::randi32)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.registerMethodWithSignature<std::int64_t, LuaRandomSource&>("randi64", std::mem_fn(&RandomSource::randi64)); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = methods.registerMethod("randf", [](LuaRandomSource& source, std::optional<float> arg1, std::optional<float> arg2)
        -> std::expected<float, StarException> {
          if (arg1 && arg2)
            return source.randf(*arg1, *arg2);
          return source.randf();
        }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.registerMethod("randd", [](LuaRandomSource& source, std::optional<double> arg1, std::optional<double> arg2)
        -> std::expected<double, StarException> {
          if (arg1 && arg2)
            return source.randd(*arg1, *arg2);
          return source.randd();
        }); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = methods.registerMethodWithSignature<bool, LuaRandomSource&>("randb", std::mem_fn(&RandomSource::randb)); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = methods.registerMethod("randInt", [](LuaRandomSource& source, std::int64_t arg1, std::optional<std::int64_t> arg2)
        -> std::expected<std::int64_t, StarException> {
          if (arg2)
            return source.randInt(arg1, *arg2);
          return source.randInt(arg1);
        }); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = methods.registerMethod("randUInt", [](LuaRandomSource& source, std::uint64_t arg1, std::optional<std::uint64_t> arg2)
        -> std::expected<std::uint64_t, StarException> {
          if (arg2)
            return source.randUInt(arg1, *arg2);
          return source.randUInt(arg1);
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

    auto result = methods.registerMethod("get",
        [](PerlinF& perlinF, float x, std::optional<float> y, std::optional<float> z) {
          if (y && z)
            return perlinF.get(x, *y, *z);
          else if (y)
            return perlinF.get(x, *y);
          else
            return perlinF.get(x);
        });
    if (!result)
      return std::unexpected(std::move(result.error()));

    return methods;
  }
};

std::expected<String, StarException> LuaBindings::formatLua(String const& string, List<LuaValue> const& args) {
  auto argsIt = args.begin();
  auto argsEnd = args.end();
  auto popArg = [&argsIt, &argsEnd]() -> LuaValue {
    if (argsIt == argsEnd)
      return LuaNil;
    return *argsIt++;
  };

  auto begin = string.begin();
  if (!begin)
    return std::unexpected(std::move(begin.error()));
  auto stringIt = *begin;
  auto stringEnd = string.end();

  String result;

  while (stringIt != stringEnd) {
    if (*stringIt == '%') {
      auto next = stringIt;
      ++next;

      if (next == stringEnd)
        return std::unexpected(StarException("No specifier following '%'"));
      else if (*next == '%')
        result += "%";
      else if (*next == 's') {
        auto text = UtilityCallbacks::print(popArg());
        if (!text)
          return std::unexpected(std::move(text.error()));
        result += *text;
      } else {
        return std::unexpected(StarException::format("Improper lua log format specifier {}", (char)*next));
      }
      ++next;
      stringIt = next;
    } else {
      auto appended = result.append(*stringIt++);
      if (!appended)
        return std::unexpected(std::move(appended.error()));
    }
  }

  return result;
}

std::expected<LuaCallbacks, StarException> LuaBindings::makeUtilityCallbacks(RandomService& random, Logger& logger, LogMap& logMap) {
  LuaCallbacks callbacks;
  if (auto result = callbacks.registerCallback("buildInfo", []() -> Json {
        return JsonObject{{"version", StarVersionString}, {"architecture", StarArchitectureString}};
      }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallback("nrand", [&random](std::optional<double> const& stdev, std::optional<double> const& mean) {
        return UtilityCallbacks::nrand(random, stdev, mean);
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("makeUuid", [&random]() { return UtilityCallbacks::makeUuid(random); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("logInfo", [&logger](String const& str, LuaVariadic<LuaValue> const& args) { return UtilityCallbacks::logInfo(logger, str, args); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("logWarn", [&logger](String const& str, LuaVariadic<LuaValue> const& args) { return UtilityCallbacks::logWarn(logger, str, args); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("logError", [&logger](String const& str, LuaVariadic<LuaValue> const& args) { return UtilityCallbacks::logError(logger, str, args); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setLogMap", [&logMap](String const& key, String const& value, LuaVariadic<LuaValue> const& args) {
        return UtilityCallbacks::setLogMap(logMap, key, value, args);
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("parseJson", UtilityCallbacks::parseJson); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("printJson", UtilityCallbacks::printJson); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("print", UtilityCallbacks::print); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("interpolateSinEase", UtilityCallbacks::interpolateSinEase); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("replaceTags", UtilityCallbacks::replace_tags); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("formatTags", [](String const& text, StringMap<String> const& tags) -> std::expected<String, StarException> {
        std::vector<std::pair<std::string_view, std::string_view>> bindings;
        bindings.reserve(tags.size());
        for (auto const& entry : tags)
          bindings.emplace_back(entry.first.utf8(), entry.second.utf8());
        auto formatted = Star::format_tags(text.utf8(), bindings);
        if (!formatted)
          return std::unexpected(std::move(formatted.error()));
        return String(std::move(*formatted));
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("stripEscapeCodes", [](String const& text) { return Text::stripEscapeCodes(text); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("parseJsonSequence", [](String const& json) { return Json::parseSequence(json); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("jsonMerge", [](Json const& a, Json const& b) { return jsonMerge(a, b); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("jsonEqual", [](Json const& a, Json const& b) { return a == b; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("jsonQuery", [](Json const& json, String const& path, Json const& def) { return json.query(path, def); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("makeRandomSource", [&random](std::optional<std::uint64_t> seed) { return LuaRandomSource(random, seed ? *seed : random.randu64()); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("makePerlinSource", [](Json const& config) { return PerlinF::create(config); }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.copyCallback("parseJson", "jsonFromString"); !result) // SE compat
    return std::unexpected(std::move(result.error()));

  auto hash64LuaValues = [](LuaVariadic<LuaValue> const& values) -> std::expected<std::uint64_t, StarException> {
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
  };

  if (auto result = callbacks.registerCallback("staticRandomI32",
      [hash64LuaValues](LuaVariadic<LuaValue> const& hashValues) -> std::expected<std::int32_t, StarException> {
        auto hash = hash64LuaValues(hashValues);
        if (!hash)
          return std::unexpected(std::move(hash.error()));
        return (std::int32_t)*hash;
      }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallback("staticRandomI32Range",
      [hash64LuaValues](std::int32_t min, std::int32_t max, LuaVariadic<LuaValue> const& hashValues) -> std::expected<std::int32_t, StarException> {
        if (max < min)
          return std::unexpected(LuaException("Maximum bound in staticRandomI32Range must be >= minimum bound!"));
        auto hash = hash64LuaValues(hashValues);
        if (!hash)
          return std::unexpected(std::move(hash.error()));
        std::uint64_t denom = (std::uint64_t)(-1) / ((std::uint64_t)(max - min) + 1);
        return static_cast<std::int32_t>(*hash / denom + min);
      }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallback("staticRandomDouble",
      [hash64LuaValues](LuaVariadic<LuaValue> const& hashValues) -> std::expected<double, StarException> {
        auto hash = hash64LuaValues(hashValues);
        if (!hash)
          return std::unexpected(std::move(hash.error()));
        return (*hash & 0x7fffffffffffffff) / 9223372036854775808.0;
      }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallback("staticRandomDoubleRange",
      [hash64LuaValues](double min, double max, LuaVariadic<LuaValue> const& hashValues) -> std::expected<double, StarException> {
        if (max < min)
          return std::unexpected(LuaException("Maximum bound in staticRandomDoubleRange must be >= minimum bound!"));
        auto hash = hash64LuaValues(hashValues);
        if (!hash)
          return std::unexpected(std::move(hash.error()));
        return (*hash & 0x7fffffffffffffff) / 9223372036854775808.0 * (max - min) + min;
      }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallback("millisecondsSinceEpoch",
      []() { return Time::millisecondsSinceEpoch(); }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallback("monotonicMilliseconds",
      []() { return Time::monotonicMilliseconds(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("monotonicTime",
      []() { return Time::monotonicTime(); }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallback("makePromise",
      []() -> std::pair<RpcPromise<Json>, std::shared_ptr<AutoFailRpcPromiseKeeper<Json>>> {
        auto pair = RpcPromise<Json>::createPair();
        return std::make_pair(pair.first, std::make_shared<AutoFailRpcPromiseKeeper<Json>>(pair.second));
      }); !result)
    return std::unexpected(std::move(result.error()));

  return callbacks;
}

double LuaBindings::UtilityCallbacks::nrand(RandomService& random, std::optional<double> const& stdev, std::optional<double> const& mean) {
  return random.nrandd(stdev.value_or(1.0), mean.value_or(0));
}

String LuaBindings::UtilityCallbacks::makeUuid(RandomService& random) {
  return Uuid(random).hex();
}

std::expected<void, StarException> LuaBindings::UtilityCallbacks::logInfo(Logger& logger, String const& str, LuaVariadic<LuaValue> const& args) {
  auto text = formatLua(str, args);
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto logged = logger.log(LogLevel::Info, std::string_view(text->utf8()).substr(0, text->utf8().find('\0')));
  if (!logged)
    return std::unexpected(LuaHostError(ExceptionDiagnostic(logged.error())));
  return {};
}

std::expected<void, StarException> LuaBindings::UtilityCallbacks::logWarn(Logger& logger, String const& str, LuaVariadic<LuaValue> const& args) {
  auto text = formatLua(str, args);
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto logged = logger.log(LogLevel::Warn, std::string_view(text->utf8()).substr(0, text->utf8().find('\0')));
  if (!logged)
    return std::unexpected(LuaHostError(ExceptionDiagnostic(logged.error())));
  return {};
}

std::expected<void, StarException> LuaBindings::UtilityCallbacks::logError(Logger& logger, String const& str, LuaVariadic<LuaValue> const& args) {
  auto text = formatLua(str, args);
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto logged = logger.log(LogLevel::Error, std::string_view(text->utf8()).substr(0, text->utf8().find('\0')));
  if (!logged)
    return std::unexpected(LuaHostError(ExceptionDiagnostic(logged.error())));
  return {};
}

std::expected<void, StarException> LuaBindings::UtilityCallbacks::setLogMap(LogMap& logMap, String const& key, String const& value, LuaVariadic<LuaValue> const& args) {
  auto text = formatLua(value, args);
  if (!text)
    return std::unexpected(std::move(text.error()));
  logMap.setValue(key, *text);
  return {};
}

std::expected<Json, StarException> LuaBindings::UtilityCallbacks::parseJson(String const& str) {
  return Json::parse(str);
}

std::expected<String, StarException> LuaBindings::UtilityCallbacks::printJson(Json const& arg, std::optional<int> pretty) {
  return arg.repr(pretty.value_or(0));
}

std::expected<String, StarException> LuaBindings::UtilityCallbacks::print(LuaValue const& value) {
  return Star::print_lua_value(value);
}

std::expected<LuaValue, StarException> LuaBindings::UtilityCallbacks::interpolateSinEase(LuaEngine& engine, double offset, LuaValue const& value1, LuaValue const& value2) {
  auto floatValue1 = engine.luaMaybeTo<double>(value1);
  if (!floatValue1)
    return std::unexpected(std::move(floatValue1.error()));
  if (*floatValue1) {
    auto floatValue2 = engine.luaTo<double>(value2);
    if (!floatValue2)
      return std::unexpected(std::move(floatValue2.error()));
    return engine.luaFrom(sinEase(offset, **floatValue1, *floatValue2));
  }

  auto vectorValue1 = engine.luaTo<Vec2F>(value1);
  if (!vectorValue1)
    return std::unexpected(std::move(vectorValue1.error()));
  auto vectorValue2 = engine.luaTo<Vec2F>(value2);
  if (!vectorValue2)
    return std::unexpected(std::move(vectorValue2.error()));
  return engine.luaFrom<Vec2F>(sinEase(offset, *vectorValue1, *vectorValue2));
}

String LuaBindings::UtilityCallbacks::replace_tags(String const& str, StringMap<String> const& tags) {
  return str.replace_tags(tags);
}

}
