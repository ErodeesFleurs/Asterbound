#include <cstdio>
extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}
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
using LuaDetail::LuaStackRestore;

// Consume one intermediate key, leaving the last key (possibly empty) in path.
std::optional<std::string_view> take_path_prefix(std::string_view& path) {
  auto dot = path.find('.');
  if (dot == std::string_view::npos)
    return std::nullopt;
  auto key = path.substr(0, dot);
  path.remove_prefix(dot + 1);
  return key;
}

std::expected<void, StarException> validate_native_lua_value(lua_State& state, int index) {
  int type = lua_type(&state, index);
  if (type == LUA_TLIGHTUSERDATA)
    return std::unexpected(LuaException("Unsupported type in popLuaValue"));
  if (type == LUA_TUSERDATA) {
    if (!lua_getmetatable(&state, index))
      return std::unexpected(LuaException("Userdata in popLuaValue missing metatable"));
    lua_pop(&state, 1);
  }
  return {};
}
int json_new_index(lua_State* state) {
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

std::expected<LuaStackValue, StarException> push_json_metadata(LuaStackValue table, Json::Type type) {
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
      lua_pushcfunction(state, json_new_index);
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
std::expected<void, StarException> push_json_container(LuaStack stack, Container const& values) {
  if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return std::unexpected(LuaException("Too many entries for a Lua JSON container"));
  constexpr bool array = std::is_same_v<Container, JsonArray>;
  auto size = static_cast<int>(values.size());
  auto table = stack.create_table(array ? size : 0, array ? 0 : size);
  if (!table)
    return std::unexpected(std::move(table.error()));
  auto nils = push_json_metadata(*table, array ? Json::Type::Array : Json::Type::Object);
  if (!nils)
    return std::unexpected(std::move(nils.error()));
  if constexpr (array) {
    for (std::size_t i = 0; i < values.size(); ++i) {
      auto set = values[i] ? table->raw_set(i + 1, values[i]) : nils->raw_set(i + 1, 0);
      if (!set)
        return std::unexpected(std::move(set.error()));
    }
  } else {
    for (auto const& [key, value] : values) {
      auto set = value ? table->raw_set(key, value) : nils->raw_set(key, 0);
      if (!set)
        return std::unexpected(std::move(set.error()));
    }
  }
  stack.pop();
  return {};
}
}// namespace

std::expected<void, StarException> LuaStack::push_callback_table(LuaCallbacks const& callbacks) const {
  LuaStackRestore restore(state());
  auto const& entries = callbacks.callbacks();
  if (entries.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    return std::unexpected(LuaException("Too many callbacks for a Lua table"));
  auto table = create_table(0, static_cast<int>(entries.size()));
  if (!table)
    return std::unexpected(std::move(table.error()));
  for (auto const& [name, callback] : entries) {
    if (auto installed = table->set(name, callback); !installed)
      return installed;
  }
  ++restore.top;
  return {};
}

std::expected<void, StarException> LuaStack::call(lua_CFunction function, int nargs, int nresults) const {
  if (!lua_checkstack(&m_state, nresults == LUA_MULTRET ? 2 : std::max(2, nresults + 1)))
    return std::unexpected(LuaException("Exhausted the Lua stack while entering a protected call"));
  lua_pushcfunction(&m_state, function);
  lua_insert(&m_state, -nargs - 1);
  return m_engine.handle_error(m_state, m_engine.pcall_with_traceback(m_state, nargs, nresults));
}

std::expected<void, StarException> LuaStack::load(LuaStackValue environment, std::span<char const> contents,
                                                  String const& name) const {
  if (&environment.stack().engine() != &m_engine)
    return std::unexpected(LuaException("Lua environments cannot be shared between engines"));
  if (&environment.stack().state() != &m_state)
    return std::unexpected(LuaException("Lua environments must belong to the loading stack"));
  if (!environment.is<LuaTable>())
    return std::unexpected(LuaConversionException("Lua script environment must be a table"));
  LuaStackRestore restore(m_state);
  if (!lua_checkstack(&m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while loading a script"));
  auto loaded = m_engine.handle_source_error(
    m_state, luaL_loadbuffer(&m_state, contents.data(), contents.size(), name.empty() ? nullptr : name.utf8().c_str()), name);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  lua_pushvalue(&m_state, environment.index());
  lua_setupvalue(&m_state, -2, 1);
  auto entered = m_engine.increment_recursion_level();
  if (!entered)
    return std::unexpected(std::move(entered.error()));
  int res = m_engine.pcall_with_traceback(m_state, 0, 0);
  m_engine.decrement_recursion_level();
  return m_engine.handle_error(m_state, res);
}

std::expected<void, StarException> LuaStack::push_string(std::string_view value) const {
  if (!lua_checkstack(&m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while pushing a string"));
  lua_pushlightuserdata(&m_state, &value);
  return call(
    +[](lua_State* state) {
      auto& value = *static_cast<std::string_view*>(lua_touserdata(state, 1));
      lua_pushlstring(state, value.data(), value.size());
      return 1;
    },
    1, 1);
}

std::expected<LuaStackValue, StarException> LuaStack::create_table(int narr, int nrec) const {
  if (!lua_checkstack(&m_state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a table"));
  int sizes[] = {narr, nrec};
  lua_pushlightuserdata(&m_state, sizes);
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

StringView LuaStackValue::string_view() const {
  std::size_t size;
  auto data = lua_tolstring(&m_stack.state(), m_index, &size);
  return StringView(std::string_view(data, size));
}

std::expected<void, StarException> LuaStackValue::validate() const {
  if (!lua_checkstack(&m_stack.state(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while reading a value"));
  return validate_native_lua_value(m_stack.state(), m_index);
}

std::expected<LuaValue, StarException> LuaStackValue::own() const {
  if (auto valid = validate(); !valid)
    return std::unexpected(std::move(valid.error()));
  lua_pushvalue(&m_stack.state(), m_index);
  return m_stack.engine().pop_lua_value(m_stack.state());
}

std::expected<LuaInt, StarException> LuaStackValue::raw_length() const {
  return static_cast<LuaInt>(lua_rawlen(&m_stack.state(), m_index));
}

std::expected<LuaInt, StarException> LuaStackValue::length() const {
  auto& state = m_stack.state();
  LuaStackRestore restore(state);
  if (!lua_checkstack(&state, 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while measuring a table"));
  lua_pushvalue(&state, m_index);
  auto measured = m_stack.call(
    +[](lua_State* state) {
      lua_len(state, 1);
      return 1;
    },
    1, 1);
  if (!measured)
    return std::unexpected(std::move(measured.error()));
  return static_cast<LuaInt>(lua_tointeger(&state, -1));
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
    append_text(output, std::get<LuaFunction>(value).handle_index());
    output.push_back('>');
  } else if (std::holds_alternative<LuaThread>(value)) {
    output.append("<thread reg:");
    append_text(output, std::get<LuaThread>(value).handle_index());
    output.push_back('>');
  } else if (std::holds_alternative<LuaUserData>(value)) {
    output.append("<userdata reg:");
    append_text(output, std::get<LuaUserData>(value).handle_index());
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

std::expected<void, StarException> LuaTable::remove(std::string_view key) const { return set(key, lua_nil); }

std::expected<LuaInt, StarException> LuaTable::length() const {
  auto& eng = engine();
  LuaStackRestore restore(*eng.m_state);
  if (!lua_checkstack(eng.m_state.get(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while measuring a table"));
  eng.push_handle(*eng.m_state, handle_index());
  return LuaStack(eng, *eng.m_state).value(-1).length();
}

std::expected<std::optional<LuaTable>, StarException> LuaTable::get_metatable() const {
  return engine().table_get_metatable(handle_index());
}

std::expected<void, StarException> LuaTable::set_metatable(LuaTable const& table) const {
  return engine().table_set_metatable(handle_index(), table);
}

std::expected<LuaInt, StarException> LuaTable::raw_length() const {
  auto& eng = engine();
  LuaStackRestore restore(*eng.m_state);
  if (!lua_checkstack(eng.m_state.get(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while measuring a table"));
  eng.push_handle(*eng.m_state, handle_index());
  return LuaStack(eng, *eng.m_state).value(-1).raw_length();
}

std::expected<void, StarException> LuaCallbacks::copy_callback(String src_name, String dst_name) {
  auto callback = m_callbacks.get(src_name);
  if (!callback)
    return std::unexpected(std::move(callback.error()));
  m_callbacks.set(dst_name, callback->get());
  return {};
}

bool LuaCallbacks::remove_callback(String name) {
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

LuaCallbacks LuaCallbacks::with_lifetime(std::stop_token lifetime) const {
  LuaCallbacks guarded;
  for (auto const& entry : m_callbacks) {
    guarded.m_callbacks.set(entry.first,
                            [callback = entry.second, lifetime](LuaStack stack) -> std::expected<int, StarException> {
                              if (lifetime.stop_requested())
                                return std::unexpected(LuaException("Lua callback scope is retired"));
                              if (stack.engine().promise_lifetime() == lifetime)
                                return callback(stack);
                              auto previous = stack.engine().set_promise_lifetime(lifetime);
                              auto restored = make_scope_exit([&] { stack.engine().set_promise_lifetime(std::move(previous)); });
                              return callback(stack);
                            });
  }
  return guarded;
}

StringMap<LuaDetail::LuaWrappedFunction> const& LuaCallbacks::callbacks() const {
  return m_callbacks;
}

std::expected<bool, StarException> LuaContext::contains_path(std::string_view path) const {
  auto& eng = engine();
  LuaStackRestore restore(*eng.m_state);
  if (auto pushed = eng.push_context_path(handle_index(), path); !pushed)
    return std::unexpected(std::move(pushed.error()));
  auto value = LuaStack(eng, *eng.m_state).value(-1);
  if (auto valid = value.validate(); !valid)
    return std::unexpected(std::move(valid.error()));
  return !value.is<LuaNilType>();
}

std::expected<void, StarException> LuaContext::load(std::span<char const> contents, String const& name) {
  auto& eng = engine();
  LuaStackRestore restore(*eng.m_state);
  if (!lua_checkstack(eng.m_state.get(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while loading a context"));
  eng.push_handle(*eng.m_state, handle_index());
  LuaStack stack(eng, *eng.m_state);
  return stack.load(stack.value(-1), contents, name);
}

std::expected<void, StarException> LuaContext::load(String const& contents, String const& name) {
  return load(std::span(contents.utf8()), name);
}

std::expected<void, StarException> LuaContext::load(ByteArray const& contents, String const& name) {
  return load(contents.span(), name);
}

std::expected<void, StarException> LuaContext::set_require_function(RequireFunction require_function) {
  return engine().set_context_require(handle_index(), std::move(require_function));
}

std::expected<void, StarException> LuaContext::set_callbacks(String const& table_name, LuaCallbacks const& callbacks) const {
  auto& eng = engine();
  LuaStack stack(eng, *eng.m_state);
  LuaStackRestore restore(stack.state());
  if (!lua_checkstack(&stack.state(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while installing callbacks"));
  eng.push_handle(stack.state(), handle_index());
  auto globals = stack.value(-1);
  auto present = globals.contains(table_name);
  if (!present)
    return std::unexpected(std::move(present.error()));
  if (*present)
    return {};
  if (auto pushed = stack.push_callback_table(callbacks); !pushed)
    return pushed;
  return globals.set(table_name, stack.value(-1));
}

std::expected<LuaString, StarException> LuaContext::create_string(String const& str) {
  return engine().create_string(str);
}

std::expected<LuaString, StarException> LuaContext::create_string(std::string_view str) {
  return engine().create_string(str);
}

std::expected<LuaTable, StarException> LuaContext::create_table() {
  return engine().create_table();
}

std::expected<void, StarException> LuaConverter<Json>::from(LuaStack stack, Json const& v) {
  switch (v.type()) {
  case Json::Type::Null: return stack.push(lua_nil);
  case Json::Type::Float: {
    auto value = v.to_double();
    if (!value)
      return std::unexpected(std::move(value.error()));
    return stack.push(*value);
  }
  case Json::Type::Bool: {
    auto value = v.to_bool();
    if (!value)
      return std::unexpected(std::move(value.error()));
    return stack.push(*value);
  }
  case Json::Type::Int: {
    auto value = v.to_int();
    if (!value)
      return std::unexpected(std::move(value.error()));
    return stack.push(*value);
  }
  case Json::Type::String: {
    auto value = v.string_ptr();
    if (!value)
      return std::unexpected(std::move(value.error()));
    return stack.push(**value);
  }
  case Json::Type::Array: {
    auto values = v.array_ptr();
    if (!values)
      return std::unexpected(std::move(values.error()));
    return push_json_container(stack, **values);
  }
  case Json::Type::Object: {
    auto values = v.object_ptr();
    if (!values)
      return std::unexpected(std::move(values.error()));
    return push_json_container(stack, **values);
  }
  }
  std::unreachable();
}

std::expected<std::optional<Json>, StarException> LuaConverter<Json>::to(LuaStackValue v) {
  return v.stack().engine().stack_to_json(v.stack().state(), v.index());
}

std::expected<void, StarException> LuaConverter<JsonObject>::from(LuaStack stack, JsonObject const& v) {
  return push_json_container(stack, v);
}

std::expected<std::optional<JsonObject>, StarException> LuaConverter<JsonObject>::to(LuaStackValue v) {
  auto j = v.maybe_to<Json>();
  if (!j)
    return std::unexpected(std::move(j.error()));
  if (!*j)
    return std::nullopt;
  if ((**j).type() == Json::Type::Object)
    return (**j).to_object();
  if ((**j).type() == Json::Type::Array) {
    auto values = (**j).array_ptr();
    if (!values)
      return std::unexpected(std::move(values.error()));
    if ((*values)->empty())
      return JsonObject();
  }
  return std::nullopt;
}

std::expected<void, StarException> LuaConverter<JsonArray>::from(LuaStack stack, JsonArray const& v) {
  return push_json_container(stack, v);
}

std::expected<std::optional<JsonArray>, StarException> LuaConverter<JsonArray>::to(LuaStackValue v) {
  auto j = v.maybe_to<Json>();
  if (!j)
    return std::unexpected(std::move(j.error()));
  if (!*j)
    return std::nullopt;
  if ((**j).type() == Json::Type::Array)
    return (**j).to_array();
  if ((**j).type() == Json::Type::Object) {
    auto values = (**j).object_ptr();
    if (!values)
      return std::unexpected(std::move(values.error()));
    if ((*values)->empty())
      return JsonArray();
  }
  return std::nullopt;
}

std::expected<LuaEnginePtr, StarException> LuaEngine::create(bool safe) {
  LuaEnginePtr self(*new LuaEngine);

  self->m_state.reset(lua_newstate(reallocate_lua_storage, nullptr, luaL_makeseed(nullptr)));

  self->m_script_default_env_registry_id = LUA_NOREF;
  self->m_wrapped_function_metatable_registry_id = LUA_NOREF;
  self->m_require_function_metatable_registry_id = LUA_NOREF;

  self->m_instruction_limit = 0;
  self->m_profiling_enabled = false;
  self->m_instruction_measure_interval = 1000;
  self->m_instruction_count = 0;
  self->m_recursion_level = 0;
  self->m_recursion_limit = 0;

  if (!self->m_state)
    return std::unexpected(LuaException("Failed to initialize Lua"));

  lua_checkstack(self->m_state.get(), 5);
  using EngineBorrow = std::reference_wrapper<LuaEngine>;
  static_assert(sizeof(EngineBorrow) <= LUA_EXTRASPACE);
  std::construct_at(static_cast<EngineBorrow*>(lua_getextraspace(self->m_state.get())), *self);
  lua_setwarnf(self->m_state.get(), [](void* userdata, char const* message, int continued) {
    auto& self = *static_cast<LuaEngine*>(userdata);
    self.m_warning_message += message;
    if (!continued) {
      if (auto written = Star::print(*stderr, "Lua warning: {}\n", self.m_warning_message); !written)
        std::_Exit(1);
      self.m_warning_message.clear();
    }
  }, std::addressof(*self));
  auto initialize = [](lua_State* state) {
    auto& self = lua_engine(*state);
    bool safe = lua_toboolean(state, 1);
    lua_settop(state, 0);
    lua_createtable(state, 64, 0);
    self.m_diagnostic_strings_registry_id = luaL_ref(state, LUA_REGISTRYINDEX);

  // Create the common message handler function for pcall to print a better
  // message with a traceback
  lua_pushcfunction(state, [](lua_State* state) {
      auto& self = lua_engine(*state);
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
        auto entry = self.find_diagnostic_entry(*state, 1);
        auto non_recoverable_error = entry ? entry->get().non_recoverable_error : std::nullopt;
        auto summary = std::make_shared<ExceptionSummary const>(
            self.capture_diagnostic(*state, *state, 1, ExceptionSummaryKind::ScriptRuntime));
        self.remember_diagnostic(*state, 1, std::move(summary), std::move(non_recoverable_error));
      }
      // Keep limit sentinels untouched, including their native error types.
      if (lua_islightuserdata(state, 1)) {
        void* error = lua_touserdata(state, 1);
        if (error == &s_lua_instruction_limit_exception_key || error == &s_lua_recursion_limit_exception_key)
          return 1;
      }

      luaL_traceback(state, state, lua_tostring(state, 1), 0);
      {
        if (auto entry = self.find_diagnostic_entry(*state, 1)) {
          auto summary = entry->get().summary;
          auto non_recoverable_error = entry->get().non_recoverable_error;
          if (summary)
            self.append_source_diagnostics(*state, *state, *summary);
          self.remember_diagnostic(*state, -1, std::move(summary), std::move(non_recoverable_error));
        }
      }
      lua_remove(state, 1);
      return 1;
    });
  self.m_pcall_traceback_message_handler_registry_id = luaL_ref(state, LUA_REGISTRYINDEX);

  // Create the common metatable for wrapped functions
  lua_newtable(state);
  lua_pushcfunction(state, [](lua_State* state) {
      auto& func = *static_cast<LuaDetail::LuaWrappedFunction*>(lua_touserdata(state, 1));
      std::destroy_at(&func);
      return 0;
    });
  LuaDetail::raw_set_field(*state, -2, "__gc");
  lua_pushboolean(state, 0);
  LuaDetail::raw_set_field(*state, -2, "__metatable");
  self.m_wrapped_function_metatable_registry_id = luaL_ref(state, LUA_REGISTRYINDEX);

  // Create the common metatable for require functions
  lua_newtable(state);
  lua_pushcfunction(state, [](lua_State* state) {
      auto& func = *static_cast<LuaContext::RequireFunction*>(lua_touserdata(state, 1));
      std::destroy_at(&func);
      return 0;
    });
  LuaDetail::raw_set_field(*state, -2, "__gc");
  lua_pushboolean(state, 0);
  LuaDetail::raw_set_field(*state, -2, "__metatable");
  self.m_require_function_metatable_registry_id = luaL_ref(state, LUA_REGISTRYINDEX);

  // Load all base libraries and prune them of unsafe functions

  luaL_requiref(state, "_ENV", luaopen_base, true);
  if (safe) {
    StringSet base_whitelist = {
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

      if (!base_whitelist.contains(key)) {
        lua_pushvalue(state, -1);
        lua_pushnil(state);
        lua_rawset(state, -4);
      }
    }
  }
  lua_pop(state, 1);

  luaL_requiref(state, "os", luaopen_os, true);
  if (safe) {
    StringSet os_whitelist = {"clock", "difftime", "time", "date"};

    lua_pushnil(state);
    while (lua_next(state, -2) != 0) {
      lua_pop(state, 1);
      String key(std::string_view(lua_tostring(state, -1)));

      if (!os_whitelist.contains(key)) {
        lua_pushvalue(state, -1);
        lua_pushnil(state);
        lua_rawset(state, -4);
      }
    }
  }
  lua_pop(state, 1);

  // loads a lua base library, leaves it at the top of the stack
  // Only terminated literal library names enter this native Lua adapter.
  auto load_base_library = [](lua_State& state, std::string_view modname, lua_CFunction openf) {
    luaL_requiref(&state, modname.data(), openf, true);

    // set __metatable metamethod to false
    // otherwise scripts can access and mutate the metatable, allowing passing values
    // between script contexts, breaking the sandbox
    lua_newtable(&state);
    lua_pushliteral(&state, "__metatable");
    lua_pushboolean(&state, 0);
    lua_rawset(&state, -3);
    lua_setmetatable(&state, -2);
  };

  load_base_library(*state, "coroutine", luaopen_coroutine);
  // replace coroutine resume with one that appends tracebacks
  lua_pushliteral(state, "resume");
  lua_pushcfunction(state, &LuaEngine::coresume_with_traceback);
  lua_rawset(state, -3);

  load_base_library(*state, "math", luaopen_math);
  load_base_library(*state, "string", luaopen_string);
  load_base_library(*state, "table", luaopen_table);
  load_base_library(*state, "utf8", luaopen_utf8);
  lua_pop(state, 5);

  if (!safe) {
    load_base_library(*state, "io", luaopen_io);
    load_base_library(*state, "package", luaopen_package);
    load_base_library(*state, "debug", luaopen_debug);
    lua_pop(state, 3);
  }

  // Make a shallow copy of the default script environment and save it for
  // resetting the global state.
  lua_rawgeti(state, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  lua_newtable(state);
  LuaDetail::shallow_copy(*state, -2, -1);
  self.m_script_default_env_registry_id = luaL_ref(state, LUA_REGISTRYINDEX);
  lua_pop(state, 1);
  // Use Lua's own freelist layout rather than a version-specific registry slot.
  lua_pushboolean(state, 0);
  int temporary = luaL_ref(state, LUA_REGISTRYINDEX);
  luaL_unref(state, LUA_REGISTRYINDEX, temporary);
    return 0;
  };
  lua_pushcfunction(self->m_state.get(), initialize);
  lua_pushboolean(self->m_state.get(), safe);
  auto initialized = self->handle_error(*self->m_state, lua_pcall(self->m_state.get(), 1, 0, 0));
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto install = [&self](std::string_view name, std::expected<LuaFunction, StarException> function) -> std::expected<void, StarException> {
    if (!function)
      return std::unexpected(std::move(function.error()));
    return self->set_global(name, std::move(*function));
  };

  auto jarray = install("jarray", self->create_function(&LuaDetail::jarray));
  if (!jarray)
    return std::unexpected(std::move(jarray.error()));
  auto jobject = install("jobject", self->create_function(&LuaDetail::jobject));
  if (!jobject)
    return std::unexpected(std::move(jobject.error()));
  auto jremove = install("jremove", self->create_function(&LuaDetail::jcont_remove));
  if (!jremove)
    return std::unexpected(std::move(jremove.error()));
  auto jsize = install("jsize", self->create_function(&LuaDetail::jcont_size));
  if (!jsize)
    return std::unexpected(std::move(jsize.error()));
  auto jresize = install("jresize", self->create_function(&LuaDetail::jcont_resize));
  if (!jresize)
    return std::unexpected(std::move(jresize.error()));
  auto shared_table = self->create_table();
  if (!shared_table)
    return std::unexpected(std::move(shared_table.error()));
  auto shared = self->set_global("shared", std::move(*shared_table));
  if (!shared)
    return std::unexpected(std::move(shared.error()));
  return self;
}

LuaEngine::~LuaEngine() {
  if (!m_state)
    return;
  // If we've had a stack space leak, this will not be zero
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((lua_gettop(m_state.get()) == 0));
  }
  // Lua may run host callbacks from __gc. Keep the main state observable until
  // lua_close returns (unique_ptr::reset clears its pointer before closing).
  lua_close(m_state.get());
  (void)m_state.release();
}

void LuaEngine::set_instruction_limit(std::uint64_t instruction_limit) {
  if (instruction_limit != m_instruction_limit) {
    m_instruction_limit = instruction_limit;
    update_count_hook();
  }
}

std::uint64_t LuaEngine::instruction_limit() const {
  return m_instruction_limit;
}

void LuaEngine::set_profiling_enabled(bool profiling_enabled) {
  if (profiling_enabled != m_profiling_enabled) {
    m_profiling_enabled = profiling_enabled;
    m_profile_entries.clear();
    update_count_hook();
  }
}

bool LuaEngine::profiling_enabled() const {
  return m_profiling_enabled;
}

List<LuaProfileEntry> LuaEngine::get_profile() {
  List<LuaProfileEntry> profile_entries;
  for (auto const& p : m_profile_entries) {
    profile_entries.push_back(*p.second);
  }

  return profile_entries;
}

void LuaEngine::set_instruction_measure_interval(unsigned measure_interval) {
  if (measure_interval != m_instruction_measure_interval) {
    m_instruction_measure_interval = measure_interval;
    update_count_hook();
  }
}

unsigned LuaEngine::instruction_measure_interval() const {
  return m_instruction_measure_interval;
}

void LuaEngine::set_recursion_limit(unsigned recursion_limit) {
  m_recursion_limit = recursion_limit;
}

unsigned LuaEngine::recursion_limit() const {
  return m_recursion_limit;
}

std::expected<ByteArray, StarException> LuaEngine::compile(std::span<char const> contents, String const& name) {
  LuaStackRestore stack(*m_state);
  if (!lua_checkstack(m_state.get(), 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while compiling a script"));
  auto loaded = handle_source_error(*m_state, luaL_loadbuffer(m_state.get(), contents.data(), contents.size(), name.empty() ? nullptr : name.utf8().c_str()), name);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));

  ByteArray compiled_script;
  lua_Writer writer = [](lua_State*, void const* data, std::size_t size, void* output) -> int {
    static_cast<ByteArray*>(output)->append(std::span(static_cast<char const*>(data), size));
    return 0;
  };
  if (lua_dump(m_state.get(), writer, &compiled_script, false) != 0)
    return std::unexpected(LuaException("Failed to dump compiled Lua script"));
  lua_pop(m_state.get(), 1);

  return compiled_script;
}

std::expected<ByteArray, StarException> LuaEngine::compile(String const& contents, String const& name) {
  return compile(std::span(contents.utf8()), name);
}

std::expected<ByteArray, StarException> LuaEngine::compile(ByteArray const& contents, String const& name) {
  return compile(contents.span(), name);
}

void LuaEngine::set_source_resolver(SourceResolver resolver) {
  m_source_resolver = std::move(resolver);
}

lua_Debug const& LuaEngine::debug_info(int level, String const& what) {
  lua_Debug& debug = m_debug_info = lua_Debug();
  lua_getstack(m_state.get(), level, &debug);
  lua_getinfo(m_state.get(), what.utf8().c_str(), &debug);
  return debug;
}

std::optional<String> LuaEngine::script_source(int level) {
  lua_Debug info{};
  if (!lua_getstack(m_state.get(), level, &info) || !lua_getinfo(m_state.get(), "S", &info) || !info.source)
    return {};
  return String(std::string_view(info.source));
}

std::expected<LuaString, StarException> LuaEngine::create_string(std::string_view str) {
  LuaStackRestore stack(*m_state);
  if (!lua_checkstack(m_state.get(), 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a string"));
  lua_pushcfunction(m_state.get(), [](lua_State* state) {
    auto const& input = *static_cast<std::string_view const*>(lua_touserdata(state, 1));
    lua_pushlstring(state, input.data(), input.size());
    return 1;
  });
  lua_pushlightuserdata(m_state.get(), &str);
  auto called = handle_error(*m_state, pcall_with_traceback(*m_state, 1, 1));
  if (!called)
    return std::unexpected(std::move(called.error()));
  auto handle = pop_handle(*m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaString(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
}

std::expected<LuaString, StarException> LuaEngine::create_string(String const& str) {
  return create_string(str.utf8());
}


std::expected<LuaTable, StarException> LuaEngine::create_table(int narr, int nrec) {
  LuaStackRestore stack(*m_state);
  if (!lua_checkstack(m_state.get(), 4))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a table"));
  lua_pushcfunction(m_state.get(), [](lua_State* state) {
    lua_createtable(state, (int)lua_tointeger(state, 1), (int)lua_tointeger(state, 2));
    return 1;
  });
  lua_pushinteger(m_state.get(), narr);
  lua_pushinteger(m_state.get(), nrec);
  auto called = handle_error(*m_state, pcall_with_traceback(*m_state, 2, 1));
  if (!called)
    return std::unexpected(std::move(called.error()));
  auto handle = pop_handle(*m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaTable(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
}

std::expected<LuaThread, StarException> LuaEngine::create_thread() {
  LuaStackRestore stack(*m_state);
  if (!lua_checkstack(m_state.get(), 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a thread"));
  lua_pushcfunction(m_state.get(), [](lua_State* state) {
    lua_newthread(state);
    return 1;
  });
  auto called = handle_error(*m_state, pcall_with_traceback(*m_state, 0, 1));
  if (!called)
    return std::unexpected(std::move(called.error()));
  auto handle = pop_handle(*m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaThread(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
}

std::expected<std::span<std::byte>, StarException> LuaEngine::allocate_userdata(lua_State& state, std::size_t size) {
  LuaStackRestore restore(state);
  if (!lua_checkstack(&state, 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while allocating userdata"));
  lua_pushcfunction(&state, [](lua_State* state) {
    auto size = *static_cast<std::size_t const*>(lua_touserdata(state, 1));
    lua_newuserdatauv(state, size, 0);
    return 1;
  });
  lua_pushlightuserdata(&state, &size);
  auto allocated = handle_error(state, pcall_with_traceback(state, 1, 1));
  if (!allocated)
    return std::unexpected(std::move(allocated.error()));
  ++restore.top;
  return std::span(static_cast<std::byte*>(lua_touserdata(&state, -1)), size);
}

std::expected<void, StarException> LuaEngine::thread_push_function(int thread_index, int function_index) {
  if (!lua_checkstack(m_state.get(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a thread"));
  push_handle(*m_state, thread_index);
  lua_State& thread = *lua_tothread(m_state.get(), -1);
  lua_pop(m_state.get(), 1);

  int status = lua_status(&thread);
  lua_Debug ar;
  if (status != LUA_OK || lua_getstack(&thread, 0, &ar) > 0 || lua_gettop(&thread) > 0)
    return std::unexpected(LuaException(Star::format("Cannot push function to active or errored thread with status {}", status)));
  if (!lua_checkstack(&thread, 1))
    return std::unexpected(LuaException("Exhausted the Lua thread stack"));

  push_handle(thread, function_index);
  return {};
}

LuaThread::Status LuaEngine::thread_status(int handle_index) {
  push_handle(*m_state, handle_index);
  lua_State& thread = *lua_tothread(m_state.get(), -1);
  lua_pop(m_state.get(), 1);

  int status = lua_status(&thread);
  if (status != LUA_OK && status != LUA_YIELD)
    return LuaThread::Status::Error;

  lua_Debug ar;
  if (status == LUA_YIELD || lua_getstack(&thread, 0, &ar) > 0 || lua_gettop(&thread) > 0)
    return LuaThread::Status::Active;

  return LuaThread::Status::Dead;
}

std::expected<LuaContext, StarException> LuaEngine::create_context() {
  LuaStackRestore stack(*m_state);
  if (!lua_checkstack(m_state.get(), 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a context"));
  lua_pushcfunction(m_state.get(), [](lua_State* state) {
    lua_newtable(state);
    LuaDetail::shallow_copy(*state, 1, -1);
    // An escaped loader keeps its environment alive without a native registry root.
    lua_pushvalue(state, -1);
    lua_pushcclosure(state, context_loadstring, 1);
    lua_setfield(state, -2, "loadstring");
    return 1;
  });
  lua_rawgeti(m_state.get(), LUA_REGISTRYINDEX, m_script_default_env_registry_id);
  auto created = handle_error(*m_state, pcall_with_traceback(*m_state, 1, 1));
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto handle = pop_handle(*m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaContext(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
}

void LuaEngine::release_handles(lua_State& state) {
  if (m_handle_free.empty() || !lua_checkstack(&state, 2))
    return;
  // Existing slots are overwritten without allocating or running finalizers.
  // Retain the queue capacity instead of allocating another batch next time.
  for (int handle : m_handle_free)
    luaL_unref(&state, LUA_REGISTRYINDEX, handle);
  m_handle_free.clear();
}

std::expected<void, StarException> LuaEngine::collect_garbage(std::optional<unsigned> steps) {
  LuaStackRestore stack(*m_state);
  if (!lua_checkstack(m_state.get(), 4))
    return std::unexpected(LuaException("Exhausted the Lua stack while collecting garbage"));
  struct Input {
    bool incremental;
    std::size_t bytes;
  } input{steps.has_value(), steps ? std::size_t(*steps) * 1024 : 0};
  bool running = lua_gc(m_state.get(), LUA_GCISRUNNING);
  lua_pushcfunction(m_state.get(), [](lua_State* state) {
    auto const& input = *static_cast<Input const*>(lua_touserdata(state, 1));
    if (input.incremental)
      lua_gc(state, LUA_GCSTEP, input.bytes);
    else
      lua_gc(state, LUA_GCCOLLECT);
    return 0;
  });
  lua_pushlightuserdata(m_state.get(), &input);
  int status = pcall_with_traceback(*m_state, 1, 0);
  if (bool(lua_gc(m_state.get(), LUA_GCISRUNNING)) != running)
    lua_gc(m_state.get(), running ? LUA_GCRESTART : LUA_GCSTOP);
  return handle_error(*m_state, status);
}

void LuaEngine::set_auto_garbage_collection(bool enabled) {
  lua_gc(m_state.get(), enabled ? LUA_GCRESTART : LUA_GCSTOP);
}

void LuaEngine::tune_auto_garbage_collection(float pause, float step_multiplier) {
  lua_gc(m_state.get(), LUA_GCPARAM, LUA_GCPPAUSE, static_cast<int>(std::round(pause * 100)));
  lua_gc(m_state.get(), LUA_GCPARAM, LUA_GCPSTEPMUL, static_cast<int>(std::round(step_multiplier * 100)));
}

std::size_t LuaEngine::memory_usage() const {
  return (std::size_t)lua_gc(m_state.get(), LUA_GCCOUNT, 0) * 1024 + lua_gc(m_state.get(), LUA_GCCOUNTB, 0);
}

std::expected<void, StarException> LuaEngine::add_imgui() {
  LuaStackRestore stack(*m_state);
  if (!lua_checkstack(m_state.get(), 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while adding ImGui bindings"));
  lua_pushcfunction(m_state.get(), [](lua_State* state) {
    pushImguiBindings(state);
    lua_setfield(state, 1, "imgui");
    return 0;
  });
  lua_rawgeti(m_state.get(), LUA_REGISTRYINDEX, m_script_default_env_registry_id);
  return handle_error(*m_state, pcall_with_traceback(*m_state, 1, 0));
}

LuaEngine& LuaEngine::lua_engine(lua_State& state) {
  return static_cast<std::reference_wrapper<LuaEngine>*>(lua_getextraspace(&state))->get();
}

void LuaEngine::count_hook(lua_State* state, lua_Debug* ar) {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((ar->event == LUA_HOOKCOUNT));
  }
  lua_checkstack(state, 4);

  auto& self = lua_engine(*state);

  // If the instruction count is 0, that means in this sequence of calls,
  // we have not hit a debug hook yet.  Since we don't know the state of
  // the internal lua instruction counter at the start, we don't know how
  // many instructions have been executed, only that it is >= 1 and <=
  // m_instruction_measure_interval, so we pick the low estimate.
  if (self.m_instruction_count == 0)
    self.m_instruction_count = 1;
  else
    self.m_instruction_count += self.m_instruction_measure_interval;

  if (self.m_instruction_limit != 0 && self.m_instruction_count > self.m_instruction_limit) {
    lua_pushlightuserdata(state, &s_lua_instruction_limit_exception_key);
    lua_error(state);
  }

  if (self.m_profiling_enabled) {
    // find bottom of the stack
    // ar will contain the stack info from the last call that returns 1
    int stack_level = -1;
    while (lua_getstack(state, stack_level + 1, ar) == 1)
      stack_level++;

    std::shared_ptr<LuaProfileEntry> parent_entry = nullptr;
    while (true) {
      // Get the 'n' name info and 'S' source info
      if (lua_getinfo(state, "nS", ar) == 0)
        break;

      auto key = std::make_tuple(String(ar->short_src), (unsigned)ar->linedefined);
      auto& entry_map = parent_entry ? parent_entry->calls : self.m_profile_entries;
      if (!entry_map.contains(key)) {
        auto e = std::make_shared<LuaProfileEntry>();
        e->source = std::string_view(ar->short_src);
        e->source_line = ar->linedefined;
        entry_map.set(key, e);
      }
      auto entry = entry_map.find(key)->second;

      // metadata
      if (!entry->name && ar->name)
        entry->name = String(std::string_view(ar->name));
      if (!entry->name_scope && String() != std::string_view(ar->namewhat))
        entry->name_scope = String(std::string_view(ar->namewhat));

      // Only add time to the self time for the function we are actually
      // in, not parent functions
      if (stack_level == 0) {
        entry->total_time += 1;
        entry->self_time += 1;
      } else {
        entry->total_time += 1;
      }

      parent_entry = entry;
      if (lua_getstack(state, --stack_level, ar) == 0)
        break;
    }
  }
}

void LuaEngine::push_callback_error(lua_State& state, StarException const& error) {
  if (error.is<LuaInstructionLimitReached>()) {
    lua_pushlightuserdata(&state, &s_lua_instruction_limit_exception_key);
  } else if (error.is<LuaRecursionLimitReached>()) {
    lua_pushlightuserdata(&state, &s_lua_recursion_limit_exception_key);
  } else {
    auto diagnostic = Star::print_exception(ExceptionDiagnostic(error), true);
    lua_pushcfunction(&state, [](lua_State* state) {
      auto diagnostic = static_cast<char const*>(lua_touserdata(state, 1));
      luaL_where(state, 2);
      lua_pushstring(state, diagnostic);
      lua_concat(state, 2);
      return 1;
    });
    lua_pushlightuserdata(&state, const_cast<char*>(diagnostic.c_str()));
    // A finalizer/allocation failure must not inherit the callback's cause.
    if (lua_pcall(&state, 1, 1, 0) != LUA_OK)
      return;
  }
  remember_diagnostic(state, -1, error.diagnostic_summary(),
      error.is<LuaHostError>() ? std::optional<StarException>(error) : std::nullopt);
}

std::optional<LuaEngine::DiagnosticKey> LuaEngine::diagnostic_key(lua_State& state, int index) {
  if (lua_type(&state, index) == LUA_TSTRING) {
    std::size_t length;
    auto bytes = lua_tolstring(&state, index, &length);
    return std::string_view(bytes, length);
  }
  if (lua_islightuserdata(&state, index)) {
    if (auto identity = lua_touserdata(&state, index))
      return reinterpret_cast<std::uintptr_t>(identity);
  }
  return {};
}

std::optional<std::reference_wrapper<LuaEngine::DiagnosticTransport const>> LuaEngine::find_diagnostic_entry(lua_State& state, int index) const {
  auto key = diagnostic_key(state, index);
  if (!key)
    return {};
  for (std::size_t i = 0; i < m_diagnostic_count; ++i) {
    auto const& entry = m_diagnostic_transport[(m_next_diagnostic + m_diagnostic_transport.size() - i - 1)
        % m_diagnostic_transport.size()];
    if (std::holds_alternative<std::string_view>(*key)) {
      if (std::holds_alternative<std::string_view>(entry.key)
          && std::get<std::string_view>(entry.key).data() == std::get<std::string_view>(*key).data())
        return std::cref(entry);
    } else if (entry.key == *key) {
      return std::cref(entry);
    }
  }
  if (!std::holds_alternative<std::string_view>(*key))
    return {};
  // Lua's error(message) can prepend a location to a caught native string.
  // Accept only a prefix verified against a live frame and the complete pinned
  // payload, never a guessed suffix or a parsed flattened exception/traceback.
  auto message = std::get<std::string_view>(*key);
  lua_Debug frame;
  for (int level = 0; lua_getstack(&state, level, &frame); ++level) {
    if (!lua_getinfo(&state, "Sl", &frame) || frame.currentline <= 0)
      continue;
    auto prefix = std::format("{}:{}: ", frame.short_src, frame.currentline);
    if (!message.starts_with(prefix))
      continue;
    auto payload = message.substr(prefix.size());
    for (std::size_t i = 0; i < m_diagnostic_count; ++i) {
      auto const& entry = m_diagnostic_transport[(m_next_diagnostic + m_diagnostic_transport.size() - i - 1)
          % m_diagnostic_transport.size()];
      if (std::holds_alternative<std::string_view>(entry.key) && std::get<std::string_view>(entry.key) == payload)
        return std::cref(entry);
    }
  }
  return {};
}

std::shared_ptr<ExceptionSummary const> LuaEngine::find_diagnostic(lua_State& state, int index) const {
  auto entry = find_diagnostic_entry(state, index);
  return entry ? entry->get().summary : nullptr;
}

void LuaEngine::remember_diagnostic(lua_State& state, int index, std::shared_ptr<ExceptionSummary const> summary,
    std::optional<StarException> non_recoverable_error) {
  if ((!summary && !non_recoverable_error) || m_diagnostic_strings_registry_id == LUA_NOREF || !lua_checkstack(&state, 4))
    return;
  auto key = diagnostic_key(state, index);
  if (!key)
    return;
  index = lua_absindex(&state, index);
  struct Input {
    int registry;
    std::size_t slot;
  } input{m_diagnostic_strings_registry_id, m_next_diagnostic};
  lua_pushcfunction(&state, [](lua_State* state) {
    auto& input = *static_cast<Input*>(lua_touserdata(state, 2));
    lua_rawgeti(state, LUA_REGISTRYINDEX, input.registry);
    lua_pushvalue(state, 1);
    lua_rawseti(state, -2, static_cast<lua_Integer>(input.slot + 1));
    return 0;
  });
  lua_pushvalue(&state, index);
  lua_pushlightuserdata(&state, &input);
  int result = lua_pcall(&state, 2, 0, 0);
  if (result != LUA_OK) {
    lua_pop(&state, 1);
    return;
  }
  m_diagnostic_transport[m_next_diagnostic] = {*key, std::move(summary), std::move(non_recoverable_error)};
  m_next_diagnostic = (m_next_diagnostic + 1) % m_diagnostic_transport.size();
  m_diagnostic_count = std::min(m_diagnostic_count + 1, m_diagnostic_transport.size());
}

ExceptionSummary LuaEngine::capture_diagnostic(lua_State& from, lua_State& state, int index,
    ExceptionSummaryKind kind) const {
  auto inherited = kind == ExceptionSummaryKind::ScriptRuntime ? find_diagnostic(state, index) : nullptr;
  ExceptionSummary summary;
  if (inherited) {
    summary = *inherited;
  } else {
    summary.kind = kind;
    summary.category = kind == ExceptionSummaryKind::ScriptSyntax ? "Lua syntax error" : "Lua runtime error";
    if (lua_type(&state, index) == LUA_TSTRING) {
      std::size_t length = 0;
      auto reason = lua_tolstring(&state, index, &length);
      summary.reason.assign(reason, length);
    } else {
      summary.reason = "<unknown error>";
    }
  }
  if (lua_islightuserdata(&state, index)) {
    auto sentinel = lua_touserdata(&state, index);
    if (sentinel == &s_lua_instruction_limit_exception_key || sentinel == &s_lua_recursion_limit_exception_key) {
      summary.kind = ExceptionSummaryKind::ScriptRuntime;
      summary.category = sentinel == &s_lua_instruction_limit_exception_key
          ? "LuaInstructionLimitReached" : "LuaRecursionLimitReached";
      summary.reason = sentinel == &s_lua_instruction_limit_exception_key
          ? "Lua instruction limit reached" : "Lua recursion limit reached";
    }
  }
  lua_Debug frame;
  for (int level = 0; lua_getstack(&from, level, &frame); ++level) {
    if (!lua_getinfo(&from, "Sl", &frame) || !frame.source || std::string_view(frame.what) == "C")
      continue;
    std::string source(frame.source, frame.srclen);
    auto existing = std::ranges::find_if(summary.locations, [&](auto const& location) {
      return location.source == source;
    });
    if (std::ranges::any_of(summary.locations, [&](auto const& location) {
          return location.source == source && location.line == frame.currentline;
        }))
      continue;
    auto provider = existing != summary.locations.end() ? existing->provider : source_diagnostic(String(source)).take_utf8();
    summary.locations.push_back({std::move(source), frame.currentline, std::move(provider)});
  }
  return summary;
}

void LuaEngine::begin_diagnostic_scope() {
  if (m_diagnostic_depth++ != 0 || m_diagnostic_count == 0)
    return;
  // Only a previous failure makes this loop run. Successful calls allocate no
  // summary metadata and do not scan an empty transport.
  bool clear_roots = lua_checkstack(m_state.get(), 2);
  if (clear_roots)
    lua_rawgeti(m_state.get(), LUA_REGISTRYINDEX, m_diagnostic_strings_registry_id);
  for (std::size_t i = 0; i < m_diagnostic_count; ++i) {
    m_diagnostic_transport[i] = {};
    if (clear_roots) {
      lua_pushnil(m_state.get());
      lua_rawseti(m_state.get(), -2, static_cast<lua_Integer>(i + 1));
    }
  }
  if (clear_roots)
    lua_pop(m_state.get(), 1);
  m_diagnostic_count = 0;
  m_next_diagnostic = 0;
}

void LuaEngine::end_diagnostic_scope() {
  --m_diagnostic_depth;
}

std::expected<void, StarException> LuaEngine::handle_error(lua_State& state, int res) {
  if (res == LUA_OK)
    return {};
  auto entry = find_diagnostic_entry(state, -1);
  auto non_recoverable_error = entry ? entry->get().non_recoverable_error : std::nullopt;
  auto summary = capture_diagnostic(state, state, -1, res == LUA_ERRSYNTAX
      ? ExceptionSummaryKind::ScriptSyntax : ExceptionSummaryKind::ScriptRuntime);
  if (non_recoverable_error) {
    lua_pop(&state, 1);
    return std::unexpected(std::move(*non_recoverable_error).with_summary(std::move(summary)));
  }
  if (lua_islightuserdata(&state, -1)) {
    void* error = lua_touserdata(&state, -1);
    if (error == &s_lua_instruction_limit_exception_key) {
      lua_pop(&state, 1);
      return std::unexpected(LuaInstructionLimitReached().with_summary(std::move(summary)));
    }
    if (error == &s_lua_recursion_limit_exception_key) {
      lua_pop(&state, 1);
      return std::unexpected(LuaRecursionLimitReached().with_summary(std::move(summary)));
    }
  }
  if (lua_type(&state, -1) == LUA_TNUMBER) {
    // Numeric Lua error objects are string-coerced just as before, but the
    // coercion may collect garbage and run a user finalizer.
    lua_pushcfunction(&state, [](lua_State* state) {
      lua_tolstring(state, 1, nullptr);
      lua_pushvalue(state, 1);
      return 1;
    });
    lua_insert(&state, -2);
    int converted = lua_pcall(&state, 1, 1, 0);
    if (converted != LUA_OK)
      res = converted;
    summary = capture_diagnostic(state, state, -1, ExceptionSummaryKind::ScriptRuntime);
  }
  String error = lua_type(&state, -1) == LUA_TSTRING
      ? Star::format("Error code {}, {}", res, lua_tostring(&state, -1))
      : Star::format("Error code {}, <unknown error>", res);
  lua_pop(&state, 1);
  if (error.utf8().ends_with("<eof>"))
    return std::unexpected(LuaIncompleteStatementException(error.take_utf8()).with_summary(std::move(summary)));
  return std::unexpected(LuaException(error.take_utf8()).with_summary(std::move(summary)));
}

std::expected<void, StarException> LuaEngine::handle_source_error(lua_State& state, int res, String const& source) {
  auto result = handle_error(state, res);
  if (!result) {
    auto summary = *result.error().diagnostic_summary();
    auto diagnostic = source_diagnostic(source);
    if (std::ranges::none_of(summary.locations, [&](auto const& location) { return location.source == source.utf8(); }))
      summary.locations.insert(summary.locations.begin(), {source.utf8(), -1, diagnostic.utf8()});
    auto error = std::move(result.error()).with_summary(std::move(summary));
    if (!diagnostic.empty())
      error = std::move(error).with_context(Star::format("Lua source '{}': {}", source, diagnostic));
    return std::unexpected(std::move(error));
  }
  return result;
}

int LuaEngine::pcall_with_traceback(lua_State& state, int nargs, int nresults) {
  release_handles(state);
  begin_diagnostic_scope();
  int msgh_position = lua_gettop(&state) - nargs;
  lua_rawgeti(&state, LUA_REGISTRYINDEX, m_pcall_traceback_message_handler_registry_id);
  lua_insert(&state, msgh_position);
  int ret = lua_pcall(&state, nargs, nresults, msgh_position);
  lua_remove(&state, msgh_position);
  release_handles(state);
  end_diagnostic_scope();
  return ret;
}

String LuaEngine::source_diagnostic(String const& source) const noexcept {
  if (!m_source_resolver || source.empty())
    return {};
  auto diagnostic = m_source_resolver(source);
  if (diagnostic)
    return std::move(*diagnostic);
  return {};
}

void LuaEngine::append_source_diagnostics(lua_State& from, lua_State& to, ExceptionSummary const& summary) const noexcept {
  if (!m_source_resolver)
    return;
  String diagnostics;
  StringSet sources;
  lua_Debug frame;
  for (int level = 0; lua_getstack(&from, level, &frame); ++level) {
    // "S" only reads the live frame, without pushing a function or invoking Lua.
    if (!lua_getinfo(&from, "S", &frame) || !frame.source)
      continue;
    String source(std::string_view(frame.source, frame.srclen));
    if (!sources.insert(source).second)
      continue;
    auto location = std::ranges::find_if(summary.locations, [&](auto const& location) {
      return location.source == source.utf8();
    });
    auto provider = location == summary.locations.end() ? source_diagnostic(source) : String(location->provider);
    if (provider.empty())
      continue;
    diagnostics += "\n  ";
    diagnostics += source;
    diagnostics += ": ";
    diagnostics += provider;
  }
  if (diagnostics.empty() || !lua_checkstack(&to, 3))
    return;
  diagnostics = "\nLua source providers:" + diagnostics;
  // Keep host ownership outside the protected C callback. Any Lua allocation
  // or finalizer error during the push/concat returns through pcall, not through
  // C++ destructors. Keep the original traceback if enrichment fails.
  lua_pushcfunction(&to, [](lua_State* state) {
    auto const& diagnostic = *static_cast<String const*>(lua_touserdata(state, 2));
    lua_pushvalue(state, 1);
    lua_pushlstring(state, diagnostic.utf8().data(), diagnostic.utf8().size());
    lua_concat(state, 2);
    return 1;
  });
  lua_pushvalue(&to, -2);
  lua_pushlightuserdata(&to, &diagnostics);
  if (lua_pcall(&to, 2, 1, 0) == LUA_OK)
    lua_replace(&to, -2);
  else
    lua_pop(&to, 1);
}

int LuaEngine::coresume_with_traceback(lua_State* state) {
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
    propagate_error_with_traceback(*co, *state);
    return 2;
  }
}

void LuaEngine::propagate_error_with_traceback(lua_State& from, lua_State& to) {
  lua_checkstack(&to, 4);
  lua_xmove(&from, &to, 1);
  lua_pushcfunction(&to, [](lua_State* state) {
    auto& from = *static_cast<lua_State*>(lua_touserdata(state, 2));
    auto& self = lua_engine(*state);
    if (lua_type(state, 1) == LUA_TNUMBER)
      lua_tolstring(state, 1, nullptr);
    {
      auto entry = self.find_diagnostic_entry(*state, 1);
      auto non_recoverable_error = entry ? entry->get().non_recoverable_error : std::nullopt;
      auto summary = std::make_shared<ExceptionSummary const>(
          self.capture_diagnostic(from, *state, 1, ExceptionSummaryKind::ScriptRuntime));
      self.remember_diagnostic(*state, 1, std::move(summary), std::move(non_recoverable_error));
    }
    if (auto error = lua_tostring(state, 1)) {
      luaL_traceback(state, &from, error, 0);
      if (auto entry = self.find_diagnostic_entry(*state, 1)) {
        auto summary = entry->get().summary;
        auto non_recoverable_error = entry->get().non_recoverable_error;
        if (summary)
          self.append_source_diagnostics(from, *state, *summary);
        self.remember_diagnostic(*state, -1, std::move(summary), std::move(non_recoverable_error));
      }
    } else {
      lua_pushvalue(state, 1);
    }
    return 1;
  });
  lua_insert(&to, -2);
  lua_pushlightuserdata(&to, &from);
  // A traceback allocation failure is itself the outgoing error object.
  (void)lua_pcall(&to, 2, 1, 0);
}


std::size_t LuaEngine::string_length(int handle_index) {
  push_handle(*m_state, handle_index);
  std::size_t len = 0;
  lua_tolstring(m_state.get(), -1, &len);
  lua_pop(m_state.get(), 1);
  return len;
}

String LuaEngine::string(int handle_index) {
  push_handle(*m_state, handle_index);
  std::size_t length = 0;
  auto data = lua_tolstring(m_state.get(), -1, &length);
  lua_pop(m_state.get(), 1);
  return String(std::string_view(data, length));
}

StringView LuaEngine::string_view(int handle_index) {
  push_handle(*m_state, handle_index);
  std::size_t length = 0;
  auto data = lua_tolstring(m_state.get(), -1, &length);
  lua_pop(m_state.get(), 1);
  return StringView(std::string_view(data, length));
}

std::expected<std::optional<LuaTable>, StarException> LuaEngine::table_get_metatable(int handle_index) {
  LuaStackRestore stack(*m_state);
  if (!lua_checkstack(m_state.get(), 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while getting a metatable"));
  push_handle(*m_state, handle_index);
  if (lua_getmetatable(m_state.get(), -1) == 0)
    return std::optional<LuaTable>();
  auto handle = pop_handle(*m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaTable(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
}

std::expected<void, StarException> LuaEngine::table_set_metatable(int handle_index, LuaTable const& table) {
  if (&table.engine() != this)
    return std::unexpected(LuaException("lua reference values cannot be shared between engines"));
  if (!lua_checkstack(m_state.get(), 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while setting a metatable"));
  push_handle(*m_state, handle_index);
  push_handle(*m_state, table.handle_index());
  lua_setmetatable(m_state.get(), -2);
  lua_pop(m_state.get(), 1);
  return {};
}

std::expected<void, StarException> LuaEngine::set_context_require(int handle_index, LuaContext::RequireFunction require_function) {
  LuaStackRestore stack(*m_state);
  if (!lua_checkstack(m_state.get(), 4))
    return std::unexpected(LuaException("Exhausted the Lua stack while installing require"));

  auto invoke_require = [](lua_State* state) {
    {
      auto invoke = [state]() -> std::expected<void, StarException> {
        if (!lua_checkstack(state, 2))
          return std::unexpected(LuaException("Exhausted the Lua stack while invoking require"));
        auto& require = *static_cast<LuaContext::RequireFunction*>(lua_touserdata(state, lua_upvalueindex(1)));
        auto& self = lua_engine(*state);
        if (lua_gettop(state) == 0)
          return std::unexpected(LuaException("require expects a module name"));
        LuaStack stack(self, *state);
        auto module = stack.value(-1);
        if (!module.is<LuaString>())
          return std::unexpected(LuaConversionException("require expects a string module name"));
        auto module_name = module.string_view();
        lua_pushvalue(state, lua_upvalueindex(2));
        if (!require)
          return std::unexpected(LuaException("Cannot invoke an empty Lua require function"));
        return require(stack.value(-1), module_name);
      };
      auto result = invoke();
      if (result)
        return 0;
      lua_settop(state, 0);
      lua_engine(*state).push_callback_error(*state, result.error());
    }
    // Every context, value and diagnostic has been released before longjmp.
    return lua_error(state);
  };

  struct Input {
    std::reference_wrapper<LuaContext::RequireFunction> function;
    int metatable;
    lua_CFunction invoke;
  };
  Input input{std::ref(require_function), m_require_function_metatable_registry_id, invoke_require};
  lua_pushcfunction(m_state.get(), [](lua_State* state) {
    auto& input = *static_cast<Input*>(lua_touserdata(state, 2));
    new (lua_newuserdatauv(state, sizeof(LuaContext::RequireFunction), 0))
        LuaContext::RequireFunction(std::move(input.function.get()));
    lua_rawgeti(state, LUA_REGISTRYINDEX, input.metatable);
    lua_setmetatable(state, -2);
    lua_pushvalue(state, 1);
    lua_pushcclosure(state, input.invoke, 2);
    LuaDetail::raw_set_field(*state, 1, "require");
    return 0;
  });
  push_handle(*m_state, handle_index);
  lua_pushlightuserdata(m_state.get(), &input);
  return handle_error(*m_state, pcall_with_traceback(*m_state, 2, 0));
}

std::expected<int, StarException> LuaEngine::context_eval(int handle_index, String const& lua, int nresults) {
  int top = lua_gettop(m_state.get());
  if (!lua_checkstack(m_state.get(), nresults == LUA_MULTRET ? 2 : std::max(2, nresults + 1)))
    return std::unexpected(LuaException("Exhausted the Lua stack while evaluating a script"));
  auto expression = "return " + lua;
  auto load = [&](String const& source) {
    auto const& bytes = source.utf8();
    return luaL_loadbuffer(m_state.get(), bytes.data(), bytes.size(), bytes.c_str());
  };
  int load_result = load(expression);
  if (load_result == LUA_ERRSYNTAX) {
    lua_pop(m_state.get(), 1);
    load_result = load(lua);
  }
  if (auto loaded = handle_error(*m_state, load_result); !loaded)
    return std::unexpected(std::move(loaded.error()));
  push_handle(*m_state, handle_index);
  lua_setupvalue(m_state.get(), -2, 1);
  if (auto entered = increment_recursion_level(); !entered)
    return std::unexpected(std::move(entered.error()));
  int result = pcall_with_traceback(*m_state, 0, nresults);
  decrement_recursion_level();
  if (auto called = handle_error(*m_state, result); !called)
    return std::unexpected(std::move(called.error()));
  return lua_gettop(m_state.get()) - top;
}

std::expected<void, StarException> LuaEngine::push_context_path(int handle_index, std::string_view path) {
  if (!lua_checkstack(m_state.get(), 4))
    return std::unexpected(LuaException("Exhausted the Lua stack while reading a context path"));
  lua_pushcfunction(m_state.get(), [](lua_State* state) {
    auto path = *static_cast<std::string_view const*>(lua_touserdata(state, 2));
    lua_pop(state, 1);
    while (auto key = take_path_prefix(path)) {
      lua_pushlstring(state, key->data(), key->size());
      if (lua_gettable(state, -2) != LUA_TTABLE) {
        lua_pushnil(state);
        return 1;
      }
      lua_remove(state, -2);
    }
    lua_pushlstring(state, path.data(), path.size());
    lua_gettable(state, -2);
    return 1;
  });
  push_handle(*m_state, handle_index);
  lua_pushlightuserdata(m_state.get(), &path);
  return handle_error(*m_state, pcall_with_traceback(*m_state, 2, 1));
}

std::expected<void, StarException> LuaEngine::set_context_path(int handle_index, std::string_view path) {
  if (!lua_checkstack(m_state.get(), 4))
    return std::unexpected(LuaException("Exhausted the Lua stack while setting a context path"));
  lua_pushcfunction(m_state.get(), [](lua_State* state) {
    // Argument 1 is the value to assign; argument 2 is the first table.
    auto path = *static_cast<std::string_view const*>(lua_touserdata(state, 3));
    lua_pop(state, 1);
    while (auto key = take_path_prefix(path)) {
      lua_pushlstring(state, key->data(), key->size());
      int type = lua_gettable(state, -2);
      if (type == LUA_TNIL) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_pushlstring(state, key->data(), key->size());
        lua_pushvalue(state, -2);
        lua_settable(state, -4);
      } else if (type != LUA_TTABLE) {
        return luaL_error(state, "Sub-path in set_path is not nil and is not a table");
      }
      lua_remove(state, -2);
    }
    lua_pushlstring(state, path.data(), path.size());
    lua_pushvalue(state, 1);
    lua_settable(state, -3);
    return 0;
  });
  lua_insert(m_state.get(), -2);
  push_handle(*m_state, handle_index);
  lua_pushlightuserdata(m_state.get(), &path);
  return handle_error(*m_state, pcall_with_traceback(*m_state, 3, 0));
}

std::expected<int, StarException> LuaEngine::pop_handle(lua_State& state) {
  LuaStackRestore stack(state);
  --stack.top;
  if (!lua_checkstack(&state, 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while acquiring a registry reference"));
  lua_pushcfunction(&state, [](lua_State* state) {
    lua_pushvalue(state, 1);
    int reference = luaL_ref(state, LUA_REGISTRYINDEX);
    lua_pushinteger(state, reference);
    return 1;
  });
  lua_insert(&state, -2);
  auto acquired = handle_error(state, pcall_with_traceback(state, 1, 1));
  if (!acquired)
    return std::unexpected(std::move(acquired.error()));
  return (int)lua_tointeger(&state, -1);
}

void LuaEngine::push_handle(lua_State& state, int handle_index) {
  lua_rawgeti(&state, LUA_REGISTRYINDEX, handle_index);
}

std::expected<void, StarException> LuaConverter<LuaDetail::LuaWrappedFunction>::from(LuaStack stack, LuaDetail::LuaWrappedFunction function) {
  auto& state = stack.state();
  if (!lua_checkstack(&state, 3))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a function"));

  auto invoke_function = [](lua_State* state) {
    auto& function = *static_cast<LuaDetail::LuaWrappedFunction*>(lua_touserdata(state, lua_upvalueindex(1)));
    auto& self = LuaEngine::lua_engine(*state);
    {
      auto invoke = [&]() -> std::expected<int, StarException> {
        if (!function)
          return std::unexpected(LuaException("Cannot invoke an empty Lua callback"));
        for (int i = 1, count = lua_gettop(state); i <= count; ++i) {
          if (auto valid = validate_native_lua_value(*state, i); !valid)
            return std::unexpected(std::move(valid.error()));
        }
        return function(LuaStack(self, *state));
      };
      auto result = invoke();
      if (result)
        return *result;
      lua_settop(state, 0);
      self.push_callback_error(*state, result.error());
    }
    return lua_error(state);
  };

  struct Input {
    std::reference_wrapper<LuaDetail::LuaWrappedFunction> function;
    int metatable;
    lua_CFunction invoke;
  };
  Input input{std::ref(function), stack.engine().m_wrapped_function_metatable_registry_id, invoke_function};
  lua_pushlightuserdata(&state, &input);
  return stack.call([](lua_State* state) {
    auto& input = *static_cast<Input*>(lua_touserdata(state, 1));
    new (lua_newuserdatauv(state, sizeof(LuaDetail::LuaWrappedFunction), 0))
        LuaDetail::LuaWrappedFunction(std::move(input.function.get()));
    lua_rawgeti(state, LUA_REGISTRYINDEX, input.metatable);
    lua_setmetatable(state, -2);
    lua_pushcclosure(state, input.invoke, 1);
    return 1;
  }, 1, 1);
}

std::expected<LuaFunction, StarException> LuaEngine::create_wrapped_function(LuaDetail::LuaWrappedFunction function) {
  LuaStackRestore restore(*m_state);
  auto created = LuaStack(*this, *m_state).push(std::move(function));
  if (!created)
    return std::unexpected(std::move(created.error()));

  auto handle = pop_handle(*m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaFunction(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
}

std::expected<LuaFunction, StarException> LuaEngine::create_raw_function(lua_CFunction function) {
  if (!function)
    return std::unexpected(LuaException("Cannot create a Lua function from a null function pointer"));
  if (!lua_checkstack(m_state.get(), 2))
    return std::unexpected(LuaException("Exhausted the Lua stack while creating a C function"));

  lua_pushcfunction(m_state.get(), function);
  auto handle = pop_handle(*m_state);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return LuaFunction(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
}

int LuaEngine::context_loadstring(lua_State* state) {
  auto& engine = lua_engine(*state);
  {
    auto compile = [&]() -> std::expected<void, StarException> {
      if (!lua_checkstack(state, 2))
        return std::unexpected(LuaException("Exhausted the Lua stack while loading a function"));
      LuaStack stack(engine, *state);
      // Validate even the ignored trailing arguments, as for other native callbacks.
      for (int i = 1, count = stack.top(); i <= count; ++i) {
        if (auto valid = validate_native_lua_value(*state, i); !valid)
          return std::unexpected(std::move(valid.error()));
      }

      auto source = stack.value(1);
      String converted_source;
      std::string_view contents;
      if (source.is<LuaString>()) {
        contents = source.string_view().utf8();
      } else {
        auto converted = source.to<String>();
        if (!converted)
          return std::unexpected(std::move(converted.error()));
        converted_source = std::move(*converted);
        contents = converted_source.utf8();
      }

      String name = "loadstring";
      if (auto arg = stack.value(2); !arg.is<LuaNilType>()) {
        name += ": ";
        if (arg.is<LuaString>()) {
          name += arg.string_view();
        } else {
          auto converted = arg.to<std::optional<String>>();
          if (!converted)
            return std::unexpected(std::move(converted.error()));
          name += **converted;
        }
      }

      int environment = lua_upvalueindex(1);
      if (auto arg = stack.value(3); !arg.is<LuaNilType>()) {
        if (!arg.is<LuaTable>())
          return std::unexpected(conversion_error<std::optional<LuaTable>>());
        environment = arg.index();
      }

      int status = luaL_loadbufferx(state, contents.data(), contents.size(), name.utf8().c_str(), "t");
      auto loaded = engine.handle_source_error(*state, status, name);
      if (!loaded)
        return std::unexpected(std::move(loaded.error()));
      lua_pushvalue(state, environment);
      lua_setupvalue(state, -2, 1);
      return {};
    };
    auto loaded = compile();
    if (loaded)
      return 1;
    lua_settop(state, 0);
    engine.push_callback_error(*state, loaded.error());
  }
  return lua_error(state);
}

std::expected<void, StarException> LuaEngine::push_lua_value(lua_State& state, LuaValue const& lua_value) {
  if (!lua_checkstack(&state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while pushing a value"));
  return std::visit([&engine = *this, &state](auto const& value) -> std::expected<void, StarException> {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, LuaNilType>) {
      lua_pushnil(&state);
    } else if constexpr (std::is_same_v<T, LuaBoolean>) {
      lua_pushboolean(&state, value);
    } else if constexpr (std::is_same_v<T, LuaInt>) {
      lua_pushinteger(&state, value);
    } else if constexpr (std::is_same_v<T, LuaFloat>) {
      lua_pushnumber(&state, value);
    } else {
      if (&value.engine() != &engine)
        return std::unexpected(LuaException("lua reference values cannot be shared between engines"));
      engine.push_handle(state, value.handle_index());
    }
    return {};
  }, lua_value);
}

std::expected<LuaValue, StarException> LuaEngine::pop_lua_value(lua_State& state) {
  if (!lua_checkstack(&state, 1)) {
    lua_pop(&state, 1);
    return std::unexpected(LuaException("Exhausted the Lua stack while reading a value"));
  }

  LuaValue result;
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((!lua_isnone(&state, -1)));
  }
  switch (lua_type(&state, -1)) {
    case LUA_TNIL: {
      lua_pop(&state, 1);
      break;
    }
    case LUA_TBOOLEAN: {
      result = lua_toboolean(&state, -1) != 0;
      lua_pop(&state, 1);
      break;
    }
    case LUA_TNUMBER: {
      if (lua_isinteger(&state, -1)) {
        result = lua_tointeger(&state, -1);
        lua_pop(&state, 1);
      } else {
        result = lua_tonumber(&state, -1);
        lua_pop(&state, 1);
      }
      break;
    }
    case LUA_TSTRING: {
      auto handle = pop_handle(state);
      if (!handle)
        return std::unexpected(std::move(handle.error()));
      result = LuaString(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
      break;
    }
    case LUA_TTABLE: {
      auto handle = pop_handle(state);
      if (!handle)
        return std::unexpected(std::move(handle.error()));
      result = LuaTable(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
      break;
    }
    case LUA_TFUNCTION: {
      auto handle = pop_handle(state);
      if (!handle)
        return std::unexpected(std::move(handle.error()));
      result = LuaFunction(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
      break;
    }
    case LUA_TTHREAD: {
      auto handle = pop_handle(state);
      if (!handle)
        return std::unexpected(std::move(handle.error()));
      result = LuaThread(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
      break;
    }
    case LUA_TUSERDATA: {
      if (lua_getmetatable(&state, -1) == 0) {
        lua_pop(&state, 1);
        return std::unexpected(LuaException("Userdata in popLuaValue missing metatable"));
      }
      lua_pop(&state, 1);
      auto handle = pop_handle(state);
      if (!handle)
        return std::unexpected(std::move(handle.error()));
      result = LuaUserData(LuaDetail::LuaHandle(LuaEnginePtr(*this), *handle));
      break;
    }
    default: {
      lua_pop(&state, 1);
      return std::unexpected(LuaException("Unsupported type in popLuaValue"));
    }
  }

  return result;
}

std::expected<void, StarException> LuaEngine::increment_recursion_level() {
  // We reset the instruction count and profiling timing only on the *top
  // level* function entrance, not on recursive entrances.
  if (m_recursion_level == 0) {
    m_instruction_count = 0;
  }

  if (m_recursion_limit != 0 && m_recursion_level == m_recursion_limit)
    return std::unexpected(LuaRecursionLimitReached());

  ++m_recursion_level;
  return {};
}

void LuaEngine::decrement_recursion_level() {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((m_recursion_level != 0));
  }
  --m_recursion_level;
}

void LuaEngine::update_count_hook() {
  if (m_instruction_limit || m_profiling_enabled)
    lua_sethook(m_state.get(), &LuaEngine::count_hook, LUA_MASKCOUNT, m_instruction_measure_interval);
  else
    lua_sethook(m_state.get(), &LuaEngine::count_hook, 0, 0);
}

int LuaEngine::s_lua_instruction_limit_exception_key = 0;
int LuaEngine::s_lua_recursion_limit_exception_key = 0;

void LuaDetail::raw_set_field(lua_State& state, int index, std::string_view key) {
  lua_checkstack(&state, 1);

  int abs_table_index = lua_absindex(&state, index);
  lua_pushlstring(&state, key.data(), key.size());

  // Move the newly pushed key to the secont to top spot, leaving the value in
  // the top spot.
  lua_insert(&state, -2);

  // Pops the value and the key
  lua_rawset(&state, abs_table_index);
}

void LuaDetail::shallow_copy(lua_State& state, int source_index, int target_index) {
  lua_checkstack(&state, 3);

  int abs_source_index = lua_absindex(&state, source_index);
  int abs_target_index = lua_absindex(&state, target_index);

  lua_pushnil(&state);
  while (lua_next(&state, abs_source_index) != 0) {
    lua_pushvalue(&state, -2);
    lua_insert(&state, -2);
    lua_rawset(&state, abs_target_index);
  }
}

std::expected<void, StarException> LuaDetail::insert_json_metatable(LuaEngine& engine, LuaTable const& table,
                                                                  Json::Type type) {
  LuaStackRestore restore(*engine.m_state);
  LuaStack stack(engine, *engine.m_state);
  if (auto pushed = stack.push(table); !pushed)
    return std::unexpected(std::move(pushed.error()));
  auto nils = push_json_metadata(stack.value(-1), type);
  if (!nils)
    return std::unexpected(std::move(nils.error()));
  return {};
}

std::expected<LuaTable, StarException> LuaDetail::json_container_to_table(LuaEngine& engine, Json const& container) {
  if (!container.is_type(Json::Type::Array) && !container.is_type(Json::Type::Object))
    return std::unexpected(LuaException("jsonContainerToTable called on improper json type"));
  auto result = engine.lua_from(container);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::move(std::get<LuaTable>(*result));
}

std::expected<std::optional<Json>, StarException> LuaEngine::stack_to_json(lua_State& state, int index) {
  LuaStackRestore restore(state);
  index = lua_absindex(&state, index);
  switch (lua_type(&state, index)) {
  case LUA_TNONE:
  case LUA_TNIL: return Json();
  case LUA_TBOOLEAN: return Json(lua_toboolean(&state, index) != 0);
  case LUA_TNUMBER:
    if (lua_isinteger(&state, index))
      return Json(lua_tointeger(&state, index));
    return Json(lua_tonumber(&state, index));
  case LUA_TSTRING: {
    std::size_t size;
    char const* data = lua_tolstring(&state, index, &size);
    return Json(String(std::string_view(data, size)));
  }
  case LUA_TTABLE: break;
  default: return std::optional<Json>();
  }
  if (!lua_checkstack(&state, 8))
    return std::unexpected(LuaException("Exhausted the Lua stack while reading JSON"));

  // Only the metadata lookup can invoke user metamethods. Keep it protected;
  // all C++ JSON ownership remains outside Lua's longjmp boundary.
  lua_pushcfunction(&state, [](lua_State* state) {
    if (!lua_getmetatable(state, 1)) {
      lua_pushnil(state);
      lua_pushnil(state);
    } else {
      lua_getfield(state, -1, "__typehint");
      lua_getfield(state, -2, "__nils");
    }
    return 2;
  });
  lua_pushvalue(&state, index);
  auto metadata = handle_error(state, pcall_with_traceback(state, 1, 2));
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto hint = LuaStack(*this, state).read<std::optional<int>>(-2);
  if (!hint)
    return std::unexpected(std::move(hint.error()));
  int type_hint = hint->value_or(0);
  int nils_index = lua_gettop(&state);
  if (!lua_isnil(&state, nils_index) && !lua_istable(&state, nils_index))
    return std::unexpected(LuaConversionException("JSON null tracking value is not a table"));

  JsonObject string_entries;
  struct Entry {
    unsigned index;
    std::size_t order;
    Json value;
  };
  std::vector<Entry> int_entries;
  int_entries.reserve(lua_rawlen(&state, index));
  bool ordered = true;
  auto visit = [&](int table_index, bool nulls) -> std::expected<bool, StarException> {
    LuaStackRestore cursor_restore(state);
    lua_pushnil(&state);
    int cursor = lua_gettop(&state);
    while (true) {
      lua_pushcfunction(&state, [](lua_State* state) { return lua_next(state, 1) ? 2 : 0; });
      lua_pushvalue(&state, table_index);
      lua_pushvalue(&state, cursor);
      auto next = handle_error(state, pcall_with_traceback(state, 2, LUA_MULTRET));
      if (!next)
        return std::unexpected(std::move(next.error()));
      lua_remove(&state, cursor);
      if (lua_gettop(&state) < cursor)
        return true;
      // Preserve the native boundary's error precedence even when the value
      // is ignored by null tracking or a different value cannot become JSON.
      for (int position : {cursor, cursor + 1}) {
        auto representable = validate_native_lua_value(state, position);
        if (!representable)
          return std::unexpected(std::move(representable.error()));
      }
      Json value;
      if (!nulls) {
        auto converted = stack_to_json(state, cursor + 1);
        if (!converted)
          return std::unexpected(std::move(converted.error()));
        if (!*converted)
          return false;
        value = std::move(**converted);
      }
      int key_type = lua_type(&state, cursor);
      if (key_type == LUA_TNUMBER) {
        LuaValue key =
          lua_isinteger(&state, cursor) ? LuaValue(lua_tointeger(&state, cursor)) : LuaValue(lua_tonumber(&state, cursor));
        if (auto integer = LuaDetail::as_integer(key)) {
          unsigned key_index = static_cast<unsigned>(*integer);
          if (!int_entries.empty() && int_entries.back().index >= key_index)
            ordered = false;
          int_entries.push_back({key_index, int_entries.size(), std::move(value)});
        } else {
          string_entries[String(Star::to_string(lua_tonumber(&state, cursor)))] = std::move(value);
        }
      } else if (key_type == LUA_TSTRING) {
        std::size_t size;
        char const* data = lua_tolstring(&state, cursor, &size);
        string_entries[String(std::string_view(data, size))] = std::move(value);
      } else {
        return false;
      }
      lua_pop(&state, 1);// The next iteration retains only the original key.
    }
  };
  if (lua_istable(&state, nils_index)) {
    auto visited = visit(nils_index, true);
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
    std::sort(int_entries.begin(), int_entries.end(), [](Entry const& a, Entry const& b) {
      return a.index < b.index || (a.index == b.index && a.order < b.order);
    });
    std::size_t count = 0;
    for (std::size_t i = 0; i < int_entries.size(); ++i) {
      if (count && int_entries[count - 1].index == int_entries[i].index)
        int_entries[count - 1].value = std::move(int_entries[i].value);
      else {
        if (count != i)
          int_entries[count] = std::move(int_entries[i]);
        ++count;
      }
    }
    int_entries.resize(count);
  }
  bool interpret_as_list = string_entries.empty()
      && (type_hint == 1 || (type_hint != 2 && !int_entries.empty() && int_entries.back().index == int_entries.size()));
  if (interpret_as_list) {
    if (!int_entries.empty() && int_entries.front().index == 0)
      return std::unexpected(LuaException("JSON array index must be positive"));
    JsonArray list(int_entries.empty() ? 0 : int_entries.back().index);
    for (auto& entry : int_entries)
      list[entry.index - 1] = std::move(entry.value);
    return Json(std::move(list));
  }
  for (auto& entry : int_entries)
    string_entries[Star::to_string(entry.index)] = std::move(entry.value);
  return Json(std::move(string_entries));
}

Json LuaDetail::jarray_create() {
  return JsonArray();
}

Json LuaDetail::jobject_create() {
  return JsonObject();
}

std::expected<LuaTable, StarException> LuaDetail::jarray(LuaEngine& engine, std::optional<LuaTable> table) {
  if (table) {
    auto inserted = insert_json_metatable(engine, *table, Json::Type::Array);
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
    return std::move(*table);
  }
  return json_container_to_table(engine, JsonArray());
}

std::expected<LuaTable, StarException> LuaDetail::jobject(LuaEngine& engine, std::optional<LuaTable> table) {
  if (table) {
    auto inserted = insert_json_metatable(engine, *table, Json::Type::Object);
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
    return std::move(*table);
  }
  return json_container_to_table(engine, JsonObject());
}

std::expected<void, StarException> LuaDetail::jcont_remove(LuaTable const& table, LuaValue const& key) {
  auto mt = table.get_metatable();
  if (!mt)
    return std::unexpected(std::move(mt.error()));
  if (*mt) {
    auto nils = (*mt)->raw_get<std::optional<LuaTable>>("__nils");
    if (!nils)
      return std::unexpected(std::move(nils.error()));
    if (*nils) {
      auto removed = (**nils).raw_set(key, lua_nil);
      if (!removed)
        return std::unexpected(std::move(removed.error()));
    }
  }
  return table.raw_set(key, lua_nil);
}

std::expected<std::size_t, StarException> LuaDetail::jcont_size(LuaTable const& table) {
  std::size_t elem_count = 0;
  std::size_t highest_index = 0;
  bool hint_list = false;
  auto count = [&](LuaValue const& key, LuaValue const&) -> std::expected<void, StarException> {
    auto i = as_integer(key);
    if (i && *i >= 0)
      highest_index = std::max<std::size_t>(*i, highest_index);
    else
      hint_list = false;
    ++elem_count;
    return {};
  };
  auto mt = table.get_metatable();
  if (!mt)
    return std::unexpected(std::move(mt.error()));
  if (*mt) {
    auto hint = (*mt)->raw_get<std::optional<int>>("__typehint");
    if (!hint)
      return std::unexpected(std::move(hint.error()));
    hint_list = *hint == 1;
    auto nils = (*mt)->raw_get<std::optional<LuaTable>>("__nils");
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
  return hint_list ? highest_index : elem_count;
}

std::expected<void, StarException> LuaDetail::jcont_resize(LuaTable const& table, std::size_t target_size) {
  auto mt = table.get_metatable();
  if (!mt)
    return std::unexpected(std::move(mt.error()));
  if (*mt) {
    auto nils = (*mt)->raw_get<std::optional<LuaTable>>("__nils");
    if (!nils)
      return std::unexpected(std::move(nils.error()));
    if (*nils) {
      auto resized = (**nils).iterate([&](LuaValue const& key, LuaValue const&) -> std::expected<void, StarException> {
        auto i = as_integer(key);
        if (i && *i > 0 && (std::size_t)*i > target_size)
          return (**nils).raw_set(key, lua_nil);
        return {};
      });
      if (!resized)
        return std::unexpected(std::move(resized.error()));
    }
  }
  auto resized = table.iterate([&](LuaValue const& key, LuaValue const&) -> std::expected<void, StarException> {
    auto i = as_integer(key);
    if (i && *i > 0 && (std::size_t)*i > target_size)
      return table.raw_set(key, lua_nil);
    return {};
  });
  if (!resized)
    return std::unexpected(std::move(resized.error()));
  auto last = table.get(target_size);
  if (!last)
    return std::unexpected(std::move(last.error()));
  return table.set(target_size, std::move(*last));
}

std::optional<LuaInt> LuaDetail::as_integer(LuaValue const& v) {
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
