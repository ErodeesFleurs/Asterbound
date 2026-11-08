#include <cstdio>
#include <lua.hpp>
import std;
import star.lua;
import star.json;
import star.string;
import star.string_view;
import star.list;
import star.map;
import star.set;
import star.optional;
import star.format;
import star.text_formatter;
import star.output_proxy;
import star.array;
import star.thread;
import star.time;
#include "imgui_lua_bindings.hpp"

import star.ref_ptr;
import star.memory;
import star.lexical_cast;
import star.byte_array;
import star.math_common;
import star.algorithm;
namespace Star {

namespace {
struct LuaStackRestore {
  lua_State* state;
  int top;
  explicit LuaStackRestore(lua_State* state) : state(state), top(lua_gettop(state)) {}
  ~LuaStackRestore() { lua_settop(state, top); }
};


std::expected<void, StarException> validateNativeLuaValue(lua_State* state, int index) {
  int type = lua_type(state, index);
  if (type == LUA_TLIGHTUSERDATA)
    return std::unexpected(LuaException("Unsupported type in popLuaValue"));
  if (type == LUA_TUSERDATA) {
    if (!lua_getmetatable(state, index))
      return std::unexpected(LuaException("Userdata in popLuaValue missing metatable"));
    lua_pop(state, 1);
  }
  return {};
}
int jsonNewIndex(lua_State* state) {
  if (!lua_getmetatable(state, 1))
    return luaL_error(state, "JSON table missing metatable");
  lua_getfield(state, -1, "__nils");
  if (!lua_istable(state, -1))
    return luaL_error(state, "JSON table missing null tracking table");
  lua_pushvalue(state, 2);
  if (lua_isnil(state, 3))
    lua_pushinteger(state, 0);
  else
    lua_pushnil(state);
  lua_rawset(state, -3);
  lua_settop(state, 3);
  lua_rawset(state, 1);
  return 0;
}

std::expected<LuaStackValue, StarException> pushJsonMetadata(LuaStackValue table, Json::Type type) {
  auto stack = table.stack();
  if (auto pushed = stack.push(table); !pushed)
    return std::unexpected(std::move(pushed.error()));
  if (auto pushed = stack.push(type == Json::Type::Array ? 1 : 2); !pushed)
    return std::unexpected(std::move(pushed.error()));
  auto installed = stack.call(
    +[](lua_State* state) {
      lua_createtable(state, 0, 3);
      lua_newtable(state);
      lua_pushvalue(state, -1);
      lua_setfield(state, -3, "__nils");
      lua_pushcfunction(state, jsonNewIndex);
      lua_setfield(state, -3, "__newindex");
      lua_pushvalue(state, 2);
      lua_setfield(state, -3, "__typehint");
      lua_pushvalue(state, -2);
      lua_setmetatable(state, 1);
      return 1;
    },
    2, 1);
  if (!installed)
    return std::unexpected(std::move(installed.error()));
  return stack.value(-1);
}

template <typename Container>
std::expected<void, StarException> pushJsonContainer(LuaStack stack, Container const& values) {
  if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return std::unexpected(LuaException("Too many entries for a Lua JSON container"));
  constexpr bool array = std::is_same_v<Container, JsonArray>;
  auto size = static_cast<int>(values.size());
  auto table = stack.createTable(array ? size : 0, array ? 0 : size);
  if (!table)
    return std::unexpected(std::move(table.error()));
  auto nils = pushJsonMetadata(*table, array ? Json::Type::Array : Json::Type::Object);
  if (!nils)
    return std::unexpected(std::move(nils.error()));
  if constexpr (array) {
    for (std::size_t i = 0; i < values.size(); ++i) {
      auto set = values[i] ? table->rawSet(i + 1, values[i]) : nils->rawSet(i + 1, 0);
      if (!set)
        return std::unexpected(std::move(set.error()));
    }
  } else {
    for (auto const& [key, value] : values) {
      auto set = value ? table->rawSet(key, value) : nils->rawSet(key, 0);
      if (!set)
        return std::unexpected(std::move(set.error()));
    }
  }
  stack.pop();
  return {};
}
}// namespace

std::expected<void, StarException> LuaStack::call(lua_CFunction function, int nargs, int nresults) const {
  if (!lua_checkstack(m_state, nresults == LUA_MULTRET ? 2 : std::max(2, nresults + 1)))
    return std::unexpected(LuaException("Exhausted the Lua stack while entering a protected call"));
  lua_pushcfunction(m_state, function);
  lua_insert(m_state, -nargs - 1);
  return m_engine.handleError(m_state, m_engine.pcallWithTraceback(m_state, nargs, nresults));
}

std::expected<void, StarException> LuaStack::load(LuaStackValue environment, std::span<char const> contents,
                                                  String const& name) const {
  if (&environment.stack().engine() != &m_engine)
    return std::unexpected(LuaException("Lua environments cannot be shared between engines"));
  if (environment.stack().state() != m_state)
    return std::unexpected(LuaException("Lua environments must belong to the loading stack"));
  if (!environment.is<LuaTable>())
    return std::unexpected(LuaConversionException("Lua script environment must be a table"));
  LuaStackRestore restore(m_state);
  if (!lua_checkstack(m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while loading a script"));
  auto loaded = m_engine.handleSourceError(
    m_state, luaL_loadbuffer(m_state, contents.data(), contents.size(), name.empty() ? nullptr : name.utf8().c_str()), name);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  lua_pushvalue(m_state, environment.index());
  lua_setupvalue(m_state, -2, 1);
  auto entered = m_engine.incrementRecursionLevel();
  if (!entered)
    return std::unexpected(std::move(entered.error()));
  int res = m_engine.pcallWithTraceback(m_state, 0, 0);
  m_engine.decrementRecursionLevel();
  return m_engine.handleError(m_state, res);
}

std::expected<void, StarException> LuaStack::pushString(std::string_view value) const {
  if (!lua_checkstack(m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while pushing a string"));
  lua_pushlightuserdata(m_state, &value);
  return call(
    +[](lua_State* state) {
      auto& value = *static_cast<std::string_view*>(lua_touserdata(state, 1));
      lua_pushlstring(state, value.data(), value.size());
      return 1;
    },
    1, 1);
}

std::expected<LuaStackValue, StarException> LuaStack::createTable(int narr, int nrec) const {
  if (!lua_checkstack(m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a table"));
  int sizes[] = {narr, nrec};
  lua_pushlightuserdata(m_state, sizes);
  auto created = call(
    +[](lua_State* state) {
      auto sizes = static_cast<int*>(lua_touserdata(state, 1));
      lua_createtable(state, sizes[0], sizes[1]);
      return 1;
    },
    1, 1);
  if (!created)
    return std::unexpected(std::move(created.error()));
  return value(-1);
}

StringView LuaStackValue::stringView() const {
  std::size_t size;
  auto data = lua_tolstring(m_stack.state(), m_index, &size);
  return StringView(std::string_view(data, size));
}

std::expected<void, StarException> LuaStackValue::validate() const {
  if (!lua_checkstack(m_stack.state(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while reading a value"));
  return validateNativeLuaValue(m_stack.state(), m_index);
}

std::expected<LuaValue, StarException> LuaStackValue::own() const {
  if (auto valid = validate(); !valid)
    return std::unexpected(std::move(valid.error()));
  lua_pushvalue(m_stack.state(), m_index);
  return m_stack.engine().popLuaValue(m_stack.state());
}

std::expected<LuaInt, StarException> LuaStackValue::rawLength() const {
  return static_cast<LuaInt>(lua_rawlen(m_stack.state(), m_index));
}

std::expected<LuaInt, StarException> LuaStackValue::length() const {
  auto state = m_stack.state();
  LuaStackRestore restore(state);
  if (!lua_checkstack(state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while measuring a table"));
  lua_pushvalue(state, m_index);
  auto measured = m_stack.call(
    +[](lua_State* state) {
      lua_len(state, 1);
      return 1;
    },
    1, 1);
  if (!measured)
    return std::unexpected(std::move(measured.error()));
  return static_cast<LuaInt>(lua_tointeger(state, -1));
}

std::expected<void, StarException> write_text(TextBuffer& output, LuaValue const& value) {
  if (std::holds_alternative<LuaBoolean>(value)) {
    output.append(std::get<LuaBoolean>(value) ? "true" : "false");
  } else if (std::holds_alternative<LuaInt>(value)) {
    append_text(output, std::get<LuaInt>(value));
  } else if (std::holds_alternative<LuaFloat>(value)) {
    append_text(output, std::get<LuaFloat>(value));
  } else if (std::holds_alternative<LuaString>(value)) {
    auto text = std::get<LuaString>(value).view().utf8();
    output.append(text.substr(0, text.find('\0')));
  } else if (std::holds_alternative<LuaTable>(value)) {
    output.push_back('{');
    bool first = true;
    auto result = std::get<LuaTable>(value).iterate([&output, &first](LuaValue const& key, LuaValue const& value) -> std::expected<void, StarException> {
      if (first)
        first = false;
      else
        output.append(", ");
      auto key_result = write_text(output, key);
      if (!key_result)
        return std::unexpected(std::move(key_result.error()));
      output.append(": ");
      return write_text(output, value);
    });
    if (!result)
      return std::unexpected(std::move(result.error()));
    output.push_back('}');
  } else if (std::holds_alternative<LuaFunction>(value)) {
    output.append("<function reg:");
    append_text(output, std::get<LuaFunction>(value).handleIndex());
    output.push_back('>');
  } else if (std::holds_alternative<LuaThread>(value)) {
    output.append("<thread reg:");
    append_text(output, std::get<LuaThread>(value).handleIndex());
    output.push_back('>');
  } else if (std::holds_alternative<LuaUserData>(value)) {
    output.append("<userdata reg:");
    append_text(output, std::get<LuaUserData>(value).handleIndex());
    output.push_back('>');
  } else {
    output.append("nil");
  }
  return {};
}

std::expected<String, StarException> print_lua_value(LuaValue const& value) {
  TextBuffer output;
  auto result = write_text(output, value);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return String(std::move(output).str());
}

std::expected<bool, StarException> LuaTable::contains(std::string_view key) const {
  return contains<std::string_view>(key);
}

std::expected<void, StarException> LuaTable::remove(std::string_view key) const { return set(key, LuaNil); }

std::expected<LuaInt, StarException> LuaTable::length() const {
  auto& eng = engine();
  LuaStackRestore restore(eng.m_state);
  if (!lua_checkstack(eng.m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while measuring a table"));
  eng.pushHandle(eng.m_state, handleIndex());
  return LuaStack(eng, eng.m_state).value(-1).length();
}

std::expected<std::optional<LuaTable>, StarException> LuaTable::getMetatable() const {
  return engine().tableGetMetatable(handleIndex());
}

std::expected<void, StarException> LuaTable::setMetatable(LuaTable const& table) const {
  return engine().tableSetMetatable(handleIndex(), table);
}

std::expected<LuaInt, StarException> LuaTable::rawLength() const {
  auto& eng = engine();
  LuaStackRestore restore(eng.m_state);
  if (!lua_checkstack(eng.m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while measuring a table"));
  eng.pushHandle(eng.m_state, handleIndex());
  return LuaStack(eng, eng.m_state).value(-1).rawLength();
}

std::expected<void, StarException> LuaCallbacks::copyCallback(String srcName, String dstName) {
  auto callback = m_callbacks.get(srcName);
  if (!callback)
    return std::unexpected(std::move(callback.error()));
  m_callbacks.set(dstName, callback->get());
  return {};
}

bool LuaCallbacks::removeCallback(String name) {
  return m_callbacks.remove(name);
}

std::expected<void, StarException> LuaCallbacks::merge(LuaCallbacks const& callbacks) {
  for (auto const& pair : callbacks.m_callbacks) {
    auto added = m_callbacks.add(pair.first, pair.second);
    if (!added)
      return std::unexpected(LuaException(Star::format("Failed to merge LuaCallbacks: {}", Star::output_exception(ExceptionDiagnostic(added.error()), true))));
  }
  return {};
}

LuaCallbacks LuaCallbacks::withLifetime(std::stop_token lifetime) const {
  LuaCallbacks guarded;
  for (auto const& entry : m_callbacks) {
    guarded.m_callbacks.set(entry.first,
                            [callback = entry.second, lifetime](LuaStack stack) -> std::expected<int, StarException> {
                              if (lifetime.stop_requested())
                                return std::unexpected(LuaException("Lua callback scope is retired"));
                              if (stack.engine().promiseLifetime() == lifetime)
                                return callback(stack);
                              auto previous = stack.engine().setPromiseLifetime(lifetime);
                              auto restored = finally([&] { stack.engine().setPromiseLifetime(std::move(previous)); });
                              return callback(stack);
                            });
  }
  return guarded;
}

StringMap<LuaDetail::LuaWrappedFunction> const& LuaCallbacks::callbacks() const {
  return m_callbacks;
}

std::expected<bool, StarException> LuaContext::containsPath(String path) const {
  auto value = engine().contextGetPath(handleIndex(), std::move(path));
  if (!value)
    return std::unexpected(std::move(value.error()));
  return *value != LuaNil;
}

std::expected<void, StarException> LuaContext::load(std::span<char const> contents, String const& name) {
  auto& eng = engine();
  LuaStackRestore restore(eng.m_state);
  if (!lua_checkstack(eng.m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while loading a context"));
  eng.pushHandle(eng.m_state, handleIndex());
  LuaStack stack(eng, eng.m_state);
  return stack.load(stack.value(-1), contents, name);
}

std::expected<void, StarException> LuaContext::load(String const& contents, String const& name) {
  return load(std::span(contents.utf8()), name);
}

std::expected<void, StarException> LuaContext::load(ByteArray const& contents, String const& name) {
  return load(contents.span(), name);
}

std::expected<void, StarException> LuaContext::setRequireFunction(RequireFunction requireFunction) {
  return engine().setContextRequire(handleIndex(), std::move(requireFunction));
}

std::expected<void, StarException> LuaContext::setCallbacks(String const& tableName, LuaCallbacks const& callbacks) const {
  auto& eng = engine();
  auto present = LuaContext::contains(tableName);
  if (!present)
    return std::unexpected(std::move(present.error()));
  if (*present)
    return {};
  auto callbackTable = eng.createTable();
  if (!callbackTable)
    return std::unexpected(std::move(callbackTable.error()));
  for (auto const& p : callbacks.callbacks()) {
    auto function = eng.createWrappedFunction(p.second);
    if (!function)
      return std::unexpected(std::move(function.error()));
    auto result = callbackTable->set(p.first, std::move(*function));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return LuaContext::set(tableName, std::move(*callbackTable));
}

std::expected<LuaString, StarException> LuaContext::createString(String const& str) {
  return engine().createString(str);
}

std::expected<LuaString, StarException> LuaContext::createString(std::string_view str) {
  return engine().createString(str);
}

std::expected<LuaTable, StarException> LuaContext::createTable() {
  return engine().createTable();
}

std::expected<void, StarException> LuaConverter<Json>::from(LuaStack stack, Json const& v) {
  switch (v.type()) {
  case Json::Type::Null: return stack.push(LuaNil);
  case Json::Type::Float: {
    auto value = v.toDouble();
    if (!value)
      return std::unexpected(std::move(value.error()));
    return stack.push(*value);
  }
  case Json::Type::Bool: {
    auto value = v.toBool();
    if (!value)
      return std::unexpected(std::move(value.error()));
    return stack.push(*value);
  }
  case Json::Type::Int: {
    auto value = v.toInt();
    if (!value)
      return std::unexpected(std::move(value.error()));
    return stack.push(*value);
  }
  case Json::Type::String: {
    auto value = v.stringPtr();
    if (!value)
      return std::unexpected(std::move(value.error()));
    return stack.push(**value);
  }
  case Json::Type::Array: {
    auto values = v.arrayPtr();
    if (!values)
      return std::unexpected(std::move(values.error()));
    return pushJsonContainer(stack, **values);
  }
  case Json::Type::Object: {
    auto values = v.objectPtr();
    if (!values)
      return std::unexpected(std::move(values.error()));
    return pushJsonContainer(stack, **values);
  }
  }
  std::unreachable();
}

std::expected<std::optional<Json>, StarException> LuaConverter<Json>::to(LuaStackValue v) {
  return v.stack().engine().stackToJson(v.stack().state(), v.index());
}

std::expected<void, StarException> LuaConverter<JsonObject>::from(LuaStack stack, JsonObject const& v) {
  return pushJsonContainer(stack, v);
}

std::expected<std::optional<JsonObject>, StarException> LuaConverter<JsonObject>::to(LuaStackValue v) {
  auto j = v.to<Json>();
  if (!j)
    return std::unexpected(std::move(j.error()));
  if (j->type() == Json::Type::Object)
    return j->toObject();
  if (j->type() == Json::Type::Array) {
    auto values = j->arrayPtr();
    if (!values)
      return std::unexpected(std::move(values.error()));
    if ((*values)->empty())
      return JsonObject();
  }
  return std::nullopt;
}

std::expected<void, StarException> LuaConverter<JsonArray>::from(LuaStack stack, JsonArray const& v) {
  return pushJsonContainer(stack, v);
}

std::expected<std::optional<JsonArray>, StarException> LuaConverter<JsonArray>::to(LuaStackValue v) {
  auto j = v.to<Json>();
  if (!j)
    return std::unexpected(std::move(j.error()));
  if (j->type() == Json::Type::Array)
    return j->toArray();
  if (j->type() == Json::Type::Object) {
    auto values = j->objectPtr();
    if (!values)
      return std::unexpected(std::move(values.error()));
    if ((*values)->empty())
      return JsonArray();
  }
  return std::nullopt;
}

std::expected<LuaEnginePtr, StarException> LuaEngine::create(bool safe) {
  LuaEnginePtr self(new LuaEngine);

  self->m_state = lua_newstate(allocate, nullptr, luaL_makeseed(nullptr));

  self->m_scriptDefaultEnvRegistryId = LUA_NOREF;
  self->m_wrappedFunctionMetatableRegistryId = LUA_NOREF;
  self->m_requireFunctionMetatableRegistryId = LUA_NOREF;

  self->m_instructionLimit = 0;
  self->m_profilingEnabled = false;
  self->m_instructionMeasureInterval = 1000;
  self->m_instructionCount = 0;
  self->m_recursionLevel = 0;
  self->m_recursionLimit = 0;

  if (!self->m_state)
    return std::unexpected(LuaException("Failed to initialize Lua"));

  lua_checkstack(self->m_state, 5);
  *reinterpret_cast<LuaEngine**>(lua_getextraspace(self->m_state)) = self.get();
  lua_setwarnf(self->m_state, [](void* userdata, char const* message, int continued) {
    auto& self = *static_cast<LuaEngine*>(userdata);
    self.m_warningMessage += message;
    if (!continued) {
      if (auto written = Star::print(stderr, "Lua warning: {}\n", self.m_warningMessage); !written)
        std::_Exit(1);
      self.m_warningMessage.clear();
    }
  }, self.get());
  auto initialize = [](lua_State* state) {
    auto& self = luaEngine(state);
    bool safe = lua_toboolean(state, 1);
    lua_settop(state, 0);
    lua_createtable(state, 64, 0);
    self.m_diagnosticStringsRegistryId = luaL_ref(state, LUA_REGISTRYINDEX);

  // Create the common message handler function for pcall to print a better
  // message with a traceback
  lua_pushcfunction(state, [](lua_State* state) {
      auto& self = luaEngine(state);
      if (lua_type(state, 1) == LUA_TNUMBER) {
        // Match traceback's numeric coercion before owning host metadata.
        lua_pushcfunction(state, [](lua_State* state) {
          lua_tolstring(state, 1, nullptr);
          return 1;
        });
        lua_pushvalue(state, 1);
        if (lua_pcall(state, 1, 1, 0) != LUA_OK)
          return lua_error(state);
        lua_replace(state, 1);
      }
      {
        auto summary = std::make_shared<ExceptionSummary const>(
            self.captureDiagnostic(state, state, 1, ExceptionSummaryKind::ScriptRuntime));
        self.rememberDiagnostic(state, 1, std::move(summary));
      }
      // Keep limit sentinels untouched, including their native error types.
      if (lua_islightuserdata(state, 1)) {
        void* error = lua_touserdata(state, 1);
        if (error == &s_luaInstructionLimitExceptionKey || error == &s_luaRecursionLimitExceptionKey)
          return 1;
      }

      luaL_traceback(state, state, lua_tostring(state, 1), 0);
      {
        auto summary = self.findDiagnostic(state, 1);
        if (summary) {
          self.appendSourceDiagnostics(state, state, *summary);
          self.rememberDiagnostic(state, -1, std::move(summary));
        }
      }
      lua_remove(state, 1);
      return 1;
    });
  self.m_pcallTracebackMessageHandlerRegistryId = luaL_ref(state, LUA_REGISTRYINDEX);

  // Create the common metatable for wrapped functions
  lua_newtable(state);
  lua_pushcfunction(state, [](lua_State* state) {
      auto& func = *static_cast<LuaDetail::LuaWrappedFunction*>(lua_touserdata(state, 1));
      std::destroy_at(&func);
      return 0;
    });
  LuaDetail::rawSetField(state, -2, "__gc");
  lua_pushboolean(state, 0);
  LuaDetail::rawSetField(state, -2, "__metatable");
  self.m_wrappedFunctionMetatableRegistryId = luaL_ref(state, LUA_REGISTRYINDEX);

  // Create the common metatable for require functions
  lua_newtable(state);
  lua_pushcfunction(state, [](lua_State* state) {
      auto& func = *static_cast<LuaContext::RequireFunction*>(lua_touserdata(state, 1));
      std::destroy_at(&func);
      return 0;
    });
  LuaDetail::rawSetField(state, -2, "__gc");
  lua_pushboolean(state, 0);
  LuaDetail::rawSetField(state, -2, "__metatable");
  self.m_requireFunctionMetatableRegistryId = luaL_ref(state, LUA_REGISTRYINDEX);

  // Load all base libraries and prune them of unsafe functions

  luaL_requiref(state, "_ENV", luaopen_base, true);
  if (safe) {
    StringSet baseWhitelist = {
        "assert",
        "error",
        "getmetatable",
        "ipairs",
        "next",
        "pairs",
        "pcall",
        "print",
        "rawequal",
        "rawget",
        "rawlen",
        "rawset",
        "select",
        "setmetatable",
        "tonumber",
        "tostring",
        "type",
        "unpack",
        "_VERSION",
        "xpcall"};

    lua_pushnil(state);
    while (lua_next(state, -2) != 0) {
      lua_pop(state, 1);
      String key(std::string_view(lua_tostring(state, -1)));

      if (!baseWhitelist.contains(key)) {
        lua_pushvalue(state, -1);
        lua_pushnil(state);
        lua_rawset(state, -4);
      }
    }
  }
  lua_pop(state, 1);

  luaL_requiref(state, "os", luaopen_os, true);
  if (safe) {
    StringSet osWhitelist = {"clock", "difftime", "time", "date"};

    lua_pushnil(state);
    while (lua_next(state, -2) != 0) {
      lua_pop(state, 1);
      String key(std::string_view(lua_tostring(state, -1)));

      if (!osWhitelist.contains(key)) {
        lua_pushvalue(state, -1);
        lua_pushnil(state);
        lua_rawset(state, -4);
      }
    }
  }
  lua_pop(state, 1);

  // loads a lua base library, leaves it at the top of the stack
  // Only terminated literal library names enter this native Lua adapter.
  auto loadBaseLibrary = [](lua_State* state, std::string_view modname, lua_CFunction openf) {
    luaL_requiref(state, modname.data(), openf, true);

    // set __metatable metamethod to false
    // otherwise scripts can access and mutate the metatable, allowing passing values
    // between script contexts, breaking the sandbox
    lua_newtable(state);
    lua_pushliteral(state, "__metatable");
    lua_pushboolean(state, 0);
    lua_rawset(state, -3);
    lua_setmetatable(state, -2);
  };

  loadBaseLibrary(state, "coroutine", luaopen_coroutine);
  // replace coroutine resume with one that appends tracebacks
  lua_pushliteral(state, "resume");
  lua_pushcfunction(state, &LuaEngine::coresumeWithTraceback);
  lua_rawset(state, -3);

  loadBaseLibrary(state, "math", luaopen_math);
  loadBaseLibrary(state, "string", luaopen_string);
  loadBaseLibrary(state, "table", luaopen_table);
  loadBaseLibrary(state, "utf8", luaopen_utf8);
  lua_pop(state, 5);

  if (!safe) {
    loadBaseLibrary(state, "io", luaopen_io);
    loadBaseLibrary(state, "package", luaopen_package);
    loadBaseLibrary(state, "debug", luaopen_debug);
    lua_pop(state, 3);
  }

  // Make a shallow copy of the default script environment and save it for
  // resetting the global state.
  lua_rawgeti(state, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  lua_newtable(state);
  LuaDetail::shallowCopy(state, -2, -1);
  self.m_scriptDefaultEnvRegistryId = luaL_ref(state, LUA_REGISTRYINDEX);
  lua_pop(state, 1);
  // Use Lua's own freelist layout rather than a version-specific registry slot.
  lua_pushboolean(state, 0);
  int temporary = luaL_ref(state, LUA_REGISTRYINDEX);
  luaL_unref(state, LUA_REGISTRYINDEX, temporary);
    return 0;
  };
  lua_pushcfunction(self->m_state, initialize);
  lua_pushboolean(self->m_state, safe);
  auto initialized = self->handleError(self->m_state, lua_pcall(self->m_state, 1, 0, 0));
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto install = [&self](std::string_view name, std::expected<LuaFunction, StarException> function) -> std::expected<void, StarException> {
    if (!function)
      return std::unexpected(std::move(function.error()));
    return self->setGlobal(name, std::move(*function));
  };

  auto jarray = install("jarray", self->createFunction(&LuaDetail::jarray));
  if (!jarray)
    return std::unexpected(std::move(jarray.error()));
  auto jobject = install("jobject", self->createFunction(&LuaDetail::jobject));
  if (!jobject)
    return std::unexpected(std::move(jobject.error()));
  auto jremove = install("jremove", self->createFunction(&LuaDetail::jcontRemove));
  if (!jremove)
    return std::unexpected(std::move(jremove.error()));
  auto jsize = install("jsize", self->createFunction(&LuaDetail::jcontSize));
  if (!jsize)
    return std::unexpected(std::move(jsize.error()));
  auto jresize = install("jresize", self->createFunction(&LuaDetail::jcontResize));
  if (!jresize)
    return std::unexpected(std::move(jresize.error()));
  auto sharedTable = self->createTable();
  if (!sharedTable)
    return std::unexpected(std::move(sharedTable.error()));
  auto shared = self->setGlobal("shared", std::move(*sharedTable));
  if (!shared)
    return std::unexpected(std::move(shared.error()));
  return self;
}

LuaEngine::~LuaEngine() {
  if (!m_state)
    return;
  // If we've had a stack space leak, this will not be zero
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((lua_gettop(m_state) == 0));
  }
  lua_close(m_state);
}

void LuaEngine::setInstructionLimit(std::uint64_t instructionLimit) {
  if (instructionLimit != m_instructionLimit) {
    m_instructionLimit = instructionLimit;
    updateCountHook();
  }
}

std::uint64_t LuaEngine::instructionLimit() const {
  return m_instructionLimit;
}

void LuaEngine::setProfilingEnabled(bool profilingEnabled) {
  if (profilingEnabled != m_profilingEnabled) {
    m_profilingEnabled = profilingEnabled;
    m_profileEntries.clear();
    updateCountHook();
  }
}

bool LuaEngine::profilingEnabled() const {
  return m_profilingEnabled;
}

List<LuaProfileEntry> LuaEngine::getProfile() {
  List<LuaProfileEntry> profileEntries;
  for (auto const& p : m_profileEntries) {
    profileEntries.push_back(*p.second);
  }

  return profileEntries;
}

void LuaEngine::setInstructionMeasureInterval(unsigned measureInterval) {
  if (measureInterval != m_instructionMeasureInterval) {
    m_instructionMeasureInterval = measureInterval;
    updateCountHook();
  }
}

unsigned LuaEngine::instructionMeasureInterval() const {
  return m_instructionMeasureInterval;
}

void LuaEngine::setRecursionLimit(unsigned recursionLimit) {
  m_recursionLimit = recursionLimit;
}

unsigned LuaEngine::recursionLimit() const {
  return m_recursionLimit;
}

std::expected<ByteArray, StarException> LuaEngine::compile(std::span<char const> contents, String const& name) {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while compiling a script"));
  auto loaded = handleSourceError(m_state, luaL_loadbuffer(m_state, contents.data(), contents.size(), name.empty() ? nullptr : name.utf8().c_str()), name);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));

  ByteArray compiledScript;
  lua_Writer writer = [](lua_State*, void const* data, std::size_t size, void* byteArrayPtr) -> int {
    static_cast<ByteArray*>(byteArrayPtr)->append(std::span(static_cast<char const*>(data), size));
    return 0;
  };
  if (lua_dump(m_state, writer, &compiledScript, false) != 0)
    return std::unexpected(LuaException("Failed to dump compiled Lua script"));
  lua_pop(m_state, 1);

  return compiledScript;
}

std::expected<ByteArray, StarException> LuaEngine::compile(String const& contents, String const& name) {
  return compile(std::span(contents.utf8()), name);
}

std::expected<ByteArray, StarException> LuaEngine::compile(ByteArray const& contents, String const& name) {
  return compile(contents.span(), name);
}

void LuaEngine::setSourceResolver(SourceResolver resolver) {
  m_sourceResolver = std::move(resolver);
}

lua_Debug const& LuaEngine::debugInfo(int level, String const& what) {
  lua_Debug& debug = m_debugInfo = lua_Debug();
  lua_getstack(m_state, level, &debug);
  lua_getinfo(m_state, what.utf8().c_str(), &debug);
  return debug;
}

std::expected<LuaString, StarException> LuaEngine::createString(std::string_view str) {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a string"));
  lua_pushcfunction(m_state, [](lua_State* state) {
    auto const& input = *static_cast<std::string_view const*>(lua_touserdata(state, 1));
    lua_pushlstring(state, input.data(), input.size());
    return 1;
  });
  lua_pushlightuserdata(m_state, &str);
  auto called = handleError(m_state, pcallWithTraceback(m_state, 1, 1));
  if (!called)
    return std::unexpected(std::move(called.error()));
  auto handle = popHandle(m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaString(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
}

std::expected<LuaString, StarException> LuaEngine::createString(String const& str) {
  return createString(str.utf8());
}


std::expected<LuaTable, StarException> LuaEngine::createTable(int narr, int nrec) {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 4))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a table"));
  lua_pushcfunction(m_state, [](lua_State* state) {
    lua_createtable(state, (int)lua_tointeger(state, 1), (int)lua_tointeger(state, 2));
    return 1;
  });
  lua_pushinteger(m_state, narr);
  lua_pushinteger(m_state, nrec);
  auto called = handleError(m_state, pcallWithTraceback(m_state, 2, 1));
  if (!called)
    return std::unexpected(std::move(called.error()));
  auto handle = popHandle(m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaTable(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
}

std::expected<LuaThread, StarException> LuaEngine::createThread() {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a thread"));
  lua_pushcfunction(m_state, [](lua_State* state) {
    lua_newthread(state);
    return 1;
  });
  auto called = handleError(m_state, pcallWithTraceback(m_state, 0, 1));
  if (!called)
    return std::unexpected(std::move(called.error()));
  auto handle = popHandle(m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaThread(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
}

std::expected<void*, StarException> LuaEngine::allocateUserData(lua_State* state, std::size_t size) {
  LuaStackRestore restore(state);
  if (!lua_checkstack(state, 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while allocating userdata"));
  lua_pushcfunction(state, [](lua_State* state) {
    auto size = static_cast<std::size_t*>(lua_touserdata(state, 1));
    lua_newuserdatauv(state, *size, 0);
    return 1;
  });
  lua_pushlightuserdata(state, &size);
  auto allocated = handleError(state, pcallWithTraceback(state, 1, 1));
  if (!allocated)
    return std::unexpected(std::move(allocated.error()));
  ++restore.top;
  return lua_touserdata(state, -1);
}

std::expected<void, StarException> LuaEngine::threadPushFunction(int threadIndex, int functionIndex) {
  if (!lua_checkstack(m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a thread"));
  pushHandle(m_state, threadIndex);
  lua_State* thread = lua_tothread(m_state, -1);
  lua_pop(m_state, 1);

  int status = lua_status(thread);
  lua_Debug ar;
  if (status != LUA_OK || lua_getstack(thread, 0, &ar) > 0 || lua_gettop(thread) > 0)
    return std::unexpected(LuaException(Star::format("Cannot push function to active or errored thread with status {}", status)));
  if (!lua_checkstack(thread, 1))
    return std::unexpected(LuaException("Exhausted the Lua thread stack"));

  pushHandle(thread, functionIndex);
  return {};
}

LuaThread::Status LuaEngine::threadStatus(int handleIndex) {
  pushHandle(m_state, handleIndex);
  lua_State* thread = lua_tothread(m_state, -1);
  lua_pop(m_state, 1);

  int status = lua_status(thread);
  if (status != LUA_OK && status != LUA_YIELD)
    return LuaThread::Status::Error;

  lua_Debug ar;
  if (status == LUA_YIELD || lua_getstack(thread, 0, &ar) > 0 || lua_gettop(thread) > 0)
    return LuaThread::Status::Active;

  return LuaThread::Status::Dead;
}

std::expected<LuaContext, StarException> LuaEngine::createContext() {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a context"));
  lua_pushcfunction(m_state, [](lua_State* state) {
    lua_newtable(state);
    LuaDetail::shallowCopy(state, 1, -1);
    return 1;
  });
  lua_rawgeti(m_state, LUA_REGISTRYINDEX, m_scriptDefaultEnvRegistryId);
  auto created = handleError(m_state, pcallWithTraceback(m_state, 1, 1));
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto handle = popHandle(m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  auto context = LuaContext(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
  // Add loadstring
  auto handleIndex = context.handleIndex();
  auto loadstring = createFunction([&engine = *this, handleIndex](String const& source, std::optional<String> const& name, std::optional<LuaTable> const& env) -> std::expected<LuaFunction, StarException> {
    String functionName = name ? Star::format("loadstring: {}", *name) : "loadstring";
    return engine.createFunctionFromSource(env ? env->handleIndex() : handleIndex, std::span(source.utf8()), functionName);
  });
  if (!loadstring)
    return std::unexpected(std::move(loadstring.error()));
  auto installed = context.set("loadstring", std::move(*loadstring));
  if (!installed)
    return std::unexpected(std::move(installed.error()));

  // Then set that environment as the new context environment in the registry.
  return context;
}

void LuaEngine::releaseHandles(lua_State* state) {
  if (m_handleFree.empty() || !lua_checkstack(state, 2))
    return;
  // Existing slots are overwritten without allocating or running finalizers.
  // Retain the queue capacity instead of allocating another batch next time.
  for (int handle : m_handleFree)
    luaL_unref(state, LUA_REGISTRYINDEX, handle);
  m_handleFree.clear();
}

std::expected<void, StarException> LuaEngine::collectGarbage(std::optional<unsigned> steps) {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 4))
    return std::unexpected(LuaException("Exhausted the Lua stack while collecting garbage"));
  struct Input {
    bool incremental;
    std::size_t bytes;
  } input{steps.has_value(), steps ? std::size_t(*steps) * 1024 : 0};
  bool running = lua_gc(m_state, LUA_GCISRUNNING);
  lua_pushcfunction(m_state, [](lua_State* state) {
    auto const& input = *static_cast<Input const*>(lua_touserdata(state, 1));
    if (input.incremental)
      lua_gc(state, LUA_GCSTEP, input.bytes);
    else
      lua_gc(state, LUA_GCCOLLECT);
    return 0;
  });
  lua_pushlightuserdata(m_state, &input);
  int status = pcallWithTraceback(m_state, 1, 0);
  if (bool(lua_gc(m_state, LUA_GCISRUNNING)) != running)
    lua_gc(m_state, running ? LUA_GCRESTART : LUA_GCSTOP);
  return handleError(m_state, status);
}

void LuaEngine::setAutoGarbageCollection(bool autoGarbageColleciton) {
  lua_gc(m_state, autoGarbageColleciton ? LUA_GCRESTART : LUA_GCSTOP);
}

void LuaEngine::tuneAutoGarbageCollection(float pause, float stepMultiplier) {
  lua_gc(m_state, LUA_GCPARAM, LUA_GCPPAUSE, static_cast<int>(std::round(pause * 100)));
  lua_gc(m_state, LUA_GCPARAM, LUA_GCPSTEPMUL, static_cast<int>(std::round(stepMultiplier * 100)));
}

std::size_t LuaEngine::memoryUsage() const {
  return (std::size_t)lua_gc(m_state, LUA_GCCOUNT, 0) * 1024 + lua_gc(m_state, LUA_GCCOUNTB, 0);
}

std::expected<void, StarException> LuaEngine::addImGui() {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while adding ImGui bindings"));
  lua_pushcfunction(m_state, [](lua_State* state) {
    pushImguiBindings(state);
    lua_setfield(state, 1, "imgui");
    return 0;
  });
  lua_rawgeti(m_state, LUA_REGISTRYINDEX, m_scriptDefaultEnvRegistryId);
  return handleError(m_state, pcallWithTraceback(m_state, 1, 0));
}

LuaEngine& LuaEngine::luaEngine(lua_State* state) {
  return **reinterpret_cast<LuaEngine**>(lua_getextraspace(state));
}

void LuaEngine::countHook(lua_State* state, lua_Debug* ar) {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((ar->event == LUA_HOOKCOUNT));
  }
  lua_checkstack(state, 4);

  auto& self = luaEngine(state);

  // If the instruction count is 0, that means in this sequence of calls,
  // we have not hit a debug hook yet.  Since we don't know the state of
  // the internal lua instruction counter at the start, we don't know how
  // many instructions have been executed, only that it is >= 1 and <=
  // m_instructionMeasureInterval, so we pick the low estimate.
  if (self.m_instructionCount == 0)
    self.m_instructionCount = 1;
  else
    self.m_instructionCount += self.m_instructionMeasureInterval;

  if (self.m_instructionLimit != 0 && self.m_instructionCount > self.m_instructionLimit) {
    lua_pushlightuserdata(state, &s_luaInstructionLimitExceptionKey);
    lua_error(state);
  }

  if (self.m_profilingEnabled) {
    // find bottom of the stack
    // ar will contain the stack info from the last call that returns 1
    int stackLevel = -1;
    while (lua_getstack(state, stackLevel + 1, ar) == 1)
      stackLevel++;

    std::shared_ptr<LuaProfileEntry> parentEntry = nullptr;
    while (true) {
      // Get the 'n' name info and 'S' source info
      if (lua_getinfo(state, "nS", ar) == 0)
        break;

      auto key = std::make_tuple(String(ar->short_src), (unsigned)ar->linedefined);
      auto& entryMap = parentEntry ? parentEntry->calls : self.m_profileEntries;
      if (!entryMap.contains(key)) {
        auto e = std::make_shared<LuaProfileEntry>();
        e->source = std::string_view(ar->short_src);
        e->sourceLine = ar->linedefined;
        entryMap.set(key, e);
      }
      auto entry = entryMap.find(key)->second;

      // metadata
      if (!entry->name && ar->name)
        entry->name = String(std::string_view(ar->name));
      if (!entry->nameScope && String() != std::string_view(ar->namewhat))
        entry->nameScope = String(std::string_view(ar->namewhat));

      // Only add timeTaken to the self time for the function we are actually
      // in, not parent functions
      if (stackLevel == 0) {
        entry->totalTime += 1;
        entry->selfTime += 1;
      } else {
        entry->totalTime += 1;
      }

      parentEntry = entry;
      if (lua_getstack(state, --stackLevel, ar) == 0)
        break;
    }
  }
}

void* LuaEngine::allocate(void*, void* ptr, std::size_t oldSize, std::size_t newSize) {
  if (newSize == 0) {
    Star::free(ptr, oldSize);
    return nullptr;
  } else {
    return Star::realloc(ptr, newSize);
  }
}

void LuaEngine::pushCallbackError(lua_State* state, StarException const& error) {
  if (error.is<LuaInstructionLimitReached>()) {
    lua_pushlightuserdata(state, &s_luaInstructionLimitExceptionKey);
  } else if (error.is<LuaRecursionLimitReached>()) {
    lua_pushlightuserdata(state, &s_luaRecursionLimitExceptionKey);
  } else {
    auto diagnostic = Star::print_exception(ExceptionDiagnostic(error), true);
    lua_pushcfunction(state, [](lua_State* state) {
      auto diagnostic = static_cast<char const*>(lua_touserdata(state, 1));
      luaL_where(state, 2);
      lua_pushstring(state, diagnostic);
      lua_concat(state, 2);
      return 1;
    });
    lua_pushlightuserdata(state, const_cast<char*>(diagnostic.c_str()));
    // A finalizer/allocation failure must not inherit the callback's cause.
    if (lua_pcall(state, 1, 1, 0) != LUA_OK)
      return;
  }
  rememberDiagnostic(state, -1, error.diagnosticSummary());
}

std::shared_ptr<ExceptionSummary const> LuaEngine::findDiagnostic(lua_State* state, int index) const {
  int type = lua_type(state, index);
  std::size_t length = 0;
  void const* identity = type == LUA_TSTRING ? lua_tolstring(state, index, &length)
      : type == LUA_TLIGHTUSERDATA ? lua_touserdata(state, index) : nullptr;
  if (!identity)
    return {};
  for (std::size_t i = 0; i < m_diagnosticCount; ++i) {
    auto const& entry = m_diagnosticTransport[(m_nextDiagnostic + m_diagnosticTransport.size() - i - 1)
        % m_diagnosticTransport.size()];
    if (entry.type == type && entry.identity == identity)
      return entry.summary;
  }
  if (type != LUA_TSTRING)
    return {};
  // Lua's error(message) can prepend a location to a caught native string.
  // Accept only a prefix verified against a live frame and the complete pinned
  // payload, never a guessed suffix or a parsed flattened exception/traceback.
  std::string_view message(static_cast<char const*>(identity), length);
  lua_Debug frame;
  for (int level = 0; lua_getstack(state, level, &frame); ++level) {
    if (!lua_getinfo(state, "Sl", &frame) || frame.currentline <= 0)
      continue;
    auto prefix = std::format("{}:{}: ", frame.short_src, frame.currentline);
    if (!message.starts_with(prefix))
      continue;
    auto payload = message.substr(prefix.size());
    for (std::size_t i = 0; i < m_diagnosticCount; ++i) {
      auto const& entry = m_diagnosticTransport[(m_nextDiagnostic + m_diagnosticTransport.size() - i - 1)
          % m_diagnosticTransport.size()];
      if (entry.type == LUA_TSTRING && entry.length == payload.size()
          && payload == std::string_view(static_cast<char const*>(entry.identity), entry.length))
        return entry.summary;
    }
  }
  return {};
}

void LuaEngine::rememberDiagnostic(lua_State* state, int index, std::shared_ptr<ExceptionSummary const> summary) {
  if (!summary || m_diagnosticStringsRegistryId == LUA_NOREF || !lua_checkstack(state, 4))
    return;
  int type = lua_type(state, index);
  std::size_t length = 0;
  void const* identity = type == LUA_TSTRING ? lua_tolstring(state, index, &length)
      : type == LUA_TLIGHTUSERDATA ? lua_touserdata(state, index) : nullptr;
  if (!identity)
    return;
  index = lua_absindex(state, index);
  struct Input {
    int registry;
    std::size_t slot;
  } input{m_diagnosticStringsRegistryId, m_nextDiagnostic};
  lua_pushcfunction(state, [](lua_State* state) {
    auto& input = *static_cast<Input*>(lua_touserdata(state, 2));
    lua_rawgeti(state, LUA_REGISTRYINDEX, input.registry);
    lua_pushvalue(state, 1);
    lua_rawseti(state, -2, static_cast<lua_Integer>(input.slot + 1));
    return 0;
  });
  lua_pushvalue(state, index);
  lua_pushlightuserdata(state, &input);
  int result = lua_pcall(state, 2, 0, 0);
  if (result != LUA_OK) {
    lua_pop(state, 1);
    return;
  }
  m_diagnosticTransport[m_nextDiagnostic] = {identity, type, length, std::move(summary)};
  m_nextDiagnostic = (m_nextDiagnostic + 1) % m_diagnosticTransport.size();
  m_diagnosticCount = std::min(m_diagnosticCount + 1, m_diagnosticTransport.size());
}

ExceptionSummary LuaEngine::captureDiagnostic(lua_State* from, lua_State* state, int index,
    ExceptionSummaryKind kind) const {
  auto inherited = kind == ExceptionSummaryKind::ScriptRuntime ? findDiagnostic(state, index) : nullptr;
  ExceptionSummary summary;
  if (inherited) {
    summary = *inherited;
  } else {
    summary.kind = kind;
    summary.category = kind == ExceptionSummaryKind::ScriptSyntax ? "Lua syntax error" : "Lua runtime error";
    if (lua_type(state, index) == LUA_TSTRING) {
      std::size_t length = 0;
      auto reason = lua_tolstring(state, index, &length);
      summary.reason.assign(reason, length);
    } else {
      summary.reason = "<unknown error>";
    }
  }
  if (lua_islightuserdata(state, index)) {
    auto sentinel = lua_touserdata(state, index);
    if (sentinel == &s_luaInstructionLimitExceptionKey || sentinel == &s_luaRecursionLimitExceptionKey) {
      summary.kind = ExceptionSummaryKind::ScriptRuntime;
      summary.category = sentinel == &s_luaInstructionLimitExceptionKey
          ? "LuaInstructionLimitReached" : "LuaRecursionLimitReached";
      summary.reason = sentinel == &s_luaInstructionLimitExceptionKey
          ? "Lua instruction limit reached" : "Lua recursion limit reached";
    }
  }
  lua_Debug frame;
  for (int level = 0; lua_getstack(from, level, &frame); ++level) {
    if (!lua_getinfo(from, "Sl", &frame) || !frame.source || std::string_view(frame.what) == "C")
      continue;
    std::string source(frame.source, frame.srclen);
    auto existing = std::ranges::find_if(summary.locations, [&](auto const& location) {
      return location.source == source;
    });
    if (std::ranges::any_of(summary.locations, [&](auto const& location) {
          return location.source == source && location.line == frame.currentline;
        }))
      continue;
    auto provider = existing != summary.locations.end() ? existing->provider : sourceDiagnostic(String(source)).take_utf8();
    summary.locations.push_back({std::move(source), frame.currentline, std::move(provider)});
  }
  return summary;
}

void LuaEngine::beginDiagnosticScope() {
  if (m_diagnosticDepth++ != 0 || m_diagnosticCount == 0)
    return;
  // Only a previous failure makes this loop run. Successful calls allocate no
  // summary metadata and do not scan an empty transport.
  bool clearRoots = lua_checkstack(m_state, 2);
  if (clearRoots)
    lua_rawgeti(m_state, LUA_REGISTRYINDEX, m_diagnosticStringsRegistryId);
  for (std::size_t i = 0; i < m_diagnosticCount; ++i) {
    m_diagnosticTransport[i] = {};
    if (clearRoots) {
      lua_pushnil(m_state);
      lua_rawseti(m_state, -2, static_cast<lua_Integer>(i + 1));
    }
  }
  if (clearRoots)
    lua_pop(m_state, 1);
  m_diagnosticCount = 0;
  m_nextDiagnostic = 0;
}

void LuaEngine::endDiagnosticScope() {
  --m_diagnosticDepth;
}

std::expected<void, StarException> LuaEngine::handleError(lua_State* state, int res) {
  if (res == LUA_OK)
    return {};
  auto summary = captureDiagnostic(state, state, -1, res == LUA_ERRSYNTAX
      ? ExceptionSummaryKind::ScriptSyntax : ExceptionSummaryKind::ScriptRuntime);
  if (lua_islightuserdata(state, -1)) {
    void* error = lua_touserdata(state, -1);
    if (error == &s_luaInstructionLimitExceptionKey) {
      lua_pop(state, 1);
      return std::unexpected(LuaInstructionLimitReached().withSummary(std::move(summary)));
    }
    if (error == &s_luaRecursionLimitExceptionKey) {
      lua_pop(state, 1);
      return std::unexpected(LuaRecursionLimitReached().withSummary(std::move(summary)));
    }
  }
  if (lua_type(state, -1) == LUA_TNUMBER) {
    // Numeric Lua error objects are string-coerced just as before, but the
    // coercion may collect garbage and run a user finalizer.
    lua_pushcfunction(state, [](lua_State* state) {
      lua_tolstring(state, 1, nullptr);
      lua_pushvalue(state, 1);
      return 1;
    });
    lua_insert(state, -2);
    int converted = lua_pcall(state, 1, 1, 0);
    if (converted != LUA_OK)
      res = converted;
    summary = captureDiagnostic(state, state, -1, ExceptionSummaryKind::ScriptRuntime);
  }
  String error = lua_type(state, -1) == LUA_TSTRING
      ? Star::format("Error code {}, {}", res, lua_tostring(state, -1))
      : Star::format("Error code {}, <unknown error>", res);
  lua_pop(state, 1);
  if (error.utf8().ends_with("<eof>"))
    return std::unexpected(LuaIncompleteStatementException(error.take_utf8()).withSummary(std::move(summary)));
  return std::unexpected(LuaException(error.take_utf8()).withSummary(std::move(summary)));
}

std::expected<void, StarException> LuaEngine::handleSourceError(lua_State* state, int res, String const& source) {
  auto result = handleError(state, res);
  if (!result) {
    auto summary = *result.error().diagnosticSummary();
    auto diagnostic = sourceDiagnostic(source);
    if (std::ranges::none_of(summary.locations, [&](auto const& location) { return location.source == source.utf8(); }))
      summary.locations.insert(summary.locations.begin(), {source.utf8(), -1, diagnostic.utf8()});
    auto error = std::move(result.error()).withSummary(std::move(summary));
    if (!diagnostic.empty())
      error = std::move(error).withContext(Star::format("Lua source '{}': {}", source, diagnostic));
    return std::unexpected(std::move(error));
  }
  return result;
}

int LuaEngine::pcallWithTraceback(lua_State* state, int nargs, int nresults) {
  releaseHandles(state);
  beginDiagnosticScope();
  int msghPosition = lua_gettop(state) - nargs;
  lua_rawgeti(state, LUA_REGISTRYINDEX, m_pcallTracebackMessageHandlerRegistryId);
  lua_insert(state, msghPosition);
  int ret = lua_pcall(state, nargs, nresults, msghPosition);
  lua_remove(state, msghPosition);
  releaseHandles(state);
  endDiagnosticScope();
  return ret;
}

String LuaEngine::sourceDiagnostic(String const& source) const noexcept {
  if (!m_sourceResolver || source.empty())
    return {};
  auto diagnostic = m_sourceResolver(source);
  if (diagnostic)
    return std::move(*diagnostic);
  return {};
}

void LuaEngine::appendSourceDiagnostics(lua_State* from, lua_State* to, ExceptionSummary const& summary) const noexcept {
  if (!m_sourceResolver)
    return;
  String diagnostics;
  StringSet sources;
  lua_Debug frame;
  for (int level = 0; lua_getstack(from, level, &frame); ++level) {
    // "S" only reads the live frame, without pushing a function or invoking Lua.
    if (!lua_getinfo(from, "S", &frame) || !frame.source)
      continue;
    String source(std::string_view(frame.source, frame.srclen));
    if (!sources.insert(source).second)
      continue;
    auto location = std::ranges::find_if(summary.locations, [&](auto const& location) {
      return location.source == source.utf8();
    });
    auto provider = location == summary.locations.end() ? sourceDiagnostic(source) : String(location->provider);
    if (provider.empty())
      continue;
    diagnostics += "\n  ";
    diagnostics += source;
    diagnostics += ": ";
    diagnostics += provider;
  }
  if (diagnostics.empty() || !lua_checkstack(to, 3))
    return;
  diagnostics = "\nLua source providers:" + diagnostics;
  // Keep host ownership outside the protected C callback. Any Lua allocation
  // or finalizer error during the push/concat returns through pcall, not through
  // C++ destructors. Keep the original traceback if enrichment fails.
  lua_pushcfunction(to, [](lua_State* state) {
    auto diagnostic = static_cast<String const*>(lua_touserdata(state, 2));
    lua_pushvalue(state, 1);
    lua_pushlstring(state, diagnostic->utf8().data(), diagnostic->utf8().size());
    lua_concat(state, 2);
    return 1;
  });
  lua_pushvalue(to, -2);
  lua_pushlightuserdata(to, &diagnostics);
  if (lua_pcall(to, 2, 1, 0) == LUA_OK)
    lua_replace(to, -2);
  else
    lua_pop(to, 1);
}

int LuaEngine::coresumeWithTraceback(lua_State* state) {
  lua_State* co = lua_tothread(state, 1);
  if (!co) {
    lua_checkstack(state, 2);
    lua_pushboolean(state, 0);
    lua_pushliteral(state, "bad argument #1 to 'resume' (thread expected)");
    return 2;
  }

  int args = lua_gettop(state) - 1;
  lua_checkstack(co, args);
  if (lua_status(co) == LUA_OK && lua_gettop(co) == 0) {
    lua_checkstack(state, 2);
    lua_pushboolean(state, 0);
    lua_pushliteral(state, "cannot resume dead coroutine");
    return 2;
  }

  lua_xmove(state, co, args);
  int res = 0;
  int status = lua_resume(co, state, args, &res);
  if (status == LUA_OK || status == LUA_YIELD) {
    lua_checkstack(state, res + 1);
    lua_pushboolean(state, 1);
    lua_xmove(co, state, res);
    return res + 1;
  } else {
    lua_checkstack(state, 2);
    lua_pushboolean(state, 0);
    propagateErrorWithTraceback(co, state);
    return 2;
  }
}

void LuaEngine::propagateErrorWithTraceback(lua_State* from, lua_State* to) {
  lua_checkstack(to, 4);
  lua_xmove(from, to, 1);
  lua_pushcfunction(to, [](lua_State* state) {
    auto from = static_cast<lua_State*>(lua_touserdata(state, 2));
    auto& self = luaEngine(state);
    if (lua_type(state, 1) == LUA_TNUMBER)
      lua_tolstring(state, 1, nullptr);
    {
      auto summary = std::make_shared<ExceptionSummary const>(
          self.captureDiagnostic(from, state, 1, ExceptionSummaryKind::ScriptRuntime));
      self.rememberDiagnostic(state, 1, std::move(summary));
    }
    if (auto error = lua_tostring(state, 1)) {
      luaL_traceback(state, from, error, 0);
      auto summary = self.findDiagnostic(state, 1);
      if (summary) {
        self.appendSourceDiagnostics(from, state, *summary);
        self.rememberDiagnostic(state, -1, std::move(summary));
      }
    } else {
      lua_pushvalue(state, 1);
    }
    return 1;
  });
  lua_insert(to, -2);
  lua_pushlightuserdata(to, from);
  // A traceback allocation failure is itself the outgoing error object.
  (void)lua_pcall(to, 2, 1, 0);
}


std::size_t LuaEngine::stringLength(int handleIndex) {
  pushHandle(m_state, handleIndex);
  std::size_t len = 0;
  lua_tolstring(m_state, -1, &len);
  lua_pop(m_state, 1);
  return len;
}

String LuaEngine::string(int handleIndex) {
  pushHandle(m_state, handleIndex);
  std::size_t length = 0;
  auto data = lua_tolstring(m_state, -1, &length);
  lua_pop(m_state, 1);
  return String(std::string_view(data, length));
}

StringView LuaEngine::stringView(int handleIndex) {
  pushHandle(m_state, handleIndex);
  std::size_t length = 0;
  auto data = lua_tolstring(m_state, -1, &length);
  lua_pop(m_state, 1);
  return StringView(std::string_view(data, length));
}

std::expected<std::optional<LuaTable>, StarException> LuaEngine::tableGetMetatable(int handleIndex) {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while getting a metatable"));
  pushHandle(m_state, handleIndex);
  if (lua_getmetatable(m_state, -1) == 0)
    return std::optional<LuaTable>();
  auto handle = popHandle(m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaTable(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
}

std::expected<void, StarException> LuaEngine::tableSetMetatable(int handleIndex, LuaTable const& table) {
  if (&table.engine() != this)
    return std::unexpected(LuaException("lua reference values cannot be shared between engines"));
  if (!lua_checkstack(m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while setting a metatable"));
  pushHandle(m_state, handleIndex);
  pushHandle(m_state, table.handleIndex());
  lua_setmetatable(m_state, -2);
  lua_pop(m_state, 1);
  return {};
}

std::expected<void, StarException> LuaEngine::setContextRequire(int handleIndex, LuaContext::RequireFunction requireFunction) {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 4))
    return std::unexpected(LuaException("Exhausted the Lua stack while installing require"));

  auto invokeRequire = [](lua_State* state) {
    {
      auto invoke = [state]() -> std::expected<void, StarException> {
        if (!lua_checkstack(state, 2))
          return std::unexpected(LuaException("Exhausted the Lua stack while invoking require"));
        auto& require = *static_cast<LuaContext::RequireFunction*>(lua_touserdata(state, lua_upvalueindex(1)));
        auto& self = luaEngine(state);
        if (lua_gettop(state) == 0)
          return std::unexpected(LuaException("require expects a module name"));
        LuaStack stack(self, state);
        auto module = stack.value(-1);
        if (!module.is<LuaString>())
          return std::unexpected(LuaConversionException("require expects a string module name"));
        auto moduleName = module.stringView();
        lua_pushvalue(state, lua_upvalueindex(2));
        if (!require)
          return std::unexpected(LuaException("Cannot invoke an empty Lua require function"));
        return require(stack.value(-1), moduleName);
      };
      auto result = invoke();
      if (result)
        return 0;
      lua_settop(state, 0);
      luaEngine(state).pushCallbackError(state, result.error());
    }
    // Every context, value and diagnostic has been released before longjmp.
    return lua_error(state);
  };

  struct Input {
    std::reference_wrapper<LuaContext::RequireFunction> function;
    int metatable;
    lua_CFunction invoke;
  };
  Input input{std::ref(requireFunction), m_requireFunctionMetatableRegistryId, invokeRequire};
  lua_pushcfunction(m_state, [](lua_State* state) {
    auto& input = *static_cast<Input*>(lua_touserdata(state, 2));
    auto function = (LuaContext::RequireFunction*)lua_newuserdatauv(state, sizeof(LuaContext::RequireFunction), 0);
    new (function) LuaContext::RequireFunction(std::move(input.function.get()));
    lua_rawgeti(state, LUA_REGISTRYINDEX, input.metatable);
    lua_setmetatable(state, -2);
    lua_pushvalue(state, 1);
    lua_pushcclosure(state, input.invoke, 2);
    LuaDetail::rawSetField(state, 1, "require");
    return 0;
  });
  pushHandle(m_state, handleIndex);
  lua_pushlightuserdata(m_state, &input);
  return handleError(m_state, pcallWithTraceback(m_state, 2, 0));
}

std::expected<int, StarException> LuaEngine::contextEval(int handleIndex, String const& lua, int nresults) {
  int top = lua_gettop(m_state);
  if (!lua_checkstack(m_state, nresults == LUA_MULTRET ? 2 : std::max(2, nresults + 1)))
    return std::unexpected(LuaException("Exhausted the Lua stack while evaluating a script"));
  auto expression = "return " + lua;
  int loadRes = luaL_loadstring(m_state, expression.utf8_data());
  if (loadRes == LUA_ERRSYNTAX) {
    lua_pop(m_state, 1);
    loadRes = luaL_loadstring(m_state, lua.utf8_data());
  }
  if (auto loaded = handleError(m_state, loadRes); !loaded)
    return std::unexpected(std::move(loaded.error()));
  pushHandle(m_state, handleIndex);
  lua_setupvalue(m_state, -2, 1);
  if (auto entered = incrementRecursionLevel(); !entered)
    return std::unexpected(std::move(entered.error()));
  int result = pcallWithTraceback(m_state, 0, nresults);
  decrementRecursionLevel();
  if (auto called = handleError(m_state, result); !called)
    return std::unexpected(std::move(called.error()));
  return lua_gettop(m_state) - top;
}

std::expected<bool, StarException> LuaEngine::pushContextPath(int handleIndex, std::string_view path) {
  LuaStackRestore restore(m_state);
  if (!lua_checkstack(m_state, 4))
    return std::unexpected(LuaException("Exhausted the Lua stack while reading a context path"));
  lua_pushcfunction(m_state, [](lua_State* state) {
    auto path = *static_cast<std::string_view*>(lua_touserdata(state, 2));
    lua_pop(state, 1);
    std::size_t start = 0;
    for (std::size_t i = 0; i <= path.size(); ++i) {
      if (i != path.size() && path[i] != '.')
        continue;
      lua_pushlstring(state, path.data() + start, i - start);
      lua_gettable(state, -2);
      lua_remove(state, -2);
      if (i != path.size() && !lua_istable(state, -1)) {
        lua_pushnil(state);
        return 1;
      }
      start = i + 1;
    }
    return 1;
  });
  pushHandle(m_state, handleIndex);
  lua_pushlightuserdata(m_state, &path);
  auto found = handleError(m_state, pcallWithTraceback(m_state, 2, 1));
  if (!found)
    return std::unexpected(std::move(found.error()));
  ++restore.top;
  return lua_isfunction(m_state, -1) != 0;
}

std::expected<LuaValue, StarException> LuaEngine::contextGetPath(int handleIndex, String path) {
  LuaStackRestore restore(m_state);
  auto found = pushContextPath(handleIndex, path.utf8());
  if (!found)
    return std::unexpected(std::move(found.error()));
  return popLuaValue(m_state);
}

std::expected<void, StarException> LuaEngine::contextSetPath(int handleIndex, String path, LuaValue const& value) {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 5))
    return std::unexpected(LuaException("Exhausted the Lua stack while setting a context path"));
  auto utf8Path = path.take_utf8();
  std::span<char> pathData(utf8Path);
  lua_pushcfunction(m_state, [](lua_State* state) {
    auto& path = *static_cast<std::span<char>*>(lua_touserdata(state, 2));
    lua_pushvalue(state, 1);
    std::size_t start = 0;
    for (std::size_t i = 0; i < path.size(); ++i) {
      if (path[i] != '.')
        continue;
      path[i] = '\0';
      int type = lua_getfield(state, -1, path.data() + start);
      if (type == LUA_TNIL) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_pushvalue(state, -1);
        lua_setfield(state, -3, path.data() + start);
        lua_remove(state, -2);
      } else if (type == LUA_TTABLE) {
        lua_remove(state, -2);
      } else {
        return luaL_error(state, "Sub-path in setPath is not nil and is not a table");
      }
      start = i + 1;
    }
    lua_pushvalue(state, 3);
    lua_setfield(state, -2, path.data() + start);
    return 0;
  });
  pushHandle(m_state, handleIndex);
  lua_pushlightuserdata(m_state, &pathData);
  auto pushed = pushLuaValue(m_state, value);
  if (!pushed)
    return std::unexpected(std::move(pushed.error()));
  return handleError(m_state, pcallWithTraceback(m_state, 3, 0));
}

std::expected<int, StarException> LuaEngine::popHandle(lua_State* state) {
  LuaStackRestore stack(state);
  --stack.top;
  if (!lua_checkstack(state, 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while acquiring a registry reference"));
  lua_pushcfunction(state, [](lua_State* state) {
    lua_pushvalue(state, 1);
    int reference = luaL_ref(state, LUA_REGISTRYINDEX);
    lua_pushinteger(state, reference);
    return 1;
  });
  lua_insert(state, -2);
  auto acquired = handleError(state, pcallWithTraceback(state, 1, 1));
  if (!acquired)
    return std::unexpected(std::move(acquired.error()));
  return (int)lua_tointeger(state, -1);
}

void LuaEngine::pushHandle(lua_State* state, int handleIndex) {
  lua_rawgeti(state, LUA_REGISTRYINDEX, handleIndex);
}

std::expected<int, StarException> LuaEngine::copyHandle(int handleIndex) {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while copying a registry reference"));
  pushHandle(m_state, handleIndex);
  return popHandle(m_state);
}

std::expected<LuaFunction, StarException> LuaEngine::createWrappedFunction(LuaDetail::LuaWrappedFunction function) {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a function"));

  auto invokeFunction = [](lua_State* state) {
    auto& function = *static_cast<LuaDetail::LuaWrappedFunction*>(lua_touserdata(state, lua_upvalueindex(1)));
    auto& self = luaEngine(state);
    {
      auto invoke = [&]() -> std::expected<int, StarException> {
        if (!function)
          return std::unexpected(LuaException("Cannot invoke an empty Lua callback"));
        for (int i = 1, count = lua_gettop(state); i <= count; ++i) {
          if (auto valid = validateNativeLuaValue(state, i); !valid)
            return std::unexpected(std::move(valid.error()));
        }
        return function(LuaStack(self, state));
      };
      auto result = invoke();
      if (result)
        return *result;
      lua_settop(state, 0);
      self.pushCallbackError(state, result.error());
    }
    return lua_error(state);
  };

  struct Input {
    std::reference_wrapper<LuaDetail::LuaWrappedFunction> function;
    int metatable;
    lua_CFunction invoke;
  };
  Input input{std::ref(function), m_wrappedFunctionMetatableRegistryId, invokeFunction};
  lua_pushcfunction(m_state, [](lua_State* state) {
    auto& input = *static_cast<Input*>(lua_touserdata(state, 1));
    auto function = (LuaDetail::LuaWrappedFunction*)lua_newuserdatauv(state, sizeof(LuaDetail::LuaWrappedFunction), 0);
    new (function) LuaDetail::LuaWrappedFunction(std::move(input.function.get()));
    lua_rawgeti(state, LUA_REGISTRYINDEX, input.metatable);
    lua_setmetatable(state, -2);
    lua_pushcclosure(state, input.invoke, 1);
    return 1;
  });
  lua_pushlightuserdata(m_state, &input);
  auto created = handleError(m_state, pcallWithTraceback(m_state, 1, 1));
  if (!created)
    return std::unexpected(std::move(created.error()));

  auto handle = popHandle(m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaFunction(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
}

std::expected<LuaFunction, StarException> LuaEngine::createRawFunction(lua_CFunction function) {
  if (!function)
    return std::unexpected(LuaException("Cannot create a Lua function from a null function pointer"));
  if (!lua_checkstack(m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a C function"));

  lua_pushcfunction(m_state, function);
  auto handle = popHandle(m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaFunction(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
}

std::expected<LuaFunction, StarException> LuaEngine::createFunctionFromSource(int handleIndex, std::span<char const> contents, String const& name) {
  LuaStackRestore stack(m_state);
  if (!lua_checkstack(m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while loading a function"));
  auto loaded = handleSourceError(m_state, luaL_loadbufferx(m_state, contents.data(), contents.size(), name.empty() ? nullptr : name.utf8().c_str(), "t"), name);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));

  pushHandle(m_state, handleIndex);
  lua_setupvalue(m_state, -2, 1);

  auto handle = popHandle(m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaFunction(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
}

std::expected<void, StarException> LuaEngine::pushLuaValue(lua_State* state, LuaValue const& luaValue) {
  if (!lua_checkstack(state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while pushing a value"));
  return std::visit([&engine = *this, state](auto const& value) -> std::expected<void, StarException> {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, LuaNilType>) {
      lua_pushnil(state);
    } else if constexpr (std::is_same_v<T, LuaBoolean>) {
      lua_pushboolean(state, value);
    } else if constexpr (std::is_same_v<T, LuaInt>) {
      lua_pushinteger(state, value);
    } else if constexpr (std::is_same_v<T, LuaFloat>) {
      lua_pushnumber(state, value);
    } else {
      if (&value.engine() != &engine)
        return std::unexpected(LuaException("lua reference values cannot be shared between engines"));
      engine.pushHandle(state, value.handleIndex());
    }
    return {};
  }, luaValue);
}

std::expected<LuaValue, StarException> LuaEngine::popLuaValue(lua_State* state) {
  if (!lua_checkstack(state, 1)) {
    lua_pop(state, 1);
    return std::unexpected(LuaException("Exhausted the Lua stack while reading a value"));
  }

  LuaValue result;
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((!lua_isnone(state, -1)));
  }
  switch (lua_type(state, -1)) {
    case LUA_TNIL: {
      lua_pop(state, 1);
      break;
    }
    case LUA_TBOOLEAN: {
      result = lua_toboolean(state, -1) != 0;
      lua_pop(state, 1);
      break;
    }
    case LUA_TNUMBER: {
      if (lua_isinteger(state, -1)) {
        result = lua_tointeger(state, -1);
        lua_pop(state, 1);
      } else {
        result = lua_tonumber(state, -1);
        lua_pop(state, 1);
      }
      break;
    }
    case LUA_TSTRING: {
      auto handle = popHandle(state);
      if (!handle)
        return std::unexpected(std::move(handle.error()));
      result = LuaString(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
      break;
    }
    case LUA_TTABLE: {
      auto handle = popHandle(state);
      if (!handle)
        return std::unexpected(std::move(handle.error()));
      result = LuaTable(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
      break;
    }
    case LUA_TFUNCTION: {
      auto handle = popHandle(state);
      if (!handle)
        return std::unexpected(std::move(handle.error()));
      result = LuaFunction(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
      break;
    }
    case LUA_TTHREAD: {
      auto handle = popHandle(state);
      if (!handle)
        return std::unexpected(std::move(handle.error()));
      result = LuaThread(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
      break;
    }
    case LUA_TUSERDATA: {
      if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        return std::unexpected(LuaException("Userdata in popLuaValue missing metatable"));
      }
      lua_pop(state, 1);
      auto handle = popHandle(state);
      if (!handle)
        return std::unexpected(std::move(handle.error()));
      result = LuaUserData(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *handle));
      break;
    }
    default: {
      lua_pop(state, 1);
      return std::unexpected(LuaException("Unsupported type in popLuaValue"));
    }
  }

  return result;
}

std::expected<void, StarException> LuaEngine::incrementRecursionLevel() {
  // We reset the instruction count and profiling timing only on the *top
  // level* function entrance, not on recursive entrances.
  if (m_recursionLevel == 0) {
    m_instructionCount = 0;
  }

  if (m_recursionLimit != 0 && m_recursionLevel == m_recursionLimit)
    return std::unexpected(LuaRecursionLimitReached());

  ++m_recursionLevel;
  return {};
}

void LuaEngine::decrementRecursionLevel() {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((m_recursionLevel != 0));
  }
  --m_recursionLevel;
}

void LuaEngine::updateCountHook() {
  if (m_instructionLimit || m_profilingEnabled)
    lua_sethook(m_state, &LuaEngine::countHook, LUA_MASKCOUNT, m_instructionMeasureInterval);
  else
    lua_sethook(m_state, &LuaEngine::countHook, 0, 0);
}

int LuaEngine::s_luaInstructionLimitExceptionKey = 0;
int LuaEngine::s_luaRecursionLimitExceptionKey = 0;

void LuaDetail::rawSetField(lua_State* state, int index, std::string_view key) {
  lua_checkstack(state, 1);

  int absTableIndex = lua_absindex(state, index);
  lua_pushlstring(state, key.data(), key.size());

  // Move the newly pushed key to the secont to top spot, leaving the value in
  // the top spot.
  lua_insert(state, -2);

  // Pops the value and the key
  lua_rawset(state, absTableIndex);
}

void LuaDetail::shallowCopy(lua_State* state, int sourceIndex, int targetIndex) {
  lua_checkstack(state, 3);

  int absSourceIndex = lua_absindex(state, sourceIndex);
  int absTargetIndex = lua_absindex(state, targetIndex);

  lua_pushnil(state);
  while (lua_next(state, absSourceIndex) != 0) {
    lua_pushvalue(state, -2);
    lua_insert(state, -2);
    lua_rawset(state, absTargetIndex);
  }
}

std::expected<void, StarException> LuaDetail::insertJsonMetatable(LuaEngine& engine, LuaTable const& table,
                                                                  Json::Type type) {
  LuaStackRestore restore(engine.m_state);
  LuaStack stack(engine, engine.m_state);
  if (auto pushed = stack.push(table); !pushed)
    return std::unexpected(std::move(pushed.error()));
  auto nils = pushJsonMetadata(stack.value(-1), type);
  if (!nils)
    return std::unexpected(std::move(nils.error()));
  return {};
}

std::expected<LuaTable, StarException> LuaDetail::jsonContainerToTable(LuaEngine& engine, Json const& container) {
  if (!container.isType(Json::Type::Array) && !container.isType(Json::Type::Object))
    return std::unexpected(LuaException("jsonContainerToTable called on improper json type"));
  auto result = engine.luaFrom(container);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::move(std::get<LuaTable>(*result));
}

std::expected<std::optional<Json>, StarException> LuaEngine::stackToJson(lua_State* state, int index) {
  LuaStackRestore restore(state);
  index = lua_absindex(state, index);
  switch (lua_type(state, index)) {
  case LUA_TNONE:
  case LUA_TNIL: return Json();
  case LUA_TBOOLEAN: return Json(lua_toboolean(state, index) != 0);
  case LUA_TNUMBER:
    if (lua_isinteger(state, index))
      return Json(lua_tointeger(state, index));
    return Json(lua_tonumber(state, index));
  case LUA_TSTRING: {
    std::size_t size;
    char const* data = lua_tolstring(state, index, &size);
    return Json(String(std::string_view(data, size)));
  }
  case LUA_TTABLE: break;
  default: return std::optional<Json>();
  }
  if (!lua_checkstack(state, 8))
    return std::unexpected(LuaException("Exhausted the Lua stack while reading JSON"));

  // Only the metadata lookup can invoke user metamethods. Keep it protected;
  // all C++ JSON ownership remains outside Lua's longjmp boundary.
  lua_pushcfunction(state, [](lua_State* state) {
    if (!lua_getmetatable(state, 1)) {
      lua_pushnil(state);
      lua_pushnil(state);
    } else {
      lua_getfield(state, -1, "__typehint");
      lua_getfield(state, -2, "__nils");
    }
    return 2;
  });
  lua_pushvalue(state, index);
  auto metadata = handleError(state, pcallWithTraceback(state, 1, 2));
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto hint = LuaStack(*this, state).read<std::optional<int>>(-2);
  if (!hint)
    return std::unexpected(std::move(hint.error()));
  int typeHint = hint->value_or(0);
  int nilsIndex = lua_gettop(state);
  if (!lua_isnil(state, nilsIndex) && !lua_istable(state, nilsIndex))
    return std::unexpected(LuaConversionException("JSON null tracking value is not a table"));

  JsonObject stringEntries;
  struct Entry {
    unsigned index;
    std::size_t order;
    Json value;
  };
  std::vector<Entry> intEntries;
  intEntries.reserve(lua_rawlen(state, index));
  bool ordered = true;
  auto visit = [&](int tableIndex, bool nulls) -> std::expected<bool, StarException> {
    LuaStackRestore cursorRestore(state);
    lua_pushnil(state);
    int cursor = lua_gettop(state);
    while (true) {
      lua_pushcfunction(state, [](lua_State* state) { return lua_next(state, 1) ? 2 : 0; });
      lua_pushvalue(state, tableIndex);
      lua_pushvalue(state, cursor);
      auto next = handleError(state, pcallWithTraceback(state, 2, LUA_MULTRET));
      if (!next)
        return std::unexpected(std::move(next.error()));
      lua_remove(state, cursor);
      if (lua_gettop(state) < cursor)
        return true;
      // Preserve the native boundary's error precedence even when the value
      // is ignored by null tracking or a different value cannot become JSON.
      for (int position : {cursor, cursor + 1}) {
        auto representable = validateNativeLuaValue(state, position);
        if (!representable)
          return std::unexpected(std::move(representable.error()));
      }
      Json value;
      if (!nulls) {
        auto converted = stackToJson(state, cursor + 1);
        if (!converted)
          return std::unexpected(std::move(converted.error()));
        if (!*converted)
          return false;
        value = std::move(**converted);
      }
      int keyType = lua_type(state, cursor);
      if (keyType == LUA_TNUMBER) {
        LuaValue key =
          lua_isinteger(state, cursor) ? LuaValue(lua_tointeger(state, cursor)) : LuaValue(lua_tonumber(state, cursor));
        if (auto integer = LuaDetail::asInteger(key)) {
          unsigned keyIndex = static_cast<unsigned>(*integer);
          if (!intEntries.empty() && intEntries.back().index >= keyIndex)
            ordered = false;
          intEntries.push_back({keyIndex, intEntries.size(), std::move(value)});
        } else {
          stringEntries[String(Star::to_string(lua_tonumber(state, cursor)))] = std::move(value);
        }
      } else if (keyType == LUA_TSTRING) {
        std::size_t size;
        char const* data = lua_tolstring(state, cursor, &size);
        stringEntries[String(std::string_view(data, size))] = std::move(value);
      } else {
        return false;
      }
      lua_pop(state, 1);// The next iteration retains only the original key.
    }
  };
  if (lua_istable(state, nilsIndex)) {
    auto visited = visit(nilsIndex, true);
    if (!visited)
      return std::unexpected(std::move(visited.error()));
    if (!*visited)
      return std::optional<Json>();
  }
  auto visited = visit(index, false);
  if (!visited)
    return std::unexpected(std::move(visited.error()));
  if (!*visited)
    return std::optional<Json>();
  if (!ordered) {
    std::sort(intEntries.begin(), intEntries.end(), [](Entry const& a, Entry const& b) {
      return a.index < b.index || (a.index == b.index && a.order < b.order);
    });
    std::size_t count = 0;
    for (std::size_t i = 0; i < intEntries.size(); ++i) {
      if (count && intEntries[count - 1].index == intEntries[i].index)
        intEntries[count - 1].value = std::move(intEntries[i].value);
      else {
        if (count != i)
          intEntries[count] = std::move(intEntries[i]);
        ++count;
      }
    }
    intEntries.resize(count);
  }
  bool interpretAsList = stringEntries.empty()
      && (typeHint == 1 || (typeHint != 2 && !intEntries.empty() && intEntries.back().index == intEntries.size()));
  if (interpretAsList) {
    if (!intEntries.empty() && intEntries.front().index == 0)
      return std::unexpected(LuaException("JSON array index must be positive"));
    JsonArray list(intEntries.empty() ? 0 : intEntries.back().index);
    for (auto& entry : intEntries)
      list[entry.index - 1] = std::move(entry.value);
    return Json(std::move(list));
  }
  for (auto& entry : intEntries)
    stringEntries[Star::to_string(entry.index)] = std::move(entry.value);
  return Json(std::move(stringEntries));
}

Json LuaDetail::jarrayCreate() {
  return JsonArray();
}

Json LuaDetail::jobjectCreate() {
  return JsonObject();
}

std::expected<LuaTable, StarException> LuaDetail::jarray(LuaEngine& engine, std::optional<LuaTable> table) {
  if (table) {
    auto inserted = insertJsonMetatable(engine, *table, Json::Type::Array);
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
    return std::move(*table);
  }
  return jsonContainerToTable(engine, JsonArray());
}

std::expected<LuaTable, StarException> LuaDetail::jobject(LuaEngine& engine, std::optional<LuaTable> table) {
  if (table) {
    auto inserted = insertJsonMetatable(engine, *table, Json::Type::Object);
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
    return std::move(*table);
  }
  return jsonContainerToTable(engine, JsonObject());
}

std::expected<void, StarException> LuaDetail::jcontRemove(LuaTable const& table, LuaValue const& key) {
  auto mt = table.getMetatable();
  if (!mt)
    return std::unexpected(std::move(mt.error()));
  if (*mt) {
    auto nils = (*mt)->rawGet<std::optional<LuaTable>>("__nils");
    if (!nils)
      return std::unexpected(std::move(nils.error()));
    if (*nils) {
      auto removed = (**nils).rawSet(key, LuaNil);
      if (!removed)
        return std::unexpected(std::move(removed.error()));
    }
  }
  return table.rawSet(key, LuaNil);
}

std::expected<std::size_t, StarException> LuaDetail::jcontSize(LuaTable const& table) {
  std::size_t elemCount = 0;
  std::size_t highestIndex = 0;
  bool hintList = false;
  auto count = [&](LuaValue const& key, LuaValue const&) -> std::expected<void, StarException> {
    auto i = asInteger(key);
    if (i && *i >= 0)
      highestIndex = std::max<std::size_t>(*i, highestIndex);
    else
      hintList = false;
    ++elemCount;
    return {};
  };
  auto mt = table.getMetatable();
  if (!mt)
    return std::unexpected(std::move(mt.error()));
  if (*mt) {
    auto hint = (*mt)->rawGet<std::optional<int>>("__typehint");
    if (!hint)
      return std::unexpected(std::move(hint.error()));
    hintList = *hint == 1;
    auto nils = (*mt)->rawGet<std::optional<LuaTable>>("__nils");
    if (!nils)
      return std::unexpected(std::move(nils.error()));
    if (*nils) {
      auto counted = (**nils).iterate(count);
      if (!counted)
        return std::unexpected(std::move(counted.error()));
    }
  }
  auto counted = table.iterate(count);
  if (!counted)
    return std::unexpected(std::move(counted.error()));
  return hintList ? highestIndex : elemCount;
}

std::expected<void, StarException> LuaDetail::jcontResize(LuaTable const& table, std::size_t targetSize) {
  auto mt = table.getMetatable();
  if (!mt)
    return std::unexpected(std::move(mt.error()));
  if (*mt) {
    auto nils = (*mt)->rawGet<std::optional<LuaTable>>("__nils");
    if (!nils)
      return std::unexpected(std::move(nils.error()));
    if (*nils) {
      auto resized = (**nils).iterate([&](LuaValue const& key, LuaValue const&) -> std::expected<void, StarException> {
        auto i = asInteger(key);
        if (i && *i > 0 && (std::size_t)*i > targetSize)
          return (**nils).rawSet(key, LuaNil);
        return {};
      });
      if (!resized)
        return std::unexpected(std::move(resized.error()));
    }
  }
  auto resized = table.iterate([&](LuaValue const& key, LuaValue const&) -> std::expected<void, StarException> {
    auto i = asInteger(key);
    if (i && *i > 0 && (std::size_t)*i > targetSize)
      return table.rawSet(key, LuaNil);
    return {};
  });
  if (!resized)
    return std::unexpected(std::move(resized.error()));
  auto last = table.get(targetSize);
  if (!last)
    return std::unexpected(std::move(last.error()));
  return table.set(targetSize, std::move(*last));
}

std::optional<LuaInt> LuaDetail::asInteger(LuaValue const& v) {
  if (std::holds_alternative<LuaInt>(v))
    return std::get<LuaInt>(v);
  if (std::holds_alternative<LuaFloat>(v)) {
    auto f = std::get<LuaFloat>(v);
    if (!std::isfinite(f) || f < LuaFloat(std::numeric_limits<LuaInt>::min())
        || f >= -LuaFloat(std::numeric_limits<LuaInt>::min()))
      return {};
    if ((LuaFloat)(LuaInt)f == f)
      return (LuaInt)f;
    return {};
  }
  return {};
}

}
