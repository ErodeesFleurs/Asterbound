module;
import std;
import star.string;
import star.list;
import star.json;
import star.lua;
import star.random;
import star.logging;
import star.format;

namespace Star {

namespace LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeUtilityCallbacks(RandomService& random, Logger& logger, LogMap& logMap);

  [[nodiscard]] std::expected<String, StarException> formatLua(String const& string, List<LuaValue> const& args);

  namespace UtilityCallbacks {
    double nrand(RandomService& random, std::optional<double> const& stdev, std::optional<double> const& mean);
    String makeUuid(RandomService& random);
    [[nodiscard]] std::expected<void, StarException> logInfo(Logger& logger, String const& str, LuaVariadic<LuaValue> const& args);
    [[nodiscard]] std::expected<void, StarException> logWarn(Logger& logger, String const& str, LuaVariadic<LuaValue> const& args);
    [[nodiscard]] std::expected<void, StarException> logError(Logger& logger, String const& str, LuaVariadic<LuaValue> const& args);
    [[nodiscard]] std::expected<void, StarException> setLogMap(LogMap& logMap, String const& key, String const& value, LuaVariadic<LuaValue> const& args);
    [[nodiscard]] std::expected<Json, StarException> parseJson(String const& str);
    [[nodiscard]] std::expected<String, StarException> printJson(Json const& arg, std::optional<int> pretty);
    [[nodiscard]] std::expected<String, StarException> print(LuaValue const& arg);
    [[nodiscard]] std::expected<LuaValue, StarException> interpolateSinEase(LuaEngine& engine, double offset, LuaValue const& value1, LuaValue const& value2);
    String replace_tags(String const& str, StringMap<String> const& tags);
  }
}
}

export module star.utility_lua_bindings;

export namespace Star::LuaBindings {
  using ::Star::LuaBindings::makeUtilityCallbacks;
  using ::Star::LuaBindings::formatLua;
}

export namespace Star::LuaBindings::UtilityCallbacks {
  using ::Star::LuaBindings::UtilityCallbacks::nrand;
  using ::Star::LuaBindings::UtilityCallbacks::makeUuid;
  using ::Star::LuaBindings::UtilityCallbacks::logInfo;
  using ::Star::LuaBindings::UtilityCallbacks::logWarn;
  using ::Star::LuaBindings::UtilityCallbacks::logError;
  using ::Star::LuaBindings::UtilityCallbacks::setLogMap;
  using ::Star::LuaBindings::UtilityCallbacks::parseJson;
  using ::Star::LuaBindings::UtilityCallbacks::printJson;
  using ::Star::LuaBindings::UtilityCallbacks::print;
  using ::Star::LuaBindings::UtilityCallbacks::interpolateSinEase;
  using ::Star::LuaBindings::UtilityCallbacks::replace_tags;
}
